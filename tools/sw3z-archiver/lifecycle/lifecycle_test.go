package lifecycle

import (
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func digest(text string) string { return bytesHash([]byte(text)) }

func packageManifest(id string, role Role, files ...File) Manifest {
	return Manifest{
		Schema:  SchemaV1,
		ID:      id,
		Version: "1.0.0",
		Owner:   "wired",
		Role:    role,
		Compatibility: Compatibility{
			OS: []string{"any"}, Arch: []string{"any"}, ABI: "wired-content-v1",
		},
		Files: files,
	}
}

func validationHas(err error, code string) bool {
	var validation *ValidationError
	if !errors.As(err, &validation) {
		return false
	}
	for _, diagnostic := range validation.Diagnostics {
		if diagnostic.Code == code {
			return true
		}
	}
	return false
}

func TestValidatorReportsOwnerDependencyAndCycleDiagnostics(t *testing.T) {
	missingOwner := packageManifest("broken", RoleRuntime)
	missingOwner.Owner = ""
	if err := Validate(&missingOwner); !validationHas(err, "missing_owner") {
		t.Fatalf("missing owner diagnostic absent: %v", err)
	}

	consumer := packageManifest("consumer", RoleClient)
	consumer.Depends = []Dependency{{ID: "missing-provider"}}
	if err := ValidateSet([]Manifest{consumer}); !validationHas(err, "missing_dependency") {
		t.Fatalf("missing dependency diagnostic absent: %v", err)
	}

	a := packageManifest("a", RoleRuntime)
	b := packageManifest("b", RoleRuntime)
	a.Depends = []Dependency{{ID: "b"}}
	b.Depends = []Dependency{{ID: "a"}}
	if err := ValidateSet([]Manifest{a, b}); !validationHas(err, "dependency_cycle") {
		t.Fatalf("dependency cycle diagnostic absent: %v", err)
	}
}

func TestToolchainCompatibilityIsFirstClass(t *testing.T) {
	tool := packageManifest("wired.q3map2", RoleToolchain, File{Path: "bin/q3map2", SHA256: digest("binary"), Size: 6})
	tool.Version = "0.80.42"
	tool.Compatibility = Compatibility{OS: []string{"linux", "darwin", "windows"}, Arch: []string{"amd64", "arm64"}, ABI: "wired-bsp-v1", EngineMin: "0.80"}
	tool.Provides = []string{"wired.content-compiler"}
	if err := Validate(&tool); err != nil {
		t.Fatalf("toolchain manifest rejected: %v", err)
	}
	consumer := packageManifest("map-source", RoleRuntime)
	consumer.Depends = []Dependency{{ID: "wired.content-compiler"}}
	if err := ValidateSet([]Manifest{tool, consumer}); err != nil {
		t.Fatalf("tool capability dependency rejected: %v", err)
	}
}

func TestGenerateProducesSortedHashedInventory(t *testing.T) {
	root := t.TempDir()
	if err := os.MkdirAll(filepath.Join(root, "z"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "z", "second"), []byte("2"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "first"), []byte("1"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, ".gitkeep"), nil, 0o644); err != nil {
		t.Fatal(err)
	}
	manifest, err := Generate(root, packageManifest("wired.core", RoleRuntime))
	if err != nil {
		t.Fatal(err)
	}
	if len(manifest.Files) != 2 || manifest.Files[0].Path != "first" || manifest.Files[1].Path != "z/second" {
		t.Fatalf("inventory is not deterministic: %#v", manifest.Files)
	}
}

func TestGenerateFilesOwnsPhysicalArchiveArtifact(t *testing.T) {
	root := t.TempDir()
	archive := filepath.Join(root, "pax21.sw3z")
	if err := os.WriteFile(archive, []byte("archive"), 0o644); err != nil {
		t.Fatal(err)
	}
	manifest, err := GenerateFiles([]InputFile{{InstallPath: "pax21.sw3z", SourcePath: archive}}, packageManifest("wired.content", RoleRuntime))
	if err != nil {
		t.Fatal(err)
	}
	if len(manifest.Files) != 1 || manifest.Files[0].Path != "pax21.sw3z" || manifest.Files[0].SHA256 != digest("archive") {
		t.Fatalf("physical artifact ownership is wrong: %#v", manifest.Files)
	}
}

func TestSharedFileSurvivesUntilLastOwner(t *testing.T) {
	root := t.TempDir()
	path := filepath.Join(root, "shared.dat")
	if err := os.WriteFile(path, []byte("same"), 0o644); err != nil {
		t.Fatal(err)
	}
	shared := File{Path: "shared.dat", SHA256: digest("same"), Size: 4, Shared: true}
	a := packageManifest("a", RoleShared, shared)
	b := packageManifest("b", RoleShared, shared)
	plan, err := BuildPlan(root, OperationRemove, []Manifest{a, b}, nil, "a")
	if err != nil {
		t.Fatal(err)
	}
	if len(plan.Actions) != 0 {
		t.Fatalf("shared file was touched while an owner remains: %#v", plan.Actions)
	}
	plan, err = BuildPlan(root, OperationRemove, []Manifest{b}, nil, "b")
	if err != nil {
		t.Fatal(err)
	}
	if len(plan.Actions) != 1 || plan.Actions[0].Kind != ActionDelete {
		t.Fatalf("last shared owner did not schedule deletion: %#v", plan.Actions)
	}
}

func TestForeignOrModifiedFilesAreNeverDeletedOrOverwritten(t *testing.T) {
	root := t.TempDir()
	if err := os.WriteFile(filepath.Join(root, "owned.cfg"), []byte("user edit"), 0o644); err != nil {
		t.Fatal(err)
	}
	installed := packageManifest("old", RoleRuntime, File{Path: "owned.cfg", SHA256: digest("original"), Size: 8})
	remove, err := BuildPlan(root, OperationRemove, []Manifest{installed}, nil, "old")
	if err != nil {
		t.Fatal(err)
	}
	if len(remove.Actions) != 1 || remove.Actions[0].Kind != ActionPreserve || remove.Warnings[0].Code != "modified_file_preserved" {
		t.Fatalf("modified owned file was not preserved: %#v", remove)
	}

	if err := os.WriteFile(filepath.Join(root, "orphan.cfg"), []byte("foreign"), 0o644); err != nil {
		t.Fatal(err)
	}
	incoming := packageManifest("new", RoleRuntime, File{Path: "orphan.cfg", SHA256: digest("package"), Size: 7})
	install, err := BuildPlan(root, OperationInstall, nil, &incoming, "")
	if err != nil {
		t.Fatal(err)
	}
	if len(install.Actions) != 1 || install.Actions[0].Kind != ActionPreserve || install.Warnings[0].Code != "foreign_file_preserved" {
		t.Fatalf("foreign/orphan file was not preserved: %#v", install)
	}
}

func TestAtomicApplyRollsBackWholeUpdate(t *testing.T) {
	root := t.TempDir()
	for name, body := range map[string]string{"a.txt": "old-a", "b.txt": "old-b"} {
		if err := os.WriteFile(filepath.Join(root, name), []byte(body), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	old := packageManifest("game", RoleRuntime,
		File{Path: "a.txt", SHA256: digest("old-a"), Size: 5},
		File{Path: "b.txt", SHA256: digest("old-b"), Size: 5})
	next := packageManifest("game", RoleRuntime,
		File{Path: "a.txt", SHA256: digest("new-a"), Size: 5},
		File{Path: "b.txt", SHA256: digest("new-b"), Size: 5})
	next.Version = "2.0.0"
	plan, err := BuildPlan(root, OperationUpdate, []Manifest{old}, &next, "")
	if err != nil {
		t.Fatal(err)
	}
	failure := errors.New("injected commit failure")
	err = Apply(root, plan, map[string][]byte{"a.txt": []byte("new-a"), "b.txt": []byte("new-b")}, ApplyOptions{
		BeforeAction: func(index int, _ Action) error {
			if index == 1 {
				return failure
			}
			return nil
		},
	})
	if !errors.Is(err, failure) {
		t.Fatalf("expected injected failure, got %v", err)
	}
	for name, expected := range map[string]string{"a.txt": "old-a", "b.txt": "old-b"} {
		data, readErr := os.ReadFile(filepath.Join(root, name))
		if readErr != nil || string(data) != expected {
			t.Fatalf("rollback lost %s: %q, %v", name, data, readErr)
		}
	}
	entries, err := os.ReadDir(root)
	if err != nil {
		t.Fatal(err)
	}
	for _, entry := range entries {
		if strings.HasPrefix(entry.Name(), ".sw3z-transaction-") {
			t.Fatalf("transaction debris remains: %s", entry.Name())
		}
	}
}

func TestAtomicApplyCommitsInstallAndRemove(t *testing.T) {
	root := t.TempDir()
	incoming := packageManifest("game", RoleRuntime, File{Path: "nested/new.txt", SHA256: digest("new"), Size: 3})
	install, err := BuildPlan(root, OperationInstall, nil, &incoming, "")
	if err != nil {
		t.Fatal(err)
	}
	if err := Apply(root, install, map[string][]byte{"nested/new.txt": []byte("new")}, ApplyOptions{}); err != nil {
		t.Fatal(err)
	}
	remove, err := BuildPlan(root, OperationRemove, []Manifest{incoming}, nil, "game")
	if err != nil {
		t.Fatal(err)
	}
	if err := Apply(root, remove, nil, ApplyOptions{}); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(root, "nested/new.txt")); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("remove did not delete last-owner file: %v", err)
	}
}

func TestRecoverInterruptedUpdateRestoresPreviousWorkingSet(t *testing.T) {
	root := t.TempDir()
	target := filepath.Join(root, "game.cfg")
	if err := os.WriteFile(target, []byte("new"), 0o644); err != nil {
		t.Fatal(err)
	}
	plan := &Plan{Operation: OperationUpdate, Package: "game", Actions: []Action{{
		Kind: ActionWrite, Path: "game.cfg", SHA256: digest("new"), ExpectedHash: digest("old"), Reason: "update",
	}}}
	txn := filepath.Join(root, transactionPrefix+"crash")
	backup := filepath.Join(txn, "backup", "game.cfg")
	if err := os.MkdirAll(filepath.Dir(backup), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(backup, []byte("old"), 0o644); err != nil {
		t.Fatal(err)
	}
	journal, err := plan.JSON()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(txn, "journal.json"), journal, 0o600); err != nil {
		t.Fatal(err)
	}
	if err := Recover(root); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(target)
	if err != nil || string(data) != "old" {
		t.Fatalf("interrupted update was not rolled back: %q, %v", data, err)
	}
	if _, err := os.Stat(txn); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("recovered transaction remains: %v", err)
	}
}
