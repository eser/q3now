package subcommands

import (
	"fmt"

	"github.com/eser/q3now/tools/sw3z-archiver/lifecycle"
	"github.com/spf13/cobra"
)

var packagesCmd = &cobra.Command{
	Use:   "packages",
	Short: "Validate and dry-run SW3Z package lifecycle operations",
}

func loadPackageManifests(paths []string) ([]lifecycle.Manifest, error) {
	manifests := make([]lifecycle.Manifest, 0, len(paths))
	for _, path := range paths {
		manifest, err := lifecycle.Load(path)
		if err != nil {
			return nil, fmt.Errorf("%s: %w", path, err)
		}
		manifests = append(manifests, *manifest)
	}
	return manifests, nil
}

var packagesValidateCmd = &cobra.Command{
	Use:   "validate <manifest.json>...",
	Short: "Validate package identity, ownership and dependency graph",
	Args:  cobra.MinimumNArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		manifests, err := loadPackageManifests(args)
		if err != nil {
			return err
		}
		if err := lifecycle.ValidateSet(manifests); err != nil {
			return err
		}
		fmt.Fprintf(cmd.OutOrStdout(), "SW3Z package manifest set valid (%d package(s))\n", len(manifests))
		return nil
	},
}

var (
	packagePlanOperation string
	packagePlanRoot      string
	packagePlanInstalled []string
	packagePlanManifest  string
	packagePlanID        string
)

var packagesPlanCmd = &cobra.Command{
	Use:   "plan",
	Short: "Print a non-mutating install, update or remove plan as JSON",
	RunE: func(cmd *cobra.Command, _ []string) error {
		if packagePlanRoot == "" {
			return fmt.Errorf("--root is required")
		}
		installed, err := loadPackageManifests(packagePlanInstalled)
		if err != nil {
			return err
		}
		var incoming *lifecycle.Manifest
		if packagePlanManifest != "" {
			incoming, err = lifecycle.Load(packagePlanManifest)
			if err != nil {
				return err
			}
		}
		plan, err := lifecycle.BuildPlan(packagePlanRoot, lifecycle.Operation(packagePlanOperation), installed, incoming, packagePlanID)
		if err != nil {
			return err
		}
		data, err := plan.JSON()
		if err != nil {
			return err
		}
		_, err = fmt.Fprintln(cmd.OutOrStdout(), string(data))
		return err
	},
}

func init() {
	rootCmd.AddCommand(packagesCmd)
	packagesCmd.AddCommand(packagesValidateCmd, packagesPlanCmd)
	packagesPlanCmd.Flags().StringVar(&packagePlanOperation, "operation", "", "install, update or remove")
	packagesPlanCmd.Flags().StringVar(&packagePlanRoot, "root", "", "installation root to inspect")
	packagesPlanCmd.Flags().StringSliceVar(&packagePlanInstalled, "installed", nil, "installed manifest path (repeatable or comma-separated)")
	packagesPlanCmd.Flags().StringVar(&packagePlanManifest, "manifest", "", "incoming manifest for install/update")
	packagesPlanCmd.Flags().StringVar(&packagePlanID, "package", "", "installed package id for remove")
	_ = packagesPlanCmd.MarkFlagRequired("operation")
}
