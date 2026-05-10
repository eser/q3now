package config

import (
	"os"
	"path/filepath"
	"runtime"
)

// PredefinedQ3Paths returns candidate predefined Q3A/QL parent directories on Linux.
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

// SteamQ3Paths returns candidate Steam Q3A/QL parent directories on Linux.
func SteamQ3Paths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, ".steam", "steam", "SteamApps", "common",
			"Quake 3 Arena"),
		filepath.Join(home, ".steam", "steam", "SteamApps", "common",
			"Quake Live"),
		filepath.Join(home, ".var", "app", "com.valvesoftware.Steam",
			".steam", "steam", "SteamApps", "common",
			"Quake 3 Arena"),
		filepath.Join(home, ".var", "app", "com.valvesoftware.Steam",
			".steam", "steam", "SteamApps", "common",
			"Quake Live"),
	}
}

// GOGPaths returns candidate GOG Q3A paths on Linux.
func GOGPaths() []string {
	return nil // GOG on Linux has no standard path
}

// SteamQ1Paths returns candidate Steam Quake 1 paths on Linux.
func SteamQ1Paths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, ".steam", "steam", "SteamApps", "common",
			"Quake"),
		filepath.Join(home, ".var", "app", "com.valvesoftware.Steam",
			".steam", "steam", "SteamApps", "common",
			"Quake"),
	}
}

// PredefinedQ1Paths returns candidate predefined Quake 1 parent directories on Linux.
func PredefinedQ1Paths() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	return []string{
		filepath.Join(home, "q3now", "old-q1"),
	}
}

// GOGQ1Paths returns candidate GOG Quake 1 paths on Linux.
func GOGQ1Paths() []string {
	return nil
}

func gameBinaryName() string {
	arch := runtime.GOARCH
	switch arch {
	case "amd64":
		return "wired.x86_64"
	case "arm64":
		return "wired.aarch64"
	default:
		return "wired." + arch
	}
}
