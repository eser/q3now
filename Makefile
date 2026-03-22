# q3now — developer Makefile
#
# Composable build workflow with granular targets.
# All cmake details live in cmake/ — this file is a thin workflow layer.
#
# GENERATION TARGETS
#   make create-launcher    build Go/Wails launcher binary
#   make create-paks        package modfiles/ + QVMs → pax02.pk3 (or .sw3z)
#
# COPY TARGETS (assemble .app at Q3DIR)
#   make copy-libs          copy renderer + dependency dylibs into .app
#   make copy-build         copy Release engine + game dylibs into .app
#   make copy-build-debug   copy Debug engine + game dylibs into .app
#   make copy-paks          copy mod pak into .app data directory
#
# BUNDLING TARGETS
#   make bundle-codesign    codesign the .app bundle (macOS)
#   make bundle-dmg         create versioned DMG (macOS)
#   make bundle-tar         create versioned tar.gz (Linux)
#
# FLOW TARGETS
#   make run-launcher       build + assemble + codesign + open launcher
#   make run-game           build + assemble + run engine (QVM mode)
#   make run-gamedev        build-debug + assemble + run engine (native debug)
#   make release            build + assemble + codesign + package
#
# VARIABLES (override on command line or env)
#   Q3DIR              install destination     (default: /Applications/q3now on macOS)
#   JOBS               parallel job count      (default: CPU count)
#   MAP                map to load             (default: q3dm1)
#   USE_SW3Z           archive format          (0=pk3/zip, 1=sw3z; default: 0)
#   CODESIGN_IDENTITY  signing identity        (default: - = ad-hoc)
#   UPSTREAM_REF       fork point for diff-api (default: ecd5fa41)

# ── Defaults ──────────────────────────────────────────────────────────────────

APP_NAME   ?= q3now
MAP        ?= q3dm1

# Dual build directories — avoid cmake reconfigure thrash between Release/Debug
BUILD_DIR_RELEASE := build-release
BUILD_DIR_DEBUG   := build-debug
BUILD_DIR         := $(BUILD_DIR_RELEASE)

# CPU count and architecture detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),arm64)
  BINEXT  := .arm64
  RENDEXT := _arm64
else ifeq ($(UNAME_M),aarch64)
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
GAME_ARCH := $(patsubst _%,%,$(RENDEXT))
ifeq ($(UNAME_S),Darwin)
  JOBS        ?= $(shell sysctl -n hw.ncpu)
  BUILT_APP_RELEASE := $(BUILD_DIR_RELEASE)/$(APP_NAME)$(BINEXT).app
  BUILT_APP_DEBUG   := $(BUILD_DIR_DEBUG)/$(APP_NAME)$(BINEXT).app
  GAME_BIN    := $(BUILT_APP_RELEASE)/Contents/MacOS/$(APP_NAME)$(BINEXT)
  BUILT_DED_RELEASE := $(BUILD_DIR_RELEASE)/$(APP_NAME)-ded$(BINEXT).app/Contents/MacOS/$(APP_NAME)-ded$(BINEXT)
  BUILT_DED_DEBUG   := $(BUILD_DIR_DEBUG)/$(APP_NAME)-ded$(BINEXT).app/Contents/MacOS/$(APP_NAME)-ded$(BINEXT)
  Q3DIR       ?= /Applications/$(APP_NAME).app
else
  JOBS        ?= $(shell nproc 2>/dev/null || echo 4)
  GAME_BIN    := $(BUILD_DIR_RELEASE)/$(APP_NAME)$(BINEXT)
  Q3DIR       ?= $(HOME)/$(APP_NAME)
endif

# cmake puts game modules at <build-dir>/Release/baseq3/ (or Debug/baseq3/)
MODULE_DIR_RELEASE := $(BUILD_DIR_RELEASE)/Release/baseq3
MODULE_DIR_DEBUG   := $(BUILD_DIR_DEBUG)/Debug/baseq3
MODULE_DIR         := $(MODULE_DIR_RELEASE)

ifeq ($(UNAME_S),Darwin)
  # macOS bundle conventions: code in Contents/MacOS/, data in Contents/Resources/
  Q3BASEDIR  := $(Q3DIR)/Contents/MacOS/baseq3
  Q3DATADIR  := $(Q3DIR)/Contents/Resources/baseq3
else
  Q3BASEDIR  := $(Q3DIR)/baseq3
  Q3DATADIR  := $(Q3DIR)/baseq3
