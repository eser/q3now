// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// sv_game.c -- interface to the game dll

#include "server.h"
#include "../qcommon/q_feats.h"

#include "../qcommon/wired/net/wn_public.h"

#include "../botlib/botlib.h"

// Typed/versioned VM-IPC (docs/vm-typed-ipc-design.md). The hot/high-traffic game
// syscalls are dispatched through a per-syscall descriptor + VM_UnmarshalTyped
// (validated argc + per-arg type + VMA/bounds translation) instead of hand-written
// VMA(x) casts. The wire is UNCHANGED (flat args[13]); un-migrated handlers keep
// their casts and read the same args. A reserved-high id answers the ABI handshake.
#include "../qcommon/vm_typed_syscall.h"

LOG_DECLARE_CHANNEL( ch_game, "game" );

#if FEAT_RECAST_NAVMESH
#include "../qcommon/nav/nav_public.h"
#endif

botlib_export_t	*botlib_export;

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int	SV_NumForGentity( sharedEntity_t *ent ) {
	int num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;
	return num;
}


sharedEntity_t *SV_GentityNum( int num ) {
	if ( num < 0 || num >= MAX_GENTITIES ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad num %d", __func__, num );
	}
	sharedEntity_t *ent = (sharedEntity_t *)((byte *)sv.gentities + sv.gentitySize*(num));
	return ent;
}


playerState_t *SV_GameClientNum( int num ) {
	playerState_t *ps = (playerState_t *)((byte *)sv.gameClients + sv.gameClientSize*(num));
	return ps;
}


svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	if ( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES ) {
		Com_Terminate( TERM_CLIENT_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	return &sv.svEntities[ gEnt->s.number ];
}


sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	int num = svEnt - sv.svEntities;
	return SV_GentityNum( num );
}


