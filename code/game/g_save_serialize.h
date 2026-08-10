// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_serialize.h — savegame .svg serializer (Phase-4): the field-walker that
// orchestrates the landed primitives (Phase-1 descriptor tables, Phase-2 callback
// registry, Phase-3 atomic I/O) into a full save/load of the world.
//
// Sub-landing scope: the WRITE path is complete, and the READ path parses into a
// validated in-memory form. The load FIXUP — resolving the relocated indices back
// to live pointers, re-linking entities (trap_LinkEntity), re-acquiring the AI
// pools, re-parsing scripts — is PHASE-6. Phase-4 reads + validates + reports; it
// does NOT resume the live world.
//
// Two layers, split by dependency (the Phase-2/3 pattern):
//   - the WALKER (SG_WriteFields / SG_ReadFields + the byte stream + the index
//     relocation) is GAME-TYPE-FREE: it takes a void* struct base, a descriptor
//     table, and the three relocation bases as explicit parameters. It lives in
//     g_save_serialize.c and the round-trip unit test drives it with synthetic
//     structs — no gentity_t / traps needed.
//   - the WORLD glue (SG_WriteWorld / SG_ReadWorld — the entity/client/level walks
//     over the real g_entities/level, + the AI-pool hooks + the cheat-gated
//     command) uses the real game globals and lives in g_save_world.c (game only).
//
// Phase-4 is byte-identical for NORMAL gameplay: the serializer is only reachable
// via a dev/cheat-gated savegame/loadgame command. No cheat -> never called.

#ifndef G_SAVE_SERIALIZE_H
#define G_SAVE_SERIALIZE_H

#include <stddef.h>
#include <stdint.h>

#include "g_save.h"   // saveField_t / saveRange_t / sgFieldType_t

// ── the byte stream (append-on-write, consume-on-read) ───────────────────────
// A cursor over a caller-provided buffer. Write grows `pos` up to `cap` (an
// overflow sets `overflow` and stops); read advances `pos` up to `len` (an
// underflow sets `overflow`). One type serves both directions.
typedef struct {
	uint8_t *buf;       // the backing buffer
	size_t   cap;       // capacity (write bound)
	size_t   len;       // valid bytes (read bound; == pos after a write)
	size_t   pos;       // cursor
	int      overflow;  // set once on any over/underflow — the whole op is then void
} sgStream_t;

void SG_StreamInitWrite( sgStream_t *s, void *buf, size_t cap );
void SG_StreamInitRead( sgStream_t *s, const void *buf, size_t len );

// Low-level stream ops the world glue uses for its structural framing (section
// tags, per-entity index markers) around the field-walker calls. Over/underflow
// sets s->overflow; a read past end zero-fills. Symmetric write/read pairs.
void    SG_StreamWriteRaw( sgStream_t *s, const void *src, size_t n );
void    SG_StreamReadRaw( sgStream_t *s, void *dst, size_t n );
void    SG_StreamWriteI32( sgStream_t *s, int32_t v );
int32_t SG_StreamReadI32( sgStream_t *s );

// ── the relocation bases (passed explicitly so the walker is game-type-free) ─
// The three arrays a pointer field is index-relocated against. The walker never
// names the game globals; SG_WriteWorld/SG_ReadWorld pass the real ones, the unit
// test passes synthetic ones.
typedef struct {
	const void *entityBase;   // g_entities
	size_t      entityStride;  // sizeof(gentity_t)
	int         entityCount;   // MAX_GENTITIES (bound for read validation)
	const void *clientBase;   // level.clients
	size_t      clientStride;  // sizeof(gclient_t)
	int         clientCount;   // level.maxclients
	const void *itemBase;     // bg_itemlist
	size_t      itemStride;    // sizeof(gitem_t)
	int         itemCount;     // bg_numItems
} sgRelocBases_t;

