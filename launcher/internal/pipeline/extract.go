package pipeline

import (
	"context"
	"fmt"
	"log/slog"
	"path/filepath"
	"sort"
	"strings"

	"github.com/eser/q3now/launcher/internal/archive"
	"github.com/eser/q3now/launcher/internal/safepath"
)

// ExtractTransform scans archive files across multiple source directories
// and builds a combined []AssetEntry with origin tags.
type ExtractTransform struct {
	Sources []SourceGroup
}

func (e *ExtractTransform) Name() string { return "extract" }

// Run scans all sources and returns a combined list of asset entries.
func (e *ExtractTransform) Run(ctx context.Context, progress chan<- Progress) ([]AssetEntry, error) {
	if len(e.Sources) == 0 {
		return nil, fmt.Errorf("extract: no source groups provided")
	}

	var allEntries []AssetEntry
	totalArchives := 0
	scanned := 0

	// First pass: count total archives for progress.
	for _, src := range e.Sources {
		files, err := findArchiveFiles(src.Dir)
		if err != nil {
			return nil, fmt.Errorf("extract: scanning %s: %w", src.Origin, err)
		}
		totalArchives += len(files)
	}

	if totalArchives == 0 {
		return nil, fmt.Errorf("extract: no archive files (pk3/pak) found in any source")
	}

	for _, src := range e.Sources {
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		default:
		}

		files, err := findArchiveFiles(src.Dir)
		if err != nil {
			return nil, fmt.Errorf("extract: scanning %s: %w", src.Origin, err)
		}

		slog.Info("extract: scanning origin", "origin", src.Origin, "archives", len(files))

		var originEntries []AssetEntry

		for _, archivePath := range files {
			select {
			case <-ctx.Done():
				return nil, ctx.Err()
			default:
			}

			progress <- Progress{
				Step:    "extract",
				Current: int64(scanned),
				Total:   int64(totalArchives),
				Message: fmt.Sprintf("Scanning %s (%s)", filepath.Base(archivePath), src.Origin),
			}

			entries, err := scanArchive(archivePath, src.Origin)
			if err != nil {
				return nil, fmt.Errorf("extract: scanning %s: %w", filepath.Base(archivePath), err)
			}
			originEntries = append(originEntries, entries...)
			scanned++
		}

		// Deduplicate within this origin (higher pak number wins).
		deduped, overridden := deduplicateEntries(originEntries)
		if overridden > 0 {
			slog.Info("extract: deduplicated entries",
				"origin", src.Origin,
				"scanned", len(originEntries),
				"unique", len(deduped),
				"overridden", overridden)
		}
		allEntries = append(allEntries, deduped...)
	}

	slog.Info("extract: complete", "entries", len(allEntries), "archives", totalArchives)
	progress <- Progress{
		Step:    "extract",
		Current: int64(totalArchives),
		Total:   int64(totalArchives),
		Message: fmt.Sprintf("Scanned %d files from %d archives", len(allEntries), totalArchives),
	}

	return allEntries, nil
}

// findArchiveFiles returns sorted pak*.pk3 and pak*.pak files in a directory.
// If both pakN.pk3 and pakN.pak exist for the same base name, PK3 is preferred.
func findArchiveFiles(dir string) ([]string, error) {
	pk3s, err := filepath.Glob(filepath.Join(dir, "pak*.pk3"))
	if err != nil {
		return nil, err
	}
	paks, err := filepath.Glob(filepath.Join(dir, "pak*.pak"))
	if err != nil {
		return nil, err
	}

	// Build set of PK3 base names (without extension) for dedup.
	pk3Bases := make(map[string]bool, len(pk3s))
	for _, p := range pk3s {
		base := strings.TrimSuffix(filepath.Base(p), filepath.Ext(p))
		pk3Bases[base] = true
	}

	// Add PAK files that don't collide with a PK3 of the same base name.
	var merged []string
	merged = append(merged, pk3s...)
	for _, p := range paks {
		base := strings.TrimSuffix(filepath.Base(p), filepath.Ext(p))
		if !pk3Bases[base] {
			merged = append(merged, p)
		} else {
			slog.Info("extract: preferring pk3 over pak", "base", base, "dir", dir)
		}
	}

	sort.Strings(merged)
	return merged, nil
}

// deduplicateEntries removes duplicate paths within a single origin,
// keeping the last occurrence (highest-numbered pak wins).
// Comparison is case-insensitive, matching Quake 3's VFS behavior.
// The original casing from the winning entry is preserved.
//
//	pak0: [a.txt, b.txt]   pak1: [b.txt, c.txt]
//	→ result: [a.txt(pak0), b.txt(pak1), c.txt(pak1)], 1 override
func deduplicateEntries(entries []AssetEntry) ([]AssetEntry, int) {
	seen := make(map[string]int, len(entries)) // lowercase path → index in result
	result := make([]AssetEntry, 0, len(entries))
	overridden := 0

	for _, e := range entries {
		key := strings.ToLower(e.Path)
		if idx, exists := seen[key]; exists {
			slog.Debug("extract: pak override",
				"path", e.Path,
				"old_pak", filepath.Base(result[idx].SourcePak),
				"new_pak", filepath.Base(e.SourcePak))
			result[idx] = e
			overridden++
		} else {
			seen[key] = len(result)
			result = append(result, e)
		}
	}

	return result, overridden
}

// scanArchive opens an archive and builds AssetEntry metadata for every entry.
func scanArchive(archivePath, origin string) ([]AssetEntry, error) {
	r, err := archive.Open(archivePath)
	if err != nil {
		return nil, err
	}
	defer r.Close()

	archiveEntries := r.Entries()
	var entries []AssetEntry

	for i, ae := range archiveEntries {
		if !safepath.IsSafe(ae.Path) {
			slog.Warn("skipping unsafe path in archive",
				"path", ae.Path, "archive", filepath.Base(archivePath))
			continue
		}
		entries = append(entries, AssetEntry{
			Origin:      origin,
			Path:        ae.Path,
			SourcePak:   archivePath,
			SourceIndex: i,
			UncompSize:  int64(ae.UncompressedSize),
		})
	}

	return entries, nil
}
