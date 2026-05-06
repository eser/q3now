package main

import (
	"context"
	"fmt"
	"log/slog"
	"net"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/eser/q3now/launcher/internal/config"
	"github.com/eser/q3now/launcher/internal/detect"
	"github.com/eser/q3now/launcher/internal/engine"
	"github.com/eser/q3now/launcher/internal/manifest"
	"github.com/eser/q3now/launcher/internal/pipeline"
	"github.com/eser/q3now/launcher/internal/settings"
	"github.com/wailsapp/wails/v2/pkg/runtime"
)

// App is the main application struct. Methods are bound to the frontend via Wails.
type App struct {
	ctx     context.Context
	version string
	paths   *config.Paths

	mu           sync.Mutex
	importing    bool
	cancelImport context.CancelFunc

	ded *engine.DedServer
}

func NewApp(version string) *App {
	return &App{version: version}
}

func (a *App) startup(ctx context.Context) {
	a.ctx = ctx

	paths, err := config.ResolvePaths()
	if err != nil {
		slog.Error("failed to resolve paths", "error", err)
		return
	}
	a.paths = paths

	// Wire up dedicated server with Wails event callbacks.
	a.ded = engine.NewDedServer(
		func(line string) {
			runtime.EventsEmit(a.ctx, "ded:log", line)
		},
		func() {
			runtime.EventsEmit(a.ctx, "ded:stopped", nil)
		},
	)

	// Extract embedded sw3z archives (mod pak) to the game's baseq3/ directory.
	extractEmbeddedAssets(paths.ExecDir)

	pipeline.CleanupPartials(paths.BaseQ3Dir())
	slog.Info("paths resolved", "home", paths.HomeDir, "execDir", paths.ExecDir)
}

// beforeClose is called by Wails when the user tries to close the window.
// Returns true to prevent closing (when ded server is running).
func (a *App) beforeClose(ctx context.Context) bool {
	if a.ded == nil || !a.ded.IsRunning() {
		return false // allow close
	}

	// Show dialog asking what to do with the running server.
	choice, err := runtime.MessageDialog(a.ctx, runtime.MessageDialogOptions{
		Type:          runtime.QuestionDialog,
		Title:         "Dedicated server is running",
		Message:       "A dedicated server is still running. What would you like to do?",
		Buttons:       []string{"Stop & Quit", "Keep Running", "Cancel"},
		DefaultButton: "Cancel",
	})
	if err != nil {
		slog.Error("dialog error", "error", err)
		return false
	}

	switch choice {
	case "Stop & Quit":
		a.ded.Stop()
		return false // allow close after stopping
	case "Keep Running":
		return false // allow close, orphan the ded process
	default:
		return true // prevent close (Cancel)
	}
}

// ── App State ───────────────────────────────────────────────────────────────

func (a *App) GetAppState() map[string]interface{} {
	m, err := manifest.Read(a.paths.ManifestPath())
	if err != nil {
		slog.Info("no valid manifest found", "error", err)
		return map[string]interface{}{
			"screen":      "import",
			"version":     a.version,
			"assetsReady": false,
		}
	}
	return map[string]interface{}{
		"screen":      "launch",
		"version":     a.version,
		"assetsReady": true,
		"importedAt":  m.ImportedAt,
	}
}

// ── Settings ────────────────────────────────────────────────────────────────

func (a *App) GetSettings() *settings.Settings {
	return settings.Read(a.paths.SettingsPath())
}

func (a *App) SaveSettings(s *settings.Settings) error {
	return settings.Write(a.paths.SettingsPath(), s)
}

// ── Detection ───────────────────────────────────────────────────────────────

func (a *App) DetectQ3Installations() []detect.Q3Installation {
	installations := detect.Scan(a.paths)
	slog.Info("Q3A detection complete", "found", len(installations))
	return installations
}

func (a *App) DetectQ1Installations() []detect.Q1Installation {
	installations := detect.ScanQ1()
	slog.Info("Q1 detection complete", "found", len(installations))
	return installations
}

func (a *App) BrowseForDirectory() (string, error) {
	return runtime.OpenDirectoryDialog(a.ctx, runtime.OpenDialogOptions{
		Title: "Select game installation folder",
	})
}

// ── EULA ────────────────────────────────────────────────────────────────────

// GetEulaText returns the id Software demo EULA that must be accepted before
// downloading game assets.
func (a *App) GetEulaText() string {
	return eulaText
}

// HasAcceptedEula returns true if the user has previously accepted the EULA.
func (a *App) HasAcceptedEula() bool {
	s := settings.Read(a.paths.SettingsPath())
	return s.EulaAcceptedAt != ""
}

// AcceptEula records the user's acceptance of the EULA with a timestamp.
func (a *App) AcceptEula() error {
	s := settings.Read(a.paths.SettingsPath())
	s.EulaAcceptedAt = time.Now().UTC().Format(time.RFC3339)
	if err := settings.Write(a.paths.SettingsPath(), s); err != nil {
		slog.Error("EULA accept: failed to persist settings",
			"path", a.paths.SettingsPath(), "error", err)
		return err
	}
	return nil
}

// ── Free Resource Downloads ─────────────────────────────────────────────────

