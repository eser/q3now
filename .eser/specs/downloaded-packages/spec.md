# Spec: downloaded-packages

## Status: completed

## Concerns: beautiful-product, long-lived, move-fast

## Discovery Answers

### status_quo

{"status_quo":"Users click Download Free Resources which downloads two separate pak0.pk3 files (demoq3 + demota) sequentially. Pipeline implicitly guesses which pak has each file with silent fallback. This causes in-game inconsistencies and users get rejected from servers due to file integrity mismatches.","desired_outcome":"Single zip download (id-quake3pack.zip) → unzip to quakedata dir → pipeline extracts files from explicitly declared Source paks only → strict error if any file missing (no fallback) → consistent file integrity, no server rejections.","scope":"IN: (1) Change download URL to single zip, (2) add unzip step, (3) add Source field to ProcessorEntry, (4) update all entries in pax01-04.go with correct Source (relative to downloads dir, e.g. quakedata/demota/pak0.pk3), (5) make extraction strict with hard error on missing. OUT: pipeline processing logic changes (ModeConvert, ModePatch), UI redesign, inventory/detection system, checksum verification (deferred).","success_criteria":"(1) Launcher downloads single zip successfully, (2) zip extracts to correct directory containing both pak files, (3) every entry in pax01-04.go has a Source field, (4) pipeline reads each file from its declared Source pak only, (5) if any declared file is missing from its source pak, fails with clear error message.","risks":"Acceptable: existing users re-download, larger single file, no checksum verification (deferred). Zip kept after extraction to avoid re-download.","blockers":"None. Zip is already live at the URL."}

_-- Eser Ozvataf_

### ambition

5-star MVP: Single zip download, explicit Source field per entry, strict errors on missing files. 1-star: just change the URL. 10-star: versioned asset packs with checksum verification, integrity dashboard, automatic re-download on corruption. We're building the reliable middle ground.

_-- Eser Ozvataf_

### reversibility

Mostly reversible. Code changes are rollback-safe, data model is additive (new Source field). Only one-way element: existing users' old cached demoq3/demota pak files become orphaned (not cleaned up). The explicit sourcing pattern will still be correct in 2 years and scales to more paks/game packs.

_-- Eser Ozvataf_

### user_impact

Not a breaking change. Existing users will re-download seamlessly when they click Download. No data loss, no config migration, no disruption. The launcher handles the transition transparently.

_-- Eser Ozvataf_

### verification

Manual e2e testing is sufficient. Verification: (1) Download single zip via launcher, (2) verify unzip produces both pak files in quakedata dir, (3) run pipeline extraction with explicit Source fields, (4) launch game and connect to a server without rejection, (5) error path: rename a source pak and verify hard error with clear message. No automated tests needed for this scope.

_-- Eser Ozvataf_

### scope_boundary

This spec should NOT: change pipeline processing modes (ModeConvert, ModePatch), change launcher UI, touch inventory/detection system, add checksum verification (deferred to future spec), clean up old download directories, support multiple zip sources or dynamic URL resolution. Tech debt introduced: Source field requires manual maintenance if zip contents change. v2 polish: auto-generate Source mappings from pak manifest.

_-- Eser Ozvataf_

## v1 vs v2 Scope (move-fast)

_To be addressed during execution._

## Out of Scope

- This spec should NOT: change pipeline processing modes (ModeConvert, ModePatch), change launcher UI, touch inventory/detection system, add checksum verification (deferred to future spec), clean up old download directories, support multiple zip sources or dynamic URL resolution
- Tech debt introduced: Source field requires manual maintenance if zip contents change. v2 polish: auto-generate Source mappings from pak manifest.

## Tasks

- [x] task-1: Add Source field to ProcessorEntry struct in launcher/internal/pipeline/step.go
- [x] task-2: Update download logic in launcher/app.go to use single zip URL (objects.eser.live/quakedata/id-quake3pack.zip) and add unzip step to extract to downloaded/quakedata directory
- [x] task-3: Update CheckDownloadStatus() to detect the new zip/extracted directory structure instead of old separate pak files
- [x] task-4: Update all entries in proc_q3copy_entries_pax01-04.go with correct Source field values (relative to downloads dir e.g. quakedata/demota/pak0.pk3 or quakedata/demoq3/pak0.pk3)
- [x] task-5: Make pipeline extraction strict - error with clear message if any declared file is missing from its declared Source pak (no fallback no silent skipping)

## Verification

- Manual e2e testing is sufficient
- Verification: (1) Download single zip via launcher, (2) verify unzip produces both pak files in quakedata dir, (3) run pipeline extraction with explicit Source fields, (4) launch game and connect to a server without rejection, (5) error path: rename a source pak and verify hard error with clear message
- No automated tests needed for this scope.

## Transition History

| From | To | User | Timestamp | Reason |
|------|----|------|-----------|--------|
| IDLE | DISCOVERY | Eser Ozvataf | 2026-04-02T09:05:18.984Z | - |
