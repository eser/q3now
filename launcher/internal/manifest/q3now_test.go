package manifest

import (
	"os"
	"path/filepath"
	"testing"
)

func TestWriteAndReadRoundtrip(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "q3now.json")

	inventory := map[string]string{
		"q3_base":       "/some/path/baseq3",
		"q3_missionpack": "/some/path/missionpack",
		"q1_base":       "/another/path/Id1",
	}

	if err := WriteWithInventory(path, inventory); err != nil {
		t.Fatalf("WriteWithInventory failed: %v", err)
	}

	m, err := Read(path)
	if err != nil {
		t.Fatalf("Read failed: %v", err)
	}

	if m.Version != 1 {
		t.Errorf("expected version 1, got %d", m.Version)
	}
	if m.ImportedAt == "" {
		t.Error("expected non-empty importedAt")
	}
	if len(m.Inventory) != 3 {
		t.Errorf("expected 3 inventory items, got %d", len(m.Inventory))
	}
	if m.Inventory["q3_base"] != "/some/path/baseq3" {
		t.Errorf("expected q3_base=/some/path/baseq3, got %s", m.Inventory["q3_base"])
	}
}

func TestRead_EmptyInventory(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "q3now.json")

	// Write a manifest with empty inventory
	if err := WriteWithInventory(path, map[string]string{}); err != nil {
		t.Fatalf("WriteWithInventory failed: %v", err)
	}

	_, err := Read(path)
	if err == nil {
		t.Fatal("expected error for empty inventory, got nil")
	}
	if _, ok := err.(*InvalidManifestError); !ok {
		t.Errorf("expected InvalidManifestError, got %T: %v", err, err)
	}
}

func TestRead_MissingFile(t *testing.T) {
	_, err := Read("/nonexistent/path/q3now.json")
	if err == nil {
		t.Fatal("expected error for missing file, got nil")
	}
}

func TestRead_CorruptJSON(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "q3now.json")

	if err := os.WriteFile(path, []byte("not valid json{{{"), 0644); err != nil {
		t.Fatalf("failed to write test file: %v", err)
	}

	_, err := Read(path)
	if err == nil {
		t.Fatal("expected error for corrupt JSON, got nil")
	}
}