/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
static void SV_GameSendServerCommand( int clientNum, const char *text ) {
	if ( clientNum == -1 ) {
		SV_SendServerCommand( NULL, "%s", text );
	} else {
		if ( clientNum < 0 || clientNum >= sv.maxclients ) {
			return;
		}
		SV_SendServerCommand( svs.clients + clientNum, "%s", text );
	}
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
static void SV_GameDropClient( int clientNum, const char *reason ) {
	if ( clientNum < 0 || clientNum >= sv.maxclients ) {
		return;
	}
	SV_DropClient( svs.clients + clientNum, reason );
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
static void SV_SetBrushModel( sharedEntity_t *ent, const char *name ) {
	if ( !name ) {
		Com_Terminate( TERM_CLIENT_DROP, "SV_SetBrushModel: NULL" );
	}

	if ( name[0] != '*' ) {
		Com_Terminate( TERM_CLIENT_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}

	ent->s.modelindex = atoi( name + 1 );

	clipHandle_t h = CM_InlineModel( ent->s.modelindex );
	vec3_t mins, maxs;
	CM_ModelBounds( h, mins, maxs );
	VectorCopy (mins, ent->r.mins);
	VectorCopy (maxs, ent->r.maxs);
	ent->r.bmodel = qtrue;

	ent->r.contents = -1;		// we don't know exactly what is in the brushes

	SV_LinkEntity( ent );		// FIXME: remove
}


/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS( const vec3_t p1, const vec3_t p2 )
{
	int leafnum = CM_PointLeafnum (p1);
	int cluster = CM_LeafCluster (leafnum);
	if ( cluster < 0 )
		return qfalse;
	int area1 = CM_LeafArea (leafnum);
	byte *mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	if ( cluster < 0 )
		return qfalse;
	int area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;
	if (!CM_AreasConnected (area1, area2))
		return qfalse;		// a door blocks sight
	return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
static qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 )
{
	int leafnum = CM_PointLeafnum (p1);
	int cluster = CM_LeafCluster (leafnum);
	if ( cluster < 0 )
		return qfalse;
	byte *mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	if ( cluster < 0 )
		return qfalse;

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return qfalse;

	return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
static void SV_AdjustAreaPortalState( sharedEntity_t *ent, qboolean open ) {
	svEntity_t *svEnt = SV_SvEntityForGentity( ent );
	if ( svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}


/*
==================
SV_EntityContact
==================
*/
static qboolean SV_EntityContact( const vec3_t mins, const vec3_t maxs, const sharedEntity_t *gEnt, const int capsule ) {
	// check for exact collision
	const float *origin = gEnt->r.currentOrigin;
	const float *angles = gEnt->r.currentAngles;
	clipHandle_t ch = SV_ClipHandleForEntity( gEnt );
	trace_t trace;
	CM_TransformedBoxTrace( &trace, vec3_origin, vec3_origin, mins, maxs, ch, -1, origin, angles, capsule );

	return trace.startsolid;
}


/*
===============
SV_GetServerinfo
===============
*/
static void SV_GetServerinfo( char *buffer, int bufferSize ) {

	if ( bufferSize < 1 ) {
		Com_Terminate( TERM_CLIENT_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
	}
	if ( sv.state != SS_GAME || !sv.configstrings[ CS_SERVERINFO ] ) {
		Q_strncpyz( buffer, Cvar_InfoString( CVAR_SERVERINFO, NULL ), bufferSize );
	} else {
		Q_strncpyz( buffer, sv.configstrings[ CS_SERVERINFO ], bufferSize );
	}
}


/*
===============
SV_LocateGameData

===============
*/
static void SV_LocateGameData( sharedEntity_t *gEnts, unsigned numGEntities, unsigned sizeofGEntity_t,
							   playerState_t *clients, unsigned sizeofGameClient ) {

	if ( !gvm->entryPoint ) {
		if ( numGEntities > MAX_GENTITIES ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad entity count %u", __func__, numGEntities );
		}

		if ( sizeofGEntity_t < sizeof(sharedEntity_t) || sizeofGEntity_t > gvm->exactDataLength / MAX_GENTITIES ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad entity size %u", __func__, sizeofGEntity_t );
		} else if ( (byte*)gEnts - gvm->dataBase > gvm->exactDataLength - sizeofGEntity_t * MAX_GENTITIES ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: entities located out of data segment", __func__ );
		}

		if ( sizeofGameClient < sizeof(playerState_t) || sizeofGameClient > gvm->exactDataLength / MAX_CLIENTS ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: bad game client size %u", __func__, sizeofGameClient );
		} else if ( (byte*)clients - gvm->dataBase > gvm->exactDataLength - sizeofGameClient * MAX_CLIENTS ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s: clients located out of data segment", __func__ );
		}
	}

	sv.gentities = gEnts;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = clients;
	sv.gameClientSize = sizeofGameClient;
}


/*
===============
SV_GetUsercmd
===============
*/
static void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	if ( (unsigned) clientNum < sv.maxclients ) {
		*cmd = svs.clients[ clientNum ].lastUsercmd;
	} else {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): bad clientNum: %i", __func__, clientNum );
	}
}


//==============================================

static int FloatAsInt( float f ) {
	floatint_t fi;
	fi.f = f;
	return fi.i;
}


/*
====================
VM_ArgPtr
====================
*/
static void *VM_ArgPtr( intptr_t intValue ) {

	if ( !intValue || gvm == NULL )
		return NULL;

	if ( gvm->entryPoint )
		return (void *)(intValue);
	return (void *)( gvm->dataBase + ( intValue & gvm->dataMask ) );
}


/*
====================
GVM_ArgPtr

exported version
====================
*/
void *GVM_ArgPtr( intptr_t intValue )
{
	return VM_ArgPtr( intValue );
}


static qboolean SV_GetValue( char* value, int valueSize, const char* key )
{
	if ( !Q_stricmp( key, "SVF_SELF_PORTAL2_Q3E" ) )
	{
		Com_sprintf( value, valueSize, "%i", SVF_SELF_PORTAL2 );
		return qtrue;
	}

	if ( !Q_stricmp( key, "trap_Cvar_SetDescription_Q3E" ) )
	{
		Com_sprintf( value, valueSize, "%i", G_CVAR_SETDESCRIPTION );
		return qtrue;
	}

	if ( strncmp( key, "char:", 5 ) == 0 ) {
		const char *after = key + 5;
		const char *colon = strchr( after, ':' );
		if ( colon ) {
			const char *field = colon + 1;
			char charName[MAX_QPATH];
			int nameLen = (int)( colon - after );
			if ( nameLen >= MAX_QPATH ) nameLen = MAX_QPATH - 1;
			memcpy( charName, after, nameLen );
			charName[nameLen] = '\0';
			if ( Q_stricmp( field, "display_name" ) == 0 ) {
				return SV_Lua_GetCharacterDisplayName( charName, value, valueSize );
			}
			if ( Q_stricmp( field, "bbox" ) == 0 ) {
				return SV_Lua_GetCharacterBBox( charName, value, valueSize );
			}
			if ( Q_stricmp( field, "movement" ) == 0 ) {
				return SV_Lua_GetCharacterMovement( charName, value, valueSize );
			}
			if ( Q_stricmp( field, "attack" ) == 0 ) {
				return SV_Lua_GetCharacterAttack( charName, value, valueSize );
			}
			if ( Q_stricmp( field, "can_activate" ) == 0 ) {
				return SV_Lua_GetCharacterCanActivate( charName, value, valueSize );
			}
		}
		return qfalse;
	}

	if ( !Q_stricmp( key, "char_count" ) ) {
		Com_sprintf( value, valueSize, "%d", SV_Lua_GetCharacterCount() );
		return qtrue;
	}

	if ( strncmp( key, "char_at:", 8 ) == 0 ) {
		int idx = atoi( key + 8 );
		return SV_Lua_GetCharacterAt( idx, value, valueSize );
	}

	return qfalse;
}


/*
====================
Typed-IPC descriptor catalogue — game (docs/vm-typed-ipc-design.md, shape A)

The hot/high-traffic game syscalls. Each row is { id (UNCHANGED), name, argc,
per-arg types }. SV_GameSystemCalls unmarshals these through VM_UnmarshalTyped
instead of hand-casting VMA(x). The catalogue grows one reviewable batch at a time;
un-migrated syscalls keep their inline casts on the same flat wire.
====================
*/

// G_LOG( severity:int, channel:ptr, text:ptr )
static const vmSyscallDesc_t sv_desc_G_LOG = {
	G_LOG, "G_LOG", 3, { VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
// G_PRINT( text:ptr )
static const vmSyscallDesc_t sv_desc_G_PRINT = {
	G_PRINT, "G_PRINT", 1, { VARG_VMPTR }
};
// G_ERROR( text:ptr )
static const vmSyscallDesc_t sv_desc_G_ERROR = {
	G_ERROR, "G_ERROR", 1, { VARG_VMPTR }
};
// G_MILLISECONDS( void ) takes no args → no descriptor (the typed unmarshal would
// be a no-op; the bare Sys_Milliseconds() handler stays hot-path-cheap, W-41).

// ── cvar subsystem ───────────────────────────────────────────────────────────
// G_CVAR_REGISTER( vmCvar*:ptr, varName:ptr, default:ptr, flags:int )
//   vmCvar is a VM-memory vmCvar_t the engine writes back through; a plain VMPTR
//   (the existing VMA(1) translation) — NULL is allowed (register-without-handle),
//   which VM_ArgPtr maps to NULL exactly as today. The descriptor types the
//   translation, not the direction, so a write-back ptr is just a VARG_VMPTR.
static const vmSyscallDesc_t sv_desc_G_CVAR_REGISTER = {
	G_CVAR_REGISTER, "G_CVAR_REGISTER", 4, { VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
// G_CVAR_UPDATE( vmCvar*:ptr ) — struct write-back ptr (same VMPTR semantics)
static const vmSyscallDesc_t sv_desc_G_CVAR_UPDATE = {
	G_CVAR_UPDATE, "G_CVAR_UPDATE", 1, { VARG_VMPTR }
};
// G_CVAR_SET( var_name:ptr, value:ptr )
static const vmSyscallDesc_t sv_desc_G_CVAR_SET = {
	G_CVAR_SET, "G_CVAR_SET", 2, { VARG_VMPTR, VARG_VMPTR }
};
// G_CVAR_VARIABLE_INTEGER_VALUE( var_name:ptr )
static const vmSyscallDesc_t sv_desc_G_CVAR_VARIABLE_INTEGER_VALUE = {
	G_CVAR_VARIABLE_INTEGER_VALUE, "G_CVAR_VARIABLE_INTEGER_VALUE", 1, { VARG_VMPTR }
};
// G_CVAR_VARIABLE_STRING_BUFFER( var_name:ptr, buffer:ptr[bufsize], bufsize:int )
//   buffer is the FIRST sized output pointer: VARG_VMPTR_SIZED reads the following
//   arg (bufsize) as its length and bounds-checks (offset,len) via VM_CheckBounds —
//   exactly the VM_CHECKBOUNDS(gvm, args[2], args[3]) the handler did by hand.
static const vmSyscallDesc_t sv_desc_G_CVAR_VARIABLE_STRING_BUFFER = {
	G_CVAR_VARIABLE_STRING_BUFFER, "G_CVAR_VARIABLE_STRING_BUFFER", 3,
	{ VARG_VMPTR, VARG_VMPTR_SIZED, VARG_INT }
};
// G_CVAR_SETDESCRIPTION( var_name:ptr, description:ptr )
static const vmSyscallDesc_t sv_desc_G_CVAR_SETDESCRIPTION = {
	G_CVAR_SETDESCRIPTION, "G_CVAR_SETDESCRIPTION", 2, { VARG_VMPTR, VARG_VMPTR }
};

// ── cmd / args subsystem ─────────────────────────────────────────────────────
// G_ARGC( void ) takes no args → stays bare (no descriptor; W-41, like MILLISECONDS).
// G_ARGV( n:int, buffer:ptr[bufferLength], bufferLength:int ) — sized output buffer
// (VARG_VMPTR_SIZED reads the following arg as len, bounds-checks via VM_CheckBounds —
// the same VM_CHECKBOUNDS(gvm, args[2], args[3]) the handler did by hand).
static const vmSyscallDesc_t sv_desc_G_ARGV = {
	G_ARGV, "G_ARGV", 3, { VARG_INT, VARG_VMPTR_SIZED, VARG_INT }
};
// G_SEND_CONSOLE_COMMAND( exec_when:int, text:ptr )
static const vmSyscallDesc_t sv_desc_G_SEND_CONSOLE_COMMAND = {
	G_SEND_CONSOLE_COMMAND, "G_SEND_CONSOLE_COMMAND", 2, { VARG_INT, VARG_VMPTR }
};
// G_SEND_SERVER_COMMAND( clientNum:int, text:ptr )
static const vmSyscallDesc_t sv_desc_G_SEND_SERVER_COMMAND = {
	G_SEND_SERVER_COMMAND, "G_SEND_SERVER_COMMAND", 2, { VARG_INT, VARG_VMPTR }
};

// ── fs subsystem ─────────────────────────────────────────────────────────────
// G_FS_FOPEN_FILE( qpath:ptr, file:ptr, mode:int ) — `file` is a fileHandle_t the
// engine WRITES the opened handle back through: a plain VARG_VMPTR (the descriptor
// types the translation, not the direction — same as cvar's vmCvar_t write-back).
static const vmSyscallDesc_t sv_desc_G_FS_FOPEN_FILE = {
	G_FS_FOPEN_FILE, "G_FS_FOPEN_FILE", 3, { VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
// G_FS_READ( buffer:ptr[len], len:int, f:int ) — the canonical sized buffer; the
// VARG_VMPTR_SIZED entry reads the following arg (len) and bounds-checks via
// VM_CheckBounds, exactly the VM_CHECKBOUNDS(gvm, args[1], args[2]) by hand.
static const vmSyscallDesc_t sv_desc_G_FS_READ = {
	G_FS_READ, "G_FS_READ", 3, { VARG_VMPTR_SIZED, VARG_INT, VARG_INT }
};
// G_FS_WRITE( buffer:ptr[len], len:int, f:int )
static const vmSyscallDesc_t sv_desc_G_FS_WRITE = {
	G_FS_WRITE, "G_FS_WRITE", 3, { VARG_VMPTR_SIZED, VARG_INT, VARG_INT }
};
// G_FS_FCLOSE_FILE( f:int ) — all-int (NOT 0-arg, so it gets a descriptor).
static const vmSyscallDesc_t sv_desc_G_FS_FCLOSE_FILE = {
	G_FS_FCLOSE_FILE, "G_FS_FCLOSE_FILE", 1, { VARG_INT }
};
// G_FS_SEEK( f:int, offset:int, origin:int ) — FS_VM_SeekFile takes `long offset`,
// but the wire passes it as one i32 (a wasm32 `long` is 32-bit, zero-extended into
// args[2] like the hand path did); VARG_INT reproduces that, cast to long at the
// call. No width gap — it's an int on the wire either way.
static const vmSyscallDesc_t sv_desc_G_FS_SEEK = {
	G_FS_SEEK, "G_FS_SEEK", 3, { VARG_INT, VARG_INT, VARG_INT }
};
// G_FS_GETFILELIST( path:ptr, ext:ptr, listbuf:ptr[bufsize], bufsize:int ) — the
// list buffer is the sized output (3rd arg, bounds-checked against bufsize=args[4]).
static const vmSyscallDesc_t sv_desc_G_FS_GETFILELIST = {
	G_FS_GETFILELIST, "G_FS_GETFILELIST", 4, { VARG_VMPTR, VARG_VMPTR, VARG_VMPTR_SIZED, VARG_INT }
};
// G_FS_RENAME( from:ptr, to:ptr ) — two string pointers, marshalled exactly like
// G_CVAR_SET / G_CVAR_SETDESCRIPTION (2x VARG_VMPTR). Path-based (no VM handle), so
// it calls the engine FS_Rename directly (like FS_GetFileList), not an FS_VM_* form.
static const vmSyscallDesc_t sv_desc_G_FS_RENAME = {
	G_FS_RENAME, "G_FS_RENAME", 2, { VARG_VMPTR, VARG_VMPTR }
};

// ── collision / spatial-query subsystem ──────────────────────────────────────
// vec3_t args cross the boundary as VM POINTERS (const vec3_t = const float*,
// translated via VMA), NOT 3 spread floats — so every coordinate is a VARG_VMPTR,
// and the trace_t* result is a VARG_VMPTR the engine writes back through (the
// fileHandle_t*/vmCvar_t precedent). No VARG_FLOAT here (no bare-float scalar args).
// G_TRACE/G_TRACECAPSULE( results:ptr, start:ptr, mins:ptr, maxs:ptr, end:ptr, passEnt:int, mask:int )
static const vmSyscallDesc_t sv_desc_G_TRACE = {
	G_TRACE, "G_TRACE", 7,
	{ VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_INT, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_G_TRACECAPSULE = {
	G_TRACECAPSULE, "G_TRACECAPSULE", 7,
	{ VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_INT, VARG_INT }
};
// G_POINT_CONTENTS( point:ptr, passEnt:int )
static const vmSyscallDesc_t sv_desc_G_POINT_CONTENTS = {
	G_POINT_CONTENTS, "G_POINT_CONTENTS", 2, { VARG_VMPTR, VARG_INT }
};
// G_ENTITY_CONTACT/CAPSULE( mins:ptr, maxs:ptr, ent:ptr )
static const vmSyscallDesc_t sv_desc_G_ENTITY_CONTACT = {
	G_ENTITY_CONTACT, "G_ENTITY_CONTACT", 3, { VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_G_ENTITY_CONTACTCAPSULE = {
	G_ENTITY_CONTACTCAPSULE, "G_ENTITY_CONTACTCAPSULE", 3, { VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};
// G_IN_PVS/IGNORE_PORTALS( p1:ptr, p2:ptr )
static const vmSyscallDesc_t sv_desc_G_IN_PVS = {
	G_IN_PVS, "G_IN_PVS", 2, { VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_G_IN_PVS_IGNORE_PORTALS = {
	G_IN_PVS_IGNORE_PORTALS, "G_IN_PVS_IGNORE_PORTALS", 2, { VARG_VMPTR, VARG_VMPTR }
};
// G_AREAS_CONNECTED( area1:int, area2:int )
static const vmSyscallDesc_t sv_desc_G_AREAS_CONNECTED = {
	G_AREAS_CONNECTED, "G_AREAS_CONNECTED", 2, { VARG_INT, VARG_INT }
};
// G_SET_BRUSH_MODEL( ent:ptr, name:ptr )
static const vmSyscallDesc_t sv_desc_G_SET_BRUSH_MODEL = {
	G_SET_BRUSH_MODEL, "G_SET_BRUSH_MODEL", 2, { VARG_VMPTR, VARG_VMPTR }
};
// G_ENTITIES_IN_BOX( mins:ptr, maxs:ptr, entityList:int[maxcount], maxcount:int ) —
// entityList is a VARG_VMPTR_COUNTED int array (count = args[4]=maxcount); the
// VM_CheckBounds3(ptr, maxcount, sizeof(int)) bounds moves into VM_UnmarshalTyped.
static const vmSyscallDesc_t sv_desc_G_ENTITIES_IN_BOX = {
	G_ENTITIES_IN_BOX, "G_ENTITIES_IN_BOX", 4, { VARG_VMPTR, VARG_VMPTR, VARG_VMPTR_COUNTED, VARG_INT },
	{ [2] = { sizeof( int ), 4 } }   // entityList: count = args[4] (maxcount)
};

// ── botlib subsystem (game-VM import) ────────────────────────────────────────
// Only the cast-bearing syscalls (VMA pointer / VMF float / sized buffer) get a
// descriptor + typed unpack; pure-int (args[n] only) and 0-arg botlib syscalls stay
// bare (no cast to remove, W-41 — like G_MILLISECONDS / G_ARGC). This batch is where
// VARG_FLOAT is first exercised: the VMF args (skill, thinktime, speed, …) become
// VARG_FLOAT, unpacked as a bit-identical reinterpret. Float RETURNS (FloatAsInt) are
// the return direction — outside the descriptor (untouched), like trace_t* write-back.
// ── base 200-range ──
static const vmSyscallDesc_t sv_desc_BOTLIB_LIBVAR_SET = {
	BOTLIB_LIBVAR_SET, "BOTLIB_LIBVAR_SET", 2, { VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_LIBVAR_GET = {
	BOTLIB_LIBVAR_GET, "BOTLIB_LIBVAR_GET", 3, { VARG_VMPTR, VARG_VMPTR_SIZED, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_PC_ADD_GLOBAL_DEFINE = {
	BOTLIB_PC_ADD_GLOBAL_DEFINE, "BOTLIB_PC_ADD_GLOBAL_DEFINE", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_PC_LOAD_SOURCE = {
	BOTLIB_PC_LOAD_SOURCE, "BOTLIB_PC_LOAD_SOURCE", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_PC_SOURCE_FILE_AND_LINE = {
	BOTLIB_PC_SOURCE_FILE_AND_LINE, "BOTLIB_PC_SOURCE_FILE_AND_LINE", 3, { VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_START_FRAME = {     // VMF(1) — first VARG_FLOAT
	BOTLIB_START_FRAME, "BOTLIB_START_FRAME", 1, { VARG_FLOAT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_LOAD_MAP = {
	BOTLIB_LOAD_MAP, "BOTLIB_LOAD_MAP", 1, { VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_UPDATENTITY = {
	BOTLIB_UPDATENTITY, "BOTLIB_UPDATENTITY", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_TEST = {
	BOTLIB_TEST, "BOTLIB_TEST", 4, { VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_GET_CONSOLE_MESSAGE = {
	BOTLIB_GET_CONSOLE_MESSAGE, "BOTLIB_GET_CONSOLE_MESSAGE", 3, { VARG_INT, VARG_VMPTR_SIZED, VARG_INT }
};

// ── EA 700-range (elementary actions) ──
static const vmSyscallDesc_t sv_desc_BOTLIB_EA_SAY = {
	BOTLIB_EA_SAY, "BOTLIB_EA_SAY", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_EA_SAY_TEAM = {
	BOTLIB_EA_SAY_TEAM, "BOTLIB_EA_SAY_TEAM", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_EA_COMMAND = {
	BOTLIB_EA_COMMAND, "BOTLIB_EA_COMMAND", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_EA_MOVE = {        // VMF(3) speed
	BOTLIB_EA_MOVE, "BOTLIB_EA_MOVE", 3, { VARG_INT, VARG_VMPTR, VARG_FLOAT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_EA_VIEW = {
	BOTLIB_EA_VIEW, "BOTLIB_EA_VIEW", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_EA_END_REGULAR = {  // VMF(2) thinktime
	BOTLIB_EA_END_REGULAR, "BOTLIB_EA_END_REGULAR", 2, { VARG_INT, VARG_FLOAT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_EA_GET_INPUT = {    // VMF(2) thinktime
	BOTLIB_EA_GET_INPUT, "BOTLIB_EA_GET_INPUT", 3, { VARG_INT, VARG_FLOAT, VARG_VMPTR }
};

// ── AI 700-range ──
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_LOAD_CHARACTER = {  // VMF(2) skill
	BOTLIB_AI_LOAD_CHARACTER, "BOTLIB_AI_LOAD_CHARACTER", 2, { VARG_VMPTR, VARG_FLOAT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_CHARACTERISTIC_BFLOAT = {  // VMF(3),VMF(4) + FloatAsInt return
	BOTLIB_AI_CHARACTERISTIC_BFLOAT, "BOTLIB_AI_CHARACTERISTIC_BFLOAT", 4, { VARG_INT, VARG_INT, VARG_FLOAT, VARG_FLOAT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_CHARACTERISTIC_STRING = {
	BOTLIB_AI_CHARACTERISTIC_STRING, "BOTLIB_AI_CHARACTERISTIC_STRING", 4, { VARG_INT, VARG_INT, VARG_VMPTR_SIZED, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_QUEUE_CONSOLE_MESSAGE = {
	BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, "BOTLIB_AI_QUEUE_CONSOLE_MESSAGE", 3, { VARG_INT, VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_NEXT_CONSOLE_MESSAGE = {
	BOTLIB_AI_NEXT_CONSOLE_MESSAGE, "BOTLIB_AI_NEXT_CONSOLE_MESSAGE", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_INITIAL_CHAT = {
	BOTLIB_AI_INITIAL_CHAT, "BOTLIB_AI_INITIAL_CHAT", 11,
	{ VARG_INT, VARG_VMPTR, VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_NUM_INITIAL_CHATS = {
	BOTLIB_AI_NUM_INITIAL_CHATS, "BOTLIB_AI_NUM_INITIAL_CHATS", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_REPLY_CHAT = {
	BOTLIB_AI_REPLY_CHAT, "BOTLIB_AI_REPLY_CHAT", 12,
	{ VARG_INT, VARG_VMPTR, VARG_INT, VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GET_CHAT_MESSAGE = {
	BOTLIB_AI_GET_CHAT_MESSAGE, "BOTLIB_AI_GET_CHAT_MESSAGE", 3, { VARG_INT, VARG_VMPTR_SIZED, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_STRING_CONTAINS = {
	BOTLIB_AI_STRING_CONTAINS, "BOTLIB_AI_STRING_CONTAINS", 3, { VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_FIND_MATCH = {
	BOTLIB_AI_FIND_MATCH, "BOTLIB_AI_FIND_MATCH", 3, { VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_MATCH_VARIABLE = {
	BOTLIB_AI_MATCH_VARIABLE, "BOTLIB_AI_MATCH_VARIABLE", 4, { VARG_VMPTR, VARG_INT, VARG_VMPTR_SIZED, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_UNIFY_WHITE_SPACES = {
	BOTLIB_AI_UNIFY_WHITE_SPACES, "BOTLIB_AI_UNIFY_WHITE_SPACES", 1, { VARG_VMPTR }
};
// BOTLIB_AI_REPLACE_SYNONYMS: wire args are VMA(1) + args[2]; the middle handler param
// is the literal VM_DATA_GUARD_SIZE (not a wire arg), so the descriptor is {VMPTR, INT}.
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_REPLACE_SYNONYMS = {
	BOTLIB_AI_REPLACE_SYNONYMS, "BOTLIB_AI_REPLACE_SYNONYMS", 2, { VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_LOAD_CHAT_FILE = {
	BOTLIB_AI_LOAD_CHAT_FILE, "BOTLIB_AI_LOAD_CHAT_FILE", 3, { VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_SET_CHAT_NAME = {
	BOTLIB_AI_SET_CHAT_NAME, "BOTLIB_AI_SET_CHAT_NAME", 3, { VARG_INT, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_PUSH_GOAL = {
	BOTLIB_AI_PUSH_GOAL, "BOTLIB_AI_PUSH_GOAL", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GOAL_NAME = {
	BOTLIB_AI_GOAL_NAME, "BOTLIB_AI_GOAL_NAME", 3, { VARG_INT, VARG_VMPTR_SIZED, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GET_TOP_GOAL = {
	BOTLIB_AI_GET_TOP_GOAL, "BOTLIB_AI_GET_TOP_GOAL", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GET_SECOND_GOAL = {
	BOTLIB_AI_GET_SECOND_GOAL, "BOTLIB_AI_GET_SECOND_GOAL", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_CHOOSE_LTG_ITEM = {
	BOTLIB_AI_CHOOSE_LTG_ITEM, "BOTLIB_AI_CHOOSE_LTG_ITEM", 4, { VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_CHOOSE_NBG_ITEM = {  // VMF(6)
	BOTLIB_AI_CHOOSE_NBG_ITEM, "BOTLIB_AI_CHOOSE_NBG_ITEM", 6, { VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_INT, VARG_VMPTR, VARG_FLOAT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_TOUCHING_GOAL = {
	BOTLIB_AI_TOUCHING_GOAL, "BOTLIB_AI_TOUCHING_GOAL", 2, { VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE = {
	BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE, "BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE", 4, { VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GET_LEVEL_ITEM_GOAL = {
	BOTLIB_AI_GET_LEVEL_ITEM_GOAL, "BOTLIB_AI_GET_LEVEL_ITEM_GOAL", 3, { VARG_INT, VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL = {
	BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL, "BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GET_MAP_LOCATION_GOAL = {
	BOTLIB_AI_GET_MAP_LOCATION_GOAL, "BOTLIB_AI_GET_MAP_LOCATION_GOAL", 2, { VARG_VMPTR, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_SET_AVOID_GOAL_TIME = {  // VMF(3)
	BOTLIB_AI_SET_AVOID_GOAL_TIME, "BOTLIB_AI_SET_AVOID_GOAL_TIME", 3, { VARG_INT, VARG_INT, VARG_FLOAT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_LOAD_ITEM_WEIGHTS = {
	BOTLIB_AI_LOAD_ITEM_WEIGHTS, "BOTLIB_AI_LOAD_ITEM_WEIGHTS", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC = {
	BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC, "BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC = {  // VMF(2)
	BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, "BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC", 2, { VARG_INT, VARG_FLOAT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_ADD_AVOID_SPOT = {  // VMF(3)
	BOTLIB_AI_ADD_AVOID_SPOT, "BOTLIB_AI_ADD_AVOID_SPOT", 4, { VARG_INT, VARG_VMPTR, VARG_FLOAT, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_MOVE_TO_GOAL = {
	BOTLIB_AI_MOVE_TO_GOAL, "BOTLIB_AI_MOVE_TO_GOAL", 4, { VARG_VMPTR, VARG_INT, VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_MOVE_IN_DIRECTION = {  // VMF(3)
	BOTLIB_AI_MOVE_IN_DIRECTION, "BOTLIB_AI_MOVE_IN_DIRECTION", 4, { VARG_INT, VARG_VMPTR, VARG_FLOAT, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_REACHABILITY_AREA = {
	BOTLIB_AI_REACHABILITY_AREA, "BOTLIB_AI_REACHABILITY_AREA", 2, { VARG_VMPTR, VARG_INT }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_MOVEMENT_VIEW_TARGET = {  // VMF(4)
	BOTLIB_AI_MOVEMENT_VIEW_TARGET, "BOTLIB_AI_MOVEMENT_VIEW_TARGET", 5, { VARG_INT, VARG_VMPTR, VARG_INT, VARG_FLOAT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_PREDICT_VISIBLE_POSITION = {
	BOTLIB_AI_PREDICT_VISIBLE_POSITION, "BOTLIB_AI_PREDICT_VISIBLE_POSITION", 5, { VARG_VMPTR, VARG_INT, VARG_VMPTR, VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_INIT_MOVE_STATE = {
	BOTLIB_AI_INIT_MOVE_STATE, "BOTLIB_AI_INIT_MOVE_STATE", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON = {
	BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON, "BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GET_WEAPON_INFO = {
	BOTLIB_AI_GET_WEAPON_INFO, "BOTLIB_AI_GET_WEAPON_INFO", 3, { VARG_INT, VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_LOAD_WEAPON_WEIGHTS = {
	BOTLIB_AI_LOAD_WEAPON_WEIGHTS, "BOTLIB_AI_LOAD_WEAPON_WEIGHTS", 2, { VARG_INT, VARG_VMPTR }
};
static const vmSyscallDesc_t sv_desc_BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION = {
	BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION, "BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION", 5,
	{ VARG_INT, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR, VARG_VMPTR }
};

// Adapter so VM_UnmarshalTyped can call the file-static VM_ArgPtr translator.
static void *SV_TypedArgPtr( intptr_t v ) {
	return VM_ArgPtr( v );
}

// Unmarshal a migrated game syscall into `out` via its descriptor; a validation
// failure (bad argc / out-of-range sized ptr) is a hard error — the module built
// the call wrong. The shipping path uses the typed values directly.
static void SV_UnmarshalGame( const vmSyscallDesc_t *desc, intptr_t *args, vmTypedArg_t *out ) {
	if ( !VM_UnmarshalTyped( gvm, desc, args, SV_TypedArgPtr, out ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s: bad typed syscall args", desc->name );
	}
}

// Compact per-case unmarshal for the high-volume botlib subsystem: declares the typed
// array `t`, unmarshals via the descriptor, and runs the (Debug-only) generic parity
// walk. Each migrated botlib case is `{ SV_BOTLIB(&sv_desc_X, N); <call with t[..]>; }`.
#define SV_BOTLIB( descp, argc ) \
	vmTypedArg_t t[ VM_MAX_TYPED_ARGS ]; \
	SV_UnmarshalGame( (descp), args, t ); \
	SV_TYPED_PARITY_WALK( (argc), args, t, (descp)->name )

#if defined(_DEBUG)
// Generic descriptor-driven parity for syscalls whose args are VARG_VMPTR (translated
// == VMA(n)) / VARG_INT (== args[n]) / VARG_FLOAT (bit-reinterpret == VMF(n)) /
// VARG_VMPTR_SIZED (ptr == VMA(n) + len == args[n+1]) — walks the typed args by their
// stored type; argc is passed explicitly. Used for the collision queries and the whole
// botlib subsystem (the VARG_FLOAT branch is botlib's skill/thinktime/speed proof).
static void SV_TypedParityWalk( int argc, intptr_t *args, const vmTypedArg_t *t, const char *name ) {
	int a;
	for ( a = 0; a < argc; a++ ) {
		const intptr_t raw = args[ a + 1 ];
		switch ( t[a].type ) {
		case VARG_INT:
			if ( t[a].i != raw )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (int arg %d)", name, a );
			break;
		case VARG_VMPTR:
			if ( t[a].p != (void *)VM_ArgPtr( raw ) )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (ptr arg %d)", name, a );
			break;
		case VARG_FLOAT: {
			floatint_t fi; fi.i = (int)raw;
			if ( t[a].f != fi.f )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (float arg %d)", name, a );
			break;
		}
		case VARG_VMPTR_SIZED:
			if ( t[a].p != (void *)VM_ArgPtr( raw ) || t[a].len != (unsigned)args[ a + 2 ] )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (sized arg %d)", name, a );
			break;
		case VARG_VMPTR_COUNTED:
			// No descriptor in the generic walk → verify the ptr translation; the byte-
			// length (.len == count*elemSize) parity is asserted per-syscall where elemSize
			// is known (SV_TypedParityCheck).
			if ( t[a].p != (void *)VM_ArgPtr( raw ) )
				Com_Terminate( TERM_CLIENT_DROP, "%s typed/opaque mismatch (counted arg %d)", name, a );
			break;
		default:
			break;
		}
	}
}

// Parity check (Debug only): assert the typed unmarshal reproduces the hand-written
// VMA(x)/cast for each migrated syscall, so a conversion bug is caught immediately.
// Compiles out entirely in Release — the shipping path is the typed unpack alone.
static void SV_TypedParityCheck( int call, intptr_t *args, const vmTypedArg_t *t ) {
	switch ( call ) {
	case G_LOG:
		if ( t[0].i != args[1] ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[2].p != (void *)VM_ArgPtr( args[3] ) )
			Com_Terminate( TERM_CLIENT_DROP, "G_LOG typed/opaque mismatch" );
		break;
	case G_PRINT:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) )
			Com_Terminate( TERM_CLIENT_DROP, "G_PRINT typed/opaque mismatch" );
		break;
	case G_ERROR:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) )
			Com_Terminate( TERM_CLIENT_DROP, "G_ERROR typed/opaque mismatch" );
		break;
	case G_CVAR_REGISTER:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||   // vmCvar* (may be NULL)
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||   // varName
		     t[2].p != (void *)VM_ArgPtr( args[3] ) ||   // default
		     t[3].i != args[4] )                          // flags
			Com_Terminate( TERM_CLIENT_DROP, "G_CVAR_REGISTER typed/opaque mismatch" );
		break;
	case G_CVAR_UPDATE:
	case G_CVAR_VARIABLE_INTEGER_VALUE:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) )
			Com_Terminate( TERM_CLIENT_DROP, "G_CVAR_* (1-ptr) typed/opaque mismatch" );
		break;
	case G_CVAR_SET:
	case G_CVAR_SETDESCRIPTION:
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) )
			Com_Terminate( TERM_CLIENT_DROP, "G_CVAR_* (2-ptr) typed/opaque mismatch" );
		break;
	case G_CVAR_VARIABLE_STRING_BUFFER:
		// var_name VMPTR, buffer VMPTR_SIZED (ptr+len), bufsize INT — the sized ptr
		// must produce the same translated buffer AND the same length the hand path
		// bounds-checked (args[3]).
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[1].len != (unsigned)args[3] ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "G_CVAR_VARIABLE_STRING_BUFFER typed/opaque mismatch" );
		break;
	case G_ARGV:
		// n INT, buffer VMPTR_SIZED (ptr+len), bufferLength INT.
		if ( t[0].i != args[1] ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[1].len != (unsigned)args[3] ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "G_ARGV typed/opaque mismatch" );
		break;
	case G_SEND_CONSOLE_COMMAND:
	case G_SEND_SERVER_COMMAND:
		// leading INT (exec_when / clientNum) + text VMPTR.
		if ( t[0].i != args[1] ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) )
			Com_Terminate( TERM_CLIENT_DROP, "G_SEND_*_COMMAND typed/opaque mismatch" );
		break;
	case G_FS_FOPEN_FILE:
		// qpath VMPTR, fileHandle_t* write-back VMPTR (may be NULL), mode INT.
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "G_FS_FOPEN_FILE typed/opaque mismatch" );
		break;
	case G_FS_READ:
	case G_FS_WRITE:
		// buffer VMPTR_SIZED (ptr+len at args[1]/args[2]), len INT, handle INT.
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[0].len != (unsigned)args[2] ||
		     t[1].i != args[2] ||
		     t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "G_FS_READ/WRITE typed/opaque mismatch" );
		break;
	case G_FS_FCLOSE_FILE:
		if ( t[0].i != args[1] )
			Com_Terminate( TERM_CLIENT_DROP, "G_FS_FCLOSE_FILE typed/opaque mismatch" );
		break;
	case G_FS_SEEK:
		if ( t[0].i != args[1] || t[1].i != args[2] || t[2].i != args[3] )
			Com_Terminate( TERM_CLIENT_DROP, "G_FS_SEEK typed/opaque mismatch" );
		break;
	case G_FS_RENAME:
		// from VMPTR, to VMPTR — same 2-string-ptr parity as G_CVAR_SET.
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) )
			Com_Terminate( TERM_CLIENT_DROP, "G_FS_RENAME typed/opaque mismatch" );
		break;
	case G_FS_GETFILELIST:
		// path VMPTR, ext VMPTR, listbuf VMPTR_SIZED (ptr+len at args[3]/args[4]), bufsize INT.
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[2].p != (void *)VM_ArgPtr( args[3] ) ||
		     t[2].len != (unsigned)args[4] ||
		     t[3].i != args[4] )
			Com_Terminate( TERM_CLIENT_DROP, "G_FS_GETFILELIST typed/opaque mismatch" );
		break;
	// collision / spatial queries: all args are VARG_VMPTR (vec3/struct pointers,
	// incl. the trace_t* write-back) or VARG_INT — verified by the generic walk
	// (argc per the descriptor; t[a].type drives each per-arg check).
	case G_TRACE:                  SV_TypedParityWalk( 7, args, t, "G_TRACE" ); break;
	case G_TRACECAPSULE:           SV_TypedParityWalk( 7, args, t, "G_TRACECAPSULE" ); break;
	case G_POINT_CONTENTS:         SV_TypedParityWalk( 2, args, t, "G_POINT_CONTENTS" ); break;
	case G_ENTITY_CONTACT:         SV_TypedParityWalk( 3, args, t, "G_ENTITY_CONTACT" ); break;
	case G_ENTITY_CONTACTCAPSULE:  SV_TypedParityWalk( 3, args, t, "G_ENTITY_CONTACTCAPSULE" ); break;
	case G_IN_PVS:                 SV_TypedParityWalk( 2, args, t, "G_IN_PVS" ); break;
	case G_IN_PVS_IGNORE_PORTALS:  SV_TypedParityWalk( 2, args, t, "G_IN_PVS_IGNORE_PORTALS" ); break;
	case G_AREAS_CONNECTED:        SV_TypedParityWalk( 2, args, t, "G_AREAS_CONNECTED" ); break;
	case G_SET_BRUSH_MODEL:        SV_TypedParityWalk( 2, args, t, "G_SET_BRUSH_MODEL" ); break;
	case G_ENTITIES_IN_BOX:
		// mins/maxs VMPTR, entityList VARG_VMPTR_COUNTED (int[maxcount]), maxcount INT.
		// Verify the counted ptr translates AND .len == maxcount*sizeof(int) (== the
		// hand-written VM_CheckBounds3(args[3], args[4], sizeof(int))).
		if ( t[0].p != (void *)VM_ArgPtr( args[1] ) ||
		     t[1].p != (void *)VM_ArgPtr( args[2] ) ||
		     t[2].p != (void *)VM_ArgPtr( args[3] ) ||
		     t[2].len != (unsigned)args[4] * sizeof( int ) ||
		     t[3].i != args[4] )
			Com_Terminate( TERM_CLIENT_DROP, "G_ENTITIES_IN_BOX typed/opaque mismatch" );
		break;
	default:
		break;
	}
}
#define SV_TYPED_PARITY( call, args, t )  SV_TypedParityCheck( (call), (args), (t) )
#define SV_TYPED_PARITY_WALK( argc, args, t, name )  SV_TypedParityWalk( (argc), (args), (t), (name) )
#else
#define SV_TYPED_PARITY( call, args, t )  ((void)0)
#define SV_TYPED_PARITY_WALK( argc, args, t, name )  ((void)0)
#endif


/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
static intptr_t SV_GameSystemCalls( intptr_t *args ) {

	// detect infinite loops in QVM code by counting syscalls per VM_Call invocation
	// the stock id 1.32 qagame.qvm has a bug in ClientSpawn() where a do/while(1) loop
	// retrying spawn point selection can loop forever if all spawn points have FL_NO_BOTS
	// set, causing the server to hang at 100% CPU
	if ( gvm->syscallCount >= 1024 * 1024 ) {
		Com_Terminate( TERM_CLIENT_DROP, "game VM syscall overflow - Loss of control in VM" );
	}
	++gvm->syscallCount;

	// ABI handshake (exact-match): a reserved-high id NOT in the G_* enum (so it
	// shifts no existing syscall) returns the engine's game ABI version, which the
	// module queries + exact-matches at init. See vm_typed_syscall.h.
	if ( args[0] == VM_SYSCALL_ABI_QUERY ) {
		return GAME_API_VERSION;
	}

	switch( args[0] ) {
	case G_PRINT: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_PRINT, args, t );
		SV_TYPED_PARITY( G_PRINT, args, t );
		Com_Log( SEV_INFO, LOG_CH(ch_game), "%s", (const char*)t[0].p );
		return 0;
	}
	case G_ERROR: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_ERROR, args, t );
		SV_TYPED_PARITY( G_ERROR, args, t );
		Com_Terminate( TERM_CLIENT_DROP, "%s", (const char*)t[0].p );
		return 0;
	}
	case G_LOG: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_LOG, args, t );
		SV_TYPED_PARITY( G_LOG, args, t );
		Com_Log( (log_severity_t)t[0].i, Log_GetChannel( (const char*)t[1].p ), "%s", (const char*)t[2].p );
		return 0;
	}
	case G_TERMINATE:
		Com_Terminate( (terminationReason_t)args[1], "%s", (const char*)VMA(2) );
		return 0;
	case G_MILLISECONDS:
		// 0-arg syscall: nothing to unmarshal/translate, so the typed path would be
		// a no-op — the hot timing path stays a bare call (W-41: this is the
		// highest-frequency syscall; no per-call overhead added).
		return Sys_Milliseconds();
	case G_CVAR_REGISTER: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_CVAR_REGISTER, args, t );
		SV_TYPED_PARITY( G_CVAR_REGISTER, args, t );
		Cvar_VM_Register( t[0].p, t[1].p, t[2].p, (int)t[3].i, gvm->privateFlag );
		return 0;
	}
	case G_CVAR_UPDATE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_CVAR_UPDATE, args, t );
		SV_TYPED_PARITY( G_CVAR_UPDATE, args, t );
		Cvar_Update( t[0].p, gvm->privateFlag );
		return 0;
	}
	case G_CVAR_SET: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_CVAR_SET, args, t );
		SV_TYPED_PARITY( G_CVAR_SET, args, t );
		Cvar_SetSafe( (const char *)t[0].p, (const char *)t[1].p );
		return 0;
	}
	case G_CVAR_VARIABLE_INTEGER_VALUE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_CVAR_VARIABLE_INTEGER_VALUE, args, t );
		SV_TYPED_PARITY( G_CVAR_VARIABLE_INTEGER_VALUE, args, t );
		return Cvar_VariableIntegerValue( (const char *)t[0].p );
	}
	case G_CVAR_VARIABLE_STRING_BUFFER: {
		// The VARG_VMPTR_SIZED unmarshal bounds-checks (buffer, bufsize) via
		// VM_CheckBounds — the same guard the hand-written VM_CHECKBOUNDS did.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_CVAR_VARIABLE_STRING_BUFFER, args, t );
		SV_TYPED_PARITY( G_CVAR_VARIABLE_STRING_BUFFER, args, t );
		Cvar_VariableStringBufferSafe( t[0].p, t[1].p, (int)t[2].i, gvm->privateFlag );
		return 0;
	}
	case G_ARGC:
		// 0-arg: nothing to unmarshal — bare call (W-41), like G_MILLISECONDS.
		return Cmd_Argc();
	case G_ARGV: {
		// VARG_VMPTR_SIZED bounds-checks (buffer, bufferLength) via VM_CheckBounds.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_ARGV, args, t );
		SV_TYPED_PARITY( G_ARGV, args, t );
		Cmd_ArgvBuffer( (int)t[0].i, t[1].p, (int)t[2].i );
		return 0;
	}
	case G_SEND_CONSOLE_COMMAND: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_SEND_CONSOLE_COMMAND, args, t );
		SV_TYPED_PARITY( G_SEND_CONSOLE_COMMAND, args, t );
		Cbuf_ExecuteTextSafe( (cbufExec_t)t[0].i, (const char *)t[1].p );
		return 0;
	}

	case G_FS_FOPEN_FILE: {
		// fileHandle_t* (arg 2) is written back by the engine — plain VMPTR.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_FS_FOPEN_FILE, args, t );
		SV_TYPED_PARITY( G_FS_FOPEN_FILE, args, t );
		return FS_VM_OpenFile( t[0].p, t[1].p, (fsMode_t)t[2].i, H_QAGAME );
	}
	case G_FS_READ:
		if ( args[3] == 0 ) // UrT may pass this with args[2]=-1 and cause false bounds check error
			return 0;       // MUST stay ahead of the unmarshal: VARG_VMPTR_SIZED would
			                // bounds-check args[1]/args[2] and the args[2]=-1 case would falsely terminate.
		{
			// VARG_VMPTR_SIZED bounds-checks (buffer, len) via VM_CheckBounds.
			vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
			SV_UnmarshalGame( &sv_desc_G_FS_READ, args, t );
			SV_TYPED_PARITY( G_FS_READ, args, t );
			return FS_VM_ReadFile( t[0].p, (int)t[1].i, (fileHandle_t)t[2].i, H_QAGAME );
		}
	case G_FS_WRITE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_FS_WRITE, args, t );
		SV_TYPED_PARITY( G_FS_WRITE, args, t );
		FS_VM_WriteFile( t[0].p, (int)t[1].i, (fileHandle_t)t[2].i, H_QAGAME );
		return 0;
	}
	case G_FS_FCLOSE_FILE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_FS_FCLOSE_FILE, args, t );
		SV_TYPED_PARITY( G_FS_FCLOSE_FILE, args, t );
		FS_VM_CloseFile( (fileHandle_t)t[0].i, H_QAGAME );
		return 0;
	}
	case G_FS_SEEK: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_FS_SEEK, args, t );
		SV_TYPED_PARITY( G_FS_SEEK, args, t );
		return FS_VM_SeekFile( (fileHandle_t)t[0].i, (long)t[1].i, (fsOrigin_t)t[2].i, H_QAGAME );
	}

	case G_FS_GETFILELIST: {
		// listbuf (arg 3) is VARG_VMPTR_SIZED, bounds-checked against bufsize (arg 4).
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_FS_GETFILELIST, args, t );
		SV_TYPED_PARITY( G_FS_GETFILELIST, args, t );
		return FS_GetFileList( t[0].p, t[1].p, t[2].p, (int)t[3].i );
	}
	case G_FS_RENAME: {
		// Two string args -> the engine FS_Rename (path-based, homepath-scoped, so
		// temp+final are same-dir = a true atomic rename). Called directly (no
		// FS_VM_* / H_QAGAME — that scoping is only for handle-based ops).
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_FS_RENAME, args, t );
		SV_TYPED_PARITY( G_FS_RENAME, args, t );
		FS_Rename( t[0].p, t[1].p );
		return 0;
	}

	case G_LOCATE_GAME_DATA:
		SV_LocateGameData( VMA(1), args[2], args[3], VMA(4), args[5] );
		return 0;
	case G_DROP_CLIENT:
		SV_GameDropClient( args[1], VMA(2) );
		return 0;
	case G_SEND_SERVER_COMMAND: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_SEND_SERVER_COMMAND, args, t );
		SV_TYPED_PARITY( G_SEND_SERVER_COMMAND, args, t );
		SV_GameSendServerCommand( (int)t[0].i, (const char *)t[1].p );
		return 0;
	}
	case G_LINKENTITY:
		SV_LinkEntity( VMA(1) );
		return 0;
	case G_UNLINKENTITY:
		SV_UnlinkEntity( VMA(1) );
		return 0;
	case G_ENTITIES_IN_BOX: {
		// entityList = VARG_VMPTR_COUNTED int[maxcount]; the VM_CheckBounds3 count×size
		// bounds is now inside VM_UnmarshalTyped (from the descriptor's countArg/elemSize).
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_ENTITIES_IN_BOX, args, t );
		SV_TYPED_PARITY( G_ENTITIES_IN_BOX, args, t );
		return SV_AreaEntities( t[0].p, t[1].p, t[2].p, (int)t[3].i );
	}
	case G_ENTITY_CONTACT: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_ENTITY_CONTACT, args, t );
		SV_TYPED_PARITY( G_ENTITY_CONTACT, args, t );
		return SV_EntityContact( t[0].p, t[1].p, t[2].p, /*int capsule*/ qfalse );
	}
	case G_ENTITY_CONTACTCAPSULE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_ENTITY_CONTACTCAPSULE, args, t );
		SV_TYPED_PARITY( G_ENTITY_CONTACTCAPSULE, args, t );
		return SV_EntityContact( t[0].p, t[1].p, t[2].p, /*int capsule*/ qtrue );
	}
	case G_TRACE: {
		// trace_t* (arg 1) is the engine's write-back result; vec3s are pointers.
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_TRACE, args, t );
		SV_TYPED_PARITY( G_TRACE, args, t );
		SV_Trace( t[0].p, t[1].p, t[2].p, t[3].p, t[4].p, (int)t[5].i, (int)t[6].i, /*capsule*/ qfalse );
		return 0;
	}
	case G_TRACECAPSULE: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_TRACECAPSULE, args, t );
		SV_TYPED_PARITY( G_TRACECAPSULE, args, t );
		SV_Trace( t[0].p, t[1].p, t[2].p, t[3].p, t[4].p, (int)t[5].i, (int)t[6].i, /*capsule*/ qtrue );
		return 0;
	}
	case G_POINT_CONTENTS: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_POINT_CONTENTS, args, t );
		SV_TYPED_PARITY( G_POINT_CONTENTS, args, t );
		return SV_PointContents( t[0].p, (int)t[1].i );
	}
	case G_SET_BRUSH_MODEL: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_SET_BRUSH_MODEL, args, t );
		SV_TYPED_PARITY( G_SET_BRUSH_MODEL, args, t );
		SV_SetBrushModel( t[0].p, (const char *)t[1].p );
		return 0;
	}
	case G_IN_PVS: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_IN_PVS, args, t );
		SV_TYPED_PARITY( G_IN_PVS, args, t );
		return SV_inPVS( t[0].p, t[1].p );
	}
	case G_IN_PVS_IGNORE_PORTALS: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_IN_PVS_IGNORE_PORTALS, args, t );
		SV_TYPED_PARITY( G_IN_PVS_IGNORE_PORTALS, args, t );
		return SV_inPVSIgnorePortals( t[0].p, t[1].p );
	}

	case G_SET_CONFIGSTRING:
		SV_SetConfigstring( args[1], VMA(2) );
		return 0;
	case G_GET_CONFIGSTRING:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		SV_GetConfigstring( args[1], VMA(2), args[3] );
		return 0;
	case G_SET_USERINFO:
		SV_SetUserinfo( args[1], VMA(2) );
		return 0;
	case G_GET_USERINFO:
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		SV_GetUserinfo( args[1], VMA(2), args[3] );
		return 0;
	case G_GET_SERVERINFO:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		SV_GetServerinfo( VMA(1), args[2] );
		return 0;
	case G_ADJUST_AREA_PORTAL_STATE:
		SV_AdjustAreaPortalState( VMA(1), args[2] );
		return 0;
	case G_AREAS_CONNECTED: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_AREAS_CONNECTED, args, t );
		SV_TYPED_PARITY( G_AREAS_CONNECTED, args, t );
		return CM_AreasConnected( (int)t[0].i, (int)t[1].i );
	}

	case G_BOT_ALLOCATE_CLIENT:
		return SV_BotAllocateClient();
	case G_BOT_FREE_CLIENT:
		SV_BotFreeClient( args[1] );
		return 0;

	case G_GET_USERCMD:
		SV_GetUsercmd( args[1], VMA(2) );
		return 0;
	case G_GET_ENTITY_TOKEN:
		{
			ComParser parser = { 0 };
			char *s = (char*)COM_Parse( &parser, &sv.entityParsePoint );
			VM_CHECKBOUNDS( gvm, args[1], args[2] );
			//Q_strncpyz( VMA(1), s, args[2] );
			// we can't use our optimized Q_strncpyz() function
			// because of uninitialized memory bug in defrag mod
			{
				char *dst = (char*)VMA(1);
				const int size = args[2]-1;
				if ( size >= 0 ) {
					Q_strncpy( dst, s, size );
					dst[size] = '\0';
				}
			}
			if ( !sv.entityParsePoint && s[0] == '\0' ) {
				return qfalse;
			}
			return qtrue;
		}

	case G_DEBUG_POLYGON_CREATE:
		return BotImport_DebugPolygonCreate( args[1], args[2], VMA(3) );
	case G_DEBUG_POLYGON_DELETE:
		BotImport_DebugPolygonDelete( args[1] );
		return 0;
	case G_REAL_TIME:
		return Com_RealTime( VMA(1) );
	case G_SNAPVECTOR:
		Sys_SnapVector( VMA(1) );
		return 0;

		//====================================

	case BOTLIB_SETUP:
		return SV_BotLibSetup();
	case BOTLIB_SHUTDOWN:
		return SV_BotLibShutdown();
	case BOTLIB_LIBVAR_SET: {
		SV_BOTLIB( &sv_desc_BOTLIB_LIBVAR_SET, 2 );
		return botlib_export->BotLibVarSet( t[0].p, t[1].p );
	}
	case BOTLIB_LIBVAR_GET: {
		// VARG_VMPTR_SIZED (arg 2) bounds-checks (buffer, args[3]) — the VM_CHECKBOUNDS
		// the hand path did is now inside the unmarshal.
		SV_BOTLIB( &sv_desc_BOTLIB_LIBVAR_GET, 3 );
		return botlib_export->BotLibVarGet( t[0].p, t[1].p, (int)t[2].i );
	}

	case BOTLIB_PC_ADD_GLOBAL_DEFINE: {
		SV_BOTLIB( &sv_desc_BOTLIB_PC_ADD_GLOBAL_DEFINE, 1 );
		return botlib_export->PC_AddGlobalDefine( t[0].p );
	}
	case BOTLIB_PC_LOAD_SOURCE: {
		SV_BOTLIB( &sv_desc_BOTLIB_PC_LOAD_SOURCE, 1 );
		return botlib_export->PC_LoadSourceHandle( t[0].p );
	}
	case BOTLIB_PC_FREE_SOURCE:
		return botlib_export->PC_FreeSourceHandle( args[1] );
	case BOTLIB_PC_READ_TOKEN:
		// args[2] sized by sizeof(pc_token_t) (a fixed sizeof, not a wire arg) → the
		// VARG_VMPTR_SIZED's len-from-next-arg model doesn't fit; kept hand-written.
		VM_CHECKBOUNDS( gvm, args[2], sizeof( pc_token_t ) );
		return botlib_export->PC_ReadTokenHandle( args[1], VMA(2) );
	case BOTLIB_PC_SOURCE_FILE_AND_LINE: {
		SV_BOTLIB( &sv_desc_BOTLIB_PC_SOURCE_FILE_AND_LINE, 3 );
		return botlib_export->PC_SourceFileAndLine( (int)t[0].i, t[1].p, t[2].p );
	}

	case BOTLIB_START_FRAME: {     // VMF(1) — first VARG_FLOAT use
		SV_BOTLIB( &sv_desc_BOTLIB_START_FRAME, 1 );
		return botlib_export->BotLibStartFrame( t[0].f );
	}
	case BOTLIB_LOAD_MAP: {
		SV_BOTLIB( &sv_desc_BOTLIB_LOAD_MAP, 1 );
		return botlib_export->BotLibLoadMap( t[0].p );
	}
	case BOTLIB_UPDATENTITY: {
		SV_BOTLIB( &sv_desc_BOTLIB_UPDATENTITY, 2 );
		return botlib_export->BotLibUpdateEntity( (int)t[0].i, t[1].p );
	}
	case BOTLIB_TEST: {
		SV_BOTLIB( &sv_desc_BOTLIB_TEST, 4 );
		return botlib_export->Test( (int)t[0].i, t[1].p, t[2].p, t[3].p );
	}

	case BOTLIB_GET_SNAPSHOT_ENTITY:
		return SV_BotGetSnapshotEntity( args[1], args[2] );
	case BOTLIB_GET_CONSOLE_MESSAGE: {
		SV_BOTLIB( &sv_desc_BOTLIB_GET_CONSOLE_MESSAGE, 3 );
		return SV_BotGetConsoleMessage( (int)t[0].i, t[1].p, (int)t[2].i );
	}
	case BOTLIB_USER_COMMAND:
		{
			unsigned clientNum = args[1];
			if ( clientNum < sv.maxclients )
			{
				SV_ClientThink( &svs.clients[ clientNum ], VMA(2) );
			}
		}
		return 0;

	case BOTLIB_EA_SAY: {
		SV_BOTLIB( &sv_desc_BOTLIB_EA_SAY, 2 );
		botlib_export->ea.EA_Say( (int)t[0].i, t[1].p );
		return 0;
	}
	case BOTLIB_EA_SAY_TEAM: {
		SV_BOTLIB( &sv_desc_BOTLIB_EA_SAY_TEAM, 2 );
		botlib_export->ea.EA_SayTeam( (int)t[0].i, t[1].p );
		return 0;
	}
	case BOTLIB_EA_COMMAND: {
		SV_BOTLIB( &sv_desc_BOTLIB_EA_COMMAND, 2 );
		botlib_export->ea.EA_Command( (int)t[0].i, t[1].p );
		return 0;
	}

	case BOTLIB_EA_ACTION:
		botlib_export->ea.EA_Action( args[1], args[2] );
		return 0;
	case BOTLIB_EA_GESTURE:
		botlib_export->ea.EA_Gesture( args[1] );
		return 0;
	case BOTLIB_EA_TALK:
		botlib_export->ea.EA_Talk( args[1] );
		return 0;
	case BOTLIB_EA_ATTACK:
		botlib_export->ea.EA_Attack( args[1] );
		return 0;
	case BOTLIB_EA_USE:
		botlib_export->ea.EA_Use( args[1] );
		return 0;
	case BOTLIB_EA_RESPAWN:
		botlib_export->ea.EA_Respawn( args[1] );
		return 0;
	case BOTLIB_EA_CROUCH:
		botlib_export->ea.EA_Crouch( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_UP:
		botlib_export->ea.EA_MoveUp( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_DOWN:
		botlib_export->ea.EA_MoveDown( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_FORWARD:
		botlib_export->ea.EA_MoveForward( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_BACK:
		botlib_export->ea.EA_MoveBack( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_LEFT:
		botlib_export->ea.EA_MoveLeft( args[1] );
		return 0;
	case BOTLIB_EA_MOVE_RIGHT:
		botlib_export->ea.EA_MoveRight( args[1] );
		return 0;

	case BOTLIB_EA_SELECT_WEAPON:
		botlib_export->ea.EA_SelectWeapon( args[1], args[2] );
		return 0;
	case BOTLIB_EA_JUMP:
		botlib_export->ea.EA_Jump( args[1] );
		return 0;
	case BOTLIB_EA_DELAYED_JUMP:
		botlib_export->ea.EA_DelayedJump( args[1] );
		return 0;
	case BOTLIB_EA_MOVE: {          // VMF(3) speed — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_EA_MOVE, 3 );
		botlib_export->ea.EA_Move( (int)t[0].i, t[1].p, t[2].f );
		return 0;
	}
	case BOTLIB_EA_VIEW: {
		SV_BOTLIB( &sv_desc_BOTLIB_EA_VIEW, 2 );
		botlib_export->ea.EA_View( (int)t[0].i, t[1].p );
		return 0;
	}

	case BOTLIB_EA_END_REGULAR: {   // VMF(2) thinktime — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_EA_END_REGULAR, 2 );
		botlib_export->ea.EA_EndRegular( (int)t[0].i, t[1].f );
		return 0;
	}
	case BOTLIB_EA_GET_INPUT: {     // VMF(2) thinktime — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_EA_GET_INPUT, 3 );
		botlib_export->ea.EA_GetInput( (int)t[0].i, t[1].f, t[2].p );
		return 0;
	}
	case BOTLIB_EA_RESET_INPUT:
		botlib_export->ea.EA_ResetInput( args[1] );
		return 0;

	case BOTLIB_AI_LOAD_CHARACTER: {   // VMF(2) skill — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_AI_LOAD_CHARACTER, 2 );
		return botlib_export->ai.BotLoadCharacter( t[0].p, t[1].f );
	}
	case BOTLIB_AI_FREE_CHARACTER:
		botlib_export->ai.BotFreeCharacter( args[1] );
		return 0;
	case BOTLIB_AI_CHARACTERISTIC_FLOAT:
		return FloatAsInt( botlib_export->ai.Characteristic_Float( args[1], args[2] ) );
	case BOTLIB_AI_CHARACTERISTIC_BFLOAT: {  // VMF(3),VMF(4) — VARG_FLOAT; FloatAsInt return untouched
		SV_BOTLIB( &sv_desc_BOTLIB_AI_CHARACTERISTIC_BFLOAT, 4 );
		return FloatAsInt( botlib_export->ai.Characteristic_BFloat( (int)t[0].i, (int)t[1].i, t[2].f, t[3].f ) );
	}
	case BOTLIB_AI_CHARACTERISTIC_INTEGER:
		return botlib_export->ai.Characteristic_Integer( args[1], args[2] );
	case BOTLIB_AI_CHARACTERISTIC_BINTEGER:
		return botlib_export->ai.Characteristic_BInteger( args[1], args[2], args[3], args[4] );
	case BOTLIB_AI_CHARACTERISTIC_STRING: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_CHARACTERISTIC_STRING, 4 );
		botlib_export->ai.Characteristic_String( (int)t[0].i, (int)t[1].i, t[2].p, (int)t[3].i );
		return 0;
	}

	case BOTLIB_AI_ALLOC_CHAT_STATE:
		return botlib_export->ai.BotAllocChatState();
	case BOTLIB_AI_FREE_CHAT_STATE:
		botlib_export->ai.BotFreeChatState( args[1] );
		return 0;
	case BOTLIB_AI_QUEUE_CONSOLE_MESSAGE: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, 3 );
		botlib_export->ai.BotQueueConsoleMessage( (int)t[0].i, (int)t[1].i, t[2].p );
		return 0;
	}
	case BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
		botlib_export->ai.BotRemoveConsoleMessage( args[1], args[2] );
		return 0;
	case BOTLIB_AI_NEXT_CONSOLE_MESSAGE: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_NEXT_CONSOLE_MESSAGE, 2 );
		return botlib_export->ai.BotNextConsoleMessage( (int)t[0].i, t[1].p );
	}
	case BOTLIB_AI_NUM_CONSOLE_MESSAGE:
		return botlib_export->ai.BotNumConsoleMessages( args[1] );
	case BOTLIB_AI_INITIAL_CHAT: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_INITIAL_CHAT, 11 );
		botlib_export->ai.BotInitialChat( (int)t[0].i, t[1].p, (int)t[2].i, t[3].p, t[4].p, t[5].p, t[6].p, t[7].p, t[8].p, t[9].p, t[10].p );
		return 0;
	}
	case BOTLIB_AI_NUM_INITIAL_CHATS: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_NUM_INITIAL_CHATS, 2 );
		return botlib_export->ai.BotNumInitialChats( (int)t[0].i, t[1].p );
	}
	case BOTLIB_AI_REPLY_CHAT: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_REPLY_CHAT, 12 );
		return botlib_export->ai.BotReplyChat( (int)t[0].i, t[1].p, (int)t[2].i, (int)t[3].i, t[4].p, t[5].p, t[6].p, t[7].p, t[8].p, t[9].p, t[10].p, t[11].p );
	}
	case BOTLIB_AI_CHAT_LENGTH:
		return botlib_export->ai.BotChatLength( args[1] );
	case BOTLIB_AI_ENTER_CHAT:
		botlib_export->ai.BotEnterChat( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_GET_CHAT_MESSAGE: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GET_CHAT_MESSAGE, 3 );
		botlib_export->ai.BotGetChatMessage( (int)t[0].i, t[1].p, (int)t[2].i );
		return 0;
	}
	case BOTLIB_AI_STRING_CONTAINS: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_STRING_CONTAINS, 3 );
		return botlib_export->ai.StringContains( t[0].p, t[1].p, (int)t[2].i );
	}
	case BOTLIB_AI_FIND_MATCH: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_FIND_MATCH, 3 );
		return botlib_export->ai.BotFindMatch( t[0].p, t[1].p, (int)t[2].i );
	}
	case BOTLIB_AI_MATCH_VARIABLE: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_MATCH_VARIABLE, 4 );
		botlib_export->ai.BotMatchVariable( t[0].p, (int)t[1].i, t[2].p, (int)t[3].i );
		return 0;
	}
	case BOTLIB_AI_UNIFY_WHITE_SPACES: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_UNIFY_WHITE_SPACES, 1 );
		botlib_export->ai.UnifyWhiteSpaces( t[0].p );
		return 0;
	}
	case BOTLIB_AI_REPLACE_SYNONYMS: {
		// middle param is the literal VM_DATA_GUARD_SIZE (not a wire arg); descriptor
		// is {VMPTR(1), INT(2)} matching the two wire args.
		SV_BOTLIB( &sv_desc_BOTLIB_AI_REPLACE_SYNONYMS, 2 );
		botlib_export->ai.BotReplaceSynonyms( t[0].p, VM_DATA_GUARD_SIZE, (int)t[1].i );
		return 0;
	}
	case BOTLIB_AI_LOAD_CHAT_FILE: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_LOAD_CHAT_FILE, 3 );
		return botlib_export->ai.BotLoadChatFile( (int)t[0].i, t[1].p, t[2].p );
	}
	case BOTLIB_AI_SET_CHAT_GENDER:
		botlib_export->ai.BotSetChatGender( args[1], args[2] );
		return 0;
	case BOTLIB_AI_SET_CHAT_NAME: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_SET_CHAT_NAME, 3 );
		botlib_export->ai.BotSetChatName( (int)t[0].i, t[1].p, (int)t[2].i );
		return 0;
	}

	case BOTLIB_AI_RESET_GOAL_STATE:
		botlib_export->ai.BotResetGoalState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_AVOID_GOALS:
		botlib_export->ai.BotResetAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
		botlib_export->ai.BotRemoveFromAvoidGoals( args[1], args[2] );
		return 0;
	case BOTLIB_AI_PUSH_GOAL: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_PUSH_GOAL, 2 );
		botlib_export->ai.BotPushGoal( (int)t[0].i, t[1].p );
		return 0;
	}
	case BOTLIB_AI_POP_GOAL:
		botlib_export->ai.BotPopGoal( args[1] );
		return 0;
	case BOTLIB_AI_EMPTY_GOAL_STACK:
		botlib_export->ai.BotEmptyGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_AVOID_GOALS:
		botlib_export->ai.BotDumpAvoidGoals( args[1] );
		return 0;
	case BOTLIB_AI_DUMP_GOAL_STACK:
		botlib_export->ai.BotDumpGoalStack( args[1] );
		return 0;
	case BOTLIB_AI_GOAL_NAME: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GOAL_NAME, 3 );
		botlib_export->ai.BotGoalName( (int)t[0].i, t[1].p, (int)t[2].i );
		return 0;
	}
	case BOTLIB_AI_GET_TOP_GOAL: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GET_TOP_GOAL, 2 );
		return botlib_export->ai.BotGetTopGoal( (int)t[0].i, t[1].p );
	}
	case BOTLIB_AI_GET_SECOND_GOAL: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GET_SECOND_GOAL, 2 );
		return botlib_export->ai.BotGetSecondGoal( (int)t[0].i, t[1].p );
	}
	case BOTLIB_AI_CHOOSE_LTG_ITEM: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_CHOOSE_LTG_ITEM, 4 );
		return botlib_export->ai.BotChooseLTGItem( (int)t[0].i, t[1].p, t[2].p, (int)t[3].i );
	}
	case BOTLIB_AI_CHOOSE_NBG_ITEM: {   // VMF(6) — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_AI_CHOOSE_NBG_ITEM, 6 );
		return botlib_export->ai.BotChooseNBGItem( (int)t[0].i, t[1].p, t[2].p, (int)t[3].i, t[4].p, t[5].f );
	}
	case BOTLIB_AI_TOUCHING_GOAL: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_TOUCHING_GOAL, 2 );
		return botlib_export->ai.BotTouchingGoal( t[0].p, t[1].p );
	}
	case BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE, 4 );
		return botlib_export->ai.BotItemGoalInVisButNotVisible( (int)t[0].i, t[1].p, t[2].p, t[3].p );
	}
	case BOTLIB_AI_GET_LEVEL_ITEM_GOAL: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GET_LEVEL_ITEM_GOAL, 3 );
		return botlib_export->ai.BotGetLevelItemGoal( (int)t[0].i, t[1].p, t[2].p );
	}
	case BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL, 2 );
		return botlib_export->ai.BotGetNextCampSpotGoal( (int)t[0].i, t[1].p );
	}
	case BOTLIB_AI_GET_MAP_LOCATION_GOAL: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GET_MAP_LOCATION_GOAL, 2 );
		return botlib_export->ai.BotGetMapLocationGoal( t[0].p, t[1].p );
	}
	case BOTLIB_AI_AVOID_GOAL_TIME:
		return FloatAsInt( botlib_export->ai.BotAvoidGoalTime( args[1], args[2] ) );
	case BOTLIB_AI_SET_AVOID_GOAL_TIME: {   // VMF(3) — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_AI_SET_AVOID_GOAL_TIME, 3 );
		botlib_export->ai.BotSetAvoidGoalTime( (int)t[0].i, (int)t[1].i, t[2].f );
		return 0;
	}
	case BOTLIB_AI_INIT_LEVEL_ITEMS:
		botlib_export->ai.BotInitLevelItems();
		return 0;
	case BOTLIB_AI_UPDATE_ENTITY_ITEMS:
		botlib_export->ai.BotUpdateEntityItems();
		return 0;
	case BOTLIB_AI_LOAD_ITEM_WEIGHTS: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_LOAD_ITEM_WEIGHTS, 2 );
		return botlib_export->ai.BotLoadItemWeights( (int)t[0].i, t[1].p );
	}
	case BOTLIB_AI_FREE_ITEM_WEIGHTS:
		botlib_export->ai.BotFreeItemWeights( args[1] );
		return 0;
	case BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
		botlib_export->ai.BotInterbreedGoalFuzzyLogic( args[1], args[2], args[3] );
		return 0;
	case BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC, 2 );
		botlib_export->ai.BotSaveGoalFuzzyLogic( (int)t[0].i, t[1].p );
		return 0;
	}
	case BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC: {   // VMF(2) — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, 2 );
		botlib_export->ai.BotMutateGoalFuzzyLogic( (int)t[0].i, t[1].f );
		return 0;
	}
	case BOTLIB_AI_ALLOC_GOAL_STATE:
		return botlib_export->ai.BotAllocGoalState( args[1] );
	case BOTLIB_AI_FREE_GOAL_STATE:
		botlib_export->ai.BotFreeGoalState( args[1] );
		return 0;

	case BOTLIB_AI_RESET_MOVE_STATE:
		botlib_export->ai.BotResetMoveState( args[1] );
		return 0;
	case BOTLIB_AI_ADD_AVOID_SPOT: {   // VMF(3) — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_AI_ADD_AVOID_SPOT, 4 );
		botlib_export->ai.BotAddAvoidSpot( (int)t[0].i, t[1].p, t[2].f, (int)t[3].i );
		return 0;
	}
	case BOTLIB_AI_MOVE_TO_GOAL: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_MOVE_TO_GOAL, 4 );
		botlib_export->ai.BotMoveToGoal( t[0].p, (int)t[1].i, t[2].p, (int)t[3].i );
		return 0;
	}
	case BOTLIB_AI_MOVE_IN_DIRECTION: {   // VMF(3) — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_AI_MOVE_IN_DIRECTION, 4 );
		return botlib_export->ai.BotMoveInDirection( (int)t[0].i, t[1].p, t[2].f, (int)t[3].i );
	}
	case BOTLIB_AI_RESET_AVOID_REACH:
		botlib_export->ai.BotResetAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_RESET_LAST_AVOID_REACH:
		botlib_export->ai.BotResetLastAvoidReach( args[1] );
		return 0;
	case BOTLIB_AI_REACHABILITY_AREA: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_REACHABILITY_AREA, 2 );
		return botlib_export->ai.BotReachabilityArea( t[0].p, (int)t[1].i );
	}
	case BOTLIB_AI_MOVEMENT_VIEW_TARGET: {   // VMF(4) — VARG_FLOAT
		SV_BOTLIB( &sv_desc_BOTLIB_AI_MOVEMENT_VIEW_TARGET, 5 );
		return botlib_export->ai.BotMovementViewTarget( (int)t[0].i, t[1].p, (int)t[2].i, t[3].f, t[4].p );
	}
	case BOTLIB_AI_PREDICT_VISIBLE_POSITION: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_PREDICT_VISIBLE_POSITION, 5 );
		return botlib_export->ai.BotPredictVisiblePosition( t[0].p, (int)t[1].i, t[2].p, (int)t[3].i, t[4].p );
	}
	case BOTLIB_AI_ALLOC_MOVE_STATE:
		return botlib_export->ai.BotAllocMoveState();
	case BOTLIB_AI_FREE_MOVE_STATE:
		botlib_export->ai.BotFreeMoveState( args[1] );
		return 0;
	case BOTLIB_AI_INIT_MOVE_STATE: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_INIT_MOVE_STATE, 2 );
		botlib_export->ai.BotInitMoveState( (int)t[0].i, t[1].p );
		return 0;
	}

	case BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON, 2 );
		return botlib_export->ai.BotChooseBestFightWeapon( (int)t[0].i, t[1].p );
	}
	case BOTLIB_AI_GET_WEAPON_INFO: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GET_WEAPON_INFO, 3 );
		botlib_export->ai.BotGetWeaponInfo( (int)t[0].i, (int)t[1].i, t[2].p );
		return 0;
	}
	case BOTLIB_AI_LOAD_WEAPON_WEIGHTS: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_LOAD_WEAPON_WEIGHTS, 2 );
		return botlib_export->ai.BotLoadWeaponWeights( (int)t[0].i, t[1].p );
	}
	case BOTLIB_AI_ALLOC_WEAPON_STATE:
		return botlib_export->ai.BotAllocWeaponState();
	case BOTLIB_AI_FREE_WEAPON_STATE:
		botlib_export->ai.BotFreeWeaponState( args[1] );
		return 0;
	case BOTLIB_AI_RESET_WEAPON_STATE:
		botlib_export->ai.BotResetWeaponState( args[1] );
		return 0;

	case BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION: {
		SV_BOTLIB( &sv_desc_BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION, 5 );
		return botlib_export->ai.GeneticParentsAndChildSelection( (int)t[0].i, t[1].p, t[2].p, t[3].p, t[4].p );
	}

	case WI_LOAD_CHARACTER:
		return SV_Lua_LoadCharacter( VMA(1), VMF(2) );
	case WI_FREE_CHARACTER:
		SV_Lua_FreeCharacter( args[1] );
		return 0;
	case WI_CHARACTERISTIC_FLOAT:
		return FloatAsInt( SV_Lua_CharacteristicBFloat( args[1], args[2], 0.0f, 1.0f ) );
	case WI_CHARACTERISTIC_BFLOAT:
		return FloatAsInt( SV_Lua_CharacteristicBFloat( args[1], args[2], VMF(3), VMF(4) ) );
	case WI_CHARACTERISTIC_INTEGER:
		return (int)SV_Lua_CharacteristicBFloat( args[1], args[2], 0.0f, 1.0f );
	case WI_CHARACTERISTIC_BINTEGER:
		return (int)SV_Lua_CharacteristicBFloat( args[1], args[2], (float)args[3], (float)args[4] );
	case WI_CHARACTERISTIC_STRING:
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		SV_Lua_CharacteristicString( args[1], args[2], VMA(3), args[4] );
		return 0;
	case WI_BIND_BOT:
		return SV_Lua_BindBot( args[1], args[2] );
	case WI_BOT_THINK:
		return SV_Lua_BotThink( args[1], VMF(2) );
	case WI_BOT_PROFILE_FIELD:
		return FloatAsInt( SV_Lua_BotProfileField( args[1], args[2] ) );
	case WI_BOT_PICK_WEAPON:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( wbCombatCtx_t ) );
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		return SV_Lua_BotPickWeapon( args[1], VMA(2), VMA(3), args[4] );
	case WI_BOT_GET_ATTACK_AIM_HEIGHT:
		return FloatAsInt( SV_Lua_BotGetAttackAimHeight( args[1], args[2] ) );
	case WI_BOT_EVAL_ITEM:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( wbItemEvalCtx_t ) );
		return SV_Lua_BotEvalItem( args[1], VMA(2) );
	case WI_BOT_DECIDE:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( wbDecideCtx_t ) );
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		return SV_Lua_BotDecide( args[1], VMA(2), VMA(3), args[4] );
	case WI_BOT_ON_CHAT:
		VM_CHECKBOUNDS( gvm, args[3], sizeof( wbChatCtx_t ) );
		VM_CHECKBOUNDS( gvm, args[4], args[5] );
		return SV_Lua_BotOnChat( args[1], VMA(2), VMA(3), VMA(4), args[5] );

	// shared syscalls

	case TRAP_MEMSET:
		VM_CHECKBOUNDS( gvm, args[1], args[3] );
		memset( VMA(1), args[2], args[3] );
		return args[1];

	case TRAP_MEMCPY:
		VM_CHECKBOUNDS2( gvm, args[1], args[2], args[3] );
		memcpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_STRNCPY:
		VM_CHECKBOUNDS( gvm, args[1], args[3] );
		Q_strncpy( VMA(1), VMA(2), args[3] );
		return args[1];

	case TRAP_SIN:
		return FloatAsInt( sin( VMF(1) ) );

	case TRAP_COS:
		return FloatAsInt( cos( VMF(1) ) );

	case TRAP_ATAN2:
		return FloatAsInt( atan2( VMF(1), VMF(2) ) );

	case TRAP_SQRT:
		return FloatAsInt( sqrt( VMF(1) ) );

	case G_MATRIXMULTIPLY:
		MatrixMultiply( VMA(1), VMA(2), VMA(3) );
		return 0;

	case G_ANGLEVECTORS:
		AngleVectors( VMA(1), VMA(2), VMA(3), VMA(4) );
		return 0;

	case G_PERPENDICULARVECTOR:
		PerpendicularVector( VMA(1), VMA(2) );
		return 0;

	case G_FLOOR:
		return FloatAsInt( floor( VMF(1) ) );

	case G_CEIL:
		return FloatAsInt( ceil( VMF(1) ) );

	case G_TESTPRINTINT:
		return sprintf( VMA(1), "%i", (int)args[2] );

	case G_TESTPRINTFLOAT:
		return sprintf( VMA(1), "%f", VMF(2) );

	case G_CVAR_SETDESCRIPTION: {
		vmTypedArg_t t[ VM_MAX_TYPED_ARGS ];
		SV_UnmarshalGame( &sv_desc_G_CVAR_SETDESCRIPTION, args, t );
		SV_TYPED_PARITY( G_CVAR_SETDESCRIPTION, args, t );
		Cvar_SetDescription2( (const char*)t[0].p, (const char*)t[1].p );
		return 0;
	}

	case G_TRAP_GETVALUE:
		VM_CHECKBOUNDS( gvm, args[1], args[2] );
		return SV_GetValue( VMA(1), args[2], VMA(3) );

