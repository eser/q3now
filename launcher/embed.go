package main

import (
	"embed"
	"io/fs"
	"log/slog"
	"os"
	"path/filepath"
	"runtime"
)

//go:embed all:assets/baseq3
var embeddedBaseQ3 embed.FS

// extractEmbeddedAssets writes embedded sw3z archives to the game's baseq3/
// directory. On macOS, data goes to Contents/Resources/baseq3/ (Apple bundle
// convention: code in MacOS/, data in Resources/ — required for codesign).
// On Linux, data goes alongside the binary. Skips files that are already
// present with the same size — does NOT re-extract on every startup.
func extractEmbeddedAssets(execDir string) {
	var destDir string
	if runtime.GOOS == "darwin" {
		// macOS: Contents/MacOS/../Resources/baseq3 = Contents/Resources/baseq3
		destDir = filepath.Join(execDir, "..", "Resources", "baseq3")
	} else {
		destDir = filepath.Join(execDir, "baseq3")
	}

	entries, err := fs.ReadDir(embeddedBaseQ3, "assets/baseq3")
	if err != nil {
		slog.Debug("no embedded baseq3 assets", "error", err)
		return
	}

	for _, entry := range entries {
		if entry.IsDir() || filepath.Ext(entry.Name()) != ".sw3z" {
			continue
		}

		destPath := filepath.Join(destDir, entry.Name())

		// Skip if already extracted and same size.
		srcInfo, _ := entry.Info()
		if dstInfo, err := os.Stat(destPath); err == nil && dstInfo.Size() == srcInfo.Size() {
			slog.Debug("embedded asset already present", "file", entry.Name())
			continue
		}

		data, err := fs.ReadFile(embeddedBaseQ3, "assets/baseq3/"+entry.Name())
		if err != nil {
			slog.Error("read embedded asset", "file", entry.Name(), "error", err)
			continue
		}

		os.MkdirAll(destDir, 0o755)
		if err := os.WriteFile(destPath, data, 0o644); err != nil {
			slog.Error("extract embedded asset", "file", entry.Name(), "error", err)
			continue
		}

		slog.Info("extracted embedded asset", "file", entry.Name(), "size", len(data))
	}
}
