package config

import (
	"os"
	"path/filepath"
)

// channelSuffix is set at build time via -ldflags "-X ...config.channelSuffix=-preview"
// Empty string for public channel, "-preview" for preview, "-canary" for canary, etc.
var channelSuffix = "-preview"

var appName = "q3now" + channelSuffix

// ChannelSuffix returns the current channel suffix (e.g. "-preview", "-canary", or "").
func ChannelSuffix() string {
	return channelSuffix
}

// Paths holds all resolved filesystem paths the launcher needs.
type Paths struct {
	HomeDir string // ~/q3now/
	ExecDir string // directory containing the launcher binary
}

// ResolvePaths determines the home and executable directories.
func ResolvePaths() (*Paths, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil, err
	}

	execPath, err := os.Executable()
	if err != nil {
		return nil, err
	}
	execDir := filepath.Dir(execPath)

	return &Paths{
		HomeDir: filepath.Join(home, appName),
		ExecDir: execDir,
	}, nil
}

// BaseQ3Dir returns the path to ~/q3now/baseq3/.
func (p *Paths) BaseQ3Dir() string {
	return filepath.Join(p.HomeDir, "baseq3")
}

// MissionpackDir returns the path to ~/q3now/missionpack/.
func (p *Paths) MissionpackDir() string {
	return filepath.Join(p.HomeDir, "missionpack")
}

// ManifestPath returns the path to ~/q3now/q3now.json.
func (p *Paths) ManifestPath() string {
	return filepath.Join(p.HomeDir, "q3now.json")
}

// GameBinaryPath returns the full path to the game client binary.
func (p *Paths) GameBinaryPath() string {
	return filepath.Join(p.ExecDir, gameBinaryName())
}

// DedBinaryPath returns the full path to the dedicated server binary.
func (p *Paths) DedBinaryPath() string {
	return filepath.Join(p.ExecDir, "q3now-ded")
}

// DownloadDir returns the path to ~/q3now/downloaded/.
func (p *Paths) DownloadDir() string {
	return filepath.Join(p.HomeDir, "downloaded")
}

// SettingsPath returns the path to ~/q3now/settings.json.
func (p *Paths) SettingsPath() string {
	return filepath.Join(p.HomeDir, "settings.json")
}
