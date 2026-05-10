# Wired — developer Makefile
#
# Composable build workflow with granular targets.
# All cmake details live in cmake/ — this file is a thin workflow layer.
#
# GENERATION TARGETS
#   make create-launcher     build Go/Wails launcher binary
#   make create-packs        package modfiles/ + VM modules → mod pack
#   make build-fonts         build MSDF font atlases from TTF sources
#
# COPY TARGETS (assemble .app at Q3DIR)
#   make copy-libs           copy renderer + dependency dylibs into .app
#   make copy-build          copy Release engine + game dylibs into .app
#   make copy-build DEV=1    same with Debug build
#   make copy-packs          copy mod pack into .app data directory
#
# BUNDLING TARGETS
#   make bundle-codesign     codesign the .app bundle (macOS)
#   make bundle-dmg          create versioned DMG (macOS)
#   make bundle-tar          create versioned tar.gz (Linux)
#
# FLOW TARGETS
#   make run-launcher        		  build + assemble + codesign + open launcher
#   make run-game                     run engine (main menu)
#   make run-game DEV=1               debug build
#   make run-game DEV=1 MAP=arena7    map with debug
#   make run-game VM=1 MAP=arena7     VM modules + map
#   make run-ded                      run dedicated server (no client)
#   make run-ded DEV=1                dedicated + debug build
#   make run-ded DEV=1 MAP=arena7     dedicated + debug + load map
#   make release             		  build + assemble + codesign + package
#
# VARIABLES (override on command line or env)
#   Q3DIR              install destination      (default: /Applications/q3now on macOS)
#   JOBS               parallel job count       (default: CPU count)
#   MAP                map to load              (default: none = main menu)
#   DEV                debug build              (default: 0; 1 = debug build)
#   VM                 VM game modules           (default: 0; 1=VM + sv_pure 1)
#   USE_WASM           VM backend via WAMR       (0=off, 1=on; default: 1)
#   CHANNEL            release channel           (default: preview; "public" omits suffix)
#   CODESIGN_IDENTITY  signing identity         (default: - = ad-hoc)
#   UPSTREAM_REF       fork point for diff-api  (default: ecd5fa41)

# ── Defaults ──────────────────────────────────────────────────────────────────

# Release channel: "preview" (default), "canary", "public" (no suffix), etc.
CHANNEL ?= preview
ifeq ($(CHANNEL),public)
  CHANNEL_SUFFIX :=
else
  CHANNEL_SUFFIX := -$(CHANNEL)
endif

# Branding split:
#   PRODUCT_NAME   — game/product name (matches CMakeLists.txt PRODUCT_NAME).
#                    Used for install paths, package archives, .app display.
#                    Override via PRODUCT_NAME=othergame for builds targeting
#                    a different game on the wired engine.
#   CMAKE_APP_NAME — engine binary name (matches CMakeLists.txt CNAME = wired).
#                    Used for build-output paths inside build/<cfg>/ and for
#                    binary filenames inside installed bundles.
#   APP_NAME       — channel-suffixed product name. Used for install paths,
#                    DMG/tar/zip filenames, and the .app display name on macOS.
PRODUCT_NAME   ?= q3now
CMAKE_APP_NAME := wired
APP_NAME       ?= $(PRODUCT_NAME)$(CHANNEL_SUFFIX)
MAP        ?=
DEV        ?= 0
VM         ?= 0

# Dual build directories — avoid cmake reconfigure thrash between Release/Debug.
# BUILD_DIR / BUILD_CFG are DEV-driven aliases. Targets that should
# follow DEV=1 use these; packaging / bundling targets stay on the explicit
# *_RELEASE roots so distributions are never accidentally Debug.
ifeq ($(DEV),1)
  BUILD_DIR    := build/debug
  BUILD_CFG    := Debug

  ifeq ($(MAKECMDGOALS),release)
  	$(error 'make release' requires DEV=0 (Release build))
  endif
else
  BUILD_DIR    := build/release
  BUILD_CFG    := Release
endif

# CPU count and architecture detection
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Detect Windows (Git Bash / MSYS2 reports MINGW64_NT-* or MSYS_NT-*)
IS_WINDOWS :=
ifneq ($(findstring MINGW,$(UNAME_S)),)
  IS_WINDOWS := 1
else ifneq ($(findstring MSYS,$(UNAME_S)),)
  IS_WINDOWS := 1
endif

ifdef IS_WINDOWS
  # Windows uses .x64 (not .x86_64) — matches CMakeLists.txt BINEXT
  BINEXT  := .x64
  RENDEXT := _x86_64
  EXEEXT  := .exe
else ifeq ($(UNAME_M),arm64)
  BINEXT  := .arm64
  RENDEXT := _arm64
  EXEEXT  :=
else ifeq ($(UNAME_M),aarch64)
  BINEXT  := .aarch64
  RENDEXT := _aarch64
  EXEEXT  :=
else ifeq ($(UNAME_M),x86_64)
  BINEXT  := .x86_64
  RENDEXT := _x86_64
  EXEEXT  :=
else
  BINEXT  :=
  RENDEXT :=
  EXEEXT  :=
endif
# GAME_ARCH mirrors ARCH_STRING from q_platform.h (vm.c appends it to dylib filenames)
GAME_ARCH := $(patsubst _%,%,$(RENDEXT))
ifeq ($(UNAME_S),Darwin)
  JOBS      ?= $(shell sysctl -n hw.ncpu)
  # CMake assembles a single product bundle on macOS:
  #   build/<cfg>/$(APP_NAME)$(BINEXT).app/Contents/MacOS/{wired$(BINEXT), wired-ded$(BINEXT)}
  # Bundle directory is product+channel branded; engine binaries inside keep
  # their wired/wired-ded names. See "Combined macOS bundle assembly" in CMakeLists.txt.
  BUILT_APP  := $(BUILD_DIR)/$(APP_NAME)$(BINEXT).app
  ENGINE_BIN := $(BUILT_APP)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)
  BUILT_DED  := $(BUILT_APP)/Contents/MacOS/$(CMAKE_APP_NAME)-ded$(BINEXT)
  Q3DIR       ?= /Applications/$(APP_NAME).app
