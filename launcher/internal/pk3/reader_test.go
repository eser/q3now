package pk3

import (
	"archive/zip"
	"os"
	"path/filepath"
	"testing"
)

// createTestPk3 creates a pk3 (zip) file with the given entries.
func createTestPk3(t *testing.T, dir string, files map[string][]byte) string {
	t.Helper()
	path := filepath.Join(dir, "test.pk3")
	f, err := os.Create(path)
	if err != nil {
		t.Fatalf("failed to create test pk3: %v", err)
	}
	defer f.Close()

	zw := zip.NewWriter(f)
	for name, data := range files {
		w, err := zw.Create(name)
		if err != nil {
			t.Fatalf("failed to add %s: %v", name, err)
		}
		if _, err := w.Write(data); err != nil {
			t.Fatalf("failed to write %s: %v", name, err)
		}
	}
	if err := zw.Close(); err != nil {
		t.Fatalf("failed to close zip: %v", err)
	}
	return path
}

func TestOpen_ValidPk3(t *testing.T) {
	dir := t.TempDir()
	path := createTestPk3(t, dir, map[string][]byte{
		"maps/q3dm1.bsp":    []byte("bsp data"),
		"textures/wall.tga": []byte("tga data"),
	})

	r, err := Open(path)
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer r.Close()

	entries := r.Entries()
	if len(entries) != 2 {
		t.Fatalf("expected 2 entries, got %d", len(entries))
	}
}

func TestOpen_EmptyPk3(t *testing.T) {
	dir := t.TempDir()
	path := createTestPk3(t, dir, map[string][]byte{})

	r, err := Open(path)
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer r.Close()

	if len(r.Entries()) != 0 {
		t.Errorf("expected 0 entries, got %d", len(r.Entries()))
	}
}

func TestOpen_NotZip(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "bad.pk3")
	os.WriteFile(path, []byte("not a zip file"), 0o644)

	_, err := Open(path)
	if err == nil {
		t.Fatal("expected error for non-zip file")
	}
}

func TestOpen_FileNotFound(t *testing.T) {
	_, err := Open("/nonexistent/test.pk3")
	if err == nil {
		t.Fatal("expected error for missing file")
	}
}

func TestReadFile_Content(t *testing.T) {
	dir := t.TempDir()
	expected := []byte("hello world this is test data")
	path := createTestPk3(t, dir, map[string][]byte{
		"test.txt": expected,
	})

	r, err := Open(path)
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer r.Close()

	data, err := r.ReadFile(0)
	if err != nil {
		t.Fatalf("ReadFile failed: %v", err)
	}

	if string(data) != string(expected) {
		t.Errorf("expected %q, got %q", expected, data)
	}
}

func TestReadFile_OutOfRange(t *testing.T) {
	dir := t.TempDir()
	path := createTestPk3(t, dir, map[string][]byte{
		"test.txt": []byte("data"),
	})

	r, err := Open(path)
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer r.Close()

	_, err = r.ReadFile(5)
	if err == nil {
		t.Fatal("expected error for out-of-range index")
	}

	_, err = r.ReadFile(-1)
	if err == nil {
		t.Fatal("expected error for negative index")
	}
}

func TestEntries_Metadata(t *testing.T) {
	dir := t.TempDir()
	content := []byte("some test content for metadata check")
	path := createTestPk3(t, dir, map[string][]byte{
		"models/player.md3": content,
	})

	r, err := Open(path)
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer r.Close()

	entries := r.Entries()
	if len(entries) != 1 {
		t.Fatalf("expected 1 entry, got %d", len(entries))
	}

	e := entries[0]
	if e.Path != "models/player.md3" {
		t.Errorf("expected path models/player.md3, got %s", e.Path)
	}
	if e.UncompressedSize != uint64(len(content)) {
		t.Errorf("expected uncompressed size %d, got %d", len(content), e.UncompressedSize)
	}
}

func TestOpen_UnsafePaths(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "unsafe.pk3")

	f, _ := os.Create(path)
	zw := zip.NewWriter(f)
	// Add a safe file
	w, _ := zw.Create("safe.txt")
	w.Write([]byte("safe"))
	// Add an unsafe file with path traversal
	w, _ = zw.Create("../../../etc/passwd")
	w.Write([]byte("unsafe"))
	zw.Close()
	f.Close()

	r, err := Open(path)
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer r.Close()

	// Only the safe entry should be present
	entries := r.Entries()
	if len(entries) != 1 {
		t.Fatalf("expected 1 safe entry, got %d", len(entries))
	}
	if entries[0].Path != "safe.txt" {
		t.Errorf("expected safe.txt, got %s", entries[0].Path)
	}
}