#if FEAT_WIREDNET_OBSERVER
	// ── QUIC event emission syscalls ────────────────────────────
	// Game code (g_combat.c, g_items.c, g_cmds.c) calls these via
	// trap_WN_Emit*() which triggers a VM syscall to this handler.
	case G_WIREDNET_EMIT_KILL:
		WN_EmitKill( args[1], args[2], args[3], VMA(4), VMA(5) );
		return 0;
	case G_WIREDNET_EMIT_DAMAGE:
		WN_EmitDamage( args[1], args[2], args[3], args[4], VMA(5), VMA(6) );
		return 0;
	case G_WIREDNET_EMIT_ITEM_PICKUP:
		WN_EmitItemPickup( args[1], VMA(2), VMA(3) );
		return 0;
	case G_WIREDNET_EMIT_CHAT:
		WN_EmitChat( args[1], VMA(2), args[3] );
		return 0;
	case G_WIREDNET_EMIT_MATCH_EVENT:
		WN_EmitMatchEvent( VMA(1), VMA(2) );
		return 0;
	case G_WIREDNET_EMIT_DELAG:
		// delag event: shooter, target, timeDelta, shooterPos, targetPos
		// Currently a no-op on the server side — the event is logged
		// but no dedicated QUIC handler exists yet. The trap exists so
		// game code can emit the event when one is wired up.
		return 0;
	case G_WIREDNET_EMIT_BOT_EVENT:
		WN_EmitBotEvent( args[1], VMA(2), args[3], args[4], VMA(5) );
		return 0;
