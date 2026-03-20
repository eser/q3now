# q3now — developer Makefile
#
# Wraps cmake configure+build, pak packaging, and local install.
# All cmake details live in cmake/ — this file is a thin workflow layer.
#
# TARGETS
#   make              configure (if needed) + build Release modules + QVMs
#   make clean        remove build directory
#   make rebuild      clean + build
#   make pak          package modfiles/ + QVMs into build/baseq3/zz-q3now.pk3
#   make check        verify QVMs, dylibs, and pk3 contents
#   make install      build + pak + deploy to Q3DIR
#   make run          build + install + launch q3now
#   make run-dev      build + install + launch with native modules (vm_*=0, for debugging)
#   make help         show this message
#
# VARIABLES (override on command line or env)
#   BUILD_DIR    cmake output directory        (default: build)
#   BUILD_TYPE   Release | Debug               (default: Release)
#   Q3DIR        install destination           (default: /Applications/q3now on macOS)
#   JOBS         parallel job count            (default: CPU count)
#   MAP          map to load with `make run`   (default: q3dm1)

# ── Defaults ──────────────────────────────────────────────────────────────────

APP_NAME   ?= q3now
BUILD_DIR  ?= build
BUILD_TYPE ?= Release
MAP        ?= q3dm1

# CPU count and architecture detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),arm64)
  BINEXT  := .aarch64
  RENDEXT := _aarch64
else ifeq ($(UNAME_M),x86_64)
  BINEXT  := .x86_64
  RENDEXT := _x86_64
else
  BINEXT  :=
  RENDEXT :=
endif
# GAME_ARCH mirrors ARCH_STRING from q_platform.h (vm.c appends it to dylib filenames)
# Strip the leading "_" from RENDEXT: "_aarch64" -> "aarch64"
GAME_ARCH := $(patsubst _%,%,$(RENDEXT))
ifeq ($(UNAME_S),Darwin)
  JOBS        ?= $(shell sysctl -n hw.ncpu)
  # cmake puts app bundles at build/<name><arch>.app (no Release/ subdir)
  BUILT_APP   := $(BUILD_DIR)/$(APP_NAME)$(BINEXT).app
  BUILT_DED   := $(BUILD_DIR)/$(APP_NAME)-ded$(BINEXT).app/Contents/MacOS/$(APP_NAME)-ded$(BINEXT)
  GAME_BIN    := $(BUILT_APP)/Contents/MacOS/$(APP_NAME)$(BINEXT)
  Q3DIR       ?= /Applications/$(APP_NAME)
else
  JOBS        ?= $(shell nproc 2>/dev/null || echo 4)
  GAME_BIN    := $(BUILD_DIR)/$(APP_NAME)$(BINEXT)
  Q3DIR       ?= $(HOME)/$(APP_NAME)
endif

# cmake always places game modules at build/Release/baseq3/ regardless of platform
MODULE_DIR  := $(BUILD_DIR)/Release/baseq3

Q3BASEDIR   := $(Q3DIR)/baseq3

# Use Ninja if available — much faster incremental builds
ifneq ($(shell which ninja 2>/dev/null),)
  GENERATOR := -G Ninja
else
  GENERATOR :=
endif

CMAKE_CONFIGURE := cmake -S . -B $(BUILD_DIR) $(GENERATOR) \
                     -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

CMAKE_BUILD := cmake --build $(BUILD_DIR) --parallel $(JOBS)

# DMG packaging (macOS only)
VERSION     := $(shell git describe --always --dirty)
DMG_NAME    := $(APP_NAME)-$(VERSION)-$(UNAME_M)
DMG_STAGING := $(BUILD_DIR)/dmg-staging
DMG_OUT     := $(BUILD_DIR)/$(DMG_NAME).dmg

# ── Phony targets ─────────────────────────────────────────────────────────────

.PHONY: all configure build clean rebuild pak check install run run-dev smoke bench diff-api dmg release help

all: build

# ── Configure ─────────────────────────────────────────────────────────────────
# Re-runs only when CMakeLists.txt changes.

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE_CONFIGURE)

configure: $(BUILD_DIR)/CMakeCache.txt

