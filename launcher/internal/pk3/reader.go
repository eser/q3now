// Package pk3 provides read access to pk3 (zip) game asset archives.
package pk3

import (
	"archive/zip"
	"fmt"
	"io"
	"os"

	"github.com/eser/q3now/launcher/internal/safepath"
)

// Entry holds metadata about a single file inside a pk3 archive.
type Entry struct {
	Path             string
	CompressedSize   uint64
	UncompressedSize uint64
	CRC32            uint32
}

// Reader provides access to a pk3 (zip) archive.
type Reader struct {
	f        *os.File
	entries  []Entry
	zipFiles []*zip.File // parallel to entries, for content access
}

// Open opens a pk3 file for reading.
// Unsafe paths (containing ".." or absolute paths) are silently skipped.
func Open(path string) (*Reader, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("pk3: open: %w", err)
	}

	info, err := f.Stat()
	if err != nil {
		f.Close()
		return nil, fmt.Errorf("pk3: stat: %w", err)
	}

	zr, err := zip.NewReader(f, info.Size())
	if err != nil {
		f.Close()
		return nil, fmt.Errorf("pk3: not a valid zip: %w", err)
	}

	r := &Reader{f: f}
	for _, zf := range zr.File {
		if !safepath.IsSafe(zf.Name) {
			continue
		}
		r.entries = append(r.entries, Entry{
			Path:             zf.Name,
			CompressedSize:   zf.CompressedSize64,
			UncompressedSize: zf.UncompressedSize64,
			CRC32:            zf.CRC32,
		})
		r.zipFiles = append(r.zipFiles, zf)
	}

	return r, nil
}

// Entries returns metadata for all files in the archive.
func (r *Reader) Entries() []Entry {
	return r.entries
}

// ReadFile decompresses and returns the contents of the file at the given index.
// The index corresponds to the Entries() slice.
func (r *Reader) ReadFile(idx int) ([]byte, error) {
	if idx < 0 || idx >= len(r.zipFiles) {
		return nil, fmt.Errorf("pk3: index %d out of range [0, %d)", idx, len(r.zipFiles))
	}

	rc, err := r.zipFiles[idx].Open()
	if err != nil {
		return nil, fmt.Errorf("pk3: open entry %q: %w", r.entries[idx].Path, err)
	}
	defer rc.Close()

	data, err := io.ReadAll(rc)
	if err != nil {
		return nil, fmt.Errorf("pk3: read entry %q: %w", r.entries[idx].Path, err)
	}

	return data, nil
}

// Close releases the underlying file handle.
func (r *Reader) Close() error {
	if r.f != nil {
		return r.f.Close()
	}
	return nil
}