#endif

	// ── WiredCoreEvents generic emit ─────────────────────────────────
	case G_WCE_EMIT_EVENT: {
		wce_event_data_t ev;
		const char *text;
		memset( &ev, 0, sizeof(ev) );
		ev.type      = (wce_event_type_t)args[1];
		ev.clientNum = (int)args[2];
		ev.entityNum = (int)args[3];
		VectorCopy( (const float *)VMA(4), ev.origin );
		ev.param1    = (int)args[5];
		ev.param2    = (int)args[6];
		ev.fparam    = VMF(7);
		text = VMA(8);
		if ( text ) Q_strncpyz( ev.text, text, sizeof(ev.text) );
		WiredCoreEvents_Dispatch( &ev );
		return 0;
	}

	case G_WCE_GET_SOUND_EVENTS:
		// Bound the write extent against the VM data window: the game module
		// supplies both the destination pointer (args[2]) and the element count
		// (args[3]), and SV_BotAwareness_GetEvents writes up to maxOut
		// bot_sound_event_t records — VMA() only masks the base, not the extent.
		VM_CHECKBOUNDS3( gvm, args[2], (unsigned)args[3], sizeof( bot_sound_event_t ) );
		return SV_BotAwareness_GetEvents( (int)args[1], VMA(2), (int)args[3] );

	// ── Monster-Lua behavior traps (parallel to WI_BOT_* above) ──────────
	// Entity-keyed, distinct from the client-slot bot traps; the handlers
	// guard entityNum internally.
	case G_MONSTER_LUA_BIND:
		return SV_Lua_MonsterBind( args[1], args[2] );
	case G_MONSTER_LUA_UNBIND:
		SV_Lua_MonsterUnbind( args[1] );
		return 0;
	case G_MONSTER_LUA_DECIDE:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( wbDecideCtx_t ) );
		VM_CHECKBOUNDS( gvm, args[3], args[4] );
		return SV_Lua_MonsterDecide( args[1], VMA(2), VMA(3), args[4] );
	case G_MONSTER_LUA_PROFILE_FIELD:
		return FloatAsInt( SV_Lua_MonsterProfileField( args[1], args[2] ) );

	case G_WIREDNET_GET_PING:
		if ( args[1] >= 0 && args[1] < sv_maxclients->integer ) {
			conn_handle_t conn = ( transport && transport->lookup_by_addr )
				? transport->lookup_by_addr( &svs.clients[ args[1] ].netchan.remoteAddress )
				: CONN_INVALID;
			if ( conn != CONN_INVALID )
				return transport_for_handle( conn )->get_ping( conn );
		}
		return -1;

	case G_WIREDNET_GET_LOSS:
		if ( args[1] >= 0 && args[1] < sv_maxclients->integer ) {
			conn_handle_t conn = ( transport && transport->lookup_by_addr )
				? transport->lookup_by_addr( &svs.clients[ args[1] ].netchan.remoteAddress )
				: CONN_INVALID;
			if ( conn != CONN_INVALID )
				return (int)( transport_for_handle( conn )->get_loss( conn ) * 1000.0f );
		}
		return 0;

	case G_WIREDNET_GET_BANDWIDTH:
		if ( args[1] >= 0 && args[1] < sv_maxclients->integer ) {
			conn_handle_t conn = ( transport && transport->lookup_by_addr )
				? transport->lookup_by_addr( &svs.clients[ args[1] ].netchan.remoteAddress )
				: CONN_INVALID;
			if ( conn != CONN_INVALID )
				return transport_for_handle( conn )->get_bandwidth( conn );
		}
		return 0;

