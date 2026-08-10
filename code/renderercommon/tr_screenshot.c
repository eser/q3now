// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
tr_screenshot.c — shared `screenshot` console-command grammar.

One copy of the screenshot grammar, compiled into all three renderer DLLs
(renderervk / renderer / renderer2). The grammar parses the command tokens
and resolves a (typeMask, filename, silent) triple, then hands off to the
renderer-provided RB_ScheduleScreenshot hook — the actual capture and the
"Screenshot saved as ..." message happen renderer-side, at end of frame.

See tr_screenshot.h for the grammar reference.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "tr_screenshot.h"
#include "r_log.h"

R_LOG_DECLARE_CHANNEL( rch_screenshot, "renderer.screenshot" );

// `ri` is declared in tr_public.h.


/*
==================
R_ScreenshotFilename

Generate the next free screenshots/YYYY_MM_DD-HH_MM_SS-TTT.<ext> name.
The millisecond suffix removes the need to scan the directory when
capturing screenshots in quick succession; a numeric tail is appended
only on the rare collision.
==================
*/
static void R_ScreenshotFilename( char *fileName, const char *fileExt ) {
	qtime_t t;

	int count = 0;
	ri.Com_RealTime( &t );
	int ms = ri.Milliseconds() % 1000;
	if ( ms < 0 ) ms = 0;

	/* YYYY_MM_DD-HH_MM_SS-TTT filename format */
	Com_sprintf( fileName, MAX_OSPATH,
		"screenshots/%04d_%02d_%02d-%02d_%02d_%02d-%03d.%s",
		1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec, ms, fileExt );

	while (	ri.FS_FileExists( fileName ) && ++count < 1000 ) {
		Com_sprintf( fileName, MAX_OSPATH,
			"screenshots/%04d_%02d_%02d-%02d_%02d_%02d-%03d_%d.%s",
			1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec, ms, count, fileExt );
	}
}


