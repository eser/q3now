package pipeline

// AssetEntry is the input — one file inside a source archive, tagged with its origin.
type AssetEntry struct {
	Origin      string // inventory ID: "q3_base", "q1_base", "qlive_base", etc.
	Path        string // path inside the source archive
	SourcePak   string // source archive file path on disk
	SourceIndex int    // index within source archive for ReadFile()
	UncompSize  int64
}

// Action is the processor's decision for an entry.
type Action int

const (
	Skip    Action = iota // default — entry is excluded from output
	Include               // include entry (optionally renamed via DestPath)
	Replace               // include with new content (Data must be set)
)

// EntryDecision is returned by a Processor for each entry.
type EntryDecision struct {
	Action   Action
	DestPath string // output path (if empty, uses entry.Path). Used by Include and Replace.
	Data     []byte // new content — required for Replace, nil for Include.
}

// Processor makes per-entry decisions about what to include in the output.
// Processors are composed in a chain: last non-Skip decision wins.
//
// Pipeline chain semantics:
//
//	┌─────────┐   ┌─────────┐   ┌─────────┐
//	│ Proc 1  │──▶│ Proc 2  │──▶│ Proc 3  │──▶ final decision
//	└─────────┘   └─────────┘   └─────────┘
//
// For each entry, all processors run. The LAST non-Skip decision wins.
// If all return Skip, the entry is excluded from output.
//
// Example:
//
//	func (p *MyProcessor) Process(entry AssetEntry, readFile func() ([]byte, error)) (EntryDecision, error) {
//	    if entry.Origin == "q1_base" && entry.Path == "maps/dm1.bsp" {
//	        data, err := readFile()
//	        if err != nil { return EntryDecision{}, err }
//	        newBsp := convertBSP(data)
//	        return EntryDecision{Action: Replace, DestPath: "maps/q1dm1.bsp", Data: newBsp}, nil
//	    }
//	    return EntryDecision{Action: Skip}, nil
//	}
type Processor interface {
	// Process is called for every entry. readFile is lazy — only call it when
	// you need the content (e.g., BSP conversion, aggregation). For simple
	// include/rename, don't call it. readFile caches its result; multiple
	// processors calling it get the same data without re-reading.
	// Contract: readFile must be called during Process(), not stored for later.
	Process(entry AssetEntry, readFile func() ([]byte, error)) (EntryDecision, error)

	// Finalize is called after all entries are processed. Returns synthetic
	// entries (e.g., aggregated arenalist.json). May return nil.
	Finalize() ([]OutputEntry, error)
}

// OutputEntry is the output — one file to write to the SW3Z archive.
type OutputEntry struct {
	OutputPath  string // path in the output archive
	SourcePak   string // where to read data (empty for synthetic/replaced entries)
	SourceIndex int    // index in source archive (-1 for synthetic/replaced)
	Data        []byte // if non-nil, write this instead of reading from source
	UncompSize  int64
}

// IsSynthetic returns true if this entry's content is in Data (not from a source archive).
func (e *OutputEntry) IsSynthetic() bool {
	return e.Data != nil
}

// Progress reports pipeline progress to the UI.
type Progress struct {
	Step    string `json:"step"`
	Current int64  `json:"current"`
	Total   int64  `json:"total"`
	Message string `json:"message"`
}

// SourceGroup identifies a set of archives from a specific game origin.
type SourceGroup struct {
	Origin string // inventory ID: "q3_base", "q1_base", etc.
	Dir    string // absolute path to the directory containing pak files
}
