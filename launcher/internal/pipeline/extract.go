package pipeline

import (
	"context"
	"fmt"
	"log/slog"
	"os"
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
			allEntries = append(allEntries, entries...)
			scanned++
		}
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

// findArchiveFiles recursively finds sorted pak*.pk3 and pak*.pak files under dir.
// If both pakN.pk3 and pakN.pak exist in the same directory, PK3 is preferred.
func findArchiveFiles(dir string) ([]string, error) {
	var pk3s, paks []string

	err := filepath.WalkDir(dir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			return nil
		}
		name := strings.ToLower(d.Name())
		if strings.HasSuffix(name, ".pk3") { // strings.HasPrefix(name, "pak") &&
			pk3s = append(pk3s, path)
		} else if strings.HasSuffix(name, ".pak") { // strings.HasPrefix(name, "pak") &&
			paks = append(paks, path)
		}
		return nil
	})
	if err != nil {
		return nil, err
	}

	// Build set of PK3 keys (dir + base name without extension) for dedup.
	pk3Keys := make(map[string]bool, len(pk3s))
	for _, p := range pk3s {
		key := filepath.Join(filepath.Dir(p), strings.TrimSuffix(filepath.Base(p), filepath.Ext(p)))
		pk3Keys[key] = true
	}

	// Add PAK files that don't collide with a PK3 of the same base name in the same dir.
	var merged []string
	merged = append(merged, pk3s...)
	for _, p := range paks {
		key := filepath.Join(filepath.Dir(p), strings.TrimSuffix(filepath.Base(p), filepath.Ext(p)))
		if !pk3Keys[key] {
			merged = append(merged, p)
		} else {
			slog.Info("extract: preferring pk3 over pak", "base", filepath.Base(p), "dir", filepath.Dir(p))
		}
	}

	sort.Strings(merged)
	for _, f := range merged {
		slog.Info("extract: found archive", "path", f)
	}
	return merged, nil
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