#if FEAT_RECAST_NAVMESH
	case G_NAV_FIND_PATH:
	case G_NAV_RAYCAST:
	case G_NAV_FIND_NEAREST_POLY:
	case G_NAV_GET_POLY_AREA_FLAGS:
	case G_NAV_TRIGGER_OFF_MESH_LINK:
	case G_NAV_GET_RANDOM_POINT:
	case G_NAV_ADD_CROWD_AGENT:
	case G_NAV_UPDATE_CROWD_AGENT:
	case G_NAV_REMOVE_CROWD_AGENT:
	case G_NAV_UPDATE_CROWD:
	case G_NAV_IS_READY:
	case G_NAV_IS_BAKING:
	case G_NAV_SET_POLY_FLAGS_FOR_DOOR:
	case G_NAV_PREDICT_ENEMY_POSITION:
		{
			byte *vmBase = (gvm && !gvm->entryPoint) ? (byte *)gvm->dataBase : NULL;
			return Nav_HandleTrap( args[0], args, vmBase );
		}
#endif /* FEAT_RECAST_NAVMESH */

	default:
		Com_Terminate( TERM_CLIENT_DROP, "Bad game system trap: %ld", (long int) args[0] );
	}
	return 0;
}


/*
====================
SV_DllSyscall
====================
*/
static intptr_t QDECL SV_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
	intptr_t args[14]; // max.count for qagame
	va_list ap;

	args[0] = arg;
	va_start( ap, arg );
	for (int i = 1; i < ARRAY_LEN( args ); i++ )
		args[ i ] = va_arg( ap, intptr_t );
	va_end( ap );

	return SV_GameSystemCalls( args );
