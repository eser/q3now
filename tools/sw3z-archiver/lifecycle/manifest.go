// Package lifecycle defines the canonical SW3Z package manifest and validator.
// It is deliberately independent from archive encoding: launchers, release
// tooling and the future runtime mod layer consume the same identity and
// dependency vocabulary without teaching the SW3Z reader about installation.
package lifecycle

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

const SchemaV1 = "wired.sw3z.package/v1"

type Role string

const (
	RoleRuntime   Role = "runtime"
	RoleToolchain Role = "toolchain"
	RoleServer    Role = "server"
	RoleClient    Role = "client"
	RoleShared    Role = "shared"
	RoleCosmetic  Role = "cosmetic"
)

type Compatibility struct {
	OS        []string `json:"os"`
	Arch      []string `json:"arch"`
	ABI       string   `json:"abi"`
	EngineMin string   `json:"engineMin,omitempty"`
	EngineMax string   `json:"engineMax,omitempty"`
}

type Dependency struct {
	ID       string `json:"id"`
	Version  string `json:"version,omitempty"`
	Optional bool   `json:"optional,omitempty"`
}

type File struct {
	Path   string `json:"path"`
	SHA256 string `json:"sha256"`
	Size   int64  `json:"size"`
	Shared bool   `json:"shared,omitempty"`
}

type Manifest struct {
	Schema        string        `json:"schema"`
	ID            string        `json:"id"`
	Version       string        `json:"version"`
	Owner         string        `json:"owner"`
	Role          Role          `json:"role"`
	Depends       []Dependency  `json:"depends,omitempty"`
	Provides      []string      `json:"provides,omitempty"`
	Compatibility Compatibility `json:"compatibility"`
	Files         []File        `json:"files,omitempty"`
}

// Diagnostic is stable machine-readable validation output. Code is suitable
// for CI assertions; Message is for operators.
type Diagnostic struct {
	Code    string `json:"code"`
	Package string `json:"package,omitempty"`
	Path    string `json:"path,omitempty"`
	Message string `json:"message"`
}

type ValidationError struct{ Diagnostics []Diagnostic }

func (e *ValidationError) Error() string {
	if len(e.Diagnostics) == 0 {
		return "invalid SW3Z package manifest"
	}
	return e.Diagnostics[0].Code + ": " + e.Diagnostics[0].Message
}

var identityPattern = regexp.MustCompile(`^[a-z0-9][a-z0-9._-]*$`)
var hashPattern = regexp.MustCompile(`^[0-9a-f]{64}$`)

func validRole(role Role) bool {
	switch role {
	case RoleRuntime, RoleToolchain, RoleServer, RoleClient, RoleShared, RoleCosmetic:
		return true
	default:
		return false
	}
}

func cleanRelativePath(path string) bool {
	if path == "" || filepath.IsAbs(path) || strings.Contains(path, "\\") {
		return false
	}
	cleaned := filepath.ToSlash(filepath.Clean(path))
	return cleaned == path && cleaned != "." && cleaned != ".." && !strings.HasPrefix(cleaned, "../")
}

func appendDiagnostic(out *[]Diagnostic, code, pkg, path, message string) {
	*out = append(*out, Diagnostic{Code: code, Package: pkg, Path: path, Message: message})
}

