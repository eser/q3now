package engine

import (
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/eser/q3now/launcher/internal/config"
)

// launchBinary validates a binary exists and starts it with the given args.
// Returns the started command for lifecycle management (or nil if exitAfter).
func launchBinary(binPath string, args []string) (*exec.Cmd, error) {
	if _, err := os.Stat(binPath); err != nil {
		return nil, fmt.Errorf("binary not found at %s: %w", binPath, err)
	}

	slog.Info("launching binary", "path", binPath, "args", args)

	cmd := exec.Command(binPath, args...)
	cmd.Dir = filepath.Dir(binPath)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		return nil, fmt.Errorf("failed to start %s: %w", filepath.Base(binPath), err)
	}

	return cmd, nil
}

// Launch starts the game client binary and exits the launcher.
// args are optional Q3 command-line arguments (+map, +connect, +set, etc.)
func Launch(paths *config.Paths, args []string) error {
	cmd, err := launchBinary(paths.GameBinaryPath(), args)
	if err != nil {
		return err
	}

	slog.Info("game started, launcher exiting", "pid", cmd.Process.Pid)
	os.Exit(0)
	return nil // unreachable
}
