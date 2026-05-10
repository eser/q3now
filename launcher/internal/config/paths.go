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

// engineRootName is the centralized engine directory shared across every game
// built on the wired engine. Must match the literal "wired" used in
// code/unix/unix_shared.c and code/win32/win_shared.c (Sys_DefaultHomePath).
const engineRootName = "wired"

// Paths holds all resolved filesystem paths the launcher needs.
type Paths struct {
	HomeDir string // ~/wired/<product><channel>/  (matches engine fs_homepath)
	ExecDir string // directory containing the launcher binary
}

// ResolvePaths determines the home and executable directories.
//
// HomeDir layout matches the wired engine's Sys_DefaultHomePath:
//   ~/wired/<product><channel>/   on Linux/macOS
//   %USERPROFILE%\wired\<product><channel>\   on Windows
// Engine root ~/wired/ is shared across all games on the wired engine; the
// per-product subfolder isolates each game's per-user state. Keeping the
// launcher and engine on the same path is required so paks/configs/saved
// state the launcher writes are the ones the engine reads.
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
		HomeDir: filepath.Join(home, engineRootName, appName),
		ExecDir: execDir,
	}, nil
}

// BaseQ3Dir returns the path to <home>/baseq3/  (e.g. ~/wired/q3now-preview/baseq3/).
func (p *Paths) BaseQ3Dir() string {
	return filepath.Join(p.HomeDir, "baseq3")
}

// ManifestPath returns the path to <home>/q3now.json.
func (p *Paths) ManifestPath() string {
	return filepath.Join(p.HomeDir, "q3now.json")
}

// GameBinaryPath returns the full path to the game client binary.
func (p *Paths) GameBinaryPath() string {
	return filepath.Join(p.ExecDir, gameBinaryName())
}

// DedBinaryPath returns the full path to the dedicated server binary.
func (p *Paths) DedBinaryPath() string {
	return filepath.Join(p.ExecDir, "wired-ded")
}

// DownloadDir returns the path to <home>/downloaded/.
func (p *Paths) DownloadDir() string {
	return filepath.Join(p.HomeDir, "downloaded")
}

// SettingsPath returns the path to <home>/settings.json.
func (p *Paths) SettingsPath() string {
	return filepath.Join(p.HomeDir, "settings.json")
}
