/*
===========================================================================
asset_load_log.c — grouped asset-failure logging

See asset_load_log.h for the public API and output format.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "asset_load_log.h"

#define ASSET_LOG_MAX_SUBSYSTEMS	8
#define ASSET_LOG_RING_SIZE			64
#define ASSET_LOG_IDLE_MS			100
#define ASSET_LOG_MAX_BASENAME		128		/* long enough for any pak-internal filename */

typedef struct {
	char	basename[ASSET_LOG_MAX_BASENAME];
} assetLogBasename_t;

typedef struct {
	qboolean			active;
	char				subsystem[32];

	/* Current group key — all records in the buffer share these. */
	char				parent_dir[MAX_VFS_PATH];
	char				extensions_tried[64];
	char				shader_context[MAX_QPATH];
	assetLogSeverity_t	severity;

	assetLogBasename_t	basenames[ASSET_LOG_RING_SIZE];
	int					count;
	int					last_push_ms;
} assetLogBuffer_t;

static assetLogBuffer_t	s_logBuffers[ASSET_LOG_MAX_SUBSYSTEMS];
static int				s_numLogBuffers;


static assetLogBuffer_t *FindOrCreateBuffer( const char *subsystem ) {
	int i;

	for ( i = 0; i < s_numLogBuffers; i++ ) {
		if ( Q_stricmp( s_logBuffers[i].subsystem, subsystem ) == 0 )
			return &s_logBuffers[i];
	}

	if ( s_numLogBuffers >= ASSET_LOG_MAX_SUBSYSTEMS )
		return NULL;

	assetLogBuffer_t *buf = &s_logBuffers[s_numLogBuffers++];
	memset( buf, 0, sizeof(*buf) );
	Q_strncpyz( buf->subsystem, subsystem, sizeof(buf->subsystem) );
	return buf;
}


static void FlushBuffer( assetLogBuffer_t *buf ) {
	int		i, pos, len;
	char	basenames_str[ASSET_LOG_RING_SIZE * ASSET_LOG_MAX_BASENAME + 4];
	char	ext_display[80];
	char	fallback_ext[32];
	char	suffix[MAX_QPATH + 64];

	if ( buf->count == 0 ) {
		buf->active = qfalse;
		return;
	}

	/* ---- basenames portion ---- */
	pos = 0;
	if ( buf->count == 1 ) {
		Q_strncpyz( basenames_str, buf->basenames[0].basename, sizeof(basenames_str) );
	} else {
		basenames_str[pos++] = '{';
		for ( i = 0; i < buf->count; i++ ) {
			if ( i > 0 ) basenames_str[pos++] = ',';
			len = (int)strlen( buf->basenames[i].basename );
			if ( pos + len + 2 >= (int)sizeof(basenames_str) )
				break;
			memcpy( basenames_str + pos, buf->basenames[i].basename, len );
			pos += len;
		}
		basenames_str[pos++] = '}';
		basenames_str[pos]   = '\0';
	}

	/* ---- extension display ---- */
	fallback_ext[0] = '\0';
	{
		const char *arrow = strchr( buf->extensions_tried, '>' );
		if ( arrow ) {
			/* INFO fallback notation: "wav>opus" → display ".wav", suffix names ".opus" */
			int tried_len = (int)( arrow - buf->extensions_tried );
			Com_sprintf( ext_display, sizeof(ext_display), ".%.*s", tried_len, buf->extensions_tried );
			Q_strncpyz( fallback_ext, arrow + 1, sizeof(fallback_ext) );
		} else if ( strchr( buf->extensions_tried, ',' ) ) {
			Com_sprintf( ext_display, sizeof(ext_display), ".{%s}", buf->extensions_tried );
		} else {
			Com_sprintf( ext_display, sizeof(ext_display), ".%s", buf->extensions_tried );
		}
	}

	/* ---- message suffix ---- */
	if ( fallback_ext[0] ) {
		Com_sprintf( suffix, sizeof(suffix), " not present, .%s found instead", fallback_ext );
	} else if ( buf->shader_context[0] ) {
		Com_sprintf( suffix, sizeof(suffix),
		             " not found (referenced by shader '%s'), no texture",
		             buf->shader_context );
	} else {
		Q_strncpyz( suffix, " not found, default beep will be used", sizeof(suffix) );
	}

	/* ---- emit ---- */
	if ( buf->parent_dir[0] ) {
		if ( buf->severity == ASSET_LOG_INFO )
			Com_Log( SEV_DEBUG, LOG_CAT_SYSTEM, "%s/%s%s%s\n",
			             buf->parent_dir, basenames_str, ext_display, suffix );
		else
			COM_WARN( LOG_CAT_SYSTEM, "%s/%s%s%s\n",
			            buf->parent_dir, basenames_str, ext_display, suffix );
	} else {
		if ( buf->severity == ASSET_LOG_INFO )
			Com_Log( SEV_DEBUG, LOG_CAT_SYSTEM, "%s%s%s\n", basenames_str, ext_display, suffix );
		else
			COM_WARN( LOG_CAT_SYSTEM, "%s%s%s\n", basenames_str, ext_display, suffix );
	}

	buf->count  = 0;
	buf->active = qfalse;
}


