package pipeline

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

const (
	maxCookedMaterialFiles = 4096
	maxCookedMaterialBytes = 512 * 1024 * 1024
)

var cookedMaterialName = regexp.MustCompile(
	`^(wrmat-v[0-9]+-[0-9a-f]{16}\.wrmat|wrmat-package-v[0-9]+-[0-9a-f]{16}\.wrmatpkg|wrmp-v[0-9]+-[0-9a-f]{16}-[0-9a-f]{8}\.wrmatref)$`,
)

// CookedMaterialIncludeProcessor snapshots engine-produced material products
// into pax02. It deliberately does not parse or reproduce the C cook format:
// Release validates every receipt and content hash through the shared runtime
// reader. Reading bytes here makes archive publication independent of later
// changes to the producer directory.
type CookedMaterialIncludeProcessor struct {
	Root string
}

func (p *CookedMaterialIncludeProcessor) Process(
	entry AssetEntry, readFile func() ([]byte, error),
) (EntryDecision, error) {
	return EntryDecision{Action: Skip}, nil
}

func (p *CookedMaterialIncludeProcessor) Finalize() ([]OutputEntry, error) {
	if p == nil || p.Root == "" {
		return nil, fmt.Errorf("cooked materials: producer directory is required")
	}

	directory, err := os.Open(p.Root)
	if err != nil {
		return nil, fmt.Errorf("cooked materials: opening producer directory: %w", err)
	}
	defer directory.Close()
	directoryBefore, err := directory.Stat()
	if err != nil {
		return nil, fmt.Errorf("cooked materials: stat producer directory: %w", err)
	}

	names, err := directory.Readdirnames(maxCookedMaterialFiles + 1)
	if err != nil && !errors.Is(err, io.EOF) {
		return nil, fmt.Errorf("cooked materials: listing producer directory: %w", err)
	}
	if len(names) == 0 {
		return nil, fmt.Errorf("cooked materials: producer directory is empty")
	}
	if len(names) > maxCookedMaterialFiles {
		return nil, fmt.Errorf("cooked materials: file count exceeds %d", maxCookedMaterialFiles)
	}
	sort.Strings(names)

	seen := make(map[string]struct{}, len(names))
	output := make([]OutputEntry, 0, len(names))
	totalBytes := int64(0)
	productCount, packageCount, referenceCount := 0, 0, 0
	for _, name := range names {
		if !cookedMaterialName.MatchString(name) {
			return nil, fmt.Errorf("cooked materials: unexpected producer entry %q", name)
		}
		folded := strings.ToLower(name)
		if _, duplicate := seen[folded]; duplicate {
			return nil, fmt.Errorf("cooked materials: duplicate portable name %q", name)
		}
		seen[folded] = struct{}{}
		switch {
		case strings.HasSuffix(name, ".wrmatpkg"):
			packageCount++
		case strings.HasSuffix(name, ".wrmatref"):
			referenceCount++
		case strings.HasSuffix(name, ".wrmat"):
			productCount++
		}

		path := filepath.Join(p.Root, name)
		before, err := os.Lstat(path)
		if err != nil {
			return nil, fmt.Errorf("cooked materials: stat %q: %w", name, err)
		}
		if !before.Mode().IsRegular() {
			return nil, fmt.Errorf("cooked materials: %q is not a regular file", name)
		}
		if before.Size() <= 0 || before.Size() > maxCookedMaterialBytes-totalBytes {
			return nil, fmt.Errorf("cooked materials: invalid or over-budget size for %q", name)
		}

		data, err := os.ReadFile(path)
		if err != nil {
			return nil, fmt.Errorf("cooked materials: reading %q: %w", name, err)
		}
		after, err := os.Lstat(path)
		if err != nil || !os.SameFile(before, after) || before.Size() != after.Size() ||
			before.ModTime() != after.ModTime() || int64(len(data)) != before.Size() {
			return nil, fmt.Errorf("cooked materials: producer entry changed while snapshotting %q", name)
		}

		totalBytes += int64(len(data))
		output = append(output, OutputEntry{
			OutputPath:  filepath.ToSlash(filepath.Join("materials", "cooked", name)),
			SourceIndex: -1,
			Data:        data,
			UncompSize:  int64(len(data)),
		})
	}
	if productCount == 0 || packageCount == 0 || referenceCount == 0 {
		return nil, fmt.Errorf(
			"cooked materials: incomplete producer snapshot (products=%d packages=%d references=%d)",
			productCount, packageCount, referenceCount,
		)
	}
	directoryAfter, err := directory.Stat()
	if err != nil || !os.SameFile(directoryBefore, directoryAfter) ||
		directoryBefore.ModTime() != directoryAfter.ModTime() {
		return nil, fmt.Errorf("cooked materials: producer directory changed while snapshotting")
	}

	return output, nil
}
