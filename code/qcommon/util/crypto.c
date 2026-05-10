#include "q_shared.h"
#include "crypto.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_system, "system" );

/* Declared in qcommon.h; forward-declared here to keep util/ free of qcommon.h. */
qboolean Sys_RandomBytes( byte *string, int len );

#define SHA256_BLOCK_SIZE 64

typedef struct {
	byte data[ SHA256_BLOCK_SIZE ];
	uint32_t state[8];
	uint64_t bitlen;
	unsigned int datalen;
} sha256_ctx_t;

static const uint32_t k256[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTRIGHT(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT((x),2) ^ ROTRIGHT((x),13) ^ ROTRIGHT((x),22))
#define EP1(x) (ROTRIGHT((x),6) ^ ROTRIGHT((x),11) ^ ROTRIGHT((x),25))
#define SIG0(x) (ROTRIGHT((x),7) ^ ROTRIGHT((x),18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT((x),17) ^ ROTRIGHT((x),19) ^ ((x) >> 10))

static void SHA256_Transform( sha256_ctx_t *ctx, const byte data[ SHA256_BLOCK_SIZE ] ) {
	uint32_t m[64];
	uint32_t a, b, c, d, e, f, g, h;
	uint32_t t1, t2;
	for ( unsigned int i = 0; i < 16; i++ ) {
		m[i] =
			( (uint32_t)data[i * 4 + 0] << 24 ) |
			( (uint32_t)data[i * 4 + 1] << 16 ) |
			( (uint32_t)data[i * 4 + 2] << 8 ) |
			( (uint32_t)data[i * 4 + 3] );
	}

	for ( unsigned int i = 16; i < 64; i++ ) {
		m[i] = SIG1( m[i - 2] ) + m[i - 7] + SIG0( m[i - 15] ) + m[i - 16];
	}

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];
	f = ctx->state[5];
	g = ctx->state[6];
	h = ctx->state[7];

	for ( unsigned int i = 0; i < 64; i++ ) {
		t1 = h + EP1( e ) + CH( e, f, g ) + k256[i] + m[i];
		t2 = EP0( a ) + MAJ( a, b, c );
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
	ctx->state[5] += f;
	ctx->state[6] += g;
	ctx->state[7] += h;
}

static void SHA256_Init( sha256_ctx_t *ctx ) {
	ctx->datalen = 0;
	ctx->bitlen = 0;
	ctx->state[0] = 0x6a09e667;
	ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372;
	ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f;
	ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab;
	ctx->state[7] = 0x5be0cd19;
}

static void SHA256_Update( sha256_ctx_t *ctx, const byte *data, unsigned int len ) {
	for ( unsigned int i = 0; i < len; i++ ) {
		ctx->data[ ctx->datalen++ ] = data[i];
		if ( ctx->datalen == SHA256_BLOCK_SIZE ) {
			SHA256_Transform( ctx, ctx->data );
			ctx->bitlen += (uint64_t)SHA256_BLOCK_SIZE * 8;
			ctx->datalen = 0;
		}
	}
}

static void SHA256_Final( sha256_ctx_t *ctx, byte out[ COM_SHA256_DIGEST_LEN ] ) {
	unsigned int i = ctx->datalen;

	if ( ctx->datalen < 56 ) {
		ctx->data[i++] = 0x80;
		while ( i < 56 ) {
			ctx->data[i++] = 0;
		}
	} else {
		ctx->data[i++] = 0x80;
		while ( i < SHA256_BLOCK_SIZE ) {
			ctx->data[i++] = 0;
		}
		SHA256_Transform( ctx, ctx->data );
		memset( ctx->data, 0, 56 );
	}

	uint64_t bitlen = ctx->bitlen + (uint64_t)ctx->datalen * 8;
	ctx->data[63] = (byte)( bitlen );
	ctx->data[62] = (byte)( bitlen >> 8 );
	ctx->data[61] = (byte)( bitlen >> 16 );
	ctx->data[60] = (byte)( bitlen >> 24 );
	ctx->data[59] = (byte)( bitlen >> 32 );
	ctx->data[58] = (byte)( bitlen >> 40 );
	ctx->data[57] = (byte)( bitlen >> 48 );
	ctx->data[56] = (byte)( bitlen >> 56 );
	SHA256_Transform( ctx, ctx->data );

	for ( i = 0; i < 4; i++ ) {
		out[i + 0]  = (byte)( ( ctx->state[0] >> ( 24 - i * 8 ) ) & 0xff );
		out[i + 4]  = (byte)( ( ctx->state[1] >> ( 24 - i * 8 ) ) & 0xff );
		out[i + 8]  = (byte)( ( ctx->state[2] >> ( 24 - i * 8 ) ) & 0xff );
		out[i + 12] = (byte)( ( ctx->state[3] >> ( 24 - i * 8 ) ) & 0xff );
		out[i + 16] = (byte)( ( ctx->state[4] >> ( 24 - i * 8 ) ) & 0xff );
		out[i + 20] = (byte)( ( ctx->state[5] >> ( 24 - i * 8 ) ) & 0xff );
		out[i + 24] = (byte)( ( ctx->state[6] >> ( 24 - i * 8 ) ) & 0xff );
		out[i + 28] = (byte)( ( ctx->state[7] >> ( 24 - i * 8 ) ) & 0xff );
	}
}

void Com_SHA256( const byte *data, unsigned int len, byte out[ COM_SHA256_DIGEST_LEN ] ) {
	sha256_ctx_t ctx;

	SHA256_Init( &ctx );
	if ( data != NULL && len > 0 ) {
		SHA256_Update( &ctx, data, len );
	}
	SHA256_Final( &ctx, out );
}

void Com_HMAC_SHA256( const byte *key, unsigned int keyLen, const byte *data, unsigned int dataLen, byte out[ COM_SHA256_DIGEST_LEN ] ) {
	const byte *workKey = key;
	unsigned int workKeyLen = keyLen;

	if ( workKey == NULL ) {
		workKey = (const byte *)"";
		workKeyLen = 0;
	}

	if ( workKeyLen > SHA256_BLOCK_SIZE ) {
		byte keyHash[ COM_SHA256_DIGEST_LEN ];
		Com_SHA256( workKey, workKeyLen, keyHash );
		workKey = keyHash;
		workKeyLen = COM_SHA256_DIGEST_LEN;
	}

	byte kIpad[ SHA256_BLOCK_SIZE ];
	byte kOpad[ SHA256_BLOCK_SIZE ];
	memset( kIpad, 0x36, sizeof( kIpad ) );
	memset( kOpad, 0x5c, sizeof( kOpad ) );

	for ( unsigned int i = 0; i < workKeyLen; i++ ) {
		kIpad[i] ^= workKey[i];
		kOpad[i] ^= workKey[i];
	}

	sha256_ctx_t ctx;
	SHA256_Init( &ctx );
	SHA256_Update( &ctx, kIpad, sizeof( kIpad ) );
	if ( data != NULL && dataLen > 0 ) {
		SHA256_Update( &ctx, data, dataLen );
	}
	byte innerHash[ COM_SHA256_DIGEST_LEN ];
	SHA256_Final( &ctx, innerHash );

	SHA256_Init( &ctx );
	SHA256_Update( &ctx, kOpad, sizeof( kOpad ) );
	SHA256_Update( &ctx, innerHash, sizeof( innerHash ) );
	SHA256_Final( &ctx, out );
}

void Com_HMAC_SHA256_Hex( const char *key, const char *data, char outHex[ COM_SHA256_HEX_LEN + 1 ] ) {
	const byte *k = (const byte *)( key ? key : "" );
	const byte *d = (const byte *)( data ? data : "" );
	unsigned int keyLen = key ? (unsigned int)strlen( key ) : 0;
	unsigned int dataLen = data ? (unsigned int)strlen( data ) : 0;
	byte digest[ COM_SHA256_DIGEST_LEN ];

	Com_HMAC_SHA256( k, keyLen, d, dataLen, digest );

	for ( unsigned int i = 0; i < COM_SHA256_DIGEST_LEN; i++ ) {
		Com_sprintf( outHex + i * 2, 3, "%02x", digest[i] );
	}
	outHex[ COM_SHA256_HEX_LEN ] = '\0';
}

void Com_RandomHexString( char *out, int hexLen ) {
	static const char hex[] = "0123456789abcdef";
	byte rnd[128];

	if ( out == NULL || hexLen <= 0 ) {
		return;
	}

	int bytesNeeded = ( hexLen + 1 ) / 2;
	if ( bytesNeeded > (int)sizeof( rnd ) ) {
		bytesNeeded = (int)sizeof( rnd );
	}

	Com_RandomBytes( rnd, bytesNeeded );

	for ( int i = 0; i < hexLen; i++ ) {
		byte b = rnd[ i / 2 ];
		if ( ( i & 1 ) == 0 ) {
			out[i] = hex[ ( b >> 4 ) & 0x0f ];
		} else {
			out[i] = hex[ b & 0x0f ];
		}
	}

	out[hexLen] = '\0';
}


void Com_RandomBytes( byte *string, int len )
{
	if ( Sys_RandomBytes( string, len ) )
		return;

	Com_Log( SEV_INFO, LOG_CH(ch_system), S_COLOR_YELLOW "Com_RandomBytes: using weak randomization\n" );
	// NOLINTNEXTLINE(bugprone-random-generator-seed) — explicit fallback path; non-crypto warning string above flags the weakness
	srand( time( NULL ) );
	for( int i = 0; i < len; i++ )
		string[i] = (unsigned char)( rand() % 256 );
}
