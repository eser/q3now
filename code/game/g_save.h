// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save.h — savegame reflection engine: field-descriptor infrastructure.
//
// Phase-1 (this header + g_save.c) is DESCRIPTOR DATA ONLY: the field-type enum,
// the per-struct offset macros, the saveField_t descriptor, and the descriptor
// tables for gentity_t / gclient_t / level_locals_t. There is NO write/read/
// serialize path and NO name<->pointer registry yet — those are Phase-3/4 and
// Phase-2. Nothing calls this code, so the game is byte-identical.
//
// Design (from the savegame grounding, RealRTCW g_save.c the reference):
//   - Reflection architecture: a descriptor table per savable struct, each row a
//     {byte-offset, field-type}. The serializer (a later phase) walks the table.
//   - Function pointers are DROPPED in favour of a symbolic name (the scoped
//     ~116-entry registry, Phase-2) — WASM has no stable cross-build func-ptr
//     table. So SG_FUNCTION means "serialize the callback's registered NAME",
//     not an address or a funcList[] index (RealRTCW's dropped extractfuncs path).
//   - Entity/client/item pointers are index-relocated: written as an array index
//     (ptr - base), -1 = NULL, re-resolved to &base[index] on load.
//
// The field-type enum is deliberately named SG_* (not the spawn parser's F_*,
// g_spawn.c fieldtype_t) to avoid any collision — they are unrelated dialects.

#ifndef G_SAVE_H
#define G_SAVE_H

// Offset macros — the classic null-pointer-offset trick, one per savable struct.
// FOFS (gentity_t) already exists in g_local.h; add the client + level siblings.
//   FOFS(x)  -> byte offset of x within gentity_t      (g_local.h)
//   CFOFS(x) -> byte offset of x within gclient_t
//   SLOFS(x) -> byte offset of x within level_locals_t
#define CFOFS(x) ( (size_t)&( ( (gclient_t *)0 )->x ) )
#define SLOFS(x) ( (size_t)&( ( (level_locals_t *)0 )->x ) )

// Field types for a descriptor row. Scalars/vectors/arrays are self-describing so
// the Phase-4 serializer can size each row; the three pointer-bearing types
// (ENTITY/CLIENT/ITEM) are index-relocated; FUNCTION is name-relocated via the
// Phase-2 registry; STRING is an interned char* whose CONTENT is saved.
typedef enum {
	SG_NONE = 0,
	SG_INT,        // int
	SG_FLOAT,      // float
	SG_QBOOLEAN,   // qboolean (int-width)
	SG_VECTOR,     // vec3_t (3 floats)
	SG_STRING,     // char* into spawn-string storage — save the CONTENT, re-intern on load
	SG_ENTITY,     // gentity_t* — index-reloc: (ptr - g_entities), -1 = NULL
	SG_CLIENT,     // gclient_t* — index-reloc: (ptr - level.clients), -1 = NULL
	SG_ITEM,       // gitem_t*   — index-reloc: (ptr - bg_itemlist),  -1 = NULL
	SG_FUNCTION,   // callback pointer — name-reloc via the Phase-2 registry (DROPS funcList[])
	SG_RAW         // a fixed-size POD blob copied verbatim (embedded arrays/sub-structs), size in `size`
} sgFieldType_t;

// One descriptor row: the field's byte offset within its struct, its type, and
// (for SG_RAW) the blob size in bytes. Terminated by an { 0, SG_NONE, 0 } row.
typedef struct {
	size_t        ofs;
	sgFieldType_t type;
	size_t        size;   // only meaningful for SG_RAW (embedded array/sub-struct byte size)
} saveField_t;

// A whitelist row for the "restore from the LIVE struct after read" (ignore) and
// the "cross-level persistent transfer" (pers) mechanisms — a byte range, not a
// typed field. Terminated by an { 0, 0 } row.
typedef struct {
	size_t ofs;
	size_t len;
} saveRange_t;

// ── Descriptor tables (defined in g_save.c) ──────────────────────────────────
// The SAVE-class fields only: transient (r shared-portion, the 7 func-ptrs, the
// FEAT_* pool pointers navState/behaviorState/aiThink) are NOT listed here.
extern const saveField_t  gentityFields[];   // gentity_t  SAVE + index-reloc fields
extern const saveField_t  gclientFields[];   // gclient_t  SAVE + index-reloc fields
extern const saveField_t  levelFields[];     // level_locals_t SAVE fields

// Fields NOT taken from disk — restored from the live struct after a read so
// runtime-only state (pool slots, event ring) survives the load.
extern const saveRange_t  gentityIgnoreFields[];
extern const saveRange_t  gclientIgnoreFields[];

// The cross-level (.psw) persistent whitelist — the player-progression subset
// that carries across a map change. Data only in Phase-1; consumed in Phase-5.
extern const saveRange_t  gclientPersFields[];

#endif // G_SAVE_H
