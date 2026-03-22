package pipeline

import (
	"archive/zip"
	"context"
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

// createExtractPk3 creates a pk3 file for extract tests.
func createExtractPk3(t *testing.T, dir, name string, files map[string][]byte) {
	t.Helper()
	path := filepath.Join(dir, name)
	f, err := os.Create(path)
	if err != nil {
		t.Fatalf("create pk3: %v", err)
	}
	defer f.Close()
	zw := zip.NewWriter(f)
	for name, data := range files {
		w, _ := zw.Create(name)
		w.Write(data)
	}
	zw.Close()
}

// createExtractPak creates a minimal PAK file for extract tests.
func createExtractPak(t *testing.T, dir, name string, files map[string][]byte) {
	t.Helper()
	path := filepath.Join(dir, name)
	f, err := os.Create(path)
	if err != nil {
		t.Fatalf("create pak: %v", err)
	}
	defer f.Close()

	// Write placeholder header.
	var hdr [12]byte
	f.Write(hdr[:])

	type entry struct {
		name   string
		offset uint32
		size   uint32
	}
	var entries []entry
	for n, data := range files {
		offset, _ := f.Seek(0, 1)
		f.Write(data)
		entries = append(entries, entry{n, uint32(offset), uint32(len(data))})
	}

	dirOffset, _ := f.Seek(0, 1)
	for _, e := range entries {
		var buf [64]byte
		copy(buf[:56], e.name)
		binary.LittleEndian.PutUint32(buf[56:60], e.offset)
		binary.LittleEndian.PutUint32(buf[60:64], e.size)
		f.Write(buf[:])
	}

	copy(hdr[0:4], "PACK")
	binary.LittleEndian.PutUint32(hdr[4:8], uint32(dirOffset))
	binary.LittleEndian.PutUint32(hdr[8:12], uint32(len(entries)*64))
	f.Seek(0, 0)
	f.Write(hdr[:])
}

func TestFindArchiveFiles_DualGlob(t *testing.T) {
	dir := t.TempDir()
	createExtractPk3(t, dir, "pak0.pk3", map[string][]byte{"a.txt": []byte("a")})
	createExtractPak(t, dir, "pak1.pak", map[string][]byte{"b.txt": []byte("b")})

	files, err := findArchiveFiles(dir)
	if err != nil {
		t.Fatalf("findArchiveFiles: %v", err)
	}
	if len(files) != 2 {
		t.Fatalf("expected 2 files, got %d: %v", len(files), files)
	}
}

func TestFindArchiveFiles_PK3OverPAK(t *testing.T) {
	dir := t.TempDir()
	// Both pak0.pk3 and pak0.pak exist — PK3 should win.
	createExtractPk3(t, dir, "pak0.pk3", map[string][]byte{"a.txt": []byte("a")})
	createExtractPak(t, dir, "pak0.pak", map[string][]byte{"b.txt": []byte("b")})

	files, err := findArchiveFiles(dir)
	if err != nil {
		t.Fatalf("findArchiveFiles: %v", err)
	}
	if len(files) != 1 {
		t.Fatalf("expected 1 file (PK3 preferred), got %d: %v", len(files), files)
	}
	if filepath.Ext(files[0]) != ".pk3" {
		t.Errorf("expected .pk3, got %s", files[0])
	}
}

func TestScanArchive_SourceIndex(t *testing.T) {
	dir := t.TempDir()
	createExtractPk3(t, dir, "pak0.pk3", map[string][]byte{
		"a.txt": []byte("aaa"),
		"b.txt": []byte("bbb"),
	})

	entries, err := scanArchive(filepath.Join(dir, "pak0.pk3"), "q3_base")
	if err != nil {
		t.Fatalf("scanArchive: %v", err)
	}

	for i, e := range entries {
		if e.SourceIndex != i {
			t.Errorf("entry %d: expected SourceIndex %d, got %d", i, i, e.SourceIndex)
		}
		if e.Origin != "q3_base" {
			t.Errorf("entry %d: expected origin q3_base, got %s", i, e.Origin)
		}
	}
}

func TestFindArchiveFiles_EmptyDir(t *testing.T) {
	dir := t.TempDir()
	files, err := findArchiveFiles(dir)
	if err != nil {
		t.Fatalf("findArchiveFiles: %v", err)
	}
	if len(files) != 0 {
		t.Errorf("expected 0 files in empty dir, got %d", len(files))
	}
}

// --- deduplicateEntries unit tests ---

func TestDeduplicateEntries_HigherPakWins(t *testing.T) {
	entries := []AssetEntry{
		{Origin: "q3_base", Path: "gfx/blood.tga", SourcePak: "/dir/pak0.pk3", SourceIndex: 0, UncompSize: 100},
		{Origin: "q3_base", Path: "gfx/blood.tga", SourcePak: "/dir/pak8.pk3", SourceIndex: 5, UncompSize: 200},
	}

	result, overridden := deduplicateEntries(entries)
	if overridden != 1 {
		t.Errorf("expected 1 override, got %d", overridden)
	}
	if len(result) != 1 {
		t.Fatalf("expected 1 entry, got %d", len(result))
	}
	if result[0].SourcePak != "/dir/pak8.pk3" {
		t.Errorf("expected pak8.pk3 to win, got %s", result[0].SourcePak)
	}
	if result[0].UncompSize != 200 {
		t.Errorf("expected size 200 from pak8, got %d", result[0].UncompSize)
	}
}

func TestDeduplicateEntries_NoDuplicates(t *testing.T) {
	entries := []AssetEntry{
		{Origin: "q3_base", Path: "a.txt", SourcePak: "/dir/pak0.pk3"},
		{Origin: "q3_base", Path: "b.txt", SourcePak: "/dir/pak0.pk3"},
		{Origin: "q3_base", Path: "c.txt", SourcePak: "/dir/pak1.pk3"},
	}

	result, overridden := deduplicateEntries(entries)
	if overridden != 0 {
		t.Errorf("expected 0 overrides, got %d", overridden)
	}
	if len(result) != 3 {
		t.Errorf("expected 3 entries, got %d", len(result))
	}
}

func TestDeduplicateEntries_Empty(t *testing.T) {
	result, overridden := deduplicateEntries(nil)
	if overridden != 0 {
		t.Errorf("expected 0 overrides, got %d", overridden)
	}
	if len(result) != 0 {
		t.Errorf("expected 0 entries, got %d", len(result))
	}
}

func TestDeduplicateEntries_PreservesOrder(t *testing.T) {
	entries := []AssetEntry{
		{Origin: "q3_base", Path: "a.txt", SourcePak: "/dir/pak0.pk3"},
		{Origin: "q3_base", Path: "b.txt", SourcePak: "/dir/pak0.pk3"},
		{Origin: "q3_base", Path: "b.txt", SourcePak: "/dir/pak1.pk3"},
		{Origin: "q3_base", Path: "c.txt", SourcePak: "/dir/pak1.pk3"},
	}

	result, overridden := deduplicateEntries(entries)
	if overridden != 1 {
		t.Errorf("expected 1 override, got %d", overridden)
	}
	if len(result) != 3 {
		t.Fatalf("expected 3 entries, got %d", len(result))
	}
	// Order: a.txt(pak0), b.txt(pak1 — updated), c.txt(pak1).
	if result[0].Path != "a.txt" || result[0].SourcePak != "/dir/pak0.pk3" {
		t.Errorf("result[0]: expected a.txt from pak0, got %s from %s", result[0].Path, result[0].SourcePak)
	}
	if result[1].Path != "b.txt" || result[1].SourcePak != "/dir/pak1.pk3" {
		t.Errorf("result[1]: expected b.txt from pak1, got %s from %s", result[1].Path, result[1].SourcePak)
	}
	if result[2].Path != "c.txt" || result[2].SourcePak != "/dir/pak1.pk3" {
		t.Errorf("result[2]: expected c.txt from pak1, got %s from %s", result[2].Path, result[2].SourcePak)
	}
}

func TestDeduplicateEntries_TripleOverride(t *testing.T) {
	entries := []AssetEntry{
		{Origin: "q3_base", Path: "shared.txt", SourcePak: "/dir/pak0.pk3", UncompSize: 10},
		{Origin: "q3_base", Path: "shared.txt", SourcePak: "/dir/pak3.pk3", UncompSize: 30},
		{Origin: "q3_base", Path: "shared.txt", SourcePak: "/dir/pak8.pk3", UncompSize: 80},
	}

	result, overridden := deduplicateEntries(entries)
	if overridden != 2 {
		t.Errorf("expected 2 overrides, got %d", overridden)
	}
	if len(result) != 1 {
		t.Fatalf("expected 1 entry, got %d", len(result))
	}
	if result[0].SourcePak != "/dir/pak8.pk3" {
		t.Errorf("expected pak8 to win, got %s", result[0].SourcePak)
	}
}

func TestDeduplicateEntries_CaseInsensitive(t *testing.T) {
	entries := []AssetEntry{
		{Origin: "q3_base", Path: "textures/Gothic_Block/blocks18b.tga", SourcePak: "/dir/pak0.pk3"},
		{Origin: "q3_base", Path: "textures/gothic_block/blocks18b.tga", SourcePak: "/dir/pak8.pk3"},
	}

	result, overridden := deduplicateEntries(entries)
	if overridden != 1 {
		t.Errorf("expected 1 override (case-insensitive match), got %d", overridden)
	}
	if len(result) != 1 {
		t.Fatalf("expected 1 entry, got %d", len(result))
	}
	// Pak8 wins — its casing is preserved.
	if result[0].SourcePak != "/dir/pak8.pk3" {
		t.Errorf("expected pak8 to win, got %s", result[0].SourcePak)
	}
	if result[0].Path != "textures/gothic_block/blocks18b.tga" {
		t.Errorf("expected pak8's casing preserved, got %s", result[0].Path)
	}
}

// --- ExtractTransform.Run integration tests ---

func TestExtractRun_DedupAcrossPaks(t *testing.T) {
	dir := t.TempDir()
	// pak0 and pak1 both contain "shared.txt" — pak1 should win.
	createExtractPk3(t, dir, "pak0.pk3", map[string][]byte{
		"shared.txt": []byte("old version"),
		"only0.txt":  []byte("unique to pak0"),
	})
	createExtractPk3(t, dir, "pak1.pk3", map[string][]byte{
		"shared.txt": []byte("new version"),
		"only1.txt":  []byte("unique to pak1"),
	})

	et := &ExtractTransform{
		Sources: []SourceGroup{{Origin: "q3_base", Dir: dir}},
	}
	progress := make(chan Progress, 100)
	go func() { for range progress {} }()

	entries, err := et.Run(context.Background(), progress)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}

	// Expect 3 unique entries: shared.txt(pak1), only0.txt(pak0), only1.txt(pak1).
	if len(entries) != 3 {
		t.Fatalf("expected 3 deduplicated entries, got %d", len(entries))
	}

	// Verify shared.txt comes from pak1.
	for _, e := range entries {
		if e.Path == "shared.txt" {
			if filepath.Base(e.SourcePak) != "pak1.pk3" {
				t.Errorf("shared.txt: expected pak1.pk3, got %s", filepath.Base(e.SourcePak))
			}
			return
		}
	}
	t.Error("shared.txt not found in entries")
}

