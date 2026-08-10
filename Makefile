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
#   make run-headless                 run headless (no-GUI) server
#   make run-headless DEV=1           headless + debug build
#   make run-headless DEV=1 MAP=arena7  headless + debug + load map
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
NAV_UPDATE_GOLDEN ?= 0    # 1 = re-bless the nav-gate golden (make nav-gate)

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
  #   build/<cfg>/$(APP_NAME)$(BINEXT).app/Contents/MacOS/{wired$(BINEXT), wired-headless$(BINEXT)}
  # Bundle directory is product+channel branded; engine binaries inside keep
  # their wired/wired-headless names. See "Combined macOS bundle assembly" in CMakeLists.txt.
  BUILT_APP  := $(BUILD_DIR)/$(APP_NAME)$(BINEXT).app
  ENGINE_BIN := $(BUILT_APP)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)
  BUILT_DED  := $(BUILT_APP)/Contents/MacOS/$(CMAKE_APP_NAME)-headless$(BINEXT)
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

# cmake puts game modules at <build-dir>/<build-cfg>/base/
MODULE_DIR := $(BUILD_DIR)/$(BUILD_CFG)/base

ifeq ($(UNAME_S),Darwin)
  # macOS bundle conventions: code in Contents/MacOS/, data in Contents/Resources/.
  Q3BINDIR   := $(Q3DIR)/Contents/MacOS
  Q3DATADIR  := $(Q3DIR)/Contents/Resources/base
else
  Q3BINDIR   := $(Q3DIR)
  Q3DATADIR  := $(Q3DIR)/base
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

CMAKE_EXTRA_FLAGS ?=
CMAKE_CHANNEL_FLAG := -DCHANNEL_SUFFIX="$(CHANNEL_SUFFIX)"
CMAKE_PRODUCT_FLAG := -DPRODUCT_NAME="$(PRODUCT_NAME)"
CMAKE_CONFIGURE    := cmake -S . -B $(BUILD_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=$(BUILD_CFG) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(CMAKE_WASM_FLAG) $(CMAKE_CHANNEL_FLAG) $(CMAKE_PRODUCT_FLAG) $(CMAKE_EXTRA_FLAGS)
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
  # colocates there. Game modules + paks go under Contents/Resources/base
  # because post-engine-Phase-1 the basegame scan reaches only the resource
  # path on macOS (mirrors the Q3DATADIR collapse for the install layout).
  # Bundle name is the GAME (q3now) per Wired-vs-game branding boundary —
  # the engine binaries (wired.x64, wired_<api>_<arch>.dylib) live INSIDE the
  # game's .app wrapper. Wails emits this bundle name from launcher/wails.json
  # ("name": "q3now"); align consumer paths here so Linux / Windows / macOS
  # all key off the same launcher artifact.
  LAUNCHER_DSTBIN  := $(LAUNCHER_DIR)/build/bin/q3now.app/Contents/MacOS
  LAUNCHER_DSTDATA := $(LAUNCHER_DIR)/build/bin/q3now.app/Contents/Resources/base
else ifdef IS_WINDOWS
  LAUNCHER_BIN     := $(LAUNCHER_DIR)/build/bin/q3now-launcher.exe
  LAUNCHER_DSTBIN  := $(LAUNCHER_DIR)/build/bin
  LAUNCHER_DSTDATA := $(LAUNCHER_DIR)/build/bin/base
else
  LAUNCHER_BIN     := $(LAUNCHER_DIR)/build/bin/q3now-launcher
  LAUNCHER_DSTBIN  := $(LAUNCHER_DIR)/build/bin
  LAUNCHER_DSTDATA := $(LAUNCHER_DIR)/build/bin/base
endif
WAILS_TAGS   ?=

# SW3Z archiver
SW3Z_DIR := tools/sw3z-archiver
SW3Z_BIN := $(SW3Z_DIR)/cmd/sw3z/sw3z

# VM backend toggle (moved to top, before ifeq)
# USE_WASM is defined near other cmake flags above

# Pak output (always from Release build — VM modules are always Release)
PAK_STAGING := $(BUILD_DIR)/pak-staging
PAK_OUT := $(BUILD_DIR)/base/pax21.sw3z

# ── Phony targets ─────────────────────────────────────────────────────────────

.PHONY: all configure build _build-stamp clean clean-launcher clean-all rebuild shaders \
        create-launcher create-packs build-fonts \
        _wails-build \
        copy-libs copy-build copy-packs copy-all \
        bundle-codesign bundle-dmg bundle-tar bundle-zip bundle-docker \
        run-launcher run-game run-headless release \
        check smoke test-vm test-quic-game test-fs-dedup bench diff-api lint help

# Default target: a CONSISTENT DEPLOYABLE WORLD, not just compiled objects.
# `build` compiles the engine + native game DLLs + the WASM VM modules (the
# gamecl_wasm/gamesv_wasm custom targets are ALL-targets, so they rebuild
# whenever their sources change) into $(MODULE_DIR)/vm — but leaves the
# deployed mod pack ($(PAK_OUT), i.e. build/<cfg>/base/pax21.sw3z) untouched.
# A bare `make build` therefore ships a FRESH engine over a STALE pak: the
# server loads gamesv.wasm FROM the pak (sv_pure + vm_game=2), so game-side
# edits silently do not take effect and measurements interrogate a stale
# witness. `create-packs` (which chains `build`) stages the fresh VM modules
# and repacks the pak, so making it the default guarantees one `make`
# invocation leaves engine + pak in lockstep. Compile-only callers still use
# `make build` explicitly (create-packs, copy-*, run-* all depend on `build`,
# not on `all`, so this does not make them repack twice).
all: create-packs

# ══════════════════════════════════════════════════════════════════════════════
# FOUNDATION — configure, build, clean
# ══════════════════════════════════════════════════════════════════════════════

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE_CONFIGURE)

