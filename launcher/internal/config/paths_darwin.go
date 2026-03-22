package config

import (
	"os"
	"path/filepath"
	"runtime"
)

// PredefinedQ3Paths returns candidate predefined Q3A/QL parent directories on macOS.
func PredefinedQ3Paths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, "q3now", "old-q3"),
		filepath.Join(home, "q3now", "old-qlive"),
	}
}

// SteamQ3Paths returns candidate Steam Q3A/QL parent directories on macOS.
func SteamQ3Paths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, "Library", "Application Support", "Steam",
			"SteamApps", "common", "Quake 3 Arena"),
		filepath.Join(home, "Library", "Application Support", "Steam",
			"SteamApps", "common", "Quake Live"),
	}
}

// GOGPaths returns candidate GOG Q3A parent directories on macOS.
func GOGPaths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, "Library", "Application Support", "GOG.com",
			"Games", "Quake III Arena"),
	}
}

// PredefinedQ1Paths returns candidate predefined Quake 1 parent directories on macOS.
func PredefinedQ1Paths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, "q3now", "old-q1"),
	}
}

// SteamQ1Paths returns candidate Steam Quake 1 parent directories on macOS.
func SteamQ1Paths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, "Library", "Application Support", "Steam",
			"SteamApps", "common", "Quake"),
	}
}

// GOGQ1Paths returns candidate GOG Quake 1 parent directories on macOS.
func GOGQ1Paths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, "Library", "Application Support", "GOG.com",
			"Games", "Quake"),
	}
}

func gameBinaryName() string {
	arch := runtime.GOARCH
	switch arch {
	case "arm64":
		return "q3now.arm64"
	case "amd64":
		return "q3now.x86_64"
	default:
		return "q3now"
	}
}
