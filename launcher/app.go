package main

import (
	"context"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"os"
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

// ── Free Resource Downloads ─────────────────────────────────────────────────

// CheckDownloadStatus returns the availability of downloaded demo pk3 files.
func (a *App) CheckDownloadStatus() map[string]interface{} {
	dlDir := a.paths.DownloadDir()
	demoQ3 := fileExists(filepath.Join(dlDir, "demoq3", "pak0.pk3"))
	demoTA := fileExists(filepath.Join(dlDir, "demota", "pak0.pk3"))
	return map[string]interface{}{
		"demoQ3": demoQ3,
		"demoTA": demoTA,
		"ready":  demoQ3 && demoTA,
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

	dlDir := a.paths.DownloadDir()

	downloads := []struct {
		url     string
		dest    string
		label   string
	}{
		{pipeline.QuakeDataDemoQ3DownloadUri, filepath.Join(dlDir, "demoq3", "pak0.pk3"), "Q3 Arena Demo"},
		{pipeline.QuakeDataDemoQ3TADownloadUri, filepath.Join(dlDir, "demota", "pak0.pk3"), "Q3 Team Arena Demo"},
	}

	for i, dl := range downloads {
		if ctx.Err() != nil {
			return
		}

		runtime.EventsEmit(a.ctx, "download:progress", map[string]interface{}{
			"current": i,
			"total":   len(downloads),
			"message": fmt.Sprintf("Downloading %s...", dl.label),
		})

		if fileExists(dl.dest) {
			slog.Info("already downloaded, skipping", "file", dl.dest)
			continue
		}

		if err := downloadFile(ctx, dl.url, dl.dest, func(received, total int64) {
			runtime.EventsEmit(a.ctx, "download:progress", map[string]interface{}{
				"current": i,
				"total":   len(downloads),
				"message": fmt.Sprintf("Downloading %s... (%d MB)", dl.label, received/(1024*1024)),
			})
		}); err != nil {
			if ctx.Err() != nil {
				slog.Info("download cancelled by user")
				return
			}
			slog.Error("download failed", "url", dl.url, "error", err)
			runtime.EventsEmit(a.ctx, "download:error", err.Error())
			return
		}
	}

	runtime.EventsEmit(a.ctx, "download:progress", map[string]interface{}{
		"current": len(downloads),
		"total":   len(downloads),
		"message": "Download complete!",
	})
	time.Sleep(500 * time.Millisecond)
	runtime.EventsEmit(a.ctx, "download:complete", nil)
}

// downloadFile fetches a URL to a local path with atomic write and progress callbacks.
func downloadFile(ctx context.Context, url, destPath string, onProgress func(received, total int64)) error {
	if err := os.MkdirAll(filepath.Dir(destPath), 0755); err != nil {
		return fmt.Errorf("creating directory: %w", err)
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return err
	}

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("HTTP %d for %s", resp.StatusCode, url)
	}

	tmpPath := destPath + ".downloading"
	out, err := os.Create(tmpPath)
	if err != nil {
		return err
	}
	defer func() {
		out.Close()
		os.Remove(tmpPath) // cleanup on error; no-op after successful rename
	}()

	var received int64
	buf := make([]byte, 32*1024)
	for {
		n, readErr := resp.Body.Read(buf)
		if n > 0 {
			if _, writeErr := out.Write(buf[:n]); writeErr != nil {
				return writeErr
			}
			received += int64(n)
			if onProgress != nil {
				onProgress(received, resp.ContentLength)
			}
		}
		if readErr != nil {
			if readErr == io.EOF {
				break
			}
			return readErr
		}
	}

	if err := out.Sync(); err != nil {
		return err
	}
	out.Close()

	return os.Rename(tmpPath, destPath)
}

func fileExists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
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

// paxJob describes one pipeline pass that produces a single .sw3z file.
type paxJob struct {
	name      string                 // e.g. "pax01"
	output    string                 // e.g. "pax01.sw3z"
	sources   []pipeline.SourceGroup // where to read pk3 files from
	allowlist map[string]struct{}    // which files to include
}

// runImport executes multi-pass import pipeline in a background goroutine:
//
//	validate downloads → build pax jobs → run each pipeline → manifest → done
//
// Progress events are forwarded to the frontend via Wails EventsEmit.
// Cancellation is supported via context — CancelImport() triggers it.
func (a *App) runImport(q3Path string) {
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

	// ── Step 1: Validate downloaded demo files ───────────────────────────
	emitProgress("scan", 0, 1, "Checking downloaded resources...")

	dlDir := a.paths.DownloadDir()
	demoQ3Dir := filepath.Join(dlDir, "demoq3")
	demoTADir := filepath.Join(dlDir, "demota")

	if !fileExists(filepath.Join(demoQ3Dir, "pak0.pk3")) || !fileExists(filepath.Join(demoTADir, "pak0.pk3")) {
		runtime.EventsEmit(a.ctx, "import:error",
			"Free resources not downloaded yet. Please download them first.")
		return
	}

	// ── Step 2: Build pax jobs ───────────────────────────────────────────

	// Always: pax01 (Q3 demo) + pax03 (Q3TA demo)
	jobs := []paxJob{
		{
			name:      "pax01",
			output:    "pax01.sw3z",
			sources:   []pipeline.SourceGroup{{Origin: "demo_q3", Dir: demoQ3Dir}},
			allowlist: pipeline.Q3CopyPax01Allowlist,
		},
		{
			name:      "pax03",
			output:    "pax03.sw3z",
			sources:   []pipeline.SourceGroup{{Origin: "demo_ta", Dir: demoTADir}},
			allowlist: pipeline.Q3CopyPax03Allowlist,
		},
	}

	// Optional: pax02 + pax04 from full game installation
	if q3Path != "" {
		baseQ3Dir := filepath.Join(q3Path, "baseq3")
		if fileExists(baseQ3Dir) {
			jobs = append(jobs, paxJob{
				name:      "pax02",
				output:    "pax02.sw3z",
				sources:   []pipeline.SourceGroup{{Origin: "q3_base", Dir: baseQ3Dir}},
				allowlist: pipeline.Q3CopyPax02Allowlist,
			})
		}

		missionpackDir := filepath.Join(q3Path, "missionpack")
		if fileExists(missionpackDir) {
			jobs = append(jobs, paxJob{
				name:      "pax04",
				output:    "pax04.sw3z",
				sources:   []pipeline.SourceGroup{{Origin: "q3_ta", Dir: missionpackDir}},
				allowlist: pipeline.Q3CopyPax04Allowlist,
			})
		}
	}

	emitProgress("scan", 1, 1, fmt.Sprintf("Preparing %d asset packs...", len(jobs)))

	// Create cancellable context so CancelImport() can stop the pipeline.
	ctx, cancel := context.WithCancel(a.ctx)
	a.mu.Lock()
	a.cancelImport = cancel
	a.mu.Unlock()

	// ── Step 3: Run each pipeline pass ───────────────────────────────────

	for i, job := range jobs {
		if ctx.Err() != nil {
			slog.Info("import cancelled by user")
			return
		}

		emitProgress("process", i, len(jobs),
			fmt.Sprintf("Processing %s (%d/%d)...", job.name, i+1, len(jobs)))

		pip := pipeline.New(a.paths, job.sources,
			pipeline.WithProcessors(&pipeline.Q3CopyProcessor{Allowlist: job.allowlist}),
			pipeline.WithOutputName(job.output),
		)

		// Forward pipeline progress events to the frontend.
		go func() {
			for p := range pip.Progress() {
				runtime.EventsEmit(a.ctx, "import:progress", map[string]interface{}{
					"step":    p.Step,
					"current": p.Current,
					"total":   p.Total,
					"message": fmt.Sprintf("[%s] %s", job.name, p.Message),
				})
			}
		}()

		if err := pip.Run(ctx); err != nil {
			if ctx.Err() != nil {
				slog.Info("import cancelled by user")
				return
			}
			slog.Error("pipeline failed", "pax", job.name, "error", err)
			runtime.EventsEmit(a.ctx, "import:error",
				fmt.Sprintf("Failed to process %s: %s", job.name, err.Error()))
			return
		}
	}

	// ── Step 4: Write manifest (signals "assets are ready") ──────────────

	inventory := map[string]string{"demo_q3": demoQ3Dir, "demo_ta": demoTADir}
	if q3Path != "" {
		inventory["q3_install"] = q3Path
	}

	if err := manifest.WriteWithInventory(a.paths.ManifestPath(), inventory); err != nil {
		slog.Error("failed to write manifest", "error", err)
		runtime.EventsEmit(a.ctx, "import:error", err.Error())
		return
	}

	slog.Info("import complete", "jobs", len(jobs))

	// ── Step 5: Success notification ─────────────────────────────────────

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
