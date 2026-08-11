// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_populate.c — Wired UI: dynamic-MULTI populate callbacks
*/

#include "../../client.h"
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

/* ── batch registration ──────────────────────────────────────────────
 * Called once from WiredUI_Init. Add new populate callbacks here. */

void WiredUI_RegisterCorePopulateCallbacks( void ) {
	WiredUI_RegisterPopulateCallback( "audio_devices", CL_WiredAudioDevicesPopulate );
	WiredUI_RegisterPopulateCallback( "bot_profiles", CL_WiredBotProfilesPopulate );
}

#else  /* !FEAT_WIRED_UI — keep the translation unit non-empty for the
        * AUX_SOURCE_DIRECTORY glob so the build still picks up an
        * (empty) object file. */

void WiredUI_RegisterCorePopulateCallbacks_stub( void ) {}

#endif /* FEAT_WIRED_UI */
