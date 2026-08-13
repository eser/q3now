# Wired/q3now test inventory

This directory contains two deliberately separate evidence layers:

- **Host subsystem contracts** are native, asset-free executables orchestrated by CTest.
- **Process and visual gates** boot a product binary and own their explicit fixture/content setup.

`make test-host` is the canonical host command. It fails when the configured tree contains no `host-only` tests; `make check` and therefore the release target consume the same command.

## Host subsystem matrix

| CTest name | Condition | Owner | Production seam | Failure class | Labels |
|---|---|---|---|---|---|
| `wired_curve` | always | Wired math | production-config `wired_curve.c` spline math | contract | `host-only`, `subsystem`, `owner.wired-math`, `failure.contract`, `feature.always` |
| `wired_scene_eval` | always | Wired scene | scene sampling/playback evaluator | contract | `host-only`, `subsystem`, `owner.wired-scene`, `failure.contract`, `feature.always` |
| `sg_registry` | always | q3now save | callback registry | contract | `host-only`, `subsystem`, `owner.q3now-save`, `failure.contract`, `feature.always` |
| `sg_savefile` | always | q3now save | save header/RLE codec | contract | `host-only`, `subsystem`, `owner.q3now-save`, `failure.contract`, `feature.always` |
| `sg_serialize` | always | q3now save | field serializer/relocation | contract | `host-only`, `subsystem`, `owner.q3now-save`, `failure.contract`, `feature.always` |
| `sg_psw` | always | q3now save | persistent subset codec | contract | `host-only`, `subsystem`, `owner.q3now-save`, `failure.contract`, `feature.always` |
| `sg_reloc` | always | q3now save | two-phase pointer fixup | contract | `host-only`, `subsystem`, `owner.q3now-save`, `failure.contract`, `feature.always` |
| `mdl_anim` | always | Wired renderer model | Q1 MDL animation derivation | contract | `host-only`, `subsystem`, `owner.wired-renderer-model`, `failure.contract`, `feature.always` |
| `server_info_parse_contract` | always | q3now client | strict browser infoResponse parsing and bounds | security contract | `host-only`, `subsystem`, `owner.q3now-client`, `failure.security-contract`, `feature.always` |
| `cl_info_challenge_contract` | always | q3now client | generation-mixed lowercase browser authority-token derivation under repeated RNG bytes | security contract | `host-only`, `subsystem`, `owner.q3now-client`, `failure.security-contract`, `feature.always` |
| `cl_ping_queue_contract` | always | q3now client | directed-ping terminal retirement and deterministic saturated-queue oldest selection | contract | `host-only`, `subsystem`, `owner.q3now-client`, `failure.contract`, `feature.always` |
| `cl_ping_owner_contract` | always | q3now client | direct/local/global/favorites ping transaction ownership and exact browser-source identity mapping | contract | `host-only`, `subsystem`, `owner.q3now-client`, `failure.contract`, `feature.always` |
| `cl_demo_frame_contract` | always | q3now client | exact current-protocol frame classification/output atomicity plus pinned production-Huffman encode/decode of bounded illegal-command and snapshot-areamask fixtures | security contract | `host-only`, `subsystem`, `owner.q3now-client`, `failure.security-contract`, `feature.always` |
| `bot_slot_identity_contract` | always | q3now server | monotonic bot allocation identity, strict command token parsing and same-slot ABA rejection | security contract | `host-only`, `subsystem`, `owner.q3now-server`, `failure.security-contract`, `feature.always` |
| `vm_interpret_policy_contract` | always | Wired VM | production VM candidate policy: bytecode `.wasm` only, compiled `.aot` then `.wasm`, native DLL then compiled fallback | contract | `host-only`, `subsystem`, `owner.wired-vm`, `failure.contract`, `feature.always` |
| `cvar_value_contract` | always | Wired cvars | production typed bool/int/float/enum lexical and range validator | security contract | `host-only`, `subsystem`, `owner.wired-cvars`, `failure.security-contract`, `feature.always` |
| `fs_qpath_contract` | always | Wired VFS | production lexical qpath parent-component and legacy `::` rejection before host I/O | security contract | `host-only`, `subsystem`, `owner.wired-vfs`, `failure.security-contract`, `feature.always` |
| `nav_coord_contract` | `USE_RECAST_NAVMESH=ON` | Wired nav | exact Quake/Recast axis, handedness and round-trip conversion | contract | `host-only`, `subsystem`, `owner.wired-nav`, `failure.contract`, `feature.recast` |
| `wired_scene` | `USE_LUA=ON` | Wired scene | Lua scene-table loader | contract | `host-only`, `subsystem`, `owner.wired-scene`, `failure.contract`, `feature.lua` |