endif

# Use Ninja if available — much faster incremental builds
ifneq ($(shell which ninja 2>/dev/null),)
  GENERATOR := -G Ninja
else
  GENERATOR :=
endif

CMAKE_CONFIGURE_RELEASE := cmake -S . -B $(BUILD_DIR_RELEASE) $(GENERATOR) -DCMAKE_BUILD_TYPE=Release
CMAKE_CONFIGURE_DEBUG   := cmake -S . -B $(BUILD_DIR_DEBUG) $(GENERATOR) -DCMAKE_BUILD_TYPE=Debug
CMAKE_BUILD_RELEASE     := cmake --build $(BUILD_DIR_RELEASE) --parallel $(JOBS)
CMAKE_BUILD_DEBUG       := cmake --build $(BUILD_DIR_DEBUG) --parallel $(JOBS)

# Code signing identity (default: ad-hoc).
CODESIGN_IDENTITY ?= -

# DMG packaging (macOS only)
VERSION     := $(shell git describe --always --dirty)
DMG_NAME    := $(APP_NAME)-$(VERSION)-$(UNAME_M)
DMG_STAGING := $(BUILD_DIR_RELEASE)/dmg-staging
DMG_OUT     := $(BUILD_DIR_RELEASE)/$(DMG_NAME).dmg

# tar.gz packaging (Linux)
TAR_NAME    := $(APP_NAME)-$(VERSION)-linux-$(UNAME_M)
TAR_STAGING := $(BUILD_DIR_RELEASE)/tar-staging
TAR_OUT     := $(BUILD_DIR_RELEASE)/$(TAR_NAME).tar.gz

# Launcher (Go/Wails)
LAUNCHER_DIR := launcher
LAUNCHER_BIN := $(LAUNCHER_DIR)/build/bin/q3now.app/Contents/MacOS/q3now-launcher

# SW3Z archiver
SW3Z_DIR := pkg/sw3z-archiver
SW3Z_BIN := $(SW3Z_DIR)/cmd/sw3z/sw3z

# Archive format toggle: 1 = sw3z, 0 = pk3 (zip)
USE_SW3Z ?= 1

# Pak output (always from Release build — QVMs are always Release)
PAK_STAGING := $(BUILD_DIR_RELEASE)/pak-staging
ifeq ($(USE_SW3Z),1)
  PAK_EXT := sw3z
else
  PAK_EXT := pk3
endif
PAK_OUT := $(BUILD_DIR_RELEASE)/baseq3/pax02.$(PAK_EXT)

# ── Phony targets ─────────────────────────────────────────────────────────────

.PHONY: all configure build build-debug clean rebuild \
        create-launcher create-paks \
        copy-libs copy-build copy-build-debug copy-paks copy-all copy-all-debug \
        bundle-codesign bundle-dmg bundle-tar \
        run-launcher run-game run-gamedev release \
        check smoke test-features bench diff-api help

all: build

# ══════════════════════════════════════════════════════════════════════════════
# FOUNDATION — configure, build, clean
# ══════════════════════════════════════════════════════════════════════════════

$(BUILD_DIR_RELEASE)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE_CONFIGURE_RELEASE)

$(BUILD_DIR_DEBUG)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE_CONFIGURE_DEBUG)

configure: $(BUILD_DIR_RELEASE)/CMakeCache.txt

build: $(BUILD_DIR_RELEASE)/CMakeCache.txt
	$(CMAKE_BUILD_RELEASE)

build-debug: $(BUILD_DIR_DEBUG)/CMakeCache.txt
	$(CMAKE_BUILD_DEBUG)

clean:
	rm -rf $(BUILD_DIR_RELEASE) $(BUILD_DIR_DEBUG)

rebuild: clean build

# ══════════════════════════════════════════════════════════════════════════════
# GENERATION TARGETS — produce artifacts
# ══════════════════════════════════════════════════════════════════════════════

# ── create-launcher ──────────────────────────────────────────────────────────
# Builds the Go/Wails launcher binary. Requires: go, node, wails CLI.

create-launcher:
	@echo "==> Building launcher..."
	cd $(LAUNCHER_DIR) && PATH="$$HOME/go/bin:$$PATH" wails build \
	  -ldflags "-X main.version=$(VERSION)"
	@echo "==> Launcher ready: $(LAUNCHER_BIN)"

