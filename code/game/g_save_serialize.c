// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_serialize.c — the field-walker (Phase-4), the game-type-free core of the
// .svg serializer. It walks a Phase-1 descriptor table over a struct, streaming
// each field by type: scalars/RAW verbatim, SG_STRING length-prefixed content,
// SG_ENTITY/CLIENT/ITEM index-relocated, SG_FUNCTION as the Phase-2 registry name.
//
// Deliberately game-type-free (only <string.h>/<stdint.h> + g_save.h for the
// field-type enum + g_save_funcs.h for the callback resolvers): the relocation
// bases are passed in, never named, so the round-trip unit test drives this with
// synthetic structs. The game glue (the entity/client/level walks over the real
// g_entities/level) lives in g_save_world.c.
//
// Phase-4 byte-identical: nothing calls this until the cheat-gated command.

#include <string.h>
#include <stdint.h>

#include "g_save.h"
#include "g_save_funcs.h"       // SG_FunctionToName / SG_NameToFunction
#include "g_save_serialize.h"

// The on-wire tokens for the variable-length fields. NULL is distinct from empty
// so a NULL char*/callback round-trips as NULL, an empty string as "".
#define SG_STR_NULL   ( -1 )    // char* / callback was NULL
#define SG_IDX_NULL   ( -1 )    // pointer field was NULL

// ── the byte stream ──────────────────────────────────────────────────────────

void SG_StreamInitWrite( sgStream_t *s, void *buf, size_t cap ) {
	s->buf = (uint8_t *)buf;
	s->cap = cap;
	s->len = 0;
	s->pos = 0;
	s->overflow = 0;
}

void SG_StreamInitRead( sgStream_t *s, const void *buf, size_t len ) {
	// The read path never writes through buf; the const is dropped only to share
	// the one struct shape (writes are gated by direction, not by the type).
	s->buf = (uint8_t *)(uintptr_t)buf;
	s->cap = len;
	s->len = len;
	s->pos = 0;
	s->overflow = 0;
}

void SG_StreamWriteRaw( sgStream_t *s, const void *src, size_t n ) {
	if ( s->overflow || s->pos + n > s->cap ) {
		s->overflow = 1;
		return;
	}
	if ( n ) {
		memcpy( s->buf + s->pos, src, n );
	}
	s->pos += n;
	s->len = s->pos;
}

void SG_StreamReadRaw( sgStream_t *s, void *dst, size_t n ) {
	if ( s->overflow || s->pos + n > s->len ) {
		s->overflow = 1;
		if ( n ) {
			memset( dst, 0, n );
		}
		return;
	}
	if ( n ) {
		memcpy( dst, s->buf + s->pos, n );
	}
	s->pos += n;
}

void    SG_StreamWriteI32( sgStream_t *s, int32_t v ) { SG_StreamWriteRaw( s, &v, sizeof( v ) ); }
int32_t SG_StreamReadI32( sgStream_t *s ) { int32_t v; SG_StreamReadRaw( s, &v, sizeof( v ) ); return v; }

// Internal short aliases for the walker (below), matching the public names.
#define SG_Put( s, src, n )  SG_StreamWriteRaw( (s), (src), (n) )
#define SG_Get( s, dst, n )  SG_StreamReadRaw( (s), (dst), (n) )
#define SG_PutI32( s, v )    SG_StreamWriteI32( (s), (v) )
#define SG_GetI32( s )       SG_StreamReadI32( (s) )

// ── index relocation ─────────────────────────────────────────────────────────

int32_t SG_PtrToIndex( const void *ptr, const void *base, size_t stride, int count ) {
	ptrdiff_t off;
	int32_t   idx;

	if ( ptr == NULL || base == NULL || stride == 0 ) {
		return SG_IDX_NULL;
	}
	off = (const uint8_t *)ptr - (const uint8_t *)base;
	// A pointer that is not an exact element of this array is out of contract; a
	// negative or non-aligned offset yields NULL (defensive — should not happen
	// for a well-formed descriptor).
	if ( off < 0 || (size_t)off % stride != 0 ) {
		return SG_IDX_NULL;
	}
	idx = (int32_t)( off / (ptrdiff_t)stride );
	if ( idx < 0 || idx >= count ) {
		return SG_IDX_NULL;
	}
	return idx;
}

int SG_IndexInRange( int32_t index, int count ) {
	return ( index == SG_IDX_NULL ) || ( index >= 0 && index < count );
}

