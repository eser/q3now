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
| `make test-wiredui-demo-play WIRED=...` | current-protocol recording, typed demo-feeder selection, exact playback, advancing frames and natural EOF |
| `make test-wiredui-connect-action WIRED=...` | real Specify Server editfield input, invalid-address rejection, validated loopback connect and first gameplay frame |
| `make test-wiredui-server-browser WIRED=...` | two-row browser selection, stale-safe status feeder lifecycle, then footer Connect to real headless QUIC gameplay |
| `make test-wiredui-global-browser WIRED=...` | authorized loopback master discovery, challenge-bound directed info hydration and real keyboard selection; Connect remains a separate regression |

Analyzer-only mutation checks are separate and do not constitute a product run:
`make test-wiredui-external-actions-self`, `make test-wiredui-bot-actions-self`, `make test-wiredui-connect-action-self`, `make test-wiredui-demo-play-self`
and `make test-wiredui-server-browser-self`
and `make test-wiredui-global-browser-self`
and the corresponding scripts' `--self-test` modes.
