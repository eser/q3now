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

# ── Phony targets ─────────────────────────────────────────────────────────────

.PHONY: all configure build clean rebuild pak check install run run-dev smoke help

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
	@# Game modules live inside the bundle (apppath/baseq3/ — matches stock layout)
	mkdir -p "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/baseq3"
	cp "$(MODULE_DIR)/cgame.dylib"  "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/baseq3/"
	cp "$(MODULE_DIR)/qagame.dylib" "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/baseq3/"
	cp "$(MODULE_DIR)/ui.dylib"     "$(Q3DIR)/$(APP_NAME).app/Contents/MacOS/baseq3/"
	@# Dedicated server binary alongside .app (if built)
	@test -f "$(BUILT_DED)" && cp "$(BUILT_DED)" "$(Q3DIR)/$(APP_NAME)-ded" || true
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
# Builds, installs, and launches with native modules (vm_*=0).
# Native modules give real crash stack traces — use this for debugging.

run-dev: install
ifeq ($(UNAME_S),Darwin)
	open "$(Q3DIR)/$(APP_NAME).app" --args \
	  +set vm_game 0 +set vm_cgame 0 +set vm_ui 0 +map $(MAP)
else
	"$(Q3DIR)/$(APP_NAME)" \
	  +set vm_game 0 +set vm_cgame 0 +set vm_ui 0 +map $(MAP)
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
	@echo "  make help         show this message"
	@echo ""
	@echo "  Variables:"
	@echo "    BUILD_DIR=$(BUILD_DIR)   BUILD_TYPE=$(BUILD_TYPE)"
	@echo "    Q3DIR=$(Q3DIR)"
	@echo "    JOBS=$(JOBS)   MAP=$(MAP)"
	@echo ""
