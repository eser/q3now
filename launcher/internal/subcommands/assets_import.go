package subcommands

import (
	"fmt"

	"github.com/eser/q3now/launcher/internal/config"
	"github.com/eser/q3now/launcher/internal/pipeline"
	"github.com/spf13/cobra"
)

var assetsImportCmd = &cobra.Command{
	Use:   "import",
	Short: "Import downloaded assets (and optionally a full Q3 install) into baseq3/",
	Long: `Process the downloaded id-quakepack bundle through the q3copy pipeline,
producing pax01.sw3z in ~/q3now${channelSuffix}/baseq3/.

When --q3path points at a full Quake III Arena install root (a directory
containing baseq3/pak0.pk3), the pipeline also produces pax02.sw3z from
the full baseq3 PAKs and pax04.sw3z from missionpack/ if present.

Prerequisite: the bundle must be downloaded first (` + "`q3now assets download`" + `).
Asset conversion (TGA→PNG, WAV→Opus, SW3Z repack) runs synchronously and
takes several minutes depending on disk speed.

EULA: same gate as ` + "`q3now assets download`" + `.`,
	RunE: func(cmd *cobra.Command, args []string) error {
		paths, err := config.ResolvePaths()
		if err != nil {
			return fmt.Errorf("resolving paths: %w", err)
		}

		acceptEula, _ := cmd.Flags().GetBool("accept-eula")
		if err := requireEulaAccepted(paths, acceptEula); err != nil {
			return err
		}

		q3path, _ := cmd.Flags().GetString("q3path")

		reporter := NewStdoutReporter("import")
		return pipeline.RunImport(cmd.Context(), paths,
			pipeline.ImportOpts{Q3Path: q3path}, reporter)
	},
}

func init() {
	assetsImportCmd.Flags().String("q3path", "",
		"Path to a full Quake III Arena install root (containing baseq3/pak0.pk3). "+
			"When set, pax02 and pax04 are produced alongside pax01. Optional.")
	assetsImportCmd.Flags().Bool("accept-eula", false,
		"Record acceptance of the id Software demo EULA. Required on first run.")
	assetsCmd.AddCommand(assetsImportCmd)
}
