# Game data

How a machine with **no game files at all** gets to a running q3now, what you
do and do not need to own, and where everything lands.

---

## 1. Building is not optional

Downloading data is not enough. `default.cfg` ships in **no** downloadable
pack — it lives in this repository under `modfiles/` and is packed into
`pax21.sw3z` by `make create-packs`. Without it the engine refuses to start:

```
code/qcommon/wired/core/vfs/files.c:6282
	Com_Terminate( TERM_UNRECOVERABLE, "Couldn't load default.cfg" );
```

So the order is always: **build the repo first, fetch data second.** There is no
prebuilt-binary shortcut.

---

## 2. Two independent pack layers

Both are required; neither substitutes for the other. `FS_Startup` scans the
install resource directory first, then the home path.

| Pack | Contents | Produced by | Lands in |
|---|---|---|---|
| `pax21.sw3z` | `default.cfg`, `ui/`, `scripts/`, fonts, `gamecl.wasm`, `gamesv.wasm` | `make create-packs` (from `modfiles/`) | `<app>/Contents/Resources/base/` (macOS) · `base/` next to the binary elsewhere |
| `pax01.sw3z` | art, maps, sounds | the launcher, from the redistributable bundle | `~/wired/q3now-preview/base/` |

`pax21` sorts after `pak0`–`pak8`, so game logic in it overrides stock content.

---

## 3. You do not need to own Quake III Arena

The launcher downloads a redistributable bundle:

```
launcher/internal/pipeline/orchestrate.go:25
	IDQuake3PackURL = "https://objects.eser.live/quakedata/id-quakepack.zip"
```

It is cached and extracted under `~/wired/q3now-preview/downloaded/`, then
repacked into `pax01.sw3z`. EULA acceptance is recorded in `settings.json`.

**What the bundle contains:** the Quake III demo pak (`demoq3/pak0.pk3`), the
Team Arena demo pak, the Q3/Q3TA point-release patch paks (`baseq3/pak1..pak8`,
`missionpack/pak1..pak3`), the **Quake 1 shareware** pak (`id1/pak0.pak`), CPMA,
HQQ, a weapon-remodel pack, and 13 community maps.

**What it does not contain:** the retail content paks `baseq3/pak0.pk3` and
`missionpack/pak0.pk3`.

### Playable without owning anything

* Renamed Q3 arenas — `arena1` (q3dm1), `arena6` (pro-q3dm6), `arena7` (q3dm7),
  `arena17` (q3dm17), and others
* `arenat*` Team Arena maps, `arenax*` community maps
* **Quake 1 shareware episode 1: `e1m1`–`e1m8` and `start`**, sourced from
  `id1/pak0.pak` inside the bundle
  (`launcher/internal/pipeline/proc_q3copy_entries_pax01.go:2038-2049`)

> `e1m1` — the map the WiredIntel navigation work targets — comes from the
> bundle's Quake 1 shareware pak. **No separate Quake 1 installation is needed.**

### What owning retail Quake III Arena adds

Pointing the launcher at a retail install (`--q3path`, or auto-detection) builds
two more packs: `pax02.sw3z` from `baseq3/pak0.pk3` and `pax04.sw3z` from
`missionpack/pak0.pk3`. Between them they carry the entire stock map roster —
`q3dm0`–`q3dm19`, `q3ctf1`–`q3ctf4`, `q3tourney1`–`q3tourney6`, `mpteam1`–`mpteam8`,
`mpterra1`–`mpterra3`. This is content, not a licence check: everything in §3.1
works without it.

Auto-detection requires **all** of `baseq3/pak0.pk3` … `pak8.pk3` to be present.
Search locations are per-platform (`launcher/internal/config/paths_{darwin,linux,windows}.go`);
on macOS:

* `~/Library/Application Support/Steam/SteamApps/common/Quake 3 Arena`
* `~/Library/Application Support/GOG.com/Games/Quake III Arena`
* `~/q3now/old-q3`

---

## 4. Per-user layout

Engine state lives under `~/wired/<PRODUCT_NAME><CHANNEL_SUFFIX>/`, which by
default is `~/wired/q3now-preview/`. The names are set at the top of
`CMakeLists.txt`; override with `-DPRODUCT_NAME=` / `-DCHANNEL_SUFFIX=`.

