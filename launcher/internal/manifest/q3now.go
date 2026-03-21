package manifest

import (
	"encoding/json"
	"os"
	"time"
)

// Manifest is the q3now.json schema. It tracks which game content directories
// were found during import. Read by the launcher to decide whether to show the
// import or launch screen.
type Manifest struct {
	Version    int               `json:"version"`
	ImportedAt string            `json:"importedAt"`
	Inventory  map[string]string `json:"inventory"`
}

// AssetFile represents a single imported pk3 file (used by pipeline internally).
type AssetFile struct {
	Name     string `json:"name"`
	Size     int64  `json:"size"`
	Checksum string `json:"checksum"`
}

// PipelineResult is passed from the pipeline to create a manifest.
type PipelineResult struct {
	SourcePath string
	SourceType string
	BaseQ3     []AssetFile
	Missionpack []AssetFile
}

// Read loads and validates q3now.json from the given path.
// Returns an error if the file is missing, corrupt, or has an unexpected shape.
func Read(path string) (*Manifest, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	var m Manifest
	if err := json.Unmarshal(data, &m); err != nil {
		// Corrupt JSON — caller should delete and re-import.
		return nil, err
	}

	if m.Version == 0 || len(m.Inventory) == 0 {
		return nil, &InvalidManifestError{Reason: "missing version or empty inventory"}
	}

	return &m, nil
}

// WriteWithInventory creates a q3now.json from inventory scan results.
func WriteWithInventory(path string, inventory map[string]string) error {
	m := Manifest{
		Version:    1,
		ImportedAt: time.Now().UTC().Format(time.RFC3339),
		Inventory:  inventory,
	}
	return write(path, &m)
}

func write(path string, m *Manifest) error {
	data, err := json.MarshalIndent(m, "", "  ")
	if err != nil {
		return err
	}

	// Atomic write: write to temp, then rename.
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, data, 0644); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}

// InvalidManifestError indicates the manifest exists but is not valid.
type InvalidManifestError struct {
	Reason string
}

func (e *InvalidManifestError) Error() string {
	return "invalid manifest: " + e.Reason
}
