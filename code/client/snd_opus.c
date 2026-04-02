/*
===========================================================================
Copyright (C) 2024-2026 q3now contributors

This file is part of q3now source code.

q3now source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

q3now source code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with q3now source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		snd_opus.c
 *
 * desc:		Opus in-memory compression for sound effects
 *              (soundCompressionMethod == 4)
 *
 *              Encode path: PCM samples -> Opus frames stored in sndBuffer chain
 *              Decode path: sndBuffer chain -> opus_decode() -> sample buffer
 *
 *              Storage format in sndBuffer.sndChunk (2048 bytes per chunk):
 *                [frame_len:2 bytes][frame_data:frame_len bytes][frame_len:2 bytes]...
 *              sndBuffer.size = total bytes used in this chunk.
 *
 *****************************************************************************/

#include "snd_local.h"
#include <opus.h>

// ── static decoder (shared, reset on sfx switch) ──────────────────────────

static OpusDecoder *s_opusDecoder    = NULL;
static const sfx_t *s_opusDecoderSfx = NULL;  // which sfx the decoder state belongs to
static int          s_opusDecoderFrameIdx = -1; // last decoded frame index

// Scratch buffer for one decoded Opus frame (960 mono samples)
static short s_opusDecodeBuf[OPUS_INMEM_FRAME_SAMPLES];
static int   s_opusDecodeBufValid = 0; // number of valid samples in decode buf


/*
====================
S_OpusDecoderInit

Called once during sound system initialisation.
====================
*/
void S_OpusDecoderInit( void )
{
	int err;

	if ( s_opusDecoder ) {
		return; // already initialised
	}

	s_opusDecoder = opus_decoder_create( OPUS_INMEM_RATE, 1, &err );
	if ( err != OPUS_OK || !s_opusDecoder ) {
		Com_Printf( S_COLOR_RED "ERROR: opus_decoder_create failed (%d)\n", err );
		s_opusDecoder = NULL;
		return;
	}

	s_opusDecoderSfx = NULL;
	s_opusDecoderFrameIdx = -1;
	s_opusDecodeBufValid = 0;

	Com_Printf( "Opus in-memory decoder initialised\n" );
}


/*
====================
S_OpusDecoderShutdown
====================
*/
void S_OpusDecoderShutdown( void )
{
	if ( s_opusDecoder ) {
		opus_decoder_destroy( s_opusDecoder );
		s_opusDecoder = NULL;
	}
	s_opusDecoderSfx = NULL;
	s_opusDecoderFrameIdx = -1;
	s_opusDecodeBufValid = 0;
}


// ── sndBuffer byte-stream helpers ─────────────────────────────────────────

/*
 * Read raw bytes from the packed sndBuffer chain starting at a given
 * byte offset.  Returns the number of bytes actually read.
 */
static int S_OpusChunkRead( const sfx_t *sc, int byteOffset,
                            byte *out, int len )
{
	sndBuffer *chunk = sc->soundData;
	int nread = 0;

	// skip whole chunks
	while ( chunk && byteOffset >= chunk->size ) {
		byteOffset -= chunk->size;
		chunk = chunk->next;
	}

	while ( chunk && len > 0 ) {
		int avail = chunk->size - byteOffset;
		int n = ( len < avail ) ? len : avail;
		Com_Memcpy( out + nread, (byte *)chunk->sndChunk + byteOffset, n );
		nread += n;
		len -= n;
		byteOffset = 0;
		chunk = chunk->next;
	}

	return nread;
}


/*
 * Append raw bytes to the sndBuffer chain during encoding.
 * *pChunk points to the current chunk, *pOffset to the byte offset inside it.
 * When the current chunk is full a new one is allocated and linked.
 * sfx->soundData is set to the first chunk if it was NULL.
 */
