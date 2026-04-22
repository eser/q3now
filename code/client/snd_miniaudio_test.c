/*
===========================================================================
snd_miniaudio_test.c -- standalone unit tests for the lock-free audio
ring buffer used by snd_miniaudio.c

Build:
  gcc -O2 -Wall -o snd_miniaudio_test \
      code/client/snd_miniaudio_test.c -lm -lpthread

Run:
  ./snd_miniaudio_test

Returns 0 on success, non-zero on any test failure.

These tests cover the ring buffer ALGORITHM in isolation. The production
implementation in snd_miniaudio.c uses miniaudio's ma_atomic_uint32_get /
ma_atomic_uint32_set primitives; this test substitutes C11 _Atomic to
exercise the same single-producer / single-consumer dance without pulling
the engine in. Production correctness depends on this algorithm plus the
audio thread's actual discipline (no mutexes in the callback), which is
verified separately via static review and manual gameplay testing.
===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

/* Mirrors snd_miniaudio.c:78 */
#define S_LEVELS_RING_SIZE 128

/* Mirrors snd_miniaudio.c:79-80 */
static _Atomic uint32_t test_levelsRingPos;
static float            test_levelsRing[S_LEVELS_RING_SIZE];

/*
 * Mirrors the audio-callback writer block in snd_miniaudio.c:251-253:
 *   ringPos = ma_atomic_uint32_get( &s_levelsRingPos );
 *   s_levelsRing[ ringPos % S_LEVELS_RING_SIZE ] = rms;
 *   ma_atomic_uint32_set( &s_levelsRingPos, ringPos + 1 );
 */
static void rb_write_level( float rms )
{
	uint32_t pos = atomic_load( &test_levelsRingPos );
	test_levelsRing[ pos % S_LEVELS_RING_SIZE ] = rms;
	atomic_store( &test_levelsRingPos, pos + 1 );
}

/*
 * Mirrors S_GetRecentLevels in snd_miniaudio.c:308-331.
 */
static int rb_read_recent( float *outLevels, int outCount )
{
	if ( outLevels == NULL || outCount <= 0 )
		return 0;
	if ( outCount > S_LEVELS_RING_SIZE )
		outCount = S_LEVELS_RING_SIZE;

	uint32_t pos = atomic_load( &test_levelsRingPos );

	for ( int i = 0; i < outCount; i++ )
	{
		uint32_t idx = ( pos - 1 - (uint32_t)i ) % S_LEVELS_RING_SIZE;
		outLevels[ outCount - 1 - i ] = test_levelsRing[ idx ];
	}

	return outCount;
}

static void rb_reset( void )
{
	atomic_store( &test_levelsRingPos, 0 );
	memset( test_levelsRing, 0, sizeof( test_levelsRing ) );
}

/* ---- test framework -------------------------------------------------- */

static int g_passed = 0;
static int g_failed = 0;

#define TEST_BEGIN(name) do { printf( "  %-44s ", name ); fflush( stdout ); } while ( 0 )
#define TEST_PASS()      do { printf( "PASS\n" ); g_passed++; } while ( 0 )
#define TEST_FAIL(msg)   do { printf( "FAIL - %s\n", msg ); g_failed++; } while ( 0 )
#define TEST_ASSERT(cond, msg) do { if ( !(cond) ) { TEST_FAIL( msg ); return; } } while ( 0 )
#define TEST_END()       TEST_PASS()

/* ---- individual tests ------------------------------------------------ */

static void test_initial_state( void )
{
	float buf[8];

	TEST_BEGIN( "initial state reads zeros" );
	rb_reset();
	int n = rb_read_recent( buf, 8 );
	TEST_ASSERT( n == 8, "initial read should return outCount" );
	for ( int i = 0; i < 8; i++ )
	{
		TEST_ASSERT( buf[i] == 0.0f, "initial buffer should be all zeros" );
	}
	TEST_END();
}

static void test_basic_write_read( void )
{
	float buf[3];

	TEST_BEGIN( "basic write then read" );
	rb_reset();
	rb_write_level( 0.5f );
	rb_write_level( 0.7f );
	rb_write_level( 0.3f );
	int n = rb_read_recent( buf, 3 );
	TEST_ASSERT( n == 3, "read should return 3" );
	/* Reader stores oldest first, newest last (left-to-right draw order). */
	TEST_ASSERT( buf[0] == 0.5f, "oldest should be first" );
	TEST_ASSERT( buf[1] == 0.7f, "middle" );
	TEST_ASSERT( buf[2] == 0.3f, "newest should be last" );
	TEST_END();
}