// ── the field-walker ─────────────────────────────────────────────────────────
// Walk a { 0, SG_NONE, 0 }-terminated descriptor over the struct at `base`,
// streaming each field per its type. Pointer fields (SG_ENTITY/CLIENT/ITEM) are
// index-relocated against `bases`; SG_FUNCTION writes the registered callback
// NAME; SG_STRING writes length-prefixed content; scalars/RAW copy verbatim.
// Returns 1 on success, 0 on a stream over/underflow or a bad index/name on read.
//
// The Phase-4 READ produces the struct in its SERIALIZED form: pointer fields are
// left as their relocated index encoded in the pointer-sized slot (Phase-6
// resolves them to live &base[index]); SG_STRING/SG_FUNCTION content is copied
// into a caller-side interning step by Phase-6. Phase-4 validates indices are in
// range and names resolve, but does not patch the live pointers.
int SG_WriteFields( const saveField_t *fields, const void *base,
	const sgRelocBases_t *bases, sgStream_t *s );

// SG_ReadFields — the Phase-A load. Reads each field into `base`:
//   - scalars / VECTOR / RAW: verbatim into the field.
//   - SG_FUNCTION: the registered name is resolved to a live pointer immediately
//     (SG_NameToFunction is game-type-free), stored in the field. An unknown name
//     voids the op (a save referencing a callback this build lacks).
//   - SG_STRING: the parsed content is re-interned via `intern` (the game passes
//     G_NewString) and the resulting char* stored in the field. If `intern` is
//     NULL the string is parsed + validated but discarded (validate-only mode —
//     what the Phase-4/5 read-to-buffer callers use).
//   - SG_ENTITY/CLIENT/ITEM: the array INDEX is stored in the pointer-sized slot
//     (validated in range). The live-pointer patch is the Phase-B pass
//     (SG_ResolveIndices), run AFTER every slot is populated so forward-references
//     resolve correctly.
// `intern` may be NULL (validate-only). Returns 1 on success, 0 on over/underflow
// or a bad index / unknown callback name.
int SG_ReadFields( const saveField_t *fields, void *base,
	const sgRelocBases_t *bases, sgStream_t *s,
	char *( *intern )( const char *content ) );

// SG_ResolveIndices — the Phase-B relocation pass. AFTER every entity/client is
// loaded into its fixed-base slot, walk the pointer fields (SG_ENTITY/CLIENT/ITEM)
// and patch the stored index to the live pointer &base[index] (SG_IDX_NULL ->
// NULL). Because the bases are fixed and all slots are populated, every reference
// resolves correctly regardless of load order. Non-pointer fields are skipped.
void SG_ResolveIndices( const saveField_t *fields, void *base,
	const sgRelocBases_t *bases );

// ── the range-walker (for the saveRange_t whitelists) ────────────────────────
// The .psw persistent subset (gclientPersFields) is a { byte-offset, length }
// whitelist, not a typed field list — its members (ps / sess / pers) are whole
// PODs copied verbatim, no per-field typing. SG_WriteRanges streams each range's
// bytes from `base`; SG_ReadRanges reads them back. Terminated by a { 0, 0 } row.
// (ps carries internal index-reloc fields — groundEntityNum / clientNum — that a
// Phase-6 fixup pass resolves, exactly as the .svg ps RAW blob does; the range
// copy is verbatim here.) Returns 1 on success, 0 on stream over/underflow.
int SG_WriteRanges( const saveRange_t *ranges, const void *base, sgStream_t *s );
int SG_ReadRanges( const saveRange_t *ranges, void *base, sgStream_t *s );

// ── index relocation (exposed for the round-trip symmetry test) ──────────────
// Encode a live pointer as an array index (-1 for NULL), and decode an index back
// to a validated index (Phase-6 turns it into &base[index]). Symmetric by
// construction — the test asserts encode/decode round-trips for every base.
int32_t SG_PtrToIndex( const void *ptr, const void *base, size_t stride, int count );
// Returns 1 if `index` is a valid (-1 or 0..count-1) slot for this base, else 0.
int     SG_IndexInRange( int32_t index, int count );

#endif // G_SAVE_SERIALIZE_H
