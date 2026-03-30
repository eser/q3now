package pipeline

import "strings"

// Q3CopyProcessor includes only the exact files specified in the given allowlist.
// No origin filtering — each pipeline run targets specific source directories,
// so origin checking is unnecessary.
//
// Used for all pax01–04 pipelines, each with its own allowlist.
type Q3CopyProcessor struct {
	Allowlist map[string]struct{}
}

func (p *Q3CopyProcessor) Process(entry AssetEntry, _ func() ([]byte, error)) (EntryDecision, error) {
	lower := strings.ToLower(entry.Path)
	if _, ok := p.Allowlist[lower]; ok {
		return EntryDecision{Action: Include}, nil
	}

	return EntryDecision{Action: Skip}, nil
}

func (p *Q3CopyProcessor) Finalize() ([]OutputEntry, error) {
	return nil, nil
}
