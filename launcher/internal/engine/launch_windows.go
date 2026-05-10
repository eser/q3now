package engine

import (
	"os"
	"os/exec"
	"path/filepath"
)

// configureSpawnIO redirects the spawned engine's stdout/stderr to a log
// file beside the binary, and clears stdin.
//
// Why: the Wails launcher is a Windows GUI-subsystem binary, so the
// launcher's own os.Stdin/os.Stdout/os.Stderr are typically invalid handles
// when launched from Explorer/Start Menu (no parent console exists).  Go's
// os/exec sets STARTF_USESTDHANDLES whenever ANY of cmd.Stdin/Stdout/Stderr
// is non-nil and would inherit the parent's invalid handles, then passes
// them to CreateProcessW which fails with ERROR_NOT_SUPPORTED (50).  All
// three must be set to explicit valid (or explicit nil/DETACHED) values
// to break the inheritance chain.
//
// engine-stdout.log lives beside wired.x64.exe so users can find it without
// hunting in fs_homepath.  It captures only spawn-time pre-log-init output
// (DLL load errors, CreateProcessW failures, etc.) — the engine's own
// qconsole.jsonl handles everything after Com_Init.
func configureSpawnIO(cmd *exec.Cmd, binPath string) error {
	logPath := filepath.Join(filepath.Dir(binPath), "engine-stdout.log")
	logFile, err := os.OpenFile(logPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
	if err != nil {
		// Best-effort: fully detach all three so STARTF_USESTDHANDLES isn't
		// set with invalid inherited handles.
		cmd.Stdin = nil
		cmd.Stdout = nil
		cmd.Stderr = nil
		return nil
	}
	cmd.Stdin = nil
	cmd.Stdout = logFile
	cmd.Stderr = logFile
	// logFile intentionally not closed: Launch calls os.Exit(0) after
	// cmd.Start() returns, so there's no later moment to close it.  The
	// fd is held by the spawned engine process for its lifetime.
	return nil
}