/*
==================
R_ScreenShot_f

Unified parameterized grammar (order-independent tokens):

  screenshot                       file, default format (PNG)
  screenshot png|jpg|bmp|tga       explicit format
  screenshot clipboard             write to OS clipboard (BMP on Windows;
                                   PNG on other SDL platforms; macOS emits
                                   a graceful "not available yet" message)
  screenshot silent                no console echo on success
  screenshot levelshot             special-case levelshot writer
  screenshot <name>                explicit filename; a recognised image
                                   extension on the name sets the format
  screenshot <name> <token>...     any combination, order-independent
==================
*/
static void R_ScreenShot_f( void ) {
	char		checkname[MAX_OSPATH];
	int			typeMask;
	const char	*ext;
	const char	*filenameArg;
	qboolean	silent;
	qboolean	clipboard;
	qboolean	levelshot;
	qboolean	formatExplicit;
	int		i;
	int		argc;

	// ── Defaults ───────────────────────────────────────────────────────────
	// Default format is PNG, both for the no-arg case and any case without
	// an explicit format token.
	typeMask = SCREENSHOT_PNG;
	ext = "png";
	formatExplicit = qfalse;
	silent = qfalse;
	clipboard = qfalse;
	levelshot = qfalse;
	filenameArg = NULL;

	// ── Token parser (order-independent) ───────────────────────────────────
	argc = ri.Cmd_Argc();
	for ( i = 1; i < argc; i++ ) {
		const char *arg = ri.Cmd_Argv( i );
		if ( !Q_stricmp( arg, "levelshot" ) ) {
			levelshot = qtrue;
		} else if ( !Q_stricmp( arg, "silent" ) ) {
			silent = qtrue;
		} else if ( !Q_stricmp( arg, "clipboard" ) ) {
			clipboard = qtrue;
		} else if ( !Q_stricmp( arg, "png" ) ) {
			typeMask = SCREENSHOT_PNG;
			ext = "png";
			formatExplicit = qtrue;
		} else if ( !Q_stricmp( arg, "jpg" ) || !Q_stricmp( arg, "jpeg" ) ) {
			typeMask = SCREENSHOT_JPG;
			ext = "jpg";
			formatExplicit = qtrue;
		} else if ( !Q_stricmp( arg, "bmp" ) ) {
			typeMask = SCREENSHOT_BMP;
			ext = "bmp";
			formatExplicit = qtrue;
		} else if ( !Q_stricmp( arg, "tga" ) ) {
			typeMask = SCREENSHOT_TGA;
			ext = "tga";
			formatExplicit = qtrue;
		} else {
			// Catch-all: treat as filename. If it carries a recognised image
			// extension, also set the format from that extension (unless an
			// explicit format token already set it).
			filenameArg = arg;
			const char *dot = strrchr( arg, '.' );
			if ( dot && !formatExplicit ) {
				const char *fext = dot + 1;
				if ( !Q_stricmp( fext, "jpg" ) || !Q_stricmp( fext, "jpeg" ) ) {
					typeMask = SCREENSHOT_JPG;
					ext = "jpg";
					formatExplicit = qtrue;
				} else if ( !Q_stricmp( fext, "bmp" ) ) {
					typeMask = SCREENSHOT_BMP;
					ext = "bmp";
					formatExplicit = qtrue;
				} else if ( !Q_stricmp( fext, "tga" ) ) {
					typeMask = SCREENSHOT_TGA;
					ext = "tga";
					formatExplicit = qtrue;
				} else if ( !Q_stricmp( fext, "png" ) ) {
					typeMask = SCREENSHOT_PNG;
					ext = "png";
					formatExplicit = qtrue;
				}
			}
		}
	}

	// ── levelshot has its own handler, parameters ignored ─────────────────
	if ( levelshot ) {
		R_LevelShot();
		return;
	}

	// ── Clipboard destination ─────────────────────────────────────────────
	// Per-platform native clipboard format. Windows: CF_DIB via
	// Sys_SetClipboardBitmap (BMP-shaped bytes). Linux X11 (and any
	// non-macOS Unix SDL build): image/png target via
	// Sys_SetClipboardImagePNG. macOS stays as graceful-fail until the
	// Obj-C++ build-out.
	if ( clipboard ) {
#if defined(_WIN32)
		if ( formatExplicit && typeMask != SCREENSHOT_BMP ) {
			R_LOG( rch_screenshot, SEV_WARN, "screenshot clipboard: Windows clipboard path is BMP only\n" );
			return;
		}
		typeMask = SCREENSHOT_BMP;
		ext = "bmp";
		silent = qtrue;
#elif defined(__APPLE__)
		R_LOG( rch_screenshot, SEV_WARN, "screenshot clipboard: clipboard capture not available on macOS yet (Obj-C++ follow-up)\n" );
		return;
#else
		if ( formatExplicit && typeMask != SCREENSHOT_PNG ) {
			R_LOG( rch_screenshot, SEV_WARN, "screenshot clipboard: Linux clipboard path is PNG only\n" );
			return;
		}
		typeMask = SCREENSHOT_PNG;
		ext = "png";
		silent = qtrue;
#endif
	}

	// ── Filename resolution ───────────────────────────────────────────────
	if ( clipboard ) {
		// no filename needed — going to the clipboard, not the filesystem
		checkname[0] = '\0';
		if ( typeMask == SCREENSHOT_PNG ) {
			typeMask |= SCREENSHOT_PNG_CLIPBOARD;
		} else {
			typeMask |= SCREENSHOT_BMP_CLIPBOARD;
		}
	} else if ( filenameArg ) {
		// Explicit filename: strip a recognised image extension if present
		// (we already resolved format from it above), then append the
		// format's own extension to avoid mismatched-extension files.
		char baseName[MAX_OSPATH];
		Q_strncpyz( baseName, filenameArg, sizeof( baseName ) );
		char *dot = strrchr( baseName, '.' );
		if ( dot ) {
			const char *fext = dot + 1;
			if ( !Q_stricmp( fext, "tga" ) || !Q_stricmp( fext, "jpg" ) ||
			     !Q_stricmp( fext, "jpeg" ) || !Q_stricmp( fext, "bmp" ) ||
			     !Q_stricmp( fext, "png" ) ) {
				*dot = '\0';
			}
		}
		Com_sprintf( checkname, MAX_OSPATH, "screenshots/%s.%s", baseName, ext );
	} else {
		// scan for a free filename
		R_ScreenshotFilename( checkname, ext );
	}

	// ── Hand off to the renderer backend ──────────────────────────────────
	RB_ScheduleScreenshot( typeMask, checkname, silent );
}


/*
==================
R_ScreenshotRegisterCommands / R_ScreenshotUnregisterCommands
==================
*/
void R_ScreenshotRegisterCommands( void ) {
	ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f );
}

void R_ScreenshotUnregisterCommands( void ) {
	ri.Cmd_RemoveCommand( "screenshot" );
}


/*
==================
R_ScreenshotPrintSaved
==================
*/
void R_ScreenshotPrintSaved( const char *fileName ) {
	// renderer.screenshot is INFO-floored by the channel taxonomy
	// (cl_main.c CL_InitRef), so this confirmation is visible at default
	// verbosity despite the renderer root's WARN floor.
	R_LOG( rch_screenshot, SEV_INFO, "Screenshot saved as %s\n", fileName );
}
