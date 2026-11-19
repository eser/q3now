package pipeline

import (
	"context"
	"strings"
	"testing"

	"github.com/eser/q3now/launcher/internal/config"
)

func TestRunImportRejectsCookedMaterialsWithoutLicensedCorpus(t *testing.T) {
	err := RunImport(context.Background(), &config.Paths{}, ImportOpts{
		MaterialCookDir: t.TempDir(),
	}, NopReporter{})
	if err == nil || !strings.Contains(err.Error(), "requires a full Q3 import") {
		t.Fatalf("got %v, want licensed-corpus boundary failure", err)
	}
}