static void S_OpusChunkWrite( sfx_t *sfx, sndBuffer **pChunk, int *pOffset,
                              const byte *data, int len )
{
	while ( len > 0 ) {
		sndBuffer *chunk = *pChunk;

		if ( !chunk || *pOffset >= SND_CHUNK_SIZE_BYTE ) {
			sndBuffer *newchunk = SND_malloc();
			newchunk->size = 0;
			if ( !sfx->soundData ) {
				sfx->soundData = newchunk;
			}
			if ( chunk ) {
				chunk->next = newchunk;
			}
			*pChunk = newchunk;
			*pOffset = 0;
			chunk = newchunk;
		}

		{
			int space = SND_CHUNK_SIZE_BYTE - *pOffset;
			int n = ( len < space ) ? len : space;
			byte *dst = (byte *)chunk->sndChunk + *pOffset;
			Com_Memcpy( dst, data, n );
			*pOffset += n;
			chunk->size = *pOffset;
			data += n;
			len -= n;
		}
	}
}


// ── encoding ──────────────────────────────────────────────────────────────

/*
====================
S_OpusEncodeSound

Encode mono PCM samples (resampled to OPUS_INMEM_RATE by the caller)
into Opus frames and store them in the sndBuffer chain on sfx->soundData.

Storage: packed sequence of [uint16 frame_len][frame_data ...] across chunks.
Each Opus frame covers OPUS_INMEM_FRAME_SAMPLES (960) samples = 20 ms at 48 kHz.
====================
*/
void S_OpusEncodeSound( sfx_t *sfx, short *samples )
{
	OpusEncoder *enc;
	int err;
	int totalSamples = sfx->soundLength;
	int offset = 0;
	sndBuffer *chunk = NULL;
	int chunkOffset = 0;
	unsigned char encBuf[4000]; // safe upper bound per Opus docs

	enc = opus_encoder_create( OPUS_INMEM_RATE, 1, OPUS_APPLICATION_AUDIO, &err );
	if ( err != OPUS_OK || !enc ) {
		Com_Printf( S_COLOR_RED "ERROR: S_OpusEncodeSound: opus_encoder_create failed (%d)\n", err );
		return;
	}

	opus_encoder_ctl( enc, OPUS_SET_BITRATE( 128000 ) );

	sfx->soundData = NULL;

	while ( offset < totalSamples ) {
		short frameBuf[OPUS_INMEM_FRAME_SAMPLES];
		int remaining = totalSamples - offset;
		int frameSamples = ( remaining >= OPUS_INMEM_FRAME_SAMPLES )
		                   ? OPUS_INMEM_FRAME_SAMPLES : remaining;
		int encBytes;
		unsigned short frameLen;

		Com_Memcpy( frameBuf, samples + offset, frameSamples * sizeof( short ) );
		if ( frameSamples < OPUS_INMEM_FRAME_SAMPLES ) {
			Com_Memset( frameBuf + frameSamples, 0,
			            ( OPUS_INMEM_FRAME_SAMPLES - frameSamples ) * sizeof( short ) );
		}

		encBytes = opus_encode( enc, frameBuf, OPUS_INMEM_FRAME_SAMPLES,
		                        encBuf, sizeof( encBuf ) );
		if ( encBytes < 0 ) {
			Com_Printf( S_COLOR_RED "ERROR: opus_encode failed (%d) at offset %d\n",
			            encBytes, offset );
			break;
		}

		// write 2-byte frame length (little-endian via native short)
		frameLen = (unsigned short)encBytes;
		S_OpusChunkWrite( sfx, &chunk, &chunkOffset,
		                  (const byte *)&frameLen, 2 );

		// write encoded frame data
		S_OpusChunkWrite( sfx, &chunk, &chunkOffset, encBuf, encBytes );

		offset += OPUS_INMEM_FRAME_SAMPLES;
	}

	opus_encoder_destroy( enc );

	Com_DPrintf( "Opus encoded %s: %d samples -> sndBuffer chain\n",
	             sfx->soundName, totalSamples );
}


// ── decoding ──────────────────────────────────────────────────────────────

