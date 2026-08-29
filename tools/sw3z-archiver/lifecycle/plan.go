package lifecycle

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

type Operation string

const (
	OperationInstall Operation = "install"
	OperationUpdate  Operation = "update"
	OperationRemove  Operation = "remove"
)

type ActionKind string

const (
	ActionWrite    ActionKind = "write"
	ActionDelete   ActionKind = "delete"
	ActionPreserve ActionKind = "preserve"
)

type Action struct {
	Kind         ActionKind `json:"kind"`
	Path         string     `json:"path"`
	SHA256       string     `json:"sha256,omitempty"`
	ExpectedHash string     `json:"expectedHash,omitempty"`
	Reason       string     `json:"reason"`
}

type Plan struct {
	Operation Operation    `json:"operation"`
	Package   string       `json:"package"`
	Actions   []Action     `json:"actions"`
	Warnings  []Diagnostic `json:"warnings,omitempty"`
}

func (p *Plan) JSON() ([]byte, error) { return json.MarshalIndent(p, "", "  ") }

type ownedFile struct {
	hash   string
	owners []string
	shared bool
}

func ownership(manifests []Manifest) map[string]ownedFile {
	out := map[string]ownedFile{}
	for _, manifest := range manifests {
		for _, file := range manifest.Files {
			owned := out[file.Path]
			owned.hash = file.SHA256
			owned.shared = file.Shared
			owned.owners = append(owned.owners, manifest.ID)
			out[file.Path] = owned
		}
	}
	return out
}

func replacePackage(current []Manifest, incoming Manifest, requireExisting bool) ([]Manifest, error) {
	found := false
	desired := make([]Manifest, 0, len(current)+1)
	for _, manifest := range current {
		if manifest.ID == incoming.ID {
			found = true
			continue
		}
		desired = append(desired, manifest)
	}
	if requireExisting && !found {
		return nil, fmt.Errorf("package %q is not installed", incoming.ID)
	}
	if !requireExisting && found {
		return nil, fmt.Errorf("package %q is already installed; use update", incoming.ID)
	}
	desired = append(desired, incoming)
	return desired, nil
}

func removePackage(current []Manifest, id string) ([]Manifest, error) {
	found := false
	desired := make([]Manifest, 0, len(current))
	for _, manifest := range current {
		if manifest.ID == id {
			found = true
			continue
		}
		desired = append(desired, manifest)
	}
	if !found {
		return nil, fmt.Errorf("package %q is not installed", id)
	}
	return desired, nil
}

func diskHash(root, relative string) (string, bool, error) {
	hash, _, err := HashFile(filepath.Join(root, filepath.FromSlash(relative)))
	if errors.Is(err, os.ErrNotExist) {
		return "", false, nil
	}
	return hash, err == nil, err
}

