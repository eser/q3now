// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
log_buffer.h — Per-thread lock-free log ring buffer

Captures every Com_Log call regardless of sink severity filtering, so
dumpLogBuffer can reconstruct recent history at any time without requiring
log_severity to have been low.

Architecture:
  - One log_buffer_t per thread, allocated lazily on first LogBuffer_Append.
  - All thread rings are registered in a global registry (mutex-protected).
  - LogBuffer_Dump snapshots all rings, merges by ts_mono, writes JSONL.
  - FNV-1a hash of body is stored; consecutive identical messages are deduped
    (slot is written once, run-length counted in the slot).
===========================================================================
*/
#pragma once

// Caller must include q_shared.h and log.h before this header.

// -------------------------------------------------------------------------
// Constants
// -------------------------------------------------------------------------

#define LOG_BUFFER_SLOT_MSG_SIZE        1024
#define LOG_BUFFER_REGISTRY_CAPACITY    64

// -------------------------------------------------------------------------
// Slot
// -------------------------------------------------------------------------

typedef struct {
    int64_t         ts_mono;                        // Sys_NanoTime() at emit time (sort key only)
    char            ts_wall[40];                    // ISO 8601 wall-clock of first occurrence
    char            ts_wall_last[40];               // ISO 8601 wall-clock of last occurrence
    log_severity_t  severity;
    int             channel;        // index into log_channels[] (Phase 2+)
    char            body[LOG_BUFFER_SLOT_MSG_SIZE]; // truncated to slot size
    uint32_t        body_len;                       // bytes in body (<=MSG_SIZE)
    qboolean        truncated;                      // true if body was cut (slot or Com_Logv)
    uint32_t        truncated_bytes;                // bytes dropped when truncated
    uint32_t        fnv_hash;                       // FNV-1a of original body
    uint32_t        times;                          // consecutive-identical count (dedup run)
} log_buffer_slot_t;

// -------------------------------------------------------------------------
// Per-thread ring
// -------------------------------------------------------------------------

typedef struct {
    log_buffer_slot_t  *slots;
    uint32_t            capacity;
    uint64_t            write_pos;  // next slot to write (wraps via % capacity)
    uint32_t            last_hash;  // FNV-1a of last written body (dedup)
    sys_mutex_t         mutex;      // protects write_pos + slots during dump
} log_buffer_t;

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

// LogBuffer_Init: register log_buffer_capacity cvar, register dumpLogBuffer
// command. Call after Log_InitChannels() in Com_Init.
void LogBuffer_Init( void );

// LogBuffer_Shutdown: free all thread rings and destroy the registry.
// Call before WiredCore_Shutdown() in Com_Shutdown.
void LogBuffer_Shutdown( void );

// LogBuffer_Append: record a log record into the calling thread's ring.
// Called from Com_Logv BEFORE the severity gate so every message is captured.
// No-op during the pre-Init window (before LogBuffer_Init returns).
void LogBuffer_Append( const log_record_t *rec );

// LogBuffer_DumpCmd_f: console command handler for "dumpLogBuffer [filename]".
void LogBuffer_DumpCmd_f( void );
