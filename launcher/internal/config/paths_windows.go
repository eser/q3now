package config

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

// GOGQ1Paths returns candidate GOG Quake 1 paths on Windows.
func GOGQ1Paths() []string {
	return []string{
		`C:\GOG Games\Quake`,
	}
}

func gameBinaryName() string {
	return "q3now.exe"
}
