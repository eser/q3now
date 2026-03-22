package config

import "runtime"

// PredefinedQ3Paths returns candidate predefined Q3A/QL parent directories on Windows.
func PredefinedQ3Paths() []string {
	return []string{
		`C:\q3now\old-q3`,
		`C:\q3now\old-qlive`,
	}
}

// SteamQ3Paths returns candidate Steam Q3A/QL parent directories on Windows.
func SteamQ3Paths() []string {
	// TODO: read Steam install path from Windows registry
	// HKLM\SOFTWARE\WOW6432Node\Valve\Steam -> InstallPath
	return []string{
		`C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena`,
		`C:\Program Files (x86)\Steam\steamapps\common\Quake Live`,
	}
}

// GOGPaths returns candidate GOG Q3A paths on Windows.
func GOGPaths() []string {
	return []string{
		`C:\GOG Games\Quake III Arena`,
	}
}

// SteamQ1Paths returns candidate Steam Quake 1 paths on Windows.
func SteamQ1Paths() []string {
	return []string{
		`C:\Program Files (x86)\Steam\steamapps\common\Quake`,
	}
}

// PredefinedQ1Paths returns candidate predefined Quake 1 parent directories on Windows.
func PredefinedQ1Paths() []string {
	return []string{
		`C:\q3now\old-q1`,
	}
}

// GOGQ1Paths returns candidate GOG Quake 1 paths on Windows.
func GOGQ1Paths() []string {
	return []string{
		`C:\GOG Games\Quake`,
	}
}

func gameBinaryName() string {
	arch := runtime.GOARCH
	switch arch {
	case "amd64":
		return "q3now.x64.exe"
	case "arm64":
		return "q3now.arm64.exe"
	default:
		return "q3now." + arch + ".exe"
	}
}
