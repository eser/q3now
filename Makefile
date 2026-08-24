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
#   USE_FOG_SYSTEM     enhanced fog compile gate (0=off, 1=on; default: 0)
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
#                    Used for build-output paths inside build/ and for
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

# One canonical build tree. DEV only selects CMAKE_BUILD_TYPE; switching it
# reconfigures the same tree instead of retaining parallel Debug/Release trees.
BUILD_DIR := build
ifeq ($(DEV),1)
  BUILD_CFG := Debug

  ifeq ($(MAKECMDGOALS),release)
  	$(error 'make release' requires DEV=0 (Release build))
  endif
else
  BUILD_CFG := Release
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
else ifeq ($(UNAME_M),riscv64)
  BINEXT  := .riscv64
  RENDEXT := _riscv64
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
  #   build/$(APP_NAME)$(BINEXT).app/Contents/MacOS/{wired$(BINEXT), wired-headless$(BINEXT)}
  # Bundle directory is product+channel branded; engine binaries inside keep
  # their wired/wired-headless names. See "Combined macOS bundle assembly" in CMakeLists.txt.
  BUILT_APP  := $(BUILD_DIR)/$(APP_NAME)$(BINEXT).app
  ENGINE_BIN := $(BUILT_APP)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)
  BUILT_DED  := $(BUILT_APP)/Contents/MacOS/$(CMAKE_APP_NAME)-headless$(BINEXT)
  Q3DIR       ?= /Applications/$(APP_NAME).app
else
  JOBS        ?= $(shell nproc 2>/dev/null || echo 4)
  ENGINE_BIN  := $(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)
  BUILT_DED   := $(BUILD_DIR)/$(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)
  ifdef IS_WINDOWS
  	Q3DIR     ?= $(LOCALAPPDATA)/Programs/$(APP_NAME)
  else
  	Q3DIR     ?= $(HOME)/.local/share/$(APP_NAME)
  endif
endif

# Native and WASM game modules share the canonical build/base output.
MODULE_DIR := $(BUILD_DIR)/base

ifeq ($(UNAME_S),Darwin)
  # macOS bundle conventions: code in Contents/MacOS/, data in Contents/Resources/.
  Q3BINDIR   := $(Q3DIR)/Contents/MacOS
  Q3DATADIR  := $(Q3DIR)/Contents/Resources/base
  # Installed binary names, which callers depend on: Info.plist's
  # CFBundleExecutable points at the arch-suffixed engine, while run-headless and
  # the codesign step address the headless binary WITHOUT a suffix. That
  # asymmetry is longstanding and load-bearing, so it is named here rather than
  # normalised — the point is that install_engine stays one recipe.
  INSTALLED_ENGINE   := $(CMAKE_APP_NAME)$(BINEXT)
  INSTALLED_HEADLESS := $(CMAKE_APP_NAME)-headless
else
  Q3BINDIR   := $(Q3DIR)
  Q3DATADIR  := $(Q3DIR)/base
  INSTALLED_ENGINE   := $(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)
  INSTALLED_HEADLESS := $(CMAKE_APP_NAME)-headless$(BINEXT)$(EXEEXT)
endif

# Use Ninja if available — much faster incremental builds
ifneq ($(shell which ninja 2>/dev/null),)
  GENERATOR := -G Ninja
else
  GENERATOR :=
endif

# VM backend toggle: 1 = enable WAMR, 0 = legacy QVM only
USE_WASM ?= 1
USE_FOG_SYSTEM ?= 0

ifeq ($(USE_WASM),1)
  CMAKE_WASM_FLAG := -DUSE_WASM=ON
  # Forward WASI_SDK_PATH to cmake if set (cmake auto-detects /opt/wasi-sdk)
  ifneq ($(WASI_SDK_PATH),)
    CMAKE_WASM_FLAG += -DWASI_SDK_PATH=$(WASI_SDK_PATH)
  endif
else
  CMAKE_WASM_FLAG := -DUSE_WASM=OFF
endif

ifeq ($(USE_FOG_SYSTEM),1)
  CMAKE_FOG_FLAG := -DUSE_FOG_SYSTEM=ON
else
  CMAKE_FOG_FLAG := -DUSE_FOG_SYSTEM=OFF
endif

