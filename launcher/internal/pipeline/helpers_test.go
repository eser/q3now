package pipeline

import (
	"archive/zip"
	"errors"
	"io"
	"os"
	"path/filepath"
	"testing"
)

// createTestPk3ForCache creates a minimal pk3 for readerCache tests.
func createTestPk3ForCache(t *testing.T, dir, name string) string {
	t.Helper()
	path := filepath.Join(dir, name)
	f, err := os.Create(path)
	if err != nil {
		t.Fatalf("create test pk3: %v", err)
	}
	defer f.Close()
	zw := zip.NewWriter(f)
	w, _ := zw.Create("test.txt")
	w.Write([]byte("data"))
	zw.Close()
	return path
}

func TestReaderCache_LazyOpen(t *testing.T) {
	cache := newReaderCache()
	defer cache.closeAll()

	// Cache starts empty.
	if len(cache.readers) != 0 {
		t.Errorf("expected empty cache, got %d entries", len(cache.readers))
	}
}

func TestReaderCache_Dedup(t *testing.T) {
	dir := t.TempDir()
	pk3Path := createTestPk3ForCache(t, dir, "pak0.pk3")

	cache := newReaderCache()
	defer cache.closeAll()

	r1, err := cache.get(pk3Path)
	if err != nil {
		t.Fatalf("first get: %v", err)
	}

	r2, err := cache.get(pk3Path)
	if err != nil {
		t.Fatalf("second get: %v", err)
	}

	// Same reader instance returned — not opened twice.
	if r1 != r2 {
		t.Error("expected same reader instance for same path")
	}
	if len(cache.readers) != 1 {
		t.Errorf("expected 1 cached reader, got %d", len(cache.readers))
	}
}

func TestReaderCache_CloseAll(t *testing.T) {
	dir := t.TempDir()
	pk3Path := createTestPk3ForCache(t, dir, "pak0.pk3")

	cache := newReaderCache()
	_, err := cache.get(pk3Path)
	if err != nil {
		t.Fatalf("get: %v", err)
	}

	cache.closeAll()
	if len(cache.readers) != 0 {
		t.Errorf("expected empty cache after closeAll, got %d", len(cache.readers))
	}
}

func TestAtomicWrite_HappyPath(t *testing.T) {
	dir := t.TempDir()
	destPath := filepath.Join(dir, "output.dat")

	err := atomicWrite(destPath, func(w io.Writer) error {
		_, err := w.Write([]byte("hello atomic"))
		return err
	})
	if err != nil {
		t.Fatalf("atomicWrite: %v", err)
	}

	data, err := os.ReadFile(destPath)
	if err != nil {
		t.Fatalf("read output: %v", err)
	}
	if string(data) != "hello atomic" {
		t.Errorf("expected %q, got %q", "hello atomic", string(data))
	}

	// Temp file should be cleaned up.
	tmpPath := destPath + ".importing"
	if _, err := os.Stat(tmpPath); !os.IsNotExist(err) {
		t.Error("temp file should not exist after successful write")
	}
}

func TestAtomicWrite_ErrorCleansUp(t *testing.T) {
	dir := t.TempDir()
	destPath := filepath.Join(dir, "output.dat")

	writeErr := errors.New("simulated write failure")
	err := atomicWrite(destPath, func(w io.Writer) error {
		return writeErr
	})
	if !errors.Is(err, writeErr) {
		t.Fatalf("expected write error, got %v", err)
	}

	// Neither dest nor temp should exist.
	if _, err := os.Stat(destPath); !os.IsNotExist(err) {
		t.Error("dest file should not exist after failed write")
	}
	tmpPath := destPath + ".importing"
	if _, err := os.Stat(tmpPath); !os.IsNotExist(err) {
		t.Error("temp file should not exist after failed write")
	}
}
