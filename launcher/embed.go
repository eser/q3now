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

// extractEmbeddedAssets writes embedded sw3z archives to the game's base/
// directory. The directory name must match the engine BASEGAME ("base" in
// code/qcommon/q_shared.h) so the engine actually scans the extracted paks.
// On macOS, data goes to Contents/Resources/base/ (Apple bundle convention:
// code in MacOS/, data in Resources/ — required for codesign). On Linux, data
// goes alongside the binary. Skips files that are already present with the same
// size — does NOT re-extract on every startup.
func extractEmbeddedAssets(execDir string) {
	var destDir string
	if runtime.GOOS == "darwin" {
		// macOS: Contents/MacOS/../Resources/base = Contents/Resources/base
		destDir = filepath.Join(execDir, "..", "Resources", "base")
	} else {
		destDir = filepath.Join(execDir, "base")
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