```
~/wired/q3now-preview/
├── base/
│   ├── pax01.sw3z          ← launcher-produced art/maps/sounds
│   ├── settings.json       ← EULA acceptance, launcher config
│   └── qconsole.jsonl      ← structured log sink
└── downloaded/             ← id-quakepack.zip cache + extraction
```

`fs_installpath` is separate: it is where the engine binary and `pax21.sw3z`
live. Do **not** override `fs_installpath` to work around a missing map — the
game resolves its own asset paths, and overriding it produces `Can't find map`.

The install root differs per platform, and `Q3DIR` in the Makefile (`:137-157`)
is the authority:

| | install root | binaries | paks |
|---|---|---|---|
| macOS | `/Applications/<app>.app` | `Contents/MacOS/` | `Contents/Resources/base/` |
| Windows | `%LOCALAPPDATA%/Programs/<app>` | same dir | `base/` |
| Linux | `~/.local/share/<app>` | same dir | `base/` |

`~/wired/<product><channel>/` (the home dir above) is **the same shape on every
platform** — only `$HOME` changes. Binaries carry an arch suffix: `wired.arm64`,
`wired.x64[.exe]`.

### Writing a script that needs these paths

Source the helper — do not re-derive, and never hardcode:

```bash
. "$(git rev-parse --show-toplevel)/tests/lib/wired_paths.sh"
echo "$WIRED_BASE"      # ~/wired/q3now-preview/base — screenshots, qconsole.jsonl
echo "$WIRED_BINARY"    # GUI binary, arch- and platform-resolved
echo "$WIRED_BINARY_HEADLESS"  # headless binary
echo "$WIRED_GAMEDATA"   # installed paks
```

Every value honours a pre-set environment variable, so a caller can still point
a run elsewhere without editing the script.

This helper exists because ~14 test scripts each invented their own answer and
most baked in one developer's Windows layout (`/c/Users/<name>/...`). That broke
every non-Windows run and leaked a personal directory structure into a public
repo. If you find yourself typing an absolute path into a script, that is the
bug — the layout is fixed and documented, so there is nothing to guess.

Two related traps, both of which have cost real debugging time:

- **Do not hand-assemble a run** (copying paks into a scratch dir, setting
  `fs_homepath` yourself, writing a `.cfg` and `+exec`-ing it). Use
  `make run-game` / `make run-headless`, or call an existing script in `tests/`.
  If none fits, extend one (add a `--mode`) rather than starting a new one.
- **`_DEBUG`-only functionality is invisible in release builds.** `USE_ZONE_ID`
  (`common.c:409-412`) and `wui_test_dump_clay` are compiled out, so a release
  binary cannot witness a zone-consistency failure or emit a Clay dump. A probe
  that "found nothing" on release found nothing because nothing could be found.
  Check with `strings <binary> | grep -c '<the assertion text>'` before trusting
  a green result.
- **Screenshots always land in `$WIRED_BASE/screenshots/`** —
  `~/wired/q3now-preview/base/screenshots/`, no matter which directory the
  engine was launched from. The `screenshot` command logs a path relative to
  `fs_homepath` (`Screenshot saved as screenshots/<stamp>.png`), which reads
  like a cwd-relative path and is not one. Go straight to the fixed location;
  do not go hunting for it with `find`. Two ways this has cost real time:
  reading a truncated `ls | head` as an empty directory and concluding the
  write had failed, and treating the "saved" line as proof a file exists — it
  is printed without checking `FS_WriteFile`'s result
  (`renderervk/tr_init.c:1164`), so it says the write was *attempted*, not that
  it landed. Stat the file.

---

## 5. Walkthrough

```bash
# 1. build (see BUILD.md) — produces pax21.sw3z
make

# 2. build and run the launcher; accept the EULA, let it download and import
make run-launcher

# 3. verify the data landed
ls -l ~/wired/q3now-preview/base/pax01.sw3z

# 4. run
make run-game MAP=arena1
make run-game MAP=e1m1
```

`make run-launcher` needs the Wails CLI:
`go install github.com/wailsapp/wails/v2/cmd/wails@latest`, with `$(go env GOPATH)/bin`
on `PATH`.

### Verifying a specific map is present

```bash
unzip -l ~/wired/q3now-preview/base/pax01.sw3z | grep 'maps/e1m1'
```

A map that resolves in the pack but still fails to load is an engine or entity
problem, not a data problem — do not go hunting for asset directories. Paths are
fixed by `CMakeLists.txt` and the `FS_Get*Path` family in
`code/qcommon/qcommon.h`.
