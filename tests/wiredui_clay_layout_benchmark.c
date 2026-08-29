// SPDX-License-Identifier: GPL-3.0-or-later

// TASK-142 M-07: deterministic native 200-item Clay layout budget fixture.
// This is deliberately a host test: it measures the exact vendored layout
// engine used by WiredUI without GPU, content-pack or window noise.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

#define CLAY_IMPLEMENTATION
#include "clay.h"

#define FIXTURE_ITEMS 200
#define WARMUP_FRAMES 100
#define SAMPLE_FRAMES 1000
#define P99_BUDGET_US 1000.0

static int clay_errors;

static void on_clay_error( Clay_ErrorData error )
{
    (void) error;
    clay_errors++;
}

static uint64_t now_ns( void )
{
#if defined(_WIN32)
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter( &counter );
    QueryPerformanceFrequency( &frequency );
    return (uint64_t)( ( counter.QuadPart * 1000000000ULL ) / frequency.QuadPart );
#elif defined(__APPLE__)
    static mach_timebase_info_data_t timebase;
    uint64_t ticks = mach_absolute_time();
    if ( timebase.denom == 0 ) mach_timebase_info( &timebase );
    return ticks * timebase.numer / timebase.denom;
#else
    struct timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static int compare_u64( const void *lhs, const void *rhs )
{
    uint64_t a = *(const uint64_t *)lhs;
    uint64_t b = *(const uint64_t *)rhs;
    return ( a > b ) - ( a < b );
}

static void emit_fixture( void )
{
    int i;
    Clay_BeginLayout();
    CLAY({
        .id = CLAY_ID( "wiredui_200_item_fixture" ),
        .layout = {
            .sizing = { CLAY_SIZING_FIXED( 1280 ), CLAY_SIZING_FIXED( 720 ) },
            .padding = CLAY_PADDING_ALL( 8 ),
            .childGap = 2,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        }
    }) {
        for ( i = 0; i < FIXTURE_ITEMS; i++ ) {
            CLAY({
                .id = CLAY_IDI( "fixture_row", i ),
                .layout = {
                    .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_FIXED( 2 ) }
                },
                .backgroundColor = { 30, 24, 18, 128 }
            }) {}
        }
    }
    (void) Clay_EndLayout();
}

int main( void )
{
    Clay_Arena arena;
    Clay_Dimensions dims = { 1280.0f, 720.0f };
    Clay_ErrorHandler errors = { on_clay_error, NULL };
    uint32_t memory_size = Clay_MinMemorySize();
    void *memory = malloc( memory_size );
    uint64_t samples[SAMPLE_FRAMES];
    int i;
    size_t p99_index;
    double p50_us, p99_us, max_us;

    if ( !memory ) {
        fprintf( stderr, "FAIL: cannot allocate %u-byte Clay arena\n", memory_size );
        return 1;
    }
    arena = Clay_CreateArenaWithCapacityAndMemory( memory_size, memory );
    if ( !Clay_Initialize( arena, dims, errors ) ) {
        fprintf( stderr, "FAIL: Clay initialization failed\n" );
        free( memory );
        return 1;
    }

    for ( i = 0; i < WARMUP_FRAMES; i++ ) emit_fixture();
    for ( i = 0; i < SAMPLE_FRAMES; i++ ) {
        uint64_t begin = now_ns();
        emit_fixture();
        samples[i] = now_ns() - begin;
    }
    qsort( samples, SAMPLE_FRAMES, sizeof( samples[0] ), compare_u64 );
    p99_index = ( SAMPLE_FRAMES * 99u ) / 100u;
    if ( p99_index >= SAMPLE_FRAMES ) p99_index = SAMPLE_FRAMES - 1;
    p50_us = (double)samples[SAMPLE_FRAMES / 2] / 1000.0;
    p99_us = (double)samples[p99_index] / 1000.0;
    max_us = (double)samples[SAMPLE_FRAMES - 1] / 1000.0;

    free( memory );
    if ( clay_errors ) {
        fprintf( stderr, "FAIL: Clay reported %d error(s)\n", clay_errors );
        return 1;
    }
    if ( p99_us > P99_BUDGET_US ) {
        fprintf( stderr,
            "FAIL: 200-item Clay layout p99 %.2f us exceeds %.0f us budget "
            "(p50 %.2f us, max %.2f us)\n",
            p99_us, P99_BUDGET_US, p50_us, max_us );
        return 1;
    }
    printf( "PASS: 200-item native Clay layout p50 %.2f us, p99 %.2f us, "
            "max %.2f us (budget %.0f us)\n",
            p50_us, p99_us, max_us, P99_BUDGET_US );
    return 0;
}
