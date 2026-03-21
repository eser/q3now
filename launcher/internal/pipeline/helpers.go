package pipeline

import (
	"fmt"
	"io"
	"os"
	"sync"

	"github.com/eser/q3now/launcher/internal/archive"
)

// atomicWrite writes content via writeFn to a temp file, then atomically
// renames it to destPath. Uses fsync before rename for durability.
// On writeFn error, the temp file is cleaned up.
func atomicWrite(destPath string, writeFn func(w io.Writer) error) error {
	tmpPath := destPath + ".importing"

	tmpFile, err := os.Create(tmpPath)
	if err != nil {
		return fmt.Errorf("creating temp file: %w", err)
	}
	defer func() {
		tmpFile.Close()
		// Clean up temp on error.
		if _, err := os.Stat(tmpPath); err == nil {
			os.Remove(tmpPath)
		}
	}()

	if err := writeFn(tmpFile); err != nil {
		return err
	}

	if err := tmpFile.Sync(); err != nil {
		return fmt.Errorf("syncing temp file: %w", err)
	}
	tmpFile.Close()

	if err := os.Rename(tmpPath, destPath); err != nil {
		// Cross-device fallback: copy + delete.
		if err := copyFile(tmpPath, destPath); err != nil {
			return fmt.Errorf("moving temp to destination: %w", err)
		}
		os.Remove(tmpPath)
	}

	return nil
}

// copyFile copies src to dst. Fallback for cross-device rename failures.
func copyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()

	out, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer out.Close()

	if _, err := io.Copy(out, in); err != nil {
		return err
	}
	return out.Sync()
}

// readerCache provides lazy, deduplicated archive.Reader instances.
// Open readers are tracked and closed together via closeAll().
type readerCache struct {
	mu      sync.Mutex
	readers map[string]archive.Reader
}

func newReaderCache() *readerCache {
	return &readerCache{
		readers: make(map[string]archive.Reader),
	}
}

// get returns an archive.Reader for the given path, opening it if needed.
func (c *readerCache) get(path string) (archive.Reader, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if r, ok := c.readers[path]; ok {
		return r, nil
	}

	r, err := archive.Open(path)
	if err != nil {
		return nil, err
	}
	c.readers[path] = r
	return r, nil
}

// closeAll closes all cached readers.
func (c *readerCache) closeAll() {
	c.mu.Lock()
	defer c.mu.Unlock()

	for _, r := range c.readers {
		r.Close()
	}
	c.readers = make(map[string]archive.Reader)
}
