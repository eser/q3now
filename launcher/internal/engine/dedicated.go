package engine

import (
	"bufio"
	"context"
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

// ServerConfig holds dedicated server launch configuration.
type ServerConfig struct {
	Hostname   string `json:"hostname"`
	Map        string `json:"map"`
	GameType   string `json:"gameType"` // "dm", "tdm", "ctf"
	MaxClients int    `json:"maxClients"`
	Password   string `json:"password"`
	AddBots    bool   `json:"addBots"`
	BotCount   int    `json:"botCount"`
}

// DedServer manages the lifecycle of a dedicated server process.
//
//	State machine:
//	  STOPPED ──Start()──▶ RUNNING ──Stop()──▶ STOPPED
//	                         │
//	                         └──crash──▶ STOPPED (onStop called)
type DedServer struct {
	mu      sync.Mutex
	cmd     *exec.Cmd
	cancel  context.CancelFunc
	running bool
	done    chan struct{} // closed when process exits
	onLog   func(string)
	onStop  func()
}

// NewDedServer creates a DedServer with callbacks for log lines and stop events.
func NewDedServer(onLog func(string), onStop func()) *DedServer {
	return &DedServer{
		onLog:  onLog,
		onStop: onStop,
	}
}

// Start launches the dedicated server with the given config.
func (d *DedServer) Start(binPath string, config ServerConfig) error {
	d.mu.Lock()
	defer d.mu.Unlock()

	if d.running {
		return fmt.Errorf("dedicated server is already running")
	}

	if config.Map == "" {
		return fmt.Errorf("map is required")
	}

	args := BuildArgs(config)
	slog.Info("starting dedicated server", "binary", binPath, "args", args)

	if _, err := os.Stat(binPath); err != nil {
		return fmt.Errorf("dedicated server binary not found at %s: %w", binPath, err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	cmd := exec.CommandContext(ctx, binPath, args...)
	cmd.Dir = filepath.Dir(binPath)

	stdout, err := cmd.StdoutPipe()
	if err != nil {
		cancel()
		return fmt.Errorf("failed to create stdout pipe: %w", err)
	}
	cmd.Stderr = cmd.Stdout

	if err := cmd.Start(); err != nil {
		cancel()
		return fmt.Errorf("failed to start dedicated server: %w", err)
	}

	d.cmd = cmd
	d.cancel = cancel
	d.running = true
	d.done = make(chan struct{})

	// Stream stdout lines.
	go func() {
		scanner := bufio.NewScanner(stdout)
		for scanner.Scan() {
			if d.onLog != nil {
				d.onLog(scanner.Text())
			}
		}
	}()

	// Single goroutine waits for process exit — the ONLY place that resets state.
	go func() {
		err := cmd.Wait()

		d.mu.Lock()
		d.running = false
		d.cmd = nil
		d.cancel = nil
		close(d.done)
		d.mu.Unlock()

		if err != nil {
			slog.Info("dedicated server exited", "error", err)
		} else {
			slog.Info("dedicated server exited cleanly")
			if d.onLog != nil {
				d.onLog("Server stopped.")
			}
		}
		if d.onStop != nil {
			d.onStop()
		}
	}()

	slog.Info("dedicated server started", "pid", cmd.Process.Pid)
	return nil
}

// Stop sends SIGTERM, waits up to 5s, then force-kills.
func (d *DedServer) Stop() error {
	d.mu.Lock()
	if !d.running || d.cmd == nil {
		d.mu.Unlock()
		return nil
	}
	cmd := d.cmd
	cancel := d.cancel
	done := d.done
	d.mu.Unlock()

	slog.Info("stopping dedicated server")

	// Graceful: SIGTERM / interrupt.
	if cmd.Process != nil {
		cmd.Process.Signal(os.Interrupt)
	}

	// Wait for the existing wait-goroutine to finish, or timeout.
	select {
	case <-done:
		slog.Info("dedicated server stopped gracefully")
	case <-time.After(5 * time.Second):
		slog.Warn("dedicated server did not stop gracefully, force killing")
		if cancel != nil {
			cancel() // kills via context
		}
		if d.onLog != nil {
			d.onLog("Server force-stopped after timeout.")
		}
		// Wait for the wait-goroutine to finish after force kill.
		<-done
	}

	return nil
}

// IsRunning returns whether the dedicated server is currently running.
func (d *DedServer) IsRunning() bool {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.running
}

// BuildArgs converts a ServerConfig into Q3 dedicated server command-line arguments.
func BuildArgs(config ServerConfig) []string {
	args := []string{
		"+set", "dedicated", "1",
	}

	if config.Hostname != "" {
		args = append(args, "+set", "sv_hostname", config.Hostname)
	}

	switch strings.ToLower(config.GameType) {
	case "tdm":
		args = append(args, "+set", "g_gametype", "3")
	case "ctf":
		args = append(args, "+set", "g_gametype", "4")
	case "freezetag":
		args = append(args, "+set", "g_gametype", "0", "+set", "g_freeze", "1")
	default:
		args = append(args, "+set", "g_gametype", "0")
	}

	if config.MaxClients > 0 {
		args = append(args, "+set", "sv_maxclients", fmt.Sprintf("%d", config.MaxClients))
	}

	if config.Password != "" {
		args = append(args, "+set", "g_needpass", "1", "+set", "g_password", config.Password)
	}

	args = append(args, "+map", config.Map)

	if config.AddBots && config.BotCount > 0 {
		bots := []string{"Doom", "Bones", "Slash", "Orbb", "Klesk", "Anarki", "Grunt", "Keel"}
		for i := 0; i < config.BotCount && i < len(bots); i++ {
			args = append(args, "+addbot", bots[i], "3")
		}
	}

	return args
}

// CommandPreview returns the full command line that would be executed.
func CommandPreview(binPath string, config ServerConfig) string {
	args := BuildArgs(config)
	parts := []string{filepath.Base(binPath)}
	parts = append(parts, args...)
	return strings.Join(parts, " ")
}