else
  JOBS        ?= $(shell nproc 2>/dev/null || echo 4)
  ENGINE_BIN  := $(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)
  ifdef IS_WINDOWS
  	Q3DIR     ?= $(LOCALAPPDATA)/Programs/$(APP_NAME)
  else
  	Q3DIR     ?= $(HOME)/.local/share/$(APP_NAME)
  endif
endif

# cmake puts game modules at <build-dir>/<build-cfg>/baseq3/
MODULE_DIR := $(BUILD_DIR)/$(BUILD_CFG)/baseq3

ifeq ($(UNAME_S),Darwin)
  # macOS bundle conventions: code in Contents/MacOS/, data in Contents/Resources/.
  Q3BINDIR   := $(Q3DIR)/Contents/MacOS
  Q3DATADIR  := $(Q3DIR)/Contents/Resources/baseq3
else
  Q3BINDIR   := $(Q3DIR)
  Q3DATADIR  := $(Q3DIR)/baseq3
endif

# Use Ninja if available — much faster incremental builds
ifneq ($(shell which ninja 2>/dev/null),)
  GENERATOR := -G Ninja
else
  GENERATOR :=
endif

# VM backend toggle: 1 = enable WAMR, 0 = legacy QVM only
USE_WASM ?= 1

ifeq ($(USE_WASM),1)
  CMAKE_WASM_FLAG := -DUSE_WASM=ON
  # Forward WASI_SDK_PATH to cmake if set (cmake auto-detects /opt/wasi-sdk)
  ifneq ($(WASI_SDK_PATH),)
    CMAKE_WASM_FLAG += -DWASI_SDK_PATH=$(WASI_SDK_PATH)
  endif
else
  CMAKE_WASM_FLAG := -DUSE_WASM=OFF
endif

# SDL toggle: 1 = enable (default), 0 = disable (Windows CI has no SDL)
USE_SDL ?= 1

ifeq ($(USE_SDL),1)
  CMAKE_SDL_FLAG := -DUSE_SDL=ON
else
  CMAKE_SDL_FLAG := -DUSE_SDL=OFF
endif

CMAKE_EXTRA_FLAGS ?=
CMAKE_CHANNEL_FLAG := -DCHANNEL_SUFFIX="$(CHANNEL_SUFFIX)"
CMAKE_PRODUCT_FLAG := -DPRODUCT_NAME="$(PRODUCT_NAME)"
CMAKE_CONFIGURE    := cmake -S . -B $(BUILD_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=$(BUILD_CFG) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(CMAKE_WASM_FLAG) $(CMAKE_SDL_FLAG) $(CMAKE_CHANNEL_FLAG) $(CMAKE_PRODUCT_FLAG) $(CMAKE_EXTRA_FLAGS)
CMAKE_BUILD        := cmake --build $(BUILD_DIR) --parallel $(JOBS)

# Code signing identity (default: ad-hoc).
CODESIGN_IDENTITY ?= -

# DMG packaging (macOS only)
VERSION     := $(shell date +%Y%m%d)-$(shell git describe --always --dirty)
DMG_NAME    := $(APP_NAME)-$(VERSION)-$(UNAME_M)
DMG_STAGING := $(BUILD_DIR)/dmg-staging
DMG_OUT     := $(BUILD_DIR)/$(DMG_NAME).dmg

# tar.gz packaging (Linux)
TAR_NAME    := $(APP_NAME)-$(VERSION)-linux-$(UNAME_M)
TAR_STAGING := $(BUILD_DIR)/tar-staging
TAR_OUT     := $(BUILD_DIR)/$(TAR_NAME).tar.gz

# zip packaging (Windows)
ZIP_NAME    := $(APP_NAME)-$(VERSION)-windows-x86_64
ZIP_STAGING := $(BUILD_DIR)/zip-staging
ZIP_OUT     := $(BUILD_DIR)/$(ZIP_NAME).zip

# Launcher (Go/Wails)
LAUNCHER_DIR := launcher
ifeq ($(UNAME_S),Darwin)
  LAUNCHER_BIN     := $(LAUNCHER_DIR)/build/bin/q3now.app/Contents/MacOS/q3now-launcher
  # Self-contained launcher build dir layout (post create-launcher).
  # paths.GameBinaryPath() = filepath.Dir(os.Executable())/<engine name>;
  # the launcher binary lives in Contents/MacOS/ so the engine binary
  # colocates there. Game modules + paks go under Contents/Resources/baseq3
  # because post-engine-Phase-1 the basegame scan reaches only the resource
  # path on macOS (mirrors the Q3DATADIR collapse for the install layout).
  # Bundle name is the GAME (q3now) per Wired-vs-game branding boundary —
  # the engine binaries (wired.x64, wired_<api>_<arch>.dylib) live INSIDE the
  # game's .app wrapper. Wails emits this bundle name from launcher/wails.json
  # ("name": "q3now"); align consumer paths here so Linux / Windows / macOS
  # all key off the same launcher artifact.
  LAUNCHER_DSTBIN  := $(LAUNCHER_DIR)/build/bin/q3now.app/Contents/MacOS
  LAUNCHER_DSTDATA := $(LAUNCHER_DIR)/build/bin/q3now.app/Contents/Resources/baseq3
else ifdef IS_WINDOWS
  LAUNCHER_BIN     := $(LAUNCHER_DIR)/build/bin/q3now-launcher.exe
  LAUNCHER_DSTBIN  := $(LAUNCHER_DIR)/build/bin
  LAUNCHER_DSTDATA := $(LAUNCHER_DIR)/build/bin/baseq3
else
  LAUNCHER_BIN     := $(LAUNCHER_DIR)/build/bin/q3now-launcher
  LAUNCHER_DSTBIN  := $(LAUNCHER_DIR)/build/bin
  LAUNCHER_DSTDATA := $(LAUNCHER_DIR)/build/bin/baseq3
endif
WAILS_TAGS   ?=

# SW3Z archiver
SW3Z_DIR := tools/sw3z-archiver
SW3Z_BIN := $(SW3Z_DIR)/cmd/sw3z/sw3z

# VM backend toggle (moved to top, before ifeq)
# USE_WASM is defined near other cmake flags above

# Pak output (always from Release build — VM modules are always Release)
PAK_STAGING := $(BUILD_DIR)/pak-staging
PAK_OUT := $(BUILD_DIR)/baseq3/pax21.sw3z

# ── Phony targets ─────────────────────────────────────────────────────────────

.PHONY: all configure build clean clean-launcher clean-all rebuild \
        create-launcher create-packs build-fonts \
        _wails-build \
        copy-libs copy-build copy-packs copy-all \
        bundle-codesign bundle-dmg bundle-tar bundle-zip bundle-docker \
        run-launcher run-game release \
        check smoke test-vm test-quic-game test-fs-dedup bench diff-api lint help

all: build

# ══════════════════════════════════════════════════════════════════════════════
# FOUNDATION — configure, build, clean
# ══════════════════════════════════════════════════════════════════════════════

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE_CONFIGURE)

