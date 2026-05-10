/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)
Copyright (C) 2024-2026 Wired engine contributors

This file is part of the Wired Engine source code. GPLv2.
===========================================================================
*/

// snd_codec_init.c — engine-side codec init/shutdown.
//
// Split out from snd_codec.c so the dispatcher TU can be linked into
// offline tools (extract-meta) that need S_CodecResolves but don't
// link any codec implementation. Tool-side targets compile snd_codec.c
// only; engine targets compile both.

#include "client.h"
#include "snd_codec.h"

/*
=================
S_CodecInit
=================
*/
void S_CodecInit( void )
{
	S_CodecResetList();

#if FEAT_LEGACY_FORMATS_AUDIO
#ifdef USE_OGG_VORBIS
	S_CodecRegister( &ogg_codec );
#endif
	S_CodecRegister( &wav_codec );
#endif // FEAT_LEGACY_FORMATS_AUDIO

	// Register opus codec last so it is head of list (tried first in fallback)
	S_CodecRegister( &opus_codec );
}


/*
=================
S_CodecShutdown
=================
*/
void S_CodecShutdown( void )
{
	S_CodecResetList();
}
