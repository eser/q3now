// cl_wired_hud_elem_ng.c — Lagometer / Netgraph HUD element
// Renders frame timing (top 1/3) and snapshot latency (bottom 2/3)
// as a bar graph. Yellow=interpolating, Blue=extrapolating,
// Green=healthy ping, Yellow=rate-delayed, Red=dropped packet.
#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

#define MAX_LAGOMETER_PING   900
#define MAX_LAGOMETER_RANGE  300

typedef struct {
	superhudConfig_t config;
} shudElementNG_t;

void* CG_SHUDElementNGCreate(const superhudConfig_t* config)
{
	shudElementNG_t* element;

	SHUD_ELEMENT_INIT(element, config);

	return element;
}

void CG_SHUDElementNGRoutine(void* context)
{
	shudElementNG_t* element = (shudElementNG_t*)context;
	const wiredLagometer_t *lag;
	float x, y, w, h;
	float ax, ay, aw, ah;
	float mid, range, vscale, v;
	int a, i, color;
	qhandle_t white;

	if ( !wiredHud->valid ) return;

	// skip on local server (no meaningful network stats)
	if ( wiredHud->localServer ) return;

	lag = &wiredHud->lagometer;
	white = wiredHud->whiteShader;

	// use SuperHUD config rect, or default to bottom-right 48x48
	if ( element->config.rect.isSet ) {
		x = element->config.rect.value[0];
		y = element->config.rect.value[1];
		w = element->config.rect.value[2];
		h = element->config.rect.value[3];
	} else {
		x = (float)cls.glconfig.vidWidth - 48;
		y = (float)cls.glconfig.vidHeight - 48;
		w = 48;
		h = 48;
	}

	// draw background
	CG_SHUDFill( &element->config );

	// coordinates are already real screen pixels
	ax = x; ay = y; aw = w; ah = h;

	// ── frame interpolation/extrapolation graph (top 1/3) ────────
	color = -1;
	range = ah / 3.0f;
	mid = ay + range;
	vscale = range / MAX_LAGOMETER_RANGE;

	for ( a = 0; a < (int)aw; a++ ) {
		i = ( lag->frameCount - 1 - a ) & ( WIRED_LAG_SAMPLES - 1 );
		v = (float)lag->frameSamples[i] * vscale;

		if ( v > 0 ) {
			if ( color != 1 ) {
				color = 1;
				re.SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
			}
			if ( v > range ) v = range;
			re.DrawStretchPic( ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, white );
		} else if ( v < 0 ) {
			if ( color != 2 ) {
				color = 2;
				re.SetColor( g_color_table[ColorIndex(COLOR_BLUE)] );
			}
			v = -v;
			if ( v > range ) v = range;
			re.DrawStretchPic( ax + aw - a, mid, 1, v, 0, 0, 0, 0, white );
		}
	}

	// ── snapshot latency/drop graph (bottom 2/3) ─────────────────
	range = ah / 2.0f;
	vscale = range / MAX_LAGOMETER_PING;

	for ( a = 0; a < (int)aw; a++ ) {
		i = ( lag->snapshotCount - 1 - a ) & ( WIRED_LAG_SAMPLES - 1 );
		v = (float)lag->snapshotSamples[i];

		if ( v > 0 ) {
			if ( lag->snapshotFlags[i] & SNAPFLAG_RATE_DELAYED ) {
				if ( color != 5 ) {
					color = 5;
					re.SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
				}
			} else {
				if ( color != 3 ) {
					color = 3;
					re.SetColor( g_color_table[ColorIndex(COLOR_GREEN)] );
				}
			}
			v *= vscale;
			if ( v > range ) v = range;
			re.DrawStretchPic( ax + aw - a, ay + ah - v, 1, v, 0, 0, 0, 0, white );
		} else if ( v < 0 ) {
			if ( color != 4 ) {
				color = 4;
				re.SetColor( g_color_table[ColorIndex(COLOR_RED)] );
			}
			re.DrawStretchPic( ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, white );
		}
	}

	re.SetColor( NULL );
}

void CG_SHUDElementNGDestroy(void* context)
{
	if ( context ) {
		Z_Free( context );
	}
}
#endif // FEAT_WIRED_UI
