// Package subcommands implements the launcher's CLI mode using spf13/cobra.
//
// The same launcher binary serves both modes: GUI (default action — bare
// invocation) and CLI (recognized subcommands).  Cobra dispatches to the
// right path; main.go injects the GUI bootstrap as RunE on rootCmd, so
// a bare `q3now-launcher` opens the GUI like any other Cobra command.
//
// Subcommand structure follows the noun-verb hierarchy:
//
//	q3now-launcher assets download
//	q3now-launcher assets import [--q3path=PATH] [--accept-eula]
//
// Future verbs and noun groups (e.g. `q3now-launcher server start`,
// `q3now-launcher mod install`) extend the tree without restructuring.
//
// See docs/launcher.md "Wails-vs-CLI separation".
package subcommands

import (
	"context"
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"github.com/spf13/cobra"
)

// GUILauncher is the GUI bootstrap callback main injects before Execute.
// Invoked by rootCmd's RunE when the binary runs with no subcommand.
// Nil in headless build configurations (e.g. dedicated server CI builds);
// RunE then returns an explanatory error.
var GUILauncher func() error

var rootCmd = &cobra.Command{
	Use:   "q3now-launcher",
	Short: "q3now launcher — GUI by default, CLI via subcommands",
	Long: `q3now is a Quake 3 derivative engine with an integrated launcher.
Running the binary with no arguments launches the GUI. Running it with a
recognized subcommand (e.g. ` + "`q3now-launcher assets download`" + `) executes
that operation headlessly and exits.`,
	SilenceUsage: true,
	RunE: func(cmd *cobra.Command, args []string) error {
		if GUILauncher == nil {
			return fmt.Errorf("GUI not available in this build")
		}
		return GUILauncher()
	},
}

// SetVersion wires the ldflags-injected version string into rootCmd's
// --version flag handler.
func SetVersion(v string) {
	rootCmd.Version = v
}

// Execute runs the cobra dispatcher.  SIGINT/SIGTERM cancel the
// context so long-running subcommands (assets download/import) abort
// cleanly on Ctrl-C rather than dying mid-write.
func Execute() error {
	ctx, cancel := signal.NotifyContext(context.Background(),
		os.Interrupt, syscall.SIGTERM)
	defer cancel()

	return rootCmd.ExecuteContext(ctx)
}