// Pick the reloc base for a pointer field type. Returns 0 for a non-pointer type.
static int SG_RelocFor( sgFieldType_t type, const sgRelocBases_t *b,
	const void **base, size_t *stride, int *count ) {
	switch ( type ) {
	case SG_ENTITY: *base = b->entityBase; *stride = b->entityStride; *count = b->entityCount; return 1;
	case SG_CLIENT: *base = b->clientBase; *stride = b->clientStride; *count = b->clientCount; return 1;
	case SG_ITEM:   *base = b->itemBase;   *stride = b->itemStride;   *count = b->itemCount;   return 1;
	default: return 0;
	}
}

// ── variable-length helpers (string content + callback name) ─────────────────
// Wire form: int32 len (SG_STR_NULL = the pointer was NULL, 0 = empty, n = n bytes
// follow, NOT NUL-terminated). Symmetric on both directions.

static void SG_PutStr( sgStream_t *s, const char *str ) {
	if ( str == NULL ) {
		SG_PutI32( s, SG_STR_NULL );
		return;
	}
	{
		int32_t n = (int32_t)strlen( str );
		SG_PutI32( s, n );
		SG_Put( s, str, (size_t)n );
	}
}

// Read a string token, copying up to (dstCap-1) bytes into dst (always NUL-term).
// A NULL token yields an empty dst and sets *wasNull. Phase-6 re-interns dst via
// G_NewString; Phase-4 just needs the content parsed + validated.
static void SG_GetStr( sgStream_t *s, char *dst, size_t dstCap, int *wasNull ) {
	int32_t n = SG_GetI32( s );
	*wasNull = 0;
	if ( n == SG_STR_NULL ) {
		*wasNull = 1;
		if ( dstCap ) dst[0] = '\0';
		return;
	}
	if ( n < 0 || (size_t)n >= dstCap ) {
		// A length that cannot fit the caller buffer is a malformed / oversized
		// token — mark the whole op void.
		s->overflow = 1;
		if ( dstCap ) dst[0] = '\0';
		return;
	}
	SG_Get( s, dst, (size_t)n );
	dst[n] = '\0';
}

// Scratch for a string/callback field during the walk. MAX save token length —
// classnames/targetnames are short; a spawn string over this is rejected (the
// overflow flag), not truncated.
#define SG_STR_SCRATCH 1024

// ── the field-walker ─────────────────────────────────────────────────────────

int SG_WriteFields( const saveField_t *fields, const void *base,
	const sgRelocBases_t *bases, sgStream_t *s ) {
	const saveField_t *f;

	for ( f = fields; f->type != SG_NONE; f++ ) {
		const uint8_t *fieldPtr = (const uint8_t *)base + f->ofs;

		switch ( f->type ) {
		case SG_INT:
		case SG_FLOAT:
		case SG_QBOOLEAN:
			SG_Put( s, fieldPtr, 4 );
			break;
		case SG_VECTOR:
			SG_Put( s, fieldPtr, 12 );  // 3 floats
			break;
		case SG_RAW:
			SG_Put( s, fieldPtr, f->size );
			break;
		case SG_STRING: {
			const char *str = *(const char *const *)fieldPtr;
			SG_PutStr( s, str );
			break;
		}
		case SG_FUNCTION: {
			void *fn = *(void *const *)fieldPtr;
			// The registered NAME (or "" for NULL) — Phase-6 resolves it back.
			SG_PutStr( s, SG_FunctionToName( fn ) );
			break;
		}
		case SG_ENTITY:
		case SG_CLIENT:
		case SG_ITEM: {
			const void *rbase; size_t rstride; int rcount;
			const void *ptr = *(const void *const *)fieldPtr;
			SG_RelocFor( f->type, bases, &rbase, &rstride, &rcount );
			SG_PutI32( s, SG_PtrToIndex( ptr, rbase, rstride, rcount ) );
			break;
		}
		default:
			// Every type in the Phase-1 tables is handled above; an unknown type
			// is a descriptor bug — void the op rather than silently skip.
			s->overflow = 1;
			break;
		}
		if ( s->overflow ) {
			return 0;
		}
	}
	return 1;
}

