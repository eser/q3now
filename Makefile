# q3now — developer Makefile
#
# Composable build workflow with granular targets.
# All cmake details live in cmake/ — this file is a thin workflow layer.
#
# GENERATION TARGETS
#   make create-launcher    build Go/Wails launcher binary
#   make create-packs        package modfiles/ + VM modules → mod pack
#   make build-fonts          build MSDF font atlases from TTF sources
#
# COPY TARGETS (assemble .app at Q3DIR)
#   make copy-libs          copy renderer + dependency dylibs into .app
#   make copy-build         copy Release engine + game dylibs into .app
#   make copy-build-debug   copy Debug engine + game dylibs into .app
#   make copy-packs          copy mod pack into .app data directory
#
# BUNDLING TARGETS
#   make bundle-codesign    codesign the .app bundle (macOS)
#   make bundle-dmg         create versioned DMG (macOS)
#   make bundle-tar         create versioned tar.gz (Linux)
#
# FLOW TARGETS
#   make run-launcher       build + assemble + codesign + open launcher
#   make run-game                     run engine (main menu)
#   make run-game DEV=1               developer mode (debug build)
#   make run-game DEV=1 MAP=q3dm17    map with debug
#   make run-game VM=1 MAP=q3dm17     VM modules + map
#   make release            build + assemble + codesign + package
#
# VARIABLES (override on command line or env)
#   Q3DIR              install destination      (default: /Applications/q3now on macOS)
#   JOBS               parallel job count       (default: CPU count)
#   MAP                map to load              (default: none = main menu)
#   DEV                developer mode           (default: 0; 1 = debug build + developer 1)
#   VM                 VM game modules           (default: 0; 1=VM + sv_pure 1)
#   USE_SW3Z           archive format           (0=legacy pk3, 1=sw3z; default: 0)
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

# CMAKE_APP_NAME: matches cmake PROJECT() name — used for build output paths
# APP_NAME: channel-suffixed name — used for install paths and artifact names
CMAKE_APP_NAME := q3now
APP_NAME       ?= $(CMAKE_APP_NAME)$(CHANNEL_SUFFIX)
MAP        ?=
DEV        ?= 0
VM         ?= 0

# Dual build directories — avoid cmake reconfigure thrash between Release/Debug
BUILD_DIR_RELEASE := build/release
BUILD_DIR_DEBUG   := build/debug
BUILD_DIR         := $(BUILD_DIR_RELEASE)

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
  JOBS        ?= $(shell sysctl -n hw.ncpu)
  BUILT_APP_RELEASE := $(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)$(BINEXT).app
  BUILT_APP_DEBUG   := $(BUILD_DIR_DEBUG)/$(CMAKE_APP_NAME)$(BINEXT).app
  GAME_BIN    := $(BUILT_APP_RELEASE)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)
  BUILT_DED_RELEASE := $(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)-ded$(BINEXT).app/Contents/MacOS/$(CMAKE_APP_NAME)-ded$(BINEXT)
  BUILT_DED_DEBUG   := $(BUILD_DIR_DEBUG)/$(CMAKE_APP_NAME)-ded$(BINEXT).app/Contents/MacOS/$(CMAKE_APP_NAME)-ded$(BINEXT)
  Q3DIR       ?= /Applications/$(APP_NAME).app
else
  JOBS        ?= $(shell nproc 2>/dev/null || echo 4)
  GAME_BIN    := $(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)
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
CMAKE_CONFIGURE_RELEASE := cmake -S . -B $(BUILD_DIR_RELEASE) $(GENERATOR) -DCMAKE_BUILD_TYPE=Release $(CMAKE_WASM_FLAG) $(CMAKE_SDL_FLAG) $(CMAKE_CHANNEL_FLAG) $(CMAKE_EXTRA_FLAGS)
CMAKE_CONFIGURE_DEBUG   := cmake -S . -B $(BUILD_DIR_DEBUG) $(GENERATOR) -DCMAKE_BUILD_TYPE=Debug $(CMAKE_WASM_FLAG) $(CMAKE_SDL_FLAG) $(CMAKE_CHANNEL_FLAG) $(CMAKE_EXTRA_FLAGS)
CMAKE_BUILD_RELEASE     := cmake --build $(BUILD_DIR_RELEASE) --parallel $(JOBS)
CMAKE_BUILD_DEBUG       := cmake --build $(BUILD_DIR_DEBUG) --parallel $(JOBS)

