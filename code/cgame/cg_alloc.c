/*
===========================================================================
cg_alloc.c -- Cgame-local memory allocator

Simple bump allocator with two regions:
- Permanent region: grows from the start (parser tables, etc.)
- Temp region: grows from the end (hud elements, text commands)

CG_AllocReset() only resets the temp region. Safe to call on HUD reload.
===========================================================================
*/
#include "cg_local.h"

#define CG_ALLOC_POOL_SIZE  ( 1024 * 1024 )   // 1MB pool
#define CG_ALLOC_ALIGNMENT  8

static char cgAllocPool[CG_ALLOC_POOL_SIZE];

/* Permanent region: grows upward from 0 */
static int cgPermOffset = 0;

/* Temp region: grows downward from end */
static int cgTempOffset = CG_ALLOC_POOL_SIZE;

/* Which region to allocate from (0 = temp, 1 = permanent) */
static int cgAllocPermanent = 0;

static int CG_AlignUp(int val) {
	return (val + CG_ALLOC_ALIGNMENT - 1) & ~(CG_ALLOC_ALIGNMENT - 1);
}

void CG_AllocSetPermanent(int permanent) {
	cgAllocPermanent = permanent;
}

void *CG_Alloc( int size ) {
	if ( size <= 0 ) return NULL;

	int aligned = CG_AlignUp(size);

	if (cgAllocPermanent) {
		if (cgPermOffset + aligned > cgTempOffset) {
			CG_Printf(S_COLOR_RED "CG_Alloc: pool exhausted (perm %d + %d > temp %d)\n",
				cgPermOffset, aligned, cgTempOffset);
			return NULL;
		}
		{
			void *ptr = cgAllocPool + cgPermOffset;
			cgPermOffset += aligned;
			memset(ptr, 0, aligned);
			return ptr;
		}
	} else {
		if (cgTempOffset - aligned < cgPermOffset) {
			CG_Printf(S_COLOR_RED "CG_Alloc: pool exhausted (temp %d - %d < perm %d)\n",
				cgTempOffset, aligned, cgPermOffset);
			return NULL;
		}
		cgTempOffset -= aligned;
		memset(cgAllocPool + cgTempOffset, 0, aligned);
		return cgAllocPool + cgTempOffset;
	}
}

void CG_Free( void *ptr ) {
	/* bump allocator — individual frees are no-ops.
	   Temp region is bulk-freed by CG_AllocReset(). */
}

/* Resets only the temp region. Permanent allocations survive. */
void CG_AllocReset( void ) {
	cgTempOffset = CG_ALLOC_POOL_SIZE;
}
