package pipeline

import (
	"context"
	"os"
	"path/filepath"
	"testing"

	sw3z "github.com/eser/q3now/tools/sw3z-archiver"
)

func writeCookedMaterialFixture(t *testing.T, root, name, contents string) {
	t.Helper()
	if err := os.WriteFile(filepath.Join(root, name), []byte(contents), 0600); err != nil {
		t.Fatal(err)
	}
}

func TestCookedMaterialIncludeProcessorSnapshotsDeterministically(t *testing.T) {
	root := t.TempDir()
	writeCookedMaterialFixture(t, root, "wrmp-v1-0000000000000001-00000002.wrmatref", "reference")
	writeCookedMaterialFixture(t, root, "wrmat-v4-0000000000000003.wrmat", "material")
	writeCookedMaterialFixture(t, root, "wrmat-package-v1-0000000000000002.wrmatpkg", "package")

	processor := &CookedMaterialIncludeProcessor{Root: root}
	output, err := processor.Finalize()
	if err != nil {
		t.Fatal(err)
	}
	if len(output) != 3 {
		t.Fatalf("got %d outputs, want 3", len(output))
	}
	want := []string{
		"materials/cooked/wrmat-package-v1-0000000000000002.wrmatpkg",
		"materials/cooked/wrmat-v4-0000000000000003.wrmat",
		"materials/cooked/wrmp-v1-0000000000000001-00000002.wrmatref",
	}
	for i := range want {
		if output[i].OutputPath != want[i] {
			t.Fatalf("output[%d] = %q, want %q", i, output[i].OutputPath, want[i])
		}
		if output[i].Data == nil || output[i].SourceIndex != -1 {
			t.Fatalf("output[%d] is not a byte snapshot", i)
		}
	}

	writeCookedMaterialFixture(t, root, "wrmat-v4-0000000000000003.wrmat", "changed after snapshot")
	if string(output[1].Data) != "material" {
		t.Fatal("snapshot changed with producer directory")
	}

	archiveRoot := t.TempDir()
	sink := &SW3ZSink{DestDir: archiveRoot, OutputName: "pax02.sw3z"}
	if err := sink.Run(context.Background(), output, make(chan Progress, 8)); err != nil {
		t.Fatal(err)
	}
	archive, err := os.Open(filepath.Join(archiveRoot, "pax02.sw3z"))
	if err != nil {
		t.Fatal(err)
	}
	defer archive.Close()
	header, err := sw3z.ReadHeader(archive)
	if err != nil {
		t.Fatal(err)
	}
	entries, err := sw3z.ReadIndex(archive, header.EntryCount)
	if err != nil {
		t.Fatal(err)
	}
	strings, err := sw3z.ReadStringTable(archive, header.StringTableSize)
	if err != nil {
		t.Fatal(err)
	}
	for i := range entries {
		if got := sw3z.EntryPath(entries[i], strings); got != want[i] {
			t.Fatalf("archive entry[%d] = %q, want %q", i, got, want[i])
		}
		bytes, err := sw3z.VerifyEntry(archive, entries[i])
		if err != nil {
			t.Fatal(err)
		}
		if string(bytes) != string(output[i].Data) {
			t.Fatalf("archive entry[%d] payload changed", i)
		}
	}
}

func TestCookedMaterialIncludeProcessorFailsClosed(t *testing.T) {
	for _, test := range []struct {
		name  string
		setup func(*testing.T, string)
	}{
		{"empty", func(t *testing.T, root string) {}},
		{"unexpected", func(t *testing.T, root string) {
			writeCookedMaterialFixture(t, root, "material.tmp", "partial")
		}},
		{"invalid-name", func(t *testing.T, root string) {
			writeCookedMaterialFixture(t, root, "friendly.wrmat", "unsealed")
		}},
		{"unreachable-products-only", func(t *testing.T, root string) {
			writeCookedMaterialFixture(t, root, "wrmat-v4-0000000000000001.wrmat", "material")
		}},
	} {
		t.Run(test.name, func(t *testing.T) {
			root := t.TempDir()
			test.setup(t, root)
			processor := &CookedMaterialIncludeProcessor{Root: root}
			if output, err := processor.Finalize(); err == nil || output != nil {
				t.Fatalf("got output=%v err=%v, want fail-closed", output, err)
			}
		})
	}
}