# Code signing identity (default: ad-hoc).
CODESIGN_IDENTITY ?= -

# DMG packaging (macOS only)
VERSION     := $(shell date +%Y%m%d)-$(shell git describe --always --dirty)
DMG_NAME    := $(APP_NAME)-$(VERSION)-$(UNAME_M)
DMG_STAGING := $(BUILD_DIR_RELEASE)/dmg-staging
DMG_OUT     := $(BUILD_DIR_RELEASE)/$(DMG_NAME).dmg

# tar.gz packaging (Linux)
TAR_NAME    := $(APP_NAME)-$(VERSION)-linux-$(UNAME_M)
TAR_STAGING := $(BUILD_DIR_RELEASE)/tar-staging
TAR_OUT     := $(BUILD_DIR_RELEASE)/$(TAR_NAME).tar.gz

# zip packaging (Windows)
ZIP_NAME    := $(APP_NAME)-$(VERSION)-windows-x86_64
ZIP_STAGING := $(BUILD_DIR_RELEASE)/zip-staging
ZIP_OUT     := $(BUILD_DIR_RELEASE)/$(ZIP_NAME).zip

# Launcher (Go/Wails)
LAUNCHER_DIR := launcher
ifeq ($(UNAME_S),Darwin)
  LAUNCHER_BIN := $(LAUNCHER_DIR)/build/bin/q3now.app/Contents/MacOS/q3now-launcher
else ifdef IS_WINDOWS
  LAUNCHER_BIN := $(LAUNCHER_DIR)/build/bin/q3now-launcher.exe
else
  LAUNCHER_BIN := $(LAUNCHER_DIR)/build/bin/q3now-launcher
endif
WAILS_TAGS   ?=

# SW3Z archiver
SW3Z_DIR := tools/sw3z-archiver
SW3Z_BIN := $(SW3Z_DIR)/cmd/sw3z/sw3z

# Archive format toggle: 1 = sw3z, 0 = legacy pk3
USE_SW3Z ?= 1

# VM backend toggle (moved to top, before ifeq)
# USE_WASM is defined near other cmake flags above

# Pak output (always from Release build — VM modules are always Release)
PAK_STAGING := $(BUILD_DIR_RELEASE)/pak-staging
ifeq ($(USE_SW3Z),1)
  PAK_EXT := sw3z
else
  PAK_EXT := pk3
endif
PAK_OUT := $(BUILD_DIR_RELEASE)/baseq3/pax21.$(PAK_EXT)

# ── Phony targets ─────────────────────────────────────────────────────────────

.PHONY: all configure build build-debug clean rebuild \
        create-launcher create-packs build-fonts \
        copy-libs copy-build copy-build-debug copy-packs copy-all copy-all-debug \
        bundle-codesign bundle-dmg bundle-tar bundle-zip bundle-docker \
        run-launcher run-game release \
        check smoke test-features test-vm bench diff-api help

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
	  $(WAILS_TAGS) -ldflags "-X main.version=$(VERSION) -X github.com/eser/q3now/launcher/internal/config.channelSuffix=$(CHANNEL_SUFFIX)"
	@echo "==> Launcher ready: $(LAUNCHER_BIN)"

# ── create-packs ──────────────────────────────────────────────────────────────
# Packages modfiles/ + VM modules into the mod pack (pax21.sw3z or legacy .pk3).
# "pax21" sorts after pak0–pak8, ensuring highest override priority.
# VM modules here override the stock 1999 bytecode in the base pack.

$(SW3Z_BIN):
	cd $(SW3Z_DIR) && go build -o $(CURDIR)/$(SW3Z_BIN) ./cmd/sw3z

