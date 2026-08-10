package pipeline

import (
	"fmt"
	"log/slog"
	"path"
	"path/filepath"
	"sort"
	"strings"
)

// Q3CopyProcessor includes only the exact files specified in the given entry map.
// Map keys are OUTPUT paths. PackIndex (or key if empty) is the SOURCE path inside archives.
//
// Used for all pax01-04 pipelines, each with its own entry map.
//
// One source → many targets: a single source path may be declared by more than
// one entry (e.g. a Q1 monster's progs/<n>.mdl fans out to both
// creatures/<n>/<n>.mdl and characters/<n>/models/body.mdl). The source appears
// once in pak0, so it is scanned once. Process() emits the first matching output
// via its EntryDecision return; every additional matching output is stashed in
// fanout and emitted from Finalize() (the existing synthetic-output channel) so
// no single-source entry is silently dropped.
type Q3CopyProcessor struct {
	Entries     map[string]ProcessorEntry
	matched     map[string]bool     // tracks which entries were fulfilled
	byPackIndex map[string][]string // lowercase pack-index → output keys (built lazily)
	fanout      []OutputEntry       // extra outputs (2nd..Nth target of a shared source)
}

// resolvePackIndex returns the source path for an entry (PackIndex if set, otherwise the key).
func resolvePackIndex(key string, pe ProcessorEntry) string {
	if pe.PackIndex != "" {
		return pe.PackIndex
	}
	return key
}

// initIndex builds the reverse lookup from pack-index → output keys.
// A source path may map to more than one output key, so entries APPEND to the
// slice rather than overwrite (which would nondeterministically drop all but one
// output for a shared source — the collision this fixes).
func (p *Q3CopyProcessor) initIndex() {
	if p.byPackIndex != nil {
		return
	}
	p.byPackIndex = make(map[string][]string, len(p.Entries))
	p.matched = make(map[string]bool, len(p.Entries))
	for key, pe := range p.Entries {
		idx := strings.ToLower(resolvePackIndex(key, pe))
		p.byPackIndex[idx] = append(p.byPackIndex[idx], key)
	}
	// Deterministic fan-out order: Go map iteration above is random, so a shared
	// source's output keys land in the slice in arbitrary order. Sort each slice
	// so the "first" output (the EntryDecision return) and the stashed remainder
	// are stable across runs — otherwise which target the Process() return emits
	// vs. which Finalize() emits would vary build-to-build.
	for idx := range p.byPackIndex {
		sort.Strings(p.byPackIndex[idx])
	}
}

func (p *Q3CopyProcessor) Process(entry AssetEntry, readFile func() ([]byte, error)) (EntryDecision, error) {
	p.initIndex()

	lower := strings.ToLower(entry.Path)
	outputKeys, ok := p.byPackIndex[lower]
	if !ok {
		return EntryDecision{Action: Skip}, nil
	}

	// A single scanned source may satisfy several entries (one source → many
	// targets). Build a decision for each matching output key; return the first
	// non-Skip decision through the normal EntryDecision channel and stash the
	// rest in fanout for Finalize() to emit. Keys are sorted (see initIndex) so
	// the split between "returned" and "stashed" is deterministic.
	var first *EntryDecision
	for _, outputKey := range outputKeys {
		pe := p.Entries[outputKey]

		// Strict pack validation: the asset must come from the declared pak.
		// entry.SourcePak comes from filepath.WalkDir using the OS-native separator
		// (backslash on Windows). pe.Pack is hardcoded with forward slashes
		// (e.g. "demota/pak0.pk3"). Normalize at compare time only — entry.SourcePak
		// is consumed elsewhere as an OS-native path for actual file opens
		// (process.go:cache.get, repackage_sw3z.go), so we don't normalize at the
		// scan site. Compare-time normalization is local and defensive.
		if pe.Pack != "" && !strings.HasSuffix(filepath.ToSlash(entry.SourcePak), pe.Pack) {
			slog.Debug("q3copy: pack mismatch", "path", lower, "want", pe.Pack, "got", filepath.ToSlash(entry.SourcePak))
			continue
		}

		decision, err := p.decide(entry, lower, outputKey, pe, readFile)
		if err != nil {
			return EntryDecision{}, err
		}
		if decision.Action == Skip {
			// ModeSkip still counts as fulfilled (the entry was explicitly matched
			// and intentionally excluded), so mark it and move on.
			p.matched[outputKey] = true
			continue
		}

		slog.Debug("q3copy: matched", "path", lower, "output", outputKey, "pack", entry.SourcePak)
		p.matched[outputKey] = true

		if first == nil {
			d := decision // capture
			first = &d
			continue
		}
		// Second and later outputs of this shared source: emit via Finalize().
		p.fanout = append(p.fanout, decisionToOutput(entry, decision))
	}

	if first == nil {
		return EntryDecision{Action: Skip}, nil
	}
	return *first, nil
}

