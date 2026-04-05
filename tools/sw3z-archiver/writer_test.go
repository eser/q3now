package sw3z

import (
	"bytes"
	"hash/crc32"
	"testing"
)

func TestWriter_EmptyArchive(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)
	if err := w.Close(); err != nil {
		t.Fatalf("Close failed: %v", err)
	}

	// Verify header.
	r := bytes.NewReader(buf.Bytes())
	h, err := ReadHeader(r)
	if err != nil {
		t.Fatalf("ReadHeader failed: %v", err)
	}

	if h.Magic != Magic {
		t.Errorf("magic: expected 0x%08X, got 0x%08X", Magic, h.Magic)
	}
	if h.Version != Version {
		t.Errorf("version: expected %d, got %d", Version, h.Version)
	}
	if h.EntryCount != 0 {
		t.Errorf("entry_count: expected 0, got %d", h.EntryCount)
	}
	if h.StringTableSize != 0 {
		t.Errorf("string_table_size: expected 0, got %d", h.StringTableSize)
	}
	// data_offset = header only (no index, no string table)
	if h.DataOffset != HeaderSize {
		t.Errorf("data_offset: expected %d, got %d", HeaderSize, h.DataOffset)
	}
	// Total size should be just the header.
	if buf.Len() != HeaderSize {
		t.Errorf("total size: expected %d, got %d", HeaderSize, buf.Len())
	}
}

func TestWriter_SingleFile(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)

	data := []byte("hello world")
	if err := w.AddFile("maps/q3dm1.bsp", data, CompressNone); err != nil {
		t.Fatalf("AddFile failed: %v", err)
	}
	if err := w.Close(); err != nil {
		t.Fatalf("Close failed: %v", err)
	}

	// Read back and verify.
	r := bytes.NewReader(buf.Bytes())
	h, err := ReadHeader(r)
	if err != nil {
		t.Fatalf("ReadHeader failed: %v", err)
	}
	if h.EntryCount != 1 {
		t.Fatalf("entry_count: expected 1, got %d", h.EntryCount)
	}

	entries, err := ReadIndex(r, h.EntryCount)
	if err != nil {
		t.Fatalf("ReadIndex failed: %v", err)
	}

	st, err := ReadStringTable(r, h.StringTableSize)
	if err != nil {
		t.Fatalf("ReadStringTable failed: %v", err)
	}

	e := entries[0]
	// Verify path string.
	path := string(st[e.StringOffset : e.StringOffset+e.StringLength])
	if path != "maps/q3dm1.bsp" {
		t.Errorf("path: expected 'maps/q3dm1.bsp', got %q", path)
	}

	// Verify hash matches path.
	if e.PathHash != FNV1a64("maps/q3dm1.bsp") {
		t.Errorf("path_hash mismatch")
	}

	// Verify sizes.
	if e.CompressedSize != uint32(len(data)) {
		t.Errorf("compressed_size: expected %d, got %d", len(data), e.CompressedSize)
	}
	if e.UncompressedSize != uint32(len(data)) {
		t.Errorf("uncompressed_size: expected %d, got %d", len(data), e.UncompressedSize)
	}

	// Verify CRC32C.
	expectedCRC := crc32.Checksum(data, crc32.MakeTable(crc32.Castagnoli))
	if e.CRC32C != expectedCRC {
		t.Errorf("crc32c: expected 0x%08X, got 0x%08X", expectedCRC, e.CRC32C)
	}

	// Verify data_offset points to actual data.
	if e.DataOffset != h.DataOffset {
		t.Errorf("data_offset: expected %d, got %d", h.DataOffset, e.DataOffset)
	}

	// Verify actual data at the offset.
	allBytes := buf.Bytes()
	actualData := allBytes[e.DataOffset : e.DataOffset+uint64(e.CompressedSize)]
	if string(actualData) != "hello world" {
		t.Errorf("data: expected 'hello world', got %q", actualData)
	}
}

