// Package sw3z implements the SW3Z game asset archive format (spec v0.9).
//
// SW3Z is an index-first, read-optimized archive format designed to replace
// pk3/zip containers. The writer collects entries in memory and writes the
// complete archive on Close().
//
// File layout:
//
//	┌─────────────────────────────────┐
//	│ File Header            (24 B)   │
//	├─────────────────────────────────┤
//	│ Index Entries        (N × 40 B) │
//	├─────────────────────────────────┤
//	│ String Table         (variable) │
//	├─────────────────────────────────┤
//	│ Asset Data           (variable) │
//	└─────────────────────────────────┘
package sw3z

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"hash/crc32"
	"io"
	"strings"

	"github.com/pierrec/lz4/v4"
)

// Format constants per SW3Z spec v0.9.
const (
	Magic      = 0x5A335753 // "SW3Z" as little-endian u32
	Version    = 1
	HeaderSize = 24
	EntrySize  = 40

	CompressNone = 0x00
	CompressLZ4  = 0x01
	CompressZstd = 0x02

	FlagExecutable = 0x01
	FlagSymlink    = 0x02
	FlagAligned    = 0x04

	SignNone    = 0x00
	SignEd25519 = 0x01
)

// crc32cTable is the CRC32C (Castagnoli) polynomial table.
var crc32cTable = crc32.MakeTable(crc32.Castagnoli)

// fileEntry holds a single file's data and metadata before writing.
type fileEntry struct {
	path        string // normalized: lowercase, forward slashes
	data        []byte // raw or compressed data
	uncompSize  uint32
	crc32c      uint32
	compression uint8
	flags       uint8
	alignment   uint8
}

// Writer builds an sw3z archive. Entries are collected in memory,
// then the complete archive is written sequentially on Close().
type Writer struct {
	w       io.Writer
	entries []fileEntry
	paths   map[string]bool // tracks added paths to reject duplicates
	closed  bool
}

// Create returns a new Writer that writes an sw3z archive to w.
func Create(w io.Writer) *Writer {
	return &Writer{
		w:     w,
		paths: make(map[string]bool),
	}
}

// AddFile adds a file to the archive. The path is normalized to lowercase
// with forward slashes. Data is the uncompressed file contents. Compression
// specifies the algorithm (CompressNone or CompressLZ4).
func (w *Writer) AddFile(path string, data []byte, compression uint8) error {
	if w.closed {
		return fmt.Errorf("sw3z: writer is closed")
	}

	path = normalizePath(path)
	if path == "" {
		return fmt.Errorf("sw3z: empty path")
	}

	if w.paths[path] {
		return fmt.Errorf("sw3z: duplicate path %q", path)
	}

	// CRC32C is always computed on the uncompressed data.
	checksum := crc32.Checksum(data, crc32cTable)

	var storedData []byte
	switch compression {
	case CompressNone:
		storedData = data
	case CompressLZ4:
		compressed, err := CompressLZ4Data(data)
		if err != nil {
			return fmt.Errorf("sw3z: lz4 compress %q: %w", path, err)
		}
		storedData = compressed
	default:
		return fmt.Errorf("sw3z: unsupported compression 0x%02x", compression)
	}

	w.entries = append(w.entries, fileEntry{
		path:        path,
		data:        storedData,
		uncompSize:  uint32(len(data)),
		crc32c:      checksum,
		compression: compression,
	})
	w.paths[path] = true

	return nil
}

