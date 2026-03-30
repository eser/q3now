package main

import (
	"embed"
	"log/slog"
	"os"

	"github.com/eser/q3now/launcher/internal/config"
	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
)

// version is set via -ldflags at build time.
var version = "dev"

//go:embed all:frontend/dist
var assets embed.FS

func main() {
	slog.SetDefault(slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	})))
	slog.Info("starting q3now launcher", "version", version)

	app := NewApp(version)

	err := wails.Run(&options.App{
		Title:    "q3now" + config.ChannelSuffix(),
		Width:    720,
		Height:   480,
		MinWidth: 720,
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
		os.Exit(1)
	}
}