func TestWriter_MultipleFiles(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)

	files := []struct {
		path string
		data []byte
	}{
		{"textures/wall.tga", []byte("wall texture data")},
		{"models/player.md3", []byte("player model data")},
		{"sounds/jump.wav", []byte("jump sound data")},
	}

	for _, f := range files {
		if err := w.AddFile(f.path, f.data, CompressNone); err != nil {
			t.Fatalf("AddFile %s failed: %v", f.path, err)
		}
	}
	if err := w.Close(); err != nil {
		t.Fatalf("Close failed: %v", err)
	}

	r := bytes.NewReader(buf.Bytes())
	h, err := ReadHeader(r)
	if err != nil {
		t.Fatalf("ReadHeader failed: %v", err)
	}
	if h.EntryCount != 3 {
		t.Fatalf("entry_count: expected 3, got %d", h.EntryCount)
	}

	entries, err := ReadIndex(r, h.EntryCount)
	if err != nil {
		t.Fatalf("ReadIndex failed: %v", err)
	}
	st, _ := ReadStringTable(r, h.StringTableSize)

	// Verify each entry has correct data.
	allBytes := buf.Bytes()
	for i, f := range files {
		e := entries[i]
		path := string(st[e.StringOffset : e.StringOffset+e.StringLength])
		if path != f.path {
			t.Errorf("entry %d path: expected %q, got %q", i, f.path, path)
		}
		actual := allBytes[e.DataOffset : e.DataOffset+uint64(e.CompressedSize)]
		if string(actual) != string(f.data) {
			t.Errorf("entry %d data mismatch", i)
		}
	}
}

func TestWriter_FNV1a64(t *testing.T) {
	// Test normalization: uppercase and backslashes are handled.
	h1 := FNV1a64("maps/q3dm1.bsp")
	h2 := FNV1a64("Maps/Q3DM1.BSP")
	h3 := FNV1a64("maps\\q3dm1.bsp")

	if h1 != h2 {
		t.Errorf("FNV1a64 should be case-insensitive: %x != %x", h1, h2)
	}
	if h1 != h3 {
		t.Errorf("FNV1a64 should normalize separators: %x != %x", h1, h3)
	}

	// Different paths should produce different hashes.
	h4 := FNV1a64("textures/wall.tga")
	if h1 == h4 {
		t.Error("different paths should have different hashes")
	}

	// Trailing slash matters (directory vs file).
	h5 := FNV1a64("textures/gothic/")
	h6 := FNV1a64("textures/gothic")
	if h5 == h6 {
		t.Error("trailing slash should produce different hash")
	}
}

func TestWriter_CRC32C(t *testing.T) {
	// Verify we use Castagnoli (not IEEE).
	data := []byte("test data for checksum")
	castagnoli := crc32.Checksum(data, crc32.MakeTable(crc32.Castagnoli))
	ieee := crc32.ChecksumIEEE(data)

	if castagnoli == ieee {
		t.Skip("CRC32C and IEEE happen to collide for this input")
	}

	var buf bytes.Buffer
	w := Create(&buf)
	w.AddFile("test.txt", data, CompressNone)
	w.Close()

	r := bytes.NewReader(buf.Bytes())
	ReadHeader(r)
	entries, _ := ReadIndex(r, 1)

	if entries[0].CRC32C != castagnoli {
		t.Errorf("expected CRC32C (Castagnoli) 0x%08X, got 0x%08X", castagnoli, entries[0].CRC32C)
	}
}

func TestWriter_PathNormalization(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)
	w.AddFile("Textures\\Gothic\\Wall.TGA", []byte("data"), CompressNone)
	w.Close()

	r := bytes.NewReader(buf.Bytes())
	h, _ := ReadHeader(r)
	entries, _ := ReadIndex(r, h.EntryCount)
	st, _ := ReadStringTable(r, h.StringTableSize)

	e := entries[0]
	path := string(st[e.StringOffset : e.StringOffset+e.StringLength])
	if path != "textures/gothic/wall.tga" {
		t.Errorf("expected normalized path 'textures/gothic/wall.tga', got %q", path)
	}
}

func TestWriter_DataOffsets(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)
	w.AddFile("a.txt", []byte("aaaa"), CompressNone)
	w.AddFile("b.txt", []byte("bbbbbb"), CompressNone)
	w.Close()

	r := bytes.NewReader(buf.Bytes())
	h, _ := ReadHeader(r)
	entries, _ := ReadIndex(r, h.EntryCount)

	// First entry's data should start at data_offset.
	if entries[0].DataOffset != h.DataOffset {
		t.Errorf("first entry data_offset: expected %d, got %d", h.DataOffset, entries[0].DataOffset)
	}

	// Second entry should follow immediately after first.
	expectedSecond := entries[0].DataOffset + uint64(entries[0].CompressedSize)
	if entries[1].DataOffset != expectedSecond {
		t.Errorf("second entry data_offset: expected %d, got %d", expectedSecond, entries[1].DataOffset)
	}
}