// CompressLZ4Data compresses data using LZ4 Frame Format.
// Exported for use by the CLI's auto-detect logic.
func CompressLZ4Data(data []byte) ([]byte, error) {
	var buf bytes.Buffer
	zw := lz4.NewWriter(&buf)
	// Enable content checksum for spec compliance (second integrity layer).
	if err := zw.Apply(lz4.ChecksumOption(true)); err != nil {
		return nil, err
	}
	if _, err := zw.Write(data); err != nil {
		zw.Close()
		return nil, err
	}
	if err := zw.Close(); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

// Close writes the complete archive (header, index, string table, asset data)
// to the underlying writer. The Writer must not be used after Close.
func (w *Writer) Close() error {
	if w.closed {
		return fmt.Errorf("sw3z: writer already closed")
	}
	w.closed = true

	// Build string table: concatenated paths (no null terminators).
	var stringTable []byte
	type stringRef struct {
		offset uint32
		length uint32
	}
	refs := make([]stringRef, len(w.entries))
	for i, e := range w.entries {
		refs[i] = stringRef{
			offset: uint32(len(stringTable)),
			length: uint32(len(e.path)),
		}
		stringTable = append(stringTable, []byte(e.path)...)
	}

	// Calculate data_offset: header + index + string table.
	dataOffset := uint64(HeaderSize) + uint64(len(w.entries))*uint64(EntrySize) + uint64(len(stringTable))

	// Calculate per-entry data offsets (sequential, no alignment for now).
	entryDataOffsets := make([]uint64, len(w.entries))
	currentOffset := dataOffset
	for i, e := range w.entries {
		entryDataOffsets[i] = currentOffset
		currentOffset += uint64(len(e.data))
	}

	// Write header (24 bytes).
	if err := w.writeHeader(uint32(len(w.entries)), uint32(len(stringTable)), dataOffset); err != nil {
		return fmt.Errorf("sw3z: write header: %w", err)
	}

	// Write index entries (N × 40 bytes).
	for i, e := range w.entries {
		if err := w.writeIndexEntry(e, refs[i], entryDataOffsets[i]); err != nil {
			return fmt.Errorf("sw3z: write index entry %d: %w", i, err)
		}
	}

	// Write string table.
	if _, err := w.w.Write(stringTable); err != nil {
		return fmt.Errorf("sw3z: write string table: %w", err)
	}

	// Write asset data.
	for i, e := range w.entries {
		if _, err := w.w.Write(e.data); err != nil {
			return fmt.Errorf("sw3z: write data for entry %d: %w", i, err)
		}
	}

	return nil
}

func (w *Writer) writeHeader(entryCount, stringTableSize uint32, dataOffset uint64) error {
	var buf [HeaderSize]byte
	binary.LittleEndian.PutUint32(buf[0:4], Magic)            // magic
	binary.LittleEndian.PutUint16(buf[4:6], Version)          // version
	binary.LittleEndian.PutUint16(buf[6:8], 0)                // flags (no signing)
	binary.LittleEndian.PutUint32(buf[8:12], entryCount)      // entry_count
	binary.LittleEndian.PutUint32(buf[12:16], stringTableSize) // string_table_size
	binary.LittleEndian.PutUint64(buf[16:24], dataOffset)     // data_offset
	_, err := w.w.Write(buf[:])
	return err
}

func (w *Writer) writeIndexEntry(e fileEntry, ref struct{ offset, length uint32 }, dataOffset uint64) error {
	var buf [EntrySize]byte
	binary.LittleEndian.PutUint64(buf[0:8], FNV1a64(e.path))          // path_hash
	binary.LittleEndian.PutUint32(buf[8:12], ref.offset)              // string_offset
	binary.LittleEndian.PutUint32(buf[12:16], ref.length)             // string_length
	binary.LittleEndian.PutUint64(buf[16:24], dataOffset)             // data_offset
	binary.LittleEndian.PutUint32(buf[24:28], uint32(len(e.data)))    // compressed_size
	binary.LittleEndian.PutUint32(buf[28:32], e.uncompSize)           // uncompressed_size
	binary.LittleEndian.PutUint32(buf[32:36], e.crc32c)              // crc32c
	buf[36] = e.compression                                           // compression
	buf[37] = e.flags                                                  // flags
	buf[38] = e.alignment                                              // alignment
	buf[39] = 0                                                        // reserved
	_, err := w.w.Write(buf[:])
	return err
}

// FNV1a64 computes the FNV-1a 64-bit hash of a path string.
// The path should already be normalized (lowercase, forward slashes).
func FNV1a64(path string) uint64 {
	hash := uint64(0xcbf29ce484222325)
	for _, c := range []byte(path) {
		if c >= 'A' && c <= 'Z' {
			c += 32
		}
		if c == '\\' {
			c = '/'
		}
		hash ^= uint64(c)
		hash *= 0x00000100000001B3
	}
	return hash
}

// normalizePath converts a path to lowercase with forward slashes.
func normalizePath(path string) string {
	path = strings.ReplaceAll(path, "\\", "/")
	path = strings.ToLower(path)
	return path
}
