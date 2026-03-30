package sw3z

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"hash/crc32"
	"io"
	"path/filepath"
	"strings"

	"github.com/pierrec/lz4/v4"
)

// Header represents the 24-byte sw3z file header.
type Header struct {
	Magic           uint32
	Version         uint16
	Flags           uint16
	EntryCount      uint32
	StringTableSize uint32
	DataOffset      uint64
}

// IndexEntry represents a 40-byte index entry.
type IndexEntry struct {
	PathHash         uint64
	StringOffset     uint32
	StringLength     uint32
	DataOffset       uint64
	CompressedSize   uint32
	UncompressedSize uint32
	CRC32C           uint32
	Compression      uint8
	Flags            uint8
	Alignment        uint8
	Reserved         uint8
}

// ReadHeader reads and validates the 24-byte header from r.
func ReadHeader(r io.Reader) (*Header, error) {
	var buf [HeaderSize]byte
	if _, err := io.ReadFull(r, buf[:]); err != nil {
		return nil, fmt.Errorf("sw3z: read header: %w", err)
	}

	h := &Header{
		Magic:           binary.LittleEndian.Uint32(buf[0:4]),
		Version:         binary.LittleEndian.Uint16(buf[4:6]),
		Flags:           binary.LittleEndian.Uint16(buf[6:8]),
		EntryCount:      binary.LittleEndian.Uint32(buf[8:12]),
		StringTableSize: binary.LittleEndian.Uint32(buf[12:16]),
		DataOffset:      binary.LittleEndian.Uint64(buf[16:24]),
	}

	if h.Magic != Magic {
		return nil, fmt.Errorf("sw3z: invalid magic 0x%08X (expected 0x%08X)", h.Magic, Magic)
	}

	return h, nil
}

// ReadIndex reads count index entries from r.
func ReadIndex(r io.Reader, count uint32) ([]IndexEntry, error) {
	entries := make([]IndexEntry, count)
	for i := uint32(0); i < count; i++ {
		var buf [EntrySize]byte
		if _, err := io.ReadFull(r, buf[:]); err != nil {
			return nil, fmt.Errorf("sw3z: read index entry %d: %w", i, err)
		}
		entries[i] = IndexEntry{
			PathHash:         binary.LittleEndian.Uint64(buf[0:8]),
			StringOffset:     binary.LittleEndian.Uint32(buf[8:12]),
			StringLength:     binary.LittleEndian.Uint32(buf[12:16]),
			DataOffset:       binary.LittleEndian.Uint64(buf[16:24]),
			CompressedSize:   binary.LittleEndian.Uint32(buf[24:28]),
			UncompressedSize: binary.LittleEndian.Uint32(buf[28:32]),
			CRC32C:           binary.LittleEndian.Uint32(buf[32:36]),
			Compression:      buf[36],
			Flags:            buf[37],
			Alignment:        buf[38],
			Reserved:         buf[39],
		}
	}
	return entries, nil
}

// ReadStringTable reads the string table bytes from r.
func ReadStringTable(r io.Reader, size uint32) ([]byte, error) {
	buf := make([]byte, size)
	if _, err := io.ReadFull(r, buf); err != nil {
		return nil, fmt.Errorf("sw3z: read string table: %w", err)
	}
	return buf, nil
}

// EntryPath extracts the file path for an index entry from the string table.
func EntryPath(e IndexEntry, stringTable []byte) string {
	return string(stringTable[e.StringOffset : e.StringOffset+e.StringLength])
}

// CompressionName returns a human-readable name for a compression byte.
func CompressionName(c uint8) string {
	switch c {
	case CompressNone:
		return "Store"
	case CompressLZ4:
		return "LZ4"
	case CompressZstd:
		return "Zstd"
	default:
		return fmt.Sprintf("0x%02x", c)
	}
}

// DecompressLZ4Data decompresses LZ4 Frame Format data.
func DecompressLZ4Data(data []byte) ([]byte, error) {
	zr := lz4.NewReader(bytes.NewReader(data))
	out, err := io.ReadAll(zr)
	if err != nil {
		return nil, fmt.Errorf("sw3z: lz4 decompress: %w", err)
	}
	return out, nil
}

// ReadEntryData reads the compressed/raw data for one entry using random access.
func ReadEntryData(rs io.ReadSeeker, e IndexEntry) ([]byte, error) {
	if _, err := rs.Seek(int64(e.DataOffset), io.SeekStart); err != nil {
		return nil, fmt.Errorf("sw3z: seek to offset %d: %w", e.DataOffset, err)
	}
	buf := make([]byte, e.CompressedSize)
	if _, err := io.ReadFull(rs, buf); err != nil {
		return nil, fmt.Errorf("sw3z: read entry data: %w", err)
	}
	return buf, nil
}

// DecompressEntry decompresses entry data based on its compression method.
func DecompressEntry(compressedData []byte, e IndexEntry) ([]byte, error) {
	switch e.Compression {
	case CompressNone:
		return compressedData, nil
	case CompressLZ4:
		return DecompressLZ4Data(compressedData)
	default:
		return nil, fmt.Errorf("sw3z: unsupported compression 0x%02x", e.Compression)
	}
}

// VerifyEntry reads, decompresses, and CRC-checks an entry.
// Returns the uncompressed data on success.
func VerifyEntry(rs io.ReadSeeker, e IndexEntry) ([]byte, error) {
	compressed, err := ReadEntryData(rs, e)
	if err != nil {
		return nil, err
	}
	data, err := DecompressEntry(compressed, e)
	if err != nil {
		return nil, err
	}
	actual := crc32.Checksum(data, crc32cTable)
	if actual != e.CRC32C {
		return nil, fmt.Errorf("sw3z: CRC32C mismatch: expected 0x%08X, got 0x%08X", e.CRC32C, actual)
	}
	if uint32(len(data)) != e.UncompressedSize {
		return nil, fmt.Errorf("sw3z: size mismatch: expected %d, got %d", e.UncompressedSize, len(data))
	}
	return data, nil
}

// IsPathSafe rejects paths with ".." segments or absolute paths.
func IsPathSafe(path string) bool {
	if filepath.IsAbs(path) {
		return false
	}
	for _, part := range strings.Split(filepath.ToSlash(path), "/") {
		if part == ".." {
			return false
		}
	}
	return true
}