configure: $(BUILD_DIR)/CMakeCache.txt

build: $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE_BUILD)

clean:
	rm -rf $(BUILD_DIR)

# clean-launcher: nuke wails build output. Preserves frontend/node_modules
# and Go module cache (rebuilding those takes minutes).
clean-launcher:
	rm -rf $(LAUNCHER_DIR)/build

clean-all: clean clean-launcher

rebuild: clean build

# ══════════════════════════════════════════════════════════════════════════════
# GENERATION TARGETS — produce artifacts
# ══════════════════════════════════════════════════════════════════════════════

# ── create-launcher ──────────────────────────────────────────────────────────
# Builds the Go/Wails launcher AND populates its build directory with the
# engine, ded server, renderer/dependency libs, game modules, and mod pack.
#
# After this target the launcher build dir is a self-contained mini-install:
# running launcher/build/bin/q3now-launcher{,.exe,.app/Contents/MacOS/...}
# works without copy-build / Q3DIR. The structural assumption in
# launcher/internal/config/paths.go (engine binary lives next to launcher
# binary) holds on every platform.
#
# Requires: go, node, wails CLI for the launcher; cmake for the engine.

create-launcher: build create-packs _wails-build
	$(call install_engine,$(LAUNCHER_DSTBIN),$(LAUNCHER_DSTDATA))
	$(call install_libs,$(LAUNCHER_DSTBIN))
	$(call install_pack,$(LAUNCHER_DSTDATA))
	@echo "==> Launcher ready (self-contained): $(LAUNCHER_BIN)"

# _wails-build: internal step. Runs wails build and asserts the output exists
# before downstream _install-* helpers populate the build dir. Split from
# create-launcher's recipe so the helpers can run as ordered prerequisites.
_wails-build:
	@echo "==> Building launcher (wails)..."
	cd $(LAUNCHER_DIR) && PATH="$$HOME/go/bin:$$PATH" wails build \
	  $(WAILS_TAGS) -ldflags "-X main.version=$(VERSION) -X github.com/eser/q3now/launcher/internal/config.channelSuffix=$(CHANNEL_SUFFIX)"
	@test -f "$(LAUNCHER_BIN)" || { \
	  echo "ERROR: wails build did not produce $(LAUNCHER_BIN)"; \
	  exit 1; }

# ── create-packs ──────────────────────────────────────────────────────────────
# Packages modfiles/ + VM modules into the mod pack (pax21.sw3z).
# "pax21" sorts after pak0–pak8, ensuring highest override priority.
# VM modules here override the stock 1999 bytecode in the base pack.

$(SW3Z_BIN):
	cd $(SW3Z_DIR) && go build -o $(CURDIR)/$(SW3Z_BIN) ./cmd/sw3z

create-packs: build $(SW3Z_BIN)
	@echo "==> Staging pak contents..."
	rm -rf $(PAK_STAGING)
	mkdir -p $(PAK_STAGING) $(BUILD_DIR)/baseq3
	cp -R modfiles/. $(PAK_STAGING)/
	@echo "==> Copying VM modules into pak..."
	# WASM VM modules (cgame.wasm / qagame.wasm) are config-independent
	# bytecode and the cmake config only builds them under the Release
	# tree — so DEV=1 still pulls them from build/release. Engine
	# binaries and native game DLLs DO follow DEV via $(MODULE_DIR);
	# this one line is the deliberate exception.
	cp -R $(MODULE_DIR)/vm $(PAK_STAGING)/
	@echo "==> Stamping version..."
	echo "$(APP_NAME) $$(git describe --always --dirty) ($$(date +%Y-%m-%d))" > $(PAK_STAGING)/description.txt
	@echo "==> Creating $(PAK_OUT)..."
	$(SW3Z_BIN) a -x "**/.DS_Store" -x ".DS_Store" "$(PAK_OUT)" $(PAK_STAGING)
	@echo "==> $(PAK_OUT) ready"

# ── build-fonts ───────────────────────────────────────────────────────────────
# Build MSDF font atlases from TTF sources using msdf-atlas-gen.
# Requires: TTF files in assets/fonts/ (see assets/fonts/README.md)

MSDF_ATLAS_GEN_DIR = tools/msdf-atlas-gen
MSDF_ATLAS_GEN     = $(MSDF_ATLAS_GEN_DIR)/build/bin/msdf-atlas-gen
FONT_SRC           = assets/fonts
FONT_OUT           = modfiles/fonts

