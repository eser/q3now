package pipeline

import (
	"bytes"
	"encoding/binary"
	"strings"
	"testing"
)

// buildMinimalTGA creates a minimal 2x2 uncompressed 32-bit TGA for testing conversions.
func buildMinimalTGA() []byte {
	hdr := tgaHeader{
		IDLength:        0,
		ColorMapType:    0,
		ImageType:       2,
		Width:           2,
		Height:          2,
		BitsPerPixel:    32,
		ImageDescriptor: 0x28, // top-to-bottom + 8 alpha bits
	}
	var buf bytes.Buffer
	binary.Write(&buf, binary.LittleEndian, &hdr)

	// 4 pixels in BGRA order.
	for i := 0; i < 4; i++ {
		buf.Write([]byte{0, 0, 255, 255}) // blue=0, green=0, red=255, alpha=255
	}
	return buf.Bytes()
}

func TestQ3CopyProcessor_ModeCopy_Default(t *testing.T) {
	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"maps/q3dm1.bsp": {},
		},
	}

	entry := AssetEntry{Origin: "q3_base", Path: "maps/q3dm1.bsp"}
	decision, err := proc.Process(entry, nil)
	if err != nil {
		t.Fatalf("Process: %v", err)
	}
	if decision.Action != Include {
		t.Errorf("expected Include, got %d", decision.Action)
	}
	if decision.DestPath != "" {
		t.Errorf("expected empty DestPath for default copy, got %q", decision.DestPath)
	}
}

func TestQ3CopyProcessor_ModeCopy_WithRename(t *testing.T) {
	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"maps/new.bsp": {PackIndex: "maps/old.bsp"},
		},
	}

	entry := AssetEntry{Origin: "q3_base", Path: "maps/old.bsp"}
	decision, err := proc.Process(entry, nil)
	if err != nil {
		t.Fatalf("Process: %v", err)
	}
	if decision.Action != Include {
		t.Errorf("expected Include, got %d", decision.Action)
	}
	if decision.DestPath != "maps/new.bsp" {
		t.Errorf("expected DestPath %q, got %q", "maps/new.bsp", decision.DestPath)
	}
}

func TestQ3CopyProcessor_ModeSkip(t *testing.T) {
	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"vm/cgame.qvm": {Mode: ModeSkip},
		},
	}

	entry := AssetEntry{Origin: "q3_base", Path: "vm/cgame.qvm"}
	decision, err := proc.Process(entry, nil)
	if err != nil {
		t.Fatalf("Process: %v", err)
	}
	if decision.Action != Skip {
		t.Errorf("expected Skip, got %d", decision.Action)
	}
}

func TestQ3CopyProcessor_ModeConvert(t *testing.T) {
	tgaData := buildMinimalTGA()

	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"gfx/test.png": {Mode: ModeConvert, Converter: "png", PackIndex: "gfx/test.tga"},
		},
	}

	entry := AssetEntry{Origin: "q3_base", Path: "gfx/test.tga"}
	readFile := func() ([]byte, error) {
		return tgaData, nil
	}

	decision, err := proc.Process(entry, readFile)
	if err != nil {
		t.Fatalf("Process: %v", err)
	}
	if decision.Action != Replace {
		t.Fatalf("expected Replace, got %d", decision.Action)
	}
	if decision.DestPath != "gfx/test.png" {
		t.Errorf("expected DestPath %q, got %q", "gfx/test.png", decision.DestPath)
	}
	if decision.Data == nil {
		t.Fatal("expected non-nil Data for Replace action")
	}
	// Verify the output is valid PNG by checking magic bytes.
	pngMagic := []byte{0x89, 0x50, 0x4E, 0x47}
	if len(decision.Data) < 4 || !bytes.Equal(decision.Data[:4], pngMagic) {
		t.Error("converted data does not start with PNG magic bytes")
	}
}

func TestQ3CopyProcessor_ModePatch(t *testing.T) {
	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"maps/q3dm1.bsp": {Mode: ModePatch},
		},
	}

	entry := AssetEntry{Origin: "q3_base", Path: "maps/q3dm1.bsp"}
	_, err := proc.Process(entry, nil)
	if err == nil {
		t.Fatal("expected error for ModePatch")
	}
	if !strings.Contains(err.Error(), "not yet implemented") {
		t.Errorf("expected error containing %q, got: %v", "not yet implemented", err)
	}
}

func TestQ3CopyProcessor_UnknownMode(t *testing.T) {
	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"test.txt": {Mode: ProcessorMode(99)},
		},
	}

	entry := AssetEntry{Origin: "q3_base", Path: "test.txt"}
	_, err := proc.Process(entry, nil)
	if err == nil {
		t.Fatal("expected error for unknown ProcessorMode")
	}
	if !strings.Contains(err.Error(), "unknown") {
		t.Errorf("expected error containing %q, got: %v", "unknown", err)
	}
}

func TestQ3CopyProcessor_EntryNotInMap(t *testing.T) {
	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"maps/q3dm1.bsp": {},
		},
	}

	entry := AssetEntry{Origin: "q3_base", Path: "maps/q3dm99.bsp"}
	decision, err := proc.Process(entry, nil)
	if err != nil {
		t.Fatalf("Process: %v", err)
	}
	if decision.Action != Skip {
		t.Errorf("expected Skip for missing entry, got %d", decision.Action)
	}
}

func TestQ3CopyProcessor_CaseInsensitiveLookup(t *testing.T) {
	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"maps/q3dm1.bsp": {},
		},
	}

	// Entry with mixed case — should match after lowercasing.
	cases := []string{
		"Maps/Q3DM1.bsp",
		"MAPS/Q3DM1.BSP",
		"maps/Q3dm1.Bsp",
	}

	for _, path := range cases {
		t.Run(path, func(t *testing.T) {
			entry := AssetEntry{Origin: "q3_base", Path: path}
			decision, err := proc.Process(entry, nil)
			if err != nil {
				t.Fatalf("Process(%q): %v", path, err)
			}
			if decision.Action != Include {
				t.Errorf("Process(%q): expected Include (case-insensitive), got Skip", path)
			}
		})
	}
}

func TestQ3CopyProcessor_Finalize(t *testing.T) {
	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{},
	}
	entries, err := proc.Finalize()
	if err != nil {
		t.Fatalf("Finalize: %v", err)
	}
	if len(entries) != 0 {
		t.Errorf("expected 0 synthetic entries, got %d", len(entries))
	}
}
