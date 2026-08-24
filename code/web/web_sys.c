// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "web_sys.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#include <stdarg.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS( void, WiredWeb_ConsoleWrite, ( const char *text ), {
	console.log( UTF8ToString( text ) );
} );

EM_JS( void, WiredWeb_PublishFatal, ( const char *text ), {
	const message = UTF8ToString(text);
	if (typeof globalThis.wiredWebClientFatal === "function")
		globalThis.wiredWebClientFatal(message);
	else console.error(message);
} );

EM_JS( int, WiredWeb_FillRandom, ( byte *bytes, int length ), {
	if (!globalThis.crypto?.getRandomValues || length < 0) return 0;
	globalThis.crypto.getRandomValues(HEAPU8.subarray(bytes, bytes + length));
	return 1;
} );

EM_JS( void, WiredWeb_CallDllEntrySymbol,
		( uintptr_t symbol, uintptr_t syscall ), {
	try {
		getWasmTableEntry(symbol)(syscall);
	} catch (error) {
		console.error(`browser dllEntry failure @ ${symbol}: ${error?.stack ?? error}`);
		throw error;
	}
} );

EM_JS( intptr_t, WiredWeb_CallVmMainSymbol,
		( uintptr_t symbol, int command, int arg0, int arg1, int arg2 ), {
	try {
		return getWasmTableEntry(symbol)(command, arg0, arg1, arg2);
	} catch (error) {
		console.error(`browser vmMain failure @ ${symbol}: ${error?.stack ?? error}`);
		throw error;
	}
} );
#else
#error "code/web/web_sys.c is an Emscripten-only platform adapter"
#endif

void Sys_Init( void ) {}
void Sys_SendKeyEvents( void ) {}
void Sys_Sleep( int msec ) { (void)msec; }
char *Sys_ConsoleInput( void ) { return NULL; }

void Sys_Print( const char *msg ) {
	if ( msg ) WiredWeb_ConsoleWrite( msg );
}

void NORETURN FORMAT_PRINTF(1, 2) QDECL Sys_Error( const char *error, ... ) {
	char text[4096];
	va_list args;
	va_start( args, error );
	vsnprintf( text, sizeof( text ), error, args );
	va_end( args );
	WiredWeb_PublishFatal( text );
	emscripten_force_exit( 1 );
	abort();
}

void NORETURN Sys_Quit( void ) {
	emscripten_force_exit( 0 );
	abort();
}

qboolean Sys_RandomBytes( byte *string, int len ) {
	if ( !string || len < 0 ) return qfalse;
	return WiredWeb_FillRandom( string, len ) ? qtrue : qfalse;
}

qboolean Sys_LowPhysicalMemory( void ) { return qfalse; }
char *Sys_GetClipboardData( void ) { return NULL; }
void Sys_SetClipboardData( const char *text ) { (void)text; }
void Sys_SetClipboardBitmap( const byte *bitmap, int length ) {
	(void)bitmap; (void)length;
}
void Sys_SetClipboardImagePNG( const byte *png, int length ) {
	(void)png; (void)length;
}
void Sys_FlashWindow( void ) {}
void Sys_BeepAttention( void ) {}
void Sys_DisplaySystemConsole( qboolean show ) { (void)show; }
void Sys_ShowConsole( int level, qboolean quitOnClose ) {
	(void)level; (void)quitOnClose;
}
void Sys_SetErrorText( const char *text ) { (void)text; }
void QDECL Sys_SetStatus( const char *format, ... ) { (void)format; }
void Sys_SetMainThreadPolicy( void ) {}
void Sys_BeginProfiling( void ) {}
void Sys_EndProfiling( void ) {}
void Sys_InstallCrashHandler( void ) {}

qboolean Sys_MutexInit( sys_mutex_t *mutex ) {
	if ( !mutex ) return qfalse;
	memset( mutex, 0, sizeof( *mutex ) );
	mutex->opaque[0] = 1u;
	return qtrue;
}