func TestExtractRun_DedupPerOrigin(t *testing.T) {
	dir0 := t.TempDir()
	dir1 := t.TempDir()
	// Both origins have "common.txt" — both should survive (dedup is per-origin).
	createExtractPk3(t, dir0, "pak0.pk3", map[string][]byte{
		"common.txt": []byte("from origin 0"),
	})
	createExtractPk3(t, dir1, "pak0.pk3", map[string][]byte{
		"common.txt": []byte("from origin 1"),
	})

	et := &ExtractTransform{
		Sources: []SourceGroup{
			{Origin: "q3_base", Dir: dir0},
			{Origin: "q3_missionpack", Dir: dir1},
		},
	}
	progress := make(chan Progress, 100)
	go func() { for range progress {} }()

	entries, err := et.Run(context.Background(), progress)
	if err != nil {
		t.Fatalf("Run: %v", err)
	}

	// Both origins should have their own "common.txt".
	if len(entries) != 2 {
		t.Fatalf("expected 2 entries (one per origin), got %d", len(entries))
	}

	origins := map[string]bool{}
	for _, e := range entries {
		if e.Path != "common.txt" {
			t.Errorf("unexpected path: %s", e.Path)
		}
		origins[e.Origin] = true
	}
	if !origins["q3_base"] || !origins["q3_missionpack"] {
		t.Errorf("expected both origins, got %v", origins)
	}
}
