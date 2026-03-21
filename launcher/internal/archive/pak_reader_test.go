package archive

import (
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

// createTestPak writes a valid PAK archive to dir/name with the given entries.
// Returns the full path. Files maps entry path → content.
func createTestPak(t *testing.T, dir, name string, files map[string][]byte) string {
	t.Helper()

	path := filepath.Join(dir, name)
	f, err := os.Create(path)
	if err != nil {
		t.Fatalf("create test pak: %v", err)
	}
	defer f.Close()

	type dirEntry struct {
		name   string
		offset uint32
		size   uint32
	}

	// Write placeholder header (will rewrite after we know offsets).
	var hdr [pakHeaderSz]byte
	f.Write(hdr[:])

	// Write file data and track offsets.
	var entries []dirEntry
	for name, data := range files {
		offset, _ := f.Seek(0, 1) // current position
		f.Write(data)
		entries = append(entries, dirEntry{
			name:   name,
			offset: uint32(offset),
			size:   uint32(len(data)),
		})
	}

	// Write directory.
	dirOffset, _ := f.Seek(0, 1)
	for _, e := range entries {
		var buf [pakEntrySz]byte
		copy(buf[:pakNameLen], e.name)
		binary.LittleEndian.PutUint32(buf[56:60], e.offset)
		binary.LittleEndian.PutUint32(buf[60:64], e.size)
		f.Write(buf[:])
	}

	// Rewrite header with correct offsets.
	dirSize := uint32(len(entries) * pakEntrySz)
	copy(hdr[0:4], pakMagic)
	binary.LittleEndian.PutUint32(hdr[4:8], uint32(dirOffset))
	binary.LittleEndian.PutUint32(hdr[8:12], dirSize)
	f.Seek(0, 0)
	f.Write(hdr[:])

	return path
}

func TestPAK_ValidArchive(t *testing.T) {
	dir := t.TempDir()
	path := createTestPak(t, dir, "test.pak", map[string][]byte{
		"maps/dm1.bsp":     []byte("bsp data here"),
		"textures/wall.tga": []byte("tga pixels"),
	})

	r, err := openPAK(path)
	if err != nil {
		t.Fatalf("openPAK failed: %v", err)
	}
	defer r.Close()

	entries := r.Entries()
	if len(entries) != 2 {
		t.Fatalf("expected 2 entries, got %d", len(entries))
	}
}

func TestPAK_EmptyArchive(t *testing.T) {
	dir := t.TempDir()
	path := createTestPak(t, dir, "empty.pak", map[string][]byte{})

	r, err := openPAK(path)
	if err != nil {
		t.Fatalf("openPAK failed: %v", err)
	}
	defer r.Close()

	if len(r.Entries()) != 0 {
		t.Errorf("expected 0 entries, got %d", len(r.Entries()))
	}
}

func TestPAK_BadMagic(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "bad.pak")
	os.WriteFile(path, []byte("NOT_A_PAK_FILE!!"), 0o644)

	_, err := openPAK(path)
	if err == nil {
		t.Fatal("expected error for bad magic")
	}
}

func TestPAK_TruncatedHeader(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "truncated.pak")
	os.WriteFile(path, []byte("PACK"), 0o644) // only 4 bytes, need 12

	_, err := openPAK(path)
	if err == nil {
		t.Fatal("expected error for truncated header")
	}
}

func TestPAK_EntryOOBOffset(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "oob.pak")

	// Manually craft a PAK with an entry pointing beyond the file.
	f, _ := os.Create(path)
	var hdr [pakHeaderSz]byte
	copy(hdr[0:4], pakMagic)
	binary.LittleEndian.PutUint32(hdr[4:8], pakHeaderSz)  // dir at offset 12
	binary.LittleEndian.PutUint32(hdr[8:12], pakEntrySz)   // 1 entry
	f.Write(hdr[:])

	// Write directory entry with offset way beyond file.
	var entry [pakEntrySz]byte
	copy(entry[:pakNameLen], "maps/evil.bsp")
	binary.LittleEndian.PutUint32(entry[56:60], 999999) // offset beyond file
	binary.LittleEndian.PutUint32(entry[60:64], 100)    // size
	f.Write(entry[:])
	f.Close()

	_, err := openPAK(path)
	if err == nil {
		t.Fatal("expected error for out-of-bounds entry offset")
	}
}