Every host test has a 30-second timeout. A test target must compile the owned production algorithm/source rather than a test-only reimplementation; production/runtime targets must never link a test framework. `wired_curve` now compiles the owned source with the production `q_shared.h` configuration and explicitly does not enable its standalone fallback. This inventory still has seam debt: `wired_scene_eval` and `wired_scene` use their source files' standalone fallbacks, while `mdl_anim` exercises a header through a separate test translation unit. Those remaining modes are contracts for the owned logic, not proof that the shipped engine translation unit was linked and executed unchanged.

## Commands

```sh
make test-host
ctest --test-dir build/release --output-on-failure --no-tests=error -L host-only
ctest --test-dir build/release --output-on-failure -L owner.wired-scene
ctest --test-dir build/release --show-only=json-v1
```

`BUILD_TESTING=OFF` is the production-only configure boundary: host test targets are not created and no test dependency may be required.

## Sanitizer lane

`make test-sanitize-host` configures a separate `build/sanitize-host` tree and
builds only the `host_tests` aggregate. Every host contract is compiled and
linked with fail-fast AddressSanitizer + UndefinedBehaviorSanitizer; production,
game-VM and WASM targets are not instrumented. Leak detection is disabled so
the lane remains portable across supported Clang/GCC hosts; bounds, lifetime,
alignment, integer and undefined-behavior failures remain fatal. CTest writes
the durable machine-readable result to
`build/sanitize-host/host-sanitizer-results.xml`.

The sanitizer-only `sanitizer_runtime_contract` intentionally executes one
heap-use-after-free and one signed-overflow probe in child processes. It passes
only when both children exit nonzero with the expected ASan/UBSan diagnostics;
this prevents an ignored compiler flag or missing runtime from turning the lane
into a vacuous green.

```sh
make test-sanitize-host
ctest --test-dir build/sanitize-host --output-on-failure --no-tests=error -L sanitizer.asan-ubsan
```

## WiredUI process gates

These gates boot an assembled GUI product with an isolated home. They require a
current `pax21.sw3z`; `WIRED_CONTENT_ROOT` supplies the read-only canonical
`pax01.sw3z` (or legacy `pak0.pk3`) when it is not installed beside the binary.