// Validate verifies one manifest without requiring its dependency set.
func Validate(m *Manifest) error {
	var diagnostics []Diagnostic
	if m == nil {
		appendDiagnostic(&diagnostics, "nil_manifest", "", "", "manifest is nil")
		return &ValidationError{Diagnostics: diagnostics}
	}
	if m.Schema != SchemaV1 {
		appendDiagnostic(&diagnostics, "unsupported_schema", m.ID, "", fmt.Sprintf("schema must be %q", SchemaV1))
	}
	if !identityPattern.MatchString(m.ID) {
		appendDiagnostic(&diagnostics, "invalid_identity", m.ID, "", "package id must use lowercase letters, digits, dot, underscore or dash")
	}
	if strings.TrimSpace(m.Version) == "" {
		appendDiagnostic(&diagnostics, "missing_version", m.ID, "", "package version is required")
	}
	if !identityPattern.MatchString(m.Owner) {
		appendDiagnostic(&diagnostics, "missing_owner", m.ID, "", "package owner is required and must be a canonical identity")
	}
	if !validRole(m.Role) {
		appendDiagnostic(&diagnostics, "invalid_role", m.ID, "", "package role is not recognized")
	}
	if len(m.Compatibility.OS) == 0 || len(m.Compatibility.Arch) == 0 || strings.TrimSpace(m.Compatibility.ABI) == "" {
		appendDiagnostic(&diagnostics, "missing_compatibility", m.ID, "", "compatibility.os, compatibility.arch and compatibility.abi are required")
	}
	seenDepends := map[string]bool{}
	for _, dependency := range m.Depends {
		if !identityPattern.MatchString(dependency.ID) {
			appendDiagnostic(&diagnostics, "invalid_dependency", m.ID, "", fmt.Sprintf("dependency %q is not a canonical identity", dependency.ID))
		}
		if dependency.ID == m.ID {
			appendDiagnostic(&diagnostics, "dependency_cycle", m.ID, "", "package depends on itself")
		}
		if seenDepends[dependency.ID] {
			appendDiagnostic(&diagnostics, "duplicate_dependency", m.ID, "", fmt.Sprintf("dependency %q is repeated", dependency.ID))
		}
		seenDepends[dependency.ID] = true
	}
	seenProvides := map[string]bool{}
	for _, provided := range m.Provides {
		if !identityPattern.MatchString(provided) {
			appendDiagnostic(&diagnostics, "invalid_provide", m.ID, "", fmt.Sprintf("provided capability %q is not canonical", provided))
		}
		if seenProvides[provided] {
			appendDiagnostic(&diagnostics, "duplicate_provide", m.ID, "", fmt.Sprintf("provided capability %q is repeated", provided))
		}
		seenProvides[provided] = true
	}
	seenFiles := map[string]bool{}
	for _, file := range m.Files {
		if !cleanRelativePath(file.Path) {
			appendDiagnostic(&diagnostics, "unsafe_path", m.ID, file.Path, "file path must be normalized, slash-separated and relative")
		}
		if !hashPattern.MatchString(file.SHA256) {
			appendDiagnostic(&diagnostics, "invalid_hash", m.ID, file.Path, "file sha256 must be 64 lowercase hexadecimal characters")
		}
		if file.Size < 0 {
			appendDiagnostic(&diagnostics, "invalid_size", m.ID, file.Path, "file size cannot be negative")
		}
		if seenFiles[file.Path] {
			appendDiagnostic(&diagnostics, "duplicate_file", m.ID, file.Path, "file path is repeated")
		}
		seenFiles[file.Path] = true
	}
	if len(diagnostics) != 0 {
		return &ValidationError{Diagnostics: diagnostics}
	}
	return nil
}

// ValidateSet resolves required dependencies through package IDs or provides,
// detects cycles, and rejects unsafe multi-owner file collisions.
func ValidateSet(manifests []Manifest) error {
	var diagnostics []Diagnostic
	byID := map[string]*Manifest{}
	providers := map[string][]string{}
	for i := range manifests {
		m := &manifests[i]
		if err := Validate(m); err != nil {
			diagnostics = append(diagnostics, err.(*ValidationError).Diagnostics...)
		}
		if prior := byID[m.ID]; prior != nil {
			appendDiagnostic(&diagnostics, "duplicate_package", m.ID, "", "package id is repeated")
		} else {
			byID[m.ID] = m
		}
		providers[m.ID] = append(providers[m.ID], m.ID)
		for _, capability := range m.Provides {
			providers[capability] = append(providers[capability], m.ID)
		}
	}

	edges := map[string][]string{}
	for i := range manifests {
		m := &manifests[i]
		for _, dependency := range m.Depends {
			matches := providers[dependency.ID]
			if len(matches) == 0 {
				if !dependency.Optional {
					appendDiagnostic(&diagnostics, "missing_dependency", m.ID, "", fmt.Sprintf("required dependency %q is not installed", dependency.ID))
				}
				continue
			}
			if len(matches) != 1 {
				appendDiagnostic(&diagnostics, "ambiguous_dependency", m.ID, "", fmt.Sprintf("dependency %q has multiple providers", dependency.ID))
				continue
			}
			edges[m.ID] = append(edges[m.ID], matches[0])
		}
	}

	state := map[string]uint8{}
	var visit func(string)
	visit = func(id string) {
		if state[id] == 2 {
			return
		}
		if state[id] == 1 {
			appendDiagnostic(&diagnostics, "dependency_cycle", id, "", "dependency graph contains a cycle")
			return
		}
		state[id] = 1
		for _, dependency := range edges[id] {
			visit(dependency)
		}
		state[id] = 2
	}
	for id := range byID {
		visit(id)
	}

	type owner struct {
		id     string
		hash   string
		shared bool
	}
	files := map[string][]owner{}
	for i := range manifests {
		for _, file := range manifests[i].Files {
			files[file.Path] = append(files[file.Path], owner{manifests[i].ID, file.SHA256, file.Shared})
		}
	}
	for path, owners := range files {
		if len(owners) < 2 {
			continue
		}
		for _, candidate := range owners {
			if !candidate.shared || candidate.hash != owners[0].hash {
				appendDiagnostic(&diagnostics, "file_owner_conflict", candidate.id, path, "multiple packages own a non-shared or content-mismatched file")
				break
			}
		}
	}
	if len(diagnostics) != 0 {
		sort.SliceStable(diagnostics, func(i, j int) bool {
			if diagnostics[i].Code != diagnostics[j].Code {
				return diagnostics[i].Code < diagnostics[j].Code
			}
			if diagnostics[i].Package != diagnostics[j].Package {
				return diagnostics[i].Package < diagnostics[j].Package
			}
			return diagnostics[i].Path < diagnostics[j].Path
		})
		return &ValidationError{Diagnostics: diagnostics}
	}
	return nil
}