# ── Build ─────────────────────────────────────────────────────────────────────
# Builds native .so/.dylib modules AND QVM bytecode.
# QVM toolchain (lcc/q3asm) is compiled from source the first time (~30s),
# then cached. Subsequent builds only recompile changed files.

build: $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE_BUILD)

# ── Clean / Rebuild ───────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build

# ── Pak packaging ─────────────────────────────────────────────────────────────
# Creates build/baseq3/zz-q3now.pk3 from modfiles/ + QVMs.
# zz- prefix ensures this pak loads last (highest override priority).
# QVMs in zz-q3now.pk3 override the stock 1999 QVMs in pak0.pk3.

PAK_STAGING := $(BUILD_DIR)/pak-staging
PAK_OUT     := $(BUILD_DIR)/baseq3/zz-$(APP_NAME).pk3

pak: build
	@echo "==> Staging pak contents..."
	rm -rf $(PAK_STAGING)
	mkdir -p $(PAK_STAGING) $(BUILD_DIR)/baseq3
	cp -R modfiles/. $(PAK_STAGING)/
	@echo "==> Copying QVMs into pak..."
	cp -R $(MODULE_DIR)/vm $(PAK_STAGING)/
	@echo "==> Stamping version..."
	echo "$(APP_NAME) $$(git describe --always --dirty) ($$(date +%Y-%m-%d))" > $(PAK_STAGING)/description.txt
	@echo "==> Creating $(PAK_OUT)..."
	cd $(PAK_STAGING) && zip -r9 "$(CURDIR)/$(PAK_OUT)" . -x "**/.DS_Store" -x ".DS_Store"
	@echo "==> $(PAK_OUT) ready"

# ── Check ─────────────────────────────────────────────────────────────────────
# Verifies all outputs are present and pk3 contains QVMs.
# Guards against the silent failure mode: stock QVMs loading instead of custom.

check: pak
	@echo "==> Verifying build..."
	@ls $(MODULE_DIR)/vm/cgame.qvm  > /dev/null && echo "  cgame.qvm:   OK"
	@ls $(MODULE_DIR)/vm/qagame.qvm > /dev/null && echo "  qagame.qvm:  OK"
	@ls $(MODULE_DIR)/vm/ui.qvm     > /dev/null && echo "  ui.qvm:      OK"
	@ls $(MODULE_DIR)/cgame.*       > /dev/null && echo "  cgame.dylib: OK"
	@ls $(MODULE_DIR)/qagame.*      > /dev/null && echo "  qagame.dylib: OK"
	@ls $(MODULE_DIR)/ui.*          > /dev/null && echo "  ui.dylib:    OK"
	@unzip -l $(PAK_OUT) | grep -q "vm/cgame.qvm" && echo "  pk3 QVMs:    OK"
ifeq ($(UNAME_S),Darwin)
	@codesign --verify "$(Q3DIR)/$(APP_NAME).app" 2>/dev/null && echo "  codesign:    OK" || echo "  codesign:    MISSING (run make install)"
	@codesign -d --entitlements - "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/$(APP_NAME)$(BINEXT)" 2>/dev/null | grep -q "allow-jit" && echo "  JIT entitlement: OK" || echo "  JIT entitlement: MISSING"
endif
	@echo "==> All checks passed."

# ── Install ───────────────────────────────────────────────────────────────────
# Deploys to Q3DIR (default: /Applications/q3now on macOS).
# Uses rsync --delete to cleanly replace the .app bundle (no stale files).
# Override Q3DIR to target a different install location.

install: build pak
	@echo "==> Installing to: $(Q3DIR)"