// BuildPlan creates a non-mutating install/update/remove plan. Unknown files,
// modified owned files, and files with incomplete ownership are preserved.
func BuildPlan(root string, operation Operation, current []Manifest, incoming *Manifest, removeID string) (*Plan, error) {
	if err := ValidateSet(current); err != nil && len(current) != 0 {
		return nil, fmt.Errorf("installed package state: %w", err)
	}
	var desired []Manifest
	var packageID string
	var err error
	switch operation {
	case OperationInstall:
		if incoming == nil {
			return nil, errors.New("install requires an incoming manifest")
		}
		packageID = incoming.ID
		desired, err = replacePackage(current, *incoming, false)
	case OperationUpdate:
		if incoming == nil {
			return nil, errors.New("update requires an incoming manifest")
		}
		packageID = incoming.ID
		desired, err = replacePackage(current, *incoming, true)
	case OperationRemove:
		if removeID == "" {
			return nil, errors.New("remove requires a package id")
		}
		packageID = removeID
		desired, err = removePackage(current, removeID)
	default:
		return nil, fmt.Errorf("unsupported package operation %q", operation)
	}
	if err != nil {
		return nil, err
	}
	if err := ValidateSet(desired); err != nil && len(desired) != 0 {
		return nil, fmt.Errorf("desired package state: %w", err)
	}

	before := ownership(current)
	after := ownership(desired)
	paths := map[string]bool{}
	for path := range before {
		paths[path] = true
	}
	for path := range after {
		paths[path] = true
	}
	ordered := make([]string, 0, len(paths))
	for path := range paths {
		ordered = append(ordered, path)
	}
	sort.Strings(ordered)

	plan := &Plan{Operation: operation, Package: packageID}
	for _, path := range ordered {
		oldFile, hadOwner := before[path]
		newFile, wantsFile := after[path]
		hash, exists, hashErr := diskHash(root, path)
		if hashErr != nil {
			return nil, fmt.Errorf("hash %s: %w", path, hashErr)
		}
		switch {
		case wantsFile && (!hadOwner || oldFile.hash != newFile.hash):
			if exists && (!hadOwner || hash != oldFile.hash) {
				plan.Actions = append(plan.Actions, Action{Kind: ActionPreserve, Path: path, ExpectedHash: oldFile.hash, Reason: "foreign-or-modified-file"})
				plan.Warnings = append(plan.Warnings, Diagnostic{Code: "foreign_file_preserved", Package: packageID, Path: path, Message: "existing file is not owned with the expected hash"})
				continue
			}
			plan.Actions = append(plan.Actions, Action{Kind: ActionWrite, Path: path, SHA256: newFile.hash, ExpectedHash: oldFile.hash, Reason: string(operation)})
		case wantsFile:
			if !exists {
				plan.Actions = append(plan.Actions, Action{Kind: ActionWrite, Path: path, SHA256: newFile.hash, ExpectedHash: oldFile.hash, Reason: "restore-missing-owned-file"})
			} else if hash != newFile.hash {
				plan.Actions = append(plan.Actions, Action{Kind: ActionPreserve, Path: path, ExpectedHash: newFile.hash, Reason: "modified-owned-file"})
				plan.Warnings = append(plan.Warnings, Diagnostic{Code: "modified_file_preserved", Package: packageID, Path: path, Message: "owned file differs from its recorded hash"})
			}
		case hadOwner:
			if !exists {
				continue
			}
			if hash != oldFile.hash {
				plan.Actions = append(plan.Actions, Action{Kind: ActionPreserve, Path: path, ExpectedHash: oldFile.hash, Reason: "modified-owned-file"})
				plan.Warnings = append(plan.Warnings, Diagnostic{Code: "modified_file_preserved", Package: packageID, Path: path, Message: "remove refused because the file hash changed"})
				continue
			}
			plan.Actions = append(plan.Actions, Action{Kind: ActionDelete, Path: path, ExpectedHash: oldFile.hash, Reason: "last-owner-removed"})
		}
	}
	return plan, nil
}

type ApplyOptions struct {
	// BeforeAction is a test/embedding seam. Returning an error triggers the
	// same rollback used for real I/O failures.
	BeforeAction func(index int, action Action) error
}

const transactionPrefix = ".sw3z-transaction-"

func recoverTransaction(root, txn string) error {
	data, err := os.ReadFile(filepath.Join(txn, "journal.json"))
	if errors.Is(err, os.ErrNotExist) {
		// A crash during staging has not touched the working set.
		return os.RemoveAll(txn)
	}
	if err != nil {
		return err
	}
	var plan Plan
	if err := json.Unmarshal(data, &plan); err != nil {
		return fmt.Errorf("decode interrupted transaction journal: %w", err)
	}
	backupRoot := filepath.Join(txn, "backup")
	for index := len(plan.Actions) - 1; index >= 0; index-- {
		action := plan.Actions[index]
		if action.Kind == ActionPreserve {
			continue
		}
		target, err := validateActionPath(root, action.Path)
		if err != nil {
			return err
		}
		backup := filepath.Join(backupRoot, filepath.FromSlash(action.Path))
		if _, err := os.Lstat(backup); err == nil {
			if err := os.Remove(target); err != nil && !errors.Is(err, os.ErrNotExist) {
				return err
			}
			if err := os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
				return err
			}
			if err := os.Rename(backup, target); err != nil {
				return err
			}
			continue
		} else if !errors.Is(err, os.ErrNotExist) {
			return err
		}
		// A new install has no backup. Delete it only if its exact planned
		// content reached the destination; an unrelated file is preserved.
		if action.Kind == ActionWrite {
			hash, exists, hashErr := diskHash(root, action.Path)
			if hashErr != nil {
				return hashErr
			}
			if exists && hash == action.SHA256 {
				if err := os.Remove(target); err != nil {
					return err
				}
			}
		}
	}
	return os.RemoveAll(txn)
}

// Recover rolls back transactions interrupted after their durable journal was
// written. It is safe to call before every package operation.
func Recover(root string) error {
	entries, err := os.ReadDir(root)
	if errors.Is(err, os.ErrNotExist) {
		return nil
	}
	if err != nil {
		return err
	}
	for _, entry := range entries {
		if !entry.IsDir() || !strings.HasPrefix(entry.Name(), transactionPrefix) {
			continue
		}
		if err := recoverTransaction(root, filepath.Join(root, entry.Name())); err != nil {
			return err
		}
	}
	return nil
}