ifeq ($(USE_SW3Z),1)
create-packs: build $(SW3Z_BIN)
else
create-packs: build
endif
	@echo "==> Staging pak contents..."
	rm -rf $(PAK_STAGING)
	mkdir -p $(PAK_STAGING) $(BUILD_DIR_RELEASE)/baseq3
	cp -R modfiles/. $(PAK_STAGING)/
	@echo "==> Copying VM modules into pak..."
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

# ── build-fonts ───────────────────────────────────────────────────────────────
# Build MSDF font atlases from TTF sources using msdf-atlas-gen.
# Requires: TTF files in assets/fonts/ (see assets/fonts/README.md)

MSDF_ATLAS_GEN_DIR = tools/msdf-atlas-gen
MSDF_ATLAS_GEN     = $(MSDF_ATLAS_GEN_DIR)/build/bin/msdf-atlas-gen
FONT_SRC           = assets/fonts
FONT_OUT           = modfiles/fonts

$(MSDF_ATLAS_GEN):
	cd $(MSDF_ATLAS_GEN_DIR) && cmake -B build -DCMAKE_BUILD_TYPE=Release -DMSDF_ATLAS_BUILD_STANDALONE=ON -DMSDF_ATLAS_USE_VCPKG=OFF -DMSDF_ATLAS_USE_SKIA=OFF && cmake --build build --config Release

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
	$(MSDF_ATLAS_GEN) -font $(FONT_BUILD_TMP)/sansman-regular.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 48 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-regular.png -json $(FONT_OUT)/sansman-regular.json
	$(MSDF_ATLAS_GEN) -font $(FONT_BUILD_TMP)/sansman-medium.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 48 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-medium.png -json $(FONT_OUT)/sansman-medium.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/entsans.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 48 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-bold.png -json $(FONT_OUT)/sansman-bold.json
	$(MSDF_ATLAS_GEN) -font $(FONT_BUILD_TMP)/sansman-italic.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 48 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-italic.png -json $(FONT_OUT)/sansman-italic.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/entsani.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 48 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sansman-bold-italic.png -json $(FONT_OUT)/sansman-bold-italic.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/Oxanium-Regular.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 48 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/oxanium.png -json $(FONT_OUT)/oxanium.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/Oxanium-Medium.ttf -charset $(FONT_SRC)/charset_latin.txt -type msdf -format png -size 48 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/oxanium-medium.png -json $(FONT_OUT)/oxanium-medium.json
	$(MSDF_ATLAS_GEN) -font $(FONT_SRC)/ShareTechMono-Regular.ttf -charset $(FONT_SRC)/charset_console.txt -type msdf -format png -size 48 -pxrange 8 -dimensions 1024 1024 -imageout $(FONT_OUT)/sharetechmono.png -json $(FONT_OUT)/sharetechmono.json
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
	  "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-ded" || true
else
	mkdir -p "$(Q3DIR)"
	cp "$(_BDIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" "$(Q3DIR)/"
	@test -f "$(_BDIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)" && \
	  cp "$(_BDIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)$(EXEEXT)" "$(Q3DIR)/" || true
ifdef IS_WINDOWS
	@test -f "$(LAUNCHER_BIN)" && \
	  cp "$(LAUNCHER_BIN)" "$(Q3DIR)/q3now-launcher.exe" || \
	  echo "  NOTE: launcher not built (run make create-launcher)"
endif
endif
	mkdir -p "$(Q3BASEDIR)"
	cp "$(_BDIR)/$(_CFG)/baseq3/cgame$(_GAME_MODULE_EXT)"  "$(Q3BASEDIR)/"
	cp "$(_BDIR)/$(_CFG)/baseq3/qagame$(_GAME_MODULE_EXT)" "$(Q3BASEDIR)/"

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
	cp "$(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).dylib" "$(Q3DIR)/Contents/MacOS/"
	@test -f "$(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dylib" && \
	  cp "$(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dylib" "$(Q3DIR)/Contents/MacOS/" || true
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
	    for BIN in "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)" \
	              "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)_opengl$(RENDEXT).dylib" \
	              "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dylib"; do \
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
else ifdef IS_WINDOWS
	@echo "==> Copying renderer DLLs..."
	@for dll in "$(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).dll" \
	            "$(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).dll"; do \
	  [ -f "$$dll" ] && cp "$$dll" "$(Q3DIR)/" || true; \
	done
