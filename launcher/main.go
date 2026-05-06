package main

import (
	"embed"
	"log/slog"
	"os"

	"github.com/eser/q3now/launcher/internal/config"
	"github.com/eser/q3now/launcher/internal/subcommands"
	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
)

// version is set via -ldflags at build time.
var version = "dev"

//go:embed all:frontend/dist
var assets embed.FS

func main() {
	// Reattach stdout/stderr to the parent console on Windows so any
	// subcommand's output is visible.  No-op on Linux/macOS, and a no-op
	// on Windows when there is no parent console (Explorer launch, etc).
	attachParentConsole()

	slog.SetDefault(slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	})))

	// Wire the GUI bootstrap into Cobra's default RunE so a bare
	// invocation (no subcommand) opens the Wails window via the same
	// dispatcher that handles `assets download`, `--help`, `--version`,
	// and unknown-command suggestions.
	subcommands.SetVersion(version)
	subcommands.GUILauncher = runGUI

	if err := subcommands.Execute(); err != nil {
		// Cobra has already printed "Error: <msg>" to stderr (with
		// suggestions for unknown commands).  Just propagate exit code.
		os.Exit(1)
	}
}

func runGUI() error {
	slog.Info("starting q3now launcher", "version", version)

	app := NewApp(version)

	err := wails.Run(&options.App{
		Title:     "q3now" + config.ChannelSuffix(),
		Width:     720,
		Height:    480,
		MinWidth:  720,
		MinHeight: 480,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		BackgroundColour: &options.RGBA{R: 18, G: 18, B: 24, A: 1},
		OnStartup:        app.startup,
		OnBeforeClose:    app.beforeClose,
		Bind: []interface{}{
			app,
		},
	})
	if err != nil {
		slog.Error("launcher failed", "error", err)
	}
	return err
}