func bytesHash(data []byte) string {
	hash := sha256.Sum256(data)
	return hex.EncodeToString(hash[:])
}

func writeJournal(path string, data []byte) error {
	file, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o600)
	if err != nil {
		return err
	}
	if _, err := file.Write(data); err != nil {
		file.Close()
		return err
	}
	if err := file.Sync(); err != nil {
		file.Close()
		return err
	}
	return file.Close()
}

type appliedAction struct {
	action    Action
	target    string
	backup    string
	hadBackup bool
}

func validateActionPath(root, path string) (string, error) {
	if !cleanRelativePath(path) {
		return "", fmt.Errorf("unsafe plan path %q", path)
	}
	rootAbs, err := filepath.Abs(root)
	if err != nil {
		return "", err
	}
	target := filepath.Join(rootAbs, filepath.FromSlash(path))
	relative, err := filepath.Rel(rootAbs, target)
	if err != nil || relative == ".." || strings.HasPrefix(relative, ".."+string(filepath.Separator)) {
		return "", fmt.Errorf("plan path escapes root: %q", path)
	}
	return target, nil
}

// Apply atomically stages all writes, backs up every touched destination, and
// rolls the whole transaction back if any commit action fails.
func Apply(root string, plan *Plan, content map[string][]byte, options ApplyOptions) error {
	if plan == nil {
		return errors.New("plan is nil")
	}
	if err := os.MkdirAll(root, 0o755); err != nil {
		return err
	}
	if err := Recover(root); err != nil {
		return fmt.Errorf("recover interrupted package transaction: %w", err)
	}
	txn, err := os.MkdirTemp(root, transactionPrefix)
	if err != nil {
		return err
	}
	defer os.RemoveAll(txn)
	stagedRoot := filepath.Join(txn, "staged")
	backupRoot := filepath.Join(txn, "backup")

	for _, action := range plan.Actions {
		if action.Kind != ActionWrite {
			continue
		}
		data, ok := content[action.Path]
		if !ok {
			return fmt.Errorf("missing staged content for %s", action.Path)
		}
		if bytesHash(data) != action.SHA256 {
			return fmt.Errorf("staged content hash mismatch for %s", action.Path)
		}
		staged := filepath.Join(stagedRoot, filepath.FromSlash(action.Path))
		if err := os.MkdirAll(filepath.Dir(staged), 0o755); err != nil {
			return err
		}
		if err := os.WriteFile(staged, data, 0o644); err != nil {
			return err
		}
	}
	journal, err := plan.JSON()
	if err != nil {
		return err
	}
	if err := writeJournal(filepath.Join(txn, "journal.json"), journal); err != nil {
		return err
	}

	var applied []appliedAction
	rollback := func() {
		for i := len(applied) - 1; i >= 0; i-- {
			entry := applied[i]
			_ = os.Remove(entry.target)
			if entry.hadBackup {
				_ = os.MkdirAll(filepath.Dir(entry.target), 0o755)
				_ = os.Rename(entry.backup, entry.target)
			}
		}
	}

	for index, action := range plan.Actions {
		if action.Kind == ActionPreserve {
			continue
		}
		if options.BeforeAction != nil {
			if err := options.BeforeAction(index, action); err != nil {
				rollback()
				return err
			}
		}
		target, err := validateActionPath(root, action.Path)
		if err != nil {
			rollback()
			return err
		}
		entry := appliedAction{action: action, target: target, backup: filepath.Join(backupRoot, filepath.FromSlash(action.Path))}
		if _, err := os.Lstat(target); err == nil {
			currentHash, _, hashErr := HashFile(target)
			if hashErr != nil || (action.ExpectedHash != "" && currentHash != action.ExpectedHash) {
				rollback()
				return fmt.Errorf("destination changed after planning: %s", action.Path)
			}
			if err := os.MkdirAll(filepath.Dir(entry.backup), 0o755); err != nil {
				rollback()
				return err
			}
			if err := os.Rename(target, entry.backup); err != nil {
				rollback()
				return err
			}
			entry.hadBackup = true
		} else if !errors.Is(err, os.ErrNotExist) {
			rollback()
			return err
		}
		applied = append(applied, entry)
		if action.Kind == ActionDelete {
			continue
		}
		if err := os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
			rollback()
			return err
		}
		staged := filepath.Join(stagedRoot, filepath.FromSlash(action.Path))
		if err := os.Rename(staged, target); err != nil {
			rollback()
			return err
		}
	}
	return nil
}
