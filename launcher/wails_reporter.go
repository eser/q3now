package main

import (
	"context"
	"time"

	"github.com/wailsapp/wails/v2/pkg/runtime"
)

// WailsReporter forwards pipeline events to the Wails frontend as
// "<op>:progress" / "<op>:error" / "<op>:complete" events.
//
// Implements pipeline.Reporter. The CLI counterpart is StdoutReporter
// in cmd/shared.go.
type WailsReporter struct {
	ctx context.Context
	op  string

	// doneDelay is added before the final "<op>:complete" event so the
	// user sees the last progress message before the next screen
	// transition. Matches the prior in-line time.Sleep behavior.
	doneDelay time.Duration
}

// NewWailsReporter binds a Reporter to a Wails context and operation
// name. op is the event prefix (e.g. "download", "import") matching the
// React frontend's existing EventsOn handlers.
func NewWailsReporter(ctx context.Context, op string, doneDelay time.Duration) *WailsReporter {
	return &WailsReporter{ctx: ctx, op: op, doneDelay: doneDelay}
}

func (r *WailsReporter) Progress(step string, current, total int, message string) {
	runtime.EventsEmit(r.ctx, r.op+":progress", map[string]interface{}{
		"step":    step,
		"current": int64(current),
		"total":   int64(total),
		"message": message,
	})
}

func (r *WailsReporter) Error(err error) {
	runtime.EventsEmit(r.ctx, r.op+":error", err.Error())
}

func (r *WailsReporter) Done(_ string) {
	if r.doneDelay > 0 {
		time.Sleep(r.doneDelay)
	}
	runtime.EventsEmit(r.ctx, r.op+":complete", nil)
}
