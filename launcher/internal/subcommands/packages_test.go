package subcommands

import (
	"errors"
	"path/filepath"
	"testing"

	"github.com/eser/q3now/tools/sw3z-archiver/lifecycle"
)

func TestLauncherUsesCanonicalLifecycleValidator(t *testing.T) {
	path := filepath.Join(t.TempDir(), "package.json")
	manifest := lifecycle.Manifest{
		Schema:  lifecycle.SchemaV1,
		ID:      "wired.test",
		Version: "1",
		Role:    lifecycle.RoleRuntime,
		Compatibility: lifecycle.Compatibility{
			OS: []string{"any"}, Arch: []string{"any"}, ABI: "test",
		},
	}
	// Save must reject this through the same missing-owner diagnostic used by
	// the release CLI; use direct JSON-free proof of the shared error type.
	err := lifecycle.Save(path, &manifest)
	var validation *lifecycle.ValidationError
	if !errors.As(err, &validation) || len(validation.Diagnostics) == 0 || validation.Diagnostics[0].Code != "missing_owner" {
		t.Fatalf("launcher did not receive canonical validator diagnostic: %v", err)
	}
}
