package pipeline

import (
	"archive/zip"
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

// createExtractPk3 creates a pk3 file for extract tests.
func createExtractPk3(t *testing.T, dir, name string, files map[string][]byte) {
	t.Helper()
	path := filepath.Join(dir, name)
	f, err := os.Create(path)
	if err != nil {
		t.Fatalf("create pk3: %v", err)
	}
	defer f.Close()
	zw := zip.NewWriter(f)
	for name, data := range files {
		w, _ := zw.Create(name)
		w.Write(data)
	}
	zw.Close()
}

// createExtractPak creates a minimal PAK file for extract tests.
func createExtractPak(t *testing.T, dir, name string, files map[string][]byte) {
	t.Helper()
	path := filepath.Join(dir, name)
	f, err := os.Create(path)
	if err != nil {
		t.Fatalf("create pak: %v", err)
	}
	defer f.Close()

	// Write placeholder header.
	var hdr [12]byte
	f.Write(hdr[:])

	type entry struct {
		name   string
		offset uint32
		size   uint32
	}
	var entries []entry
	for n, data := range files {
		offset, _ := f.Seek(0, 1)
		f.Write(data)
		entries = append(entries, entry{n, uint32(offset), uint32(len(data))})
	}

	dirOffset, _ := f.Seek(0, 1)
	for _, e := range entries {
		var buf [64]byte
		copy(buf[:56], e.name)
		binary.LittleEndian.PutUint32(buf[56:60], e.offset)
		binary.LittleEndian.PutUint32(buf[60:64], e.size)
		f.Write(buf[:])
	}

	copy(hdr[0:4], "PACK")
	binary.LittleEndian.PutUint32(hdr[4:8], uint32(dirOffset))
	binary.LittleEndian.PutUint32(hdr[8:12], uint32(len(entries)*64))
	f.Seek(0, 0)
	f.Write(hdr[:])
}

func TestFindArchiveFiles_DualGlob(t *testing.T) {
	dir := t.TempDir()
	createExtractPk3(t, dir, "pak0.pk3", map[string][]byte{"a.txt": []byte("a")})
	createExtractPak(t, dir, "pak1.pak", map[string][]byte{"b.txt": []byte("b")})

	files, err := findArchiveFiles(dir)
	if err != nil {
		t.Fatalf("findArchiveFiles: %v", err)
	}
	if len(files) != 2 {
		t.Fatalf("expected 2 files, got %d: %v", len(files), files)
	}
}

func TestFindArchiveFiles_PK3OverPAK(t *testing.T) {
	dir := t.TempDir()
	// Both pak0.pk3 and pak0.pak exist — PK3 should win.
	createExtractPk3(t, dir, "pak0.pk3", map[string][]byte{"a.txt": []byte("a")})
	createExtractPak(t, dir, "pak0.pak", map[string][]byte{"b.txt": []byte("b")})

	files, err := findArchiveFiles(dir)
	if err != nil {
		t.Fatalf("findArchiveFiles: %v", err)
	}
	if len(files) != 1 {
		t.Fatalf("expected 1 file (PK3 preferred), got %d: %v", len(files), files)
	}
	if filepath.Ext(files[0]) != ".pk3" {
		t.Errorf("expected .pk3, got %s", files[0])
	}
}

func TestScanArchive_SourceIndex(t *testing.T) {
	dir := t.TempDir()
	createExtractPk3(t, dir, "pak0.pk3", map[string][]byte{
		"a.txt": []byte("aaa"),
		"b.txt": []byte("bbb"),
	})

	entries, err := scanArchive(filepath.Join(dir, "pak0.pk3"), "q3_base")
	if err != nil {
		t.Fatalf("scanArchive: %v", err)
	}

	for i, e := range entries {
		if e.SourceIndex != i {
			t.Errorf("entry %d: expected SourceIndex %d, got %d", i, i, e.SourceIndex)
		}
		if e.Origin != "q3_base" {
			t.Errorf("entry %d: expected origin q3_base, got %s", i, e.Origin)
		}
	}
}

func TestFindArchiveFiles_EmptyDir(t *testing.T) {
	dir := t.TempDir()
	files, err := findArchiveFiles(dir)
	if err != nil {
		t.Fatalf("findArchiveFiles: %v", err)
	}
	if len(files) != 0 {
		t.Errorf("expected 0 files in empty dir, got %d", len(files))
	}
}
