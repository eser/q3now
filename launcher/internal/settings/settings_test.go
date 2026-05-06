package settings

import (
	"os"
	"path/filepath"
	"testing"
)

// TestWriteCreatesMissingParent regression-guards the EULA-accept bug
// fixed 2026-05-05. Before the fix, settings.Write failed silently when
// the HomeDir didn't exist yet (typical first-run state), causing the
// EULA Accept button to silently swallow a write error. See
// docs/launcher.md "Writer self-bootstrap".
func TestWriteCreatesMissingParent(t *testing.T) {
	root := t.TempDir()
	// Target path lives TWO levels below root so MkdirAll has work to do.
	missing := filepath.Join(root, "nonexistent-home", "settings.json")

	if _, err := os.Stat(filepath.Dir(missing)); !os.IsNotExist(err) {
		t.Fatalf("test setup: parent directory %q should not exist", filepath.Dir(missing))
	}

	s := defaults()
	s.EulaAcceptedAt = "2026-05-05T00:00:00Z"
	if err := Write(missing, s); err != nil {
		t.Fatalf("Write should self-bootstrap missing parent dir, got error: %v", err)
	}

	if _, err := os.Stat(missing); err != nil {
		t.Fatalf("settings.json should exist after Write: %v", err)
	}

	got := Read(missing)
	if got.EulaAcceptedAt != "2026-05-05T00:00:00Z" {
		t.Fatalf("round-trip mismatch: want EulaAcceptedAt=2026-05-05T00:00:00Z, got %q", got.EulaAcceptedAt)
	}
}