func TestWriter_DuplicatePath(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)
	w.AddFile("test.txt", []byte("data"), CompressNone)

	err := w.AddFile("test.txt", []byte("other"), CompressNone)
	if err == nil {
		t.Fatal("expected error for duplicate path")
	}
}

func TestWriter_EmptyPath(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)

	err := w.AddFile("", []byte("data"), CompressNone)
	if err == nil {
		t.Fatal("expected error for empty path")
	}
}

func TestWriter_EmptyData(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)
	if err := w.AddFile("empty.txt", []byte{}, CompressNone); err != nil {
		t.Fatalf("AddFile with empty data should succeed: %v", err)
	}
	if err := w.Close(); err != nil {
		t.Fatalf("Close failed: %v", err)
	}

	r := bytes.NewReader(buf.Bytes())
	h, _ := ReadHeader(r)
	entries, _ := ReadIndex(r, h.EntryCount)

	if entries[0].CompressedSize != 0 {
		t.Errorf("expected compressed_size 0, got %d", entries[0].CompressedSize)
	}
	if entries[0].UncompressedSize != 0 {
		t.Errorf("expected uncompressed_size 0, got %d", entries[0].UncompressedSize)
	}
}

func TestWriter_ClosedWriter(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)
	w.Close()

	err := w.AddFile("test.txt", []byte("data"), CompressNone)
	if err == nil {
		t.Fatal("expected error for closed writer")
	}

	err = w.Close()
	if err == nil {
		t.Fatal("expected error for double close")
	}
}

func TestWriter_UnsupportedCompression(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)

	err := w.AddFile("test.txt", []byte("data"), CompressZstd)
	if err == nil {
		t.Fatal("expected error for unsupported Zstd compression")
	}
}

func TestWriter_LZ4Compression(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)

	// Use repetitive data that compresses well.
	data := bytes.Repeat([]byte("hello world, this is compressible data! "), 100)

	if err := w.AddFile("big.txt", data, CompressLZ4); err != nil {
		t.Fatalf("AddFile with LZ4 failed: %v", err)
	}
	if err := w.Close(); err != nil {
		t.Fatalf("Close failed: %v", err)
	}

	// Read back and verify.
	r := bytes.NewReader(buf.Bytes())
	h, _ := ReadHeader(r)
	entries, _ := ReadIndex(r, h.EntryCount)

	e := entries[0]
	if e.Compression != CompressLZ4 {
		t.Errorf("compression: expected 0x%02x, got 0x%02x", CompressLZ4, e.Compression)
	}
	if e.UncompressedSize != uint32(len(data)) {
		t.Errorf("uncompressed_size: expected %d, got %d", len(data), e.UncompressedSize)
	}
	if e.CompressedSize >= e.UncompressedSize {
		t.Errorf("LZ4 should compress: compressed=%d >= uncompressed=%d", e.CompressedSize, e.UncompressedSize)
	}

	// Verify CRC32C is of the uncompressed data.
	expectedCRC := crc32.Checksum(data, crc32.MakeTable(crc32.Castagnoli))
	if e.CRC32C != expectedCRC {
		t.Errorf("crc32c should match uncompressed data: expected 0x%08x, got 0x%08x", expectedCRC, e.CRC32C)
	}
}

func TestWriter_LZ4_EmptyData(t *testing.T) {
	var buf bytes.Buffer
	w := Create(&buf)

	if err := w.AddFile("empty.bin", []byte{}, CompressLZ4); err != nil {
		t.Fatalf("AddFile with LZ4 on empty data failed: %v", err)
	}
	if err := w.Close(); err != nil {
		t.Fatalf("Close failed: %v", err)
	}

	r := bytes.NewReader(buf.Bytes())
	h, _ := ReadHeader(r)
	entries, _ := ReadIndex(r, h.EntryCount)

	if entries[0].UncompressedSize != 0 {
		t.Errorf("expected uncompressed_size 0, got %d", entries[0].UncompressedSize)
	}
}
