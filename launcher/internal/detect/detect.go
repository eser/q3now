package detect

import (
	"log/slog"
	"os"

	"github.com/eser/q3now/launcher/internal/config"
)

// Q3Installation represents a detected Q3A/QL installation directory.
type Q3Installation struct {
	Path       string `json:"path"`
	SourceType string `json:"sourceType"` // "steam", "gog"
}

// Q1Installation represents a detected Quake 1 installation directory.
type Q1Installation struct {
	Path       string `json:"path"`
	SourceType string `json:"sourceType"`
}

// Scan checks known platform paths for Q3A/QL game directories.
// Returns paths where the directory exists (no file content validation).
func Scan(paths *config.Paths) []Q3Installation {
	var found []Q3Installation

	for _, p := range config.PredefinedQ3Paths() {
		if dirExists(p) {
			found = append(found, Q3Installation{Path: p, SourceType: "predefined"})
		}
	}

	for _, p := range config.SteamQ3Paths() {
		if dirExists(p) {
			found = append(found, Q3Installation{Path: p, SourceType: "steam"})
		}
	}

	for _, p := range config.GOGPaths() {
		if dirExists(p) {
			found = append(found, Q3Installation{Path: p, SourceType: "gog"})
		}
	}

	slog.Info("Q3A/QL auto-detect", "found", len(found))
	return found
}

// ScanQ1 checks known platform paths for Quake 1 game directories.
func ScanQ1() []Q1Installation {
	var found []Q1Installation

	for _, p := range config.PredefinedQ1Paths() {
		if dirExists(p) {
			found = append(found, Q1Installation{Path: p, SourceType: "predefined"})
		}
	}

	for _, p := range config.SteamQ1Paths() {
		if dirExists(p) {
			found = append(found, Q1Installation{Path: p, SourceType: "steam"})
		}
	}

	for _, p := range config.GOGQ1Paths() {
		if dirExists(p) {
			found = append(found, Q1Installation{Path: p, SourceType: "gog"})
		}
	}

	slog.Info("Q1 auto-detect", "found", len(found))
	return found
}

func dirExists(path string) bool {
	info, err := os.Stat(path)
	return err == nil && info.IsDir()
}