$(MSDF_ATLAS_GEN):
	cd $(MSDF_ATLAS_GEN_DIR) && cmake -B build -DCMAKE_BUILD_TYPE=$(BUILD_CFG) -DMSDF_ATLAS_BUILD_STANDALONE=ON -DMSDF_ATLAS_USE_VCPKG=OFF -DMSDF_ATLAS_USE_SKIA=OFF && cmake --build build --config $(BUILD_CFG)

FONT_WEIGHT_TRANSFORM = python3 tools/font-weight-transform.py
FONT_BUILD_TMP        = $(FONT_SRC)/.build

# Sansman changeWeight deltas
SANSMAN_REGULAR_DELTA = -50
SANSMAN_MEDIUM_DELTA  = -25

build-fonts: $(MSDF_ATLAS_GEN)
	@command -v fontforge >/dev/null 2>&1 || { echo "ERROR: fontforge not found. Install: brew install fontforge (macOS) or apt install fontforge python3-fontforge (Ubuntu)"; exit 1; }
	@mkdir -p $(FONT_OUT) $(FONT_BUILD_TMP)
	@echo "==> Transforming Enter Sansman weights via FontForge..."
	$(FONT_WEIGHT_TRANSFORM) $(FONT_SRC)/entsans.ttf $(FONT_BUILD_TMP)/sansman-regular.ttf $(SANSMAN_REGULAR_DELTA)
	$(FONT_WEIGHT_TRANSFORM) $(FONT_SRC)/entsans.ttf $(FONT_BUILD_TMP)/sansman-medium.ttf $(SANSMAN_MEDIUM_DELTA)
	$(FONT_WEIGHT_TRANSFORM) $(FONT_SRC)/entsani.ttf $(FONT_BUILD_TMP)/sansman-italic.ttf $(SANSMAN_REGULAR_DELTA)
	@echo "==> Generating MSDF atlases..."
	$(MSDF_ATLAS_GEN) -font $(FONT_BUILD_TMP)/sansman-regular.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 72 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-regular.png -json $(FONT_OUT)/sansman-regular.json
	$(MSDF_ATLAS_GEN) -font $(FONT_BUILD_TMP)/sansman-medium.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 72 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-medium.png -json $(FONT_OUT)/sansman-medium.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/entsans.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 72 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-bold.png -json $(FONT_OUT)/sansman-bold.json
	$(MSDF_ATLAS_GEN) -font $(FONT_BUILD_TMP)/sansman-italic.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 72 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-italic.png -json $(FONT_OUT)/sansman-italic.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/entsani.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 72 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-bold-italic.png -json $(FONT_OUT)/sansman-bold-italic.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/Oxanium-Regular.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 72 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/oxanium.png -json $(FONT_OUT)/oxanium.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/Oxanium-Medium.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 72 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/oxanium-medium.png -json $(FONT_OUT)/oxanium-medium.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/ShareTechMono-Regular.ttf -charset $(FONT_SRC)/charset_console.txt -type msdf -format png -size 72 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sharetechmono.png -json $(FONT_OUT)/sharetechmono.json
	@rm -rf $(FONT_BUILD_TMP)
	@echo "==> Generated 10 MSDF font atlases in $(FONT_OUT)/"

# ══════════════════════════════════════════════════════════════════════════════
# COPY TARGETS — assemble .app at Q3DIR
# ══════════════════════════════════════════════════════════════════════════════

# ── Shared copy logic ────────────────────────────────────────────────────────
# Platform-specific game module extension (resolved at read time, used in macro)
ifeq ($(UNAME_S),Darwin)
  _GAME_MODULE_EXT = $(GAME_ARCH).dylib
else ifdef IS_WINDOWS
  _GAME_MODULE_EXT = $(GAME_ARCH).dll
else
  _GAME_MODULE_EXT = $(GAME_ARCH).so
endif

# ── Install helpers (parameterized via $(call ...) macros) ───────────────────
# These four helpers are invoked from create-launcher / copy-build / copy-libs
# / copy-packs.  Same engine artifacts can land in Q3DIR (production install)
# or launcher/build/bin/ (self-contained launcher dev dir) without
# duplicating the copy logic.
#
# Why macros, not phony targets: phony targets dedup per Make invocation, so
# `make run-launcher` (which depends on both create-launcher and copy-build)
# would build _install-engine ONCE, with whichever caller's target-specific
# vars were resolved first — silently skipping the other caller's install.
# Macros expand inline at the call site with explicit args, no dedup.
#
# Across all four callers, _BDIR/_CFG/_APP/_DED were invariant globals
# (BUILD_DIR/BUILD_CFG/BUILT_APP/BUILT_DED); only _DSTBIN/_DSTDATA varied.
# The macros take only the varying values as arguments and reference the
# rest of the globals directly.

# install_engine($(1)=dstbin, $(2)=dstdata) — copies engine + ded binaries
# and game modules into the destination.  On macOS the engine lives inside
# cmake's per-target .app bundle; elsewhere it's a flat file under BUILD_DIR.
ifeq ($(UNAME_S),Darwin)
define install_engine
	@echo "==> Installing engine + game modules into $(1) ..."
	@mkdir -p "$(1)" "$(2)"
	cp "$(BUILT_APP)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)" "$(1)/"
	@test -f "$(BUILT_DED)" && cp "$(BUILT_DED)" "$(1)/$(CMAKE_APP_NAME)-ded" || true
	cp "$(BUILD_DIR)/$(BUILD_CFG)/baseq3/cgame$(_GAME_MODULE_EXT)"  "$(2)/"
	cp "$(BUILD_DIR)/$(BUILD_CFG)/baseq3/qagame$(_GAME_MODULE_EXT)" "$(2)/"
endef
else
define install_engine
	@echo "==> Installing engine + game modules into $(1) ..."
	@mkdir -p "$(1)" "$(2)"
	cp "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" "$(1)/"
	@test -f "$(BUILD_DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)" && \
	  cp "$(BUILD_DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)" "$(1)/" || true
	cp "$(BUILD_DIR)/$(BUILD_CFG)/baseq3/cgame$(_GAME_MODULE_EXT)"  "$(2)/"
	cp "$(BUILD_DIR)/$(BUILD_CFG)/baseq3/qagame$(_GAME_MODULE_EXT)" "$(2)/"
