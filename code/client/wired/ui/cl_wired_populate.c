// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_populate.c — Wired UI: dynamic-MULTI populate callbacks
*/

#include "../../client.h"
#include "../../cl_display_catalog.h"
#include "../../snd_public.h"
#include "cl_wired_ui.h"

#if FEAT_WIRED_UI

/* ── audio device populate callback ──────────────────────────────────
 *
 * Calls S_GetAudioDeviceList (defined in snd_miniaudio.c) and exposes the
 * result through wuiPopulateResult_t. The static buffers below own the
 * device-name pointers; they are valid until this callback runs again,
 * which the renderer guarantees by being fully synchronous.
 *
 * Threading: MAIN THREAD ONLY. The renderer invokes this from the UI
 * frame loop, never from the audio callback. */

/* +1 for the leading "Default Device" sentinel entry */
#define WUI_AUDIO_DEVICE_CAPACITY  64

static const char *s_audioDeviceNames[WUI_AUDIO_DEVICE_CAPACITY + 1];
static const char *s_audioDeviceValues[WUI_AUDIO_DEVICE_CAPACITY + 1];

static int CL_WiredAudioDevicesPopulate( wuiPopulateResult_t *out ) {
	if ( !out ) return 0;

	out->state = WUI_POPULATE_LOADING;
	out->count = 0;
	out->names = NULL;
	out->values = NULL;

	/* Reserve slot 0 for the system-default option; enumerate into [1..]. */
	int count = S_GetAudioDeviceList( s_audioDeviceNames + 1, WUI_AUDIO_DEVICE_CAPACITY );

	if ( count < 0 ) {
		/* miniaudio context init or device enumeration failed. */
		out->state = WUI_POPULATE_ERROR;
		return 0;
	}

	/* Prepend "Default Device" — empty cvar value tells miniaudio to use
	 * whatever the OS considers the default output. */
	s_audioDeviceNames[0]  = "Default Device";
	s_audioDeviceValues[0] = "";

	/* For miniaudio the cvar value IS the device name (s_device matching
	 * is by name, not UUID), so values[] mirrors names[] for real devices. */
	for ( int i = 1; i <= count; i++ ) {
		s_audioDeviceValues[i] = s_audioDeviceNames[i];
	}

	out->state = WUI_POPULATE_SUCCESS;
	out->count = count + 1;
	out->names = s_audioDeviceNames;
	out->values = s_audioDeviceValues;
	return count + 1;
}

/* ── bot-profile populate callback ──────────────────────────────────
 * The character registry is the authority for both the visible label and the
 * canonical profile token. A manifest is offered only after its explicit
 * bot_eligible opt-in has resolved to a packaged primary model. */

#define WUI_BOT_PROFILE_CAPACITY 32

static const char *s_botProfileNames[WUI_BOT_PROFILE_CAPACITY];
static const char *s_botProfileValues[WUI_BOT_PROFILE_CAPACITY];

static int CL_WiredBotProfilesPopulate( wuiPopulateResult_t *out ) {
	const clCharacterEntry_t *entries[WUI_BOT_PROFILE_CAPACITY];
	int count = 0;
	qboolean partial = qfalse;

	if ( !out ) return 0;
	out->state = WUI_POPULATE_LOADING;
	out->count = 0;
	out->names = NULL;
	out->values = NULL;

	for ( int i = 0; i < CL_Characters_Count(); i++ ) {
		const clCharacterEntry_t *entry = CL_Characters_At( i );
		if ( !entry || !entry->loaded || !entry->botEligible ) continue;
		if ( count >= WUI_BOT_PROFILE_CAPACITY ) {
			partial = qtrue;
			continue;
		}
		entries[count++] = entry;
	}

	/* Stable lexical order makes UI navigation independent of filesystem and
	 * archive enumeration order. */
	for ( int i = 1; i < count; i++ ) {
		const clCharacterEntry_t *entry = entries[i];
		int j = i;
		while ( j > 0 && Q_stricmp( entries[j - 1]->dirname, entry->dirname ) > 0 ) {
			entries[j] = entries[j - 1];
			j--;
		}
		entries[j] = entry;
	}
	for ( int i = 0; i < count; i++ ) {
		s_botProfileNames[i] = entries[i]->manifest.displayName[0]
			? entries[i]->manifest.displayName : entries[i]->dirname;
		s_botProfileValues[i] = entries[i]->dirname;
	}

	out->state = count > 0
		? ( partial ? WUI_POPULATE_PARTIAL : WUI_POPULATE_SUCCESS )
		: WUI_POPULATE_EMPTY;
	out->count = count;
	out->names = s_botProfileNames;
	out->values = s_botProfileValues;
	return count;
}