static void test_underrun( void )
{
	float buf[16];

	TEST_BEGIN( "underrun: read with fewer writes than asked" );
	rb_reset();
	rb_write_level( 0.9f );
	rb_write_level( 0.8f );
	int n = rb_read_recent( buf, 16 );
	TEST_ASSERT( n == 16, "read should still return full count" );
	/* Newest two are at the tail; the rest are stale zeros from the
	 * untouched ring slots. The algorithm intentionally returns the
	 * ring's natural state rather than tracking valid-fill — that is
	 * exactly what the production code in S_GetRecentLevels does. */
	TEST_ASSERT( buf[15] == 0.8f, "newest sample at tail" );
	TEST_ASSERT( buf[14] == 0.9f, "second-newest at tail-1" );
	for ( int i = 0; i < 14; i++ )
	{
		TEST_ASSERT( buf[i] == 0.0f, "uninitialised slots stay zero" );
	}
	TEST_END();
}

static void test_wraparound( void )
{
	float buf[10];

	TEST_BEGIN( "wraparound past buffer end" );
	rb_reset();
	for ( int i = 0; i < S_LEVELS_RING_SIZE + 5; i++ )
	{
		rb_write_level( (float)i );
	}
	int n = rb_read_recent( buf, 10 );
	TEST_ASSERT( n == 10, "read should return 10" );
	for ( int i = 0; i < 10; i++ )
	{
		float expected = (float)( S_LEVELS_RING_SIZE + 5 - 10 + i );
		if ( buf[i] != expected )
		{
			char msg[96];
			snprintf( msg, sizeof( msg ),
			          "wraparound mismatch at i=%d: got %f, expected %f",
			          i, buf[i], expected );
			TEST_FAIL( msg );
			return;
		}
	}
	TEST_END();
}

static void test_overrun( void )
{
	float buf[S_LEVELS_RING_SIZE];

	TEST_BEGIN( "overrun overwrites oldest data" );
	rb_reset();
	for ( int i = 0; i < 3 * S_LEVELS_RING_SIZE; i++ )
	{
		rb_write_level( (float)i );
	}
	int n = rb_read_recent( buf, S_LEVELS_RING_SIZE );
	TEST_ASSERT( n == S_LEVELS_RING_SIZE, "read should return RING_SIZE" );
	for ( int i = 0; i < S_LEVELS_RING_SIZE; i++ )
	{
		float expected = (float)( 3 * S_LEVELS_RING_SIZE - S_LEVELS_RING_SIZE + i );
		if ( buf[i] != expected )
		{
			char msg[96];
			snprintf( msg, sizeof( msg ),
			          "overrun mismatch at i=%d: got %f, expected %f",
			          i, buf[i], expected );
			TEST_FAIL( msg );
			return;
		}
	}
	TEST_END();
}

static void test_read_clamped( void )
{
	float buf[S_LEVELS_RING_SIZE * 2];

	TEST_BEGIN( "read clamped to RING_SIZE" );
	rb_reset();
	for ( int i = 0; i < S_LEVELS_RING_SIZE; i++ )
		rb_write_level( (float)i );
	int n = rb_read_recent( buf, S_LEVELS_RING_SIZE * 2 );
	TEST_ASSERT( n == S_LEVELS_RING_SIZE, "should clamp to RING_SIZE" );
	TEST_END();
}

static void test_zero_count( void )
{
	float dummy = 0.0f;

	TEST_BEGIN( "read with outCount=0" );
	int n = rb_read_recent( NULL, 0 );
	TEST_ASSERT( n == 0, "zero count should return 0" );
	n = rb_read_recent( &dummy, 0 );
	TEST_ASSERT( n == 0, "zero count with valid ptr should return 0" );
	TEST_END();
}

static void test_null_ptr( void )
{
	TEST_BEGIN( "read with null ptr" );
	int n = rb_read_recent( NULL, 5 );
	TEST_ASSERT( n == 0, "null ptr should return 0" );
	TEST_END();
}

