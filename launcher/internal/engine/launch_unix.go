//go:build linux

package engine

import (
	"os"
	"os/exec"
)

// configureSpawnIO inherits the launcher's stdio so engine output appears
// in the parent terminal when the launcher was started from one.  No
// regression versus the pre-2026-05 behavior.
func configureSpawnIO(cmd *exec.Cmd, _ string) error {
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return nil
}
