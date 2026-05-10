package subcommands

import (
	"fmt"

	"github.com/eser/q3now/launcher/internal/config"
	"github.com/eser/q3now/launcher/internal/pipeline"
	"github.com/spf13/cobra"
)

var assetsDownloadCmd = &cobra.Command{
	Use:   "download",
	Short: "Download the redistributable id-quakepack bundle",
	Long: `Download the redistributable Quake III demo + Q3TA demo bundle from the
canonical mirror. Output is cached at ~/wired${channelSuffix}/downloaded/
and unzipped to ~/wired${channelSuffix}/downloaded/id-quakepack/.

Re-running this command after a successful download is a no-op: the
cached zip is reused. Delete the cached zip to force re-download.

EULA: requires prior acceptance via either the GUI EULA screen or the
--accept-eula flag (which records the same acceptance timestamp).`,
	RunE: func(cmd *cobra.Command, args []string) error {
		paths, err := config.ResolvePaths()
		if err != nil {
			return fmt.Errorf("resolving paths: %w", err)
		}

		acceptEula, _ := cmd.Flags().GetBool("accept-eula")
		if err := requireEulaAccepted(paths, acceptEula); err != nil {
			return err
		}

		reporter := NewStdoutReporter("download")
		return pipeline.RunDownload(cmd.Context(), paths, pipeline.DownloadOpts{}, reporter)
	},
}

func init() {
	assetsDownloadCmd.Flags().Bool("accept-eula", false,
		"Record acceptance of the id Software demo EULA. Required on first run; "+
			"writes the same EulaAcceptedAt timestamp the GUI uses.")
	assetsCmd.AddCommand(assetsDownloadCmd)
}