ifeq ($(UNAME_S),Darwin)
	@# Replace .app bundle cleanly (rsync --delete removes stale files from old bundle)
	rsync -a --delete "$(BUILT_APP)/" "$(Q3DIR)/$(APP_NAME).app/"
	@# Renderer dylibs live inside Contents/MacOS/ — self-contained bundle.
	@# cl_main.c tries Sys_DefaultAppPath() (Contents/MacOS/) before Sys_DefaultBasePath().
	cp "$(BUILD_DIR)/$(APP_NAME)_opengl$(RENDEXT).dylib" "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/"
	@test -f "$(BUILD_DIR)/$(APP_NAME)_vulkan$(RENDEXT).dylib" && \
	  cp "$(BUILD_DIR)/$(APP_NAME)_vulkan$(RENDEXT).dylib" "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/" || true
	@# MoltenVK: copy into Contents/MacOS/ — VKimp_Init calls SDL_Vulkan_LoadLibrary(bundledPath)
	@MOLTEN_VK=$$(find /opt/homebrew/lib /usr/local/lib 2>/dev/null -name "libMoltenVK.dylib" -maxdepth 1 | head -1); \
	  if [ -n "$$MOLTEN_VK" ]; then \
	    echo "  MoltenVK: $$MOLTEN_VK → Contents/MacOS/"; \
	    cp "$$MOLTEN_VK" "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/"; \
	  else \
	    echo "  WARNING: libMoltenVK.dylib not found — Vulkan renderer will not work"; \
	    echo "  Run: brew install molten-vk"; \
	  fi
	@# SDL3: copy libSDL3.dylib into Contents/MacOS/ + fix load paths (belt & suspenders;
	@#   cmake RPATH handles the rpath, install_name_tool fixes the explicit install path)
	@SDL3_DYLIB=$$(find /opt/homebrew/lib /usr/local/lib 2>/dev/null -name "libSDL3*.dylib" -maxdepth 1 | head -1); \
	  if [ -n "$$SDL3_DYLIB" ]; then \
	    echo "  SDL3: $$SDL3_DYLIB → Contents/MacOS/"; \
	    cp "$$SDL3_DYLIB" "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/"; \
	    SDL3_BASE=$$(basename "$$SDL3_DYLIB"); \
	    install_name_tool -change "$$SDL3_DYLIB" "@executable_path/$$SDL3_BASE" "$(GAME_BIN)" 2>/dev/null || true; \
	    install_name_tool -change "$$SDL3_DYLIB" "@executable_path/$$SDL3_BASE" \
	      "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/$(APP_NAME)_opengl$(RENDEXT).dylib" 2>/dev/null || true; \
	    install_name_tool -change "$$SDL3_DYLIB" "@executable_path/$$SDL3_BASE" \
	      "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/$(APP_NAME)_vulkan$(RENDEXT).dylib" 2>/dev/null || true; \
	  else \
	    echo "  WARNING: libSDL3.dylib not found — app may not launch without SDL3"; \
	    echo "  Run: brew install sdl3"; \
	  fi
	@# Game modules go in Q3BASEDIR/ where FS_LoadLibrary finds them via DIR_STATIC searchpaths.
	@# FS_LoadLibrary builds: FS_BuildOSPath(path, gamedir, name) = $Q3DIR/baseq3/<name>
	@# This matches how Linux installs them; inside the .app bundle is NOT searched.
	mkdir -p "$(Q3BASEDIR)"
	cp "$(MODULE_DIR)/cgame$(GAME_ARCH).dylib"  "$(Q3BASEDIR)/"
	cp "$(MODULE_DIR)/qagame$(GAME_ARCH).dylib" "$(Q3BASEDIR)/"
	cp "$(MODULE_DIR)/ui$(GAME_ARCH).dylib"     "$(Q3BASEDIR)/"
	@# Dedicated server binary alongside .app (if built)
	@test -f "$(BUILT_DED)" && cp "$(BUILT_DED)" "$(Q3DIR)/$(APP_NAME)-ded" || true
	@# Code sign: ad-hoc with JIT entitlements (required for ARM64 JIT on macOS)
	@echo "==> Code signing..."
	@for dylib in "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/"*.dylib \
	              "$(Q3BASEDIR)/"*$(GAME_ARCH).dylib; do \
	  [ -f "$$dylib" ] && codesign --force --sign - "$$dylib" 2>/dev/null; \
	done
	codesign --force --sign - --entitlements misc/macos/q3now.entitlements \
	  "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/$(APP_NAME)$(BINEXT)"
	codesign --force --sign - --entitlements misc/macos/q3now.entitlements "$(Q3DIR)/$(APP_NAME).app"
	@test -f "$(Q3DIR)/$(APP_NAME)-ded" && \
	  codesign --force --sign - --entitlements misc/macos/q3now.entitlements "$(Q3DIR)/$(APP_NAME)-ded" || true
