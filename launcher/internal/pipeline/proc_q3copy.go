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
type Q3CopyProcessor struct {
	Entries     map[string]ProcessorEntry
	matched     map[string]bool   // tracks which entries were fulfilled
	byPackIndex map[string]string // lowercase pack-index → output key (built lazily)
}

// resolvePackIndex returns the source path for an entry (PackIndex if set, otherwise the key).
func resolvePackIndex(key string, pe ProcessorEntry) string {
	if pe.PackIndex != "" {
		return pe.PackIndex
	}
	return key
}

// initIndex builds the reverse lookup from pack-index → output key.
func (p *Q3CopyProcessor) initIndex() {
	if p.byPackIndex != nil {
		return
	}
	p.byPackIndex = make(map[string]string, len(p.Entries))
	p.matched = make(map[string]bool, len(p.Entries))
	for key, pe := range p.Entries {
		idx := strings.ToLower(resolvePackIndex(key, pe))
		p.byPackIndex[idx] = key
	}
}

func (p *Q3CopyProcessor) Process(entry AssetEntry, readFile func() ([]byte, error)) (EntryDecision, error) {
	p.initIndex()

	lower := strings.ToLower(entry.Path)
	outputKey, ok := p.byPackIndex[lower]
	if !ok {
		return EntryDecision{Action: Skip}, nil
	}
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
		return EntryDecision{Action: Skip}, nil
	}
	slog.Debug("q3copy: matched", "path", lower, "output", outputKey, "pack", entry.SourcePak)
	p.matched[outputKey] = true

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
	return nil, nil
}