configure: $(BUILD_DIR)/CMakeCache.txt

build: _build-stamp $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE_BUILD)
	# Native game modules must rebuild with the engine so the shared viewport
	# ABI (qcommon/wired/ui_viewport_types.h, consumed by both the engine
	# client and these DLLs) cannot diverge between binaries. Building them
	# explicitly after the full build guarantees the gamecl/gamesv DLLs are
	# from the same header revision as the engine — a layout mismatch reads
	# the provider struct at the wrong offsets and crashes.
	cmake --build $(BUILD_DIR) --target gamecl_base gamesv_base
	# Deploy the freshly-compiled VM modules into the pak so a bare `make build`
	# leaves engine + pak in lockstep — the server loads gamesv.wasm FROM the pak
	# (sv_pure + vm_game=2), so a build that refreshes the WASM object but not the
	# pak silently ships a stale game module and every measurement interrogates a
	# stale witness (the 2026-07-23/24 stale-pak trap, twice).  $(PAK_OUT) is a
	# file target keyed on the VM modules, so this repacks ONLY when they changed
	# (idempotent) and, being the same target create-packs uses, never double-packs.
	$(MAKE) $(PAK_OUT)

# ── per-build stamp (content-gated counter + timestamp) ───────────────────────
# Bump a monotonic build counter (.build_number at the REPO ROOT so `make clean`
# — which rm -rf's $(BUILD_DIR) — does NOT reset it) + a compile timestamp, and
# write both into a GENERATED HEADER included directly by the few TUs that embed
# the stamp (via wired/wired_build_stamp.h; NOT via q_shared.h — routing it
# through q_shared.h made 660/1232 TUs stamp-dependent and turned every no-op
# `make` into a ~6s full-tree churn). The header route (not -D) is deliberate:
# it refreshes with NO cmake reconfigure and sidesteps the windres/Ninja
# spaced-string -D escaping trap (the value is plain C source).
#
# CONTENT-GATED: the counter bumps only when the tree's content signature
# (HEAD + tracked diff + status) changed since the last stamp. Two builds of
# identical inputs share one id — which is what makes the id a usable
# staleness signal (`sysinfo` comparisons) rather than an invocation counter.
# It also makes a no-op `make` a true no-op: nothing recompiles, and the baked
# .nav caches (whose identity hash folds WIRED_BUILD_ID in — nav_cache.c)
# survive across builds that changed nothing. Untracked-file CONTENT edits are
# outside the signature (their names appear in `git status`, their bytes do
# not) — acceptable: untracked sources don't reach the build without a
# reconfigure anyway.
WIRED_BUILD_STAMP_HDR := code/qcommon/wired/wired_build_stamp_gen.h
_build-stamp:
	@sig=$$( { git rev-parse HEAD; git diff HEAD; git status --porcelain; } 2>/dev/null \
	         | git hash-object --stdin 2>/dev/null || date +%s ); \
	 old=$$(cat .build_signature 2>/dev/null || true); \
	 if [ "$$sig" != "$$old" ] || [ ! -f $(WIRED_BUILD_STAMP_HDR) ]; then \
	   n=$$(cat .build_number 2>/dev/null || echo 0); \
	   n=$$((n + 1)); \
	   echo $$n > .build_number; \
	   printf '%s\n' "$$sig" > .build_signature; \
	   ts=$$(date +'%Y-%m-%d-%H-%M'); \
	   echo "==> build #$$n ($$ts)"; \
	   { echo "// Generated by 'make' (_build-stamp). DO NOT EDIT; not committed."; \
	     echo "#define WIRED_BUILD_ID $$n"; \
	     echo "#define WIRED_BUILD_DATE \"$$ts\""; } > $(WIRED_BUILD_STAMP_HDR); \
	 fi

clean:
	rm -rf $(BUILD_DIR)

# clean-launcher: nuke wails build output. Preserves frontend/node_modules
# and Go module cache (rebuilding those takes minutes).
clean-launcher:
	rm -rf $(LAUNCHER_DIR)/build

clean-all: clean clean-launcher

