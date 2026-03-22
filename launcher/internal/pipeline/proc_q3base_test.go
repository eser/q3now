package pipeline

import "testing"

func TestQ3BaseProcessor_AllowedFiles(t *testing.T) {
	proc := &Q3BaseProcessor{}

	cases := []string{
		"botfiles/bots/anarki_c.c",
		"botfiles/chars.h",
		"env/space1_bk.jpg",
		"gfx/2d/bigchars.tga",
		"gfx/damage/blood_screen.tga",
		"icons/quad.tga",
		"levelshots/Q3DM1.jpg",
		"maps/q3dm1.bsp",
		"maps/q3dm1.aas",
		"menu/art/back_0.tga",
		"models/players/sarge/animation.cfg",
		"models/weapons2/railgun/railgun.md3",
		"music/fla22k_01_intro.wav",
		"scripts/base.shader",
		"scripts/arenas.txt",
		"sound/feedback/fight.wav",
		"sprites/balloon4.tga",
		"textures/base_wall/basewall01.jpg",
		"ffa.config",
		"teamplay.config",
		"tourney.config",
		"default.cfg",
	}

	for _, path := range cases {
		entry := AssetEntry{Origin: "q3_base", Path: path}
		decision, err := proc.Process(entry, nil)
		if err != nil {
			t.Fatalf("Process(%q): %v", path, err)
		}
		if decision.Action != Include {
			t.Errorf("Process(%q): expected Include, got Skip", path)
		}
	}
}

func TestQ3BaseProcessor_SkipDisallowed(t *testing.T) {
	proc := &Q3BaseProcessor{}

	skipped := []string{
		"vm/cgame.qvm",
		"vm/qagame.qvm",
		"vm/ui.qvm",
		"video/idlogo.roq",
		"q3config.cfg",
		"unknown/stuff.dat",
		"readme.txt",
		"player/anarki", // directory entry, no extension
	}

	for _, path := range skipped {
		entry := AssetEntry{Origin: "q3_base", Path: path}
		decision, err := proc.Process(entry, nil)
		if err != nil {
			t.Fatalf("Process(%q): %v", path, err)
		}
		if decision.Action != Skip {
			t.Errorf("Process(%q): expected Skip, got Include", path)
		}
	}
}

func TestQ3BaseProcessor_SkipWrongOrigin(t *testing.T) {
	proc := &Q3BaseProcessor{}

	entry := AssetEntry{Origin: "q3_missionpack", Path: "maps/q3dm1.bsp"}
	decision, err := proc.Process(entry, nil)
	if err != nil {
		t.Fatalf("Process: %v", err)
	}
	if decision.Action != Skip {
		t.Errorf("wrong origin: expected Skip, got Include")
	}
}

func TestQ3BaseProcessor_CaseInsensitive(t *testing.T) {
	proc := &Q3BaseProcessor{}

	cases := []string{
		"Maps/Q3DM1.bsp",
		"TEXTURES/Gothic_Block/blocks18b.jpg",
		"Levelshots/Q3DM1.jpg",
		"FFA.config",
		"TOURNEY.CONFIG",
		"DEFAULT.CFG",
	}

	for _, path := range cases {
		entry := AssetEntry{Origin: "q3_base", Path: path}
		decision, err := proc.Process(entry, nil)
		if err != nil {
			t.Fatalf("Process(%q): %v", path, err)
		}
		if decision.Action != Include {
			t.Errorf("Process(%q): expected Include (case-insensitive), got Skip", path)
		}
	}
}

func TestQ3BaseProcessor_Finalize(t *testing.T) {
	proc := &Q3BaseProcessor{}
	entries, err := proc.Finalize()
	if err != nil {
		t.Fatalf("Finalize: %v", err)
	}
	if len(entries) != 0 {
		t.Errorf("expected 0 synthetic entries, got %d", len(entries))
	}
}

func TestQ3BaseProcessor_AllowlistSize(t *testing.T) {
	// Sanity check: the allowlist should have a reasonable number of entries.
	if len(q3BaseAllowlist) < 3000 {
		t.Errorf("allowlist too small: %d entries (expected 3000+)", len(q3BaseAllowlist))
	}
}