void AssetLog_Event( const char *subsystem,
                     const char *full_path,
                     const char *extensions_tried,
                     const char *shader_context,
                     assetLogSeverity_t severity ) {
	assetLogBuffer_t	*buf;
	const char			*slash;
	char				parent_dir[MAX_VFS_PATH];
	char				basename[ASSET_LOG_MAX_BASENAME];
	const char			*sc;
	int					i;

	if ( !subsystem || !full_path || !extensions_tried )
		return;

	buf = FindOrCreateBuffer( subsystem );
	if ( !buf ) {
		/* table full — emit directly without grouping */
		if ( severity == ASSET_LOG_INFO )
			Com_Log( SEV_DEBUG, LOG_CAT_SYSTEM, "%s.%s: fallback found\n", full_path, extensions_tried );
		else
			COM_WARN( LOG_CAT_SYSTEM, "%s.%s not found\n", full_path, extensions_tried );
		return;
	}

	/* split full_path → parent_dir + basename */
	slash = strrchr( full_path, '/' );
	if ( slash ) {
		int dir_len = (int)( slash - full_path );
		Q_strncpyz( parent_dir, full_path, dir_len + 1 );
		Q_strncpyz( basename, slash + 1, sizeof(basename) );
	} else {
		parent_dir[0] = '\0';
		Q_strncpyz( basename, full_path, sizeof(basename) );
	}

	sc = shader_context ? shader_context : "";

	/* flush if group key changed or ring is full */
	if ( buf->active ) {
		qboolean key_changed =
			strcmp( buf->parent_dir,       parent_dir       ) != 0 ||
			strcmp( buf->extensions_tried, extensions_tried ) != 0 ||
			buf->severity                 != severity             ||
			strcmp( buf->shader_context,   sc               ) != 0;

		if ( key_changed || buf->count >= ASSET_LOG_RING_SIZE )
			FlushBuffer( buf );
	}

	/* set group key when starting a fresh group */
	if ( !buf->active ) {
		Q_strncpyz( buf->parent_dir,       parent_dir,       sizeof(buf->parent_dir)       );
		Q_strncpyz( buf->extensions_tried, extensions_tried, sizeof(buf->extensions_tried) );
		Q_strncpyz( buf->shader_context,   sc,               sizeof(buf->shader_context)   );
		buf->severity = severity;
		buf->active   = qtrue;
	}

	/* skip duplicates */
	for ( i = 0; i < buf->count; i++ ) {
		if ( strcmp( buf->basenames[i].basename, basename ) == 0 )
			return;
	}

	Q_strncpyz( buf->basenames[buf->count].basename, basename, ASSET_LOG_MAX_BASENAME );
	buf->count++;
	buf->last_push_ms = Sys_Milliseconds();
}


/* ======================================================================
 * Two-axis (multi-path) grouped logging
 * ====================================================================== */

#define ASSET_LOG_MAX_MP_BUFFERS	4
#define ASSET_LOG_MAX_VARIANTS		8

typedef struct {
	char	v[MAX_VFS_PATH];
} assetLogVariant_t;

typedef struct {
	qboolean			active;
	char				subsystem[32];

	char				common_prefix[MAX_VFS_PATH];
	char				common_suffix[MAX_VFS_PATH];
	assetLogVariant_t	variants[ASSET_LOG_MAX_VARIANTS];
	int					num_variants;
	char				extensions_tried[64];
	char				shader_context[MAX_QPATH];
	assetLogSeverity_t	severity;

	assetLogBasename_t	basenames[ASSET_LOG_RING_SIZE];
	int					count;
	int					last_push_ms;
} assetLogMPBuffer_t;

static assetLogMPBuffer_t	s_mpBuffers[ASSET_LOG_MAX_MP_BUFFERS];
static int					s_numMPBuffers;


static qboolean VariantsMatch( const assetLogMPBuffer_t *buf,
                                const char * const *variants, int num_variants ) {
	int i;
	if ( buf->num_variants != num_variants )
		return qfalse;
	for ( i = 0; i < num_variants; i++ ) {
		if ( strcmp( buf->variants[i].v, variants[i] ) != 0 )
			return qfalse;
	}
	return qtrue;
}


