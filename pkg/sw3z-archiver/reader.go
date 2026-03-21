package sw3z

import (
	"encoding/binary"
	"fmt"
	"io"
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
