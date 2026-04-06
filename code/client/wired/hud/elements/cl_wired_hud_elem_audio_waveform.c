/*
===========================================================================
cl_wired_hud_elem_audio_waveform.c — Wired UI HUD element: audio waveform

Renders a bar-graph visualization of recent audio RMS levels, pulled from
the lock-free ring buffer that the miniaudio callback writes in
snd_miniaudio.c (via S_GetRecentLevels).

Layout:
  - Reads up to WAVEFORM_BAR_COUNT recent RMS levels from the audio thread.
  - Draws one vertical bar per level inside the element's rect.
  - Bar height is proportional to level, scaled for perceptibility.
  - Bar colour classifies the level:
      level <  0.5  → green  (normal)
      0.5 ≤ level < 0.8 → amber  (loud)
      level >= 0.8 → red    (near-clipping)

The element is stateless between frames: each routine call refreshes the
level snapshot and redraws. When no audio is playing the levels are zero
and the widget shows a flat baseline.

Used primarily by the Wired UI audio settings panel (modfiles/ui/sound.wmenu)
in combination with the `snd_test` console command which plays a 1 kHz
stereo sine for ~2 seconds.
===========================================================================
*/

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

/* S_GetRecentLevels is declared in snd_public.h, transitively included
 * from client.h above. Defined in snd_miniaudio.c. */

#if FEAT_WIRED_UI

#define WAVEFORM_BAR_COUNT   120
#define WAVEFORM_BAR_GAP_PX  1.0f    /* gap between bars, in pixels */
#define WAVEFORM_GAIN        3.0f    /* perceptual scale: RMS is usually <0.3 */

typedef struct
{
	superhudConfig_t config;
	float            levels[WAVEFORM_BAR_COUNT];
} shudElementAudioWaveform_t;

void *CG_SHUDElementAudioWaveformCreate( const superhudConfig_t *config )
{
	shudElementAudioWaveform_t *element;

	SHUD_ELEMENT_INIT( element, config );

	return element;
}

void CG_SHUDElementAudioWaveformRoutine( void *context )
{
	shudElementAudioWaveform_t *element = (shudElementAudioWaveform_t *)context;
	float  x, y, w, h;
	float  barWidth;
	float  baseY;
	int    count;
	int    i;
	vec4_t baseColor;
	vec4_t greenColor = { 0.35f, 0.90f, 0.45f, 0.95f };
	vec4_t amberColor = { 0.95f, 0.75f, 0.25f, 0.95f };
	vec4_t redColor   = { 0.95f, 0.35f, 0.30f, 0.95f };

	/* Paint the element background + border if the author set one. */
	CG_SHUDFill( &element->config );
	CG_SHUDDrawBorder( &element->config );

	x = element->config.rect.value[0];
	y = element->config.rect.value[1];
	w = element->config.rect.value[2];
	h = element->config.rect.value[3];

	if ( w <= 0.0f || h <= 0.0f )
		return;

	/* Refresh the level snapshot from the audio thread. This is a
	 * lock-free read that may race with the writer — acceptable for
	 * a visualization (see S_GetRecentLevels comment). */
	count = S_GetRecentLevels( element->levels, WAVEFORM_BAR_COUNT );
	if ( count <= 0 )
		return;

	/* Compute bar width from the element rect. We draw `count` bars
	 * with a 1-pixel gap between each so the bars remain visually
	 * distinct on sub-millisecond periods. */
	barWidth = ( w - WAVEFORM_BAR_GAP_PX * (float)( count - 1 ) ) / (float)count;
	if ( barWidth < 1.0f )
		barWidth = 1.0f;

	/* Baseline: draw a single-pixel flat line so the widget is
	 * visible even when audio is silent. Uses the border colour if
	 * the author set one, otherwise a muted white. */
	if ( element->config.color.isSet )
	{
		Vector4Copy( element->config.color.value.rgba, baseColor );
		baseColor[3] *= 0.5f;
	}
	else
	{
		baseColor[0] = 0.55f;
		baseColor[1] = 0.58f;
		baseColor[2] = 0.65f;
		baseColor[3] = 0.50f;
	}
	{
		superhudCoord_t baseline;
		baseline.named.x = x;
		baseline.named.y = y + h - 1.0f;
		baseline.named.w = w;
		baseline.named.h = 1.0f;
		CG_SHUDFillWithColor( &baseline, baseColor );
	}

	/* Draw each bar from left (oldest) to right (newest). The RMS
	 * range is small (~0-0.3), so we apply a modest gain so the
	 * visual response matches the audible dynamic range. */
	baseY = y + h;
	for ( i = 0; i < count; i++ )
	{
		float level = element->levels[i] * WAVEFORM_GAIN;
		float barH;
		float bx;
		superhudCoord_t bar;
		float *color;

		if ( level < 0.0f )
			level = 0.0f;
		if ( level > 1.0f )
			level = 1.0f;

		barH = level * h;
		if ( barH < 1.0f )
			continue;    /* silent period — let baseline show through */

		bx = x + (float)i * ( barWidth + WAVEFORM_BAR_GAP_PX );

		bar.named.x = bx;
		bar.named.y = baseY - barH;
		bar.named.w = barWidth;
		bar.named.h = barH;

		if ( level >= 0.8f )
			color = redColor;
		else if ( level >= 0.5f )
			color = amberColor;
		else
			color = greenColor;

		CG_SHUDFillWithColor( &bar, color );
	}
}

void CG_SHUDElementAudioWaveformDestroy( void *context )
{
	if ( context )
	{
		Z_Free( context );
	}
}

#endif // FEAT_WIRED_UI