/* ── display catalog populate callbacks ─────────────────────────────
 * Values are persistent semantic selectors, never provider order or
 * SDL_DisplayID. The active provider snapshot is immutable for its generation,
 * so these main-thread callbacks may read it directly during a UI frame. */

static char s_displayOutputNames[WIRED_DISPLAY_MAX_DISPLAYS + 1u][WIRED_DISPLAY_NAME_MAX];
static char s_displayOutputValues[WIRED_DISPLAY_MAX_DISPLAYS + 1u][WIRED_DISPLAY_KEY_MAX];
static const char *s_displayOutputNamePtrs[WIRED_DISPLAY_MAX_DISPLAYS + 1u];
static const char *s_displayOutputValuePtrs[WIRED_DISPLAY_MAX_DISPLAYS + 1u];

static char s_displayModeNames[WIRED_MAX_MULTI_CHOICES][64];
static char s_displayModeValues[WIRED_MAX_MULTI_CHOICES][64];
static const char *s_displayModeNamePtrs[WIRED_MAX_MULTI_CHOICES];
static const char *s_displayModeValuePtrs[WIRED_MAX_MULTI_CHOICES];
static char s_displayRefreshNames[WIRED_MAX_MULTI_CHOICES][64];
static char s_displayRefreshValues[WIRED_MAX_MULTI_CHOICES][32];
static const char *s_displayRefreshNamePtrs[WIRED_MAX_MULTI_CHOICES];
static const char *s_displayRefreshValuePtrs[WIRED_MAX_MULTI_CHOICES];

static const wiredDisplayCatalog_t *CL_WiredDisplayCatalog( void ) {
	static wiredDisplayCatalog_t fallback;
	const wiredDisplayCatalog_t *catalog = WiredDisplay_GetActiveCatalog();
	if ( catalog ) return catalog;
	WiredDisplay_BuildFallbackCatalog( WIRED_DISPLAY_PROVIDER_INTERNAL, &fallback );
	return &fallback;
}

static uint32_t CL_WiredSelectedDisplayIndex( const wiredDisplayCatalog_t *catalog ) {
	char selector[WIRED_DISPLAY_KEY_MAX];
	uint32_t i;
	Cvar_VariableStringBuffer( "r_output", selector, sizeof( selector ) );
	if ( selector[0] )
		for ( i = 0u; i < catalog->displayCount; ++i )
			if ( !strcmp( selector, catalog->displays[i].persistentKey ) ) return i;
	for ( i = 0u; i < catalog->displayCount; ++i )
		if ( catalog->displays[i].flags & WIRED_DISPLAY_FLAG_CURRENT ) return i;
	for ( i = 0u; i < catalog->displayCount; ++i )
		if ( catalog->displays[i].flags & WIRED_DISPLAY_FLAG_PRIMARY ) return i;
	return 0u;
}

