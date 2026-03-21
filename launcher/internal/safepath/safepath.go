package safepath

import (
	"path/filepath"
	"strings"
)

// IsSafe validates a zip/archive entry path against directory traversal attacks.
// Returns false for empty paths, absolute paths, paths containing "..", or null bytes.
func IsSafe(name string) bool {
	if len(name) == 0 {
		return false
	}
	if filepath.IsAbs(name) {
		return false
	}
	clean := filepath.Clean(name)
	if clean == ".." || strings.HasPrefix(clean, ".."+string(filepath.Separator)) {
		return false
	}
	if strings.ContainsRune(name, 0) {
		return false
	}
	return true
}
