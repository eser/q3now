package detect

import (
	"log/slog"
	"os"
	"path/filepath"
	"strings"
)

// Item represents a known game content entry in the inventory.
type Item struct {
	ID            string   // unique key, e.g. "q3_base"
	RequiredFiles []string // files relative to game root, e.g. "baseq3/pak0.pk3"
	Name          string   // display name
	Group         string   // game family: "q1", "q2", "q3", "qlive"
}

// ScanResult is the outcome of checking one inventory item.
type ScanResult struct {
	Item  Item
	Found bool
	Path  string // resolved absolute directory path (empty if not found)
}

// Items is the complete inventory of known game content.
var Items = []Item{
	// Q1 — original
	{ID: "q1_base", RequiredFiles: []string{"id1/pak0.pak", "id1/pak1.pak"}, Name: "Quake", Group: "q1"},
	{ID: "q1_mp1", RequiredFiles: []string{"hipnotic/pak0.pak"}, Name: "Quake 1 - Mission Pack 1 - Scourge of Armagon", Group: "q1"},
	{ID: "q1_mp2", RequiredFiles: []string{"rogue/pak0.pak"}, Name: "Quake 1 - Mission Pack 2 - Dissolution of Eternity", Group: "q1"},
	// Q1 — rerelease
	{ID: "q1_rerelease_base", RequiredFiles: []string{"rerelease/id1/pak0.pak"}, Name: "Quake 1 Remastered", Group: "q1"},
	{ID: "q1_rerelease_ctf", RequiredFiles: []string{"rerelease/ctf/pak0.pak"}, Name: "Quake 1 Remastered - Capture The Flag", Group: "q1"},
	{ID: "q1_rerelease_dopa", RequiredFiles: []string{"rerelease/dopa/pak0.pak"}, Name: "Quake 1 Remastered - Dimension of the Past", Group: "q1"},
	{ID: "q1_rerelease_mp1", RequiredFiles: []string{"rerelease/hipnotic/pak0.pak"}, Name: "Quake 1 Remastered - Mission Pack 1 - Scourge of Armagon", Group: "q1"},
	{ID: "q1_rerelease_mp2", RequiredFiles: []string{"rerelease/rogue/pak0.pak"}, Name: "Quake 1 Remastered - Mission Pack 2 - Dissolution of Eternity", Group: "q1"},
	{ID: "q1_rerelease_mg1", RequiredFiles: []string{"rerelease/mg1/pak0.pak"}, Name: "Quake 1 Remastered - Dimension of the Machine", Group: "q1"},

	// Q2
	{ID: "q2_base", RequiredFiles: []string{"baseq2/pak0.pak", "baseq2/pak1.pak", "baseq2/pak2.pak"}, Name: "Quake 2", Group: "q2"},
	{ID: "q2_ctf", RequiredFiles: []string{"ctf/pak0.pak"}, Name: "Quake 2 - Capture The Flag", Group: "q2"},
	{ID: "q2_xatrix", RequiredFiles: []string{"xatrix/pak0.pak"}, Name: "Quake 2 - Mission Pack 1 - The Reckoning", Group: "q2"},
	{ID: "q2_rogue", RequiredFiles: []string{"rogue/pak0.pak"}, Name: "Quake 2 - Mission Pack 2 - Ground Zero", Group: "q2"},
	{ID: "q2_rerelease_base", RequiredFiles: []string{"rerelease/baseq2/pak0.pak"}, Name: "Quake 2 Remastered", Group: "q2"},

	// Q3
	{ID: "q3_base", RequiredFiles: []string{
		"baseq3/pak0.pk3", "baseq3/pak1.pk3", "baseq3/pak2.pk3",
		"baseq3/pak3.pk3", "baseq3/pak4.pk3", "baseq3/pak5.pk3",
		"baseq3/pak6.pk3", "baseq3/pak7.pk3", "baseq3/pak8.pk3",
	}, Name: "Quake 3 Arena", Group: "q3"},
	{ID: "q3_missionpack", RequiredFiles: []string{"missionpack/pak0.pk3"}, Name: "Quake 3 Team Arena", Group: "q3"},

	// Quake Live
	{ID: "qlive_base", RequiredFiles: []string{"baseq3/pak00.pk3", "baseq3/bin.pk3"}, Name: "Quake Live", Group: "qlive"},
}