endef
endif

# install_app_skeleton — installs the launcher binary into Q3DIR.  macOS
# rsyncs the engine .app skeleton (Contents/Info.plist, Resources/, etc.)
# and overlays the launcher binary, rewriting CFBundleExecutable so
# double-click runs the launcher.  Windows copies the launcher .exe into
# Q3DIR.  Linux is a no-op — Q3DIR-installed launcher isn't part of the
# Linux flow; users run launcher/build/bin/q3now-launcher directly.
ifeq ($(UNAME_S),Darwin)
define install_app_skeleton
	@echo "==> Installing .app skeleton to $(Q3DIR) ..."
	rsync -a --checksum --delete "$(BUILT_APP)/" "$(Q3DIR)/"
	@test -f "$(LAUNCHER_BIN)" && \
	  cp "$(LAUNCHER_BIN)" "$(Q3DIR)/Contents/MacOS/q3now-launcher" || \
	  echo "  NOTE: launcher not built (run make create-launcher)"
	/usr/libexec/PlistBuddy -c "Set :CFBundleExecutable q3now-launcher" \
	  "$(Q3DIR)/Contents/Info.plist"
endef
else ifdef IS_WINDOWS
define install_app_skeleton
	@mkdir -p "$(Q3DIR)"
	@test -f "$(LAUNCHER_BIN)" && \
	  cp "$(LAUNCHER_BIN)" "$(Q3DIR)/q3now-launcher.exe" || \
	  echo "  NOTE: launcher not built (run make create-launcher)"
endef
else
define install_app_skeleton
	@true
endef
endif