#else
	return SV_GameSystemCalls( &arg );
#endif
}


/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, 1, GAME_SHUTDOWN, qfalse );
	VM_Free( gvm );
	gvm = NULL;
	FS_VM_CloseFiles( H_QAGAME, 0 );   // game VM is host-wide, not per-app (slot 0)
}


/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM( qboolean restart ) {
	// start the entity parsing at the beginning
	sv.entityParsePoint = CM_EntityString();

	// clear all gentity pointers that might still be set from
	// a previous level
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
	// now done before GAME_INIT call
	for ( int i = 0; i < sv.maxclients; i++ ) {
		svs.clients[i].gentity = NULL;
	}

	// Random seed for this gamestate. Normally the wall-clock msec count, so
	// spawn selection (and anything else driven by the game RNG) varies every
	// launch. Setting sv_seed to a non-negative value pins the seed instead,
	// making a headless run reproducible — used by the nav-trace regression
	// gate to diff bot pathing against a golden. Default -1 keeps wall-clock.
	{
		int seed = Cvar_VariableIntegerValue( "sv_seed" );
		if ( seed < 0 ) {
			seed = Com_Milliseconds();
		}
		VM_Call( gvm, 3, GAME_INIT, sv.time, seed, restart );
	}
}


/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
	if ( !gvm ) {
		return;
	}
	VM_Call( gvm, 1, GAME_SHUTDOWN, qtrue );

	// do a restart instead of a free
	gvm = VM_Restart( gvm );
	if ( !gvm ) {
		Com_Terminate( TERM_CLIENT_DROP, "VM_Restart on game failed" );
	}

	SV_InitGameVM( qtrue );

	// load userinfo filters
	SV_LoadFilters( sv_filter->string );
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs( void ) {
	//FIXME these are temp while I make bots run in vm
	extern int bot_enable;

	cvar_t *var = Cvar_Get( "bot_enable", "1", CVAR_LATCH );
	if ( var ) {
		bot_enable = var->integer;
	}
	else {
		bot_enable = 0;
	}

	// owner = an engine-owned token for the single server game VM (VM_GAME never
	// multiplies). Pointer identity only; NOT clc.clientNum (tier rule).
	static char sv_gameAppPrimary;
	// cgameInstance arg (0) is ignored for VM_GAME — the server game VM never multiplies.
	gvm = VM_Create( VM_GAME, 0, &sv_gameAppPrimary, SV_GameSystemCalls, SV_DllSyscall, Cvar_VariableIntegerValue( "vm_game" ) );
	if ( !gvm ) {
		Com_Terminate( TERM_CLIENT_DROP, "VM_Create on game failed" );
	}

	SV_InitGameVM( qfalse );

	// load userinfo filters
	SV_LoadFilters( sv_filter->string );
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
	if ( sv.state != SS_GAME ) {
		return qfalse;
	}

	return VM_Call( gvm, 0, GAME_CONSOLE_COMMAND );
}
