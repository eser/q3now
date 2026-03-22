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

// ── Import Pipeline ─────────────────────────────────────────────────────────

func (a *App) StartImport(q3Path, q1Path string) error {
	a.mu.Lock()
	if a.importing {
		a.mu.Unlock()
		return ErrImportInProgress
	}
	a.importing = true
	a.mu.Unlock()

	// Run import in background so the frontend can navigate to ProgressScreen
	// and receive events.
	go a.runImport(q3Path, q1Path)
	return nil
}

// runImport executes the full import pipeline in a background goroutine:
//
//	scan inventory → validate → pipeline (Extract → Process → SW3Z) → manifest → done
//
// Progress events are forwarded to the frontend via Wails EventsEmit.
// Cancellation is supported via context — CancelImport() triggers it.
func (a *App) runImport(q3Path, q1Path string) {
	defer func() {
		a.mu.Lock()
		a.importing = false
		a.cancelImport = nil
		a.mu.Unlock()
	}()

	emitProgress := func(step string, current, total int, message string) {
		runtime.EventsEmit(a.ctx, "import:progress", map[string]interface{}{
			"step":    step,
			"current": int64(current),
			"total":   int64(total),
			"message": message,
		})
	}

	// Give the frontend a moment to mount ProgressScreen.
	time.Sleep(100 * time.Millisecond)

	// ── Step 1: Scan game directories ────────────────────────────────────
	emitProgress("scan", 0, 1, "Scanning game directories...")

	inventory := make(map[string]string)

	if q3Path != "" {
		for _, r := range detect.ScanInventory(q3Path, "q3", "qlive") {
			if r.Found {
				inventory[r.Item.ID] = r.Path
			}
		}
	}
	if q1Path != "" {
		for _, r := range detect.ScanInventory(q1Path, "q1") {
			if r.Found {
				inventory[r.Item.ID] = r.Path
			}
		}
	}

	emitProgress("scan", 1, 1, fmt.Sprintf("Found %d game content directories", len(inventory)))

	// Validate: need Q3A or QL base game.
	if _, ok := inventory["q3_base"]; !ok {
		if _, ok := inventory["qlive_base"]; !ok {
			runtime.EventsEmit(a.ctx, "import:error",
				fmt.Sprintf("No valid Quake 3 Arena / Quake Live installation found at %s", q3Path))
			return
		}
	}

	// ── Step 2: Run the import pipeline ──────────────────────────────────

	// Build source groups from scanned inventory.
	var sources []pipeline.SourceGroup
	if dir, ok := inventory["q3_base"]; ok {
		sources = append(sources, pipeline.SourceGroup{Origin: "q3_base", Dir: dir})
	}

	// Create cancellable context so CancelImport() can stop the pipeline.
	ctx, cancel := context.WithCancel(a.ctx)
	a.mu.Lock()
	a.cancelImport = cancel
	a.mu.Unlock()

	pip := pipeline.New(a.paths, sources,
		pipeline.WithProcessors(&pipeline.Q3BaseProcessor{}),
	)

	// Forward pipeline progress events to the frontend.
	go func() {
		for p := range pip.Progress() {
			runtime.EventsEmit(a.ctx, "import:progress", map[string]interface{}{
				"step":    p.Step,
				"current": p.Current,
				"total":   p.Total,
				"message": p.Message,
			})
		}
	}()

	if err := pip.Run(ctx); err != nil {
		// User-initiated cancel: silent return (frontend already handling it).
		if ctx.Err() != nil {
			slog.Info("import cancelled by user")
			return
		}
		slog.Error("pipeline failed", "error", err)
		runtime.EventsEmit(a.ctx, "import:error", err.Error())
		return
	}

	// ── Step 3: Write manifest (signals "assets are ready") ──────────────

	if err := manifest.WriteWithInventory(a.paths.ManifestPath(), inventory); err != nil {
		slog.Error("failed to write manifest", "error", err)
		runtime.EventsEmit(a.ctx, "import:error", err.Error())
		return
	}

	slog.Info("import complete", "inventory_size", len(inventory))

	// ── Step 4: Success notification ─────────────────────────────────────

	emitProgress("complete", 1, 1, "Import successful!")
	time.Sleep(2 * time.Second)
	runtime.EventsEmit(a.ctx, "import:complete", nil)
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
