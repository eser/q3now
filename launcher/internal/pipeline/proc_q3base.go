package pipeline

import (
	"fmt"
	"path"
	"strings"
)

// Q3BaseProcessor includes only the exact files specified in q3BaseEntries
// from the "q3_base" origin. No prefix matching — every file is explicitly listed.
//
// The entries are defined in proc_q3base_entries.go as a map[string]ProcessorEntry
// with lowercase paths for case-insensitive lookup.
type Q3BaseProcessor struct{}

func (p *Q3BaseProcessor) Process(entry AssetEntry, readFile func() ([]byte, error)) (EntryDecision, error) {
	if entry.Origin != "q3_base" {
		return EntryDecision{Action: Skip}, nil
	}

	lower := strings.ToLower(entry.Path)
	pe, ok := q3BaseEntries[lower]
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

func (p *Q3BaseProcessor) Finalize() ([]OutputEntry, error) {
	return nil, nil
}
