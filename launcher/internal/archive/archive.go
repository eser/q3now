// Package archive provides a unified read interface for game asset archives.
//
// Supported formats:
//   - .pk3 (ZIP-based, Quake 3)
//   - .pak (Quake 1/2, uncompressed)
//
// Usage:
//
//	r, err := archive.Open("baseq3/pak0.pk3")
//	defer r.Close()
//	for i, e := range r.Entries() {
//	    data, _ := r.ReadFile(i) // decompressed
//	}
package archive

import (
	"fmt"
	"path/filepath"
	"strings"
)

// Entry holds metadata about a single file inside a game archive.
type Entry struct {
	Path             string
	CompressedSize   uint64
	UncompressedSize uint64
	CRC32            uint32
}

// Reader provides read access to a game archive (PAK or PK3).
// ReadFile always returns decompressed data regardless of the source format.
type Reader interface {
	Entries() []Entry
	ReadFile(idx int) ([]byte, error)
	Close() error
}

// Open detects the archive format by extension and returns the appropriate reader.
func Open(path string) (Reader, error) {
	ext := strings.ToLower(filepath.Ext(path))
	switch ext {
	case ".pk3":
		return openPK3(path)
	case ".pak":
		return openPAK(path)
	default:
		return nil, fmt.Errorf("archive: unsupported format %q", ext)
	}
}
