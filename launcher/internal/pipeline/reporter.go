package pipeline

// Reporter receives progress and completion signals from long-running
// pipeline operations (download, import). It abstracts the difference
// between the GUI's Wails event emission and the CLI's stdout printing.
//
// All methods must be safe to call from any goroutine — the orchestration
// functions in this package may invoke them from worker goroutines that
// drain pipeline.Progress channels.
//
// See docs/launcher.md "Wails-vs-CLI separation".
type Reporter interface {
	// Progress reports that a step has advanced. step is a stable identifier
	// the consumer can use to group/route messages (e.g. "download",
	// "extract", "process", "scan", "complete"). current/total describe the
	// progress bar position; total may be zero when unknown. message is a
	// short human-readable description.
	Progress(step string, current, total int, message string)

	// Error reports a fatal error that aborts the operation. After Error,
	// the orchestration function returns and the reporter receives no
	// further calls for this operation.
	Error(err error)

	// Done signals successful completion. message is a short
	// human-readable summary suitable for display.
	Done(message string)
}

// NopReporter discards all events. Useful in tests and as a fallback
// when callers want to ignore progress events entirely.
type NopReporter struct{}

func (NopReporter) Progress(string, int, int, string) {}
func (NopReporter) Error(error)                       {}
func (NopReporter) Done(string)                       {}
