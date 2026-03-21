package archive

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"log/slog"
	"os"
	"path/filepath"

	"github.com/eser/q3now/launcher/internal/safepath"
)

// PAK format constants.
const (
	pakMagic    = "PACK"
	pakHeaderSz = 12 // magic(4) + dirOffset(4) + dirSize(4)
	pakEntrySz  = 64 // name(56) + offset(4) + size(4)
	pakNameLen  = 56
)

// pakEntry holds parsed directory information for a single PAK entry.
type pakEntry struct {
	name   string
	offset uint32
	size   uint32
}

// pakReader provides read access to a Quake PAK archive.
type pakReader struct {
	f       *os.File
	entries []Entry
	raw     []pakEntry // parallel to entries, for seek/read
}

func openPAK(path string) (Reader, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("pak: open: %w", err)
	}

	info, err := f.Stat()
	if err != nil {
		f.Close()
		return nil, fmt.Errorf("pak: stat: %w", err)
	}
	fileSize := info.Size()

	// Read 12-byte header.
	var hdr [pakHeaderSz]byte
	if _, err := io.ReadFull(f, hdr[:]); err != nil {
		f.Close()
		return nil, fmt.Errorf("pak: read header: %w", err)
	}

	magic := string(hdr[0:4])
	if magic != pakMagic {
		f.Close()
		return nil, fmt.Errorf("pak: bad magic %q (expected %q)", magic, pakMagic)
	}

	dirOffset := binary.LittleEndian.Uint32(hdr[4:8])
	dirSize := binary.LittleEndian.Uint32(hdr[8:12])

	if dirSize%pakEntrySz != 0 {
		f.Close()
		return nil, fmt.Errorf("pak: directory size %d is not a multiple of %d", dirSize, pakEntrySz)
	}

	// Bounds check: directory region must fit within the file.
	if int64(dirOffset)+int64(dirSize) > fileSize {
		f.Close()
		return nil, fmt.Errorf("pak: directory region [%d, %d) exceeds file size %d",
			dirOffset, uint64(dirOffset)+uint64(dirSize), fileSize)
	}

	// Seek to directory and parse entries.
	if _, err := f.Seek(int64(dirOffset), io.SeekStart); err != nil {
		f.Close()
		return nil, fmt.Errorf("pak: seek to directory: %w", err)
	}

	entryCount := int(dirSize / pakEntrySz)
	dirBuf := make([]byte, dirSize)
	if _, err := io.ReadFull(f, dirBuf); err != nil {
		f.Close()
		return nil, fmt.Errorf("pak: read directory: %w", err)
	}

	var entries []Entry
	var raw []pakEntry

	for i := 0; i < entryCount; i++ {
		base := i * pakEntrySz
		nameBuf := dirBuf[base : base+pakNameLen]
		offset := binary.LittleEndian.Uint32(dirBuf[base+56 : base+60])
		size := binary.LittleEndian.Uint32(dirBuf[base+60 : base+64])

		// Parse null-terminated name; if no null, use all 56 bytes.
		name := parsePAKName(nameBuf)

		// Bounds check: entry data region must fit within the file.
		if int64(offset)+int64(size) > fileSize {
			f.Close()
			return nil, fmt.Errorf("pak: entry %q data region [%d, %d) exceeds file size %d",
				name, offset, uint64(offset)+uint64(size), fileSize)
		}

		if !safepath.IsSafe(name) {
			slog.Warn("skipping unsafe path in pak", "path", name, "pak", filepath.Base(path))
			continue
		}

		entries = append(entries, Entry{
			Path:             name,
			CompressedSize:   uint64(size), // PAK is uncompressed
			UncompressedSize: uint64(size),
		})
		raw = append(raw, pakEntry{name: name, offset: offset, size: size})
	}

	return &pakReader{f: f, entries: entries, raw: raw}, nil
}

// parsePAKName extracts a null-terminated (or full 56-byte) name from a buffer.
func parsePAKName(buf []byte) string {
	if idx := bytes.IndexByte(buf, 0); idx >= 0 {
		return string(buf[:idx])
	}
	return string(buf)
}

func (r *pakReader) Entries() []Entry { return r.entries }

func (r *pakReader) ReadFile(idx int) ([]byte, error) {
	if idx < 0 || idx >= len(r.raw) {
		return nil, fmt.Errorf("pak: index %d out of range [0, %d)", idx, len(r.raw))
	}

	e := r.raw[idx]
	if _, err := r.f.Seek(int64(e.offset), io.SeekStart); err != nil {
		return nil, fmt.Errorf("pak: seek to %q: %w", e.name, err)
	}

	data := make([]byte, e.size)
	if _, err := io.ReadFull(r.f, data); err != nil {
		return nil, fmt.Errorf("pak: read %q: %w", e.name, err)
	}

	return data, nil
}

func (r *pakReader) Close() error {
	if r.f != nil {
		return r.f.Close()
	}
	return nil
}