static void test_negative_count( void )
{
	float buf[4];

	TEST_BEGIN( "read with negative outCount" );
	int n = rb_read_recent( buf, -3 );
	TEST_ASSERT( n == 0, "negative count should return 0" );
	TEST_END();
}

/*
 * Concurrency: spawn a writer thread that hammers the ring at top
 * speed while the main thread reads. The single-producer / single-
 * consumer algorithm allows visual jitter at the race boundary but
 * MUST NOT crash, MUST NOT read out of bounds, and MUST always
 * return the requested element count.
 */
typedef struct {
	int          iterations;
	volatile int stop;
} writer_arg_t;

static void *writer_thread( void *arg )
{
	writer_arg_t *a = (writer_arg_t *)arg;
	for ( int i = 0; i < a->iterations && !a->stop; i++ )
	{
		rb_write_level( (float)i );
	}
	return NULL;
}

static void test_concurrent( void )
{
	writer_arg_t arg;
	pthread_t    thr;
	float        buf[16];

	TEST_BEGIN( "concurrent reader/writer no crash" );
	rb_reset();
	arg.iterations = 200000;
	arg.stop       = 0;
	if ( pthread_create( &thr, NULL, writer_thread, &arg ) != 0 )
	{
		TEST_FAIL( "pthread_create failed" );
		return;
	}
	for ( int i = 0; i < 100000; i++ )
	{
		int n = rb_read_recent( buf, 16 );
		if ( n != 16 )
		{
			arg.stop = 1;
			pthread_join( thr, NULL );
			TEST_FAIL( "read returned unexpected count during concurrent run" );
			return;
		}
	}
	pthread_join( thr, NULL );
	TEST_END();
}

/*
 * Mock ma_device: build a 1 kHz sine wave at 44100 Hz interleaved
 * stereo, run the same RMS computation as the audio callback in
 * snd_miniaudio.c:235-254, push the result into the ring, then read
 * it back. Verify the value matches the closed-form expectation
 * (RMS of A*sin = A/sqrt(2)) within tolerance.
 */
static void test_rms_known_input( void )
{
	const int   sampleRate = 44100;
	const int   frameCount = 256;
	const float freq       = 1000.0f;
	const float amp        = 0.25f * 32767.0f;
	const int   channels   = 2;
	int16_t     pcm[256 * 2];
	double      sumSquares = 0.0;
	float       readBack[1];

	TEST_BEGIN( "RMS of 1 kHz sine matches A/sqrt(2)" );
	rb_reset();

	for ( int i = 0; i < frameCount; i++ )
	{
		float    t      = (float)i;
		int16_t  sample = (int16_t)( amp * sinf( 2.0f * 3.14159265f * freq * t / (float)sampleRate ) );
		pcm[i * 2 + 0]  = sample;
		pcm[i * 2 + 1]  = sample;
	}

	int totalSamples = frameCount * channels;
	for ( int i = 0; i < totalSamples; i++ )
	{
		float s = (float)pcm[i] * ( 1.0f / 32768.0f );
		sumSquares += (double)( s * s );
	}
	float rms = (float)sqrt( sumSquares / (double)totalSamples );

	rb_write_level( rms );

	float expected  = 0.25f / 1.41421356f;
	float tolerance = 0.01f;
	if ( rms < expected - tolerance || rms > expected + tolerance )
	{
		char msg[96];
		snprintf( msg, sizeof( msg ),
		          "RMS %f outside expected %f +/- %f",
		          rms, expected, tolerance );
		TEST_FAIL( msg );
		return;
	}

	int n = rb_read_recent( readBack, 1 );
	if ( n != 1 || readBack[0] != rms )
	{
		TEST_FAIL( "ring did not echo the RMS we just wrote" );
		return;
	}

	TEST_END();
}

/* ---- main ------------------------------------------------------------ */

int main( int argc, char *argv[] )
{
	(void)argc;
	(void)argv;

	printf( "snd_miniaudio_test - lock-free ring buffer unit tests\n" );
	printf( "=======================================================\n" );

	test_initial_state();
	test_basic_write_read();
	test_underrun();
	test_wraparound();
	test_overrun();
	test_read_clamped();
	test_zero_count();
	test_null_ptr();
	test_negative_count();
	test_concurrent();
	test_rms_known_input();

	printf( "\n" );
	printf( "Results: %d passed, %d failed\n", g_passed, g_failed );
	return g_failed > 0 ? 1 : 0;
}