// CheckDownloadStatus returns whether the id-quakepack has been downloaded and extracted.
func (a *App) CheckDownloadStatus() map[string]interface{} {
	return map[string]interface{}{
		"ready": pipeline.IsBundleStaged(a.paths),
	}
}

// DownloadFreeResources downloads Q3 demo and Q3TA demo pk3 files.
func (a *App) DownloadFreeResources() error {
	a.mu.Lock()
	if a.importing {
		a.mu.Unlock()
		return ErrImportInProgress
	}
	a.importing = true
	a.mu.Unlock()

	go a.runDownload()
	return nil
}

func (a *App) runDownload() {
	defer func() {
		a.mu.Lock()
		a.importing = false
		a.cancelImport = nil
		a.mu.Unlock()
	}()

	ctx, cancel := context.WithCancel(a.ctx)
	a.mu.Lock()
	a.cancelImport = cancel
	a.mu.Unlock()

	reporter := NewWailsReporter(a.ctx, "download", 500*time.Millisecond)
	// Errors are surfaced via reporter.Error to the frontend; the return
	// value is ignored here (the GUI flow uses event-driven progress).
	_ = pipeline.RunDownload(ctx, a.paths, pipeline.DownloadOpts{}, reporter)
}

// ── Import Pipeline ─────────────────────────────────────────────────────────

func (a *App) StartImport(q3Path string) error {
	a.mu.Lock()
	if a.importing {
		a.mu.Unlock()
		return ErrImportInProgress
	}
	a.importing = true
	a.mu.Unlock()

	// Run import in background so the frontend can navigate to ProgressScreen
	// and receive events.
	go a.runImport(q3Path)
	return nil
}

// runImport executes the multi-pass import pipeline in a background
// goroutine, forwarding progress events to the Wails frontend via
// WailsReporter. Orchestration logic lives in pipeline.RunImport.
func (a *App) runImport(q3Path string) {
	defer func() {
		a.mu.Lock()
		a.importing = false
		a.cancelImport = nil
		a.mu.Unlock()
	}()

	// Give the frontend a moment to mount ProgressScreen before the
	// first progress event arrives.
	time.Sleep(100 * time.Millisecond)

	ctx, cancel := context.WithCancel(a.ctx)
	a.mu.Lock()
	a.cancelImport = cancel
	a.mu.Unlock()

	reporter := NewWailsReporter(a.ctx, "import", 2*time.Second)
	_ = pipeline.RunImport(ctx, a.paths, pipeline.ImportOpts{Q3Path: q3Path}, reporter)
}

func (a *App) CancelImport() {
	a.mu.Lock()
	defer a.mu.Unlock()
	if a.cancelImport != nil {
		slog.Info("cancelling import")
		a.cancelImport()
	}
}

// ── Game Launch ─────────────────────────────────────────────────────────────

// LaunchGame launches the game client with no args (opens game menu).
func (a *App) LaunchGame() error {
	slog.Info("quick launching game")
	return engine.Launch(a.paths, nil)
}

// LaunchGameWithArgs launches the game client with the given arguments.
func (a *App) LaunchGameWithArgs(args []string) error {
	slog.Info("launching game with options", "args", args)

	// Save connect address to recent servers if applicable.
	for i, arg := range args {
		if arg == "+connect" && i+1 < len(args) {
			s := settings.Read(a.paths.SettingsPath())
			s.AddRecentServer(args[i+1])
			settings.Write(a.paths.SettingsPath(), s)
			break
		}
	}

	return engine.Launch(a.paths, args)
}

// BuildGameCommand returns the command preview string for the game launch.
func (a *App) BuildGameCommand(opts map[string]interface{}) string {
	gameBin := filepath.Base(a.paths.GameBinaryPath())
	parts := []string{gameBin}

	if renderer, ok := opts["renderer"].(string); ok && renderer != "" {
		parts = append(parts, "+set", "cl_renderer", renderer)
	}
	if name, ok := opts["playerName"].(string); ok && name != "" {
		parts = append(parts, "+set", "name", fmt.Sprintf("%q", name))
	}
	if mapName, ok := opts["map"].(string); ok && mapName != "" {
		parts = append(parts, "+map", mapName)
	} else if connect, ok := opts["connect"].(string); ok && connect != "" {
		parts = append(parts, "+connect", connect)
	}
	if custom, ok := opts["customArgs"].(string); ok && custom != "" {
		parts = append(parts, strings.Fields(custom)...)
	}

	return strings.Join(parts, " ")
}

// ── Dedicated Server ────────────────────────────────────────────────────────

func (a *App) StartDedicated(config engine.ServerConfig) error {
	return a.ded.Start(a.paths.DedBinaryPath(), config)
}

func (a *App) StopDedicated() error {
	return a.ded.Stop()
}

func (a *App) GetDedStatus() bool {
	return a.ded.IsRunning()
}

func (a *App) GetDedCommandPreview(config engine.ServerConfig) string {
	return engine.CommandPreview(a.paths.DedBinaryPath(), config)
}

// ── Utilities ───────────────────────────────────────────────────────────────

func (a *App) GetLocalIP() string {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return "unknown"
	}
	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok && !ipnet.IP.IsLoopback() && ipnet.IP.To4() != nil {
			return ipnet.IP.String()
		}
	}
	return "unknown"
}
