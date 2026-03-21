package detect

import (
	"fmt"
	"os"
	"path/filepath"
	"testing"
)

// createFile creates an empty file at the given path, creating parent dirs as needed.
func createFile(t *testing.T, path string) {
	t.Helper()
	os.MkdirAll(filepath.Dir(path), 0o755)
	if err := os.WriteFile(path, []byte{}, 0o644); err != nil {
		t.Fatalf("failed to create test file %s: %v", path, err)
	}
}

// ── ScanInventory ───────────────────────────────────────────────────────────

func TestScanInventory_Q3_ParentDir(t *testing.T) {
	root := t.TempDir()
	for i := 0; i <= 8; i++ {
		createFile(t, filepath.Join(root, "baseq3", fmt.Sprintf("pak%d.pk3", i)))
	}
	createFile(t, filepath.Join(root, "missionpack", "pak0.pk3"))

	results := ScanInventory(root, "q3")

	assertFound(t, results, "q3_base", true)
	assertFound(t, results, "q3_missionpack", true)
}

func TestScanInventory_QLive_ParentDir(t *testing.T) {
	root := t.TempDir()
	createFile(t, filepath.Join(root, "baseq3", "pak00.pk3"))
	createFile(t, filepath.Join(root, "baseq3", "bin.pk3"))

	results := ScanInventory(root, "qlive")

	assertFound(t, results, "qlive_base", true)
}

func TestScanInventory_Q3_vs_QLive_Distinction(t *testing.T) {
	// Q3A directory should match q3_base but NOT qlive_base
	q3Root := t.TempDir()
	for i := 0; i <= 8; i++ {
		createFile(t, filepath.Join(q3Root, "baseq3", fmt.Sprintf("pak%d.pk3", i)))
	}

	q3Results := ScanInventory(q3Root, "q3", "qlive")
	assertFound(t, q3Results, "q3_base", true)
	assertFound(t, q3Results, "qlive_base", false) // no pak00.pk3 or bin.pk3

	// QL directory should match qlive_base but NOT q3_base
	qlRoot := t.TempDir()
	createFile(t, filepath.Join(qlRoot, "baseq3", "pak00.pk3"))
	createFile(t, filepath.Join(qlRoot, "baseq3", "bin.pk3"))

	qlResults := ScanInventory(qlRoot, "q3", "qlive")
	assertFound(t, qlResults, "qlive_base", true)
	assertFound(t, qlResults, "q3_base", false) // no pak0.pk3 through pak8.pk3
}

func TestScanInventory_Q1_CaseInsensitive(t *testing.T) {
	// Create with uppercase PAK files (like original CD installs)
	root := t.TempDir()
	createFile(t, filepath.Join(root, "Id1", "PAK0.PAK"))
	createFile(t, filepath.Join(root, "Id1", "PAK1.PAK"))

	results := ScanInventory(root, "q1")
	assertFound(t, results, "q1_base", true)

	// Also works with different directory casing
	root2 := t.TempDir()
	createFile(t, filepath.Join(root2, "ID1", "pak0.pak"))
	createFile(t, filepath.Join(root2, "ID1", "pak1.pak"))

	results2 := ScanInventory(root2, "q1")
	assertFound(t, results2, "q1_base", true)
}

func TestScanInventory_Q1_Rerelease_NestedPath(t *testing.T) {
	root := t.TempDir()
	createFile(t, filepath.Join(root, "rerelease", "id1", "pak0.pak"))

	results := ScanInventory(root, "q1")
	assertFound(t, results, "q1_rerelease_base", true)
}

func TestScanInventory_EmptyRoot(t *testing.T) {
	results := ScanInventory("", "q3")
	if results != nil {
		t.Errorf("expected nil for empty root, got %d results", len(results))
	}
}

func TestScanInventory_NonexistentRoot(t *testing.T) {
	results := ScanInventory("/nonexistent/path/that/does/not/exist", "q3")

	for _, r := range results {
		if r.Found {
			t.Errorf("expected all items not found, but %s was found", r.Item.ID)
		}
	}
}

func TestScanInventory_NoMatchingGroups(t *testing.T) {
	root := t.TempDir()
	for i := 0; i <= 8; i++ {
		createFile(t, filepath.Join(root, "baseq3", fmt.Sprintf("pak%d.pk3", i)))
	}

	// Scan for Q1 — Q3 files shouldn't match
	results := ScanInventory(root, "q1")
	for _, r := range results {
		if r.Found {
			t.Errorf("expected no Q1 items found in Q3 directory, but %s was found", r.Item.ID)
		}
	}
}

func TestScanInventory_PartialFiles_NotFound(t *testing.T) {
	// Q3A with only some pak files — should NOT match (all 9 required)
	root := t.TempDir()
	createFile(t, filepath.Join(root, "baseq3", "pak0.pk3"))
	createFile(t, filepath.Join(root, "baseq3", "pak1.pk3"))
	// Missing pak2-8

	results := ScanInventory(root, "q3")
	assertFound(t, results, "q3_base", false)
}

// ── resolveCaseInsensitiveFile ──────────────────────────────────────────────