# Regenerate the committed shader SPIR-V (code/renderervk/shaders/spirv/shader_data.c)
# from the GLSL sources via compile.mjs. The build COMPILES this file but never
# regenerates it, so a GLSL/manifest change without a re-run ships stale bytecode.
# Best-effort: if node/glslang is absent the committed file is kept so SDK-less /
# CI builds still succeed (the committed shader_data.c is in-tree).
shaders:
	@if command -v node >/dev/null 2>&1; then \
	  echo "==> Regenerating shaders (compile.mjs)..."; \
	  node code/renderervk/shaders/compile.mjs \
	    || echo "WARNING: shader regen failed — keeping committed shader_data.c"; \
	else \
	  echo "WARNING: node not found — skipping shader regen (committed shader_data.c)"; \
	fi

# rebuild: from-scratch. clean build dir + regenerate shaders + full staged build
# (copy-all = cmake build + paks + libs into the runnable Q3DIR), NO launch.
# shaders runs as a prerequisite (before any cmake build), so the recipe's
# copy-all compiles a freshly regenerated shader_data.c. The clean only removes
# $(BUILD_DIR); shader_data.c lives in the source tree and survives it.
rebuild: clean shaders
	$(MAKE) copy-all

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

# The VM modules the pak SHIPS.  Keying $(PAK_OUT) on these files (not on the
# `build` phony) is what makes the repack re-fire whenever the game WASM changes
# — and, crucially, NON-cyclic: the pak depends on the compiled artifacts, the
# compile (build) triggers the pak, and no edge points back from the artifacts to
# `build`.  These are the exact files create-packs copies into the pak below, so a
# stale pak over a fresh WASM (the 2026-07-23/24 stale-pak trap) is now a rebuilt
# prerequisite, not a discipline the caller must remember.
PAK_VM_MODULES := $(MODULE_DIR)/vm/gamesv.wasm $(MODULE_DIR)/vm/gamecl.wasm
# All packed source content: any modfiles/ edit also invalidates the pak.
PAK_CONTENT_SRC := $(shell find modfiles -type f 2>/dev/null)

# $(PAK_OUT) — the deployable mod pack as a FILE target.  It repacks iff any of
# its inputs (the just-compiled VM modules, the packed modfiles, or the archiver)
# is newer than the pak.  `build` runs this after compiling (see the build recipe)
# so `make build` alone leaves engine + pak in lockstep; create-packs / copy-* /
# run-* reach the same file target, and Make de-duplicates it within one
# invocation, so `make all` (build -> pak, then create-packs -> pak-already-fresh)
# repacks EXACTLY once — no double-repack, no cycle.
$(PAK_OUT): $(PAK_VM_MODULES) $(PAK_CONTENT_SRC) $(SW3Z_BIN)
	@echo "==> Staging pak contents..."
	rm -rf $(PAK_STAGING)
	mkdir -p $(PAK_STAGING) $(BUILD_DIR)/base
	cp -R modfiles/. $(PAK_STAGING)/
	@echo "==> Copying VM modules into pak..."
	# WASM VM modules (gamecl.wasm / gamesv.wasm) follow DEV via $(MODULE_DIR),
	# the same tree the engine + native game DLLs deploy from — so the packed
	# WASM is always the config's just-compiled module (the prerequisite above
	# guarantees this rule re-fires when it changes).
	cp -R $(MODULE_DIR)/vm $(PAK_STAGING)/
	@echo "==> Stamping version..."
	echo "$(APP_NAME) $$(git describe --always --dirty) ($$(date +%Y-%m-%d))" > $(PAK_STAGING)/description.txt
	@echo "==> Creating $(PAK_OUT)..."
	$(SW3Z_BIN) a -x "**/.DS_Store" -x ".DS_Store" "$(PAK_OUT)" $(PAK_STAGING)
	@echo "==> $(PAK_OUT) ready"

# create-packs — public alias for "produce a fresh deployable pak".  Depends on
# `build` (so the VM modules are compiled first) and on the $(PAK_OUT) file target
# (which repacks iff the modules/content changed).  Because build's own recipe
# already brings $(PAK_OUT) current, the pak is up-to-date by the time Make
# evaluates it here, so this adds no second repack.
create-packs: build $(PAK_OUT)
	@:

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

# ── wui-icons-atlas ───────────────────────────────────────────────────────────
# Phase 3e — assemble TTF from SVG icon sources and run msdf-atlas-gen against
# it. tools/msdf/build_wui_icons.py does the SVG → TTF → atlas pipeline in
# one step; the Makefile target is the modder-facing entry point.
#
# Sources : assets/_sources/fonts/wui-icons/<name>.svg (8 canonical icons,
#           PUA-mapped per docs/wui-authoring-guide.md §5.3)
# Outputs : modfiles/fonts/wui_icons.png + modfiles/fonts/wui_icons.json
#           assets/fonts/wui_icons.ttf + assets/fonts/wui_icons_charset.txt
#
# Dependencies: $(MSDF_ATLAS_GEN) (vendored submodule, built into
# tools/msdf-atlas-gen/build/bin/) and fontTools in the Python env (provides
# cu2qu, fontBuilder, svgLib).

