# q3now — developer Makefile
#
# Wraps cmake configure+build, pak packaging, and local install.
# All cmake details live in cmake/ — this file is a thin workflow layer.
#
# TARGETS
#   make              configure (if needed) + build Release modules
#   make dev          fast incremental build — skips QVM compilation
#   make clean        remove build directory
#   make rebuild      clean + build
#   make pak          package modfiles/ into build/baseq3/pak0.pk3
#   make install      copy game modules + pak into Q3DIR/baseq3/
#   make run          launch q3now locally (requires install first)
#   make ci           run the same build the CI workflow uses
#   make help         show this message
#
# VARIABLES (override on command line or env)
#   BUILD_DIR    cmake output directory        (default: build)
#   BUILD_TYPE   Release | Debug               (default: Release)
#   Q3DIR        install destination           (default: see below)
#   JOBS         parallel job count            (default: CPU count)
#   MAP          map to load with `make run`   (default: q3dm1)

# ── Defaults ──────────────────────────────────────────────────────────────────

BUILD_DIR  ?= build
BUILD_TYPE ?= Release
MAP        ?= q3dm1

# CPU count: macOS uses sysctl, Linux uses nproc
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  JOBS   ?= $(shell sysctl -n hw.ncpu)
  # macOS: prefer the .app bundle executable; fall back to loose binary
  GAME_BIN    := $(BUILD_DIR)/Release/ioquake3.app/Contents/MacOS/ioquake3
  MODULE_DIR  := $(BUILD_DIR)/Release/baseq3
  Q3DIR       ?= $(HOME)/Library/Application Support/Quake3
else
  JOBS   ?= $(shell nproc 2>/dev/null || echo 4)
  GAME_BIN    := $(BUILD_DIR)/Release/ioquake3
  MODULE_DIR  := $(BUILD_DIR)/Release/baseq3
  Q3DIR       ?= $(HOME)/.q3a
endif

Q3BASEDIR := $(Q3DIR)/baseq3

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

.PHONY: all configure build dev clean rebuild pak install run ci help

all: build

# ── Configure ─────────────────────────────────────────────────────────────────
# Re-runs only when CMakeLists.txt changes.

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE_CONFIGURE)

configure: $(BUILD_DIR)/CMakeCache.txt

# ── Build (Release, no QVMs — standard developer workflow) ───────────────────

build: $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE_BUILD)

# Fast iteration: skip QVM compilation (no lcc/q3asm toolchain needed).
# Uses a separate build dir so it doesn't clobber a Release build.
dev:
	cmake -S . -B $(BUILD_DIR)-dev $(GENERATOR) \
	  -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	  -DBUILD_GAME_QVMS=OFF
	cmake --build $(BUILD_DIR)-dev --parallel $(JOBS)

# ── Clean / Rebuild ───────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR)-dev

rebuild: clean build

# ── Pak packaging ─────────────────────────────────────────────────────────────
# Creates build/baseq3/pak0.pk3 from modfiles/ content.
# For distribution with QVMs: run `make build` with BUILD_GAME_QVMS=ON first,
# then copy build/Release/baseq3/vm/ into modfiles/vm/ before running make pak.

PAK_STAGING := $(BUILD_DIR)/pak-staging
PAK_OUT     := $(BUILD_DIR)/baseq3/pak0.pk3

pak: build
	@echo "==> Staging pak contents..."
	rm -rf $(PAK_STAGING)
	mkdir -p $(PAK_STAGING) $(BUILD_DIR)/baseq3
	cp -R modfiles/. $(PAK_STAGING)/
	@echo "==> Creating $(PAK_OUT)..."
	cd $(PAK_STAGING) && zip -r9 "$(CURDIR)/$(PAK_OUT)" . -x "**/.DS_Store" -x ".DS_Store"
	@echo "==> pak0.pk3 ready: $(PAK_OUT)"

# ── Install ───────────────────────────────────────────────────────────────────
# Copies game modules (cgame/qagame/ui) and pak0.pk3 into Q3DIR/baseq3/.
# Override Q3DIR to target a different install location.

install: build pak
	@echo "==> Installing to: $(Q3BASEDIR)"
	mkdir -p "$(Q3BASEDIR)"
	cp $(MODULE_DIR)/cgame.*  "$(Q3BASEDIR)/"
	cp $(MODULE_DIR)/qagame.* "$(Q3BASEDIR)/"
	cp $(MODULE_DIR)/ui.*     "$(Q3BASEDIR)/"
	cp $(PAK_OUT)             "$(Q3BASEDIR)/"
	cp modfiles/description.txt "$(Q3BASEDIR)/"
	@echo "==> Done. Launch with: make run"

# ── Run ───────────────────────────────────────────────────────────────────────
# Launches the locally-built ioquake3 with baseq3 and drops into MAP.
# Requires `make install` to have been run first.

run:
	"$(GAME_BIN)" +set fs_game baseq3 +map $(MAP)

# ── CI (mirrors .github/workflows/build.yml) ─────────────────────────────────
# Runs the same configure+build the CI workflow uses, locally.
# Useful for catching CI failures before pushing.

ci:
	cmake -S . -B $(BUILD_DIR)-ci $(GENERATOR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)-ci --parallel $(JOBS)

# ── Help ──────────────────────────────────────────────────────────────────────

help:
	@echo ""
	@echo "  q3now build targets"
	@echo "  ───────────────────────────────────────────────────────────"
	@echo "  make              configure + build Release modules"
	@echo "  make dev          fast build without QVMs"
	@echo "  make clean        remove build/ and build-dev/"
	@echo "  make rebuild      clean + build"
	@echo "  make pak          package modfiles/ → build/baseq3/pak0.pk3"
	@echo "  make install      build + pak + copy to Q3DIR/baseq3/"
	@echo "  make run          launch q3now (needs install first)"
	@echo "  make ci           mirror the CI workflow build locally"
	@echo "  make help         show this message"
	@echo ""
	@echo "  Variables:"
	@echo "    BUILD_DIR=$(BUILD_DIR)   BUILD_TYPE=$(BUILD_TYPE)"
	@echo "    Q3DIR=$(Q3DIR)"
	@echo "    JOBS=$(JOBS)   MAP=$(MAP)"
	@echo ""