void Sys_MutexLock( sys_mutex_t *mutex ) { (void)mutex; }
void Sys_MutexUnlock( sys_mutex_t *mutex ) { (void)mutex; }
void Sys_MutexDestroy( sys_mutex_t *mutex ) {
	if ( mutex ) memset( mutex, 0, sizeof( *mutex ) );
}

FILE *Sys_FOpen( const char *ospath, const char *mode ) {
	struct stat info;
	if ( !ospath || !mode ) return NULL;
	if ( stat( ospath, &info ) == 0 && S_ISDIR( info.st_mode ) ) return NULL;
	return fopen( ospath, mode );
}

qboolean Sys_GetFileStats( const char *filename, fileOffset_t *size,
		fileTime_t *mtime, fileTime_t *ctime ) {
	struct stat info;
	if ( !size || !mtime || !ctime || !filename || stat( filename, &info ) != 0 ) {
		if ( size ) *size = 0;
		if ( mtime ) *mtime = 0;
		if ( ctime ) *ctime = 0;
		return qfalse;
	}
	*size = (fileOffset_t)info.st_size;
	*mtime = (fileTime_t)info.st_mtime;
	*ctime = (fileTime_t)info.st_ctime;
	return qtrue;
}

qboolean Sys_Mkdir( const char *path ) {
	return path && ( mkdir( path, 0750 ) == 0 || errno == EEXIST )
		? qtrue : qfalse;
}

qboolean Sys_ResetReadOnlyAttribute( const char *ospath ) {
	(void)ospath;
	return qfalse;
}

const char *Sys_Pwd( void ) {
	static char path[MAX_OSPATH];
	if ( !path[0] && !getcwd( path, sizeof( path ) ) ) Q_strncpyz( path, "/", sizeof( path ) );
	return path;
}

const char *Sys_DefaultInstallPath( void ) { return "/"; }
const char *Sys_DefaultHomePath( void ) { return "/home/web_user"; }

static int WiredWeb_ListFiles( const char *directory, const char *subdir,
		const char *extension, const char *filter, char **list, int maxFiles,
		int remainingDepth ) {
	char search[MAX_OSPATH * 2 + MAX_QPATH + 1];
	char relative[MAX_OSPATH * 2];
	char absolute[MAX_OSPATH * 2];
	DIR *dir;
	struct dirent *entry;
	struct stat info;
	int count = 0;
	int extensionLength;
	qboolean directoryOnly;
	qboolean patterns;
	if ( !directory || !subdir || !extension || !list || maxFiles <= 0 ) return 0;
	directoryOnly = extension[0] == '/' && extension[1] == '\0';
	if ( directoryOnly ) extension = "";
	extensionLength = (int)strlen( extension );
	patterns = Com_HasPatterns( extension );
	if ( patterns && extension[0] == '.' && extension[1] ) ++extension;
	Com_sprintf( search, sizeof( search ), "%s%s%s", directory,
		subdir[0] ? "/" : "", subdir );
	dir = opendir( search );
	if ( !dir ) return 0;
	while ( count < maxFiles && ( entry = readdir( dir ) ) != NULL ) {
		const char *dot;
		int nameLength;
		Com_sprintf( absolute, sizeof( absolute ), "%s/%s", search, entry->d_name );
		if ( stat( absolute, &info ) != 0 ) continue;
		if ( S_ISDIR( info.st_mode ) && remainingDepth > 0
				&& strcmp( entry->d_name, "." ) && strcmp( entry->d_name, ".." ) ) {
			char child[MAX_OSPATH * 2 + MAX_QPATH + 1];
			Com_sprintf( child, sizeof( child ), "%s%s%s", subdir,
				subdir[0] ? "/" : "", entry->d_name );
			count += WiredWeb_ListFiles( directory, child, extension, filter,
				list + count, maxFiles - count, remainingDepth - 1 );
			if ( count >= maxFiles ) break;
		}
		if ( S_ISDIR( info.st_mode ) != directoryOnly ) continue;
		Com_sprintf( relative, sizeof( relative ), "%s%s%s", subdir,
			subdir[0] ? "/" : "", entry->d_name );
		if ( filter && filter[0] ) {
			if ( !Com_FilterPath( filter, relative ) ) continue;
		} else if ( extension[0] ) {
			if ( patterns ) {
				dot = strrchr( entry->d_name, '.' );
				if ( !dot || !Com_FilterExt( extension, dot + 1 ) ) continue;
			} else {
				nameLength = (int)strlen( entry->d_name );
				if ( nameLength < extensionLength
						|| Q_stricmp( entry->d_name + nameLength - extensionLength,
							extension ) ) continue;
			}
		}
		list[count++] = FS_CopyString( relative );
	}
	closedir( dir );
	return count;
}