# ── create-paks ──────────────────────────────────────────────────────────────
# Packages modfiles/ + QVMs into pax02.pk3 (or .sw3z when USE_SW3Z=1).
# "pax02" sorts after pak0–pak8, ensuring highest override priority.
# QVMs here override the stock 1999 QVMs in pak0.pk3.

$(SW3Z_BIN):
	cd $(SW3Z_DIR) && go build -o $(CURDIR)/$(SW3Z_BIN) ./cmd/sw3z

ifeq ($(USE_SW3Z),1)
create-paks: build $(SW3Z_BIN)
else
create-paks: build
endif
	@echo "==> Staging pak contents..."
	rm -rf $(PAK_STAGING)
	mkdir -p $(PAK_STAGING) $(BUILD_DIR_RELEASE)/baseq3
	cp -R modfiles/. $(PAK_STAGING)/
	@echo "==> Copying QVMs into pak..."
	cp -R $(MODULE_DIR_RELEASE)/vm $(PAK_STAGING)/
	@echo "==> Stamping version..."
	echo "$(APP_NAME) $$(git describe --always --dirty) ($$(date +%Y-%m-%d))" > $(PAK_STAGING)/description.txt
	@echo "==> Creating $(PAK_OUT)..."
ifeq ($(USE_SW3Z),1)
	$(SW3Z_BIN) a -x "**/.DS_Store" -x ".DS_Store" "$(PAK_OUT)" $(PAK_STAGING)
else
	cd $(PAK_STAGING) && zip -r9 "$(CURDIR)/$(PAK_OUT)" . -x "**/.DS_Store" -x ".DS_Store"
endif
	@echo "==> $(PAK_OUT) ready"

# ══════════════════════════════════════════════════════════════════════════════
# COPY TARGETS — assemble .app at Q3DIR
# ══════════════════════════════════════════════════════════════════════════════

# ── Shared copy logic ────────────────────────────────────────────────────────
# Platform-specific game module extension (resolved at read time, used in macro)
ifeq ($(UNAME_S),Darwin)
  _GAME_MODULE_EXT = $(GAME_ARCH).dylib
else
  _GAME_MODULE_EXT = .so
endif

# _do-copy-build: internal target pattern for copy-build / copy-build-debug
# Uses target-specific variables set by the caller
_do-copy-build:
	@echo "==> Copying $(_CFG) build into .app..."
ifeq ($(UNAME_S),Darwin)
	rsync -a --delete "$(_APP)/" "$(Q3DIR)/"
	@test -f "$(LAUNCHER_BIN)" && \
	  cp "$(LAUNCHER_BIN)" "$(Q3DIR)/Contents/MacOS/q3now-launcher" || \
	  echo "  NOTE: launcher not built (run make create-launcher)"
	/usr/libexec/PlistBuddy -c "Set :CFBundleExecutable q3now-launcher" \
	  "$(Q3DIR)/Contents/Info.plist"
	@test -f "$(_DED)" && cp "$(_DED)" \
	  "$(Q3DIR)/Contents/MacOS/$(APP_NAME)-ded" || true
endif
	mkdir -p "$(Q3BASEDIR)"
	cp "$(_BDIR)/$(_CFG)/baseq3/cgame$(_GAME_MODULE_EXT)"  "$(Q3BASEDIR)/"
	cp "$(_BDIR)/$(_CFG)/baseq3/qagame$(_GAME_MODULE_EXT)" "$(Q3BASEDIR)/"
	cp "$(_BDIR)/$(_CFG)/baseq3/ui$(_GAME_MODULE_EXT)"     "$(Q3BASEDIR)/"

# ── copy-build ───────────────────────────────────────────────────────────────
# Build Release engine, copy engine binary + game dylibs into .app at Q3DIR.
# Also copies launcher binary if previously built (see create-launcher).

copy-build: _BDIR := $(BUILD_DIR_RELEASE)
copy-build: _CFG  := Release
copy-build: _APP  := $(BUILT_APP_RELEASE)
copy-build: _DED  := $(BUILT_DED_RELEASE)
copy-build: build _do-copy-build

# ── copy-build-debug ─────────────────────────────────────────────────────────
# Build Debug engine, copy engine binary + game dylibs into .app at Q3DIR.
# Also copies launcher binary if previously built (see create-launcher).

