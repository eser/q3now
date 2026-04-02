package pipeline

import (
	"fmt"
	"path"
	"strings"
)

// Q3CopyProcessor includes only the exact files specified in the given entry map.
// No origin filtering — each pipeline run targets specific source directories,
// so origin checking is unnecessary.
//
// Used for all pax01-04 pipelines, each with its own entry map.
type Q3CopyProcessor struct {
	Entries map[string]ProcessorEntry
}

func (p *Q3CopyProcessor) Process(entry AssetEntry, readFile func() ([]byte, error)) (EntryDecision, error) {
	lower := strings.ToLower(entry.Path)
	pe, ok := p.Entries[lower]
	if !ok {
		return EntryDecision{Action: Skip}, nil
	}

	switch pe.Mode {
	case ModeCopy:
		return EntryDecision{Action: Include, DestPath: pe.TargetPath}, nil

	case ModeSkip:
		return EntryDecision{Action: Skip}, nil

	case ModeConvert:
		srcFmt := strings.TrimPrefix(path.Ext(lower), ".")

		// Try options-aware converter first (for formats that support Quality/Preprocess).
		if fnOpts, ok := LookupConverterWithOpts(srcFmt, pe.TargetFmt); ok {
			data, err := readFile()
			if err != nil {
				return EntryDecision{}, fmt.Errorf("read %s for convert: %w", entry.Path, err)
			}
			opts := ConvertOptions{Quality: pe.Quality, Preprocess: pe.Preprocess}
			converted, err := fnOpts(data, opts)
			if err != nil {
				return EntryDecision{}, fmt.Errorf("convert %s: %w", entry.Path, err)
			}
			return EntryDecision{Action: Replace, DestPath: pe.TargetPath, Data: converted}, nil
		}

		// Fall back to simple converter (for TGA->PNG etc.).
		fn, err := LookupConverter(srcFmt, pe.TargetFmt)
		if err != nil {
			return EntryDecision{}, fmt.Errorf("convert %s: %w", entry.Path, err)
		}
		data, err := readFile()
		if err != nil {
			return EntryDecision{}, fmt.Errorf("read %s for convert: %w", entry.Path, err)
		}
		converted, err := fn(data)
		if err != nil {
			return EntryDecision{}, fmt.Errorf("convert %s: %w", entry.Path, err)
		}
		return EntryDecision{Action: Replace, DestPath: pe.TargetPath, Data: converted}, nil

	case ModePatch:
		return EntryDecision{}, fmt.Errorf("pipeline: ModePatch not yet implemented for %s", entry.Path)

	default:
		return EntryDecision{}, fmt.Errorf("pipeline: unknown ProcessorMode %d for %s", pe.Mode, entry.Path)
	}
}

func (p *Q3CopyProcessor) Finalize() ([]OutputEntry, error) {
	return nil, nil
}