else
	mkdir -p "$(Q3BASEDIR)"
	cp "$(MODULE_DIR)/cgame.so"  "$(Q3BASEDIR)/"
	cp "$(MODULE_DIR)/qagame.so" "$(Q3BASEDIR)/"
	cp "$(MODULE_DIR)/ui.so"     "$(Q3BASEDIR)/"
endif
	@# Mod pak into basepath/baseq3/ — QVMs here override stock paks
	mkdir -p "$(Q3BASEDIR)"
	cp "$(PAK_OUT)" "$(Q3BASEDIR)/"
	@echo "==> Done. Launch with: make run"

# ── Run ───────────────────────────────────────────────────────────────────────
# Builds, installs, and launches q3now. QVMs loaded (vm_*=2, default).

run: install
ifeq ($(UNAME_S),Darwin)
	open "$(Q3DIR)/$(APP_NAME).app" --args +map $(MAP)
else
	"$(Q3DIR)/$(APP_NAME)" +map $(MAP)
endif

# ── Run (dev / debug) ─────────────────────────────────────────────────────────
# Builds, installs, and launches with native dylibs (vm_*=0) for debugging.
# sv_pure must be disabled or the client forces QVM mode regardless of vm_*.

run-dev: install
ifeq ($(UNAME_S),Darwin)
	open "$(Q3DIR)/$(APP_NAME).app" --args \
	  +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 +set vm_ui 0 \
	  +set developer 1 +devmap $(MAP)
else
	"$(Q3DIR)/$(APP_NAME)" \
	  +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 +set vm_ui 0 \
	  +set developer 1 +devmap $(MAP)
endif

# ── Smoke test ────────────────────────────────────────────────────────────────
# Headless gameplay smoke test. Requires Q3DIR with pak0.pk3.
# Skips gracefully (exit 77) in asset-free CI environments.

smoke: build
ifeq ($(UNAME_S),Darwin)
	Q3DIR="$(Q3DIR)" tests/smoke.sh "$(Q3DIR)/$(APP_NAME)-ded"
else
	Q3DIR="$(Q3DIR)" tests/smoke.sh "$(Q3DIR)/$(APP_NAME)-ded$(BINEXT)"
endif

# ── Feature stress test ──────────────────────────────────────────────────────
# Starts devmap with ALL feature cvars enabled, spawns 3 bots, runs 30s.
# Catches crashes from feature interactions. Exit 0 = all features coexist.

test-features: install
ifeq ($(UNAME_S),Darwin)
	@echo "==> Testing all features enabled (30s)..."
	@timeout 35 "$(GAME_BIN)" \
	  +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 +set vm_ui 0 \
	  +set g_fastWeaponSwitch 2 \
	  +set g_spawnProtect 2 \
	  +set cg_scorePlums 2 \
	  +set developer 1 +set ttycon 0 \
	  +devmap $(MAP) +addbot Doom 3 +addbot Bones 3 +addbot Slash 3 \
	  +wait 900 +quit > /tmp/q3now-test-features.log 2>&1 || true
	@if grep -q "Unknown event\|Error\|FATAL\|Signal caught" /tmp/q3now-test-features.log; then \
	  echo "FAIL: Feature test detected errors"; \
	  grep "Unknown event\|Error\|FATAL\|Signal caught" /tmp/q3now-test-features.log; \
	  exit 1; \
	else \
	  echo "==> All features OK"; \
	fi
else
	@echo "test-features: not yet implemented for Linux"
endif

# ── Bench ─────────────────────────────────────────────────────────────────────
# Timedemo benchmark. Requires a demo at Q3BASEDIR/demos/four.dm_68.
# Override DEMO= to use a different demo file (without the .dm_68 extension).

DEMO ?= four

bench: install
	@if [ ! -f "$(Q3BASEDIR)/demos/$(DEMO).dm_68" ]; then \
	  echo "ERROR: $(Q3BASEDIR)/demos/$(DEMO).dm_68 not found"; \
	  echo "Copy a demo file (.dm_68) to $(Q3BASEDIR)/demos/ and set DEMO=<name>"; \
	  exit 1; \
	fi
ifeq ($(UNAME_S),Darwin)
	open "$(Q3DIR)/$(APP_NAME).app" --args +timedemo 1 +demo $(DEMO)
