# Spec: preview-release

## Status: executing

## Concerns: beautiful-product, long-lived, move-fast

## Discovery Answers

### status_quo

Today there is a single release channel. make release produces q3now.app, q3now-VERSION.dmg, etc. All artifacts and user data paths use the fixed name q3now. No way to run preview and public side by side.

### ambition

Add a CHANNEL variable to the build system. Default=preview. CHANNEL=public omits suffix. All artifact names, install paths, user data dirs, and window titles respect the channel. Multiple channels can coexist on the same machine with isolated data.

### reversibility

Fully reversible. CHANNEL=public produces identical output to today. The default just shifts from implicit-public to explicit-preview.

### user_impact

Existing users unaffected — CHANNEL=public preserves current behavior exactly. New default (preview) means dev builds go to ~/q3now-preview instead of ~/q3now, which is actually safer.

### verification

1) make release produces q3now-preview artifacts with -preview suffix. 2) make release CHANNEL=public produces q3now artifacts without suffix (identical to today). 3) make release CHANNEL=canary produces q3now-canary artifacts. 4) Engine uses ~/q3now-preview for preview channel. 5) Launcher uses correct paths per channel. 6) Window title shows channel for non-public builds.

### scope_boundary

Out of scope: auto-update between channels, migration tools between channels, CI/CD pipeline changes, GitHub release automation. This spec only changes the build system and hardcoded paths to be channel-aware.

## v1 vs v2 Scope (move-fast)

_To be addressed during execution._

## Out of Scope

- Out of scope: auto-update between channels, migration tools between channels, CI/CD pipeline changes, GitHub release automation
- This spec only changes the build system and hardcoded paths to be channel-aware.

## Tasks

- [x] task-1: Makefile — Add CHANNEL variable (default=preview). Compute CHANNEL_SUFFIX: empty string if CHANNEL=public, otherwise -CHANNEL. Update APP_NAME to q3now$(CHANNEL_SUFFIX). This automatically propagates to Q3DIR, DMG_NAME, TAR_NAME, ZIP_NAME, BUILT_APP_*, launcher paths, and all artifact names. Update header comment docs to list the CHANNEL variable.
- [x] task-2: CMakeLists.txt — Accept -DCHANNEL_SUFFIX from Makefile cmake configure line. Add add_definitions(-DCHANNEL_SUFFIX=\"$(CHANNEL_SUFFIX)\") so C code can use the CHANNEL_SUFFIX preprocessor define. Pass it in CMAKE_CONFIGURE_RELEASE and CMAKE_CONFIGURE_DEBUG in Makefile.
- [ ] task-3: Engine homepath — In code/unix/unix_shared.c:401 change hardcoded /q3now to /q3now CHANNEL_SUFFIX (using the preprocessor define). Same in code/win32/win_shared.c:118 for \\q3now. If CHANNEL_SUFFIX is empty, result is /q3now (public). If -preview, result is /q3now-preview.
- [ ] task-4: Window title — In code/qcommon/q_shared.h, update CLIENT_WINDOW_TITLE and CONSOLE_WINDOW_TITLE to append CHANNEL_SUFFIX when defined and non-empty. e.g. q3now-preview or just q3now for public.
- [ ] task-5: Launcher — In launcher/internal/config/paths.go, make appName channel-aware. Accept channel suffix via Go build ldflags (-X) passed from Makefile create-launcher target. Update Makefile create-launcher to pass -ldflags "-X ...config.channelSuffix=-preview".

## Verification

- 1) make release produces q3now-preview artifacts with -preview suffix. 2) make release CHANNEL=public produces q3now artifacts without suffix (identical to today). 3) make release CHANNEL=canary produces q3now-canary artifacts. 4) Engine uses ~/q3now-preview for preview channel. 5) Launcher uses correct paths per channel. 6) Window title shows channel for non-public builds.