func Load(path string) (*Manifest, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var manifest Manifest
	if err := json.Unmarshal(data, &manifest); err != nil {
		return nil, fmt.Errorf("decode package manifest %s: %w", path, err)
	}
	if err := Validate(&manifest); err != nil {
		return nil, err
	}
	return &manifest, nil
}

func Save(path string, manifest *Manifest) error {
	if err := Validate(manifest); err != nil {
		return err
	}
	data, err := json.MarshalIndent(manifest, "", "  ")
	if err != nil {
		return err
	}
	data = append(data, '\n')
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	temporary, err := os.CreateTemp(filepath.Dir(path), ".sw3z-manifest-*")
	if err != nil {
		return err
	}
	temporaryPath := temporary.Name()
	defer os.Remove(temporaryPath)
	if _, err := temporary.Write(data); err != nil {
		temporary.Close()
		return err
	}
	if err := temporary.Chmod(0o644); err != nil {
		temporary.Close()
		return err
	}
	if err := temporary.Sync(); err != nil {
		temporary.Close()
		return err
	}
	if err := temporary.Close(); err != nil {
		return err
	}
	return os.Rename(temporaryPath, path)
}

func hashReader(reader io.Reader) (string, int64, error) {
	hash := sha256.New()
	size, err := io.Copy(hash, reader)
	if err != nil {
		return "", 0, err
	}
	return hex.EncodeToString(hash.Sum(nil)), size, nil
}

func HashFile(path string) (string, int64, error) {
	file, err := os.Open(path)
	if err != nil {
		return "", 0, err
	}
	defer file.Close()
	return hashReader(file)
}

// Generate inventories a staging directory without following symlinks.
func Generate(root string, metadata Manifest) (*Manifest, error) {
	metadata.Schema = SchemaV1
	metadata.Files = nil
	err := filepath.WalkDir(root, func(path string, entry os.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		// Match the SW3Z writer's package surface: dotfiles and dot-directories
		// are developer metadata, never archive entries or owned install files.
		if path != root && strings.HasPrefix(entry.Name(), ".") {
			if entry.IsDir() {
				return filepath.SkipDir
			}
			return nil
		}
		if entry.IsDir() {
			return nil
		}
		if entry.Type()&os.ModeSymlink != 0 {
			return fmt.Errorf("symlink is not packageable: %s", path)
		}
		relative, err := filepath.Rel(root, path)
		if err != nil {
			return err
		}
		relative = filepath.ToSlash(relative)
		hash, size, err := HashFile(path)
		if err != nil {
			return err
		}
		metadata.Files = append(metadata.Files, File{Path: relative, SHA256: hash, Size: size})
		return nil
	})
	if err != nil {
		return nil, err
	}
	sort.Slice(metadata.Files, func(i, j int) bool { return metadata.Files[i].Path < metadata.Files[j].Path })
	if err := Validate(&metadata); err != nil {
		return nil, err
	}
	return &metadata, nil
}

// InputFile maps one physical source artifact to its install-relative path.
// Packed SW3Z releases normally contain one mapping (paxNN.sw3z); unpacked
// toolchain packages may use Generate instead.
type InputFile struct {
	InstallPath string
	SourcePath  string
	Shared      bool
}

func GenerateFiles(inputs []InputFile, metadata Manifest) (*Manifest, error) {
	metadata.Schema = SchemaV1
	metadata.Files = nil
	for _, input := range inputs {
		if !cleanRelativePath(input.InstallPath) {
			return nil, fmt.Errorf("unsafe install path %q", input.InstallPath)
		}
		hash, size, err := HashFile(input.SourcePath)
		if err != nil {
			return nil, fmt.Errorf("hash package artifact %s: %w", input.SourcePath, err)
		}
		metadata.Files = append(metadata.Files, File{
			Path: input.InstallPath, SHA256: hash, Size: size, Shared: input.Shared,
		})
	}
	sort.Slice(metadata.Files, func(i, j int) bool { return metadata.Files[i].Path < metadata.Files[j].Path })
	if err := Validate(&metadata); err != nil {
		return nil, err
	}
	return &metadata, nil
}