static int CL_WiredDisplayOutputsPopulate( wuiPopulateResult_t *out ) {
	const wiredDisplayCatalog_t *catalog;
	uint32_t i;
	int count = 1;
	if ( !out ) return 0;
	memset( out, 0, sizeof( *out ) );
	catalog = CL_WiredDisplayCatalog();
	if ( catalog->displayCount <= 1u ) {
		Q_strncpyz( s_displayOutputNames[0], "Current Display",
			sizeof( s_displayOutputNames[0] ) );
		s_displayOutputValues[0][0] = '\0';
		s_displayOutputNamePtrs[0] = s_displayOutputNames[0];
		s_displayOutputValuePtrs[0] = s_displayOutputValues[0];
		out->state = WUI_POPULATE_SUCCESS;
		out->count = 1;
		out->names = s_displayOutputNamePtrs;
		out->values = s_displayOutputValuePtrs;
		return 1;
	}
	Q_strncpyz( s_displayOutputNames[0], catalog->displayCount > 1u
		? "Automatic (Current Display)" : "Current Display",
		sizeof( s_displayOutputNames[0] ) );
	s_displayOutputValues[0][0] = '\0';
	s_displayOutputNamePtrs[0] = s_displayOutputNames[0];
	s_displayOutputValuePtrs[0] = s_displayOutputValues[0];
	for ( i = 0u; i < catalog->displayCount
		&& count < (int)ARRAY_LEN( s_displayOutputNames ); ++i, ++count ) {
		Q_strncpyz( s_displayOutputNames[count], catalog->displays[i].name,
			sizeof( s_displayOutputNames[count] ) );
		Q_strncpyz( s_displayOutputValues[count], catalog->displays[i].persistentKey,
			sizeof( s_displayOutputValues[count] ) );
		s_displayOutputNamePtrs[count] = s_displayOutputNames[count];
		s_displayOutputValuePtrs[count] = s_displayOutputValues[count];
	}
	out->state = count > 0 ? WUI_POPULATE_SUCCESS : WUI_POPULATE_EMPTY;
	out->count = count;
	out->names = s_displayOutputNamePtrs;
	out->values = s_displayOutputValuePtrs;
	return count;
}

static int CL_WiredDisplayModesPopulate( wuiPopulateResult_t *out ) {
	const wiredDisplayCatalog_t *catalog;
	uint32_t displayIndex;
	uint32_t i;
	int count = 2;
	qboolean partial = qfalse;
	if ( !out ) return 0;
	memset( out, 0, sizeof( *out ) );
	catalog = CL_WiredDisplayCatalog();
	displayIndex = CL_WiredSelectedDisplayIndex( catalog );
	Q_strncpyz( s_displayModeNames[0], "Native Desktop", sizeof( s_displayModeNames[0] ) );
	Q_strncpyz( s_displayModeValues[0], "desktop", sizeof( s_displayModeValues[0] ) );
	Q_strncpyz( s_displayModeNames[1], "Custom (r_customWidth / r_customHeight)", sizeof( s_displayModeNames[1] ) );
	Q_strncpyz( s_displayModeValues[1], "custom", sizeof( s_displayModeValues[1] ) );
	for ( i = 0; i < 2u; ++i ) {
		s_displayModeNamePtrs[i] = s_displayModeNames[i];
		s_displayModeValuePtrs[i] = s_displayModeValues[i];
	}
	/* Current/desktop entries first, then the remaining deterministic catalog
	 * order. This keeps the active high-resolution mode visible even if a
	 * provider reports more refresh variants than the UI capacity. */
	for ( int pass = 0; pass < 2; ++pass ) {
		for ( i = 0u; i < catalog->modeCount; ++i ) {
			const wiredDisplayMode_t *mode = &catalog->modes[i];
			int existing;
			const qboolean preferred = ( mode->flags
				& (WIRED_DISPLAY_MODE_CURRENT | WIRED_DISPLAY_MODE_DESKTOP) ) != 0u;
			if ( mode->displayIndex != displayIndex || preferred != ( pass == 0 ) ) continue;
			for ( existing = 2; existing < count; ++existing ) {
				uint32_t width = 0u, height = 0u, rn = 0u, rd = 0u;
				if ( WiredDisplay_ParseModeValue( s_displayModeValues[existing],
					&width, &height, &rn, &rd ) && width == mode->width
					&& height == mode->height ) break;
			}
			if ( existing < count ) continue;
			if ( count >= WIRED_MAX_MULTI_CHOICES ) {
				partial = qtrue;
				continue;
			}
			Com_sprintf( s_displayModeValues[count], sizeof( s_displayModeValues[count] ),
				"%ux%u", mode->width, mode->height );
			Com_sprintf( s_displayModeNames[count], sizeof( s_displayModeNames[count] ),
				"%ux%u", mode->width, mode->height );
			s_displayModeNamePtrs[count] = s_displayModeNames[count];
			s_displayModeValuePtrs[count] = s_displayModeValues[count];
			count++;
		}
	}
	out->state = partial ? WUI_POPULATE_PARTIAL : WUI_POPULATE_SUCCESS;
	out->count = count;
	out->names = s_displayModeNamePtrs;
	out->values = s_displayModeValuePtrs;
	return count;
}