int SG_ReadFields( const saveField_t *fields, void *base,
	const sgRelocBases_t *bases, sgStream_t *s,
	char *( *intern )( const char *content ) ) {
	const saveField_t *f;

	for ( f = fields; f->type != SG_NONE; f++ ) {
		uint8_t *fieldPtr = (uint8_t *)base + f->ofs;

		switch ( f->type ) {
		case SG_INT:
		case SG_FLOAT:
		case SG_QBOOLEAN:
			SG_Get( s, fieldPtr, 4 );
			break;
		case SG_VECTOR:
			SG_Get( s, fieldPtr, 12 );
			break;
		case SG_RAW:
			SG_Get( s, fieldPtr, f->size );
			break;
		case SG_STRING: {
			// Parse the content. With `intern` (the live load, G_NewString) the
			// re-interned char* is stored in the field; without it (validate-only
			// mode — the read-to-buffer callers) the content is parsed + validated
			// then discarded. A NULL token stores NULL either way.
			char scratch[SG_STR_SCRATCH];
			int  wasNull;
			SG_GetStr( s, scratch, sizeof( scratch ), &wasNull );
			if ( intern ) {
				char *stored = wasNull ? NULL : intern( scratch );
				memcpy( fieldPtr, &stored, sizeof( char * ) );
			}
			break;
		}
		case SG_FUNCTION: {
			// The registered name resolves to a live callback pointer immediately —
			// SG_NameToFunction is game-type-free. A NULL/empty name is a NULL
			// callback; an unknown non-empty name is a version/corruption signal
			// (the save references a callback this build lacks) and voids the op.
			char  scratch[SG_STR_SCRATCH];
			int   wasNull;
			void *fn = NULL;
			SG_GetStr( s, scratch, sizeof( scratch ), &wasNull );
			if ( !wasNull && scratch[0] != '\0' ) {
				fn = SG_NameToFunction( scratch );
				if ( fn == NULL ) {
					s->overflow = 1;
					break;
				}
			}
			// Store the resolved pointer (NULL for a NULL/empty name). In validate-
			// only mode we still store it — a resolved fn ptr is harmless in a
			// scratch struct, and it keeps the read self-consistent.
			memcpy( fieldPtr, &fn, sizeof( void * ) );
			break;
		}
		case SG_ENTITY:
		case SG_CLIENT:
		case SG_ITEM: {
			const void *rbase; size_t rstride; int rcount;
			int32_t idx = SG_GetI32( s );
			SG_RelocFor( f->type, bases, &rbase, &rstride, &rcount );
			if ( !SG_IndexInRange( idx, rcount ) ) {
				s->overflow = 1;  // out-of-range index = corrupt save
				break;
			}
			// Phase-A stores the INDEX in the pointer-sized slot; the Phase-B
			// SG_ResolveIndices pass turns it into &base[idx] once every slot is
			// populated (so forward-references resolve). idx>=0 = pending-resolve,
			// SG_IDX_NULL = NULL.
			memset( fieldPtr, 0, sizeof( void * ) );
			*(int32_t *)fieldPtr = idx;
			break;
		}
		default:
			s->overflow = 1;
			break;
		}
		if ( s->overflow ) {
			return 0;
		}
	}
	return 1;
}

// ── Phase-B: resolve the index-in-pointer-slot to a live pointer ─────────────
void SG_ResolveIndices( const saveField_t *fields, void *base,
	const sgRelocBases_t *bases ) {
	const saveField_t *f;

	for ( f = fields; f->type != SG_NONE; f++ ) {
		uint8_t    *fieldPtr = (uint8_t *)base + f->ofs;
		const void *rbase; size_t rstride; int rcount;
		int32_t     idx;
		void       *resolved;

		if ( !SG_RelocFor( f->type, bases, &rbase, &rstride, &rcount ) ) {
			continue;  // not a pointer field
		}
		idx = *(int32_t *)fieldPtr;
		if ( idx == SG_IDX_NULL || rbase == NULL ) {
			resolved = NULL;
		} else {
			resolved = (void *)( (const uint8_t *)rbase + (size_t)idx * rstride );
		}
		memcpy( fieldPtr, &resolved, sizeof( void * ) );
	}
}

// ── the range-walker (saveRange_t whitelists — the .psw persistent subset) ───
// Each { ofs, len } names a whole POD sub-struct copied verbatim. Symmetric: the
// write streams the bytes, the read pulls them back into the same offsets.

int SG_WriteRanges( const saveRange_t *ranges, const void *base, sgStream_t *s ) {
	const saveRange_t *r;
	for ( r = ranges; r->len != 0; r++ ) {
		SG_Put( s, (const uint8_t *)base + r->ofs, r->len );
		if ( s->overflow ) {
			return 0;
		}
	}
	return 1;
}

int SG_ReadRanges( const saveRange_t *ranges, void *base, sgStream_t *s ) {
	const saveRange_t *r;
	for ( r = ranges; r->len != 0; r++ ) {
		SG_Get( s, (uint8_t *)base + r->ofs, r->len );
		if ( s->overflow ) {
			return 0;
		}
	}
	return 1;
}