// ScanInventory checks a root path against inventory items for the given groups.
// Uses case-insensitive file matching to identify game content.
func ScanInventory(rootPath string, groups ...string) []ScanResult {
	if rootPath == "" {
		return nil
	}

	groupSet := make(map[string]bool, len(groups))
	for _, g := range groups {
		groupSet[g] = true
	}

	var results []ScanResult
	for _, item := range Items {
		if !groupSet[item.Group] {
			continue
		}
		resolvedDir, ok := checkRequiredFiles(rootPath, item)
		results = append(results, ScanResult{
			Item:  item,
			Found: ok,
			Path:  resolvedDir,
		})
	}

	slog.Info("inventory scan",
		"root", rootPath,
		"groups", groups,
		"found", countFound(results),
		"total", len(results))

	return results
}

// HasAnyInGroups returns true if any result in the given groups was found.
func HasAnyInGroups(results []ScanResult, groups ...string) bool {
	for _, r := range results {
		if !r.Found {
			continue
		}
		for _, g := range groups {
			if r.Item.Group == g {
				return true
			}
		}
	}
	return false
}

// FindFirst returns the first found result matching any of the given IDs.
func FindFirst(results []ScanResult, ids ...string) *ScanResult {
	for i, r := range results {
		if !r.Found {
			continue
		}
		for _, id := range ids {
			if r.Item.ID == id {
				return &results[i]
			}
		}
	}
	return nil
}

func countFound(results []ScanResult) int {
	n := 0
	for _, r := range results {
		if r.Found {
			n++
		}
	}
	return n
}

// checkRequiredFiles verifies all required files exist under root.
// Returns the resolved directory of the first file and true if all files are found.
func checkRequiredFiles(root string, item Item) (string, bool) {
	if len(item.RequiredFiles) == 0 {
		return "", false
	}
	var resolvedDir string
	for _, reqFile := range item.RequiredFiles {
		resolved, ok := resolveCaseInsensitiveFile(root, reqFile)
		if !ok {
			return "", false
		}
		if resolvedDir == "" {
			resolvedDir = filepath.Dir(resolved)
		}
	}
	return resolvedDir, true
}

// resolveCaseInsensitiveFile resolves a file path (dir/file) case-insensitively.
// Directory components are walked with resolveCaseInsensitive, then the file
// is matched case-insensitively in the resolved directory.
func resolveCaseInsensitiveFile(root, relPath string) (string, bool) {
	dir := filepath.Dir(relPath)
	file := filepath.Base(relPath)

	// Resolve directory components case-insensitively.
	resolvedDir := root
	if dir != "." {
		var ok bool
		resolvedDir, ok = resolveCaseInsensitive(root, dir)
		if !ok {
			return "", false
		}
	}

	// Find the file case-insensitively in the resolved directory.
	entries, err := os.ReadDir(resolvedDir)
	if err != nil {
		return "", false
	}
	lowerFile := strings.ToLower(file)
	for _, e := range entries {
		if !e.IsDir() && strings.ToLower(e.Name()) == lowerFile {
			return filepath.Join(resolvedDir, e.Name()), true
		}
	}
	return "", false
}

// resolveCaseInsensitive walks relPath components under root,
// matching each directory name case-insensitively.
// Follows symlinks to detect symlinked game directories.
func resolveCaseInsensitive(root, relPath string) (string, bool) {
	parts := strings.Split(filepath.ToSlash(relPath), "/")
	current := root
	for _, part := range parts {
		entries, err := os.ReadDir(current)
		if err != nil {
			return "", false
		}
		lowerPart := strings.ToLower(part)
		found := false
		for _, e := range entries {
			if strings.ToLower(e.Name()) != lowerPart {
				continue
			}
			candidate := filepath.Join(current, e.Name())
			// Use os.Stat (not e.IsDir) to follow symlinks
			info, err := os.Stat(candidate)
			if err != nil || !info.IsDir() {
				continue
			}
			current = candidate
			found = true
			break
		}
		if !found {
			return "", false
		}
	}
	return current, true
}
