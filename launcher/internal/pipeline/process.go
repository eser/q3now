package pipeline

import (
	"context"
	"fmt"
	"log/slog"

	"github.com/eser/q3now/launcher/internal/safepath"
)

// ProcessStep runs a composable chain of Processors over all entries.
//
// Chain semantics: last non-Skip decision wins.
//
//	┌─────────┐   ┌─────────┐   ┌─────────┐
//	│ Proc 1  │──▶│ Proc 2  │──▶│ Proc 3  │──▶ final decision
//	│ (common │   │ (Q3     │   │ (Q1     │
//	│ filter) │   │ assets) │   │ assets) │
//	└─────────┘   └─────────┘   └─────────┘
//
// For each entry:
//   - Run all processors in order
//   - Collect all non-Skip decisions
//   - Use the LAST non-Skip decision as the final decision
//   - If all return Skip → entry is excluded
type ProcessStep struct {
	Processors []Processor
}

func (p *ProcessStep) Name() string { return "process" }

// Run processes all entries through the processor chain and returns output entries.
func (p *ProcessStep) Run(ctx context.Context, entries []AssetEntry, progress chan<- Progress) ([]OutputEntry, error) {
	cache := newReaderCache()
	defer cache.closeAll()

	var output []OutputEntry
	var included, replaced, skipped int

	total := int64(len(entries))

	for i, entry := range entries {
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		default:
		}

		if i%500 == 0 {
			progress <- Progress{
				Step:    "process",
				Current: int64(i),
				Total:   total,
				Message: fmt.Sprintf("Processing entries (%d/%d)", i, total),
			}
		}

		// Build a cached readFile callback for this entry.
		var cached []byte
		var cachedErr error
		var called bool
		readFile := func() ([]byte, error) {
			if called {
				return cached, cachedErr
			}
			called = true
			r, err := cache.get(entry.SourcePak)
			if err != nil {
				cachedErr = fmt.Errorf("open %s: %w", entry.SourcePak, err)
				return nil, cachedErr
			}
			cached, cachedErr = r.ReadFile(entry.SourceIndex)
			return cached, cachedErr
		}

		// Run the processor chain: last non-Skip wins.
		var lastDecision *EntryDecision
		for _, proc := range p.Processors {
			decision, err := proc.Process(entry, readFile)
			if err != nil {
				return nil, fmt.Errorf("processor error for %s/%s: %w", entry.Origin, entry.Path, err)
			}
			if decision.Action != Skip {
				d := decision // capture
				lastDecision = &d
			}
		}

		if lastDecision == nil {
			skipped++
			continue
		}

		// Determine the output path.
		outputPath := entry.Path
		if lastDecision.DestPath != "" {
			outputPath = lastDecision.DestPath
		}

		// Validate output path safety.
		if lastDecision.DestPath != "" && !safepath.IsSafe(lastDecision.DestPath) {
			return nil, fmt.Errorf("unsafe DestPath %q for entry %s/%s", lastDecision.DestPath, entry.Origin, entry.Path)
		}

		switch lastDecision.Action {
		case Include:
			output = append(output, OutputEntry{
				OutputPath:  outputPath,
				SourcePak:   entry.SourcePak,
				SourceIndex: entry.SourceIndex,
				UncompSize:  entry.UncompSize,
			})
			included++
		case Replace:
			output = append(output, OutputEntry{
				OutputPath:  outputPath,
				SourcePak:   "",
				SourceIndex: -1,
				Data:        lastDecision.Data,
				UncompSize:  int64(len(lastDecision.Data)),
			})
			replaced++
		}
	}

	// Finalize: collect synthetic entries from all processors.
	syntheticCount := 0
	for _, proc := range p.Processors {
		synthetics, err := proc.Finalize()
		if err != nil {
			return nil, fmt.Errorf("processor finalize error: %w", err)
		}
		output = append(output, synthetics...)
		syntheticCount += len(synthetics)
	}

	// Validate: no duplicate output paths.
	seen := make(map[string]bool, len(output))
	for _, oe := range output {
		if seen[oe.OutputPath] {
			return nil, fmt.Errorf("duplicate output path: %q", oe.OutputPath)
		}
		seen[oe.OutputPath] = true
	}

	slog.Info("process: complete",
		"included", included,
		"replaced", replaced,
		"skipped", skipped,
		"synthetic", syntheticCount,
		"total_output", len(output))

	progress <- Progress{
		Step:    "process",
		Current: total,
		Total:   total,
		Message: fmt.Sprintf("Processed: %d included, %d replaced, %d skipped, %d synthetic",
			included, replaced, skipped, syntheticCount),
	}

	return output, nil
}
