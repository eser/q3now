package pipeline

import (
	"context"
	"fmt"
	"io"
	"log/slog"
	"os"
	"path/filepath"

	sw3z "github.com/eser/q3now/pkg/sw3z-archiver"
)

// SW3ZSink writes all output entries into a single SW3Z archive.
type SW3ZSink struct {
	DestDir    string // e.g., ~/.q3a/baseq3/
	OutputName string // e.g., "pax01.sw3z"
}

func (s *SW3ZSink) Name() string { return "repackage-sw3z" }

// Run writes all output entries to a single SW3Z archive.
func (s *SW3ZSink) Run(ctx context.Context, output []OutputEntry, progress chan<- Progress) error {
	if len(output) == 0 {
		slog.Info("sw3z-sink: no entries to write, skipping")
		return nil
	}

	if err := os.MkdirAll(s.DestDir, 0755); err != nil {
		return fmt.Errorf("creating destination directory: %w", err)
	}

	destPath := filepath.Join(s.DestDir, s.OutputName)

	cache := newReaderCache()
	defer cache.closeAll()

	// Calculate total bytes for progress.
	var totalBytes int64
	for _, e := range output {
		totalBytes += e.UncompSize
	}

	var writtenBytes int64

	err := atomicWrite(destPath, func(w io.Writer) error {
		writer := sw3z.Create(w)

		for i, entry := range output {
			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}

			var data []byte
			if entry.Data != nil {
				// Replace or synthetic — data already in entry.
				data = entry.Data
			} else {
				// Include — read from source archive.
				r, err := cache.get(entry.SourcePak)
				if err != nil {
					return fmt.Errorf("opening source %s: %w", filepath.Base(entry.SourcePak), err)
				}
				var readErr error
				data, readErr = r.ReadFile(entry.SourceIndex)
				if readErr != nil {
					return fmt.Errorf("reading %s from %s: %w",
						entry.OutputPath, filepath.Base(entry.SourcePak), readErr)
				}
			}

			if err := writer.AddFile(entry.OutputPath, data, sw3z.CompressLZ4); err != nil {
				return fmt.Errorf("adding %s: %w", entry.OutputPath, err)
			}

			writtenBytes += entry.UncompSize
			if i%100 == 0 || i == len(output)-1 {
				progress <- Progress{
					Step:    "repackage",
					Current: writtenBytes,
					Total:   totalBytes,
					Message: fmt.Sprintf("Writing %s (%d/%d)", s.OutputName, i+1, len(output)),
				}
			}
		}

		if err := writer.Close(); err != nil {
			return fmt.Errorf("closing sw3z writer: %w", err)
		}

		return nil
	})

	if err != nil {
		return fmt.Errorf("writing %s: %w", s.OutputName, err)
	}

	slog.Info("sw3z-sink: complete", "name", s.OutputName, "entries", len(output))

	progress <- Progress{
		Step:    "repackage",
		Current: totalBytes,
		Total:   totalBytes,
		Message: "Import complete",
	}

	return nil
}