// decide computes the EntryDecision for a single (source, outputKey) pair. It is
// the per-entry body that used to live inline in Process(); factoring it out lets
// both the returned decision and the fanout decisions share identical logic.
func (p *Q3CopyProcessor) decide(entry AssetEntry, lower, outputKey string, pe ProcessorEntry, readFile func() ([]byte, error)) (EntryDecision, error) {
	switch pe.Mode {
	case ModeCopy:
		// Output key IS the target path.
		destPath := outputKey
		if destPath == lower {
			destPath = "" // no rename needed
		}
		return EntryDecision{Action: Include, DestPath: destPath}, nil

	case ModeSkip:
		return EntryDecision{Action: Skip}, nil

	case ModeConvert:
		srcFmt := strings.TrimPrefix(path.Ext(lower), ".")

		// Try options-aware converter first (for formats that support Quality/Preprocess).
		if fnOpts, ok := LookupConverterWithOpts(srcFmt, pe.Converter); ok {
			data, err := readFile()
			if err != nil {
				return EntryDecision{}, fmt.Errorf("read %s for convert: %w", entry.Path, err)
			}
			opts := ConvertOptions{Quality: pe.Quality, Preprocess: pe.Preprocess}
			converted, err := fnOpts(data, opts)
			if err != nil {
				return EntryDecision{}, fmt.Errorf("convert %s: %w", entry.Path, err)
			}
			return EntryDecision{Action: Replace, DestPath: outputKey, Data: converted}, nil
		}

		// Fall back to simple converter (for TGA->PNG etc.).
		fn, err := LookupConverter(srcFmt, pe.Converter)
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
		return EntryDecision{Action: Replace, DestPath: outputKey, Data: converted}, nil

	case ModePatch:
		return EntryDecision{}, fmt.Errorf("pipeline: ModePatch not yet implemented for %s", entry.Path)

	default:
		return EntryDecision{}, fmt.Errorf("pipeline: unknown ProcessorMode %d for %s", pe.Mode, entry.Path)
	}
}

// decisionToOutput turns a fan-out EntryDecision into an OutputEntry, mirroring
// how process.go materializes the returned decision. For Include the content is
// copied from the source archive (SourcePak/SourceIndex); for Replace the
// converted bytes travel in Data (synthetic — no source read).
func decisionToOutput(entry AssetEntry, d EntryDecision) OutputEntry {
	outputPath := entry.Path
	if d.DestPath != "" {
		outputPath = d.DestPath
	}
	if d.Action == Replace {
		return OutputEntry{
			OutputPath:  outputPath,
			SourcePak:   "",
			SourceIndex: -1,
			Data:        d.Data,
			UncompSize:  int64(len(d.Data)),
		}
	}
	// Include: copy verbatim from the same source archive slot.
	return OutputEntry{
		OutputPath:  outputPath,
		SourcePak:   entry.SourcePak,
		SourceIndex: entry.SourceIndex,
		UncompSize:  entry.UncompSize,
	}
}

func (p *Q3CopyProcessor) Finalize() ([]OutputEntry, error) {
	var missingList []string
	for key, pe := range p.Entries {
		if !p.matched[key] {
			// Include PackIndex (the source filename inside the archive) so
			// the user can see what was being looked up — q3copy converts
			// wav→opus, tga→png etc., so the destination key alone doesn't
			// reveal the original asset path.
			missingList = append(missingList, key+" (want "+pe.Pack+":"+pe.PackIndex+")")
		}
	}
	if len(missingList) > 0 {
		sort.Strings(missingList)
		return nil, fmt.Errorf("q3copy: %d entries not found in any source archive:\n  %s",
			len(missingList), strings.Join(missingList, "\n  "))
	}
	// Emit the extra outputs of any one-source-many-targets entries. Each was
	// already counted in p.matched during Process(), so the missing-entry check
	// above is unaffected; these are the additional target rows that Process()'s
	// single EntryDecision return could not carry.
	return p.fanout, nil
}