static int CL_WiredDisplayRefreshPopulate( wuiPopulateResult_t *out ) {
	const wiredDisplayCatalog_t *catalog;
	uint32_t displayIndex, width = 0u, height = 0u, ignoredN = 0u, ignoredD = 0u;
	uint32_t i;
	int count = 1;
	qboolean partial = qfalse;
	char selectedMode[64];
	if ( !out ) return 0;
	memset( out, 0, sizeof( *out ) );
	catalog = CL_WiredDisplayCatalog();
	displayIndex = CL_WiredSelectedDisplayIndex( catalog );
	Cvar_VariableStringBuffer( "r_outputMode", selectedMode, sizeof( selectedMode ) );
	(void)WiredDisplay_ParseModeValue( selectedMode, &width, &height,
		&ignoredN, &ignoredD );
	Q_strncpyz( s_displayRefreshNames[0], "Automatic", sizeof( s_displayRefreshNames[0] ) );
	Q_strncpyz( s_displayRefreshValues[0], "auto", sizeof( s_displayRefreshValues[0] ) );
	s_displayRefreshNamePtrs[0] = s_displayRefreshNames[0];
	s_displayRefreshValuePtrs[0] = s_displayRefreshValues[0];
	for ( i = 0u; i < catalog->modeCount; ++i ) {
		const wiredDisplayMode_t *mode = &catalog->modes[i];
		char value[32];
		int existing;
		if ( mode->displayIndex != displayIndex || mode->refreshNumerator == 0u
			|| (width != 0u && (mode->width != width || mode->height != height)) ) continue;
		if ( !WiredDisplay_FormatRefreshValue( mode->refreshNumerator,
			mode->refreshDenominator, value, sizeof( value ) ) ) continue;
		for ( existing = 1; existing < count; ++existing )
			if ( !strcmp( value, s_displayRefreshValues[existing] ) ) break;
		if ( existing < count ) continue;
		if ( count >= WIRED_MAX_MULTI_CHOICES ) { partial = qtrue; continue; }
		Q_strncpyz( s_displayRefreshValues[count], value,
			sizeof( s_displayRefreshValues[count] ) );
		Com_sprintf( s_displayRefreshNames[count], sizeof( s_displayRefreshNames[count] ),
			"%.2f Hz", (double)mode->refreshNumerator / (double)mode->refreshDenominator );
		s_displayRefreshNamePtrs[count] = s_displayRefreshNames[count];
		s_displayRefreshValuePtrs[count] = s_displayRefreshValues[count];
		count++;
	}
	out->state = partial ? WUI_POPULATE_PARTIAL : WUI_POPULATE_SUCCESS;
	out->count = count;
	out->names = s_displayRefreshNamePtrs;
	out->values = s_displayRefreshValuePtrs;
	return count;
}

/* ── batch registration ──────────────────────────────────────────────
 * Called once from WiredUI_Init. Add new populate callbacks here. */

void WiredUI_RegisterCorePopulateCallbacks( void ) {
	WiredUI_RegisterPopulateCallback( "audio_devices", CL_WiredAudioDevicesPopulate );
	WiredUI_RegisterPopulateCallback( "bot_profiles", CL_WiredBotProfilesPopulate );
	WiredUI_RegisterPopulateCallback( "display_outputs", CL_WiredDisplayOutputsPopulate );
	WiredUI_RegisterPopulateCallback( "display_modes", CL_WiredDisplayModesPopulate );
	WiredUI_RegisterPopulateCallback( "display_refresh_rates", CL_WiredDisplayRefreshPopulate );
}

#else  /* !FEAT_WIRED_UI — keep the translation unit non-empty for the
        * AUX_SOURCE_DIRECTORY glob so the build still picks up an
        * (empty) object file. */

void WiredUI_RegisterCorePopulateCallbacks_stub( void ) {}

#endif /* FEAT_WIRED_UI */