static void FlushMPBuffer( assetLogMPBuffer_t *buf ) {
	int		i, pos, len;
	char	variants_str[ASSET_LOG_MAX_VARIANTS * MAX_VFS_PATH + 4];
	char	basenames_str[ASSET_LOG_RING_SIZE * ASSET_LOG_MAX_BASENAME + 4];
	char	ext_display[80];
	char	fallback_ext[32];
	char	suffix[MAX_QPATH + 64];

	if ( buf->count == 0 ) {
		buf->active = qfalse;
		return;
	}

	/* ---- variants portion ---- */
	pos = 0;
	if ( buf->num_variants == 1 ) {
		Q_strncpyz( variants_str, buf->variants[0].v, sizeof(variants_str) );
	} else {
		variants_str[pos++] = '{';
		for ( i = 0; i < buf->num_variants; i++ ) {
			if ( i > 0 ) variants_str[pos++] = ',';
			len = (int)strlen( buf->variants[i].v );
			if ( pos + len + 2 >= (int)sizeof(variants_str) ) break;
			memcpy( variants_str + pos, buf->variants[i].v, len );
			pos += len;
		}
		variants_str[pos++] = '}';
		variants_str[pos]   = '\0';
	}

	/* ---- basenames portion ---- */
	pos = 0;
	if ( buf->count == 1 ) {
		Q_strncpyz( basenames_str, buf->basenames[0].basename, sizeof(basenames_str) );
	} else {
		basenames_str[pos++] = '{';
		for ( i = 0; i < buf->count; i++ ) {
			if ( i > 0 ) basenames_str[pos++] = ',';
			len = (int)strlen( buf->basenames[i].basename );
			if ( pos + len + 2 >= (int)sizeof(basenames_str) ) break;
			memcpy( basenames_str + pos, buf->basenames[i].basename, len );
			pos += len;
		}
		basenames_str[pos++] = '}';
		basenames_str[pos]   = '\0';
	}

	/* ---- extension display (same logic as FlushBuffer) ---- */
	fallback_ext[0] = '\0';
	{
		const char *arrow = strchr( buf->extensions_tried, '>' );
		if ( arrow ) {
			int tried_len = (int)( arrow - buf->extensions_tried );
			Com_sprintf( ext_display, sizeof(ext_display), ".%.*s", tried_len, buf->extensions_tried );
			Q_strncpyz( fallback_ext, arrow + 1, sizeof(fallback_ext) );
		} else if ( strchr( buf->extensions_tried, ',' ) ) {
			Com_sprintf( ext_display, sizeof(ext_display), ".{%s}", buf->extensions_tried );
		} else {
			Com_sprintf( ext_display, sizeof(ext_display), ".%s", buf->extensions_tried );
		}
	}

	/* ---- message suffix ---- */
	if ( fallback_ext[0] ) {
		Com_sprintf( suffix, sizeof(suffix), " not present, .%s found instead", fallback_ext );
	} else if ( buf->shader_context[0] ) {
		Com_sprintf( suffix, sizeof(suffix),
		             " not found (referenced by shader '%s'), no texture",
		             buf->shader_context );
	} else {
		Q_strncpyz( suffix, " not found, default beep will be used", sizeof(suffix) );
	}

	/* Format: {common_prefix}{variants}{common_suffix}{basenames}{ext} {message} */
	if ( buf->severity == ASSET_LOG_INFO )
		Com_Log( SEV_DEBUG, LOG_CAT_SYSTEM, "%s%s%s%s%s%s\n",
		             buf->common_prefix, variants_str, buf->common_suffix,
		             basenames_str, ext_display, suffix );
	else
		COM_WARN( LOG_CAT_SYSTEM, "%s%s%s%s%s%s\n",
		            buf->common_prefix, variants_str, buf->common_suffix,
		            basenames_str, ext_display, suffix );

	buf->count  = 0;
	buf->active = qfalse;
}


