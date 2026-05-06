package subcommands

import (
	"fmt"
	"os"
	"time"

	"github.com/eser/q3now/launcher/internal/config"
	"github.com/eser/q3now/launcher/internal/settings"
)

// StdoutReporter prints pipeline.Reporter events to stdout (progress)
// and stderr (errors), in a human-readable line format.
//
// Output shape per call:
//
//	[<op>] <step> (cur/tot): message
//
// Errors go to stderr with a leading "Error: " prefix; the cobra
// command then returns the same error and main.go exits non-zero.
type StdoutReporter struct {
	op string
}

// NewStdoutReporter binds a Reporter to an operation name (e.g.
// "download", "import"). The op is prefixed in every progress line.
func NewStdoutReporter(op string) *StdoutReporter {
	return &StdoutReporter{op: op}
}

func (r *StdoutReporter) Progress(step string, current, total int, message string) {
	if total > 0 {
		fmt.Fprintf(os.Stdout, "[%s] %s (%d/%d): %s\n", r.op, step, current, total, message)
	} else {
		fmt.Fprintf(os.Stdout, "[%s] %s: %s\n", r.op, step, message)
	}
}

func (r *StdoutReporter) Error(err error) {
	fmt.Fprintf(os.Stderr, "Error: %v\n", err)
}

func (r *StdoutReporter) Done(message string) {
	fmt.Fprintf(os.Stdout, "[%s] %s\n", r.op, message)
}

// requireEulaAccepted gates an operation behind the EULA acceptance
// timestamp in settings.json. If --accept-eula was passed, write the
// acceptance now. Otherwise refuse with a message pointing at the EULA
// file path so the user can read before accepting.
//
// CLI-side equivalent of the GUI's EulaScreen Accept button.
func requireEulaAccepted(paths *config.Paths, acceptFlag bool) error {
	s := settings.Read(paths.SettingsPath())

	if s.EulaAcceptedAt != "" {
		// Already accepted (either via GUI or a prior --accept-eula run).
		return nil
	}

	if !acceptFlag {
		return fmt.Errorf(
			"id Software demo EULA has not been accepted yet.\n\n"+
				"  Read the EULA: %s\n\n"+
				"  To accept and proceed, re-run with --accept-eula.\n"+
				"  (You can also accept via the GUI EULA screen — "+
				"the same acceptance timestamp is shared between modes.)",
			eulaFilePath(paths))
	}

	// Record acceptance using the same settings layer the GUI uses.
	// settings.Write self-bootstraps the parent dir per
	// docs/launcher.md "Writer self-bootstrap" — safe on first run.
	s.EulaAcceptedAt = time.Now().UTC().Format(time.RFC3339)
	if err := settings.Write(paths.SettingsPath(), s); err != nil {
		return fmt.Errorf("recording EULA acceptance: %w", err)
	}
	fmt.Fprintf(os.Stdout,
		"EULA accepted at %s (recorded in %s).\n",
		s.EulaAcceptedAt, paths.SettingsPath())
	return nil
}

// eulaFilePath returns a hint about where to find the human-readable
// EULA. Today the EULA text is embedded in the launcher binary
// (launcher/eula.go); we point users at the in-repo source as the
// canonical reference for CLI use.
func eulaFilePath(_ *config.Paths) string {
	return "https://www.idsoftware.com/business/legal/lic_q3a_demo.html (also embedded in launcher/eula.go)"
}