char **Sys_ListFiles( const char *directory, const char *extension,
		const char *filter, int *numFiles, int subdirs ) {
	char *found[MAX_FOUND_FILES];
	char **copy;
	int count, i;
	if ( !numFiles ) return NULL;
	if ( !extension ) extension = "";
	count = WiredWeb_ListFiles( directory, "", extension, filter, found,
		ARRAY_LEN( found ), subdirs );
	copy = Z_Malloc( (size_t)( count + 1 ) * sizeof( *copy ) );
	for ( i = 0; i < count; ++i ) copy[i] = found[i];
	copy[count] = NULL;
	if ( count > 1 ) Com_SortList( copy, count );
	*numFiles = count;
	return copy;
}

void Sys_FreeFileList( char **list ) {
	int i;
	if ( !list ) return;
	for ( i = 0; list[i]; ++i ) Z_Free( list[i] );
	Z_Free( list );
}

static int s_libraryErrors;

void *Sys_LoadLibrary( const char *name ) {
	const char *extension;
	void *handle;
	if ( !name || !*name ) {
		s_libraryErrors++;
		return NULL;
	}
	extension = strrchr( name, '.' );
	if ( !extension || Q_stricmp( extension, ".wasm" ) != 0 ) {
		s_libraryErrors++;
		return NULL;
	}
	handle = dlopen( name, RTLD_NOW );
	if ( !handle ) {
		const char *error = dlerror();
		s_libraryErrors++;
		Sys_Print( error ? error : "browser dynamic module load failed" );
		Sys_Print( "\n" );
	}
	return handle;
}
void *Sys_LoadFunction( void *handle, const char *name ) {
	void *symbol;
	if ( !handle || !name || !name[0] ) {
		s_libraryErrors++;
		return NULL;
	}
	dlerror();
	symbol = dlsym( handle, name );
	if ( dlerror() ) {
		s_libraryErrors++;
		return NULL;
	}
	return symbol;
}
void Sys_CallDllEntry( void *symbol, dllSyscall_t syscall ) {
	WiredWeb_CallDllEntrySymbol( (uintptr_t)symbol, (uintptr_t)syscall );
}
intptr_t Sys_CallVmMain( vmMainFunc_t symbol, int command,
		int arg0, int arg1, int arg2 ) {
	return WiredWeb_CallVmMainSymbol( (uintptr_t)symbol, command, arg0, arg1, arg2 );
}
int Sys_LoadFunctionErrors( void ) {
	int errors = s_libraryErrors;
	s_libraryErrors = 0;
	return errors;
}
void Sys_UnloadLibrary( void *handle ) { if ( handle ) dlclose( handle ); }

void Nav_Frame( void ) {}

EMSCRIPTEN_KEEPALIVE void WiredWeb_InputKey( int key, int down ) {
	Sys_QueEvent( (uint64_t)Sys_NanoTime(), SE_KEY, key, down ? 1 : 0, 0, NULL );
}

EMSCRIPTEN_KEEPALIVE void WiredWeb_InputChar( int codepoint ) {
	if ( codepoint > 0 )
		Sys_QueEvent( (uint64_t)Sys_NanoTime(), SE_CHAR, codepoint, 0, 0, NULL );
}

EMSCRIPTEN_KEEPALIVE void WiredWeb_InputMouse( float dx, float dy ) {
	Sys_QueEvent( (uint64_t)Sys_NanoTime(), SE_MOUSE,
		SE_MouseEnc( dx ), SE_MouseEnc( dy ), 0, NULL );
}