/*
 * Walk the packed frame headers to find the byte offset of frame `frameIdx`.
 * Returns the byte offset of the 2-byte length field, or -1 on error.
 * Writes the encoded frame size into *frameLen.
 */
static int S_OpusFindFrame( const sfx_t *sc, int frameIdx, int *frameLen )
{
	int byteOfs = 0;
	unsigned short len16;
	int i;

	for ( i = 0; i <= frameIdx; i++ ) {
		int got = S_OpusChunkRead( sc, byteOfs, (byte *)&len16, 2 );
		if ( got < 2 ) {
			return -1;
		}
		if ( i == frameIdx ) {
			*frameLen = (int)len16;
			return byteOfs;
		}
		byteOfs += 2 + (int)len16;
	}

	return -1;
}


/*
 * Decode a single Opus frame into the static decode buffer.
 * Returns the number of decoded samples or 0 on error.
 */
static int S_OpusDecodeFrame( const sfx_t *sc, int frameIdx )
{
	int frameLen;
	int headerOfs;
	unsigned char encBuf[4000];
	int decoded;
	int got;

	if ( !s_opusDecoder ) {
		return 0;
	}

	// if switching to a different sound, reset decoder state
	if ( s_opusDecoderSfx != sc ) {
		opus_decoder_ctl( s_opusDecoder, OPUS_RESET_STATE );
		s_opusDecoderSfx = sc;
		s_opusDecoderFrameIdx = -1;
	}

	// already decoded this frame?
	if ( s_opusDecoderFrameIdx == frameIdx ) {
		return s_opusDecodeBufValid;
	}

	// non-sequential access -- reset predictor to avoid artefacts
	if ( frameIdx != s_opusDecoderFrameIdx + 1 ) {
		opus_decoder_ctl( s_opusDecoder, OPUS_RESET_STATE );
	}

	headerOfs = S_OpusFindFrame( sc, frameIdx, &frameLen );
	if ( headerOfs < 0 || frameLen <= 0 || frameLen > (int)sizeof( encBuf ) ) {
		s_opusDecodeBufValid = 0;
		return 0;
	}

	got = S_OpusChunkRead( sc, headerOfs + 2, encBuf, frameLen );
	if ( got < frameLen ) {
		s_opusDecodeBufValid = 0;
		return 0;
	}

	decoded = opus_decode( s_opusDecoder, encBuf, frameLen,
	                       s_opusDecodeBuf, OPUS_INMEM_FRAME_SAMPLES, 0 );
	if ( decoded < 0 ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: opus_decode error %d (frame %d)\n",
		             decoded, frameIdx );
		s_opusDecodeBufValid = 0;
		return 0;
	}

	s_opusDecoderFrameIdx = frameIdx;
	s_opusDecodeBufValid = decoded;
	return decoded;
}


/*
====================
S_OpusGetSamples

Decode Opus-compressed sound data and write mono 16-bit PCM samples
into `out`.  Starts at `sampleOffset` (in the decompressed stream)
and writes up to `count` samples.

Returns the number of samples actually written (may be less than `count`
if the sound ends or on decode error).

This is the primary interface used by S_PaintChannelFromOpus() in snd_mix.c.
====================
*/
int S_OpusGetSamples( const sfx_t *sc, int sampleOffset, short *out, int count )
{
	int written = 0;

	while ( written < count ) {
		int curSample = sampleOffset + written;
		int frameIdx  = curSample / OPUS_INMEM_FRAME_SAMPLES;
		int intraOfs  = curSample % OPUS_INMEM_FRAME_SAMPLES;
		int valid, avail, n;

		valid = S_OpusDecodeFrame( sc, frameIdx );
		if ( valid <= 0 ) {
			break;
		}

		avail = valid - intraOfs;
		if ( avail <= 0 ) {
			break;
		}

		n = count - written;
		if ( n > avail ) {
			n = avail;
		}

		Com_Memcpy( out + written, s_opusDecodeBuf + intraOfs,
		            n * sizeof( short ) );
		written += n;
	}

	return written;
}