copy-build-debug: _BDIR := $(BUILD_DIR_DEBUG)
copy-build-debug: _CFG  := Debug
copy-build-debug: _APP  := $(BUILT_APP_DEBUG)
copy-build-debug: _DED  := $(BUILT_DED_DEBUG)
copy-build-debug: build-debug _do-copy-build

# ── copy-libs ────────────────────────────────────────────────────────────────
# Copies renderer dylibs and third-party dependencies into .app.
# Does NOT copy engine binaries or game modules — copy-build handles those.

copy-libs: build
ifeq ($(UNAME_S),Darwin)
	@echo "==> Copying renderer + dependency dylibs..."
	cp "$(BUILD_DIR_RELEASE)/$(APP_NAME)_opengl$(RENDEXT).dylib" "$(Q3DIR)/Contents/MacOS/"
	@test -f "$(BUILD_DIR_RELEASE)/$(APP_NAME)_vulkan$(RENDEXT).dylib" && \
	  cp "$(BUILD_DIR_RELEASE)/$(APP_NAME)_vulkan$(RENDEXT).dylib" "$(Q3DIR)/Contents/MacOS/" || true
	@# Bundle third-party dylibs (SDL3, MoltenVK) into .app and rewrite load paths.
	@# Uses otool -L to find the EXACT path each binary references, then rewrites
	@# it to @executable_path/. This handles Homebrew symlink vs real path mismatches
	@# (e.g. /opt/homebrew/opt/sdl3/lib/... vs /opt/homebrew/lib/...).
	@for LIBNAME in libSDL3 libMoltenVK; do \
	  DYLIB=$$(find /opt/homebrew/lib /opt/homebrew/opt/*/lib /usr/local/lib \
	    2>/dev/null -name "$$LIBNAME*.dylib" -not -type l -maxdepth 1 | head -1); \
	  if [ -n "$$DYLIB" ]; then \
	    BASENAME=$$(basename "$$DYLIB"); \
	    echo "  $$LIBNAME: $$DYLIB → Contents/MacOS/$$BASENAME"; \
	    rm -f "$(Q3DIR)/Contents/MacOS/$$BASENAME"; \
	    cp "$$DYLIB" "$(Q3DIR)/Contents/MacOS/"; \
	    for BIN in "$(Q3DIR)/Contents/MacOS/$(APP_NAME)$(BINEXT)" \
	              "$(Q3DIR)/Contents/MacOS/$(APP_NAME)_opengl$(RENDEXT).dylib" \
	              "$(Q3DIR)/Contents/MacOS/$(APP_NAME)_vulkan$(RENDEXT).dylib"; do \
	      [ -f "$$BIN" ] || continue; \
	      LINKED=$$(otool -L "$$BIN" 2>/dev/null | grep --color=never "$$LIBNAME" | awk '{print $$1}'); \
	      if [ -n "$$LINKED" ] && [ "$$LINKED" != "@executable_path/$$BASENAME" ]; then \
	        install_name_tool -change "$$LINKED" "@executable_path/$$BASENAME" "$$BIN"; \
	        echo "    Rewrite: $$LINKED → @executable_path/$$BASENAME (in $$(basename $$BIN))"; \
	      fi; \
	    done; \
	  else \
	    echo "  WARNING: $$LIBNAME not found — run: brew install $$(echo $$LIBNAME | sed 's/lib//' | tr '[:upper:]' '[:lower:]')"; \
	  fi; \
	done
endif

# ── copy-paks ────────────────────────────────────────────────────────────────
# Copies the mod pak (pk3 or sw3z) into Q3DATADIR. Removes stale format.

copy-paks: create-paks
	mkdir -p "$(Q3DATADIR)"
ifeq ($(USE_SW3Z),1)
	@echo "==> Copying sw3z pak to $(Q3DATADIR)/"
	@rm -f "$(Q3DATADIR)/zz-$(APP_NAME).pk3"
	cp "$(PAK_OUT)" "$(Q3DATADIR)/"
else
	@echo "==> Copying pk3 pak to $(Q3DATADIR)/"
	@rm -f "$(Q3DATADIR)/zz-$(APP_NAME).sw3z"
	cp "$(PAK_OUT)" "$(Q3DATADIR)/"
endif

# ── Composite helpers ────────────────────────────────────────────────────────

copy-all:       copy-build copy-libs copy-paks
copy-all-debug: copy-build-debug copy-libs copy-paks

# ══════════════════════════════════════════════════════════════════════════════
# BUNDLING TARGETS — codesign, package for distribution
# ══════════════════════════════════════════════════════════════════════════════

# ── bundle-codesign ──────────────────────────────────────────────────────────
# Code signs the fully assembled .app bundle (macOS only).
# Ad-hoc by default; set CODESIGN_IDENTITY for distribution signing.

bundle-codesign:
ifeq ($(UNAME_S),Darwin)
	@echo "==> Code signing..."
	@for dylib in "$(Q3DIR)/Contents/MacOS/"*.dylib \
	              "$(Q3BASEDIR)/"*$(GAME_ARCH).dylib; do \
	  [ -f "$$dylib" ] && codesign --force --options runtime --sign "$(CODESIGN_IDENTITY)" "$$dylib" 2>/dev/null; \
	done
	codesign --force --options runtime --entitlements misc/macos/q3now.entitlements \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/$(APP_NAME)$(BINEXT)"
	codesign --force --options runtime \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/q3now-launcher"
	@test -f "$(Q3DIR)/Contents/MacOS/$(APP_NAME)-ded" && \
	  codesign --force --options runtime --entitlements misc/macos/q3now.entitlements \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/$(APP_NAME)-ded" || true
	@# Sign the bundle as a whole (--deep re-signs all subcomponents).
	@# Non-code data (.sw3z) lives in Contents/Resources/, not Contents/MacOS/,
	@# so codesign handles the bundle cleanly.
	codesign --force --deep --options runtime --entitlements misc/macos/q3now.entitlements \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)"
else
	@echo "bundle-codesign: macOS only, skipping"
endif

# ── bundle-dmg ───────────────────────────────────────────────────────────────
# Creates a styled, version-stamped DMG with background + drag-to-Applications.

bundle-dmg: bundle-codesign
ifeq ($(UNAME_S),Darwin)
	@echo "==> Creating $(DMG_NAME).dmg..."
	rm -rf $(DMG_STAGING) "$(DMG_OUT)"
	mkdir -p $(DMG_STAGING)
	cp -R "$(Q3DIR)" "$(DMG_STAGING)/"
	chmod -R u+w "$(DMG_STAGING)/$(APP_NAME).app/"
	xattr -cr "$(DMG_STAGING)/$(APP_NAME).app/"
	misc/macos/create-styled-dmg.sh "$(DMG_STAGING)" misc/macos/dmg-background.png "$(DMG_OUT)" "$(APP_NAME)"
	rm -rf $(DMG_STAGING)
	@echo "==> $(DMG_OUT) ready ($$(du -h "$(DMG_OUT)" | cut -f1))"
else
	@echo "DMG creation requires macOS"
endif

# ── bundle-tar ───────────────────────────────────────────────────────────────
# Creates a version-stamped tar.gz for Linux distribution.

bundle-tar:
ifeq ($(UNAME_S),Linux)
	@echo "==> Creating $(TAR_NAME).tar.gz..."
	rm -rf $(TAR_STAGING) "$(TAR_OUT)"
	mkdir -p $(TAR_STAGING)/baseq3
	cp "$(Q3DIR)/$(APP_NAME)$(BINEXT)" "$(TAR_STAGING)/"
	@test -f "$(Q3DIR)/$(APP_NAME)-ded$(BINEXT)" && \
	  cp "$(Q3DIR)/$(APP_NAME)-ded$(BINEXT)" "$(TAR_STAGING)/" || true
	cp "$(LAUNCHER_BIN)" "$(TAR_STAGING)/q3now-launcher" 2>/dev/null || true
	cp -R "$(Q3BASEDIR)/." "$(TAR_STAGING)/baseq3/"
	cp README.md "$(TAR_STAGING)/"
	tar czf "$(TAR_OUT)" -C $(TAR_STAGING) .
	rm -rf $(TAR_STAGING)
	@echo "==> $(TAR_OUT) ready ($$(du -h "$(TAR_OUT)" | cut -f1))"
else
	@echo "tar packaging is for Linux — use 'make bundle-dmg' on macOS"
endif

# ══════════════════════════════════════════════════════════════════════════════
# FLOW TARGETS — composable workflows
# ══════════════════════════════════════════════════════════════════════════════

# ── run-launcher ─────────────────────────────────────────────────────────────
# Build launcher + engine + paks, assemble .app, codesign, open launcher.

run-launcher: create-launcher copy-all bundle-codesign
ifeq ($(UNAME_S),Darwin)
	open "$(Q3DIR)"
else
	"$(Q3DIR)/q3now-launcher"
endif

# ── run-game ─────────────────────────────────────────────────────────────────
# Build engine + paks, assemble .app, run engine directly (QVM mode, no launcher).

run-game: copy-all
ifeq ($(UNAME_S),Darwin)
	"$(Q3DIR)/Contents/MacOS/$(APP_NAME)$(BINEXT)" +map $(MAP)
else
	"$(Q3DIR)/$(APP_NAME)" +map $(MAP)
endif

# ── run-gamedev ──────────────────────────────────────────────────────────────
# Build Debug engine + paks, assemble .app, run with native dylibs (vm_*=0).
# sv_pure disabled, developer mode enabled — for debugging with crash symbols.

run-gamedev: copy-all-debug
ifeq ($(UNAME_S),Darwin)
	"$(Q3DIR)/Contents/MacOS/$(APP_NAME)$(BINEXT)" \
	  +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 +set vm_ui 0 \
	  +set developer 1 +devmap $(MAP)
else
	"$(Q3DIR)/$(APP_NAME)" \
	  +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 +set vm_ui 0 \
	  +set developer 1 +devmap $(MAP)
endif

# ── release ──────────────────────────────────────────────────────────────────
# Verify outputs, build launcher + engine + paks, assemble .app, codesign,
# package DMG (macOS) or tar.gz (Linux).

release: check create-launcher copy-all bundle-codesign
ifeq ($(UNAME_S),Darwin)
	$(MAKE) bundle-dmg
else
	$(MAKE) bundle-tar
endif
	@echo ""
	@echo "  ┌─────────────────────────────────────┐"
	@echo "  │  q3now release ready                 │"
	@echo "  ├─────────────────────────────────────┤"
ifeq ($(UNAME_S),Darwin)
	@echo "  │  DMG: $(DMG_OUT)"
	@echo "  │  Size: $$(du -h "$(DMG_OUT)" | cut -f1)"
else
	@echo "  │  TAR: $(TAR_OUT)"
	@echo "  │  Size: $$(du -h "$(TAR_OUT)" | cut -f1)"
endif
	@echo "  └─────────────────────────────────────┘"

# ══════════════════════════════════════════════════════════════════════════════
# VERIFICATION & TESTING
# ══════════════════════════════════════════════════════════════════════════════

# ── check ────────────────────────────────────────────────────────────────────
# Verifies all build outputs are present.

check: create-paks
	@echo "==> Verifying build..."
	@ls $(MODULE_DIR_RELEASE)/vm/cgame.qvm  > /dev/null && echo "  cgame.qvm:   OK"
	@ls $(MODULE_DIR_RELEASE)/vm/qagame.qvm > /dev/null && echo "  qagame.qvm:  OK"
	@ls $(MODULE_DIR_RELEASE)/vm/ui.qvm     > /dev/null && echo "  ui.qvm:      OK"
	@ls $(MODULE_DIR_RELEASE)/cgame$(GAME_ARCH).*    > /dev/null && echo "  cgame.dylib: OK"
	@ls $(MODULE_DIR_RELEASE)/qagame$(GAME_ARCH).*  > /dev/null && echo "  qagame.dylib: OK"
	@ls $(MODULE_DIR_RELEASE)/ui$(GAME_ARCH).*      > /dev/null && echo "  ui.dylib:    OK"
	@test -f $(PAK_OUT) && echo "  mod pak:     OK ($(PAK_EXT))"
ifeq ($(UNAME_S),Darwin)
	@codesign --verify "$(Q3DIR)" 2>/dev/null && echo "  codesign:    OK" || echo "  codesign:    MISSING (run make bundle-codesign)"
	@codesign -d --entitlements - "$(Q3DIR)/Contents/MacOS/$(APP_NAME)$(BINEXT)" 2>/dev/null | grep -q "allow-jit" && echo "  JIT entitlement: OK" || echo "  JIT entitlement: MISSING"
endif
	@echo "==> All checks passed."

# ── smoke ────────────────────────────────────────────────────────────────────
# Headless gameplay smoke test. Requires Q3DIR with pak0.pk3.

smoke: build
ifeq ($(UNAME_S),Darwin)
	Q3DIR="$(Q3DIR)" tests/smoke.sh "$(Q3DIR)/Contents/MacOS/$(APP_NAME)-ded"
else
	Q3DIR="$(Q3DIR)" tests/smoke.sh "$(Q3DIR)/$(APP_NAME)-ded$(BINEXT)"
endif

# ── test-features ────────────────────────────────────────────────────────────
# Starts devmap with ALL feature cvars enabled, spawns 3 bots, runs 30s.

test-features: copy-all
ifeq ($(UNAME_S),Darwin)
	@echo "==> Testing all features enabled (30s)..."
	@timeout 35 "$(Q3DIR)/Contents/MacOS/$(APP_NAME)$(BINEXT)" \
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

# ── bench ────────────────────────────────────────────────────────────────────
# Timedemo benchmark. Requires a demo at Q3BASEDIR/demos/<DEMO>.dm_68.

DEMO ?= four

bench: copy-all
	@if [ ! -f "$(Q3BASEDIR)/demos/$(DEMO).dm_68" ]; then \
	  echo "ERROR: $(Q3BASEDIR)/demos/$(DEMO).dm_68 not found"; \
	  echo "Copy a demo file (.dm_68) to $(Q3BASEDIR)/demos/ and set DEMO=<name>"; \
	  exit 1; \
	fi
ifeq ($(UNAME_S),Darwin)
	open "$(Q3DIR)" --args +timedemo 1 +demo $(DEMO)
else
	"$(Q3DIR)/$(APP_NAME)$(BINEXT)" +timedemo 1 +demo $(DEMO)
endif

# ── diff-api ─────────────────────────────────────────────────────────────────
# Diffs q3now's game API headers against upstream Quake3e at the fork point.

UPSTREAM_REF ?= ecd5fa41

diff-api:
	@echo "==> API header diff (q3now vs upstream $(UPSTREAM_REF))"
	@echo "--- g_public.h ---"
	@git diff $(UPSTREAM_REF) -- code/game/g_public.h
	@echo "--- cg_public.h ---"
	@git diff $(UPSTREAM_REF) -- code/cgame/cg_public.h
	@echo "--- ui_public.h ---"
	@git diff $(UPSTREAM_REF) -- code/ui/ui_public.h

# ══════════════════════════════════════════════════════════════════════════════
# HELP
# ══════════════════════════════════════════════════════════════════════════════

help:
	@echo ""
	@echo "  q3now build targets"
	@echo "  ───────────────────────────────────────────────────────────"
	@echo "  make              configure + build Release"
	@echo "  make build-debug  configure + build Debug"
	@echo "  make clean        remove build-release/ + build-debug/"
	@echo "  make rebuild      clean + build"
	@echo ""
	@echo "  Generation:"
	@echo "    make create-launcher    build Go/Wails launcher binary"
	@echo "    make create-paks        package modfiles/ + QVMs → .$(PAK_EXT)"
	@echo ""
	@echo "  Copy (assemble .app):"
	@echo "    make copy-build         Release engine + dylibs → .app"
	@echo "    make copy-build-debug   Debug engine + dylibs → .app"
	@echo "    make copy-libs          renderer + deps → .app"
	@echo "    make copy-paks          mod pak (.$(PAK_EXT)) → .app"
	@echo "    make copy-all           all of the above (Release)"
	@echo "    make copy-all-debug     all of the above (Debug)"
	@echo ""
	@echo "  Bundling:"
	@echo "    make bundle-codesign    codesign .app (macOS)"
	@echo "    make bundle-dmg         create versioned DMG (macOS)"
	@echo "    make bundle-tar         create versioned tar.gz (Linux)"
	@echo ""
	@echo "  Flows:"
	@echo "    make run-launcher       build + assemble + codesign + open launcher"
	@echo "    make run-game           build + assemble + run engine (QVM)"
	@echo "    make run-gamedev        build-debug + assemble + run engine (native)"
	@echo "    make release            build + assemble + codesign + package"
	@echo ""
	@echo "  Testing:"
	@echo "    make check              verify all outputs present"
	@echo "    make smoke              headless gameplay test"
	@echo "    make test-features      all features enabled stress test"
	@echo "    make bench              timedemo benchmark"
	@echo "    make diff-api           diff API headers vs upstream"
	@echo ""
	@echo "  Variables:"
	@echo "    Q3DIR=$(Q3DIR)"
	@echo "    JOBS=$(JOBS)   MAP=$(MAP)   DEMO=$(DEMO)"
	@echo "    USE_SW3Z=$(USE_SW3Z)   CODESIGN_IDENTITY=$(CODESIGN_IDENTITY)"
	@echo ""
