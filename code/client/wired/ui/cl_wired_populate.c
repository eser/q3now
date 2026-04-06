/*
===========================================================================
cl_wired_populate.c — Wired UI: dynamic-MULTI populate callbacks

Hosts the runtime callbacks that fill dynamic-MULTI dropdowns at render
time. Each callback is registered by name; .wmenu items reference it via
the `populateCallback "name"` keyword.

Currently registered:

  audio_devices  → enumerates miniaudio playback devices for the audio
                   settings panel (s_device dropdown).

Adding a new callback:

  1. Implement a static function with signature
     `int CL_WiredXxxPopulate( wuiPopulateResult_t *out )`.
  2. Register it from WiredUI_RegisterCorePopulateCallbacks() below.
  3. Use the callback name in your .wmenu file's populateCallback property.
===========================================================================
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

#define WUI_AUDIO_DEVICE_CAPACITY  64

static const char *s_audioDeviceNames[WUI_AUDIO_DEVICE_CAPACITY];
static const char *s_audioDeviceValues[WUI_AUDIO_DEVICE_CAPACITY];

static int CL_WiredAudioDevicesPopulate( wuiPopulateResult_t *out ) {
	int count;
	int i;

	if ( !out ) return 0;

	out->state = WUI_POPULATE_LOADING;
	out->count = 0;
	out->names = NULL;
	out->values = NULL;

	count = S_GetAudioDeviceList( s_audioDeviceNames, WUI_AUDIO_DEVICE_CAPACITY );

	if ( count < 0 ) {
		/* miniaudio context init or device enumeration failed. */
		out->state = WUI_POPULATE_ERROR;
		return 0;
	}

	if ( count == 0 ) {
		/* No playback devices present at all. Likely no audio hardware
		 * or the OS audio service is offline. */
		out->state = WUI_POPULATE_EMPTY;
		return 0;
	}

	/* For miniaudio the cvar value IS the device name (s_device matching
	 * is by name, not UUID), so values[] mirrors names[]. */
	for ( i = 0; i < count; i++ ) {
		s_audioDeviceValues[i] = s_audioDeviceNames[i];
	}

	out->state = WUI_POPULATE_SUCCESS;
	out->count = count;
	out->names = s_audioDeviceNames;
	out->values = s_audioDeviceValues;
	return count;
}

/* ── batch registration ──────────────────────────────────────────────
 * Called once from WiredUI_Init. Add new populate callbacks here. */

void WiredUI_RegisterCorePopulateCallbacks( void ) {
	WiredUI_RegisterPopulateCallback( "audio_devices", CL_WiredAudioDevicesPopulate );
}

#else  /* !FEAT_WIRED_UI — keep the translation unit non-empty for the
        * AUX_SOURCE_DIRECTORY glob so the build still picks up an
        * (empty) object file. */

void WiredUI_RegisterCorePopulateCallbacks_stub( void ) {}

#endif /* FEAT_WIRED_UI */