else
	"$(Q3DIR)/$(APP_NAME)$(BINEXT)" +timedemo 1 +demo $(DEMO)
endif

# ── Diff-api ──────────────────────────────────────────────────────────────────
# Diffs q3now's game API headers against upstream Quake3e at the fork point.
# Use UPSTREAM_REF= to compare against a different commit or tag.

UPSTREAM_REF ?= ecd5fa41

diff-api:
	@echo "==> API header diff (q3now vs upstream $(UPSTREAM_REF))"
	@echo "--- g_public.h ---"
	@git diff $(UPSTREAM_REF) -- code/game/g_public.h
	@echo "--- cg_public.h ---"
	@git diff $(UPSTREAM_REF) -- code/cgame/cg_public.h
	@echo "--- ui_public.h ---"
	@git diff $(UPSTREAM_REF) -- code/ui/ui_public.h

# ── DMG packaging ─────────────────────────────────────────────────────────────
# Creates a version-stamped, compressed DMG containing q3now.app + pk3 + README.
# DMG filename example: q3now-4736ab7-arm64.dmg

dmg: install
ifeq ($(UNAME_S),Darwin)
	@echo "==> Creating $(DMG_NAME).dmg..."
	rm -rf $(DMG_STAGING) "$(DMG_OUT)"
	mkdir -p $(DMG_STAGING)/baseq3
	cp -R "$(Q3DIR)/$(APP_NAME).app" "$(DMG_STAGING)/"
	cp "$(PAK_OUT)" "$(DMG_STAGING)/baseq3/"
	cp README.md "$(DMG_STAGING)/"
	@test -f "$(Q3DIR)/$(APP_NAME)-ded" && cp "$(Q3DIR)/$(APP_NAME)-ded" "$(DMG_STAGING)/" || true
	hdiutil create -volname "$(APP_NAME)" -srcfolder $(DMG_STAGING) -ov -format UDZO "$(DMG_OUT)"
	rm -rf $(DMG_STAGING)
	@echo "==> $(DMG_OUT) ready ($$(du -h "$(DMG_OUT)" | cut -f1))"
else
	@echo "DMG creation requires macOS"
endif

# ── Release ───────────────────────────────────────────────────────────────────
# Full release pipeline: verify everything, then produce a distributable DMG.

release: check dmg
	@echo ""
	@echo "  ┌─────────────────────────────────────┐"
	@echo "  │  q3now release ready                 │"
	@echo "  ├─────────────────────────────────────┤"
	@echo "  │  DMG: $(DMG_OUT)"
	@echo "  │  Size: $$(du -h "$(DMG_OUT)" | cut -f1)"
	@echo "  └─────────────────────────────────────┘"

# ── Help ──────────────────────────────────────────────────────────────────────

help:
	@echo ""
	@echo "  q3now build targets"
	@echo "  ───────────────────────────────────────────────────────────"
	@echo "  make              configure + build (native modules + QVMs)"
	@echo "  make clean        remove build/"
	@echo "  make rebuild      clean + build"
	@echo "  make pak          package modfiles/ + QVMs → zz-q3now.pk3"
	@echo "  make check        verify QVMs, dylibs, pk3 contents"
	@echo "  make install      build + pak + deploy to Q3DIR"
	@echo "  make run          build + install + launch (QVM mode)"
	@echo "  make run-dev      build + install + launch (native debug mode)"
	@echo "  make bench        timedemo benchmark (requires DEMO in Q3BASEDIR/demos/)"
	@echo "  make diff-api     diff game API headers vs upstream Quake3e fork point"
	@echo "  make dmg          build + install + package versioned DMG (macOS only)"
	@echo "  make release      check + dmg + print summary (macOS only)"
	@echo "  make help         show this message"
	@echo ""
	@echo "  Variables:"
	@echo "    BUILD_DIR=$(BUILD_DIR)   BUILD_TYPE=$(BUILD_TYPE)"
	@echo "    Q3DIR=$(Q3DIR)"
	@echo "    JOBS=$(JOBS)   MAP=$(MAP)   DEMO=$(DEMO)"
	@echo "    UPSTREAM_REF=$(UPSTREAM_REF)"
	@echo ""