func TestResolveCaseInsensitiveFile_ExactMatch(t *testing.T) {
	root := t.TempDir()
	createFile(t, filepath.Join(root, "baseq3", "pak0.pk3"))

	path, ok := resolveCaseInsensitiveFile(root, "baseq3/pak0.pk3")
	if !ok {
		t.Fatal("expected to resolve file")
	}
	if filepath.Base(path) != "pak0.pk3" {
		t.Errorf("expected pak0.pk3, got %s", filepath.Base(path))
	}
}

func TestResolveCaseInsensitiveFile_CaseMismatch(t *testing.T) {
	root := t.TempDir()
	createFile(t, filepath.Join(root, "Id1", "PAK0.PAK"))

	// Search with lowercase
	path, ok := resolveCaseInsensitiveFile(root, "id1/pak0.pak")
	if !ok {
		t.Fatal("expected to resolve case-mismatched file")
	}
	if filepath.Base(path) != "PAK0.PAK" {
		t.Errorf("expected PAK0.PAK, got %s", filepath.Base(path))
	}
}

func TestResolveCaseInsensitiveFile_DirMissing(t *testing.T) {
	root := t.TempDir()
	createFile(t, filepath.Join(root, "baseq3", "pak0.pk3"))

	_, ok := resolveCaseInsensitiveFile(root, "nonexistent/pak0.pk3")
	if ok {
		t.Error("expected not found for missing directory")
	}
}

func TestResolveCaseInsensitiveFile_FileMissing(t *testing.T) {
	root := t.TempDir()
	os.MkdirAll(filepath.Join(root, "baseq3"), 0o755)

	_, ok := resolveCaseInsensitiveFile(root, "baseq3/pak0.pk3")
	if ok {
		t.Error("expected not found for missing file")
	}
}

func TestResolveCaseInsensitiveFile_Symlink(t *testing.T) {
	root := t.TempDir()
	target := filepath.Join(root, "actual_dir")
	createFile(t, filepath.Join(target, "pak0.pk3"))

	link := filepath.Join(root, "baseq3")
	if err := os.Symlink(target, link); err != nil {
		t.Skip("symlinks not supported on this platform")
	}

	path, ok := resolveCaseInsensitiveFile(root, "baseq3/pak0.pk3")
	if !ok {
		t.Fatal("expected to resolve file through symlink")
	}
	if filepath.Base(path) != "pak0.pk3" {
		t.Errorf("expected pak0.pk3, got %s", filepath.Base(path))
	}
}

// ── FindFirst ───────────────────────────────────────────────────────────────

func TestFindFirst_Found(t *testing.T) {
	results := []ScanResult{
		{Item: Item{ID: "q3_base"}, Found: true, Path: "/some/path"},
		{Item: Item{ID: "q3_missionpack"}, Found: false},
	}

	r := FindFirst(results, "q3_base")
	if r == nil {
		t.Fatal("expected to find q3_base")
	}
	if r.Path != "/some/path" {
		t.Errorf("expected /some/path, got %s", r.Path)
	}
}

func TestFindFirst_NotFound(t *testing.T) {
	results := []ScanResult{
		{Item: Item{ID: "q3_base"}, Found: false},
	}

	r := FindFirst(results, "q3_base")
	if r != nil {
		t.Error("expected nil for not-found item")
	}
}

func TestFindFirst_PriorityOrder(t *testing.T) {
	results := []ScanResult{
		{Item: Item{ID: "q3_base"}, Found: true, Path: "/q3"},
		{Item: Item{ID: "qlive_base"}, Found: true, Path: "/qlive"},
	}

	r := FindFirst(results, "q3_base", "qlive_base")
	if r == nil {
		t.Fatal("expected a result")
	}
	if r.Item.ID != "q3_base" {
		t.Errorf("expected q3_base (priority), got %s", r.Item.ID)
	}
}

// ── HasAnyInGroups ──────────────────────────────────────────────────────────

func TestHasAnyInGroups_True(t *testing.T) {
	results := []ScanResult{
		{Item: Item{ID: "q3_base", Group: "q3"}, Found: true},
		{Item: Item{ID: "q3_missionpack", Group: "q3"}, Found: false},
	}

	if !HasAnyInGroups(results, "q3") {
		t.Error("expected true when q3_base is found")
	}
}

func TestHasAnyInGroups_False(t *testing.T) {
	results := []ScanResult{
		{Item: Item{ID: "q3_base", Group: "q3"}, Found: false},
		{Item: Item{ID: "q3_missionpack", Group: "q3"}, Found: false},
	}

	if HasAnyInGroups(results, "q3") {
		t.Error("expected false when nothing found")
	}
}

// ── helpers ─────────────────────────────────────────────────────────────────

func assertFound(t *testing.T, results []ScanResult, id string, expected bool) {
	t.Helper()
	for _, r := range results {
		if r.Item.ID == id {
			if r.Found != expected {
				t.Errorf("%s: expected Found=%v, got Found=%v (path=%s)", id, expected, r.Found, r.Path)
			}
			return
		}
	}
	if expected {
		t.Errorf("%s: not present in results", id)
	}
}
