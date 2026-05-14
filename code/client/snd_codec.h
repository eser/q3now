// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2005 Stuart Dalton (badcdev@gmail.com)
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef _SND_CODEC_H_
#define _SND_CODEC_H_

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/q_feats.h"

typedef struct snd_info_s
{
	int rate;
	int width;
	int channels;
	int samples;
	int size;
	int dataofs;
	int loopStart;  // sample offset for loop start (-1 if not set)
	int loopEnd;    // sample offset for loop end (-1 if not set)
} snd_info_t;

typedef struct snd_codec_s snd_codec_t;

typedef struct snd_stream_s
{
	snd_codec_t *codec;
	fileHandle_t file;
	snd_info_t info;
	int length;
	int pos;
	void *ptr;
} snd_stream_t;

// Codec functions
typedef void *(*CODEC_LOAD)(const char *filename, snd_info_t *info);
typedef snd_stream_t *(*CODEC_OPEN)(const char *filename);
typedef int (*CODEC_READ)(snd_stream_t *stream, int bytes, void *buffer);
typedef void (*CODEC_CLOSE)(snd_stream_t *stream);

// Codec data structure
struct snd_codec_s
{
	const char *ext;
	CODEC_LOAD load;
	CODEC_OPEN open;
	CODEC_READ read;
	CODEC_CLOSE close;
	snd_codec_t *next;
};

// Codec management
void S_CodecInit( void );
void S_CodecShutdown( void );
void *S_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_CodecOpenStream(const char *filename);
void S_CodecCloseStream(snd_stream_t *stream);
int S_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);

// Probe whether `name` resolves to any registered codec format,
// applying the same path normalization and extension fallback as
// S_CodecGetSound. Does NOT allocate streams or decode buffers.
// Walks the static codec extension priority list, not the dynamic
// `codecs` linked list — safe to call from offline tools that link
// snd_codec.c without codec implementations or calling S_CodecInit.
qboolean S_CodecResolves( const char *name );

// Exposed so snd_codec_init.c can register codecs from a separate TU.
// (Was static in pre-split snd_codec.c.)
void S_CodecRegister( snd_codec_t *codec );

// Clear the dispatcher list — called at the top of S_CodecInit to
// support engine restart paths that re-register codecs from scratch.
void S_CodecResetList( void );

// Util functions (used by codecs)
snd_stream_t *S_CodecUtilOpen(const char *filename, snd_codec_t *codec);
void S_CodecUtilClose(snd_stream_t **stream);

// WAV Codec
#if FEAT_LEGACY_FORMATS_AUDIO
extern snd_codec_t wav_codec;
void *S_WAV_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_WAV_CodecOpenStream(const char *filename);
void S_WAV_CodecCloseStream(snd_stream_t *stream);
int S_WAV_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);

// Ogg Vorbis codec
#ifdef USE_OGG_VORBIS
extern snd_codec_t ogg_codec;
void *S_OGG_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_OGG_CodecOpenStream(const char *filename);
void S_OGG_CodecCloseStream(snd_stream_t *stream);
int S_OGG_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);
#endif // USE_OGG_VORBIS
#endif // FEAT_LEGACY_FORMATS_AUDIO

// Opus codec
extern snd_codec_t opus_codec;
void *S_OPUS_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_OPUS_CodecOpenStream(const char *filename);
void S_OPUS_CodecCloseStream(snd_stream_t *stream);
int S_OPUS_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);

#endif // !_SND_CODEC_H_
