/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2024 Wired engine contributors

This file is part of the Wired Engine (derived from idTech 3 & 4 source
code and community around it). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version.

Quake III Arena, q3now, Wired Engine and the rest are licensed under the
**GNU General Public License, version 2 or later (GPL-2.0-or-later)**.
The full license text is in `LICENSE` and `THIRD_PARTY_LICENSES.md` at the
repository root.
===========================================================================
*/

/*****************************************************************************
 * name:		l_memory.h
 *
 * desc:		memory management
 *
 * $Archive: /source/code/botlib/l_memory.h $
 *
 *****************************************************************************/

//#define MEMDEBUG

#ifdef MEMDEBUG
#define GetMemory(size)				GetMemoryDebug(size, #size, __FILE__, __LINE__);
#define GetClearedMemory(size)		GetClearedMemoryDebug(size, #size, __FILE__, __LINE__);
//allocate a memory block of the given size
void *GetMemoryDebug(size_t size, const char *label, const char *file, int line);
//allocate a memory block of the given size and clear it
void *GetClearedMemoryDebug(size_t size, const char *label, const char *file, int line);
//
#define GetHunkMemory(size)			GetHunkMemoryDebug(size, #size, __FILE__, __LINE__);
#define GetClearedHunkMemory(size)	GetClearedHunkMemoryDebug(size, #size, __FILE__, __LINE__);
//allocate a memory block of the given size
void *GetHunkMemoryDebug(size_t size, const char *label, const char *file, int line);
//allocate a memory block of the given size and clear it
void *GetClearedHunkMemoryDebug(size_t size, const char *label, const char *file, int line);
#else
//allocate a memory block of the given size
void *GetMemory(size_t size);
//allocate a memory block of the given size and clear it
void *GetClearedMemory(size_t size);
//
#ifdef BSPC
#define GetHunkMemory GetMemory
#define GetClearedHunkMemory GetClearedMemory
#else
//allocate a memory block of the given size
void *GetHunkMemory(size_t size);
//allocate a memory block of the given size and clear it
void *GetClearedHunkMemory(size_t size);
#endif
#endif

//free the given memory block
void FreeMemory(void *ptr);
//returns the amount available memory
int AvailableMemory(void);
//prints the total used memory size
void PrintUsedMemorySize(void);
//print all memory blocks with label
void PrintMemoryLabels(void);
//returns the size of the memory block in bytes
int MemoryByteSize(void *ptr);
//free all allocated memory
void DumpMemory(void);