| Command | Product contract |
|---|---|
| `make test-wiredui-menu-functional WIRED=...` | menu inventory primitives plus main → Host → selected map → first gameplay frame |
| `bash tests/wiredui-layout-checks.sh ...` | ordered main-menu keyboard/hover focus, DPI and single-highlight layout |
| `bash tests/wiredui-errordialog-check.sh ...` | automated/interactive error popup lifecycle across two map loads |
| `make test-wiredui-external-actions WIRED=...` | paired mod feeder selection, Load Mod, full game-directory restart, mounted mod marker and logging continuity |
| `make test-wiredui-bot-actions WIRED=...` | real in-game quick-add of two bots, bot-only feeder selection, verified bot kick and human/survivor protection |
| `make test-bot-slot-restart WIRED_HEADLESS=...` | one headless process allocates the same named bot in the same slot across a full compound `stopserver` → `map` lifecycle; the old identity is refused and only the current identity removes the replacement, with retained binary/content/harness provenance |
| `make test-wasm-map-restart WIRED_HEADLESS=...` | explicit `vm_game=1` stages an invalid loose AOT sentinel, proves interpreter-only startup plus two `map_restart` recreations without any AOT probe/server shutdown, reconnects the same allocation-1 bot, then proves stale identity rejection after a same-slot/name allocation-2 replacement |
| `make test-vmi-bytecode-gui WIRED=...` | paired isolated headless/GUI session with `vm_game=1` and `vm_cgame=1`: invalid loose AOT sentinels remain unopened, both production VMs load `.wasm` through the interpreter policy, and the client reaches arena7's first gameplay frame |
| `make test-reload-wasm-refusal WIRED=...` | `reload_wasm` is mutation-free: no live VM reports a no-op, while active headless `gamesv` and active local GUI `gamesv+gamecl` are refused as complete identity sets and continue until authored clean shutdown |
| `make test-vmi-pack-runtime WIRED=...` | fresh paired headless/GUI homes stage only shipped `pax01.sw3z` + `pax21.sw3z`; `sv_pure=1`, mode-2 gamesv/gamecl interpreter loads, admission/FIRST, exact SW3Z VM inventory/member hashes, and engine-owned `sysinfo` archive-member paths prove both VMs came from the staged pax21 rather than loose files |
| `make test-wiredui-demo-play WIRED=...` | empty Demo inventory refusal plus ESC/Back return-focus, same-process hot arrival/reload with a dotted basename, then the main-menu keyboard route to current-protocol demos, typed feeder selection, exact playback, advancing frames and natural EOF |
| `make test-wiredui-demo-malformed WIRED=...` | ten structural/semantic current-protocol cases, including a valid non-empty illegal top-level command and a valid `svc_snapshot` body with declared areabytes 255, through the real Demos selection/Play path, exact demo-local recovery, retryless popup over a reloaded Demos menu, authored dismissal, plus bounded structural attract continuation/non-recursion |
| `make test-wiredui-demo-io-fault WIRED=...` | one automated next-playback file-adapter fault after six real bytes, classified by the production frame reader as typed `io-error` through the authored Demos Play route, with exact retryless recovery and no structural/semantic/generic teardown substitution |
| `make test-wiredui-demo-disappeared WIRED=...` | a watcher removes one selected loose current-protocol demo between authored inventory selection and Play; the typed UI-owned open failure preserves connection state, rebuilds Main → Demos to an empty inventory, and presents a generic retryless popup without changing direct `demo`/`nextdemo` continuation policy |
| `make test-wiredui-demo-semantic-continuation WIRED=...` | exact `svc_snapshot/areabytes=255` demo-local recovery under two disjoint owners: attract advances frame-bounded to its successor panel, while scripted `nextdemo` executes exactly once without popup recovery or retained error state |
| `make test-wiredui-connect-action WIRED=...` | real Specify Server editfield input, invalid-address rejection, validated loopback connect and first gameplay frame |
| `make test-wiredui-server-browser WIRED=...` | two-row browser/status lifecycle, isolated Server Info Connect disposal with byte-identical late no-store, then direct unprotected popup Connect to real headless QUIC gameplay |
| `make test-wiredui-password-cancel WIRED=...` | protected prompt real pointer Cancel, menu ESC and edit-mode single ESC each erase transient secret state while preserving the same READY Server Info selection/status owner for natural reopen |
| `make test-wiredui-ingame-serverinfo WIRED=...` | in-game Server Info binds its request to the exact active QUIC connection/address rather than stale browser selection, hides browser-only actions, then Close restores in-game focus/pause and Resume preserves the match |
| `make test-local-listen-timeout WIRED=...` | a console-started integrated host (`map arena7`) is paused through real in-game ESC/Resume for 2500ms with `cl_timeout=1`; exact in-memory handles/slot, pause state, post-resume chat roundtrip and advancing server time prove the same live match without timeout/reconnect |
| `make test-wiredui-global-browser WIRED=...` | authorized loopback master malformed-packet atomicity, deadline expiry/inactive replay, fresh-generation recovery, challenge-bound directed info hydration and real keyboard selection; Connect remains a separate regression |
| `make test-directed-ping-timeout WIRED=...` | direct `/ping` transaction logs: same-address fresh generation after timeout, byte-identical stale challenge rejection, and one current directed response acceptance; no cache/row claim |
| `make test-directed-ping-queue WIRED=...` | 34 direct `/ping` transactions: a >=500ms valid completed result is retained while a free slot exists and reclaimed before a live request under saturation; an all-live saturated queue then evicts oldest, its stale response is inactive, and the current replacement is accepted |
| `make test-ping-owner-browser WIRED=...` | direct and active global-browser ping transactions sharing one endpoint: exact owner/generation routing, current-consumer global-only cache publication, direct survival across authored refresh, and exact final selection identity |
| `make test-ping-owner-offscreen WIRED=...` | a held Global transaction completed after a real source switch to Local: the all-browser terminal reaper publishes it with `consumer=offscreen` only to Global, clears the exact identity, and exposes the row only after Global is reopened |
| `make test-ping-owner-expired-offscreen WIRED=...` | a held Global transaction expires while Local is active under a covering menu: the explicit Local consumer clears only the offscreen Global cache and exact identity; reopening shows no row until an authored fresh Global generation recovers and is selected |
| `make test-ping-owner-capacity WIRED=...` | an accepted Global result covered by another menu while 32 direct transactions saturate the shared queue: completed-slot reuse publishes through `consumer=capacity` before retirement, and the browser row appears after returning |
| `make test-lan-discovery-timeout WIRED=...` | concurrent manual ping and authored LAN discovery ownership, explicit deadline/inactive rejection, fresh-generation stale challenge rejection, one committed recovery row and keyboard selection |

Analyzer-only mutation checks are separate and do not constitute a product run:
`make test-wiredui-external-actions-self`, `make test-wiredui-bot-actions-self`, `make test-wiredui-connect-action-self`, `make test-wiredui-demo-play-self`
and `make test-bot-slot-restart-self`
and `make test-wasm-map-restart-self`
and `make test-vmi-bytecode-gui-self`
and `make test-reload-wasm-refusal-self`
and `make test-vmi-pack-runtime-self`
and `make test-wiredui-demo-disappeared-self`
and `make test-wiredui-server-browser-self`
and `make test-wiredui-password-cancel-self`
and `make test-wiredui-ingame-serverinfo-self`
and `make test-local-listen-timeout-self`
and `make test-wiredui-global-browser-self`
and `make test-directed-ping-timeout-self`
and `make test-directed-ping-queue-self`
and `make test-ping-owner-browser-self`
and `make test-ping-owner-offscreen-self`
and `make test-ping-owner-expired-offscreen-self`
and `make test-ping-owner-capacity-self`
and `make test-lan-discovery-timeout-self`
and the corresponding scripts' `--self-test` modes.
