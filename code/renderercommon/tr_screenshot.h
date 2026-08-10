// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
tr_screenshot.h — shared `screenshot` console-command grammar.

The `screenshot` command grammar (token parser, filename generation,
command registration) lives once here and is compiled into all three
renderer DLLs (renderervk / renderer / renderer2). Each renderer provides
the two backend hooks below — RB_ScheduleScreenshot (schedule a capture in
the renderer's own backend) and R_LevelShot (the 128x128 menu thumbnail
writer) — and consumes R_ScreenshotRegisterCommands / ...Unregister... /
R_ScreenshotPrintSaved.

Grammar (order-independent tokens):

  screenshot                   file, default format (PNG)
  screenshot png|jpg|bmp|tga   explicit format
  screenshot clipboard         write to OS clipboard (BMP on Windows,
                               PNG on other SDL platforms; macOS not yet)
  screenshot silent            no console echo on success
  screenshot levelshot         special-case levelshot writer
  screenshot <name>            explicit filename; a recognised image
                               extension on the name sets the format
  screenshot <name> <token>... any combination, order-independent
===========================================================================
*/
#ifndef WIRED_TR_SCREENSHOT_H
#define WIRED_TR_SCREENSHOT_H

// q_shared.h (qboolean) and tr_public.h (refimport_t ri) must be included
// before this header.

// Screenshot type / destination mask. Bit values are shared across all
// renderer backends — do not renumber.
enum {
	SCREENSHOT_TGA = 1<<0,
	SCREENSHOT_JPG = 1<<1,
	SCREENSHOT_BMP = 1<<2,
	SCREENSHOT_BMP_CLIPBOARD = 1<<3,
	SCREENSHOT_AVI = 1<<4,            // take video frame
	SCREENSHOT_PNG = 1<<5,
	SCREENSHOT_PNG_CLIPBOARD = 1<<6   // Linux X11 (and any SDL non-Windows) clipboard path
};

// ── Shared grammar API (defined in tr_screenshot.c) ──────────────────────

// Register the `screenshot` console command. Only `screenshot` is
// registered — the legacy `screenshotJPEG` / `screenshotBMP` commands no
// longer exist (use `screenshot jpg` / `screenshot bmp`).
void R_ScreenshotRegisterCommands( void );

// Remove the `screenshot` console command.
void R_ScreenshotUnregisterCommands( void );

// Emit the "Screenshot saved as <file>" message. Called by each renderer's
// end-of-frame flush (or queue dispatch) once a capture has been written.
void R_ScreenshotPrintSaved( const char *fileName );

// ── Renderer-provided hooks (defined once per renderer) ──────────────────

// Schedule a screenshot in the renderer's own backend. typeMask is a
// SCREENSHOT_* value (possibly OR-ed with a *_CLIPBOARD bit). Returns
// qfalse if the screenshot cannot be taken (e.g. minimized with no FBO).
qboolean RB_ScheduleScreenshot( int typeMask, const char *fileName, qboolean silent );

// Write a 128x128 levelshot thumbnail for the current world.
void R_LevelShot( void );

#endif /* WIRED_TR_SCREENSHOT_H */
