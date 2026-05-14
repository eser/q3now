// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
/*
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Quake3 you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/

#ifdef USE_OPENGL_API

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderer/qgl.h"
#include "../renderercommon/tr_types.h"
#include "glw_win.h"
#include "win_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_system, "system" );

#define GLE( ret, name, ... ) ret ( APIENTRY * q##name )( __VA_ARGS__ );
QGL_Win32_PROCS;
QGL_Swp_PROCS;
#undef GLE

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.  This
** is only called during a hard shutdown of the OGL subsystem (e.g. vid_restart).
*/
void QGL_Shutdown( qboolean unloadDLL )
{
	Com_Log( SEV_INFO, LOG_CH(ch_system), "...shutting down QGL\n" );

	if ( glw_state.OpenGLLib && unloadDLL )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_system), "...unloading OpenGL DLL\n" );
		Sys_UnloadLibrary( glw_state.OpenGLLib );
		glw_state.OpenGLLib = NULL;
	}

#define GLE( ret, name, ... ) q##name = NULL;
	QGL_Win32_PROCS;
	QGL_Swp_PROCS;
#undef GLE
}


void *GL_GetProcAddress( const char *name )
{
	void *ptr;

	ptr = Sys_LoadFunction( glw_state.OpenGLLib, name );
	if ( !ptr && qwglGetProcAddress )
	{
		ptr = qwglGetProcAddress( name );
	}

	return ptr;
}


/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to
** the appropriate GL stuff.  In Windows this means doing a
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
*/
qboolean QGL_Init( const char *dllname )
{
	char libName[1024];
#ifdef UNICODE
	TCHAR buffer[1024];
#endif

#if 0
	char systemDir[1024];

#ifdef UNICODE
	TCHAR buffer[1024];
	GetSystemDirectory( buffer, ARRAYSIZE( buffer ) );
	strcpy( systemDir, WtoA( buffer ) );
#else
	GetSystemDirectory( systemDir, sizeof( systemDir ) );
#endif
#endif

	Com_Log( SEV_INFO, LOG_CH(ch_system), "...initializing QGL\n" );

	if ( glw_state.OpenGLLib == NULL )
	{
		// NOTE: this assumes that 'dllname' is lower case (and it should be)!
#if 0
		if ( dllname[0] != '!' )
			Com_sprintf( libName, sizeof( libName ), "%s\\%s", systemDir, dllname );
		else
			Q_strncpyz( libName, dllname+1, sizeof( libName ) );
#else
		Q_strncpyz( libName, dllname, sizeof( libName ) );
#endif
		{ qstring_t _lb_qs = QS_WrapExisting( libName, sizeof( libName ) ); QS_Append( &_lb_qs, ".dll" ); }
		glw_state.OpenGLLib = Sys_LoadLibrary( libName );
		if ( glw_state.OpenGLLib == NULL )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_system), "...loading '%s' : " S_COLOR_YELLOW "failed\n", libName );
			return qfalse;
		}

		// get exact loaded module name
#ifdef UNICODE
		GetModuleFileName( glw_state.OpenGLLib, buffer, ARRAY_LEN( buffer ) );
		buffer[ ARRAY_LEN( buffer ) - 1 ] = '\0';
		Q_strncpyz( libName, WtoA( buffer ), sizeof( libName ) );
#else
		GetModuleFileName( glw_state.OpenGLLib, libName, sizeof( libName ) );
		libName[ sizeof( libName ) - 1 ] = '\0';
#endif
		Com_Log( SEV_INFO, LOG_CH(ch_system), "...loading '%s' : succeeded\n", libName );
	}

	Sys_LoadFunctionErrors(); // reset error count

#define GLE( ret, name, ... ) q##name = GL_GetProcAddress( XSTRING( name ) ); if ( !q##name ) { Com_Log( SEV_INFO, LOG_CH(ch_system), "Error resolving core Win32 functions\n" ); return qfalse; }
	QGL_Win32_PROCS;
#undef GLE

	return qtrue;
}

#endif // USE_OPENGL_API
