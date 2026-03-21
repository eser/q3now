package archive

import (
	"archive/zip"
	"os"
	"path/filepath"
	"testing"
)

// createTestPk3 creates a pk3 (zip) file for testing the Open dispatcher.
func createTestPk3(t *testing.T, dir, name string) string {
	t.Helper()
	path := filepath.Join(dir, name)
	f, err := os.Create(path)
	if err != nil {
		t.Fatalf("create test pk3: %v", err)
	}
	defer f.Close()

	zw := zip.NewWriter(f)
	w, _ := zw.Create("test.txt")
	w.Write([]byte("pk3 content"))
	zw.Close()
	return path
}

func TestOpen_PK3(t *testing.T) {
	dir := t.TempDir()
	path := createTestPk3(t, dir, "test.pk3")

	r, err := Open(path)
	if err != nil {
		t.Fatalf("Open(.pk3) failed: %v", err)
	}
	defer r.Close()

	if len(r.Entries()) != 1 {
		t.Errorf("expected 1 entry, got %d", len(r.Entries()))
	}
}

func TestOpen_PAK(t *testing.T) {
	dir := t.TempDir()
	path := createTestPak(t, dir, "test.pak", map[string][]byte{
		"data.txt": []byte("pak content"),
	})

	r, err := Open(path)
	if err != nil {
		t.Fatalf("Open(.pak) failed: %v", err)
	}
	defer r.Close()

	if len(r.Entries()) != 1 {
		t.Errorf("expected 1 entry, got %d", len(r.Entries()))
	}
}

func TestOpen_UnsupportedExtension(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "test.wad")
	os.WriteFile(path, []byte("wad data"), 0o644)

	_, err := Open(path)
	if err == nil {
		t.Fatal("expected error for .wad extension")
	}
}