else
	@echo "==> Copying renderer shared objects..."
	@for so in "$(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)_opengl$(RENDEXT).so" \
	           "$(BUILD_DIR_RELEASE)/$(CMAKE_APP_NAME)_vulkan$(RENDEXT).so"; do \
	  [ -f "$$so" ] && cp "$$so" "$(Q3DIR)/" || true; \
	done
endif

# ── copy-packs ────────────────────────────────────────────────────────────────
# Copies the mod pack into Q3DATADIR. Removes stale format.

copy-packs: create-packs
	mkdir -p "$(Q3DATADIR)"
ifeq ($(USE_SW3Z),1)
	@echo "==> Copying mod pack to $(Q3DATADIR)/"
	@rm -f "$(Q3DATADIR)/zz-$(APP_NAME).pk3"
	cp "$(PAK_OUT)" "$(Q3DATADIR)/"
else
	@echo "==> Copying mod pack to $(Q3DATADIR)/"
	@rm -f "$(Q3DATADIR)/zz-$(APP_NAME).sw3z"
	cp "$(PAK_OUT)" "$(Q3DATADIR)/"
endif

# ── Composite helpers ────────────────────────────────────────────────────────

copy-all:       copy-build copy-libs copy-packs
copy-all-debug: copy-build-debug copy-libs copy-packs

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
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)"
	codesign --force --options runtime \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/q3now-launcher"
	@test -f "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-ded" && \
	  codesign --force --options runtime --entitlements misc/macos/q3now.entitlements \
	  --sign "$(CODESIGN_IDENTITY)" "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)-ded" || true
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
	cp "$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)" "$(TAR_STAGING)/"
	@test -f "$(Q3DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)" && \
	  cp "$(Q3DIR)/$(CMAKE_APP_NAME)-ded$(BINEXT)" "$(TAR_STAGING)/" || true
	cp "$(LAUNCHER_BIN)" "$(TAR_STAGING)/q3now-launcher" 2>/dev/null || true
	cp -R "$(Q3BASEDIR)/." "$(TAR_STAGING)/baseq3/"
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
	cp -R "$(Q3BASEDIR)/." "$(ZIP_STAGING)/baseq3/"
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
#   DEV=1    debug build, developer mode                       (default: 0)
#   VM=1     use VM game modules instead of native dylibs      (default: 0)
#   MAP=X    load map X; uses +map when DEV=1, +map otherwise
#
# Examples:
#   make run-game                    main menu (native dylibs)
#   make run-game MAP=q3dm17         load q3dm17
#   make run-game DEV=1              debug build, developer mode
#   make run-game DEV=1 MAP=q3dm17   debug build, map q3dm17
#   make run-game VM=1 MAP=q3dm17    VM modules, load q3dm17

# Build the right target based on DEV flag
ifeq ($(DEV),1)
_RUN_GAME_DEP := copy-all-debug
else
_RUN_GAME_DEP := copy-all
endif

# VM mode: 0=native dylibs (default), 1=VM modules (sv_pure 1)
ifeq ($(VM),1)
_RUN_VM_ARGS := +set sv_pure 1 +set vm_game 2 +set vm_cgame 2
else
_RUN_VM_ARGS := +set sv_pure 0 +set vm_game 0 +set vm_cgame 0
endif

# Compose command-line arguments
_RUN_GAME_ARGS := $(_RUN_VM_ARGS)
ifeq ($(DEV),1)
_RUN_GAME_ARGS += +set developer 1 +set g_cheats 1
endif
ifneq ($(MAP),)
_RUN_GAME_ARGS += +map $(MAP)
endif

# Copy VM modules into .app when VM=1
_RUN_VM_COPY :=
ifeq ($(VM),1)
_RUN_VM_COPY := _copy-vm
endif

