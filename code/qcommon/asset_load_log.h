/*
===========================================================================
asset_load_log.h — grouped asset-failure logging

Ring buffer per subsystem. Records are flushed as a single brace-expanded
line when the parent directory changes, 100 ms elapses with no new push,
the ring reaches 64 records, or the subsystem is shut down.

Output examples:
  ^3sound/voices/female2/{or_01,or_02,or_03}.{wav,opus} not found, default beep will be used
  sound/voices/female2/{or_01,or_02,or_03}.wav not present, .opus found instead
  ^3characters/visor/models/{visor_default}.{tga,jpg,png} not found (referenced by shader 'characters/visor/models/visor_default'), no texture
===========================================================================
*/

#ifndef ASSET_LOAD_LOG_H
#define ASSET_LOAD_LOG_H

typedef enum {
	ASSET_LOG_INFO,		// Com_Log info  — fallback found, informational only
	ASSET_LOG_WARN		// Com_Log yellow — all fallbacks exhausted
} assetLogSeverity_t;

/*
AssetLog_Event
  subsystem       — "sound", "image", "model", etc.
  full_path       — path stem without extension, e.g. "sound/voices/female2/or_01"
  extensions_tried — comma-separated tried exts, e.g. "wav,opus" or "tga,jpg,png"
                     For INFO fallback use "tried>found", e.g. "wav>opus"
  shader_context  — for images: the shader name that referenced the texture; NULL otherwise
  severity        — ASSET_LOG_INFO or ASSET_LOG_WARN
*/
void AssetLog_Event( const char *subsystem,
                     const char *full_path,
                     const char *extensions_tried,
                     const char *shader_context,
                     assetLogSeverity_t severity );

/* Force-flush one subsystem's pending records. Pass NULL to flush all. */
void AssetLog_Flush( const char *subsystem );

/* Call once per frame (in Com_Frame) to flush groups idle for >=100 ms. */
void AssetLog_Tick( void );

/*
AssetLog_EventMultiPath — two-axis grouped logging for multi-tier paths.

  common_prefix  — shared leading path, e.g. "characters/"
  path_variants  — array of middle-segment strings, e.g. {"visor","_archetypes/mechanized_male","_archetypes/_base"}
  num_variants   — length of path_variants
  common_suffix  — shared trailing path before basename, e.g. "/sounds/"
  basename       — filename stem (no extension), accumulates across calls with same key

  Output: characters/{visor,_archetypes/mechanized_male,_archetypes/_base}/sounds/{jump,falling}.opus not found, ...

  Group key: (subsystem, common_prefix, common_suffix, path_variants[], extensions_tried, severity, shader_context).
  Same flush triggers as AssetLog_Event.
*/
void AssetLog_EventMultiPath( const char *subsystem,
                               const char *common_prefix,
                               const char * const *path_variants,
                               int num_variants,
                               const char *common_suffix,
                               const char *basename,
                               const char *extensions_tried,
                               const char *shader_context,
                               assetLogSeverity_t severity );

#endif /* ASSET_LOAD_LOG_H */
