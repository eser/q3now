package sw3z

import (
	"bytes"
	"encoding/binary"
	"hash/crc32"
	"testing"
)

func TestEntryPath(t *testing.T) {
	st := []byte("maps/q3dm1.bsptextures/wall.png")
	e := IndexEntry{StringOffset: 0, StringLength: 14}
	if got := EntryPath(e, st); got != "maps/q3dm1.bsp" {
		t.Fatalf("EntryPath = %q, want %q", got, "maps/q3dm1.bsp")
	}

	e2 := IndexEntry{StringOffset: 14, StringLength: 17}
	if got := EntryPath(e2, st); got != "textures/wall.png" {
		t.Fatalf("EntryPath = %q, want %q", got, "textures/wall.png")
	}
}

func TestCompressionName(t *testing.T) {
	tests := []struct {
		c    uint8
		want string
	}{
		{CompressNone, "Store"},
		{CompressLZ4, "LZ4"},
		{CompressZstd, "Zstd"},
		{0xFF, "0xff"},
	}
	for _, tt := range tests {
		if got := CompressionName(tt.c); got != tt.want {
			t.Errorf("CompressionName(0x%02x) = %q, want %q", tt.c, got, tt.want)
		}
	}
}

func TestDecompressLZ4Data_RoundTrip(t *testing.T) {
	original := []byte("hello world, this is a test of LZ4 round-tripping")
	compressed, err := CompressLZ4Data(original)
	if err != nil {
		t.Fatalf("CompressLZ4Data: %v", err)
	}
	decompressed, err := DecompressLZ4Data(compressed)
	if err != nil {
		t.Fatalf("DecompressLZ4Data: %v", err)
	}
	if !bytes.Equal(original, decompressed) {
		t.Fatalf("round-trip mismatch: got %q", decompressed)
	}
}

// buildTestArchive creates an in-memory sw3z archive with the given files.
// Returns the raw archive bytes.
func buildTestArchive(t *testing.T, files map[string][]byte, compression uint8) []byte {
	t.Helper()
	var buf bytes.Buffer
	w := Create(&buf)
	for path, data := range files {
		if err := w.AddFile(path, data, compression); err != nil {
			t.Fatalf("AddFile(%q): %v", path, err)
		}
	}
	if err := w.Close(); err != nil {
		t.Fatalf("Close: %v", err)
	}
	return buf.Bytes()
}

func TestVerifyEntry_Store(t *testing.T) {
	data := []byte("the quick brown fox jumps over the lazy dog")
	archive := buildTestArchive(t, map[string][]byte{"test.txt": data}, CompressNone)

	r := bytes.NewReader(archive)
	h, err := ReadHeader(r)
	if err != nil {
		t.Fatalf("ReadHeader: %v", err)
	}
	entries, err := ReadIndex(r, h.EntryCount)
	if err != nil {
		t.Fatalf("ReadIndex: %v", err)
	}

	got, err := VerifyEntry(r, entries[0])
	if err != nil {
		t.Fatalf("VerifyEntry: %v", err)
	}
	if !bytes.Equal(data, got) {
		t.Fatalf("VerifyEntry data mismatch")
	}
}

func TestVerifyEntry_LZ4(t *testing.T) {
	data := bytes.Repeat([]byte("compress me "), 100)
	archive := buildTestArchive(t, map[string][]byte{"big.txt": data}, CompressLZ4)

	r := bytes.NewReader(archive)
	h, err := ReadHeader(r)
	if err != nil {
		t.Fatalf("ReadHeader: %v", err)
	}
	entries, err := ReadIndex(r, h.EntryCount)
	if err != nil {
		t.Fatalf("ReadIndex: %v", err)
	}

	got, err := VerifyEntry(r, entries[0])
	if err != nil {
		t.Fatalf("VerifyEntry: %v", err)
	}
	if !bytes.Equal(data, got) {
		t.Fatalf("VerifyEntry data mismatch: got %d bytes, want %d", len(got), len(data))
	}
}

func TestVerifyEntry_CRCMismatch(t *testing.T) {
	data := []byte("valid data")
	archive := buildTestArchive(t, map[string][]byte{"file.bin": data}, CompressNone)

	// Corrupt the CRC32C field in the first index entry (offset 32 within the entry).
	// Entry starts at offset HeaderSize (24).
	crcOffset := HeaderSize + 32
	binary.LittleEndian.PutUint32(archive[crcOffset:crcOffset+4], 0xDEADBEEF)

	r := bytes.NewReader(archive)
	h, _ := ReadHeader(r)
	entries, _ := ReadIndex(r, h.EntryCount)

	_, err := VerifyEntry(r, entries[0])
	if err == nil {
		t.Fatal("expected CRC mismatch error, got nil")
	}
}

func TestVerifyEntry_SizeMismatch(t *testing.T) {
	data := []byte("some data")
	archive := buildTestArchive(t, map[string][]byte{"file.bin": data}, CompressNone)

	// Corrupt the UncompressedSize field (offset 28 within the entry).
	sizeOffset := HeaderSize + 28
	binary.LittleEndian.PutUint32(archive[sizeOffset:sizeOffset+4], 999)

	// Also fix CRC to match so we hit the size check, not CRC check.
	crcOffset := HeaderSize + 32
	checksum := crc32.Checksum(data, crc32.MakeTable(crc32.Castagnoli))
	binary.LittleEndian.PutUint32(archive[crcOffset:crcOffset+4], checksum)

	r := bytes.NewReader(archive)
	h, _ := ReadHeader(r)
	entries, _ := ReadIndex(r, h.EntryCount)

	_, err := VerifyEntry(r, entries[0])
	if err == nil {
		t.Fatal("expected size mismatch error, got nil")
	}
}

func TestIsPathSafe(t *testing.T) {
	tests := []struct {
		path string
		safe bool
	}{
		{"maps/q3dm1.bsp", true},
		{"textures/gothic/wall.png", true},
		{"simple.txt", true},
		{"../escape.txt", false},
		{"maps/../../../etc/passwd", false},
		{"/absolute/path.txt", false},
		{"maps/ok/../still_ok.txt", false}, // strict: any .. is rejected
	}
	for _, tt := range tests {
		if got := IsPathSafe(tt.path); got != tt.safe {
			t.Errorf("IsPathSafe(%q) = %v, want %v", tt.path, got, tt.safe)
		}
	}
}

func TestReadEntryData(t *testing.T) {
	data := []byte("entry data content")
	archive := buildTestArchive(t, map[string][]byte{"data.bin": data}, CompressNone)

	r := bytes.NewReader(archive)
	h, _ := ReadHeader(r)
	entries, _ := ReadIndex(r, h.EntryCount)
	ReadStringTable(r, h.StringTableSize) // advance past string table

	raw, err := ReadEntryData(r, entries[0])
	if err != nil {
		t.Fatalf("ReadEntryData: %v", err)
	}
	if !bytes.Equal(data, raw) {
		t.Fatalf("ReadEntryData mismatch: got %q, want %q", raw, data)
	}
}