func TestPAK_UnsafePaths(t *testing.T) {
	dir := t.TempDir()

	// We need to craft a PAK with an unsafe path manually since createTestPak
	// uses the path as a map key (which is fine), but the reader should skip it.
	path := filepath.Join(dir, "unsafe.pak")
	f, _ := os.Create(path)

	// Write safe data + unsafe data.
	var hdr [pakHeaderSz]byte
	f.Write(hdr[:]) // placeholder

	safeData := []byte("safe content")
	safeOffset := pakHeaderSz
	f.Write(safeData)

	unsafeData := []byte("unsafe content")
	f.Write(unsafeData)

	// Directory at current position.
	dirOffset, _ := f.Seek(0, 1)

	// Safe entry.
	var e1 [pakEntrySz]byte
	copy(e1[:pakNameLen], "safe.txt")
	binary.LittleEndian.PutUint32(e1[56:60], uint32(safeOffset))
	binary.LittleEndian.PutUint32(e1[60:64], uint32(len(safeData)))
	f.Write(e1[:])

	// Unsafe entry with path traversal.
	var e2 [pakEntrySz]byte
	copy(e2[:pakNameLen], "../../../etc/passwd")
	binary.LittleEndian.PutUint32(e2[56:60], uint32(safeOffset+len(safeData)))
	binary.LittleEndian.PutUint32(e2[60:64], uint32(len(unsafeData)))
	f.Write(e2[:])

	// Rewrite header.
	copy(hdr[0:4], pakMagic)
	binary.LittleEndian.PutUint32(hdr[4:8], uint32(dirOffset))
	binary.LittleEndian.PutUint32(hdr[8:12], 2*pakEntrySz)
	f.Seek(0, 0)
	f.Write(hdr[:])
	f.Close()

	r, err := openPAK(path)
	if err != nil {
		t.Fatalf("openPAK failed: %v", err)
	}
	defer r.Close()

	entries := r.Entries()
	if len(entries) != 1 {
		t.Fatalf("expected 1 safe entry, got %d", len(entries))
	}
	if entries[0].Path != "safe.txt" {
		t.Errorf("expected safe.txt, got %s", entries[0].Path)
	}
}

func TestPAK_ReadFileContent(t *testing.T) {
	dir := t.TempDir()
	expected := []byte("hello world test data for PAK")
	path := createTestPak(t, dir, "content.pak", map[string][]byte{
		"test.txt": expected,
	})

	r, err := openPAK(path)
	if err != nil {
		t.Fatalf("openPAK failed: %v", err)
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

func TestPAK_ReadFileOutOfRange(t *testing.T) {
	dir := t.TempDir()
	path := createTestPak(t, dir, "range.pak", map[string][]byte{
		"one.txt": []byte("data"),
	})

	r, err := openPAK(path)
	if err != nil {
		t.Fatalf("openPAK failed: %v", err)
	}
	defer r.Close()

	if _, err := r.ReadFile(5); err == nil {
		t.Fatal("expected error for out-of-range index")
	}
	if _, err := r.ReadFile(-1); err == nil {
		t.Fatal("expected error for negative index")
	}
}

func TestPAK_FileNotFound(t *testing.T) {
	_, err := openPAK("/nonexistent/path/test.pak")
	if err == nil {
		t.Fatal("expected error for missing file")
	}
}

func TestPAK_NameFills56Bytes(t *testing.T) {
	dir := t.TempDir()

	// Create a PAK with a name that uses all 56 bytes (no null terminator).
	longName := "abcdefghijklmnopqrstuvwxyz/0123456789/abcdefghijklmnop" // exactly 56 bytes
	if len(longName) != pakNameLen {
		// Pad or trim to exactly 56.
		for len(longName) < pakNameLen {
			longName += "x"
		}
		longName = longName[:pakNameLen]
	}

	path := createTestPak(t, dir, "longname.pak", map[string][]byte{
		longName: []byte("content"),
	})

	r, err := openPAK(path)
	if err != nil {
		t.Fatalf("openPAK failed: %v", err)
	}
	defer r.Close()

	entries := r.Entries()
	if len(entries) != 1 {
		t.Fatalf("expected 1 entry, got %d", len(entries))
	}
	if entries[0].Path != longName {
		t.Errorf("expected %q, got %q", longName, entries[0].Path)
	}
}
