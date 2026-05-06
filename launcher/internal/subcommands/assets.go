package subcommands

import "github.com/spf13/cobra"

// assetsCmd is the parent for asset-management verbs (download, import).
// Has no RunE of its own — invoking `q3now assets` prints help.
var assetsCmd = &cobra.Command{
	Use:   "assets",
	Short: "Manage game asset downloads and imports",
	Long: `Asset operations: download the redistributable Quake III demo bundle
and import it (and optionally a full Q3 install) into q3now's content
directory. Future verbs may include verify, repack, etc.`,
}

func init() {
	rootCmd.AddCommand(assetsCmd)
}
