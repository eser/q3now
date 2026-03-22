package pipeline

import "strings"

// Q3BaseProcessor includes only the exact files specified in q3BaseAllowlist
// from the "q3_base" origin. No prefix matching — every file is explicitly listed.
//
// The allowlist is defined in proc_q3base_allowlist.go as a map[string]struct{}
// with lowercase paths for case-insensitive lookup.
type Q3BaseProcessor struct{}

func (p *Q3BaseProcessor) Process(entry AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
	if entry.Origin != "q3_base" {
		return EntryDecision{Action: Skip}, nil
	}

	lower := strings.ToLower(entry.Path)
	if _, ok := q3BaseAllowlist[lower]; ok {
		return EntryDecision{Action: Include}, nil
	}

	return EntryDecision{Action: Skip}, nil
}

func (p *Q3BaseProcessor) Finalize() ([]OutputEntry, error) {
	return nil, nil
}