wui-icons-atlas: $(MSDF_ATLAS_GEN)
	@command -v python3 >/dev/null 2>&1 || command -v python >/dev/null 2>&1 \
	  || { echo "ERROR: python not found"; exit 1; }
	@(python3 -c "import fontTools" 2>/dev/null \
	  || python -c "import fontTools" 2>/dev/null) \
	  || { echo "ERROR: fontTools missing (pip install fonttools[ufo])"; exit 1; }
	@echo "==> Building wui_icons atlas from SVG sources..."
	@if command -v python3 >/dev/null 2>&1; then \
	  python3 tools/msdf/build_wui_icons.py; \
	else \
	  python tools/msdf/build_wui_icons.py; \
	fi
	@echo "==> wui_icons atlas ready: modfiles/fonts/wui_icons.{png,json}"

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
	@test -f "$(BUILT_DED)" && cp "$(BUILT_DED)" "$(1)/$(CMAKE_APP_NAME)-headless" || true
	cp "$(BUILD_DIR)/$(BUILD_CFG)/base/gamecl$(_GAME_MODULE_EXT)"  "$(2)/"
	cp "$(BUILD_DIR)/$(BUILD_CFG)/base/gamesv$(_GAME_MODULE_EXT)" "$(2)/"
endef
else
define install_engine
	@echo "==> Installing engine + game modules into $(1) ..."
	@mkdir -p "$(1)" "$(2)"
	cp "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" "$(1)/"
	@test -f "$(BUILD_DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)" && \
	  cp "$(BUILD_DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)" "$(1)/" || true
	cp "$(BUILD_DIR)/$(BUILD_CFG)/base/gamecl$(_GAME_MODULE_EXT)"  "$(2)/"
	cp "$(BUILD_DIR)/$(BUILD_CFG)/base/gamesv$(_GAME_MODULE_EXT)" "$(2)/"
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
	@# libcrypto is in this list because picoquic's TLS links it: without the
	@# copy+rewrite the shipped bundle references the absolute Homebrew path
	@# and cannot launch on a machine without `brew install openssl@3`.
	@for LIBNAME in libSDL3 libMoltenVK libcrypto; do \
	  DYLIB=$$(find /opt/homebrew/lib /opt/homebrew/opt/*/lib /usr/local/lib \
	    2>/dev/null -name "$$LIBNAME*.dylib" -not -type l -maxdepth 1 | head -1); \
	  if [ -n "$$DYLIB" ]; then \
	    BASENAME=$$(basename "$$DYLIB"); \
	    echo "  $$LIBNAME: $$DYLIB → $$BASENAME"; \
	    rm -f "$(1)/$$BASENAME"; \
	    cp "$$DYLIB" "$(1)/"; \
	    for BIN in "$(1)/$(CMAKE_APP_NAME)$(BINEXT)" \
	              "$(1)/$(CMAKE_APP_NAME)-headless" \
	              "$(1)/$(CMAKE_APP_NAME)-headless$(BINEXT)" \
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
	@test -f "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-headless" && \
	  codesign --force --options runtime --entitlements misc/macos/wired.entitlements \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-headless" || true
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
	mkdir -p $(TAR_STAGING)/base
	cp "$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)" "$(TAR_STAGING)/"
	@test -f "$(Q3DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)" && \
	  cp "$(Q3DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)" "$(TAR_STAGING)/" || true
	cp "$(LAUNCHER_BIN)" "$(TAR_STAGING)/q3now-launcher" 2>/dev/null || true
	cp -R "$(Q3DATADIR)/." "$(TAR_STAGING)/base/"
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
	mkdir -p $(ZIP_STAGING)/base
	cp "$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" "$(ZIP_STAGING)/"
	@test -f "$(Q3DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)" && \
	  cp "$(Q3DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)" "$(ZIP_STAGING)/" || true
	cp "$(Q3DIR)/q3now-launcher.exe" "$(ZIP_STAGING)/" 2>/dev/null || true
	@for dll in "$(Q3DIR)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).dll" \
	            "$(Q3DIR)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dll"; do \
	  [ -f "$$dll" ] && cp "$$dll" "$(ZIP_STAGING)/" || true; \
	done
	cp -R "$(Q3DATADIR)/." "$(ZIP_STAGING)/base/"
	cp README.md "$(ZIP_STAGING)/"
	cd $(ZIP_STAGING) && powershell -Command "Compress-Archive -Path '*' -DestinationPath '$(CURDIR)/$(ZIP_OUT)' -Force"
	rm -rf $(ZIP_STAGING)
	@echo "==> $(ZIP_OUT) ready"
else
	@echo "zip packaging is for Windows — use 'make bundle-dmg' on macOS or 'make bundle-tar' on Linux"
endif

# ── bundle-docker ────────────────────────────────────────────────────────
# Builds a Docker image for the headless server (Linux x86_64).

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
	@mkdir -p "$(Q3DIR)/Contents/Resources/base/vm"
	@for f in $(BUILD_DIR)/$(BUILD_CFG)/base/vm/*.wasm $(BUILD_DIR)/$(BUILD_CFG)/base/vm/*.aot; do \
		[ -f "$$f" ] && cp "$$f" "$(Q3DIR)/Contents/Resources/base/vm/" || true; \
	done
else
	@mkdir -p "$(Q3DIR)/base/vm"
	@for f in $(BUILD_DIR)/$(BUILD_CFG)/base/vm/*.wasm $(BUILD_DIR)/$(BUILD_CFG)/base/vm/*.aot; do \
		[ -f "$$f" ] && cp "$$f" "$(Q3DIR)/base/vm/" || true; \
	done
endif

run-game: $(_RUN_GAME_DEP) $(_RUN_VM_COPY)
ifeq ($(UNAME_S),Darwin)
	"$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)" $(_RUN_GAME_ARGS)
else
	"$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" $(_RUN_GAME_ARGS)
endif

# Compose headless-server args. The wired-headless binary is a no-GUI build
# (compiled with HEADLESS), so it boots straight into the server — no 'dedicated'
# cvar (retired); listing policy is sv_hostListed, set separately if desired.
_RUN_HEADLESS_ARGS := $(_RUN_VM_ARGS)
ifeq ($(DEV),1)
_RUN_HEADLESS_ARGS += +set sv_cheats 1
endif
ifneq ($(MAP),)
_RUN_HEADLESS_ARGS += +map $(MAP)
endif
ifneq ($(EXTRA_ARGS),)
_RUN_HEADLESS_ARGS += $(EXTRA_ARGS)
endif

run-headless: $(_RUN_GAME_DEP)
ifeq ($(UNAME_S),Darwin)
	"$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-headless" $(_RUN_HEADLESS_ARGS)
else
	"$(Q3DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)" $(_RUN_HEADLESS_ARGS)
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

# check — verify the build produced everything it must ship. This is a REAL
# gate: any missing artifact exits nonzero (it used to be `A && echo OK ||
# echo MISSING`, which can never fail — `make release` ran with no working
# verification at all). wasi-sdk absence therefore fails loudly here by
# decision (2026-08-10): the pak always ships the WASM modules; development
# without wasi-sdk is not a supported configuration.
# The two codesign probes stay ADVISORY (no fail): `release` runs check
# BEFORE bundle-codesign, so failing on an unsigned tree would deadlock the
# release flow; bundle-codesign itself errors if signing fails.
check: create-packs
	@fail=0; \
	echo "==> Verifying build..."; \
	if ls $(MODULE_DIR)/vm/gamecl.wasm  > /dev/null 2>&1; then echo "  gamecl VM:    OK"; else echo "  gamecl VM:    MISSING (wasi-sdk not found?)"; fail=1; fi; \
	if ls $(MODULE_DIR)/vm/gamesv.wasm > /dev/null 2>&1; then echo "  gamesv VM:   OK"; else echo "  gamesv VM:   MISSING (wasi-sdk not found?)"; fail=1; fi; \
	if ls $(MODULE_DIR)/gamecl$(GAME_ARCH).*    > /dev/null 2>&1; then echo "  gamecl native: OK"; else echo "  gamecl native: MISSING"; fail=1; fi; \
	if ls $(MODULE_DIR)/gamesv$(GAME_ARCH).*  > /dev/null 2>&1; then echo "  gamesv native: OK"; else echo "  gamesv native: MISSING"; fail=1; fi; \
	if test -f $(PAK_OUT); then echo "  mod pack:     OK"; else echo "  mod pack:     MISSING"; fail=1; fi; \
	if [ "$(UNAME_S)" = "Darwin" ]; then \
	  codesign --verify "$(Q3DIR)" 2>/dev/null && echo "  codesign:    OK" || echo "  codesign:    MISSING (run make bundle-codesign; advisory)"; \
	  codesign -d --entitlements - "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)" 2>/dev/null | grep -q "allow-jit" && echo "  JIT entitlement: OK" || echo "  JIT entitlement: MISSING (advisory)"; \
	fi; \
	if [ $$fail -ne 0 ]; then echo "==> Verification FAILED"; exit 1; fi; \
	echo "==> All checks passed."

# ── smoke ────────────────────────────────────────────────────────────────────
# Headless gameplay smoke test. Requires Q3DIR with base game pack.

smoke: build
ifeq ($(UNAME_S),Darwin)
	Q3DIR="$(Q3DIR)" tests/smoke.sh "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-headless"
else
	Q3DIR="$(Q3DIR)" tests/smoke.sh "$(Q3DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)"
endif

# ── nav-gate ─────────────────────────────────────────────────────────────────
# Deterministic bot-navigation regression gate. Pins the RNG seed (sv_seed) so
# the bot spawns reproducibly, then asserts the nav progresses AND the [BOTNAV]
# trace matches a committed golden. Runs the DEV headless against Q3DIR content
# (maps), like the smoke test. NAV_UPDATE_GOLDEN=1 re-blesses after an intended
# nav change.

nav-gate: build
ifeq ($(UNAME_S),Darwin)
	Q3DIR="$(Q3DIR)" NAV_UPDATE_GOLDEN="$(NAV_UPDATE_GOLDEN)" tests/nav-trace-gate.sh "$(BUILD_DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)"
else
	Q3DIR="$(Q3DIR)" NAV_UPDATE_GOLDEN="$(NAV_UPDATE_GOLDEN)" tests/nav-trace-gate.sh "$(BUILD_DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)"
endif

.PHONY: smoke nav-gate

# ── QUIC game transport smoke test ───────────────────────────────────────────

test-quic-game:
	@bash tests/smoke-quic-game.sh

# ── VM smoke test ────────────────────────────────────────────────────────────

test-vm:
	@bash tests/smoke-wasm.sh

# ── focus-driven audio mute test ────────────────────────────────────────────
# State-machine + flush verification for the window-focus audio mute: the mixer
# reads the focus-event mute state (not a gw_active poll) and flushes one-shots
# queued while unfocused. Includes a source-consistency guard against drift.

test-focus-mute:
	@mkdir -p $(BUILD_DIR)/tmp
	@bash tests/run-focus-mute-test.sh

# ── mouse-grab x audio-mute focus/console policy test ───────────────────────
# Four-state (focus x console) verification for the consolidated mouse-grab
# gate and the audio-mute decision: confine + play only when focused AND
# console-closed, including fullscreen single-monitor. Includes teeth (each
# missing term reintroduces the bug) and a source-consistency guard.

test-grab-mute:
	@mkdir -p $(BUILD_DIR)/tmp
	@bash tests/run-grab-mute-test.sh

# ── FS dedup smoke test ─────────────────────────────────────────────────────
# Regression test for FS_DeduplicateArchives SW3Z double-free.
# Builds two same-basename SW3Z archives in fixture basepath/ + homepath/,
# launches wired-headless with explicit fs_installpath / fs_homepath, asserts clean
# exit and that the dedup code path actually fired.

test-fs-dedup: build $(SW3Z_BIN)
	@bash tests/smoke-fs-dedup.sh "$(BUILD_DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)"

# ── visual regression gate (W-17) ───────────────────────────────────────────
# vdiff is a self-contained Go tool: pixel-diff with per-region thresholds.
# Baselines are rendered from the canonical React mockup via headless Chrome;
# implementation screenshots come from a scripted engine boot at the same
# native render resolution as the baseline (artboard design native is the
# source of truth; for V1_Monolith that is 1440x900 — see qw-variants.jsx).

ARTBOARD     ?= v1_monolith
MODE         ?= dark
ACCENT       ?= amber
VISUAL_CFGS  ?= dark:amber dark:cyan dark:toxic light:amber light:cyan

VDIFF_BIN    := tools/visual-diff/vdiff$(EXEEXT)
VCOMPARE_BIN := tools/visual-compare/vcompare$(EXEEXT)
PNG2RAW_BIN  := tools/png2raw/png2raw$(EXEEXT)
PNG_PERTURB_BIN := tools/png-perturb/png-perturb$(EXEEXT)

$(VDIFF_BIN):
	cd tools/visual-diff && go build -o vdiff$(EXEEXT) ./cmd/vdiff

$(PNG2RAW_BIN):
	cd tools/png2raw && go build -o png2raw$(EXEEXT) ./cmd/png2raw

# png-perturb — test-only golden defect synthesizer (brightness delta + --shift
# translate) used by the visual-gate self-tests to prove teeth.
$(PNG_PERTURB_BIN):
	cd tools/png-perturb && go build -o png-perturb$(EXEEXT) .

png2raw: $(PNG2RAW_BIN)
png-perturb: $(PNG_PERTURB_BIN)

$(VCOMPARE_BIN):
	cd tools/visual-compare && go build -o vcompare$(EXEEXT) ./cmd/vcompare

visual-baseline:
	@ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) \
	  bash tests/visual/scripts/regen_baseline.sh

visual-test: build $(VDIFF_BIN)
	@TS=$$(date +%Y%m%d_%H%M%S); \
	  ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) TIMESTAMP=$$TS \
	  ENGINE_EXE="$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" \
	  bash tests/visual/scripts/capture_impl.sh && \
	  ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) TIMESTAMP=$$TS \
	  bash tests/visual/scripts/compare.sh

# Aggregate gate: exit 0 only when every config in VISUAL_CFGS passes; exit 1
# on any failure. Per-config failures do not short-circuit the loop (we want
# the full matrix recorded) but they accumulate into the final exit code.
# Direct script invocation — no $(MAKE) recursion (dispatch 5's recursion +
# `|| true` masked every failure).
visual-test-all: build $(VDIFF_BIN)
	@fail=0; pass=0; total=0; first_fail=""; \
	ENGINE="$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"; \
	for cfg in $(VISUAL_CFGS); do \
	  m=$${cfg%%:*}; a=$${cfg##*:}; \
	  total=$$((total+1)); \
	  TS=$$(date +%Y%m%d_%H%M%S)_$$total; \
	  echo "==> [$$total] visual-test ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a"; \
	  if ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a TIMESTAMP=$$TS ENGINE_EXE="$$ENGINE" \
	       bash tests/visual/scripts/capture_impl.sh \
	     && ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a TIMESTAMP=$$TS \
	       bash tests/visual/scripts/compare.sh; then \
	    pass=$$((pass+1)); \
	  else \
	    fail=$$((fail+1)); \
	    [ -z "$$first_fail" ] && first_fail="$$m+$$a"; \
	  fi; \
	done; \
	echo "==> visual-test-all summary: $$pass/$$total passed, $$fail failed$${first_fail:+ (first fail: $$first_fail)}"; \
	[ "$$fail" -eq 0 ]

# ── visual perceptual + structural gate (W-17 dispatch 5b) ──────────────────
# vcompare is the active gate for V1_Monolith and future artboards. Runs
# SSIM + ΔE_00 + Zhang-Shasha tree edit distance, AND-combined verdict.
# vdiff retained as audit-trail tool, not invoked by these targets.
visual-compare: build $(VCOMPARE_BIN)
	@TS=$$(date +%Y%m%d_%H%M%S); \
	  ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) TIMESTAMP=$$TS \
	  ENGINE_EXE="$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" \
	  bash tests/visual/scripts/capture_impl.sh && \
	  ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) TIMESTAMP=$$TS \
	  bash tests/visual/scripts/vcompare_run.sh

visual-compare-all: build $(VCOMPARE_BIN)
	@fail=0; pass=0; total=0; first_fail=""; \
	ENGINE="$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"; \
	for cfg in $(VISUAL_CFGS); do \
	  m=$${cfg%%:*}; a=$${cfg##*:}; \
	  total=$$((total+1)); \
	  TS=$$(date +%Y%m%d_%H%M%S)_$$total; \
	  echo "==> [$$total] visual-compare ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a"; \
	  if ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a TIMESTAMP=$$TS ENGINE_EXE="$$ENGINE" \
	       bash tests/visual/scripts/capture_impl.sh \
	     && ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a TIMESTAMP=$$TS \
	       bash tests/visual/scripts/vcompare_run.sh; then \
	    pass=$$((pass+1)); \
	  else \
	    fail=$$((fail+1)); \
	    [ -z "$$first_fail" ] && first_fail="$$m+$$a"; \
	  fi; \
	done; \
	echo "==> visual-compare-all summary: $$pass/$$total passed, $$fail failed$${first_fail:+ (first fail: $$first_fail)}"; \
	[ "$$fail" -eq 0 ]

# ── Map transition smoke (Phase 7.4d-smoke-proof-logging) ───────────────────
# Loads arena1 -> arena17 -> arena1 in a single windowed wired process,
# gating each load with +waitForMap. Verifies CA_ACTIVE was reached three
# times with distinct first-gameplay-frame markers. Requires display.

smoke-map-transition: $(_RUN_GAME_DEP) $(PNG2RAW_BIN)
ifeq ($(UNAME_S),Darwin)
	@Q3DIR="$(Q3DIR)" bash tests/smoke-map-transition.sh \
		"$(BUILD_DIR)/$(APP_NAME)$(BINEXT).app/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)"
else
	@Q3DIR="$(Q3DIR)" bash tests/smoke-map-transition.sh \
		"$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"
endif

# ── render-feature visual gates (GTAO + Forward+) ────────────────────────────
# Fully-automated, ZERO-human-in-the-loop pixel gates for two render features the
# default golden gate does NOT cover (it runs a FEAT_SSAO=0 build, and never pixel-
# equivalence-gates r_forwardPlus). Each target runs end-to-end: (build →) capture →
# COMPUTED sanity assertions → bless-if-sane → cold re-verify → pass/fail. No eyeball.
#
#   visual-gtao    : builds the FEAT_SSAO=1 verify DLL (USE_SSAO=ON, separate build/
#                    ssao-verify dir), swaps it into the run dir, captures 5 viewpoints
#                    x {r_ssao 0 (==base golden), r_ssao 1, r_showAO 1 (AO-isolated)},
#                    and ASSERTS the AO field directly (open-surface ~1.0, corners <1.0
#                    by threshold, real spread). Restores the ship DLL on exit.
#   visual-fwdplus : ship build. r_forwardPlus 0 (PMLIGHT) vs 1 (tiled) equivalence at
#                    5 viewpoints, with a synthetic dlight (r_dlightShadowTest) exercising
#                    the lit path headless (a fired-light stand-in). W-59 tile-band, not
#                    byte-identical.
visual-gtao: $(_RUN_GAME_DEP) $(PNG2RAW_BIN)
	@bash tests/visual-feature-bless-verify.sh gtao \
		"$(BUILD_DIR)" build/ssao-verify

visual-fwdplus: $(_RUN_GAME_DEP) $(PNG2RAW_BIN)
	@bash tests/visual-feature-bless-verify.sh fwdplus \
		"$(BUILD_DIR)" build/ssao-verify

#   visual-viewport : ship build. VIEWPORT-PLACEMENT gate — a fixed camera (noclip +
#                     cmd setviewpos + timescale 0) on arena1, golden-blessed, gated by
#                     a tile-diff that catches a wrong-camera frame (geometry framed at
#                     the wrong screen location). The gate-LOGIC's teeth are proven
#                     deterministically by `visual-render-features.sh --mode selftest`
#                     (png-perturb --shift). Closes the visual-verify audit's residual
#                     caveat (value regressions were covered; placement was not).
visual-viewport: $(_RUN_GAME_DEP) $(PNG2RAW_BIN) $(PNG_PERTURB_BIN)
	@bash tests/visual-feature-bless-verify.sh viewport \
		"$(BUILD_DIR)" build/ssao-verify

#   visual-selftest : NO engine — proves every visual-render-features verdict primitive
#                     has teeth (GTAO AO-assert, value tiled_diff, viewport-placement),
#                     each clean-PASS + defect-FAIL. Fast, deterministic, CI-friendly.
visual-selftest: $(PNG2RAW_BIN) $(PNG_PERTURB_BIN)
	@bash tests/visual-render-features.sh --mode selftest

#   visual-shadow-atest : ship build. ALPHA-TESTED (cut-out) shadow-caster gate — proves
#                     arena1's alpha-tested casters cast a HOLED shadow whose cut-out
#                     follows the diffuse alpha. Blesses the deterministic holed render as
#                     a golden, then verifies a fresh holed frame reproduces it while a
#                     forced-solid render (r_shadowAtestTest 2) differs — the live teeth.
visual-shadow-atest: $(_RUN_GAME_DEP) $(PNG2RAW_BIN)
	@SMOKE_UPDATE_GOLDEN=1 bash tests/visual-render-features.sh --mode shadow_atest \
		--engine "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"
	@bash tests/visual-render-features.sh --mode shadow_atest \
		--engine "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"

#   visual-scene : ship build. CINEMATIC-SCENE gate — proves the cgame-side
#                     evaluator drives the on-screen view along a spline. Starts
#                     scripts/scene/arena1.lua via sceneplay, advances K frames at a
#                     fixed frame time (com_fixedtime), samples a deterministic
#                     mid-arc pose, blesses it as a golden (only when the frame is
#                     non-black real geometry AND moved off the player view), then
#                     cold-re-verifies a fresh frame reproduces it.
# sync-cgame-run-dir: place the freshly-built native game modules where the
# harness engine actually scans for them. The harness launches the engine from
# $(BUILD_DIR) (fs_installpath = build/debug), so FS_LoadLibrary scans
# build/debug/base/ — but CMake writes the modules to $(MODULE_DIR) =
# build/debug/$(BUILD_CFG)/base/. Those are different dirs, so build/debug/base/
# can hold a STALE (or missing) cgame that the harness would load instead of HEAD.
# Copy the fresh gamecl/gamesv modules into the scan dir so any cgame-dependent
# visual mode tests the current build. This is a BUILD-TREE sync (Debug/base ->
# base within build/debug), NOT a deploy to the install tree (that is copy-all /
# install_engine, which the harness never reads). The WASM vm modules are synced
# too so a vm_cgame 1 mode would also be fresh; harmless when a mode is native.
sync-cgame-run-dir: build
	@mkdir -p "$(BUILD_DIR)/base" "$(BUILD_DIR)/base/vm"
	cp "$(MODULE_DIR)/gamecl$(_GAME_MODULE_EXT)" "$(BUILD_DIR)/base/"
	cp "$(MODULE_DIR)/gamesv$(_GAME_MODULE_EXT)" "$(BUILD_DIR)/base/"
	@for f in "$(MODULE_DIR)/vm/gamecl.wasm" "$(MODULE_DIR)/vm/gamesv.wasm"; do \
		[ -f "$$f" ] && cp "$$f" "$(BUILD_DIR)/base/vm/" || true; \
	done

visual-scene: $(_RUN_GAME_DEP) sync-cgame-run-dir $(PNG2RAW_BIN)
	@SMOKE_UPDATE_GOLDEN=1 bash tests/visual-render-features.sh --mode scene \
		--engine "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"
	@bash tests/visual-render-features.sh --mode scene \
		--engine "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"

#   visual-chromatic : ship build. CHROMATIC-ABERRATION enabled-state gate — the
#                     effect (r_chromaticAberration) was dead-wiring (cvar + C spec-
#                     const emit, no shader consumer), and every other visual mode
#                     PINS it to 0, so nothing exercised the ON path. With the
#                     tonemap.frag consumer (a radial per-channel input sample) in
#                     place, this captures a fixed camera OFF (0) vs ON (0.5) and
#                     asserts the fringe is EDGE-concentrated (edges shift, centre
#                     ~unchanged) — the enabled-state golden chromatic never had.
#                     Blesses the ON frame, then cold-re-verifies. Gate-LOGIC teeth
#                     are proven engine-free by --mode selftest (1e).
visual-chromatic: $(_RUN_GAME_DEP) $(PNG2RAW_BIN)
	@SMOKE_UPDATE_GOLDEN=1 bash tests/visual-render-features.sh --mode chromatic \
		--engine "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"
	@bash tests/visual-render-features.sh --mode chromatic \
		--engine "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)"

.PHONY: visual-gtao visual-fwdplus visual-viewport visual-selftest visual-shadow-atest visual-scene visual-chromatic sync-cgame-run-dir

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
	@echo "    make run-headless                 run headless (no-GUI) server"
	@echo "    make run-headless DEV=1 MAP=arena7  headless + debug + map"
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