CMAKE_EXTRA_FLAGS ?=
CMAKE_CHANNEL_FLAG := -DCHANNEL_SUFFIX="$(CHANNEL_SUFFIX)"
CMAKE_PRODUCT_FLAG := -DPRODUCT_NAME="$(PRODUCT_NAME)"
CMAKE_CONFIGURE    := cmake -S . -B $(BUILD_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=$(BUILD_CFG) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=ON -DBUILD_GAME_LIBRARIES=ON $(CMAKE_WASM_FLAG) $(CMAKE_FOG_FLAG) $(CMAKE_CHANNEL_FLAG) $(CMAKE_PRODUCT_FLAG) $(CMAKE_EXTRA_FLAGS)
CMAKE_BUILD        := cmake --build $(BUILD_DIR) --parallel $(JOBS)

# Code signing identity (default: ad-hoc).
CODESIGN_IDENTITY ?= -

# Release provenance is captured once by the outer make.  The build applies
# source-controlled submodule patches, so recomputing git state in a recursive
# bundle make would incorrectly add "-dirty" to the artifact and pack stamp.
SOURCE_VERSION := $(shell git describe --always --dirty)
BUILD_DATE      := $(shell date +%Y%m%d)
BUILD_DATE_ISO  := $(shell date +%Y-%m-%d)
VERSION         := $(BUILD_DATE)-$(SOURCE_VERSION)

# DMG packaging (macOS only)
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
WEB_CONTENT_OUT := $(BUILD_DIR)/browser-content
WEB_BASE_CONTENT ?= $(HOME)/wired/$(APP_NAME)/base/pax01.sw3z
WEB_VISOR_ANIMATION := code/web/content/characters/visor/models/animation.cfg
WEB_AUTHORED_CONTENT := $(WEB_CONTENT_OUT)/authored-content.wac

# Out-of-process crash handler (USE_SENTRY_CRASH=ON only). Path is the same on
# every platform because it comes from the vendored library's build tree, not
# from the platform app skeleton; the copy steps test for it and skip silently
# when the feature is off, so a default build is unaffected.
SENTRY_HANDLER_BIN := $(BUILD_DIR)/src/libs/sentry-native/sentry-crash$(EXEEXT)

# ── Phony targets ─────────────────────────────────────────────────────────────

.PHONY: all configure build build-webgpu-browser test-webgpu-browser-module \
		build-web-client test-web-client \
        _build-stamp clean clean-launcher clean-all rebuild shaders \
        create-launcher create-packs build-fonts \
        _wails-build \
        copy-libs copy-build copy-packs copy-all \
        bundle-codesign bundle-dmg bundle-tar bundle-zip bundle-docker \
        run-launcher run-game run-headless release \
        check smoke test-wiredui-menu-functional test-wiredui-external-actions \
        test-wiredui-external-actions-self test-wiredui-bot-actions \
        test-wiredui-bot-actions-self test-wiredui-connect-action \
        test-wiredui-connect-action-self test-wiredui-demo-play test-wiredui-demo-play-self \
        test-wiredui-demo-malformed test-wiredui-demo-malformed-self \
        test-wiredui-demo-io-fault test-wiredui-demo-io-fault-self \
        test-wiredui-demo-disappeared test-wiredui-demo-disappeared-self \
        test-bot-slot-restart test-bot-slot-restart-self \
        test-wasm-map-restart test-wasm-map-restart-self \
        test-vmi-bytecode-gui test-vmi-bytecode-gui-self \
	test-reload-wasm-refusal test-reload-wasm-refusal-self \
	test-vmi-pack-runtime test-vmi-pack-runtime-self \
	test-dlight-shadow-gpu-budget test-dlight-shadow-gpu-budget-self \
        test-wiredui-demo-semantic-continuation test-wiredui-demo-semantic-continuation-self \
        test-directed-ping-queue test-directed-ping-queue-self \
		test-ping-owner-browser test-ping-owner-browser-self \
		test-ping-owner-offscreen test-ping-owner-offscreen-self \
		test-ping-owner-expired-offscreen test-ping-owner-expired-offscreen-self \
		test-ping-owner-capacity test-ping-owner-capacity-self \
        test-lan-discovery-timeout test-lan-discovery-timeout-self \
		test-ral-readback-runtime test-ral-auto-exposure-runtime test-ral-metal-runtime \
		test-ral-opengl-runtime test-ral-opengl-vulkan-visual-parity \
		test-ral-metal-vulkan-visual-parity test-ral-metal-vulkan-performance \
        test-vm test-quic-game test-fs-dedup bench diff-api lint help

# Default target: a CONSISTENT DEPLOYABLE WORLD, not just compiled objects.
# `build` compiles the engine + native game DLLs + the WASM VM modules (the
# gamecl_wasm/gamesv_wasm custom targets are ALL-targets, so they rebuild
# whenever their sources change) into $(MODULE_DIR)/vm — but leaves the
# deployed mod pack ($(PAK_OUT), i.e. build/base/pax21.sw3z) untouched.
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

configure:
	$(CMAKE_CONFIGURE)

# Browser RAL artifacts use the canonical build/ tree as well. DEV=1 selects
# Debug through the same configure target; the default remains Release.
build-webgpu-browser: configure
	$(CMAKE_BUILD) --target ral_webgpu_browser_wasm_module wired_web_platform_wasm_probe

test-webgpu-browser-module: build-webgpu-browser
	ctest --test-dir $(BUILD_DIR) --output-on-failure \
		-R 'ral_webgpu_browser_(host|dispatch|module).*_contract|ral_webgpu_browser_wasm_module_contract|wired_web_(client|platform).*_contract'

# Production browser client: keep pax01 read-only/external, build pax21 and the
# Emscripten game modules from this checkout, then publish one integrity-bound
# manifest. Browser runtime receives only URLs and MEMFS mount paths.
build-web-client: create-packs
	@test -f "$(WEB_BASE_CONTENT)" || { \
		echo "ERROR: browser base content missing: $(WEB_BASE_CONTENT)"; \
		echo "       set WEB_BASE_CONTENT=/absolute/path/to/pax01.sw3z"; exit 1; }
	$(CMAKE_BUILD) --target wired_web_full_client_wasm_module
	node tools/web-authored-compiler.mjs \
		--output "$(abspath $(WEB_AUTHORED_CONTENT))" \
		--menu "$(abspath modfiles/ui/main.wui)" \
		--l10n "$(abspath modfiles/scripts/l10n/en.lua)" \
		--scene "$(abspath modfiles/scripts/scene/arena1.lua)"
	cmake -DOUT_DIR="$(abspath $(WEB_CONTENT_OUT))" \
		-DPAX01="$(abspath $(WEB_BASE_CONTENT))" \
		-DPAX21="$(abspath $(PAK_OUT))" \
		-DGAMECL="$(abspath $(WEB_CONTENT_OUT))/gameclwasm32.wasm" \
		-DGAMESV="$(abspath $(WEB_CONTENT_OUT))/gamesvwasm32.wasm" \
		-DCONFIG="$(abspath $(PAK_STAGING))/default.cfg" \
		-DVISOR_ANIMATION="$(abspath $(WEB_VISOR_ANIMATION))" \
		-DAUTHORED_CONTENT="$(abspath $(WEB_AUTHORED_CONTENT))" \
		-P cmake/wired_web_content_manifest.cmake

test-web-client: build-web-client
	ctest --test-dir $(BUILD_DIR) --output-on-failure \
		-R 'wired_web_(client|content|platform).*_contract'

# WIRED_PREBUILT=1: engine binaries were placed into $(BUILD_DIR) by an
# external builder (CI downloads them from the cross-windows artifact) —
# verify presence and refresh the pak from the shipped VM modules instead of
# compiling. Everything downstream (create-packs, copy-all, bundle-*) reads
# the same paths either way.
ifdef WIRED_PREBUILT
build:
	@test -f "$(ENGINE_BIN)" || { echo "ERROR: WIRED_PREBUILT set but engine binary missing: $(ENGINE_BIN)"; exit 1; }
	@echo "==> WIRED_PREBUILT: using externally built engine binaries"
	$(MAKE) $(PAK_OUT) VERSION="$(VERSION)" SOURCE_VERSION="$(SOURCE_VERSION)" BUILD_DATE_ISO="$(BUILD_DATE_ISO)"
else
build: _build-stamp configure
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
	$(MAKE) $(PAK_OUT) VERSION="$(VERSION)" SOURCE_VERSION="$(SOURCE_VERSION)" BUILD_DATE_ISO="$(BUILD_DATE_ISO)"
endif

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
	   rev=$$(git describe --always --dirty 2>/dev/null || echo unknown); \
	   { echo "// Generated by 'make' (_build-stamp). DO NOT EDIT; not committed."; \
	     echo "#define WIRED_BUILD_ID $$n"; \
	     echo "#define WIRED_BUILD_DATE \"$$ts\""; \
	     echo "#define WIRED_SOURCE_REVISION \"$$rev\""; } > $(WIRED_BUILD_STAMP_HDR); \
	 fi

clean:
	rm -rf $(BUILD_DIR)

# clean-launcher: nuke wails build output. Preserves frontend/node_modules
# and Go module cache (rebuilding those takes minutes).
clean-launcher:
	rm -rf $(LAUNCHER_DIR)/build

clean-all: clean clean-launcher

# Regenerate the committed shader SPIR-V (code/render/ral/backends/vulkan/renderer/shaders/spirv/shader_data.c)
# from the GLSL sources via compile.mjs. The build COMPILES this file but never
# regenerates it, so a GLSL/manifest change without a re-run ships stale bytecode.
# Best-effort: if node/glslang is absent the committed file is kept so SDK-less /
# CI builds still succeed (the committed shader_data.c is in-tree).
shaders:
	@if command -v node >/dev/null 2>&1; then \
	  echo "==> Regenerating shaders (compile.mjs)..."; \
	  node code/render/ral/backends/vulkan/renderer/shaders/compile.mjs \
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
$(PAK_OUT): Makefile $(PAK_VM_MODULES) $(PAK_CONTENT_SRC) $(SW3Z_BIN)
	@echo "==> Staging pak contents..."
	rm -rf $(PAK_STAGING)
	mkdir -p $(PAK_STAGING) $(BUILD_DIR)/base
	cp -R modfiles/. $(PAK_STAGING)/
	@echo "==> Copying VM modules into pak..."
	# Package exactly the declared prerequisites.  AOT is intentionally excluded
	# until it has its own freshness/removal contract; recursively copying vm/
	# can otherwise ship stale .aot files that runtime mode 2 prefers over WASM.
	mkdir -p $(PAK_STAGING)/vm
	cp $(PAK_VM_MODULES) $(PAK_STAGING)/vm/
	@echo "==> Stamping version..."
	echo "$(APP_NAME) $(SOURCE_VERSION) ($(BUILD_DATE_ISO))" > $(PAK_STAGING)/description.txt
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

# install_sentry_handler — copies the out-of-process crash daemon next to the
# engine binary. ONE definition for every platform: sentry is the single
# minidump producer everywhere, so "where does the daemon go" must have exactly
# one answer. It previously had two — a copy inside the macOS bundle step and
# nothing at all for Windows and Linux — and the gap went unnoticed because
# Crash_SentryInstall is fail-soft: no daemon means no dumps, quietly.
#
# $(1) is the directory holding the engine binary. The only platform-specific
# part is the ad-hoc signature macOS needs to let the daemon run, applied when
# codesign exists rather than under an ifeq, so this stays a single rule.
define install_sentry_handler
	@if test -f "$(SENTRY_HANDLER_BIN)"; then \
	  cp "$(SENTRY_HANDLER_BIN)" "$(1)/sentry-crash$(EXEEXT)"; \
	  command -v codesign >/dev/null 2>&1 && codesign --force --options runtime \
	    --sign "-" "$(1)/sentry-crash$(EXEEXT)" >/dev/null 2>&1 || true; \
	fi
endef

# install_engine($(1)=dstbin, $(2)=dstdata) — copies engine + ded binaries and
# game modules into the destination. ONE definition for every platform. Two
# things do differ by platform — where the built binaries LIVE (macOS keeps them
# inside cmake's .app bundle, everyone else has flat files under BUILD_DIR) and
# what they are CALLED once installed (the bundle drops the arch suffix) — but
# each of those differences has a name: ENGINE_BIN / BUILT_DED and
# INSTALLED_ENGINE / INSTALLED_HEADLESS. They belong in those variables, not in
# a second copy of this recipe. Two recipes doing the same job is how the sentry
# daemon ended up installed on macOS and nowhere else.
define install_engine
	@echo "==> Installing engine + game modules into $(1) ..."
	@mkdir -p "$(1)" "$(2)"
	cp "$(ENGINE_BIN)" "$(1)/$(INSTALLED_ENGINE)"
	@test -f "$(BUILT_DED)" && cp "$(BUILT_DED)" "$(1)/$(INSTALLED_HEADLESS)" || true
	cp "$(MODULE_DIR)/gamecl$(_GAME_MODULE_EXT)"  "$(2)/"
	cp "$(MODULE_DIR)/gamesv$(_GAME_MODULE_EXT)" "$(2)/"
	$(call install_sentry_handler,$(1))
endef

# install_app_skeleton — installs the launcher binary into Q3DIR.  macOS
# rsyncs the engine .app skeleton (Contents/Info.plist, Resources/, etc.)
# and overlays the launcher binary, rewriting CFBundleExecutable so
# double-click runs the launcher.  Windows copies the launcher .exe into
# Q3DIR.  Linux is a no-op — Q3DIR-installed launcher isn't part of the
# Linux flow; users run launcher/build/bin/q3now-launcher directly.
ifeq ($(UNAME_S),Darwin)
define install_app_skeleton
	@echo "==> Installing .app skeleton to $(Q3DIR) ..."
	@# --delete purges stale skeleton files, but the install dir also holds
	@# artifacts OTHER targets own — renderer/dependency dylibs (copy-libs),
	@# the mod pack under Resources/base (copy-packs), and the launcher
	@# binary. Runtime gates may also leave a noncanonical MacOS/base tree or
	@# JSONL diagnostics in the CMake app; hide those on the sender only so
	@# --delete removes stale copies from a reused release destination.
	@# Without the owner excludes, a standalone `make copy-build` silently
	@# stripped separately-installed artifacts and left an unbootable bundle.
	rsync -a --checksum --delete \
	  --filter='H /Contents/MacOS/base/' \
	  --filter='H /Contents/MacOS/*.jsonl' \
	  --exclude='libSDL3*' --exclude='libMoltenVK*' --exclude='libcrypto*' \
	  --exclude='$(CMAKE_APP_NAME)_*$(RENDEXT).dylib' \
	  --exclude='Resources/base/' \
	  --exclude='q3now-launcher' \
	  --exclude='sentry-crash' \
	  "$(BUILT_APP)/" "$(Q3DIR)/"
	@# sentry-crash — the out-of-process crash handler. Built only when
	@# USE_SENTRY_CRASH=ON, and it lands under the sentry-native build dir rather
	@# than in the app skeleton, so it needs the same own-then-restore treatment
	@# as the launcher below. Without the --exclude above, --delete removed it on
	@# every copy; the engine then logged "handler not found" and fell back to the
	@# platform handler. That fallback is soft by design, which is precisely why
	@# the missing file would otherwise go unnoticed.
	$(call install_sentry_handler,$(Q3DIR)/Contents/MacOS)
	@test ! -e "$(Q3DIR)/Contents/MacOS/base" || { \
	  echo "ERROR: runtime base directory leaked into release MacOS staging"; exit 1; }
	@set -- "$(Q3DIR)"/Contents/MacOS/*.jsonl; [ ! -e "$$1" ] || { \
	  echo "ERROR: runtime JSONL diagnostic leaked into release MacOS staging: $$1"; exit 1; }
	@# CFBundleExecutable must point at something that exists: the launcher
	@# when present, the engine binary otherwise — the old unconditional
	@# rewrite left a clean-clone bundle pointing at a missing launcher.
	@if test -f "$(LAUNCHER_BIN)"; then \
	  cp "$(LAUNCHER_BIN)" "$(Q3DIR)/Contents/MacOS/q3now-launcher"; \
	  /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable q3now-launcher" \
	    "$(Q3DIR)/Contents/Info.plist"; \
	elif test -f "$(Q3DIR)/Contents/MacOS/q3now-launcher"; then \
	  echo "  NOTE: launcher not rebuilt — keeping installed copy"; \
	  /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable q3now-launcher" \
	    "$(Q3DIR)/Contents/Info.plist"; \
	else \
	  echo "  NOTE: launcher not built (run make create-launcher) — bundle opens the engine"; \
	  /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable $(CMAKE_APP_NAME)$(BINEXT)" \
	    "$(Q3DIR)/Contents/Info.plist"; \
	fi
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
	cp "$(BUILD_DIR)/$(CMAKE_APP_NAME)_metal$(RENDEXT).dylib" "$(1)/"
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
	              "$(1)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dylib" \
	              "$(1)/$(CMAKE_APP_NAME)_metal$(RENDEXT).dylib"; do \
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
# Depends on copy-all so the bundle it seals is the freshly-deployed one —
# standalone invocations used to sign whatever happened to be installed.

bundle-codesign: copy-all
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
	cp README.md LICENSE THIRD_PARTY_LICENSES.md "$(TAR_STAGING)/"
	cp -R LICENSES "$(TAR_STAGING)/LICENSES"
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
	cp README.md LICENSE THIRD_PARTY_LICENSES.md "$(ZIP_STAGING)/"
	cp -R LICENSES "$(ZIP_STAGING)/LICENSES"
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

# DEV controls Release vs Debug through BUILD_CFG while every output remains
# under the single BUILD_DIR. copy-all picks up the active configuration.
#
# macOS runs through bundle-codesign (which itself depends on copy-all): the
# ARM64 JIT and the VM interpreter need the allow-jit entitlement, and the
# PlistBuddy rewrite in copy-build invalidates any earlier bundle seal — so
# an unsigned run-game was launching a broken-signature bundle every time.
ifeq ($(UNAME_S),Darwin)
_RUN_GAME_DEP := bundle-codesign
else
_RUN_GAME_DEP := copy-all
endif

# VM mode: 0=native dylibs (default), 1=WASM AOT modules with sv_pure
ifeq ($(VM),1)
_RUN_VM_ARGS := +set sv_pure 1 +set vm_game 2 +set vm_cgame 2
else
_RUN_VM_ARGS := +set sv_pure 0 +set vm_game 0 +set vm_cgame 0
endif

# Compose command-line arguments.  Every run-game consumer, including test
# harnesses that append +map or +exec through MAP/EXTRA_ARGS, must establish
# the window authority first.  Do not rely on archived cvars, desktop probing,
# or a stale mode fallback: those have historically published 640x480.
_RUN_GAME_ARGS := +set r_fullscreen 0 +set r_mode -1 +set r_customwidth 1280 +set r_customheight 720
_RUN_GAME_ARGS += $(_RUN_VM_ARGS)
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
	@for f in $(MODULE_DIR)/vm/*.wasm $(MODULE_DIR)/vm/*.aot; do \
		[ -f "$$f" ] && cp "$$f" "$(Q3DIR)/Contents/Resources/base/vm/" || true; \
	done
else
	@mkdir -p "$(Q3DIR)/base/vm"
	@for f in $(MODULE_DIR)/vm/*.wasm $(MODULE_DIR)/vm/*.aot; do \
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
	$(MAKE) bundle-dmg VERSION="$(VERSION)" SOURCE_VERSION="$(SOURCE_VERSION)" BUILD_DATE_ISO="$(BUILD_DATE_ISO)"
	@test -f "$(DMG_OUT)" || { echo "ERROR: expected release artifact missing: $(DMG_OUT)"; exit 1; }
else ifdef IS_WINDOWS
	$(MAKE) bundle-zip VERSION="$(VERSION)" SOURCE_VERSION="$(SOURCE_VERSION)" BUILD_DATE_ISO="$(BUILD_DATE_ISO)"
	@test -f "$(ZIP_OUT)" || { echo "ERROR: expected release artifact missing: $(ZIP_OUT)"; exit 1; }
else
	$(MAKE) bundle-tar VERSION="$(VERSION)" SOURCE_VERSION="$(SOURCE_VERSION)" BUILD_DATE_ISO="$(BUILD_DATE_ISO)"
	@test -f "$(TAR_OUT)" || { echo "ERROR: expected release artifact missing: $(TAR_OUT)"; exit 1; }
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

# ── host subsystem contracts ────────────────────────────────────────────────
# CTest is the single native runner. Labels carry layer/owner/failure/feature
# taxonomy; --no-tests=error prevents a misconfigured BUILD_TESTING=OFF tree
# from turning release evidence into a vacuous green.

test-host: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure --no-tests=error -L host-only

# Dedicated host-only sanitizer tree. Product, game-VM and WASM targets stay
# uninstrumented; host_tests is an aggregate of the exact subsystem contracts.
test-sanitize-host:
	cmake --preset sanitize-host
	cmake --build --preset sanitize-host
	ctest --test-dir build/sanitize-host --output-on-failure --no-tests=error -L host-only --output-junit host-sanitizer-results.xml

.PHONY: test-host test-sanitize-host

# ── process-level release smokes (TASK-122 #10, #11) ────────────────────────
# The launcher, network and map-transition PROCESS smokes, run through the SAME
# CTest verdict as the host contracts — one command, one report:
#
#     ctest --test-dir $(BUILD_DIR) -L process
#
# They carry a separate LABEL rather than being extra `host-only` entries
# because they launch real processes: different cost, different failure modes,
# and CI reports the two layers separately (TASK-122 #12). `test-host` keeps
# its exact prior meaning and its exact prior test count.
#
# Prerequisites are resolved by the scripts through tests/lib/wired_paths.sh and
# each SKIPs (exit 77 -> CTest NOTRUN) when its artefacts are absent, so an
# asset-free runner reports skipped rather than red. `copy-all` runs first
# because the smokes exercise the ASSEMBLED install, not the raw build tree: on
# macOS the engine resolves paks under Contents/Resources (qcommon.h:945-956),
# so a flat build dir cannot satisfy it.
#
# The map-transition repeat gate (#11) is registered ONLY in Debug: its
# ZONEID assertion is _DEBUG-only (common.c:409-412), so Release gets the
# engine-free analyzer teeth check instead of a vacuous green. Reconfigure with
# DEV=1 for the N-run gate.
test-process: build copy-all
	ctest --test-dir $(BUILD_DIR) --output-on-failure --no-tests=error -L process

.PHONY: test-process

# ── check ────────────────────────────────────────────────────────────────────
# Verifies all build outputs are present and host subsystem contracts pass.

# check — verify the build produced everything it must ship. This is a REAL
# gate: any missing artifact exits nonzero (it used to be `A && echo OK ||
# echo MISSING`, which can never fail — `make release` ran with no working
# verification at all). wasi-sdk absence therefore fails loudly here by
# decision (2026-08-10): the pak always ships the WASM modules; development
# without wasi-sdk is not a supported configuration.
# The two codesign probes stay ADVISORY (no fail): `release` runs check
# BEFORE bundle-codesign, so failing on an unsigned tree would deadlock the
# release flow; bundle-codesign itself errors if signing fails.
# SKIP_HOST_TESTS=1: CI's Windows job sets this — its host-test coverage
# moved to the windows-test lane (cross-built exes run on a bare runner).
check: create-packs $(if $(SKIP_HOST_TESTS),,test-host)
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

# Native Vulkan readback gate. Like run-game, DEV reconfigures the canonical
# build tree; the harness must never configure an ad-hoc CMake build directory
# of its own. pax21 comes from the current build,
# while the licensed base archive stays external/read-only.
test-ral-readback-runtime: build
ifeq ($(UNAME_S),Darwin)
	@WIRED_CONTENT_ROOT="$(BUILD_DIR)" \
	WIRED_BASE_CONTENT="$${WIRED_BASE_CONTENT:-$(HOME)/wired/$(APP_NAME)/base/pax01.sw3z}" \
	WIRED_RENDERER="$(BUILD_DIR)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dylib" \
	WIRED_GAMECL="$(MODULE_DIR)/gamecl$(GAME_ARCH).dylib" \
	WIRED_GAMESV="$(MODULE_DIR)/gamesv$(GAME_ARCH).dylib" \
	bash tests/ral-readback-runtime-check.sh "$(BUILD_DIR)/$(CMAKE_APP_NAME)$(BINEXT)"
else
	@echo "SKIP: native RAL readback harness currently requires macOS/MoltenVK"
	@exit 77
endif

# Debug-only GPU-readback acceptance for the RAL histogram/reduce chain.
# Use the canonical build tree: `DEV=1 make test-ral-auto-exposure-runtime`.
test-ral-auto-exposure-runtime: copy-all
ifeq ($(UNAME_S),Darwin)
	@if [ "$(DEV)" != "1" ]; then echo "FAIL: use DEV=1 make test-ral-auto-exposure-runtime"; exit 2; fi
	@WIRED_CONTENT_ROOT="$(HOME)/wired/$(APP_NAME)" \
	bash tests/ral-auto-exposure-check.sh \
		"$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)"
else
	@echo "SKIP: native auto-exposure runtime requires macOS/MoltenVK"
	@exit 77
endif

# Real-map advanced-fog acceptance. This deliberately uses the canonical
# build/ tree and an explicit 1280x720 window; the harness creates isolated
# homes and never writes the player's config or licensed pax01 archive.
test-advanced-fog-runtime: copy-all $(PNG2RAW_BIN)
ifeq ($(UNAME_S),Darwin)
	@if [ "$(DEV)" != "1" ] || [ "$(USE_FOG_SYSTEM)" != "1" ]; then \
		echo "FAIL: use DEV=1 USE_FOG_SYSTEM=1 make test-advanced-fog-runtime"; exit 2; \
	fi
	@WIRED_CONTENT_ROOT="$${WIRED_CONTENT_ROOT:-$(HOME)/wired/$(APP_NAME)}" \
	bash tests/advanced-fog-runtime-check.sh \
		"$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)"
else
	@echo "SKIP: advanced-fog visual runtime currently requires macOS Vulkan"
	@exit 77
endif

.PHONY: test-advanced-fog-runtime

# Native Metal product proof: use the assembled/codesigned application, select
# the Metal renderer explicitly, and boot both parent-acceptance arenas in
# separate 1280x720 processes. The gate requires a native-free world/asset/UI
# submission receipt, build-time metallib-backed indexed BSP, patch/material/
# lightmap/model lowering, textured/MSDF WiredUI triangles and an in-drawable
# readback. Cross-backend pixel tolerances are owned by the parity gate below.
test-ral-metal-runtime: bundle-codesign
ifeq ($(UNAME_S),Darwin)
	@Q3DIR="$(Q3DIR)" \
	bash tests/ral-metal-runtime-check.sh \
	  "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)"
else
	@echo "SKIP: native Metal product runtime requires macOS"
endif

# Exact OpenGL 4.6 product proof. macOS skips fail-closed because its system
# implementation tops out at 4.1; a capable Linux/Windows host runs both arena
# maps through the canonical wired_opengl_<arch> RAL module at 1280x720.
test-ral-opengl-runtime: build
ifeq ($(UNAME_S),Darwin)
	@echo "SKIP: macOS system OpenGL is 4.1; canonical adapter requires exact 4.6 Core"
else
	@Q3DIR="$(Q3DIR)" \
	bash tests/ral-opengl-runtime-check.sh "$(ENGINE_BIN)"
endif

# Release-only exact OpenGL 4.6 versus Vulkan arena parity. Both backends use
# one engine SHA, camera/time/presentation inputs and semantic content receipts;
# the analyzer refuses stale, blank, fallback, missing-cohort and drift evidence.
test-ral-opengl-vulkan-visual-parity: build png2raw
ifeq ($(UNAME_S),Darwin)
	@echo "SKIP: macOS system OpenGL is 4.1; OpenGL/Vulkan parity requires exact 4.6 Core"
else
	@if [ "$(DEV)" = "1" ]; then \
		echo "FAIL: test-ral-opengl-vulkan-visual-parity is Release-only; omit DEV=1"; exit 2; \
	fi
	@grep -q '^CMAKE_BUILD_TYPE:STRING=Release$$' "$(BUILD_DIR)/CMakeCache.txt" || { \
		echo "FAIL: canonical build/ tree is not configured Release"; exit 2; \
	}
	@Q3DIR="$(Q3DIR)" PNG2RAW="$(abspath $(PNG2RAW_BIN))" \
	bash tests/ral-opengl-vulkan-visual-parity.sh "$(ENGINE_BIN)"
endif

# Deterministic native Metal versus Vulkan/MoltenVK content-parity proof. Each
# backend receives the same signed binary, arena cameras, 1280x720 SDR/sRGB
# presentation policy and pinned renderer times. The analyzer also requires
# semantic receipts so a visually convenient missing cohort cannot pass.
test-ral-metal-vulkan-visual-parity: bundle-codesign png2raw
ifeq ($(UNAME_S),Darwin)
	@Q3DIR="$(Q3DIR)" \
	PNG2RAW="$(abspath $(PNG2RAW_BIN))" \
	bash tests/ral-metal-vulkan-visual-parity.sh \
	  "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)"
else
	@echo "SKIP: native Metal/Vulkan visual parity requires macOS"
	@exit 77
endif

# Release-only native Metal versus MoltenVK ceiling comparison. Aggregate
# telemetry records every measured frame in bounded 200-frame histograms, so
# p95/hitch evidence does not pay the observer cost of one JSON line per frame.
test-ral-metal-vulkan-performance: bundle-codesign
ifeq ($(UNAME_S),Darwin)
	@if [ "$(DEV)" = "1" ]; then \
		echo "FAIL: test-ral-metal-vulkan-performance is Release-only; omit DEV=1"; exit 2; \
	fi
	@grep -q '^CMAKE_BUILD_TYPE:STRING=Release$$' "$(BUILD_DIR)/CMakeCache.txt" || { \
		echo "FAIL: canonical build/ tree is not configured Release"; exit 2; \
	}
	@Q3DIR="$(Q3DIR)" \
	bash tests/ral-metal-vulkan-performance.sh \
	  "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)"
else
	@echo "SKIP: native Metal/MoltenVK performance gate requires macOS"
	@exit 77
endif

# Deterministic, pixel-free M1 menu transition gate. WIRED identifies the
# current binary/pack; WIRED_CONTENT_ROOT may supply read-only licensed base
# content for a staging build. --self-test remains pack- and engine-free.
test-wiredui-menu-functional:
	@bash tests/wiredui-menu-functional-check.sh "$${WIRED:---self-test}"

.PHONY: test-wiredui-menu-functional

# WiredUI layout/HiDPI gate.  The product target CAPTURES a layout dump from a
# real window (owner-gated: needs a display).  The -self target runs only the
# HiDPI (#4) ANALYZER over synthetic dumps — no engine, no display, no packs —
# and is wired into ctest as wiredui_dpi_analyzer_contract.
test-wiredui-layout:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-layout-checks.sh "$${WIRED}"

test-wiredui-layout-self:
	@bash tests/wiredui-layout-checks.sh --dpi-self-test

.PHONY: test-wiredui-layout test-wiredui-layout-self

# Second-map Z_Free (#96) gate, display-free. WIRED_HEADLESS must be a DEBUG
# build: the ZONEID assertion is _DEBUG-only (common.c:409-412), so a release
# binary cannot witness the crash and the gate refuses it fail-closed.
# WIRED_CONTENT_ROOT supplies the BSP-bearing pak. The -self target is the
# engine-free analyzer mutation suite (wired into ctest).
test-headless-map-transition:
	@test -n "$${WIRED_HEADLESS:-}" || { echo "ERROR: WIRED_HEADLESS=<debug wired-headless> is required"; exit 2; }
	@bash tests/headless-map-transition-zonecheck.sh "$${WIRED_HEADLESS}"

test-headless-map-transition-self:
	@bash tests/headless-map-transition-zonecheck.sh --self-test

.PHONY: test-headless-map-transition test-headless-map-transition-self

# External-data action gate.  Unlike the analyzer-only target, the product
# target is fail-closed and requires an explicit assembled GUI binary; licensed
# base content is supplied through WIRED_CONTENT_ROOT.
test-wiredui-external-actions:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-external-actions-check.sh "$${WIRED}"

test-wiredui-external-actions-self:
	@bash tests/wiredui-external-actions-check.sh --self-test

.PHONY: test-wiredui-external-actions test-wiredui-external-actions-self

# In-game bot action gate. The product target proves real add/remove UI actions
# on one isolated GUI listen-server; the self target is engine/content-free.
test-wiredui-bot-actions:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-bot-actions-check.sh "$${WIRED}"

test-wiredui-bot-actions-self:
	@bash tests/wiredui-bot-actions-check.sh --self-test

.PHONY: test-wiredui-bot-actions test-wiredui-bot-actions-self

test-wiredui-demo-play:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-demo-play-check.sh "$${WIRED}"

test-wiredui-demo-play-self:
	@bash tests/wiredui-demo-play-check.sh --self-test

.PHONY: test-wiredui-demo-play test-wiredui-demo-play-self

test-wiredui-demo-malformed:
	@test -n "$${WIRED}" || { echo "usage: make $@ WIRED=/absolute/path/to/wired"; exit 64; }
	@bash tests/wiredui-demo-malformed-check.sh "$${WIRED}"

test-wiredui-demo-malformed-self:
	@bash tests/wiredui-demo-malformed-check.sh --self-test

.PHONY: test-wiredui-demo-malformed test-wiredui-demo-malformed-self

test-wiredui-demo-io-fault:
	@test -n "$${WIRED}" || { echo "usage: make $@ WIRED=/absolute/path/to/wired"; exit 64; }
	@bash tests/wiredui-demo-io-fault-check.sh "$${WIRED}"

test-wiredui-demo-io-fault-self:
	@bash tests/wiredui-demo-io-fault-check.sh --self-test

.PHONY: test-wiredui-demo-io-fault test-wiredui-demo-io-fault-self

# Loose-demo inventory race.  A watcher removes the selected authored demo
# before Play; product recovery must rebuild Demos without disconnecting.
test-wiredui-demo-disappeared:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-demo-disappeared-check.sh "$${WIRED}"

test-wiredui-demo-disappeared-self:
	@bash tests/wiredui-demo-disappeared-check.sh --self-test

.PHONY: test-wiredui-demo-disappeared test-wiredui-demo-disappeared-self

# Process-lifetime bot allocation identity across a real stopserver/map cycle.
test-bot-slot-restart:
	@test -n "$${WIRED_HEADLESS:-}" || { echo "ERROR: WIRED_HEADLESS=<assembled-headless-binary> is required"; exit 2; }
	@bash tests/bot-slot-restart-check.sh "$${WIRED_HEADLESS}"

test-bot-slot-restart-self:
	@bash tests/bot-slot-restart-check.sh --self-test

.PHONY: test-bot-slot-restart test-bot-slot-restart-self

# Explicit vm_game=1 (.wasm-only) policy across two real map_restart cycles.
test-wasm-map-restart:
	@test -n "$${WIRED_HEADLESS:-}" || { echo "ERROR: WIRED_HEADLESS=<assembled-headless-binary> is required"; exit 2; }
	@bash tests/wasm-map-restart-check.sh "$${WIRED_HEADLESS}"

test-wasm-map-restart-self:
	@bash tests/wasm-map-restart-check.sh --self-test

.PHONY: test-wasm-map-restart test-wasm-map-restart-self

# Public VMI_BYTECODE policy across the paired gamesv/gamecl live GUI path.
test-vmi-bytecode-gui:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/vmi-bytecode-gui-check.sh "$${WIRED}"

test-vmi-bytecode-gui-self:
	@bash tests/vmi-bytecode-gui-check.sh --self-test

.PHONY: test-vmi-bytecode-gui test-vmi-bytecode-gui-self

test-reload-wasm-refusal:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/reload-wasm-refusal-check.sh "$${WIRED}"

test-reload-wasm-refusal-self:
	@bash tests/reload-wasm-refusal-check.sh --self-test

.PHONY: test-reload-wasm-refusal test-reload-wasm-refusal-self

# Shipped-pax-only paired VM runtime provenance and exact SW3Z VM inventory.
test-vmi-pack-runtime:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/vmi-pack-runtime-check.sh "$${WIRED}"

test-vmi-pack-runtime-self:
	@bash tests/vmi-pack-runtime-check.sh --self-test

.PHONY: test-vmi-pack-runtime test-vmi-pack-runtime-self

# RAL-native timestamp sweep for the dlight omni-shadow GPU budget. This
# measures OFF plus K=1..4; it deliberately does not choose the ship default.
test-dlight-shadow-gpu-budget:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/dlight-shadow-gpu-budget-check.sh "$${WIRED}"

test-dlight-shadow-gpu-budget-self:
	@bash tests/dlight-shadow-gpu-budget-check.sh --self-test

.PHONY: test-dlight-shadow-gpu-budget test-dlight-shadow-gpu-budget-self

test-wiredui-demo-semantic-continuation:
	@test -n "$${WIRED}" || { echo "usage: make $@ WIRED=/absolute/path/to/wired"; exit 64; }
	@bash tests/wiredui-demo-semantic-continuation-check.sh "$${WIRED}"

test-wiredui-demo-semantic-continuation-self:
	@bash tests/wiredui-demo-semantic-continuation-check.sh --self-test

.PHONY: test-wiredui-demo-semantic-continuation test-wiredui-demo-semantic-continuation-self

# Specify Server action gate.  The product target starts an isolated loopback
# headless server and drives the real GUI editfield/action path; the self target
# exercises the strict analyzer and its injected-fault fixtures without an
# engine, renderer, licensed pack, or socket.
test-wiredui-connect-action:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-connect-action-check.sh "$${WIRED}"

test-wiredui-connect-action-self:
	@bash tests/wiredui-connect-action-check.sh --self-test

.PHONY: test-wiredui-connect-action test-wiredui-connect-action-self

# Live-loopback server browser/status/action gate.  The product target proves
# stale-safe status plus footer Connect to the real arena7 headless endpoint;
# the self target validates the strict analyzer and defect fixtures.
test-wiredui-server-browser:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-server-browser-check.sh "$${WIRED}"

test-wiredui-server-browser-self:
	@bash tests/wiredui-server-browser-check.sh --self-test

.PHONY: test-wiredui-server-browser test-wiredui-server-browser-self

# Protected browser password prompt cancellation companion.  One isolated GUI
# process keeps the same READY Server Info owner while real pointer Cancel,
# menu ESC and edit-mode ESC each erase the prompt-owned secret.
test-wiredui-password-cancel:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-password-cancel-check.sh "$${WIRED}"

test-wiredui-password-cancel-self:
	@bash tests/wiredui-password-cancel-check.sh --self-test

.PHONY: test-wiredui-password-cancel test-wiredui-password-cancel-self

# Active-match Server Info authority.  Browser selection A remains a negative
# fixture while the popup must query the connected headless endpoint B.
test-wiredui-ingame-serverinfo:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-ingame-serverinfo-check.sh "$${WIRED}"

test-wiredui-ingame-serverinfo-self:
	@bash tests/wiredui-ingame-serverinfo-check.sh --self-test

.PHONY: test-wiredui-ingame-serverinfo test-wiredui-ingame-serverinfo-self

# A console-started integrated listen server remains connected across an
# authored in-game ESC/Resume pause longer than the client idle timeout.
test-local-listen-timeout:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/local-listen-timeout-check.sh "$${WIRED}"

test-local-listen-timeout-self:
	@bash tests/local-listen-timeout-check.sh --self-test

.PHONY: test-local-listen-timeout test-local-listen-timeout-self

# The historical command now runs the exact production offscreen RAL pipeline
# exercise: layout sharing, graphics readback, compute SSBO and cache roundtrip.
test-ral-pipeline-runtime:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/ral-pipeline-runtime-check.sh "$${WIRED}"

test-ral-pipeline-runtime-self:
	@bash tests/ral-pipeline-runtime-check.sh --self-test

.PHONY: test-ral-pipeline-runtime test-ral-pipeline-runtime-self

# Real whole-texture adapter exercise for the generic RAL residency policy:
# deterministic eviction, explicit evicted state, white fallback and restore.
test-ral-residency-runtime:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/ral-residency-runtime-check.sh "$${WIRED}"

test-ral-residency-runtime-self:
	@bash tests/ral-residency-runtime-check.sh --self-test

.PHONY: test-ral-residency-runtime test-ral-residency-runtime-self

test-ral-profile-markers:
	@test -n "$${WIRED}" || { echo "usage: make test-ral-profile-markers WIRED=/absolute/path/to/wired"; exit 64; }
	@bash tests/ral-profile-markers-check.sh "$${WIRED}"

test-ral-profile-markers-self:
	@bash tests/ral-profile-markers-check.sh --self-test

.PHONY: test-ral-profile-markers test-ral-profile-markers-self

test-ral-profile-renderdoc:
	@test -n "$${WIRED}" || { echo "usage: make test-ral-profile-renderdoc WIRED=/absolute/path/to/wired"; exit 64; }
	@bash tests/ral-profile-renderdoc-check.sh "$${WIRED}"

test-ral-profile-renderdoc-self:
	@bash tests/ral-profile-renderdoc-check.sh --self-test

.PHONY: test-ral-profile-renderdoc test-ral-profile-renderdoc-self

test-ral-profile-layout:
	@test -n "$${WIRED}" || { echo "usage: make test-ral-profile-layout WIRED=/absolute/path/to/wired"; exit 64; }
	@bash tests/ral-profile-layout-check.sh "$${WIRED}"

test-ral-profile-layout-self:
	@bash tests/ral-profile-layout-check.sh --self-test

.PHONY: test-ral-profile-layout test-ral-profile-layout-self

test-ral-frame-graph-runtime:
	@test -n "$${WIRED}" || { echo "usage: make test-ral-frame-graph-runtime WIRED=/absolute/path/to/wired"; exit 64; }
	@bash tests/ral-frame-graph-runtime-check.sh "$${WIRED}"

test-ral-frame-graph-runtime-self:
	@bash tests/ral-frame-graph-runtime-check.sh --self-test

.PHONY: test-ral-frame-graph-runtime test-ral-frame-graph-runtime-self

test-ral-profile-host:
	@tool="$${WIRED_PROFILE_HOST:-$(abspath $(BUILD_DIR))/wired_profile_host}"; \
	if [ ! -x "$$tool" ]; then \
		echo "SKIP: wired_profile_host tool build unavailable (configure WIRED_BUILD_IMGUI_TOOLS=ON)"; \
		exit 0; \
	fi; \
	bash tests/ral-profile-host-lifecycle-check.sh "$$tool"

test-ral-profile-host-self:
	@bash tests/ral-profile-host-lifecycle-check.sh --self-test

.PHONY: test-ral-profile-host test-ral-profile-host-self

test-ral-profile-live:
	@test -n "$${WIRED_PROFILE_HOST}" -a -n "$${WIRED}" || { echo "usage: make test-ral-profile-live WIRED_PROFILE_HOST=/absolute/path/to/wired_profile_host WIRED=/absolute/path/to/wired [WIRED_CONTENT_ROOT=/absolute/content-root]"; exit 64; }
	@bash tests/ral-profile-live-telemetry-check.sh "$${WIRED_PROFILE_HOST}" "$${WIRED}"

test-ral-profile-live-self:
	@bash tests/ral-profile-live-telemetry-check.sh --self-test

.PHONY: test-ral-profile-live test-ral-profile-live-self

# Real global-browser discovery through an authorized loopback master and
# challenge-bound directed info responses. Deliberately excludes Connect.
test-wiredui-global-browser:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/wiredui-global-browser-check.sh "$${WIRED}"

test-wiredui-global-browser-self:
	@bash tests/wiredui-global-browser-check.sh --self-test

.PHONY: test-wiredui-global-browser test-wiredui-global-browser-self

test-directed-ping-timeout:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/directed-ping-timeout-check.sh "$${WIRED}"

test-directed-ping-timeout-self:
	@bash tests/directed-ping-timeout-check.sh --self-test

.PHONY: test-directed-ping-timeout test-directed-ping-timeout-self

test-directed-ping-queue:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/directed-ping-queue-check.sh "$${WIRED}"

test-directed-ping-queue-self:
	@bash tests/directed-ping-queue-check.sh --self-test

.PHONY: test-directed-ping-queue test-directed-ping-queue-self

test-ping-owner-browser:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/ping-owner-browser-check.sh "$${WIRED}"

test-ping-owner-browser-self:
	@bash tests/ping-owner-browser-check.sh --self-test

.PHONY: test-ping-owner-browser test-ping-owner-browser-self

test-ping-owner-offscreen:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/ping-owner-offscreen-check.sh "$${WIRED}"

test-ping-owner-offscreen-self:
	@bash tests/ping-owner-offscreen-check.sh --self-test

.PHONY: test-ping-owner-offscreen test-ping-owner-offscreen-self

test-ping-owner-expired-offscreen:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/ping-owner-expired-offscreen-check.sh "$${WIRED}"

test-ping-owner-expired-offscreen-self:
	@bash tests/ping-owner-expired-offscreen-check.sh --self-test

.PHONY: test-ping-owner-expired-offscreen test-ping-owner-expired-offscreen-self

test-ping-owner-capacity:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/ping-owner-capacity-check.sh "$${WIRED}"

test-ping-owner-capacity-self:
	@bash tests/ping-owner-capacity-check.sh --self-test

.PHONY: test-ping-owner-capacity test-ping-owner-capacity-self

test-lan-discovery-timeout:
	@test -n "$${WIRED:-}" || { echo "ERROR: WIRED=<assembled-gui-binary> is required"; exit 2; }
	@bash tests/lan-discovery-timeout-check.sh "$${WIRED}"

test-lan-discovery-timeout-self:
	@bash tests/lan-discovery-timeout-check.sh --self-test

.PHONY: test-lan-discovery-timeout test-lan-discovery-timeout-self

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
# render resolution as the baseline: the artboard's native 1440x900.
#
# 2026-08-16 — a 1280x720 (16:9) move was attempted here and REVERTED. The
# premise was that the React mockup is resolution-independent because the
# backdrop uses viewBox + width:'100%'. That holds only for the backdrop svg.
# The artboard container itself is fixed pixels with overflow:hidden —
# qw-screens.jsx:1043 reads `width:1440, height:900, overflow:'hidden'` and
# every HUD element is absolutely positioned against that box. A smaller root
# therefore CROPS the HUD rather than reflowing it; measured at 1280x720 the
# health/armor panels, the 9-entry weapon carousel, the telemetry row and the
# ammo value all fall outside the frame entirely.
#
# The engine accepts user-selected aspect ratios. Reconciling the 16:10
# artboard with the capture harness remains an open visual-test decision tracked
# in TASK-70: scale the engine capture, fit the harness with a transform, or
# re-author the artboard for a different target.
# What matters mechanically is that baseline and impl share ONE resolution.

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

.PHONY: png2raw
png2raw:
	cd tools/png2raw && go build -o png2raw$(EXEEXT) ./cmd/png2raw
png-perturb: $(PNG_PERTURB_BIN)

$(VCOMPARE_BIN):
	cd tools/visual-compare && go build -o vcompare$(EXEEXT) ./cmd/vcompare

# Unit tests for the gate tooling itself. Pure Go — no engine, no display, no
# game data — so they run anywhere in about a second. They cover the parts of
# the gate that decide whether a run is even comparable, notably prior-run
# selection: trusting a foreign tool's result.json once made vdiff exit 2
# before it compared a single pixel.
visual-tools-test:
	cd tools/visual-diff && go test ./...
	cd tools/visual-compare && go test ./...

.PHONY: visual-tools-test

visual-baseline:
	@ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) \
	  bash tests/visual/scripts/regen_baseline.sh

# Which capture script an artboard needs depends on what it depicts. Menu
# artboards (v1_monolith and friends) are captured off the attract screen;
# v2_hud_active is an IN-GAME HUD, so it needs a loaded map and a pinned
# viewpoint — capture_v2_impl.sh does `map $(MAP)` + setviewpos, capture_impl.sh
# does not. Before this split, `make visual-test ARTBOARD=v2_hud_active` ran the
# menu capture and compared the attract screen against a HUD baseline, which
# reads as a ~83% global delta: a wrong-scene artifact, not a visual regression.
VISUAL_CAPTURE = $(if $(filter v2_%,$(ARTBOARD)),capture_v2_impl.sh,capture_impl.sh)

# ENGINE_BINARY is deliberately NOT set here. It used to point at
# $(BUILD_DIR)/wired, but the capture scripts run the engine from the user data
# root, and a raw build-tree binary launched from there resolves only one pak
# and dies with "Couldn't load default.cfg" (GAME-DATA.md §1: default.cfg ships
# inside pax21.sw3z, not on disk). Left unset, tests/lib/wired_paths.sh resolves
# the INSTALLED binary, which sits beside its paks. Run `make copy-all` first so
# the install reflects the build under test.

visual-test: build $(VDIFF_BIN)
	@TS=$$(date +%Y%m%d_%H%M%S); \
	  ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) TIMESTAMP=$$TS \
	  bash tests/visual/scripts/$(VISUAL_CAPTURE) && \
	  ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) TIMESTAMP=$$TS \
	  bash tests/visual/scripts/compare.sh

# Aggregate gate: exit 0 only when every config in VISUAL_CFGS passes; exit 1
# on any failure. Per-config failures do not short-circuit the loop (we want
# the full matrix recorded) but they accumulate into the final exit code.
# Direct script invocation — no $(MAKE) recursion (dispatch 5's recursion +
# `|| true` masked every failure).
visual-test-all: build $(VDIFF_BIN)
	@fail=0; pass=0; total=0; first_fail=""; \
	for cfg in $(VISUAL_CFGS); do \
	  m=$${cfg%%:*}; a=$${cfg##*:}; \
	  total=$$((total+1)); \
	  TS=$$(date +%Y%m%d_%H%M%S)_$$total; \
	  echo "==> [$$total] visual-test ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a"; \
	  if ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a TIMESTAMP=$$TS \
	       bash tests/visual/scripts/$(VISUAL_CAPTURE) \
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
# Uses $(VISUAL_CAPTURE), like visual-compare-all: an in-game artboard needs the
# HUD capture, not the menu one. This target used to hardcode capture_impl.sh,
# so `make visual-compare ARTBOARD=v2_hud_active` captured the attract screen
# and compared it against a HUD baseline — the same wrong-scene artifact the
# visual-test comment above describes, and it read as a visual regression.
#
# ENGINE_BINARY is likewise NOT set here (it used to point into $(BUILD_DIR)):
# the capture runs the engine from the user data root, where a raw build-tree
# binary resolves one pak and dies with "Couldn't load default.cfg". Left unset,
# wired_paths.sh resolves the INSTALLED binary beside its paks. Run
# `make copy-all` first so the install reflects the build under test.
visual-compare: build $(VCOMPARE_BIN)
	@TS=$$(date +%Y%m%d_%H%M%S); \
	  ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) TIMESTAMP=$$TS \
	  bash tests/visual/scripts/$(VISUAL_CAPTURE) && \
	  ARTBOARD=$(ARTBOARD) MODE=$(MODE) ACCENT=$(ACCENT) TIMESTAMP=$$TS \
	  bash tests/visual/scripts/vcompare_run.sh

visual-compare-all: build $(VCOMPARE_BIN)
	@fail=0; pass=0; total=0; first_fail=""; \
	for cfg in $(VISUAL_CFGS); do \
	  m=$${cfg%%:*}; a=$${cfg##*:}; \
	  total=$$((total+1)); \
	  TS=$$(date +%Y%m%d_%H%M%S)_$$total; \
	  echo "==> [$$total] visual-compare ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a"; \
	  if ARTBOARD=$(ARTBOARD) MODE=$$m ACCENT=$$a TIMESTAMP=$$TS \
	       bash tests/visual/scripts/$(VISUAL_CAPTURE) \
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
# default golden gate does NOT cover (it never isolates the AO field and never pixel-
# equivalence-gates r_forwardPlus). Each target runs end-to-end: (build →) capture →
# COMPUTED sanity assertions → bless-if-sane → cold re-verify → pass/fail. No eyeball.
#
#   visual-gtao    : builds a dedicated FEAT_SSAO=1 verify DLL (USE_SSAO=ON, separate
#                    build/ssao-verify dir), swaps it into the run dir, captures 5 viewpoints
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
# sync-cgame-run-dir is retained as a compatibility target. Native and WASM
# modules now already land in $(MODULE_DIR), the same base directory the engine
# scans when launched from $(BUILD_DIR), so no profile-directory copy is needed.
sync-cgame-run-dir: build
	@true

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
	@echo "  make clean           remove build/"
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
