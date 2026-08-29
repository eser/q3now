package main

import (
	"flag"
	"fmt"
	"strings"

	"github.com/eser/q3now/tools/sw3z-archiver/lifecycle"
)

type stringList []string

func (values *stringList) String() string { return strings.Join(*values, ",") }
func (values *stringList) Set(value string) error {
	*values = append(*values, value)
	return nil
}

func splitList(value string) []string {
	var out []string
	for _, item := range strings.Split(value, ",") {
		if item = strings.TrimSpace(item); item != "" {
			out = append(out, item)
		}
	}
	return out
}

func manifestCommand(args []string) error {
	if len(args) == 0 {
		return fmt.Errorf("usage: sw3z manifest <create|validate> ...")
	}
	switch args[0] {
	case "create":
		return manifestCreate(args[1:])
	case "validate":
		return manifestValidate(args[1:])
	default:
		return fmt.Errorf("unknown manifest subcommand %q", args[0])
	}
}

func manifestCreate(args []string) error {
	flags := flag.NewFlagSet("sw3z manifest create", flag.ContinueOnError)
	id := flags.String("id", "", "package identity")
	version := flags.String("version", "", "package version")
	owner := flags.String("owner", "", "package owner")
	role := flags.String("role", "", "runtime|toolchain|server|client|shared|cosmetic")
	osTargets := flags.String("os", "any", "comma-separated operating-system targets")
	archTargets := flags.String("arch", "any", "comma-separated architecture targets")
	abi := flags.String("abi", "", "compatibility ABI")
	engineMin := flags.String("engine-min", "", "minimum engine version")
	engineMax := flags.String("engine-max", "", "maximum engine version")
	root := flags.String("root", "", "staging directory to inventory")
	out := flags.String("out", "", "output manifest path")
	var provides stringList
	var depends stringList
	var files stringList
	flags.Var(&provides, "provides", "provided capability (repeatable)")
	flags.Var(&depends, "depends", "required dependency id (repeatable)")
	flags.Var(&files, "file", "install-path=source-path physical artifact mapping (repeatable)")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if *out == "" || (*root == "" && len(files) == 0) || (*root != "" && len(files) != 0) {
		return fmt.Errorf("--out and exactly one of --root or --file are required")
	}
	metadata := lifecycle.Manifest{
		ID:       *id,
		Version:  *version,
		Owner:    *owner,
		Role:     lifecycle.Role(*role),
		Provides: provides,
		Compatibility: lifecycle.Compatibility{
			OS: splitList(*osTargets), Arch: splitList(*archTargets), ABI: *abi,
			EngineMin: *engineMin, EngineMax: *engineMax,
		},
	}
	for _, dependency := range depends {
		metadata.Depends = append(metadata.Depends, lifecycle.Dependency{ID: dependency})
	}
	var manifest *lifecycle.Manifest
	var err error
	if *root != "" {
		manifest, err = lifecycle.Generate(*root, metadata)
	} else {
		inputs := make([]lifecycle.InputFile, 0, len(files))
		for _, specification := range files {
			parts := strings.SplitN(specification, "=", 2)
			if len(parts) != 2 || parts[0] == "" || parts[1] == "" {
				return fmt.Errorf("invalid --file %q; expected install-path=source-path", specification)
			}
			inputs = append(inputs, lifecycle.InputFile{InstallPath: parts[0], SourcePath: parts[1]})
		}
		manifest, err = lifecycle.GenerateFiles(inputs, metadata)
	}
	if err != nil {
		return err
	}
	if err := lifecycle.Save(*out, manifest); err != nil {
		return err
	}
	fmt.Printf("Created %s (%s %s, %d files)\n", *out, manifest.ID, manifest.Version, len(manifest.Files))
	return nil
}

func manifestValidate(args []string) error {
	if len(args) == 0 {
		return fmt.Errorf("usage: sw3z manifest validate <manifest.json>...")
	}
	manifests := make([]lifecycle.Manifest, 0, len(args))
	for _, path := range args {
		manifest, err := lifecycle.Load(path)
		if err != nil {
			return fmt.Errorf("%s: %w", path, err)
		}
		manifests = append(manifests, *manifest)
	}
	if err := lifecycle.ValidateSet(manifests); err != nil {
		return err
	}
	fmt.Printf("SW3Z package manifest set valid (%d package(s))\n", len(manifests))
	return nil
}