void AssetLog_EventMultiPath( const char *subsystem,
                               const char *common_prefix,
                               const char * const *path_variants,
                               int num_variants,
                               const char *common_suffix,
                               const char *basename,
                               const char *extensions_tried,
                               const char *shader_context,
                               assetLogSeverity_t severity ) {
	assetLogMPBuffer_t	*buf;
	const char			*sc;
	int					i;

	if ( !subsystem || !common_prefix || !path_variants ||
	     num_variants <= 0 || !basename || !extensions_tried )
		return;

	if ( num_variants > ASSET_LOG_MAX_VARIANTS )
		num_variants = ASSET_LOG_MAX_VARIANTS;

	buf = NULL;
	for ( i = 0; i < s_numMPBuffers; i++ ) {
		if ( Q_stricmp( s_mpBuffers[i].subsystem, subsystem ) == 0 ) {
			buf = &s_mpBuffers[i];
			break;
		}
	}
	if ( !buf ) {
		if ( s_numMPBuffers >= ASSET_LOG_MAX_MP_BUFFERS ) {
			if ( severity == ASSET_LOG_INFO )
				Com_Log( SEV_DEBUG, LOG_CAT_SYSTEM, "%s.../%s.%s: fallback found\n",
				             common_prefix, basename, extensions_tried );
			else
				COM_WARN( LOG_CAT_SYSTEM, "%s.../%s.%s not found\n",
				            common_prefix, basename, extensions_tried );
			return;
		}
		buf = &s_mpBuffers[s_numMPBuffers++];
		memset( buf, 0, sizeof(*buf) );
		Q_strncpyz( buf->subsystem, subsystem, sizeof(buf->subsystem) );
	}

	sc = shader_context ? shader_context : "";

	if ( buf->active ) {
		qboolean key_changed =
			strcmp( buf->common_prefix,    common_prefix    ) != 0 ||
			strcmp( buf->common_suffix,    common_suffix    ) != 0 ||
			strcmp( buf->extensions_tried, extensions_tried ) != 0 ||
			buf->severity                 != severity             ||
			strcmp( buf->shader_context,   sc               ) != 0 ||
			!VariantsMatch( buf, path_variants, num_variants );

		if ( key_changed || buf->count >= ASSET_LOG_RING_SIZE )
			FlushMPBuffer( buf );
	}

	if ( !buf->active ) {
		Q_strncpyz( buf->common_prefix,    common_prefix,    sizeof(buf->common_prefix)    );
		Q_strncpyz( buf->common_suffix,    common_suffix,    sizeof(buf->common_suffix)    );
		Q_strncpyz( buf->extensions_tried, extensions_tried, sizeof(buf->extensions_tried) );
		Q_strncpyz( buf->shader_context,   sc,               sizeof(buf->shader_context)   );
		buf->severity     = severity;
		buf->num_variants = num_variants;
		for ( i = 0; i < num_variants; i++ )
			Q_strncpyz( buf->variants[i].v, path_variants[i], sizeof(buf->variants[i].v) );
		buf->active = qtrue;
	}

	for ( i = 0; i < buf->count; i++ ) {
		if ( strcmp( buf->basenames[i].basename, basename ) == 0 )
			return;
	}

	Q_strncpyz( buf->basenames[buf->count].basename, basename, ASSET_LOG_MAX_BASENAME );
	buf->count++;
	buf->last_push_ms = Sys_Milliseconds();
}


void AssetLog_Flush( const char *subsystem ) {
	int i;

	if ( subsystem ) {
		for ( i = 0; i < s_numLogBuffers; i++ ) {
			if ( Q_stricmp( s_logBuffers[i].subsystem, subsystem ) == 0 ) {
				FlushBuffer( &s_logBuffers[i] );
				return;
			}
		}
		for ( i = 0; i < s_numMPBuffers; i++ ) {
			if ( Q_stricmp( s_mpBuffers[i].subsystem, subsystem ) == 0 ) {
				FlushMPBuffer( &s_mpBuffers[i] );
				return;
			}
		}
	} else {
		for ( i = 0; i < s_numLogBuffers; i++ )
			FlushBuffer( &s_logBuffers[i] );
		for ( i = 0; i < s_numMPBuffers; i++ )
			FlushMPBuffer( &s_mpBuffers[i] );
	}
}


void AssetLog_Tick( void ) {
	int	i, now;

	if ( s_numLogBuffers == 0 && s_numMPBuffers == 0 )
		return;

	now = Sys_Milliseconds();
	for ( i = 0; i < s_numLogBuffers; i++ ) {
		assetLogBuffer_t *buf = &s_logBuffers[i];
		if ( buf->active && buf->count > 0 &&
		     now - buf->last_push_ms >= ASSET_LOG_IDLE_MS )
			FlushBuffer( buf );
	}
	for ( i = 0; i < s_numMPBuffers; i++ ) {
		assetLogMPBuffer_t *buf = &s_mpBuffers[i];
		if ( buf->active && buf->count > 0 &&
		     now - buf->last_push_ms >= ASSET_LOG_IDLE_MS )
			FlushMPBuffer( buf );
	}
}
