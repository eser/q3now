/*
cl_wired_hud_elem_table.c -- Generic data-driven TABLE widget renderer.
*/

#include "../../../client.h"
#include "cl_wired_hud.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_store.h"
#include "cl_wired_text.h"
#include "cl_wired_draw.h"
#include "cl_wired_ui.h"

#if FEAT_WIRED_UI

/*
===========================================================================
WiredHud_DrawTable -- render a data-driven table from store keys

The table reads:
  - Row count from tableCountBind store key
  - Per-cell text from source.N.field
  - Per-cell color from source.N.colorfield (optional)
  - Per-cell icon from source.N.iconfield (optional)

Team filtering: if tableTeamFilter >= 0, only rows where
  source.N.team matches are displayed.
===========================================================================
*/
void WiredHud_DrawTable( wiredItemDef_t *item, float ox, float oy, float ow, float oh,
                          int fontId, float fontSize ) {
	char keyBuf[256];
	vec4_t headerColor;
	vec4_t rowBg;
	vec4_t cellColor;

	if ( !item->tableCountBind[0] || !item->tableSource[0] || item->numTableColumns <= 0 ) {
		return;
	}

	wuiStoreEntry_t *countEntry = WiredStore_Get( item->tableCountBind );
	if ( !countEntry ) return;
	int totalRows = (int)countEntry->value;
	if ( totalRows <= 0 ) return;

	/* sizing */
	float headerH = fontSize * 1.8f;
	float rowH = fontSize * 1.4f;

	/* header color -- slightly dimmed white */
	Vector4Set( headerColor, 0.7f, 0.7f, 0.8f, 1.0f );

	/* -- draw column headers ------------------------------------------- */
	float cx = ox;
	for ( int c = 0; c < item->numTableColumns; c++ ) {
		wuiTableColumn_t *col = &item->tableColumns[c];
		float colW = col->width * ow;
		if ( col->header[0] ) {
			float tx;
			if ( col->align == 2 ) {
				tx = cx + colW;  /* right-align */
			} else if ( col->align == 1 ) {
				tx = cx + colW * 0.5f;  /* center */
			} else {
				tx = cx;  /* left */
			}
			Text_Draw( col->header, tx, oy, fontId, fontSize * 0.85f, headerColor, col->align, 0 );
		}
		cx += colW;
	}

	/* -- separator line ------------------------------------------------ */
	{
		vec4_t sepColor;
		Vector4Set( sepColor, 0.5f, 0.5f, 0.6f, 0.4f );
		WUI_FillRect( ox, oy + headerH - 2, ow, 1, sepColor );
	}

	/* -- draw data rows ------------------------------------------------ */
	float y = oy + headerH;
	int visibleRow = 0;
	for ( int i = 0; i < totalRows && y + rowH <= oy + oh; i++ ) {
		/* team filter */
		if ( item->tableTeamFilter >= 0 ) {
			wuiStoreEntry_t *teamEntry;
			Com_sprintf( keyBuf, sizeof( keyBuf ), "%s.%d.team", item->tableSource, i );
			teamEntry = WiredStore_Get( keyBuf );
			if ( teamEntry && (int)teamEntry->value != item->tableTeamFilter ) {
				continue;
			}
		}

		/* alternating row background */
		if ( visibleRow % 2 == 0 ) {
			Vector4Set( rowBg, 0.0f, 0.0f, 0.0f, 0.15f );
		} else {
			Vector4Set( rowBg, 0.1f, 0.1f, 0.15f, 0.15f );
		}

		/* check for player highlight */
		{
			wuiStoreEntry_t *hlEntry;
			Com_sprintf( keyBuf, sizeof( keyBuf ), "%s.%d.highlight", item->tableSource, i );
			hlEntry = WiredStore_Get( keyBuf );
			if ( hlEntry && hlEntry->value > 0.0f ) {
				Vector4Set( rowBg, 0.1f, 0.15f, 0.3f, 0.3f );
			}
		}

		/* draw row background */
		WUI_FillRect( ox, y, ow, rowH, rowBg );

		/* draw cells */
		cx = ox;
		for ( int c = 0; c < item->numTableColumns; c++ ) {
			wuiTableColumn_t *col = &item->tableColumns[c];
			float colW = col->width * ow;

			/* build cell text key */
			Com_sprintf( keyBuf, sizeof( keyBuf ), "%s.%d.%s",
			             item->tableSource, i, col->field );
			wuiStoreEntry_t *cellEntry = WiredStore_Get( keyBuf );

			if ( cellEntry && cellEntry->text[0] ) {
				float tx;

				/* determine cell color */
				Vector4Copy( item->forecolor, cellColor );
				if ( col->colorfield[0] ) {
					Com_sprintf( keyBuf, sizeof( keyBuf ), "%s.%d.%s",
					             item->tableSource, i, col->colorfield );
					wuiStoreEntry_t *colorEntry = WiredStore_Get( keyBuf );
					if ( colorEntry ) {
						Vector4Copy( colorEntry->color, cellColor );
					}
				}

				/* position based on alignment */
				if ( col->align == 2 ) {
					tx = cx + colW;
				} else if ( col->align == 1 ) {
					tx = cx + colW * 0.5f;
				} else {
					tx = cx;
				}

				Text_Draw( cellEntry->text, tx, y + rowH * 0.15f,
				           fontId, fontSize, cellColor, col->align, 0 );
			}

			/* icon rendering */
			if ( col->iconfield[0] ) {
				wuiStoreEntry_t *iconEntry;
				Com_sprintf( keyBuf, sizeof( keyBuf ), "%s.%d.%s",
				             item->tableSource, i, col->iconfield );
				iconEntry = WiredStore_Get( keyBuf );
				if ( iconEntry && iconEntry->icon ) {
					float iconSz = rowH * 0.8f;
					re.SetColor( NULL );
					re.DrawStretchPic( cx + 2, y + (rowH - iconSz) * 0.5f,
					                   iconSz, iconSz, 0, 0, 1, 1, iconEntry->icon );
				}
			}

			cx += colW;
		}

		visibleRow++;
		y += rowH;
	}
}

#endif /* FEAT_WIRED_UI */