# install_libs($(1)=dstbin) — copies renderer plug-ins (and macOS third-party
# deps) into $(1).  On macOS, otool -L discovers each binary's exact linked
# library path and rewrites it to @executable_path/ — handles Homebrew
# symlink vs real path mismatches (e.g. /opt/homebrew/opt/sdl3/lib/... vs
# /opt/homebrew/lib/...).
ifeq ($(UNAME_S),Darwin)
define install_libs
	@mkdir -p "$(1)"
	@echo "==> Installing renderer + dependency dylibs into $(1) ..."
	cp "$(BUILD_DIR)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).dylib" "$(1)/"
	@test -f "$(BUILD_DIR)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dylib" && \
	  cp "$(BUILD_DIR)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dylib" "$(1)/" || true
	@for LIBNAME in libSDL3 libMoltenVK; do \
	  DYLIB=$$(find /opt/homebrew/lib /opt/homebrew/opt/*/lib /usr/local/lib \
	    2>/dev/null -name "$$LIBNAME*.dylib" -not -type l -maxdepth 1 | head -1); \
	  if [ -n "$$DYLIB" ]; then \
	    BASENAME=$$(basename "$$DYLIB"); \
	    echo "  $$LIBNAME: $$DYLIB → $$BASENAME"; \
	    rm -f "$(1)/$$BASENAME"; \
	    cp "$$DYLIB" "$(1)/"; \
	    for BIN in "$(1)/$(CMAKE_APP_NAME)$(BINEXT)" \
	              "$(1)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).dylib" \
	              "$(1)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dylib"; do \
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
endef
else ifdef IS_WINDOWS
define install_libs
	@mkdir -p "$(1)"
	@echo "==> Installing renderer DLLs into $(1) ..."
	@for dll in "$(BUILD_DIR)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).dll" \
	            "$(BUILD_DIR)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dll"; do \
	  [ -f "$$dll" ] && cp "$$dll" "$(1)/" || true; \
	done
endef
else
define install_libs
	@mkdir -p "$(1)"
	@echo "==> Installing renderer shared objects into $(1) ..."
	@for so in "$(BUILD_DIR)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).so" \
	           "$(BUILD_DIR)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).so"; do \
	  [ -f "$$so" ] && cp "$$so" "$(1)/" || true; \
	done
endef
endif

# install_pack($(1)=dstdata) — copies the mod pack into $(1).
define install_pack
	@echo "==> Installing mod pack into $(1) ..."
	@mkdir -p "$(1)"
	cp "$(PAK_OUT)" "$(1)/"
endef

# ── copy-build ───────────────────────────────────────────────────────────────
# Build Release engine, install engine + game modules into Q3DIR. On macOS
# also rsyncs the .app skeleton; on Windows also copies the launcher .exe.
# (The launcher binary is sourced from $(LAUNCHER_BIN), produced earlier by
# make create-launcher.)

copy-build: build
	$(call install_app_skeleton)
	$(call install_engine,$(Q3BINDIR),$(Q3DATADIR))

# ── copy-libs ────────────────────────────────────────────────────────────────
# Copies renderer plug-ins (and macOS third-party deps) into Q3DIR.
# Does NOT copy engine binaries or game modules — copy-build handles those.

copy-libs: build
	$(call install_libs,$(Q3BINDIR))

# ── copy-packs ────────────────────────────────────────────────────────────────
# Copies the mod pack into Q3DATADIR. Removes stale format.

copy-packs: create-packs
	$(call install_pack,$(Q3DATADIR))

# ── Composite helpers ────────────────────────────────────────────────────────

copy-all:       copy-build copy-libs copy-packs

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
	              "$(Q3DATADIR)/"*$(GAME_ARCH).dylib; do \
	  [ -f "$$dylib" ] && codesign --force --options runtime --sign "$(CODESIGN_IDENTITY)" "$$dylib" 2>/dev/null; \
	done
	codesign --force --options runtime --entitlements misc/macos/wired.entitlements \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)"
	codesign --force --options runtime \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/q3now-launcher"
	@test -f "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-ded" && \
	  codesign --force --options runtime --entitlements misc/macos/wired.entitlements \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-ded" || true
	@# Sign the bundle as a whole (--deep re-signs all subcomponents).
	@# Non-code data (.sw3z) lives in Contents/Resources/, not Contents/MacOS/,
	@# so codesign handles the bundle cleanly.
	codesign --force --deep --options runtime --entitlements misc/macos/wired.entitlements \
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
	cp "$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)" "$(TAR_STAGING)/"
	@test -f "$(Q3DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)" && \
	  cp "$(Q3DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)" "$(TAR_STAGING)/" || true
	cp "$(LAUNCHER_BIN)" "$(TAR_STAGING)/q3now-launcher" 2>/dev/null || true
	cp -R "$(Q3DATADIR)/." "$(TAR_STAGING)/baseq3/"
	cp README.md "$(TAR_STAGING)/"
	tar czf "$(TAR_OUT)" -C $(TAR_STAGING) .
	rm -rf $(TAR_STAGING)
	@echo "==> $(TAR_OUT) ready ($$(du -h "$(TAR_OUT)" | cut -f1))"
else
	@echo "tar packaging is for Linux — use 'make bundle-dmg' on macOS"
endif

# ── bundle-zip ──────────────────────────────────────────────────────────
# Creates a version-stamped zip for Windows distribution.

bundle-zip:
ifdef IS_WINDOWS
	@echo "==> Creating $(ZIP_NAME).zip..."
	rm -rf $(ZIP_STAGING) "$(ZIP_OUT)"
	mkdir -p $(ZIP_STAGING)/baseq3
	cp "$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" "$(ZIP_STAGING)/"
	@test -f "$(Q3DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)" && \
	  cp "$(Q3DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)" "$(ZIP_STAGING)/" || true
	cp "$(Q3DIR)/q3now-launcher.exe" "$(ZIP_STAGING)/" 2>/dev/null || true
	@for dll in "$(Q3DIR)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).dll" \
	            "$(Q3DIR)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dll"; do \
	  [ -f "$$dll" ] && cp "$$dll" "$(ZIP_STAGING)/" || true; \
	done
	cp -R "$(Q3DATADIR)/." "$(ZIP_STAGING)/baseq3/"
	cp README.md "$(ZIP_STAGING)/"
	cd $(ZIP_STAGING) && powershell -Command "Compress-Archive -Path '*' -DestinationPath '$(CURDIR)/$(ZIP_OUT)' -Force"
	rm -rf $(ZIP_STAGING)
	@echo "==> $(ZIP_OUT) ready"
else
	@echo "zip packaging is for Windows — use 'make bundle-dmg' on macOS or 'make bundle-tar' on Linux"
endif

# ── bundle-docker ────────────────────────────────────────────────────────
# Builds a Docker image for the dedicated server (Linux x86_64).

DOCKER_IMAGE ?= eserozvataf/q3now
DOCKER_TAG   ?= $(VERSION)

bundle-docker:
	docker build -t "$(DOCKER_IMAGE):$(DOCKER_TAG)" -t "$(DOCKER_IMAGE):latest" .
	@echo "==> Docker image ready: $(DOCKER_IMAGE):$(DOCKER_TAG)"

# ══════════════════════════════════════════════════════════════════════════════
# FLOW TARGETS — composable workflows
# ══════════════════════════════════════════════════════════════════════════════

# ── run-launcher ─────────────────────────────────────────────────────────────
# Build launcher + engine + paks, assemble .app, codesign, open launcher.

run-launcher: create-launcher copy-all bundle-codesign
ifeq ($(UNAME_S),Darwin)
	open "$(Q3DIR)"
else
	"$(Q3DIR)/q3now-launcher$(EXEEXT)"
endif

# ── run-game ─────────────────────────────────────────────────────────────────
# Build engine + paks, assemble .app, run engine directly.
#
# Variables:
#   DEV=1    debug build                                       (default: 0)
#   VM=1     use VM game modules instead of native dylibs      (default: 0)
#   MAP=X    load map X; uses +map when DEV=1, +map otherwise
#
# Examples:
#   make run-game                    main menu (native dylibs)
#   make run-game MAP=arena7         load arena7 (temple of retribution)
#   make run-game DEV=1              debug build
#   make run-game DEV=1 MAP=arena7   debug build, map arena7
#   make run-game VM=1 MAP=arena7    VM modules, load arena7

# DEV controls Release vs Debug throughout BUILD_DIR / BUILD_CFG
# / BUILT_APP / BUILT_DED / MODULE_DIR / PAK_OUT — copy-all picks up the right
# config automatically. No separate copy-all-debug.
_RUN_GAME_DEP := copy-all

# VM mode: 0=native dylibs (default), 1=WASM AOT modules with sv_pure
ifeq ($(VM),1)
_RUN_VM_ARGS := +set sv_pure 1 +set vm_game 2 +set vm_cgame 2
else
_RUN_VM_ARGS := +set sv_pure 0 +set vm_game 0 +set vm_cgame 0
endif

# Compose command-line arguments
_RUN_GAME_ARGS := $(_RUN_VM_ARGS)
ifeq ($(DEV),1)
_RUN_GAME_ARGS += +set sv_cheats 1
endif
ifneq ($(MAP),)
_RUN_GAME_ARGS += +map $(MAP)
endif
ifneq ($(EXTRA_ARGS),)
_RUN_GAME_ARGS += $(EXTRA_ARGS)
endif

# Copy VM modules into .app when VM=1
_RUN_VM_COPY :=
ifeq ($(VM),1)
_RUN_VM_COPY := _copy-vm
endif

_copy-vm:
ifeq ($(UNAME_S),Darwin)
	@mkdir -p "$(Q3DIR)/Contents/Resources/baseq3/vm"
	@for f in $(BUILD_DIR)/$(BUILD_CFG)/baseq3/vm/*.wasm $(BUILD_DIR)/$(BUILD_CFG)/baseq3/vm/*.aot; do \
		[ -f "$$f" ] && cp "$$f" "$(Q3DIR)/Contents/Resources/baseq3/vm/" || true; \
	done
else
	@mkdir -p "$(Q3DIR)/baseq3/vm"
	@for f in $(BUILD_DIR)/$(BUILD_CFG)/baseq3/vm/*.wasm $(BUILD_DIR)/$(BUILD_CFG)/baseq3/vm/*.aot; do \
		[ -f "$$f" ] && cp "$$f" "$(Q3DIR)/baseq3/vm/" || true; \
	done
endif

run-game: $(_RUN_GAME_DEP) $(_RUN_VM_COPY)
ifeq ($(UNAME_S),Darwin)
	"$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)" $(_RUN_GAME_ARGS)
else
	"$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" $(_RUN_GAME_ARGS)
endif

# Compose dedicated-server args (dedicated 2 = no client, VM + map as run-game)
_RUN_DED_ARGS := +set dedicated 2 $(_RUN_VM_ARGS)
ifeq ($(DEV),1)
_RUN_DED_ARGS += +set sv_cheats 1
endif
ifneq ($(MAP),)
_RUN_DED_ARGS += +map $(MAP)
endif
ifneq ($(EXTRA_ARGS),)
_RUN_DED_ARGS += $(EXTRA_ARGS)
endif

run-ded: $(_RUN_GAME_DEP)
ifeq ($(UNAME_S),Darwin)
	"$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-ded" $(_RUN_DED_ARGS)
else
	"$(Q3DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)" $(_RUN_DED_ARGS)
endif


# ── release ──────────────────────────────────────────────────────────────────
# Verify outputs, build launcher + engine + paks, assemble .app, codesign,
# package DMG (macOS) or tar.gz (Linux).

release: check create-launcher copy-all bundle-codesign
ifeq ($(UNAME_S),Darwin)
	$(MAKE) bundle-dmg
else ifdef IS_WINDOWS
	$(MAKE) bundle-zip
else
	$(MAKE) bundle-tar
endif
	@echo ""
	@echo "  ┌─────────────────────────────────────┐"
	@echo "  │  Wired release ready                 │"
	@echo "  ├─────────────────────────────────────┤"
ifeq ($(UNAME_S),Darwin)
	@echo "  │  DMG: $(DMG_OUT)"
	@echo "  │  Size: $$(du -h "$(DMG_OUT)" | cut -f1)"
else ifdef IS_WINDOWS
	@echo "  │  ZIP: $(ZIP_OUT)"
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

check: create-packs
	@echo "==> Verifying build..."
	@ls $(MODULE_DIR)/vm/cgame.wasm  > /dev/null 2>&1 && echo "  cgame VM:    OK" || echo "  cgame VM:    MISSING (wasi-sdk not found?)"
	@ls $(MODULE_DIR)/vm/qagame.wasm > /dev/null 2>&1 && echo "  qagame VM:   OK" || echo "  qagame VM:   MISSING (wasi-sdk not found?)"
	@ls $(MODULE_DIR)/cgame$(GAME_ARCH).*    > /dev/null 2>&1 && echo "  cgame native: OK" || echo "  cgame native: MISSING"
	@ls $(MODULE_DIR)/qagame$(GAME_ARCH).*  > /dev/null 2>&1 && echo "  qagame native: OK" || echo "  qagame native: MISSING"
	@test -f $(PAK_OUT) && echo "  mod pack:     OK"
ifeq ($(UNAME_S),Darwin)
	@codesign --verify "$(Q3DIR)" 2>/dev/null && echo "  codesign:    OK" || echo "  codesign:    MISSING (run make bundle-codesign)"
	@codesign -d --entitlements - "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)" 2>/dev/null | grep -q "allow-jit" && echo "  JIT entitlement: OK" || echo "  JIT entitlement: MISSING"
endif
	@echo "==> All checks passed."

# ── smoke ────────────────────────────────────────────────────────────────────
# Headless gameplay smoke test. Requires Q3DIR with base game pack.

smoke: build
ifeq ($(UNAME_S),Darwin)
	Q3DIR="$(Q3DIR)" tests/smoke.sh "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-ded"
else
	Q3DIR="$(Q3DIR)" tests/smoke.sh "$(Q3DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)"
endif

# ── QUIC game transport smoke test ───────────────────────────────────────────

test-quic-game:
	@bash tests/smoke-quic-game.sh

# ── VM smoke test ────────────────────────────────────────────────────────────

test-vm:
	@bash tests/smoke-wasm.sh

# ── FS dedup smoke test ─────────────────────────────────────────────────────
# Regression test for FS_DeduplicateArchives SW3Z double-free.
# Builds two same-basename SW3Z archives in fixture basepath/ + homepath/,
# launches wired-ded with explicit fs_installpath / fs_homepath, asserts clean
# exit and that the dedup code path actually fired.

test-fs-dedup: build $(SW3Z_BIN)
	@bash tests/smoke-fs-dedup.sh "$(BUILD_DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)"

# ── bench ────────────────────────────────────────────────────────────────────
# Timedemo benchmark. Requires a demo at Q3DATADIR/demos/<DEMO>.dm_68.

DEMO ?= four

bench: copy-all
	@if [ ! -f "$(Q3DATADIR)/demos/$(DEMO).dm_68" ]; then \
	  echo "ERROR: $(Q3DATADIR)/demos/$(DEMO).dm_68 not found"; \
	  echo "Copy a demo file (.dm_68) to $(Q3DATADIR)/demos/ and set DEMO=<name>"; \
	  exit 1; \
	fi
ifeq ($(UNAME_S),Darwin)
	open "$(Q3DIR)" --args +timedemo 1 +demo $(DEMO)
else
	"$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" +timedemo 1 +demo $(DEMO)
endif

# ── diff-api ─────────────────────────────────────────────────────────────────
# Diffs Wired's game API headers against upstream Quake3e at the fork point.

UPSTREAM_REF ?= ecd5fa41

diff-api:
	@echo "==> API header diff (wired vs upstream $(UPSTREAM_REF))"
	@echo "--- g_public.h ---"
	@git diff $(UPSTREAM_REF) -- code/game/g_public.h
	@echo "--- cg_public.h ---"
	@git diff $(UPSTREAM_REF) -- code/cgame/cg_public.h

# ── lint ─────────────────────────────────────────────────────────────────────
# Static analysis via clang-tidy.  Requires a compile_commands.json — run
# "make configure" first if the build directory doesn't exist yet.
# Override LINT_JOBS to control parallelism (default: CPU count).
# Override LINT_SRCS to lint specific files: make lint LINT_SRCS="code/server/sv_main.c"

CLANG_TIDY      ?= clang-tidy
RUN_CLANG_TIDY  ?= $(shell command -v run-clang-tidy 2>/dev/null || echo /opt/homebrew/opt/llvm/bin/run-clang-tidy)
LINT_JOBS       ?= $(JOBS)
LINT_SRCS       ?=

# macOS: Homebrew clang-tidy needs the SDK sysroot to find system headers
ifeq ($(UNAME_S),Darwin)
  LINT_EXTRA_ARGS := --extra-arg=-isysroot$(shell xcrun --show-sdk-path)
else
  LINT_EXTRA_ARGS :=
endif

lint: $(BUILD_DIR)/CMakeCache.txt
	@if [ ! -f "$(BUILD_DIR)/compile_commands.json" ]; then \
	  echo "ERROR: compile_commands.json not found — run 'make configure' first"; \
	  exit 1; \
	fi
	@if [ -n "$(LINT_SRCS)" ]; then \
	  echo "==> clang-tidy ($(LINT_SRCS))"; \
	  $(CLANG_TIDY) -p $(BUILD_DIR) $(LINT_EXTRA_ARGS) $(LINT_SRCS); \
	elif [ -x "$(RUN_CLANG_TIDY)" ]; then \
	  echo "==> run-clang-tidy -j$(LINT_JOBS) (all project sources)"; \
	  $(RUN_CLANG_TIDY) -p $(BUILD_DIR) -j $(LINT_JOBS) \
	    -clang-tidy-binary $(CLANG_TIDY) \
	    -extra-arg='-isysroot$(shell xcrun --show-sdk-path 2>/dev/null)' \
	    -header-filter='^code/.*' \
	    'code/.*\.c$$'; \
	else \
	  echo "==> clang-tidy (sequential — install run-clang-tidy for parallel)"; \
	  find code -name '*.c' -not -path '*/asm/*' | sort | while read -r f; do \
	    echo "  $$f"; \
	    $(CLANG_TIDY) -p $(BUILD_DIR) $(LINT_EXTRA_ARGS) "$$f" || true; \
	  done; \
	fi

# ══════════════════════════════════════════════════════════════════════════════
# HELP
# ══════════════════════════════════════════════════════════════════════════════

help:
	@echo ""
	@echo "  Wired build targets"
	@echo "  ───────────────────────────────────────────────────────────"
	@echo "  make                 configure + build Release"
	@echo "  make clean           remove build-release/ + build-debug/"
	@echo "  make clean-launcher  remove launcher/build/ (wails output)"
	@echo "  make clean-all       clean + clean-launcher"
	@echo "  make rebuild         clean + build"
	@echo ""
	@echo "  Generation:"
	@echo "    make create-launcher   build launcher AND populate launcher/build/bin/"
	@echo "                            (engine, ded, renderers, deps, modules, pack)"
	@echo "                            — self-contained; runs without copy-build"
	@echo "    make create-packs      package modfiles/ + VM modules → .sw3z"
	@echo ""
	@echo "  Copy (assemble .app — pass DEV=1 to use Debug build):"
	@echo "    make copy-build         engine + dylibs → .app"
	@echo "    make copy-libs          renderer + deps → .app"
	@echo "    make copy-packs          mod pack (.sw3z) → .app"
	@echo "    make copy-all           all of the above"
	@echo ""
	@echo "  Bundling:"
	@echo "    make bundle-codesign    codesign .app (macOS)"
	@echo "    make bundle-dmg         create versioned DMG (macOS)"
	@echo "    make bundle-tar         create versioned tar.gz (Linux)"
	@echo "    make bundle-zip         create versioned zip (Windows)"
	@echo ""
	@echo "  Flows:"
	@echo "    make run-launcher       build + assemble + codesign + open launcher"
	@echo "    make run-game                     run engine (main menu)"
	@echo "    make run-game DEV=1               debug build"
	@echo "    make run-game DEV=1 MAP=arena7    map with debug"
	@echo "    make run-game VM=1 MAP=arena7     VM modules + map"
	@echo "    make run-game DEV=1 EXTRA_ARGS=\"+set bsp_q1_coverage_debug 1\"  extra cvars"
	@echo "    make run-ded                      run dedicated server"
	@echo "    make run-ded DEV=1 MAP=arena7     dedicated + debug + map"
	@echo "    make release            build + assemble + codesign + package"
	@echo ""
	@echo "  Testing:"
	@echo "    make check              verify all outputs present"
	@echo "    make smoke              headless gameplay test"
	@echo "    make test-quic-game     QUIC game transport smoke test"
	@echo "    make test-fs-dedup      FS dedup SW3Z double-free regression test"
	@echo "    make bench              timedemo benchmark"
	@echo "    make diff-api           diff API headers vs upstream"
	@echo "    make lint               static analysis via clang-tidy"
	@echo "    make lint LINT_SRCS=f   lint specific file(s)"
	@echo ""
	@echo "  Variables:"
	@echo "    Q3DIR=$(Q3DIR)"
	@echo "    JOBS=$(JOBS)   MAP=$(MAP)   DEV=$(DEV)   VM=$(VM)   DEMO=$(DEMO)   EXTRA_ARGS=$(EXTRA_ARGS)"
	@echo "    USE_WASM=$(USE_WASM)   CODESIGN_IDENTITY=$(CODESIGN_IDENTITY)"
	@echo ""