_copy-vm:
ifeq ($(UNAME_S),Darwin)
	@mkdir -p "$(Q3DIR)/Contents/Resources/baseq3/vm"
	@for f in $(BUILD_DIR_DEBUG)/Debug/baseq3/vm/*.wasm $(BUILD_DIR_DEBUG)/Debug/baseq3/vm/*.aot; do \
		[ -f "$$f" ] && cp "$$f" "$(Q3DIR)/Contents/Resources/baseq3/vm/" || true; \
	done
else
	@mkdir -p "$(Q3DIR)/baseq3/vm"
	@for f in $(BUILD_DIR_DEBUG)/Debug/baseq3/vm/*.wasm $(BUILD_DIR_DEBUG)/Debug/baseq3/vm/*.aot; do \
		[ -f "$$f" ] && cp "$$f" "$(Q3DIR)/baseq3/vm/" || true; \
	done
endif

run-game: $(_RUN_GAME_DEP) $(_RUN_VM_COPY)
ifeq ($(UNAME_S),Darwin)
	"$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)" $(_RUN_GAME_ARGS)
else
	"$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" $(_RUN_GAME_ARGS)
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
	@echo "  │  q3now release ready                 │"
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
	@ls $(MODULE_DIR_RELEASE)/vm/cgame.wasm  > /dev/null 2>&1 && echo "  cgame VM:    OK" || echo "  cgame VM:    MISSING (wasi-sdk not found?)"
	@ls $(MODULE_DIR_RELEASE)/vm/qagame.wasm > /dev/null 2>&1 && echo "  qagame VM:   OK" || echo "  qagame VM:   MISSING (wasi-sdk not found?)"
	@ls $(MODULE_DIR_RELEASE)/cgame$(GAME_ARCH).*    > /dev/null 2>&1 && echo "  cgame native: OK" || echo "  cgame native: MISSING"
	@ls $(MODULE_DIR_RELEASE)/qagame$(GAME_ARCH).*  > /dev/null 2>&1 && echo "  qagame native: OK" || echo "  qagame native: MISSING"
	@test -f $(PAK_OUT) && echo "  mod pack:     OK ($(PAK_EXT))"
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

# ── test-features ────────────────────────────────────────────────────────────
# Starts map with ALL feature cvars enabled, spawns 3 bots, runs 30s.

test-features: copy-all
ifeq ($(UNAME_S),Darwin)
	@echo "==> Testing all features enabled (30s)..."
	@timeout 35 "$(Q3DIR)/Contents/MacOS/$(CMAKE_APP_NAME)$(BINEXT)" \
	  +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 \
	  +set g_fastWeaponSwitch 2 \
	  +set g_spawnProtect 2 \
	  +set cg_scorePlums 2 \
	  +set developer 1 +set ttycon 0 \
	  +map $(MAP) +addbot Doom 3 +addbot Bones 3 +addbot Slash 3 \
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

# ── VM smoke test ────────────────────────────────────────────────────────────

test-vm:
	@bash tests/smoke-wasm.sh

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
	"$(Q3DIR)/$(CMAKE_APP_NAME)$(BINEXT)$(EXEEXT)" +timedemo 1 +demo $(DEMO)
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
	@echo "    make create-packs        package modfiles/ + VM modules → .$(PAK_EXT)"
	@echo ""
	@echo "  Copy (assemble .app):"
	@echo "    make copy-build         Release engine + dylibs → .app"
	@echo "    make copy-build-debug   Debug engine + dylibs → .app"
	@echo "    make copy-libs          renderer + deps → .app"
	@echo "    make copy-packs          mod pack (.$(PAK_EXT)) → .app"
	@echo "    make copy-all           all of the above (Release)"
	@echo "    make copy-all-debug     all of the above (Debug)"
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
	@echo "    make run-game DEV=1               developer mode (debug build)"
	@echo "    make run-game DEV=1 MAP=q3dm17    map with debug"
	@echo "    make run-game VM=1 MAP=q3dm17     VM modules + map"
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
	@echo "    JOBS=$(JOBS)   MAP=$(MAP)   DEV=$(DEV)   VM=$(VM)   DEMO=$(DEMO)"
	@echo "    USE_SW3Z=$(USE_SW3Z)   USE_WASM=$(USE_WASM)   CODESIGN_IDENTITY=$(CODESIGN_IDENTITY)"
	@echo ""
