/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
/*****************************************************************************
 * name:		files.c
 *
 * desc:		handle based filesystem for Quake III Arena
 *
 * $Archive: /MissionPack/code/qcommon/files.c $
 *
 *****************************************************************************/


#include "q_shared.h"
#include "qcommon.h"
#include "unzip.h"
#include "q_feats.h"

#if FEAT_SW3Z
#include "sw3z.h"
#endif

/*
=============================================================================

QUAKE3 FILESYSTEM

All of Quake's data access is through a hierarchical file system, but the contents of
the file system can be transparently merged from several sources.

A "qpath" is a reference to game file data.  MAX_ZPATH is 256 characters, which must include
a terminating zero. "..", "\\", and ":" are explicitly illegal in qpaths to prevent any
references outside the quake directory system.

The "base path" is the path to the directory holding all the game directories and usually
the executable.  It defaults to ".", but can be overridden with a "+set fs_installpath /opt/wired"
command line to allow code debugging in a different directory.  Basepath cannot
be modified at all after startup.  Any files that are created (demos, screenshots,
etc) will be created relative to the base path, so base path should usually be writable.

The "cd path" is the path to an alternate hierarchy that will be searched if a file
is not located in the base path.  A user can do a partial install that copies some
data to a base path created on their hard drive and leave the rest on the cd.  Files
are never written to the cd path.  It defaults to a value set by the installer, like
"e:\quake3", but it can be overridden with "+set fs_cdpath g:\quake3".

If a user runs the game directly from a CD, the base path would be on the CD.  This
should still function correctly, but all file writes will fail (harmlessly).

The "home path" is the path used for all write access. On win32 systems we have "base path"
== "home path", but on *nix systems the base installation is usually read-only, and
"home path" points to ~/.q3a or similar

The user can also install custom mods and content in "home path", so it should be searched
along with "home path" and "cd path" for game content.


The "base game" is the directory under the paths where data comes from by default, and
can be either "baseq3" or "demoq3".

The "current game" may be the same as the base game, or it may be the name of another
directory under the paths that should be searched for files before looking in the base game.
This is the basis for addons.

Clients automatically set the game directory after receiving a gamestate from a server,
so only servers need to worry about +set fs_game.

No other directories outside of the base game and current game will ever be referenced by
filesystem functions.

To save disk space and speed loading, directory trees can be collapsed into zip files.
The files use a ".pk3" extension to prevent users from unzipping them accidentally, but
otherwise the are simply normal uncompressed zip files.  A game directory can have multiple
zip files of the form "pak0.pk3", "pak1.pk3", etc.  Zip files are searched in decending order
from the highest number to the lowest, and will always take precedence over the filesystem.
This allows a pk3 distributed as a patch to override all existing data.

Because we will have updated executables freely available online, there is no point to
trying to restrict demo / oem versions of the game with code changes.  Demo / oem versions
should be exactly the same executables as release versions, but with different data that
automatically restricts where game media can come from to prevent add-ons from working.

After the paths are initialized, quake will look for the product.txt file.  If not
found and verified, the game will run in restricted mode.  In restricted mode, only
files contained in demoq3/pak0.pk3 will be available for loading, and only if the zip header is
verified to not have been modified.  A single exception is made for config.cfg.  Files
can still be written out in restricted mode, so screenshots and demos are allowed.
Restricted mode can be tested by setting "+set fs_restrict 1" on the command line, even
if there is a valid product.txt under the basepath or cdpath.

If not running in restricted mode, and a file is not found in any local filesystem,
an attempt will be made to download it and save it under the base path.

If the "fs_copyfiles" cvar is set to 1, then every time a file is sourced from the cd
path, it will be copied over to the base path.  This is a development aid to help build
test releases and to copy working sets over slow network links.

File search order: when FS_FOpenFileRead gets called it will go through the fs_searchpaths
structure and stop on the first successful hit. fs_searchpaths is built with successive
calls to FS_AddGameDirectory

Additionally, we search in several subdirectories:
current game is the current mode
base game is a variable to allow mods based on other mods
(such as baseq3 + missionpack content combination in a mod for instance)
BASEGAME is the hardcoded base game ("baseq3")

e.g. the qpath "sound/newstuff/test.opus" would be searched for in the following places:

home path + current game's zip files
home path + current game's directory
base path + current game's zip files
base path + current game's directory
cd path + current game's zip files
cd path + current game's directory

home path + base game's zip file
home path + base game's directory
base path + base game's zip file
base path + base game's directory
cd path + base game's zip file
cd path + base game's directory

home path + BASEGAME's zip file
home path + BASEGAME's directory
base path + BASEGAME's zip file
base path + BASEGAME's directory
cd path + BASEGAME's zip file
cd path + BASEGAME's directory

server download, to be written to home path + current game's directory


The filesystem can be safely shutdown and reinitialized with different
basedir / cddir / game combinations, but all other subsystems that rely on it
(sound, video) must also be forced to restart.

Because the same files are loaded by both the clip model (CM_) and renderer (TR_)
subsystems, a simple single-file caching scheme is used.  The CM_ subsystems will
load the file with a request to cache.  Only one file will be kept cached at a time,
so any models that are going to be referenced by both subsystems should alternate
between the CM_ load function and the ref load function.

TODO: A qpath that starts with a leading slash will always refer to the base game, even if another
game is currently active.  This allows character models, skins, and sounds to be downloaded
to a common directory no matter which game is active.

How to prevent downloading zip files?
Pass pk3 file names in systeminfo, and download before FS_Restart()?

Aborting a download disconnects the client from the server.

How to mark files as downloadable?  Commercial add-ons won't be downloadable.

Non-commercial downloads will want to download the entire zip file.
the game would have to be reset to actually read the zip in

Auto-update information

Path separators

Casing

  separate server gamedir and client gamedir, so if the user starts
  a local game after having connected to a network game, it won't stick
  with the network game.

  allow menu options for game selection?

Read / write config to floppy option.

Different version coexistence?

When building a pak file, make sure a config.cfg isn't present in it,
or configs will never get loaded from disk!

  todo:

  downloading (outside fs?)
  game directory passing and restarting

=============================================================================

*/

// every time a new demo pk3 file is built, this checksum must be updated.
// the easiest way to get it is to just run the game and see what it spits out
#define	DEMO_PAK0_CHECKSUM	2985612116u
static const unsigned pak_checksums[] = {
	1566731103u,
	298122907u,
	412165236u,
	2991495316u,
	1197932710u,
	4087071573u,
	3709064859u,
	908855077u,
	977125798u
};

// if this is defined, the executable positively won't work with any paks other
// than the demo pak, even if productid is present.  This is only used for our
// last demo release to prevent the mac and linux users from using the demo
// executable with the production windows pak before the mac/linux products
// hit the shelves a little later
// NOW defined in build files
//#define PRE_RELEASE_TADEMO

#include "files_pack.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_filesystem, "filesystem" );

#define MAX_ZPATH			256
#define MAX_FILEHASH_SIZE	4096

typedef struct {
	char		*path;		// /opt/wired
	char		*gamedir;	// baseq3
} directory_t;

typedef enum {
	DIR_STATIC = 0,	// always allowed, never changes
	DIR_ALLOW,
	DIR_DENY
} dirPolicy_t;

typedef struct searchpath_s {
	struct searchpath_s *next;
	pack_t		*pack;		// only one of pack / dir will be non NULL
	directory_t	*dir;
	dirPolicy_t	policy;
} searchpath_t;

static	char		fs_gamedir[MAX_OSPATH];	// this will be a single file name with no separators
static	cvar_t		*fs_debug;
static	cvar_t		*fs_homepath;
static	cvar_t		*fs_installpath;
static  cvar_t      *fs_basegame;
static	cvar_t		*fs_copyfiles;
static	cvar_t		*fs_gamedirvar;
#ifndef USE_HANDLE_CACHE
static	cvar_t		*fs_locked;
#endif
static	cvar_t		*fs_excludeReference;

static	searchpath_t	*fs_searchpaths;
static	int			fs_readCount;			// total bytes read
static	int			fs_loadCount;			// total files read
static	int			fs_loadStack;			// total files in memory
static	int			fs_packFiles;			// total number of files in all loaded packs

static	int			fs_pk3dirCount;			// total number of pk3 directories in searchpath
static	int			fs_packCount;			// total number of packs in searchpath
static	int			fs_dirCount;			// total number of directories in searchpath

static	int			fs_checksumFeed;

typedef union qfile_gus {
	FILE*		o;
	unzFile		z;
	void*		v;
} qfile_gut;

// NOLINTNEXTLINE(bugprone-tagged-union-member-count) — `unique` tracks ownership, not union variant; variant is implicit (file-type-dependent)
typedef struct qfile_us {
	qfile_gut	file;
	qboolean	unique;
} qfile_ut;

typedef struct {
	qfile_ut	handleFiles;
	qboolean	handleSync;
	qboolean	zipFile;
	int			zipFilePos;
	int			zipFileLen;
	char		name[MAX_ZPATH];
	handleOwner_t	owner;
	int			pakIndex;
	pack_t		*pak;
#if FEAT_SW3Z
	byte		*sw3zData;		// decompressed file buffer (NULL if not SW3Z, or if entry decompression deferred)
	int			sw3zSize;		// total decompressed size
	int			sw3zPos;		// current read position
	/* Phase 4-#4 deferred decompression hint.
	 * When >= 0 *and* sw3zData == NULL, the handle is "open but not yet
	 * decompressed". FS_ReadFile uses this to skip the intermediate
	 * sw3zData buffer entirely — the very next FS_Read decompresses
	 * straight into the caller's hunk buffer. Cleared on consumption. */
	int			sw3zEntryIdx;
#endif
} fileHandleData_t;

static fileHandleData_t	fsh[MAX_FILE_HANDLES];

// TTimo - https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=540
// whether we did a reorder on the current search path when joining the server
qboolean fs_reordered;

#define MAX_REF_PAKS	MAX_STRING_TOKENS

// never load anything from pk3 files that are not present at the server when pure
static int		fs_numServerPaks = 0;
static int		fs_serverPaks[MAX_REF_PAKS];			// checksums
static char		*fs_serverPakNames[MAX_REF_PAKS];		// pk3 names

// only used for autodownload, to make sure the client has at least
// all the pk3 files that are referenced at the server side
static int		fs_numServerReferencedPaks;
static int		fs_serverReferencedPaks[MAX_REF_PAKS];		// checksums
static char		*fs_serverReferencedPakNames[MAX_REF_PAKS];	// pk3 names

int	fs_lastPakIndex;

#if FEAT_SW3Z
/* Phase 4-#4 hint flag.
 * Set by FS_ReadFile around its FS_FOpenFileRead call to ask
 * FS_OpenFileInSW3Z to defer decompression. Single-threaded I/O makes
 * the static safe; FS_ReadFile is responsible for clearing it on every
 * exit path. */
static qboolean		fs_sw3z_deferOpen = qfalse;
#endif

#ifdef FS_MISSING
static FILE*		missingFiles = NULL;
#endif

static int FS_GetModList( char *listbuf, int bufsize );
static void FS_CheckIdPaks( void );
void FS_Reload( void );


/*
==============
FS_Initialized
==============
*/
qboolean FS_Initialized( void ) {
	return ( fs_searchpaths != NULL );
}


/*
=================
FS_PakIsPure
=================
*/
static qboolean FS_PakIsPure( const pack_t *pack ) {
#ifndef DEDICATED
	if ( fs_numServerPaks ) {
		for ( int i = 0 ; i < fs_numServerPaks ; i++ ) {
			// FIXME: also use hashed file names
			// NOTE TTimo: a pk3 with same checksum but different name would be validated too
			//   I don't see this as allowing for any exploit, it would only happen if the client does manips of its file names 'not a bug'
			if ( pack->checksum == fs_serverPaks[i] ) {
				return qtrue;		// on the approved list
			}
		}
		return qfalse;	// not on the pure server pak list
	}
#endif
	return qtrue;
}

/* FS_SW3ZPakIsPure removed — FS_PakIsPure now handles both types */


/*
=================
FS_LoadStack
return load stack
=================
*/
int FS_LoadStack( void ) {
	return fs_loadStack;
}


/*
================
return a hash value for the filename
================
*/
#define FS_HashFileName Com_GenerateHashValue


/*
=================
FS_HandleForFile
=================
*/
static fileHandle_t	FS_HandleForFile( void )
{
	for ( int i = 1 ; i < MAX_FILE_HANDLES ; i++ )
	{
		if ( fsh[i].handleFiles.file.v == NULL
#if FEAT_SW3Z
			&& fsh[i].sw3zData == NULL
#endif
			)
			return i;
	}

	Com_Terminate( TERM_CLIENT_DROP, "FS_HandleForFile: none free" );
	return FS_INVALID_HANDLE;
}


static FILE	*FS_FileForHandle( fileHandle_t f ) {
	if ( f <= 0 || f >= MAX_FILE_HANDLES ) {
		Com_Terminate( TERM_CLIENT_DROP, "FS_FileForHandle: out of range" );
	}
	if ( fsh[f].zipFile ) {
		Com_Terminate( TERM_CLIENT_DROP, "FS_FileForHandle: can't get FILE on zip file" );
	}
	if ( ! fsh[f].handleFiles.file.o ) {
		Com_Terminate( TERM_CLIENT_DROP, "FS_FileForHandle: NULL" );
	}

	return fsh[f].handleFiles.file.o;
}


void FS_ForceFlush( fileHandle_t f ) {
	FILE *file;

	file = FS_FileForHandle(f);
	setvbuf( file, NULL, _IONBF, 0 );
}


/*
================
FS_FileLengthByHandle

If this is called on a non-unique FILE (from a pak file),
it will return the size of the pak file, not the expected
size of the file.
================
*/
#if 0
static int FS_FileLengthByHandle( fileHandle_t f ) {
	int		pos;
	int		end;
	FILE*	h;

	h = FS_FileForHandle( f );
	pos = ftell( h );
	fseek( h, 0, SEEK_END );
	end = ftell( h );
	fseek( h, pos, SEEK_SET );

	return end;
}
#endif


/*
================
FS_FileLength
================
*/
static int FS_FileLength( FILE* h )
{
	int pos = ftell( h );
	fseek( h, 0, SEEK_END );
	int end = ftell( h );
	fseek( h, pos, SEEK_SET );

	return end;
}


/*
====================
FS_PakIndexForHandle
====================
*/
int FS_PakIndexForHandle( fileHandle_t f ) {

	if ( f <= FS_INVALID_HANDLE || f >= MAX_FILE_HANDLES )
		return -1;

	return fsh[ f ].pakIndex;
}


/*
====================
FS_ReplaceSeparators

Fix things up differently for win/unix/mac
====================
*/
static void FS_ReplaceSeparators( char *path ) {
	char	*s;

	for ( s = path ; *s ; s++ ) {
		if ( *s == PATH_SEP_FOREIGN ) {
			*s = PATH_SEP;
		}
	}
}


/*
===================
FS_BuildOSPath

Qpath may have either forward or backwards slashes
===================
*/
char *FS_BuildOSPath( const char *base, const char *game, const char *qpath ) {
	char	temp[MAX_OSPATH*2+1];
	static char ospath[2][sizeof(temp)+MAX_OSPATH];
	static int toggle;

	toggle ^= 1;		// flip-flop to allow two returns without clash

	if( !game || !game[0] ) {
		game = fs_gamedir;
	}

	if ( qpath )
		Com_sprintf( temp, sizeof( temp ), "%c%s%c%s", PATH_SEP, game, PATH_SEP, qpath );
	else
		Com_sprintf( temp, sizeof( temp ), "%c%s", PATH_SEP, game );

	FS_ReplaceSeparators( temp );
	Com_sprintf( ospath[toggle], sizeof( ospath[0] ), "%s%s", base, temp );

	return ospath[toggle];
}


/*
================
FS_CheckDirTraversal

Check whether the string contains stuff like "../" to prevent directory traversal bugs
and return qtrue if it does.
================
*/
static qboolean FS_CheckDirTraversal( const char *checkdir )
{
	if ( strstr( checkdir, "../" ) || strstr( checkdir, "..\\" ) )
		return qtrue;

	if ( strstr( checkdir, "::" ) )
		return qtrue;

	return qfalse;
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
static qboolean FS_CreatePath( const char *OSPath ) {
	char	path[MAX_OSPATH*2+1];
	char	*ofs;

	// make absolutely sure that it can't back up the path
	// FIXME: is c: allowed???
	if ( FS_CheckDirTraversal( OSPath ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "WARNING: refusing to create relative path \"%s\"\n", OSPath );
		return qtrue;
	}

	Q_strncpyz( path, OSPath, sizeof( path ) );
	// Make sure we have OS correct slashes
	FS_ReplaceSeparators( path );
	for ( ofs = path + 1; *ofs; ofs++ ) {
		if ( *ofs == PATH_SEP ) {
			// create the directory
			*ofs = '\0';
			Sys_Mkdir( path );
			*ofs = PATH_SEP;
		}
	}
	return qfalse;
}


/*
=================
FS_CopyFile

Copy a fully specified file from one place to another
=================
*/
static void FS_CopyFile( const char *fromOSPath, const char *toOSPath ) {
	FILE	*f;
	size_t	len;
	byte	*buf;

	Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "copy %s to %s\n", fromOSPath, toOSPath );

	if (strstr(fromOSPath, "journal.dat") || strstr(fromOSPath, "journaldata.dat")) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "Ignoring journal files\n");
		return;
	}

	f = Sys_FOpen( fromOSPath, "rb" );
	if ( !f ) {
		return;
	}

	len = FS_FileLength( f );

	// we are using direct malloc instead of Z_Malloc here, so it
	// probably won't work on a mac... It's only for developers anyway...
	buf = malloc( len );
	if ( !buf ) {
		fclose( f );
		Com_Terminate( TERM_UNRECOVERABLE, "Memory alloc error in FS_Copyfiles()\n" );
	}

	if (fread( buf, 1, len, f ) != len) {
		free( buf );
		fclose( f );
		Com_Terminate( TERM_UNRECOVERABLE, "Short read in FS_Copyfiles()\n" );
	}
	fclose( f );

	f = Sys_FOpen( toOSPath, "wb" );
	if ( !f ) {
		if ( FS_CreatePath( toOSPath ) ) {
			free( buf );
			return;
		}
		f = Sys_FOpen( toOSPath, "wb" );
		if ( !f ) {
			free( buf );
			return;
		}
	}

	if ( fwrite( buf, 1, len, f ) != len ) {
		free( buf );
		fclose( f );
		Com_Terminate( TERM_UNRECOVERABLE, "Short write in FS_Copyfiles()\n" );
	}
	fclose( f );
	free( buf );
}


static const char *FS_HasExt( const char *fileName, const char **extList, int extCount );

/*
=================
FS_AllowedExtension
=================
*/
qboolean FS_AllowedExtension( const char *fileName, qboolean allowPk3s, const char **ext )
{
	static const char *extlist[] =	{ "dll", "exe", "so", "dylib", "qvm", "pk3" };

	const char *e = strrchr( fileName, '.' );

	// check for unix '.so.[0-9]' pattern
	if ( e >= (fileName + 3) && *(e+1) >= '0' && *(e+1) <= '9' && *(e+2) == '\0' )
	{
		if ( *(e-3) == '.' && (*(e-2) == 's' || *(e-2) == 'S') && (*(e-1) == 'o' || *(e-1) == 'O') )
		{
			if ( ext )
			{
				*ext = (e-2);
			}
			return qfalse;
		}
	}
	if ( !e )
		return qtrue;

	e++; // skip '.'

	int n = allowPk3s ? ARRAY_LEN( extlist ) - 1 : ARRAY_LEN( extlist );

	for ( int i = 0; i < n; i++ )
	{
		if ( Q_stricmp( e, extlist[i] ) == 0 )
		{
			if ( ext )
				*ext = e;
			return qfalse;
		}
	}

	return qtrue;
}


/*
=================
FS_CheckFilenameIsNotExecutable

ERR_FATAL if trying to manipulate a file with the platform library extension
=================
 */
static void FS_CheckFilenameIsNotAllowed( const char *filename, const char *function, qboolean allowPk3s )
{
	// Check if the filename ends with the library extension
	const char *extension;
	if ( FS_AllowedExtension( filename, allowPk3s, &extension ) == qfalse )
	{
		Com_Terminate( TERM_UNRECOVERABLE, "%s: Not allowed to manipulate '%s' due "
			"to %s extension", function, filename, extension );
	}
}


/*
===========
FS_Remove

===========
*/
void FS_Remove( const char *osPath )
{
	FS_CheckFilenameIsNotAllowed( osPath, __func__, qtrue );

	remove( osPath );
}


/*
===========
FS_HomeRemove
===========
*/
void FS_HomeRemove( const char *osPath )
{
	FS_CheckFilenameIsNotAllowed( osPath, __func__, qfalse );

	remove( FS_BuildOSPath( fs_homepath->string,
			fs_gamedir, osPath ) );
}


/*
================
FS_FileExists

Tests if the file exists in the current gamedir, this DOES NOT
search the paths.  This is to determine if opening a file to write
(which always goes into the current gamedir) will cause any overwrites.
NOTE TTimo: this goes with FS_FOpenFileWrite for opening the file afterwards
================
*/
qboolean FS_FileExists( const char *file )
{
	char *testpath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, file );
	FILE *f = Sys_FOpen( testpath, "rb" );
	if (f) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}


/*
================
FS_SV_FileExists

Tests if the file exists
================
*/
qboolean FS_SV_FileExists( const char *file )
{
	// SV files only ever land in fs_homepath; the basepath fallback was
	// for a write target nothing in the engine actually writes to.
	char *testpath = FS_BuildOSPath( fs_homepath->string, file, NULL );
	FILE *f = Sys_FOpen( testpath, "rb" );
	if ( f ) {
		fclose( f );
		return qtrue;
	}

	return qfalse;
}


/*
===========
FS_InitHandle
===========
*/
static void FS_InitHandle( fileHandleData_t *fd ) {
	fd->pak = NULL;
	fd->pakIndex = -1;
	fs_lastPakIndex = -1;
#if FEAT_SW3Z
	fd->sw3zData = NULL;
	fd->sw3zSize = 0;
	fd->sw3zPos = 0;
	fd->sw3zEntryIdx = -1;
#endif
}


/*
===========
FS_SV_FOpenFileWrite
===========
*/
fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) {
	char *ospath;
	fileHandle_t	f;
	fileHandleData_t *fd;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( !*filename ) {
		return FS_INVALID_HANDLE;
	}

	ospath = FS_BuildOSPath( fs_homepath->string, filename, NULL );

	f = FS_HandleForFile();
	fd = &fsh[ f ];
	FS_InitHandle( fd );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_SV_FOpenFileWrite: %s\n", ospath );
	}

	FS_CheckFilenameIsNotAllowed( ospath, __func__, qtrue );

	Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "writing to: %s\n", ospath );

	fd->handleFiles.file.o = Sys_FOpen( ospath, "wb" );
	if ( !fd->handleFiles.file.o ) {
		if ( FS_CreatePath( ospath ) ) {
			return FS_INVALID_HANDLE;
		}
		fd->handleFiles.file.o = Sys_FOpen( ospath, "wb" );
		if ( !fd->handleFiles.file.o ) {
			return FS_INVALID_HANDLE;
		}
	}

	Q_strncpyz( fd->name, filename, sizeof( fd->name ) );
	fd->handleSync = qfalse;
	fd->zipFile = qfalse;

	return f;
}


/*
===========
FS_SV_FOpenFileAppend

Opens a file for appending in fs_homepath, ignoring fs_game.
Mirrors FS_SV_FOpenFileWrite but uses "ab" mode.
===========
*/
fileHandle_t FS_SV_FOpenFileAppend( const char *filename ) {
	char *ospath;
	fileHandle_t	f;
	fileHandleData_t *fd;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( !*filename ) {
		return FS_INVALID_HANDLE;
	}

	ospath = FS_BuildOSPath( fs_homepath->string, filename, NULL );

	f = FS_HandleForFile();
	fd = &fsh[ f ];
	FS_InitHandle( fd );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_SV_FOpenFileAppend: %s\n", ospath );
	}

	FS_CheckFilenameIsNotAllowed( ospath, __func__, qtrue );

	Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "appending to: %s\n", ospath );

	fd->handleFiles.file.o = Sys_FOpen( ospath, "ab" );
	if ( !fd->handleFiles.file.o ) {
		if ( FS_CreatePath( ospath ) ) {
			return FS_INVALID_HANDLE;
		}
		fd->handleFiles.file.o = Sys_FOpen( ospath, "ab" );
		if ( !fd->handleFiles.file.o ) {
			return FS_INVALID_HANDLE;
		}
	}

	Q_strncpyz( fd->name, filename, sizeof( fd->name ) );
	fd->handleSync = qfalse;
	fd->zipFile = qfalse;

	return f;
}


/*
===========
FS_SV_FOpenFileRead
search for a file somewhere below the home path, base path or cd path
we search in that order, matching FS_SV_FOpenFileRead order
===========
*/
int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp ) {
	fileHandleData_t *fd;
	fileHandle_t f;
	char *ospath;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	// should never happen but for safe
	if ( !fp ) {
		return -1;
	}

	// allocate new file handle
	f = FS_HandleForFile();
	fd = &fsh[ f ];
	FS_InitHandle( fd );

#ifndef DEDICATED
	// don't let sound stutter
	// S_ClearSoundBuffer();
#endif

	// search homepath
	ospath = FS_BuildOSPath( fs_homepath->string, filename, NULL );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_SV_FOpenFileRead (fs_homepath): %s\n", ospath );
	}

	// SV files only ever land in fs_homepath; the basepath fallback was
	// for a write target nothing in the engine actually writes to.
	fd->handleFiles.file.o = Sys_FOpen( ospath, "rb" );

	if( fd->handleFiles.file.o != NULL ) {
		Q_strncpyz( fd->name, filename, sizeof( fd->name ) );
		fd->handleSync = qfalse;
		fd->zipFile = qfalse;
		*fp = f;
		return FS_FileLength( fd->handleFiles.file.o );
	}

	*fp = FS_INVALID_HANDLE;
	return -1;
}


/*
===========
FS_SV_Rename
===========
*/
void FS_SV_Rename( const char *from, const char *to ) {
	const char			*from_ospath, *to_ospath;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

#ifndef DEDICATED
	// don't let sound stutter
	// S_ClearSoundBuffer();
#endif

	from_ospath = FS_BuildOSPath( fs_homepath->string, from, NULL );
	to_ospath = FS_BuildOSPath( fs_homepath->string, to, NULL );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_SV_Rename: %s --> %s\n", from_ospath, to_ospath );
	}

	if ( rename( from_ospath, to_ospath ) ) {
		// Failed, try copying it and deleting the original
		FS_CopyFile( from_ospath, to_ospath );
		FS_Remove( from_ospath );
	}
}


/*
===========
FS_Rename
===========
*/
void FS_Rename( const char *from, const char *to ) {
	const char *from_ospath, *to_ospath;
	FILE *f;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

#ifndef DEDICATED
	// don't let sound stutter
	// S_ClearSoundBuffer();
#endif

	from_ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, from );
	to_ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, to );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_Rename: %s --> %s\n", from_ospath, to_ospath );
	}

	f = Sys_FOpen( from_ospath, "rb" );
	if ( f ) {
		fclose( f );
		FS_Remove( to_ospath );
	}

	if ( rename( from_ospath, to_ospath ) ) {
		// Failed, try copying it and deleting the original
		FS_CopyFile( from_ospath, to_ospath );
		FS_Remove( from_ospath );
	}
}

#ifdef USE_HANDLE_CACHE

static int		hpaksCount;
static pack_t	*hhead;

static void FS_RemoveFromHandleList( pack_t *pak )
{
	if ( pak->next_h != pak ) {
		// cut pak from list
		pak->next_h->prev_h = pak->prev_h;
		pak->prev_h->next_h = pak->next_h;
		if ( hhead == pak ) {
			hhead = pak->next_h;
		}
	} else {
#ifdef _DEBUG
		if ( hhead != pak )
			Com_Terminate( TERM_CLIENT_DROP, "%s(): invalid head pointer", __func__ );
#endif
		hhead = NULL;
	}

	pak->next_h = NULL;
	pak->prev_h = NULL;

	hpaksCount--;

#ifdef _DEBUG
	if ( hpaksCount < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): negative paks count", __func__ );
	}

	if ( hpaksCount == 0 && hhead != NULL ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): non-null head with zero paks count", __func__ );
	}
#endif
}


static void FS_AddToHandleList( pack_t *pak )
{
#ifdef _DEBUG
	if ( !PACK_ZIP_HANDLE(pak) ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): invalid pak handle", __func__ );
	}
	if ( pak->next_h || pak->prev_h ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): invalid pak pointers", __func__ );
	}
#endif
	while ( hpaksCount >= MAX_CACHED_HANDLES ) {
		// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — invariant: hpaksCount > 0 implies hhead != NULL
		pack_t *pk = hhead->prev_h; // tail item
#ifdef _DEBUG
		if ( PACK_ZIP_HANDLE(pk) == NULL || pk->handleUsed != 0 ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s(): invalid pak handle", __func__ );
		}
#endif
		unzClose( PACK_ZIP_HANDLE(pk) );
		PACK_ZIP_HANDLE(pk) = NULL;
		FS_RemoveFromHandleList( pk );
	}

	if ( hhead == NULL ) {
		pak->next_h = pak;
		pak->prev_h = pak;
	} else {
		hhead->prev_h->next_h = pak;
		pak->prev_h = hhead->prev_h;
		hhead->prev_h = pak;
		pak->next_h = hhead;
	}

	hhead = pak;
	hpaksCount++;
}

#if FEAT_SW3Z

/* ── SW3Z file-handle LRU ─────────────────────────────────────────────
 *
 * Bounds the count of simultaneously-open FILE* handles for SW3Z packs.
 * Distinct from the PK3 unzFile LRU above because:
 *  - The bounded resource differs (FILE* vs unzFile).
 *  - SW3Z handles have no "in-use" state — FS_OpenFileInSW3Z runs
 *    SW3Z_ReadEntry synchronously and the FILE* is idle on return.
 *    There is no analogue of pak->handleUsed, so every SW3Z pack with
 *    an open FILE* lives in this LRU at all times.
 *
 * Lifecycle:
 *  - Cold load (SW3Z_LoadArchive completes) → caller adds.
 *  - Cache disk-load (FS_LoadPakFromFile fopens) → caller adds.
 *  - Read access (FS_OpenFileInSW3Z) → if FILE* NULL, lazy fopen and
 *    add; otherwise touch to head.
 *  - Pack free (FS_FreePak SW3Z branch) → remove before fclose.
 *  - LRU eviction (cap exceeded) → fclose tail's FILE*, remove. Pack
 *    stays in pakHashTable; lazy reopen on next access.
 */

static int		hpaksCount_sw3z;
static pack_t	*hhead_sw3z;

static void FS_RemoveFromHandleList_SW3Z( pack_t *pak )
{
	if ( pak->next_h_sw3z != pak ) {
		// cut pak from list
		pak->next_h_sw3z->prev_h_sw3z = pak->prev_h_sw3z;
		pak->prev_h_sw3z->next_h_sw3z = pak->next_h_sw3z;
		if ( hhead_sw3z == pak ) {
			hhead_sw3z = pak->next_h_sw3z;
		}
	} else {
#ifdef _DEBUG
		if ( hhead_sw3z != pak )
			Com_Terminate( TERM_CLIENT_DROP, "%s(): invalid head pointer", __func__ );
#endif
		hhead_sw3z = NULL;
	}

	pak->next_h_sw3z = NULL;
	pak->prev_h_sw3z = NULL;

	hpaksCount_sw3z--;

#ifdef _DEBUG
	if ( hpaksCount_sw3z < 0 ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): negative paks count", __func__ );
	}

	if ( hpaksCount_sw3z == 0 && hhead_sw3z != NULL ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): non-null head with zero paks count", __func__ );
	}
#endif
}


static void FS_AddToHandleList_SW3Z( pack_t *pak )
{
#ifdef _DEBUG
	if ( !PACK_FILE_HANDLE(pak) ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): invalid pak FILE handle", __func__ );
	}
	if ( pak->next_h_sw3z || pak->prev_h_sw3z ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s(): invalid pak pointers", __func__ );
	}
#endif
	while ( hpaksCount_sw3z >= MAX_CACHED_SW3Z_HANDLES ) {
		// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — invariant: hpaksCount_sw3z > 0 implies hhead_sw3z != NULL
		pack_t *pk = hhead_sw3z->prev_h_sw3z; // tail item
#ifdef _DEBUG
		if ( PACK_FILE_HANDLE(pk) == NULL ) {
			Com_Terminate( TERM_CLIENT_DROP, "%s(): invalid pak FILE handle", __func__ );
		}
#endif
		fclose( PACK_FILE_HANDLE(pk) );
		PACK_FILE_HANDLE(pk) = NULL;
		FS_RemoveFromHandleList_SW3Z( pk );
	}

	if ( hhead_sw3z == NULL ) {
		pak->next_h_sw3z = pak;
		pak->prev_h_sw3z = pak;
	} else {
		hhead_sw3z->prev_h_sw3z->next_h_sw3z = pak;
		pak->prev_h_sw3z = hhead_sw3z->prev_h_sw3z;
		hhead_sw3z->prev_h_sw3z = pak;
		pak->next_h_sw3z = hhead_sw3z;
	}

	hhead_sw3z = pak;
	hpaksCount_sw3z++;
}


/* Move pak to LRU head ("touch" on access). If the pak is not in the
 * LRU yet, this is a no-op — the caller is responsible for ensuring
 * an open FILE* and adding via FS_AddToHandleList_SW3Z. */
static void FS_TouchHandleList_SW3Z( pack_t *pak )
{
	if ( pak->next_h_sw3z == NULL ) {
		return;	// not in LRU
	}
	if ( hhead_sw3z == pak ) {
		return;	// already at head
	}
	FS_RemoveFromHandleList_SW3Z( pak );
	FS_AddToHandleList_SW3Z( pak );
}

#endif // FEAT_SW3Z

#endif


/*
==============
FS_FCloseFile

If the FILE pointer is an open pak file, leave it open.

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
void FS_FCloseFile( fileHandle_t f ) {
	fileHandleData_t *fd;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	fd = &fsh[ f ];

#if FEAT_SW3Z
	if ( fd->sw3zData ) {
		Z_Free( fd->sw3zData );
		fd->sw3zData = NULL;
	}
	/* fall through to memset cleanup at bottom */
#endif

	if ( fd->zipFile && fd->pak ) {
		unzCloseCurrentFile( fd->handleFiles.file.z );
		if ( fd->handleFiles.unique ) {
			unzClose( fd->handleFiles.file.z );
		}
		fd->handleFiles.file.z = NULL;
		fd->zipFile = qfalse;
		fd->pak->handleUsed--;
#ifdef USE_HANDLE_CACHE
		if ( fd->pak->handleUsed == 0 ) {
			FS_AddToHandleList( fd->pak );
		}
#else
		if ( !fs_locked->integer ) {
			if ( PACK_ZIP_HANDLE(fd->pak) && !fd->pak->handleUsed ) {
				unzClose( PACK_ZIP_HANDLE(fd->pak) );
				PACK_ZIP_HANDLE(fd->pak) = NULL;
			}
		}
#endif
	} else {
		if ( fd->handleFiles.file.o ) {
			fclose( fd->handleFiles.file.o );
			fd->handleFiles.file.o = NULL;
		}
	}

	memset( fd, 0, sizeof( *fd ) );
}


/*
===========
FS_ResetReadOnlyAttribute
===========
*/
qboolean FS_ResetReadOnlyAttribute( const char *filename ) {
	char *ospath;

	ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename );

	return Sys_ResetReadOnlyAttribute( ospath );
}


/*
===========
FS_FOpenFileWrite
===========
*/
fileHandle_t FS_FOpenFileWrite( const char *filename ) {
	char			*ospath;
	fileHandle_t	f;
	fileHandleData_t *fd;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( !filename || !*filename ) {
		return FS_INVALID_HANDLE;
	}

	ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_FOpenFileWrite: %s\n", ospath );
	}

	FS_CheckFilenameIsNotAllowed( ospath, __func__, qfalse );

	f = FS_HandleForFile();
	fd = &fsh[ f ];
	FS_InitHandle( fd );

	// enabling the following line causes a recursive function call loop
	// when running with +set logfile 1 +set developer 1
	//Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "writing to: %s\n", ospath );
	fd->handleFiles.file.o = Sys_FOpen( ospath, "wb" );
	if ( fd->handleFiles.file.o == NULL ) {
		if ( FS_CreatePath( ospath ) ) {
			return FS_INVALID_HANDLE;
		}
		fd->handleFiles.file.o = Sys_FOpen( ospath, "wb" );
		if ( fd->handleFiles.file.o == NULL ) {
			return FS_INVALID_HANDLE;
		}
	}

	Q_strncpyz( fd->name, filename, sizeof( fd->name ) );
	fd->handleSync = qfalse;
	fd->zipFile = qfalse;

	return f;
}


/*
===========
FS_FOpenFileAppend
===========
*/
fileHandle_t FS_FOpenFileAppend( const char *filename ) {
	char			*ospath;
	fileHandleData_t *fd;
	fileHandle_t	f;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( !*filename ) {
		return FS_INVALID_HANDLE;
	}

#ifndef DEDICATED
	// don't let sound stutter
	// S_ClearSoundBuffer();
#endif

	ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_FOpenFileAppend: %s\n", ospath );
	}

	FS_CheckFilenameIsNotAllowed( ospath, __func__, qfalse );

	f = FS_HandleForFile();
	fd = &fsh[ f ];
	FS_InitHandle( fd );

	fd->handleFiles.file.o = Sys_FOpen( ospath, "ab" );
	if ( fd->handleFiles.file.o == NULL ) {
		if ( FS_CreatePath( ospath ) ) {
			return FS_INVALID_HANDLE;
		}
		fd->handleFiles.file.o = Sys_FOpen( ospath, "ab" );
		if ( fd->handleFiles.file.o == NULL ) {
			return FS_INVALID_HANDLE;
		}
	}

	Q_strncpyz( fd->name, filename, sizeof( fd->name ) );
	fd->handleSync = qfalse;
	fd->zipFile = qfalse;

	return f;
}


/*
===========
FS_FilenameCompare

Ignore case and separator char distinctions
===========
*/
qboolean FS_FilenameCompare( const char *s1, const char *s2 ) {
	int		c1, c2;

	do {
		c1 = (byte)*s1++;
		c2 = (byte)*s2++;

		if ( c1 <= 'Z' && c1 >= 'A' )
			c1 += ('a' - 'A');
		else if ( c1 == '\\' || c1 == ':' )
			c1 = '/';

		if ( c2 <= 'Z' && c2 >= 'A' )
			c2 += ('a' - 'A');
		else if ( c2 == '\\' || c2 == ':' )
			c2 = '/';

		if ( c1 != c2 ) {
			return qtrue;		// strings not equal
		}
	} while ( c1 );

	return qfalse;		// strings are equal
}


/*
===========
FS_IsExt

Return qtrue if ext matches file extension filename
===========
*/
static qboolean FS_IsExt( const char *filename, const char *ext, size_t namelen )
{
	size_t extlen = strlen( ext );

	if ( extlen > namelen )
		return qfalse;

	filename += namelen - extlen;

	return !Q_stricmp( filename, ext );
}


/*
===========
FS_StripExt
===========
*/
qboolean FS_StripExt( char *filename, const char *ext )
{
	int extlen = strlen( ext );
	int namelen = strlen( filename );

	if ( extlen > namelen )
		return qfalse;

	filename += namelen - extlen;

	if ( !Q_stricmp( filename, ext ) )
	{
		filename[0] = '\0';
		return qtrue;
	}

	return qfalse;
}


static const char *FS_HasExt( const char *fileName, const char **extList, int extCount )
{
	const char *e = strrchr( fileName, '.' );

	if ( !e )
		return NULL;

	e++;
	for ( int i = 0; i < extCount; i++ )
	{
		if ( !Q_stricmp( e, extList[i] ) )
			return e;
	}

	return NULL;
}


static qboolean FS_GeneralRef( const char *filename )
{
	// allowed non-ref extensions
	static const char *extList[] = { "config", "shader", "shaderx", "mtr", "arena", "menu", "bot", "cfg", "txt" };

	if ( FS_HasExt( filename, extList, ARRAY_LEN( extList ) ) )
		return qfalse;

	if ( !Q_stricmp( filename, "vm/qagame.wasm" ) )
		return qfalse;

	if ( strstr( filename, "levelshots" ) )
		return qfalse;

	return qtrue;
}


/*
===========
FS_BypassPure
===========
*/
static int numServerPaks;
void FS_BypassPure( void )
{
	numServerPaks = fs_numServerPaks;
	fs_numServerPaks = 0;
}


/*
===========
FS_RestorePure
===========
*/
void FS_RestorePure( void )
{
	fs_numServerPaks = numServerPaks;
}


#if FEAT_SW3Z
/*
===========
FS_OpenFileInSW3Z

Decompress an SW3Z entry into a memory-backed file handle.
The entire file is decompressed upfront into sw3zData.
FS_Read then does a simple memcpy from that buffer.
===========
*/
static int FS_OpenFileInSW3Z( fileHandle_t *file, pack_t *pak, fileInPack_t *pakFile ) {
	fileHandleData_t *f;
	int entryIdx = (int)pakFile->pos;
	int size = (int)pakFile->size;

	{
		const char *compName;
		byte comp = pak->entries[entryIdx].compression;
		switch ( comp ) {
			case 0: compName = "none"; break;
			case 1: compName = "lz4"; break;
			case 2: compName = "zstd"; break;
			default: compName = "other"; break;
		}
		// Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "SW3Z_Open: '%s' entry=%d size=%d compression=%s\n",
		// 	pakFile->name, entryIdx, size, compName );
	}

#ifdef USE_HANDLE_CACHE
	/* SW3Z handle LRU integration. Two cases:
	 *  (a) FILE* is open (pack has been touched recently or never
	 *      evicted): touch to MRU position so the LRU keeps it.
	 *  (b) FILE* was evicted (LRU pressure from other sw3z packs):
	 *      lazy reopen and re-register. The pack's pakHashTable entry
	 *      and parsed metadata (entries[], buildBuffer, etc.) survive
	 *      eviction — only the FILE* needed to re-fopen. */
	if ( PACK_FILE_HANDLE(pak) == NULL ) {
		PACK_FILE_HANDLE(pak) = fopen( pak->pakFilename, "rb" );
		if ( PACK_FILE_HANDLE(pak) == NULL ) {
			Com_Log( SEV_WARN, LOG_CH(ch_filesystem),
				"FS_OpenFileInSW3Z: lazy reopen failed for '%s'\n",
				pak->pakFilename );
			*file = FS_INVALID_HANDLE;
			return -1;
		}
		FS_AddToHandleList_SW3Z( pak );
	} else {
		FS_TouchHandleList_SW3Z( pak );
	}
#endif

	if ( size == 0 ) {
		/* empty file / directory entry */
		*file = FS_HandleForFile();
		f = &fsh[ *file ];
		FS_InitHandle( f );
		f->zipFile = qfalse;
		f->handleFiles.file.o = NULL;
		f->sw3zData = NULL;
		f->sw3zSize = 0;
		f->sw3zPos = 0;
		Q_strncpyz( f->name, pakFile->name, sizeof( f->name ) );
		return 0;
	}

	*file = FS_HandleForFile();
	f = &fsh[ *file ];
	FS_InitHandle( f );

	if ( fs_sw3z_deferOpen ) {
		/* Phase 4-#4 deferred path: skip the Z_Malloc'd intermediate
		 * sw3zData buffer entirely. FS_ReadFile will Hunk-allocate the
		 * caller's return buffer next, and the very next FS_Read on
		 * this handle will decompress straight into it. */
		f->sw3zData     = NULL;
		f->sw3zEntryIdx = entryIdx;
		f->pak          = pak;
	} else {
		/* Default path: decompress the entire entry up-front into a
		 * Z_Malloc'd buffer. FS_Read then memcpy's slices on demand;
		 * FS_Seek can rewind freely. Required for VM-callable reads
		 * and for callers that issue partial / repeated reads. */
		f->sw3zData = (byte *)Z_Malloc( size );
		if ( SW3Z_ReadEntry( pak, entryIdx, f->sw3zData, size ) != size ) {
			Z_Free( f->sw3zData );
			f->sw3zData = NULL;
			*file = FS_INVALID_HANDLE;
			return -1;
		}
	}

	f->sw3zSize = size;
	f->sw3zPos = 0;
	f->zipFile = qfalse;
	f->handleFiles.file.o = NULL;
	Q_strncpyz( f->name, pakFile->name, sizeof( f->name ) );


	/* reference tracking */
	if ( !( pak->referenced & FS_GENERAL_REF ) && FS_GeneralRef( pakFile->name ) ) {
		pak->referenced |= FS_GENERAL_REF;
	}

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_FOpenFileRead: %s (found in '%s' [sw3z])\n",
			pakFile->name, pak->pakFilename );
	}

	return size;
}
#endif

static int FS_OpenFileInPak( fileHandle_t *file, pack_t *pak, fileInPack_t *pakFile, qboolean uniqueFILE ) {
	fileHandleData_t *f;
	unz_s *zfi;
	FILE *temp;

	// mark the pak as having been referenced and mark specifics on cgame and ui
	// these are loaded from all pk3s
	// from every pk3 file.

	if ( !( pak->referenced & FS_GENERAL_REF ) && FS_GeneralRef( pakFile->name ) ) {
		pak->referenced |= FS_GENERAL_REF;
	}
	if ( !( pak->referenced & FS_CGAME_REF ) && !strcmp( pakFile->name, "vm/cgame.wasm" ) ) {
		pak->referenced |= FS_CGAME_REF;
	}

	if ( !PACK_ZIP_HANDLE(pak) ) {
		PACK_ZIP_HANDLE(pak) = unzOpen( pak->pakFilename );
		if ( !PACK_ZIP_HANDLE(pak) ) {
			COM_ERROR( LOG_CH(ch_filesystem), "Error opening %s@%s\n", pak->pakBasename, pakFile->name );
			*file = FS_INVALID_HANDLE;
			return -1;
		}
	}

	if ( uniqueFILE ) {
		// open a new file on the pakfile
		temp = unzReOpen( pak->pakFilename, PACK_ZIP_HANDLE(pak) );
		if ( temp == NULL ) {
			COM_ERROR( LOG_CH(ch_filesystem), "Couldn't reopen %s", pak->pakFilename );
			*file = FS_INVALID_HANDLE;
			return -1;
		}
	} else {
		temp = PACK_ZIP_HANDLE(pak);
	}

	*file = FS_HandleForFile();
	f = &fsh[ *file ];
	FS_InitHandle( f );

	f->zipFile = qtrue;
	f->handleFiles.file.z = temp;
	f->handleFiles.unique = uniqueFILE;

	Q_strncpyz( f->name, pakFile->name, sizeof( f->name ) );
	zfi = (unz_s *)f->handleFiles.file.z;
	// in case the file was new
	temp = zfi->file;
	// set the file position in the zip file (also sets the current file info)
	unzSetCurrentFileInfoPosition( PACK_ZIP_HANDLE(pak), pakFile->pos );
	// copy the file info into the unzip structure
	memcpy( zfi, PACK_ZIP_HANDLE(pak), sizeof( *zfi ) );
	// we copy this back into the structure
	zfi->file = temp;
	// open the file in the zip
	unzOpenCurrentFile( f->handleFiles.file.z );
	f->zipFilePos = pakFile->pos;
	f->zipFileLen = pakFile->size;
	f->pakIndex = pak->index;
	fs_lastPakIndex = pak->index;
	f->pak = pak;

#ifdef USE_HANDLE_CACHE
	if ( pak->next_h ) {
		FS_RemoveFromHandleList( pak );
	}
#endif

	pak->handleUsed++;

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_FOpenFileRead: %s (found in '%s')\n",
			pakFile->name, pak->pakFilename );
	}

	return zfi->cur_file_info.uncompressed_size;
}


/*
===========
FS_FOpenFileRead

Finds the file in the search path.
Returns filesize and an open FILE pointer.
Used for streaming data out of either a
separate file or a ZIP file.
===========
*/
extern qboolean		com_fullyInitialized;

int FS_FOpenFileRead( const char *filename, fileHandle_t *file, qboolean uniqueFILE ) {
	const searchpath_t	*search;
	char			*netpath;
	pack_t			*pak;
	fileInPack_t	*pakFile;
	directory_t		*dir;
	long			hash;
	long			fullHash;
	FILE			*temp;
	int				length;
	fileHandleData_t *f;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( !filename ) {
		Com_Terminate( TERM_UNRECOVERABLE, "FS_FOpenFileRead: NULL 'filename' parameter passed\n" );
	}

	// qpaths are not supposed to have a leading slash
	if ( filename[0] == '/' || filename[0] == '\\' ) {
		filename++;
	}

	// make absolutely sure that it can't back up the path.
	// The searchpaths do guarantee that something will always
	// be prepended, so we don't need to worry about "c:" or "//limbo"
	if ( FS_CheckDirTraversal( filename ) ) {
		if (file) {
			*file = FS_INVALID_HANDLE;
		}
		return -1;
	}

	// we will calculate full hash only once then just mask it by current pack->hashSize
	// we can do that as long as we know properties of our hash function
	fullHash = FS_HashFileName( filename, 0U );

	if ( file == NULL ) {
		// just wants to see if file is there
		for ( search = fs_searchpaths ; search ; search = search->next ) {
			// is the element a pak file?
			if ( search->pack && search->pack->hashTable[ (hash = fullHash & (search->pack->hashSize-1)) ] ) {
				// skip non-pure files
				if ( !FS_PakIsPure( search->pack ) )
					continue;
				// look through all the pak file elements
				pak = search->pack;
				pakFile = pak->hashTable[hash];
				do {
					// case and separator insensitive comparisons
					if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
						// found it!
						return pakFile->size;
					}
					pakFile = pakFile->next;
				} while ( pakFile != NULL );
			}
			else if ( search->dir && search->policy != DIR_DENY ) {
				dir = search->dir;
				netpath = FS_BuildOSPath( dir->path, dir->gamedir, filename );
				temp = Sys_FOpen( netpath, "rb" );
				if ( temp ) {
					length = FS_FileLength( temp );
					fclose( temp );
					return length;
				}
			}
		}
		return -1;
	}

	//
	// search through the path, one element at a time
	//
	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( search->pack && search->pack->hashTable[ (hash = fullHash & (search->pack->hashSize-1)) ] ) {
			// disregard if it doesn't match one of the allowed pure pak files
			if ( !FS_PakIsPure( search->pack ) ) {
				continue;
			}
			// look through all the pak file elements
			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				// case and separator insensitive comparisons
				if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
					// found it!
#if FEAT_SW3Z
					if ( pak->type == PACK_SW3Z )
						return FS_OpenFileInSW3Z( file, pak, pakFile );
#endif
					return FS_OpenFileInPak( file, pak, pakFile, uniqueFILE );
				}
				pakFile = pakFile->next;
			} while ( pakFile != NULL );
		}
		else if ( search->dir && search->policy != DIR_DENY ) {
			// check a file in the directory tree
			dir = search->dir;

			netpath = FS_BuildOSPath( dir->path, dir->gamedir, filename );

			temp = Sys_FOpen( netpath, "rb" );
			if ( temp == NULL ) {
				continue;
			}

			*file = FS_HandleForFile();
			f = &fsh[ *file ];
			FS_InitHandle( f );

			f->handleFiles.file.o = temp;
			Q_strncpyz( f->name, filename, sizeof( f->name ) );
			f->zipFile = qfalse;

			if ( fs_debug->integer ) {
				Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_FOpenFileRead: %s (found in '%s/%s')\n", filename,
					dir->path, dir->gamedir );
			}

			return FS_FileLength( f->handleFiles.file.o );
		}
	}

#ifdef FS_MISSING
	if ( missingFiles ) {
		fprintf( missingFiles, "%s\n", filename );
	}
#endif

	*file = FS_INVALID_HANDLE;
	return -1;
}


/*
===========
FS_TouchFileInPak
===========
*/
void FS_TouchFileInPak( const char *filename ) {
	const searchpath_t *search;
	long			fullHash, hash;
	pack_t			*pak;
	fileInPack_t	*pakFile;

	fullHash = FS_HashFileName( filename, 0U );

	for ( search = fs_searchpaths ; search ; search = search->next ) {

		// is the element a pak file?
		if ( !search->pack )
			continue;

		if ( search->pack->exclude ) // skip paks in \fs_excludeReference list
			continue;

		if ( search->pack->hashTable[ (hash = fullHash & (search->pack->hashSize-1)) ] ) {
			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				// case and separator insensitive comparisons
				if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
					// found it!
					if ( !( pak->referenced & FS_GENERAL_REF ) && FS_GeneralRef( filename ) ) {
						pak->referenced |= FS_GENERAL_REF;
					}
					if ( !( pak->referenced & FS_CGAME_REF ) && !strcmp( filename, "vm/cgame.wasm" ) ) {
						pak->referenced |= FS_CGAME_REF;
					}
					return;
				}
				pakFile = pakFile->next;
			} while ( pakFile != NULL );
		}
	}
}


/*
===========
FS_Home_FOpenFileRead
===========
*/
int FS_Home_FOpenFileRead( const char *filename, fileHandle_t *file )
{
	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	// should never happen but for safe
	if ( !file ) {
		return -1;
	}

	// allocate new file handle
	fileHandle_t f = FS_HandleForFile();
	fileHandleData_t *fd = &fsh[ f ];
	FS_InitHandle( fd );

	char path[ MAX_OSPATH*3 + 1 ];
	Com_sprintf( path, sizeof( path ), "%s%c%s%c%s", fs_homepath->string,
		PATH_SEP, fs_gamedir, PATH_SEP, filename );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "%s: %s\n", __func__, path );
	}

	fd->handleFiles.file.o = Sys_FOpen( path, "rb" );
	if ( fd->handleFiles.file.o != NULL ) {
		Q_strncpyz( fd->name, filename, sizeof( fd->name ) );
		fd->handleSync = qfalse;
		fd->zipFile = qfalse;
		*file = f;
		return FS_FileLength( fd->handleFiles.file.o );
	}

	*file = FS_INVALID_HANDLE;
	return -1;
}


/*
=================
FS_Read

Properly handles partial reads
=================
*/
int FS_Read( void *buffer, int len, fileHandle_t f ) {
	int		block, remaining;
	int		read;
	byte	*buf;
	int		tries;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( len < 0 ) {
		Com_Terminate( TERM_UNRECOVERABLE, "FS_Read: len < 0");
	}

	if ( f <= 0 || f >= MAX_FILE_HANDLES ) {
		return 0;
	}

	buf = (byte *)buffer;
	fs_readCount += len;

#if FEAT_SW3Z
	if ( fsh[f].sw3zData ) {
		if ( len <= 0 )
			return 0;
		int avail = fsh[f].sw3zSize - fsh[f].sw3zPos;
		if ( len > avail )
			len = avail;
		memcpy( buffer, fsh[f].sw3zData + fsh[f].sw3zPos, len );
		fsh[f].sw3zPos += len;
		return len;
	}
	if ( fsh[f].sw3zEntryIdx >= 0 ) {
		/* Phase 4-#4 deferred decompression. FS_ReadFile is the only
		 * caller that triggers this state and it always reads the full
		 * entry in one shot. Require len >= sw3zSize so we can decompress
		 * straight into the caller's buffer; partial reads on a deferred
		 * handle would leave state inconsistent (no place to store the
		 * remainder), so reject them. */
		if ( len < fsh[f].sw3zSize ) {
			Com_Log( SEV_WARN, LOG_CH(ch_filesystem),
				"FS_Read: partial read on deferred SW3Z handle (len=%d size=%d) — rejecting\n",
				len, fsh[f].sw3zSize );
			return 0;
		}
		int decompressed = SW3Z_ReadEntry( fsh[f].pak, fsh[f].sw3zEntryIdx, buffer, fsh[f].sw3zSize );
		fsh[f].sw3zEntryIdx = -1;	/* consume the deferred state */
		if ( decompressed != fsh[f].sw3zSize ) {
			fsh[f].sw3zPos = fsh[f].sw3zSize;	/* mark EOF so subsequent reads return 0 */
			return -1;
		}
		fsh[f].sw3zPos = fsh[f].sw3zSize;
		return decompressed;
	}
#endif

	if ( !fsh[f].zipFile ) {
		if ( !fsh[f].handleFiles.file.o ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), S_COLOR_YELLOW "FS_Read: NULL file pointer for handle %i (%s)\n", f, fsh[f].name );
			return 0;
		}
		remaining = len;
		tries = 0;
		while (remaining) {
			block = remaining;
			read = fread( buf, 1, block, fsh[f].handleFiles.file.o );
			if (read == 0) {
				// we might have been trying to read from a CD, which
				// sometimes returns a 0 read on windows
				if (!tries) {
					tries = 1;
				} else {
					return len-remaining;	//Com_Terminate( TERM_UNRECOVERABLE, "FS_Read: 0 bytes read");
				}
			}

			if (read == -1) {
				Com_Terminate( TERM_UNRECOVERABLE, "FS_Read: -1 bytes read");
			}

			remaining -= read;
			buf += read;
		}
		return len;
	}
	return unzReadCurrentFile( fsh[f].handleFiles.file.z, buffer, len );
}


/*
=================
FS_Write

Properly handles partial writes
=================
*/
int FS_Write( const void *buffer, int len, fileHandle_t h ) {
	int		block, remaining;
	int		written;
	byte	*buf;
	int		tries;
	FILE	*f;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( len < 0 ) {
		Com_Terminate( TERM_UNRECOVERABLE, "FS_Write: len < 0");
	}

	//if ( h <= 0 || h >= MAX_FILE_HANDLES ) {
	//	return 0;
	//}

	f = FS_FileForHandle(h);
	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		written = fwrite (buf, 1, block, f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			} else {
				Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_Write: 0 bytes written\n" );
				return 0;
			}
		}

		if (written == -1) {
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_Write: -1 bytes written\n" );
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	if ( fsh[h].handleSync ) {
		fflush( f );
	}
	return len;
}

void QDECL FS_Printf( fileHandle_t h, const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	FS_Write(msg, strlen(msg), h);
}

#define PAK_SEEK_BUFFER_SIZE 65536

/*
=================
FS_Seek

=================
*/
int FS_Seek( fileHandle_t f, long offset, fsOrigin_t origin ) {
	int		_origin;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
		return -1;
	}

#if FEAT_SW3Z
	if ( fsh[f].sw3zData ) {
		long newPos;
		switch ( origin ) {
		case FS_SEEK_SET: newPos = offset; break;
		case FS_SEEK_CUR: newPos = fsh[f].sw3zPos + offset; break;
		case FS_SEEK_END: newPos = fsh[f].sw3zSize + offset; break;
		default:
			Com_Terminate( TERM_UNRECOVERABLE, "Bad origin in FS_Seek" );
			return -1;
		}
		if ( newPos < 0 ) newPos = 0;
		if ( newPos > fsh[f].sw3zSize ) newPos = fsh[f].sw3zSize;
		fsh[f].sw3zPos = (int)newPos;
		return 0;
	}
#endif

	if ( fsh[f].zipFile == qtrue ) {
		int		currentPosition = unztell( fsh[f].handleFiles.file.z );
		long	targetOffset;
		long	remainder;
		byte	buffer[PAK_SEEK_BUFFER_SIZE];

		switch ( origin ) {
		case FS_SEEK_SET:
			targetOffset = offset;
			break;
		case FS_SEEK_CUR:
			targetOffset = (long)currentPosition + offset;
			break;
		case FS_SEEK_END:
			targetOffset = (long)fsh[f].zipFileLen + offset;
			break;
		default:
			Com_Terminate( TERM_UNRECOVERABLE, "Bad origin in FS_Seek" );
			return -1;
		}
		if ( targetOffset < 0 ) targetOffset = 0;
		if ( targetOffset > fsh[f].zipFileLen ) targetOffset = fsh[f].zipFileLen;

		if ( targetOffset < currentPosition ) {
			remainder = targetOffset;
			unzSetCurrentFileInfoPosition( fsh[f].handleFiles.file.z, fsh[f].zipFilePos );
			unzOpenCurrentFile( fsh[f].handleFiles.file.z );
		} else {
			remainder = targetOffset - currentPosition;
		}

		while ( remainder > 0 ) {
			int r = unzReadCurrentFile( fsh[f].handleFiles.file.z, buffer, MIN( remainder, PAK_SEEK_BUFFER_SIZE ) );
			if ( r < 0 ) {
				return r;
			}
			remainder -= r;
		}
		return 0;
	}
	FILE *file;
	file = FS_FileForHandle( f );
	switch ( origin ) {
	case FS_SEEK_CUR:
		_origin = SEEK_CUR;
		break;
	case FS_SEEK_END:
		_origin = SEEK_END;
		break;
	case FS_SEEK_SET:
		_origin = SEEK_SET;
		break;
	default:
		Com_Terminate( TERM_UNRECOVERABLE, "Bad origin in FS_Seek" );
		return -1;
	}

	return fseek( file, offset, _origin );
}


/*
======================================================================================

CONVENIENCE FUNCTIONS FOR ENTIRE FILES

======================================================================================
*/

qboolean FS_FileIsInPAK( const char *filename, int *pChecksum, char *pakName ) {
	const searchpath_t	*search;
	const pack_t		*pak;
	const fileInPack_t	*pakFile;
	long			hash;
	long			fullHash;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( !filename ) {
		Com_Terminate( TERM_UNRECOVERABLE, "FS_FOpenFileRead: NULL 'filename' parameter passed" );
	}

	// qpaths are not supposed to have a leading slash
	while ( filename[0] == '/' || filename[0] == '\\' )
		filename++;

	// make absolutely sure that it can't back up the path.
	// The searchpaths do guarantee that something will always
	// be prepended, so we don't need to worry about "c:" or "//limbo"
	if ( FS_CheckDirTraversal( filename ) ) {
		return qfalse;
	}

	fullHash = FS_HashFileName( filename, 0U );

	//
	// search through the path, one element at a time
	//
	for ( search = fs_searchpaths ; search ; search = search->next ) {

		// is the element a pak file?
		if ( search->pack && search->pack->hashTable[ (hash = fullHash & (search->pack->hashSize-1)) ] ) {
			// disregard if it doesn't match one of the allowed pure pak files
			//if ( !FS_PakIsPure( search->pack ) ) {
			//	continue;
			//}
			//
			if ( search->pack->exclude ) {
				continue;
			}

			// look through all the pak file elements
			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				// case and separator insensitive comparisons
				if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
					if ( pChecksum ) {
						*pChecksum = pak->pure_checksum;
					}
					if ( pakName ) {
						Com_sprintf( pakName, MAX_OSPATH, "%s/%s", pak->pakGamename, pak->pakBasename );
					}
					return qtrue;
				}
				pakFile = pakFile->next;
			} while ( pakFile != NULL );
		}
	}
	return qfalse;
}


/*
============
FS_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_ReadFile( const char *qpath, void **buffer ) {
	fileHandle_t	h;
	byte*			buf;
	qboolean		isConfig;
	long			len;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( qpath == NULL || qpath[0] == '\0' ) {
		Com_Terminate( TERM_UNRECOVERABLE, "FS_ReadFile with empty name" );
	}

	buf = NULL;	// quiet compiler warning
	isConfig = qfalse;

	// if this is a .cfg file and we are playing back a journal, read
	// it from the journal file
	if ( com_journalDataFile != FS_INVALID_HANDLE && strstr( qpath, ".cfg" ) ) {
		if ( com_journal->integer == 2 ) {
			int		r;

			Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "Loading %s from journal file.\n", qpath );
			r = FS_Read( &len, sizeof( len ), com_journalDataFile );
			if ( r != sizeof( len ) ) {
				if (buffer != NULL) *buffer = NULL;
				return -1;
			}
			// if the file didn't exist when the journal was created
			if (!len) {
				if (buffer == NULL) {
					return 1;			// hack for old journal files
				}
				*buffer = NULL;
				return -1;
			}
			if (buffer == NULL) {
				return len;
			}

			buf = Hunk_AllocateTempMemory(len+1);
			*buffer = buf;

			r = FS_Read( buf, len, com_journalDataFile );
			if ( r != len ) {
				Com_Terminate( TERM_UNRECOVERABLE, "Read from journalDataFile failed" );
			}

			fs_loadCount++;
			fs_loadStack++;

			// guarantee that it will have a trailing 0 for string operations
			buf[len] = '\0';

			return len;
		}
		if ( com_journal->integer == 1 ) {
			isConfig = qtrue;
		}
	}

	// look for it in the filesystem or pack files
	//
	// Phase 4-#4: ask the SW3Z opener to defer decompression. We're
	// going to Hunk-allocate the caller's return buffer immediately
	// after this call returns the entry size, then trigger decompression
	// directly into that buffer via FS_Read — eliminating the Z_Malloc'd
	// sw3zData intermediate that the default open path would use. The
	// flag is a no-op for PK3 / on-disk reads.
#if FEAT_SW3Z
	fs_sw3z_deferOpen = qtrue;
#endif
	len = FS_FOpenFileRead( qpath, &h, qfalse );
#if FEAT_SW3Z
	fs_sw3z_deferOpen = qfalse;
#endif
	if ( h == FS_INVALID_HANDLE ) {
		if ( buffer ) {
			*buffer = NULL;
		}
		// if we are journaling and it is a config file, write a zero to the journal file
		if ( isConfig ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "Writing zero for %s to journal file.\n", qpath );
			len = 0;
			FS_Write( &len, sizeof( len ), com_journalDataFile );
			FS_Flush( com_journalDataFile );
		}
		return -1;
	}

	if ( !buffer ) {
		if ( isConfig ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "Writing len for %s to journal file.\n", qpath );
			FS_Write( &len, sizeof( len ), com_journalDataFile );
			FS_Flush( com_journalDataFile );
		}
		FS_FCloseFile( h );
		return len;
	}

	buf = Hunk_AllocateTempMemory( len + 1 );

	if ( FS_Read( buf, len, h ) != len ) {
		Hunk_FreeTempMemory( buf );
		FS_FCloseFile( h );
		return -1;
	}

	*buffer = buf;

	fs_loadCount++;
	fs_loadStack++;

	// guarantee that it will have a trailing 0 for string operations
	buf[ len ] = '\0';
	FS_FCloseFile( h );

	// if we are journaling and it is a config file, write it to the journal file
	if ( isConfig ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "Writing %s to journal file.\n", qpath );
		FS_Write( &len, sizeof( len ), com_journalDataFile );
		FS_Write( buf, len, com_journalDataFile );
		FS_Flush( com_journalDataFile );
	}
	return len;
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile( void *buffer ) {
	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}
	if ( !buffer ) {
		Com_Terminate( TERM_UNRECOVERABLE, "FS_FreeFile( NULL )" );
	}
	fs_loadStack--;

	Hunk_FreeTempMemory( buffer );

	// if all of our temp files are free, clear all of our space
	if ( fs_loadStack == 0 ) {
		Hunk_ClearTempMemory();
	}
}


/*
============
FS_WriteFile

Filename are relative to the quake search path
============
*/
void FS_WriteFile( const char *qpath, const void *buffer, int size ) {
	fileHandle_t f;

	if ( !qpath || !buffer ) {
		Com_Terminate( TERM_UNRECOVERABLE, "FS_WriteFile: NULL parameter" );
	}

	f = FS_FOpenFileWrite( qpath );
	if ( f == FS_INVALID_HANDLE ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "Failed to open %s\n", qpath );
		return;
	}

	FS_Write( buffer, size, f );

	FS_FCloseFile( f );
}



/*
==========================================================================

ZIP FILE LOADING

==========================================================================
*/
static int FS_PakHashSize( const int filecount )
{
	int hashSize = 2;

	for ( ; hashSize < MAX_FILEHASH_SIZE; hashSize <<= 1 ) {
		if ( hashSize >= filecount ) {
			break;
		}
	}

	return hashSize;
}


/*
============
FS_BannedPakFile

Check if file should NOT be added to hash search table
============
*/
static qboolean FS_BannedPakFile( const char *filename )
{
	if ( !strcmp( filename, "autoexec.cfg" ) || !strcmp( filename, WIRED_CONFIG_CFG ) )
		return qtrue;
	return qfalse;
}


/*
=================
FS_ConvertFilename

lower case and replace '\\' ':' with '/'
=================
*/
static void FS_ConvertFilename( char *name )
{
	int c;
	while ( (c = (byte)*name) != '\0' ) {
		if ( c <= 'Z' && c >= 'A' ) {
			*name = c - 'A' + 'a';
		} else if ( c == '\\' || c == ':' ) {
			*name = '/';
		}
		name++;
	}
}


#ifdef USE_PAK_CACHE

#define PAK_HASH_SIZE 512

static void FS_FreePak( pack_t *pak );

static pack_t *pakHashTable[ PAK_HASH_SIZE ];

#ifdef USE_PAK_CACHE_FILE

#define CACHE_FILE_NAME "pakcache.dat"

#define CACHE_SYNC_CONDITION ( fs_paksReaded + fs_paksSkipped + fs_paksReleased >= 8 )

static int fs_paksCached;	// readed from cache file
static int fs_paksSkipped;	// outdated/non-existent cache file pk3 entries

static int fs_paksReaded;	// actually readed from the disk
static int fs_paksReleased;	// unreferenced paks since last FS restart

static qboolean fs_cacheLoaded = qfalse;
static qboolean fs_cacheSynced = qtrue;

#pragma pack( push, 1 )

// platform-specific 4-byte signature:
// 0: [version] anything following depends from it
// 1: [endianess] 0 - LSB, 1 - MSB
// 2: [path separation] '/' or '\\'
// 3: [size of file offset and file time]
// non-matching header will cause whole file being ignored
static const byte cache_header[ 4 ] = {
#if FEAT_SW3Z
	3, //version 3 — Phase 4 adds maxCompressedSize to the SW3Z tail
	   //              so disk-cache-loaded packs know the scratch
	   //              buffer cap without re-walking entries.
	   //          v2 was Phase 2: full SW3Z record persistence with
	   //              length-prefixed format tail (entries[]).
	   //          v1 was Phase 1: packType field present but only PK3
	   //              records on disk; SW3Z packs lived in-memory only.
#else
	0, //version 0 — pre-FEAT_SW3Z, no packType field
#endif
#if !Q_BIG_ENDIAN
	0x0,
#else
	0x1,
#endif
	PATH_SEP,
	( ( sizeof( fileOffset_t ) - 1 ) << 4 ) | ( sizeof( fileTime_t ) - 1 )
};

typedef struct pakcacheHeader_s {
	int pakNameLen;		// full path
	int namesLen;
	int numFiles;
	int numHeaderLongs; // including first uninitialized
	int contentLen;
	fileTime_t ctime;	// creation/status change time
	fileTime_t mtime;	// modification time
	fileOffset_t size;	// zip file size
#if FEAT_SW3Z
	int packType;		// 0 = PACK_PK3, 1 = PACK_SW3Z
#endif
} pakcacheHeader_t;

typedef struct pakcacheFileItem_s {
	unsigned long name; // offset in namebuffer
	unsigned long size;
	unsigned long pos;	// info position in pk3 file
} pakcacheFileItem_t;

#pragma pack( pop )

#endif // USE_PAK_CACHE_FILE


static int FS_HashPak( const char *name )
{
	unsigned int c, hash = 0;
	while ( (c = (byte)*name++) != '\0' )
	{
		hash = hash * 101 + c;
	}
	hash = hash ^ (hash >> 16);
	return hash & (PAK_HASH_SIZE-1);
}


static pack_t *FS_FindInCache( const char *zipfile )
{
	pack_t *pack;
	unsigned int hash;

	hash = FS_HashPak( zipfile );
	pack = pakHashTable[ hash ];
	while ( pack )
	{
		if ( !strcmp( zipfile, pack->pakFilename ) )
		{
			return pack;
		}
		pack = pack->next;
	}

	return NULL;
}


static void FS_AddToCache( pack_t *pack )
{
	pack->namehash = FS_HashPak( pack->pakFilename );
	pack->next = pakHashTable[ pack->namehash ];
	pack->prev = NULL;
	if ( pakHashTable[ pack->namehash ] )
		pakHashTable[ pack->namehash ]->prev = pack;
	pakHashTable[ pack->namehash ] = pack;
}


static void FS_RemoveFromCache( pack_t *pack )
{
	if ( !pack->next && !pack->prev && pakHashTable[ pack->namehash ] != pack )
	{
		Com_Terminate( TERM_UNRECOVERABLE, "Invalid pak link" );
	}

	if ( pack->prev != NULL )
		pack->prev->next = pack->next;
	else
		pakHashTable[ pack->namehash ] = pack->next;

	if ( pack->next != NULL )
		pack->next->prev = pack->prev;
}


static pack_t *FS_LoadCachedPak( const char *pakfile )
{
	fileOffset_t size;
	fileTime_t mtime;
	fileTime_t ctime;
	pack_t *pak;

	pak = FS_FindInCache( pakfile );
	if ( pak == NULL )
		return NULL;

	if ( !Sys_GetFileStats( pakfile, &size, &mtime, &ctime ) )
	{
		FS_RemoveFromCache( pak );
		FS_FreePak( pak );
		return NULL;
	}

	if ( pak->size != size || pak->mtime != mtime || pak->ctime != ctime )
	{
		// release outdated information
		FS_RemoveFromCache( pak );
		FS_FreePak( pak );
		return NULL;
	}

	return pak;
}


static void FS_InsertPakToCache( pack_t *pak )
{
	if ( Sys_GetFileStats( pak->pakFilename, &pak->size, &pak->mtime, &pak->ctime ) )
	{
		FS_AddToCache( pak );
		pak->touched = qtrue;
	}
}


static void FS_ResetCacheReferences( void )
{
	// pakHashTable is `pack_t *[PAK_HASH_SIZE]`; iterate by the known size
	// directly rather than via ARRAY_LEN's `sizeof(arr)/sizeof(arr[0])`,
	// which clang-tidy's bugprone-sizeof-expression flags because
	// sizeof(pack_t *) is "sizeof(A*); pointer to aggregate". The math is
	// correct but the lint is noisy — using the constant is clearer anyway.
	for ( int i = 0; i < PAK_HASH_SIZE; i++ )
	{
		pack_t *pak = pakHashTable[ i ];
		while ( pak )
		{
			pak->touched = qfalse;
			pak->referenced = 0;
			pak = pak->next;
		}
	}
}


static void FS_FreeUnusedCache( void )
{
	// See FS_ResetCacheReferences for why this uses PAK_HASH_SIZE directly.
	for ( int i = 0; i < PAK_HASH_SIZE; i++ )
	{
		pack_t *pak = pakHashTable[ i ];
		while ( pak )
		{
			pack_t *next = pak->next;
			if ( !pak->touched )
			{
				FS_RemoveFromCache( pak );
				FS_FreePak( pak );
#ifdef USE_PAK_CACHE_FILE
				fs_paksReleased++;
#endif
			}
			pak = next;
		}
	}
}

#ifdef USE_PAK_CACHE_FILE

static void FS_WriteCacheHeader( FILE *f )
{
	fwrite( cache_header, sizeof( cache_header ), 1, f );
}


static qboolean FS_ValidateCacheHeader( FILE *f )
{
	byte buf[ sizeof(cache_header) ];

	if ( fread( buf, sizeof( buf ), 1, f ) != 1 )
		return qfalse;

	if ( memcmp( buf, cache_header, sizeof( buf ) ) != 0 )
		return qfalse;

	return qtrue;
}


static qboolean FS_SavePackToFile( const pack_t *pak, FILE *f )
{
	/* Skip unknown formats — they'll re-parse from the source archive
	 * on next session via their native loader. New formats opt in by
	 * adding a positive type check below and (if needed) appending a
	 * length-prefixed format-specific tail after the common record. */
	if ( pak->type != PACK_PK3
#if FEAT_SW3Z
	     && pak->type != PACK_SW3Z
#endif
	   )
		return qtrue;

	const char *namePtr = (char*)(pak->buildBuffer + pak->numfiles);
	const char *pakName = pak->pakFilename;
	int pakNameLen = PAD( (int)strlen( pakName ) + 1, sizeof( int ) );
	pakcacheHeader_t pk;
	pakcacheFileItem_t it;
	int namesLen = pakName - namePtr;

	// file content length
	int contentLen = 0;
#if 0
	for ( i = 0; i < pak->numfiles; i++ )
	{
		if ( pak->buildBuffer[ i ].data && pak->buildBuffer[ i ].size )
		{
			contentLen += sizeof( int ) + PAD( pak->buildBuffer[ i ].size, sizeof( int ) );
		}
	}
#endif

	// pak filename length
	pk.pakNameLen = pakNameLen;
	// filenames length
	pk.namesLen = namesLen;
	// number of files
	pk.numFiles = pak->numfiles;
	// number of checksums
	pk.numHeaderLongs = pak->numHeaderLongs;
	// content of some files
	pk.contentLen = contentLen;
	// creation/status change time
	pk.ctime = pak->ctime;
	// modification time
	pk.mtime = pak->mtime;
	// pak file size
	pk.size = pak->size;
#if FEAT_SW3Z
	// archive format type
	pk.packType = (int)pak->type;
#endif

	// dump header
	fwrite( &pk, sizeof( pk ), 1, f );

	// pak filename
	fwrite( pakName, pakNameLen, 1, f );

	// filenames
	fwrite( namePtr, namesLen, 1, f );

	// file entries
	for ( int i = 0; i < pak->numfiles; i++ )
	{
		it.name = (unsigned long)(pak->buildBuffer[i].name - namePtr);
		it.size = pak->buildBuffer[i].size;
		it.pos = pak->buildBuffer[i].pos;
		fwrite( &it, sizeof( it ), 1, f );
	}

	// pure checksums, excluding first uninitialized
	fwrite( pak->headerLongs + 1, (pak->numHeaderLongs - 1) * sizeof( pak->headerLongs[0] ), 1, f );

#if 0
	if ( contentLen )
	{
		const fileInPack_t *currFile = pak->buildBuffer;
		for ( i = 0; i < pak->numfiles; i++, currFile++ )
		{
			if ( currFile->data && currFile->size ) {
				// file index
				fwrite( &i, sizeof( i ), 1, f );
				// file data
				fwrite( currFile->data, PAD( currFile->size, sizeof( int ) ), 1, f );
			}
		}
	}
#endif

#if FEAT_SW3Z
	if ( pak->type == PACK_SW3Z )
	{
		/* Format-specific tail, length-prefixed so the stat-mismatch
		 * skip path and any future-format reader can advance past it
		 * without knowing the layout. The runtime fields persisted are
		 * the minimum needed for SW3Z_ReadEntry: the global data offset,
		 * the per-pack compScratch cap (Phase 4-#2), and the per-entry
		 * compression metadata. The SW3Z stringTable is intentionally
		 * NOT persisted — at runtime, file names live in the inline
		 * namebuffer (already part of the common record); stringTable
		 * is only consulted during the cold-load name copy which the
		 * cache hit skips. */
		const uint32_t entryCount        = (uint32_t)pak->entryCount;
		const uint32_t dataOffLo         = (uint32_t)pak->dataOffset;
		const uint32_t maxCompressedSize = (uint32_t)pak->maxCompressedSize;
		const uint32_t entriesLen        = entryCount * (uint32_t)sizeof( sw3zEntry_t );
		const uint32_t tailLen           = (uint32_t)( 3 * sizeof( uint32_t ) ) + entriesLen;

		fwrite( &tailLen,           sizeof( tailLen           ), 1, f );
		fwrite( &dataOffLo,         sizeof( dataOffLo         ), 1, f );
		fwrite( &entryCount,        sizeof( entryCount        ), 1, f );
		fwrite( &maxCompressedSize, sizeof( maxCompressedSize ), 1, f );
		if ( entryCount > 0 && pak->entries )
			fwrite( pak->entries, entriesLen, 1, f );
	}
#endif

	return qtrue;
}


static qboolean FS_LoadPakFromFile( FILE *f )
{
	fileTime_t ctime, mtime;
	fileOffset_t fsize;
	fileInPack_t *curFile;
	char pakName[ PAD( MAX_OSPATH*3+1, sizeof( int ) ) ];
	char pakBase[ PAD( MAX_OSPATH, sizeof( int ) ) ], *basename;
	char *filename_inzip;
	pakcacheHeader_t pk;
	pakcacheFileItem_t it;
	pack_t *pack;
	char *namePtr;
	int size, i;
	int pakBaseLen;
	int hashSize;
	long hash;
#if FEAT_SW3Z
	qboolean is_sw3z;
#endif

	if ( fread( &pk, sizeof( pk ), 1, f ) != 1 )
		return qfalse; // probably EOF

	// validate header data

	if ( pk.pakNameLen > sizeof( pakName ) || pk.pakNameLen & 3 || pk.pakNameLen == 0 )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "bad pakNameLen: %08X\n", pk.pakNameLen );
		return qfalse;
	}

	if ( pk.namesLen & 3 || pk.namesLen < pk.numFiles )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "bad namesLen: %i\n", pk.namesLen );
		return qfalse;
	}

	if ( pk.numHeaderLongs == 0 || pk.numHeaderLongs > pk.numFiles + 1 )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "bad numHeaderLongs: %i\n", pk.numHeaderLongs );
		return qfalse;
	}

	if ( pk.contentLen & 3 || pk.contentLen < 0 )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "bad contentLen: %i\n", pk.contentLen );
		return qfalse;
	}

#if FEAT_SW3Z
	/* Format gate: PK3 and SW3Z are reconstructable from disk. Any
	 * unknown packType means this cache file was written by a future
	 * engine version with format support we lack — bail on the entire
	 * file (we can't know the tail size to skip). The cache will be
	 * rebuilt with our format vocabulary. */
	if ( (packType_t)pk.packType != PACK_PK3
	     && (packType_t)pk.packType != PACK_SW3Z )
	{
		return qfalse;
	}
	is_sw3z = ( (packType_t)pk.packType == PACK_SW3Z );
#endif

	// load filename
	if ( fread( pakName, pk.pakNameLen, 1, f ) != 1 )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "error reading pakname\n" );
		return qfalse;
	}

	// pakName must be zero-terminated
	if ( pakName[ pk.pakNameLen - 1 ] != '\0' )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "pakname is not zero-terminated!\n" );
		return qfalse;
	}

	if ( !Sys_GetFileStats( pakName, &fsize, &mtime, &ctime ) || fsize != pk.size || mtime != pk.mtime || ctime != pk.ctime )
	{
		const int seek_len = pk.namesLen + pk.numFiles * sizeof( it ) + (pk.numHeaderLongs-1) * sizeof( pack->headerLongs[0] ) + pk.contentLen;
		if ( fseek( f, seek_len, SEEK_CUR ) != 0 )
		{
			return qfalse;
		}

#if FEAT_SW3Z
		/* SW3Z records have a length-prefixed format tail after the
		 * common body — read its length and skip past it so the next
		 * record starts at the correct offset. */
		if ( (packType_t)pk.packType == PACK_SW3Z )
		{
			uint32_t tailLen;
			if ( fread( &tailLen, sizeof( tailLen ), 1, f ) != 1 )
				return qfalse;
			if ( fseek( f, (long)tailLen, SEEK_CUR ) != 0 )
				return qfalse;
		}
#endif

		fs_paksSkipped++;
		return qtrue; // just outdated info, we can continue
	}

	// extract basename from zip path
	basename = strrchr( pakName, PATH_SEP );
	if ( basename == NULL )
		basename = pakName;
	else
		basename++;

	Q_strncpyz( pakBase, basename, sizeof( pakBase ) );
#if FEAT_SW3Z
	// Use the cached header field — `pack` is not allocated until below (Z_TagMalloc),
	// so `pack->type` here was reading uninitialized memory before the fix.
	if ( (packType_t)pk.packType == PACK_SW3Z )
		FS_StripExt( pakBase, ".sw3z" );
	else
#endif
		FS_StripExt( pakBase, ".pk3" );
	pakBaseLen = (int) strlen( pakBase ) + 1;
	pakBaseLen = PAD( pakBaseLen, sizeof( int ) );

	hashSize = FS_PakHashSize( pk.numFiles );

	/* PK3 packs embed headerLongs inline — the whole pack is a single
	 * Z_TagMalloc'd block freed in one Z_Free. SW3Z packs match
	 * SW3Z_LoadArchive's layout: headerLongs is separately Z_Malloc'd
	 * because SW3Z_CloseArchive unconditionally Z_Frees it. Mixing
	 * inline+separate would double-free or free-of-inline. */
	size = sizeof( *pack ) + pk.namesLen + pk.numFiles * sizeof( pack->buildBuffer[0] );
	// pack->hashTable is `fileInPack_t **` — each slot stores a pointer.
	// Using `sizeof(fileInPack_t *)` rather than `sizeof(pack->hashTable[0])`
	// keeps clang-tidy's bugprone-sizeof-expression quiet (the latter flags
	// as "sizeof(A*); pointer to aggregate") and documents intent: we're
	// reserving room for `hashSize` pointers, not for fileInPack_t structs.
	size += hashSize * sizeof( fileInPack_t * );
	size += pk.pakNameLen;
	size += pakBaseLen;
	size += pk.numHeaderLongs * sizeof( pack->headerLongs[0] );
#if FEAT_SW3Z
	/* SW3Z packs use a separately-allocated headerLongs (matching
	 * SW3Z_LoadArchive); back out the inline reservation. */
	if ( is_sw3z )
		size -= pk.numHeaderLongs * sizeof( pack->headerLongs[0] );
#endif

	pack = Z_TagMalloc( size, TAG_PACK );
	memset( pack, 0, size );

#if FEAT_SW3Z
	pack->type = (packType_t)pk.packType;
#else
	pack->type = PACK_PK3;
#endif
	pack->mtime = pk.mtime;
	pack->ctime = pk.ctime;
	pack->size = pk.size;

//	PACK_ZIP_HANDLE(pack) = uf;
	pack->numfiles = pk.numFiles;
	pack->numHeaderLongs = pk.numHeaderLongs;

	// setup memory layout
	pack->hashSize = hashSize;
	pack->hashTable = (fileInPack_t **)( pack + 1 );

	pack->buildBuffer = (fileInPack_t*)( pack->hashTable + pack->hashSize );

	namePtr = (char*)( pack->buildBuffer + pack->numfiles );

	pack->pakFilename = (char*)( namePtr + pk.namesLen );
	pack->pakBasename = (char*)( pack->pakFilename + pk.pakNameLen );
	pack->headerLongs = (int*)( pack->pakBasename + pakBaseLen );
#if FEAT_SW3Z
	/* For SW3Z, leave pack->headerLongs NULL; it will be separately
	 * Z_Malloc'd below, before the headerLongs fread. If __error fires
	 * before that allocation, FS_FreePak → SW3Z_CloseArchive checks
	 * for NULL and skips the free. */
	if ( is_sw3z )
		pack->headerLongs = NULL;
#endif

	strcpy( pack->pakFilename, pakName );
	strcpy( pack->pakBasename, pakBase );

	if ( fread( namePtr, pk.namesLen, 1, f ) != 1 )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "error reading pak filenames\n" );
		goto __error;
	}

	// filenames buffer must be zero-terminated
	if ( namePtr[ pk.namesLen - 1 ] != '\0' )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "not zero terminated filenames\n" );
		goto __error;
	}

	curFile = pack->buildBuffer;
	for ( i = 0; i < pk.numFiles; i++ )
	{
		if ( fread( &it, sizeof( it ), 1, f ) != 1 )
		{
			//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "error reading file item[%i]\n", i );
			goto __error;
		}
		if ( it.name >= pk.namesLen )
		{
			//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "bad name offset: %i (expecting less than %i)\n", it.name, pk.namesLen );
			goto __error;
		}

		filename_inzip = namePtr + it.name;
		FS_ConvertFilename( filename_inzip );
		if ( !FS_BannedPakFile( filename_inzip ) ) {
			// store the file position in the zip
			curFile->name = filename_inzip;
			curFile->size = it.size;
			curFile->pos = it.pos;

			// update hash table
			hash = FS_HashFileName( filename_inzip, pack->hashSize );
			curFile->next = pack->hashTable[ hash ];
			pack->hashTable[ hash ] = curFile;
			curFile++;
		} else {
			pack->numfiles--;
		}
	}

#if FEAT_SW3Z
	/* Allocate SW3Z's separately-tracked headerLongs now that the
	 * items read loop has succeeded — keeping the failure window
	 * narrow so the __error path doesn't have to distinguish "before
	 * Z_Malloc" from "after". SW3Z_CloseArchive frees this on cleanup. */
	if ( is_sw3z )
	{
		pack->headerLongs = (int *)Z_Malloc( pack->numHeaderLongs * sizeof( int ) );
	}
#endif

	if ( fread( pack->headerLongs + 1, ( pack->numHeaderLongs - 1 ) * sizeof( pack->headerLongs[0] ), 1, f ) != 1 )
	{
		//Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "error reading headerLongs\n" );
		goto __error;
	}

	pack->checksumFeed = fs_checksumFeed;
	pack->headerLongs[ 0 ] = LittleLong( fs_checksumFeed );

	pack->checksum = Com_BlockChecksum( pack->headerLongs + 1, sizeof( pack->headerLongs[0] ) * ( pack->numHeaderLongs - 1 ) );
	pack->checksum = LittleLong( pack->checksum );

	pack->pure_checksum = Com_BlockChecksum( pack->headerLongs, sizeof( pack->headerLongs[0] ) * pack->numHeaderLongs );
	pack->pure_checksum = LittleLong( pack->pure_checksum );

	// seek through unused content
	if ( pk.contentLen > 0 )
	{
		if ( fseek( f, pk.contentLen, SEEK_CUR ) != 0 )
			goto __error;
	}
	else if ( pk.contentLen < 0 )
	{
		goto __error;
	}

#if FEAT_SW3Z
	if ( is_sw3z )
	{
		/* Format-specific tail (cache v3): dataOffset + entryCount +
		 * maxCompressedSize + entries[]. stringTable is intentionally
		 * not persisted — runtime file-name lookups go through the
		 * inline namebuffer above; the SW3Z stringTable is only
		 * consulted during the cold-load name copy, which the cache
		 * hit skips. */
		uint32_t tailLen, dataOffLo, entryCount, maxCompressedSize;
		uint32_t entriesLen, expectedTailLen;

		if ( fread( &tailLen,           sizeof( tailLen           ), 1, f ) != 1 )
			goto __error;
		if ( fread( &dataOffLo,         sizeof( dataOffLo         ), 1, f ) != 1 )
			goto __error;
		if ( fread( &entryCount,        sizeof( entryCount        ), 1, f ) != 1 )
			goto __error;
		if ( fread( &maxCompressedSize, sizeof( maxCompressedSize ), 1, f ) != 1 )
			goto __error;

		entriesLen       = entryCount * (uint32_t)sizeof( sw3zEntry_t );
		expectedTailLen  = (uint32_t)( 3 * sizeof( uint32_t ) ) + entriesLen;
		if ( tailLen != expectedTailLen )
		{
			Com_Log( SEV_WARN, LOG_CH(ch_filesystem),
				"FS_LoadPakFromFile: SW3Z tail length mismatch for '%s' (got %u, expected %u)\n",
				pakName, tailLen, expectedTailLen );
			goto __error;
		}

		pack->dataOffset        = (unsigned long)dataOffLo;
		pack->entryCount        = entryCount;
		pack->maxCompressedSize = maxCompressedSize;
		pack->compScratch       = NULL;
		pack->compScratchSize   = 0;
		pack->stringTable       = NULL;
		pack->stringTableSize   = 0;

		if ( entryCount > 0 )
		{
			pack->entries = (sw3zEntry_t *)Z_Malloc( entriesLen );
			if ( fread( pack->entries, entriesLen, 1, f ) != 1 )
				goto __error;
		}
		else
		{
			pack->entries = NULL;
		}

		/* SW3Z runtime needs an open FILE* for entry data reads. The
		 * fresh-load path opens this in SW3Z_LoadArchive; the cache
		 * hit must open it here. On failure, FS_FreePak →
		 * SW3Z_CloseArchive handles the partially-built pack. */
		PACK_FILE_HANDLE(pack) = fopen( pakName, "rb" );
		if ( !PACK_FILE_HANDLE(pack) )
		{
			Com_Log( SEV_WARN, LOG_CH(ch_filesystem),
				"FS_LoadPakFromFile: SW3Z fopen failed for '%s'\n", pakName );
			goto __error;
		}

#ifdef USE_HANDLE_CACHE
		/* Register the freshly-opened FILE* in the SW3Z handle LRU. */
		FS_AddToHandleList_SW3Z( pack );
#endif
	}
#endif

	fs_paksCached++;

	FS_InsertPakToCache( pack );

	return qtrue;

__error:
	FS_FreePak( pack );
	return qfalse;
}


/*
============
FS_SaveCache

Called at the end of FS_Startup() after releasing unused paks
============
*/
static qboolean FS_SaveCache( void )
{
	const char *filename = CACHE_FILE_NAME;

	if ( !fs_searchpaths )
		return qfalse;

	if ( !fs_cacheLoaded )
	{
		Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "synced FS cache on startup\n" );
		fs_cacheSynced = qfalse;
		fs_cacheLoaded = qtrue;
	}
	else if ( CACHE_SYNC_CONDITION )
	{
		Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "synced FS cache on readed=%i, released=%i, skipped=%i\n",
			fs_paksReaded, fs_paksReleased, fs_paksSkipped );
		fs_cacheSynced = qfalse;
	}

	if ( fs_cacheSynced )
		return qtrue;

	const searchpath_t *sp = fs_searchpaths;
	const char *ospath = FS_BuildOSPath( fs_homepath->string, filename, NULL );
	FILE *f = Sys_FOpen( ospath, "wb" );
	if ( f == NULL )
		return qfalse;

	FS_WriteCacheHeader( f );

	while ( sp != NULL )
	{
		if ( sp->pack )
		{
			FS_SavePackToFile( sp->pack, f );
		}
		sp = sp->next;
	}

	fclose( f );

	fs_paksReleased = 0;
	fs_paksSkipped = 0;
	fs_paksReaded = 0;

	fs_cacheSynced = qtrue;

	return qtrue;
}


/*
============
FS_LoadCache

Called at FS_Startup() before loading any pk3 file
============
*/
static void FS_LoadCache( void )
{
	const char *filename = CACHE_FILE_NAME;

	fs_paksReaded = 0;
	fs_paksReleased = 0;

	if ( fs_cacheLoaded )
		return;

	fs_paksCached = 0;
	fs_paksSkipped = 0;

	const char *ospath = FS_BuildOSPath( fs_homepath->string, filename, NULL );
	FILE *f = Sys_FOpen( ospath, "rb" );
	if ( f == NULL )
		return;

	if ( !FS_ValidateCacheHeader( f ) )
	{
		fclose( f );
		return;
	}

	while ( FS_LoadPakFromFile( f ) )
		;

	fclose( f );

	fs_cacheLoaded = qtrue;

}

#endif // USE_PAK_CACHE_FILE

#endif // USE_PAK_CACHE


/*
=================
FS_LoadZipFile

Creates a new pak_t in the search chain for the contents
of a zip file.
=================
*/
static pack_t *FS_LoadZipFile( const char *zipfile )
{
	fileInPack_t	*curFile;
	pack_t			*pack;
	unzFile			uf;
	int				err;
	unz_global_info gi;
	char			filename_inzip[MAX_ZPATH];
	unz_file_info	file_info;
	unsigned int	i, namelen, hashSize, size;
	long			hash;
	int				fs_numHeaderLongs;
	int				*fs_headerLongs;
	int				filecount;
	char			*namePtr;
	const char		*basename;
	int				fileNameLen;
	int				baseNameLen;

#ifdef USE_PAK_CACHE
	pack = FS_LoadCachedPak( zipfile );
	if ( pack )
	{
		// update pure checksum
		if ( pack->checksumFeed != fs_checksumFeed )
		{
			pack->headerLongs[ 0 ] = LittleLong( fs_checksumFeed );
			pack->pure_checksum = Com_BlockChecksum( pack->headerLongs, sizeof( pack->headerLongs[0] ) * pack->numHeaderLongs );
			pack->pure_checksum = LittleLong( pack->pure_checksum );
			pack->checksumFeed = fs_checksumFeed;
		}

		pack->touched = qtrue;
		return pack; // loaded from cache
	}
#endif

	// extract basename from zip path
	basename = strrchr( zipfile, PATH_SEP );
	if ( basename == NULL ) {
		basename = zipfile;
	} else {
		basename++;
	}

	fileNameLen = (int) strlen( zipfile ) + 1;
	baseNameLen = (int) strlen( basename ) + 1;

	uf = unzOpen( zipfile );
	err = unzGetGlobalInfo( uf, &gi );

	if ( err != UNZ_OK ) {
		return NULL;
	}

	namelen = 0;
	filecount = 0;
	unzGoToFirstFile( uf );
	for (i = 0; i < gi.number_entry; i++)
	{
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		filename_inzip[sizeof(filename_inzip)-1] = '\0';
		if (err != UNZ_OK) {
			break;
		}
		if ( file_info.compression_method != 0 && file_info.compression_method != 8 /*Z_DEFLATED*/ ) {
			COM_WARN( LOG_CH(ch_filesystem), "%s|%s: unsupported compression method %i\n", basename, filename_inzip, (int)file_info.compression_method );
			unzGoToNextFile( uf );
			continue;
		}
		namelen += strlen( filename_inzip ) + 1;
		unzGoToNextFile( uf );
		filecount++;
	}

	if ( filecount == 0 ) {
		unzClose( uf );
		return NULL;
	}

	// get the hash table size from the number of files in the zip
	// because lots of custom pk3 files have less than 32 or 64 files
	hashSize = FS_PakHashSize( filecount );

	namelen = PAD( namelen, sizeof( int ) );
	// hashTable slots are pointers — see size calc in FS_LoadZipFile / similar
	// site for the rationale on `sizeof(fileInPack_t *)` vs the indexed form.
	size = sizeof( *pack ) + hashSize * sizeof( fileInPack_t * ) + filecount * sizeof( pack->buildBuffer[0] ) + namelen;
	size += PAD( fileNameLen, sizeof( int ) );
	size += PAD( baseNameLen, sizeof( int ) );
#ifdef USE_PAK_CACHE
	size += ( filecount + 1 ) * sizeof( fs_headerLongs[0] );
#endif
	pack = Z_TagMalloc( size, TAG_PACK );
	memset( pack, 0, size );

	pack->type = PACK_PK3;
	PACK_ZIP_HANDLE(pack) = uf;
	pack->numfiles = filecount;
	pack->hashSize = hashSize;
	pack->hashTable = (fileInPack_t **)( pack + 1 );

	pack->buildBuffer = (fileInPack_t*)( pack->hashTable + pack->hashSize );
	namePtr = (char*)( pack->buildBuffer + filecount );

	pack->pakFilename = (char*)( namePtr + namelen );
	pack->pakBasename = (char*)( pack->pakFilename + PAD( fileNameLen, sizeof( int ) ) );

#ifdef USE_PAK_CACHE
	fs_headerLongs = (int*)( pack->pakBasename + PAD( baseNameLen, sizeof( int ) ) );
#else
	fs_headerLongs = Z_Malloc( ( filecount + 1 ) * sizeof( fs_headerLongs[0] ) );
#endif

	fs_numHeaderLongs = 0;
	fs_headerLongs[ fs_numHeaderLongs++ ] = LittleLong( fs_checksumFeed );

	memcpy( pack->pakFilename, zipfile, fileNameLen );
	memcpy( pack->pakBasename, basename, baseNameLen );

	// strip .pk3 if needed
	FS_StripExt( pack->pakBasename, ".pk3" );

	unzGoToFirstFile( uf );
	curFile = pack->buildBuffer;
	for ( i = 0; i < gi.number_entry; i++ )
	{
		err = unzGetCurrentFileInfo( uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0 );
		filename_inzip[sizeof(filename_inzip)-1] = '\0';
		if (err != UNZ_OK) {
			break;
		}
		if ( file_info.compression_method != 0 && file_info.compression_method != 8 /*Z_DEFLATED*/ ) {
			unzGoToNextFile( uf );
			continue;
		}
		if ( file_info.uncompressed_size > 0 ) {
			fs_headerLongs[fs_numHeaderLongs++] = LittleLong( file_info.crc );
		}

		FS_ConvertFilename( filename_inzip );
		if ( !FS_BannedPakFile( filename_inzip ) ) {
			// store the file position in the zip
			unzGetCurrentFileInfoPosition( uf, &curFile->pos );
			curFile->size = file_info.uncompressed_size;
			curFile->name = namePtr;
			strcpy( curFile->name, filename_inzip );
			namePtr += strlen( filename_inzip ) + 1;

			// update hash table
			hash = FS_HashFileName( filename_inzip, pack->hashSize );
			curFile->next = pack->hashTable[ hash ];
			pack->hashTable[ hash ] = curFile;
			curFile++;
		} else {
			pack->numfiles--;
		}

		unzGoToNextFile( uf );
	}

	pack->checksum = Com_BlockChecksum( fs_headerLongs + 1, sizeof( fs_headerLongs[0] ) * ( fs_numHeaderLongs - 1 ) );
	pack->checksum = LittleLong( pack->checksum );

	pack->pure_checksum = Com_BlockChecksum( fs_headerLongs, sizeof( fs_headerLongs[0] ) * fs_numHeaderLongs );
	pack->pure_checksum = LittleLong( pack->pure_checksum );

#ifdef USE_PAK_CACHE
	pack->headerLongs = fs_headerLongs;
	pack->numHeaderLongs = fs_numHeaderLongs;
	pack->checksumFeed = fs_checksumFeed;
#else
	Z_Free( fs_headerLongs );
#endif

#ifdef USE_HANDLE_CACHE
	FS_AddToHandleList( pack );
#else
	if ( fs_locked->integer == 0 )
	{
		unzClose( PACK_ZIP_HANDLE(pack) );
		PACK_ZIP_HANDLE(pack) = NULL;
	}
#endif

#ifdef USE_PAK_CACHE
	FS_InsertPakToCache( pack );
#ifdef USE_PAK_CACHE_FILE
	fs_paksReaded++;
#endif
#endif

	return pack;
}


/*
=================
FS_FreePak

Frees a pak structure and releases all associated resources.
Dispatches by pack->type:
  PACK_PK3  — unzClose the zip handle, disconnect from the handle LRU,
              then Z_Free the single Z_TagMalloc'd block.
  PACK_SW3Z — delegate to SW3Z_CloseArchive which owns its separately-
              allocated entries[]/stringTable/headerLongs + FILE handle.

The type check on each branch is intentional — it documents which
operations are format-specific and avoids running PK3-only logic
(unzClose) against a future pack format that might be added.
=================
*/
static void FS_FreePak( pack_t *pak )
{
#if FEAT_SW3Z
	if ( pak->type == PACK_SW3Z )
	{
#ifdef USE_HANDLE_CACHE
		/* Unlink from the SW3Z handle LRU before SW3Z_CloseArchive
		 * fcloses + Z_Frees — otherwise the LRU would hold a
		 * dangling pointer after the pack vanishes. The membership
		 * check guards against packs that were never registered
		 * (e.g., partially-built packs that errored before
		 * FS_AddToHandleList_SW3Z) and packs whose FILE* was
		 * evicted by an earlier LRU pressure event. */
		if ( pak->next_h_sw3z )
			FS_RemoveFromHandleList_SW3Z( pak );
#endif
		SW3Z_CloseArchive( pak );
		return;
	}
#endif

	if ( pak->type == PACK_PK3 )
	{
		if ( PACK_ZIP_HANDLE(pak) )
		{
#ifdef USE_HANDLE_CACHE
			if ( pak->next_h )
				FS_RemoveFromHandleList( pak );
#endif
			unzClose( PACK_ZIP_HANDLE(pak) );
			PACK_ZIP_HANDLE(pak) = NULL;
		}

		Z_Free( pak );
		return;
	}

	/* Unknown pack type — should not happen in well-formed flows.
	 * Log and free the raw allocation; format-specific resources
	 * (handles, side-allocations) will leak by definition since
	 * we don't know how to release them. */
	Com_Log( SEV_WARN, LOG_CH(ch_filesystem),
		"FS_FreePak: unknown pack type %d for '%s'\n",
		(int)pak->type, pak->pakFilename ? pak->pakFilename : "(null)" );
	Z_Free( pak );
}


/*
=================
FS_CompareZipChecksum

Compares whether the given pak file matches a referenced checksum
=================
*/
qboolean FS_CompareZipChecksum(const char *zipfile)
{
	pack_t *thepak = FS_LoadZipFile( zipfile );

	if ( !thepak )
		return qfalse;

	int checksum = thepak->checksum;
#ifndef USE_PAK_CACHE
	FS_FreePak(thepak);
#endif

	for(int index = 0; index < fs_numServerReferencedPaks; index++)
	{
		if(checksum == fs_serverReferencedPaks[index])
			return qtrue;
	}

	return qfalse;
}


/*
=================
FS_GetZipChecksum
=================
*/
int FS_GetZipChecksum( const char *zipfile )
{
	pack_t *pak = FS_LoadZipFile( zipfile );

	if ( !pak )
		return 0xFFFFFFFF;

	int checksum = pak->checksum;
#ifndef USE_PAK_CACHE
	FS_FreePak( pak );
#endif

	return checksum;
}


/*
=================================================================================

DIRECTORY SCANNING FUNCTIONS

=================================================================================
*/

static int FS_ReturnPath( const char *zname, char *zpath, int *depth ) {
	int len, at, newdep;

	newdep = 0;
	zpath[0] = '\0';
	len = 0;
	at = 0;

	while(zname[at] != 0)
	{
		if (zname[at]=='/' || zname[at]=='\\') {
			len = at;
			newdep++;
		}
		at++;
	}
	strcpy(zpath, zname);
	zpath[len] = '\0';
	*depth = newdep;

	return len;
}


char *FS_CopyString( const char *in ) {
	char *out;
	//out = S_Malloc( strlen( in ) + 1 );
	out = Z_Malloc( strlen( in ) + 1 );
	strcpy( out, in );
	return out;
}


/*
==================
FS_AddFileToList
==================
*/
static int FS_AddFileToList( const char *name, char **list, int nfiles ) {
	if ( nfiles == MAX_FOUND_FILES - 1 ) {
		return nfiles;
	}
	for ( int i = 0 ; i < nfiles ; i++ ) {
		if ( !Q_stricmp( name, list[i] ) ) {
			return nfiles; // already in list
		}
	}
	list[ nfiles ] = FS_CopyString( name );
	nfiles++;

	return nfiles;
}


/*
===============
FS_AllowListExternal
===============
*/
static qboolean FS_AllowListExternal( const char *extension )
{
	if ( !extension )
		return qfalse;

	if ( !Q_stricmp( extension, ".shader" ) )
		return qfalse;

	if ( !Q_stricmp( extension, ".shaderx" ) )
		return qfalse;

	if ( !Q_stricmp( extension, ".mtr" ) )
		return qfalse;

	return qtrue;
}

static fnamecallback_f fnamecallback = NULL;

void FS_SetFilenameCallback( fnamecallback_f func )
{
	fnamecallback = func;
}


/*
===============
FS_ListFilteredFiles

Returns a unique list of files that match the given criteria
from all search paths
===============
*/
static char **FS_ListFilteredFiles( const char *path, const char *extension, const char *filter, int *numfiles, int flags ) {
	int				nfiles;
	char			**listCopy;
	char			*list[MAX_FOUND_FILES];
	const searchpath_t	*search;
	int				i;
	int				pathLength;
	int				extLen;
	int				length, pathDepth, temp;
	pack_t			*pak;
	fileInPack_t	*buildBuffer;
	char			zpath[MAX_ZPATH];
	qboolean		hasPatterns;
	const char		*x;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if  ( fs_numServerPaks && ( flags & FS_MATCH_STICK ) == 0 ) {
		flags &= ~FS_MATCH_UNPURE;
		if ( !FS_AllowListExternal( extension ) ) {
			flags &= ~FS_MATCH_EXTERN;
		}
	}

	if ( !path ) {
		*numfiles = 0;
		return NULL;
	}

	if ( !extension ) {
		extension = "";
	}

	extLen = (int)strlen( extension );
	hasPatterns = Com_HasPatterns( extension );
	if ( hasPatterns && extension[0] == '.' && extension[1] != '\0' ) {
		extension++;
	}

	pathLength = strlen( path );
	if ( pathLength > 0 && ( path[pathLength-1] == '\\' || path[pathLength-1] == '/' ) ) {
		pathLength--;
	}

	nfiles = 0;
	FS_ReturnPath( path, zpath, &pathDepth );

	//
	// search through the path, one element at a time, adding to list
	//
	for ( search = fs_searchpaths; search; search = search->next ) {
		// is the element a pak file?
		if ( search->pack && ( flags & FS_MATCH_PAKS ) ) {

			//ZOID:  If we are pure, don't search for files on paks that
			// aren't on the pure list
			if ( !FS_PakIsPure( search->pack ) && !( flags & FS_MATCH_UNPURE ) ) {
				continue;
			}

			// look through all the pak file elements
			pak = search->pack;
			buildBuffer = pak->buildBuffer;
			for (i = 0; i < pak->numfiles; i++) {
				const char *name;
				int zpathLen, depth;

				// check for directory match
				name = buildBuffer[i].name;
				//
				if ( filter ) {
					// case insensitive
					if ( !Com_FilterPath( filter, name ) )
						continue;
					// unique the match
					nfiles = FS_AddFileToList( name, list, nfiles );
				}
				else {

					zpathLen = FS_ReturnPath(name, zpath, &depth);

					if ( (depth-pathDepth)>2 || pathLength > zpathLen || Q_stricmpn( name, path, pathLength ) ) {
						continue;
					}

					// check for extension match
					length = (int)strlen( name );

					if ( fnamecallback ) {
						// use custom filter
						if ( !fnamecallback( name, length ) )
							continue;
					} else {
						if ( length < extLen )
							continue;
						if ( *extension ) {
							if ( hasPatterns ) {
								x = strrchr( name, '.' );
								if ( !x || !Com_FilterExt( extension, x+1 ) ) {
									continue;
								}
							} else {
								if ( Q_stricmp( name + length - extLen, extension ) ) {
									continue;
								}
							}
						}
					}
					// unique the match

					temp = pathLength;
					if (pathLength) {
						temp++;		// include the '/'
					}
					nfiles = FS_AddFileToList( name + temp, list, nfiles );
				}
			}
		}
		else if ( search->dir && ( search->policy != DIR_DENY || (flags & FS_MATCH_EXTERN) != 0 ) ) { // scan for files in the filesystem
			const char *netpath;
			int		numSysFiles;
			char	**sysFiles;
			const char *name;

			netpath = FS_BuildOSPath( search->dir->path, search->dir->gamedir, path );
			sysFiles = Sys_ListFiles( netpath, extension, filter, &numSysFiles, (flags & FS_MATCH_SUBDIRS) ? FS_MAX_SUBDIRS : 0);
			for ( i = 0; i < numSysFiles; i++ ) {
				// unique the match
				name = sysFiles[ i ];
				length = strlen( name );
				if ( fnamecallback ) {
					// use custom filter
					if ( !fnamecallback( name, length ) )
						continue;
				} // else - should be already filtered by Sys_ListFiles

				nfiles = FS_AddFileToList( name, list, nfiles );
			}
			Sys_FreeFileList( sysFiles );
		}
	}

	// return a copy of the list
	*numfiles = nfiles;

	if ( nfiles == 0 ) {
		return NULL;
	}

	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( listCopy[0] ) );
	for ( i = 0; i < nfiles; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}


/*
=================
FS_ListFiles
=================
*/
char **FS_ListFiles( const char *path, const char *extension, int *numfiles )
{
	return FS_ListFilteredFiles( path, extension, NULL, numfiles, FS_MATCH_ANY );
}


/*
=================
FS_FreeFileList
=================
*/
void FS_FreeFileList( char **list ) {
	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( !list ) {
		return;
	}

	for ( int i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


/*
===================
FS_ListDirectories

Returns the names of all immediate subdirectories under `path`, merged across
all mounted backends (pk3, sw3z, filesystem).

Archive backends:  iterate stored file entries; for any entry whose path
begins with  <path>/<X>/  extract the single component <X>.  No explicit
directory entries needed.

Filesystem backend:  opendir/readdir via Sys_ListFiles(..., "/", ...).

Results are deduplicated, sorted alphabetically.  Caller frees via
FS_FreeFileList.  Returns NULL (and *numDirs = 0) when no directories found.
===================
*/
static void FS_SortFileList( char**, int );  // defined below

char **FS_ListDirectories( const char *path, int *numDirs ) {
	char			*list[MAX_FOUND_FILES];
	char			**listCopy;
	const searchpath_t	*search;
	int			nfiles = 0;
	int			pathLength;
	int			i;

	*numDirs = 0;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	if ( !path || !path[0] ) {
		return NULL;
	}

	pathLength = (int)strlen( path );
	// strip any trailing slash from the caller's path
	if ( path[pathLength-1] == '\\' || path[pathLength-1] == '/' ) {
		pathLength--;
	}

	for ( search = fs_searchpaths; search; search = search->next ) {

		if ( search->pack ) {
			pack_t		*pak   = search->pack;
			fileInPack_t	*buf   = pak->buildBuffer;
			int		n;

			for ( n = 0; n < pak->numfiles; n++ ) {
				const char *name = buf[n].name;
				const char *after, *slash;
				char		dirname[MAX_QPATH];
				int		dirLen;

				// entry must begin with "<path>/"
				if ( Q_stricmpn( name, path, pathLength ) != 0 ) continue;
				if ( name[pathLength] != '/' && name[pathLength] != '\\' ) continue;

				// the part after the prefix slash
				after = name + pathLength + 1;

				// must have another slash — meaning it's inside a subdirectory
				slash = strchr( after, '/' );
				if ( !slash ) continue;

				dirLen = (int)( slash - after );
				if ( dirLen <= 0 || dirLen >= (int)sizeof( dirname ) ) continue;

				Q_strncpyz( dirname, after, dirLen + 1 );
				nfiles = FS_AddFileToList( dirname, list, nfiles );
			}
		}
		else if ( search->dir && search->policy != DIR_DENY ) {
			const char	*netpath;
			char		**sysFiles;
			int		numSys;

			netpath  = FS_BuildOSPath( search->dir->path, search->dir->gamedir, path );
			sysFiles = Sys_ListFiles( netpath, "/", NULL, &numSys, 0 );
			for ( i = 0; i < numSys; i++ ) {
				// Sys_ListFiles with "/" returns bare names; skip . and ..
				if ( sysFiles[i][0] == '.' ) continue;
				nfiles = FS_AddFileToList( sysFiles[i], list, nfiles );
			}
			Sys_FreeFileList( sysFiles );
		}
	}

	if ( nfiles == 0 ) {
		return NULL;
	}

	if ( nfiles > 1 ) {
		FS_SortFileList( list, nfiles - 1 );
	}

	*numDirs = nfiles;
	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( listCopy[0] ) );
	for ( i = 0; i < nfiles; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}


/*
================
FS_GetFileList
================
*/
int	FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	char **pFiles = NULL;

	*listbuf = '\0';

	if (Q_stricmp(path, "$modlist") == 0) {
		return FS_GetModList(listbuf, bufsize);
	}

	int nFiles = 0, nTotal = 0;
	pFiles = FS_ListFiles(path, extension, &nFiles);

	for (int i = 0; i < nFiles; i++) {
		int nLen = strlen(pFiles[i]) + 1;
		if (nTotal + nLen + 1 < bufsize) {
			strcpy(listbuf, pFiles[i]);
			listbuf += nLen;
			nTotal += nLen;
		}
		else {
			nFiles = i;
			break;
		}
	}

	FS_FreeFileList(pFiles);

	return nFiles;
}


/*
=======================
Sys_ConcatenateFileLists

mkv: Naive implementation. Concatenates three lists into a
     new list, and frees the old lists from the heap.
bk001129 - from cvs1.17 (mkv)

FIXME TTimo those two should move to common.c next to Sys_ListFiles
=======================
 */
static unsigned int Sys_CountFileList( char **list )
{
	int i = 0;

	if ( list )
	{
		while ( *list )
		{
			list++;
			i++;
		}
	}

	return i;
}


static char** FS_ConcatenateFileLists( char **list0, char **list1 )
{
	int totalLength;
	char **src, **dst, **cat;

	totalLength = Sys_CountFileList( list0 );
	totalLength += Sys_CountFileList( list1 );

	/* Create new list. */
	dst = cat = Z_Malloc( ( totalLength + 1 ) * sizeof( char* ) );

	/* Copy over lists. */
	if ( list0 )
	{
		for (src = list0; *src; src++, dst++)
			*dst = *src;
	}

	if ( list1 )
	{
		for ( src = list1; *src; src++, dst++ )
			*dst = *src;
	}

	// Terminate the list
	*dst = NULL;

	// Free our old lists.
	// NOTE: not freeing their content, it's been merged in dst and still being used
	if ( list0 ) Z_Free( list0 );
	if ( list1 ) Z_Free( list1 );

	return cat;
}


/*
================
FS_GetModDescription
================
*/
static void FS_GetModDescription( const char *modDir, char *description, int descriptionLen ) {
	fileHandle_t	descHandle;
	char			descPath[MAX_QPATH];
	int				nDescLen;

	Com_sprintf( descPath, sizeof ( descPath ), "%s%cdescription.txt", modDir, PATH_SEP );
	FS_ReplaceSeparators( descPath );
	nDescLen = FS_SV_FOpenFileRead( descPath, &descHandle );

	if ( descHandle != FS_INVALID_HANDLE ) {
		if ( nDescLen > 0 ) {
			if ( nDescLen > descriptionLen - 1 )
				nDescLen = descriptionLen - 1;
			nDescLen = FS_Read( description, nDescLen, descHandle );
			if ( nDescLen >= 0 ) {
				description[ nDescLen ] = '\0';
				while ( nDescLen > 0 && description[ nDescLen - 1 ] == '\n' ) {
					// strip ending newlines
					description[ nDescLen - 1 ] = '\0';
					nDescLen--;
				}
			}
		} else {
			Q_strncpyz( description, modDir, descriptionLen );
		}
		FS_FCloseFile( descHandle );
	} else {
		Q_strncpyz( description, modDir, descriptionLen );
	}
}


/*
================
FS_IsBaseGame
================
*/
static qboolean FS_IsBaseGame( const char *game )
{
	if ( game == NULL || *game == '\0' ) {
		return qtrue;
	}

	if ( Q_stricmp( fs_basegame->string, game ) == 0 ) {
		return qtrue;
	}

	return qfalse;
}


/*
===========
FS_PathCmp

Ignore case and separator char distinctions
===========
*/
static int FS_PathCmp( const char* s1, const char* s2 ) {
	int		c1, c2;

	do {
		c1 = (byte)*s1++;
		c2 = (byte)*s2++;

		if ( c1 >= 'a' && c1 <= 'z' ) {
			c1 -= ('a' - 'A');
		}
		if ( c2 >= 'a' && c2 <= 'z' ) {
			c2 -= ('a' - 'A');
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}

		if ( c1 < c2 ) {
			return -1;		// strings not equal
		}
		if ( c1 > c2 ) {
			return 1;
		}
	}
	while ( c1 );

	return 0;		// strings are equal
}


/*
================
FS_SortFileList
================
*/
static void FS_SortFileList( char** list, int n ) {
	const char* m;
	char* temp;
	int i, j;
	i = 0;
	j = n;
	m = list[n >> 1];
	do {
		while ( FS_PathCmp( list[i], m ) < 0 ) i++;
		while ( FS_PathCmp( list[j], m ) > 0 ) j--;
		if ( i <= j ) {
			temp = list[i];
			list[i] = list[j];
			list[j] = temp;
			i++;
			j--;
		}
	}
	while ( i <= j );
	if ( j > 0 ) FS_SortFileList( list, j );
	if ( n > i ) FS_SortFileList( list + i, n - i );
}


/*
================
FS_GetModList

Returns a list of mod directory names
A mod directory is a peer to baseq3 with a pk3 in it
================
*/
static int FS_GetModList( char *listbuf, int bufsize ) {
	int i, j, k;
	int	nMods, nTotal, nLen, nPaks, nPotential, nDescLen;
	int nDirs, nPakDirs;
	char **pFiles = NULL;
	char **pPaks = NULL;
	char **pDirs = NULL;
	const char *name, *path;
	char description[ MAX_OSPATH ];

	int dummy;
	char **pFiles0 = NULL;
	qboolean bDrop = qfalse;

	// paths to search for mods
	const char *paths[] = { FS_GetInstallPath(), FS_GetHomePath() };

	*listbuf = '\0';
	nMods = nTotal = 0;

	// iterate through paths and get list of potential mods
	for (i = 0; i < ARRAY_LEN( paths ); i++) {
		if ( !paths[ i ] || !paths[ i ][0] )
			continue;
		pFiles0 = Sys_ListFiles( paths[ i ], "/", NULL, &dummy, 1 );
		// Sys_ConcatenateFileLists frees the lists so Sys_FreeFileList isn't required
		pFiles = FS_ConcatenateFileLists( pFiles, pFiles0 );
	}

	nPotential = Sys_CountFileList( pFiles );

	if ( nPotential >= 2 ) {
		FS_SortFileList( pFiles, nPotential - 1 );
	}

	for ( i = 0; i < nPotential; i++ ) {
		// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — Sys_CountFileList(NULL) returns 0, so loop body is unreachable when pFiles is NULL
		name = pFiles[i];
		// NOTE: cleaner would involve more changes
		// ignore duplicate mod directories
		if ( i != 0 ) {
			bDrop = qfalse;
			for ( j = 0; j < i; j++ ) {
				if ( Q_stricmp( pFiles[j], name ) == 0 ) {
					// this one can be dropped
					bDrop = qtrue;
					break;
				}
			}
		}

		// we also drop BASEGAME, "." and ".."
		if ( bDrop || strcmp(name, "." ) == 0 || strcmp( name, ".." ) == 0 ) {
			continue;
		}

		if ( FS_IsBaseGame( name ) ) {
			continue;
		}

		// in order to be a valid mod the directory must contain at least one .pk3
		// we didn't keep the information when we merged the directory names, as to what OS Path it was found under
		// so we will try each of them here
		nPaks = nPakDirs = 0;
		for ( j = 0; j < ARRAY_LEN( paths ); j++ ) {
			if ( !paths[ j ] || !paths[ j ][0] )
				break;
			path = FS_BuildOSPath( paths[ j ], name, NULL );

			nPaks = nDirs = nPakDirs = 0;
			pPaks = Sys_ListFiles( path, ".pk3", NULL, &nPaks, 0 );
			pDirs = Sys_ListFiles( path, "/", NULL, &nDirs, 0 );
			for ( k = 0; k < nDirs; k++ ) {
				// we only want to count directories ending with ".pk3dir"
				if ( FS_IsExt( pDirs[k], ".pk3dir", strlen( pDirs[k] ) ) ) {
					nPakDirs++;
				}
			}

			// we only use Sys_ListFiles to check whether files are present
			Sys_FreeFileList( pDirs );
			Sys_FreeFileList( pPaks );
			if ( nPaks > 0 || nPakDirs > 0 ) {
				break;
			}
		}

		if ( nPaks > 0 || nPakDirs > 0 ) {
			nLen = strlen( name ) + 1;
			// nLen is the length of the mod path
			// we need to see if there is a description available
			FS_GetModDescription( name, description, sizeof( description ) );
			nDescLen = strlen( description ) + 1;

			if ( nTotal + nLen + 1 + nDescLen + 1 < bufsize ) {
				strcpy( listbuf, name );
				listbuf += nLen;
				strcpy( listbuf, description );
				listbuf += nDescLen;
				nTotal += nLen + nDescLen;
				nMods++;
			} else {
				break;
			}
		}
	}
	Sys_FreeFileList( pFiles );

	return nMods;
}


//============================================================================

/*
===========
FS_ConvertPath
===========
*/
static void FS_ConvertPath( char *s ) {
	while (*s) {
		if ( *s == '\\' || *s == ':' ) {
			*s = '/';
		}
		s++;
	}
}


/*
================
FS_Ls_f
================
*/
static void FS_Ls_f( void ) {
	const char *filter;
	char	**dirnames;
	char	dirname[ MAX_STRING_CHARS ];
	int		ndirs;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "usage: ls <filter>\n" );
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "example: ls *q3dm*.bsp\n");
		return;
	}

	filter = Cmd_Argv( 1 );

	Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "---------------\n" );

	dirnames = FS_ListFilteredFiles( "", "", filter, &ndirs, FS_MATCH_ANY | FS_MATCH_SUBDIRS );

	if ( ndirs >= 2 ) {
		FS_SortFileList( dirnames, ndirs - 1 );
	}

	for ( int i = 0; i < ndirs; i++ ) {
		Q_strncpyz( dirname, dirnames[i], sizeof( dirname ) );
		FS_ConvertPath( dirname );
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "%s\n", dirname );
	}

	Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "%d files listed\n", ndirs );
	FS_FreeFileList( dirnames );
}


/*
============
FS_Path_f
============
*/
static void FS_Path_f( void ) {
	const searchpath_t *s;

	Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "Current search path:\n" );
	for ( s = fs_searchpaths; s; s = s->next ) {
		if ( s->pack ) {
#if FEAT_SW3Z
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "%s (%i files)%s\n", s->pack->pakFilename, s->pack->numfiles,
				s->pack->type == PACK_SW3Z ? " [sw3z]" : "" );
#else
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "%s (%i files)\n", s->pack->pakFilename, s->pack->numfiles );
#endif
			if ( fs_numServerPaks ) {
				if ( !FS_PakIsPure( s->pack ) ) {
					COM_WARN( LOG_CH(ch_filesystem), "    not on the pure list\n" );
				} else {
					Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "    on the pure list\n" );
				}
			}
		}
		else {
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "%s%c%s\n", s->dir->path, PATH_SEP, s->dir->gamedir );
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "\n" );
	for ( int i = 1 ; i < MAX_FILE_HANDLES ; i++ ) {
		if ( fsh[i].handleFiles.file.o ) {
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "handle %i: %s\n", i, fsh[i].name );
		}
	}
}


/*
============
FS_TouchFile_f

The only purpose of this function is to allow game script files to copy
arbitrary files furing an "fs_copyfiles 1" run.
============
*/
static void FS_TouchFile_f( void ) {
	fileHandle_t	f;

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "Usage: touchFile <file>\n" );
		return;
	}

	FS_FOpenFileRead( Cmd_Argv( 1 ), &f, qfalse );
	if ( f != FS_INVALID_HANDLE ) {
		FS_FCloseFile( f );
	}
}


/*
============
FS_CompleteFileName
============
*/
static void FS_CompleteFileName( const char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "", qfalse, FS_MATCH_ANY );
	}
}


/*
============
FS_Which_f
============
*/
static void FS_Which_f( void ) {
	const searchpath_t *search;
	char			*netpath;
	pack_t			*pak;
	fileInPack_t	*pakFile;
	directory_t		*dir;
	long			hash;
	FILE			*temp;
	const char			*filename;
	char			buf[ MAX_OSPATH*2 + 1 ];
	int				numfound;

	hash = 0;
	numfound = 0;
	filename = Cmd_Argv(1);

	if ( !filename[0] ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "Usage: which <file>\n" );
		return;
	}

	// qpaths are not supposed to have a leading slash
	if ( filename[0] == '/' || filename[0] == '\\' ) {
		filename++;
	}

	// just wants to see if file is there
	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->pack ) {
			hash = FS_HashFileName(filename, search->pack->hashSize);
		}
		// is the element a pak file?
		if ( search->pack && search->pack->hashTable[hash] ) {
			// look through all the pak file elements
			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				// case and separator insensitive comparisons
				if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
					// found it!
					Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "File \"%s\" found in \"%s\"\n", filename, pak->pakFilename );
					if ( ++numfound >= 32 ) {
						return;
					}
				}
				pakFile = pakFile->next;
			} while(pakFile != NULL);
		}
		else if ( search->dir ) {
			dir = search->dir;

			netpath = FS_BuildOSPath( dir->path, dir->gamedir, filename );
			temp = Sys_FOpen( netpath, "rb" );
			if ( !temp ) {
				continue;
			}
			fclose(temp);
			Com_sprintf( buf, sizeof( buf ), "%s%c%s", dir->path, PATH_SEP, dir->gamedir );
			FS_ReplaceSeparators( buf );
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "File \"%s\" found at \"%s\"\n", filename, buf );
			if ( ++numfound >= 32 ) {
				return;
			}
		}
	}

	if ( !numfound ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "File not found: \"%s\"\n", filename );
	}
}


//===========================================================================

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads the zip headers
================
*/
static void FS_AddGameDirectory( const char *path, const char *dir ) {
	const searchpath_t *sp;
	int				len;
	searchpath_t	*search;
	const char		*gamedir;
	pack_t			*pak;
	char			curpath[MAX_OSPATH*2 + 1];
	char			*pakfile;
	int				numfiles;
	char			**pakfiles;
	int				pakfilesi;
	int				numdirs;
	char			**pakdirs;
	int				pakdirsi;
	int				pakwhich;
	int				path_len;
	int				dir_len;

	for ( sp = fs_searchpaths ; sp ; sp = sp->next ) {
		if ( sp->dir && !Q_stricmp( sp->dir->path, path ) && !Q_stricmp( sp->dir->gamedir, dir )) {
			return;	// we've already got this one
		}
	}

	Q_strncpyz( fs_gamedir, dir, sizeof( fs_gamedir ) );

	//
	// add the directory to the search path
	//
	path_len = (int) strlen( path ) + 1;
	path_len = PAD( path_len, sizeof( int ) );
	dir_len = (int) strlen( dir ) + 1;
	dir_len = PAD( dir_len, sizeof( int ) );
	len = sizeof( *search ) + sizeof( *search->dir ) + path_len + dir_len;

	search = Z_TagMalloc( len, TAG_SEARCH_PATH );
	memset( search, 0, len );
	search->dir = (directory_t*)( search + 1 );
	search->dir->path = (char*)( search->dir + 1 );
	search->dir->gamedir = (char*)( search->dir->path + path_len );

	strcpy( search->dir->path, path );
	strcpy( search->dir->gamedir, dir );
	gamedir = search->dir->gamedir;

	search->next = fs_searchpaths;
	fs_searchpaths = search;
	fs_dirCount++;

	// find all pak files in this directory
	Q_strncpyz( curpath, FS_BuildOSPath( path, dir, NULL ), sizeof( curpath ) );

	// Get .pk3 files
	pakfiles = Sys_ListFiles( curpath, ".pk3", NULL, &numfiles, 0 );

	if ( numfiles >= 2 )
		FS_SortFileList( pakfiles, numfiles - 1 );

	pakfilesi = 0;
	pakdirsi = 0;

	if ( fs_numServerPaks ) {
		numdirs = 0;
		pakdirs = NULL;
	} else {
		// Get top level directories (we'll filter them later since the Sys_ListFiles filtering is terrible)
		pakdirs = Sys_ListFiles( curpath, "/", NULL, &numdirs, 0 );
		if ( numdirs >= 2 ) {
			FS_SortFileList( pakdirs, numdirs - 1 );
		}
	}

	while (( pakfilesi < numfiles) || (pakdirsi < numdirs) )
	{
		// Check if a pakfile or pakdir comes next
		if (pakfilesi >= numfiles) {
			// We've used all the pakfiles, it must be a pakdir.
			pakwhich = 0;
		}
		else if (pakdirsi >= numdirs) {
			// We've used all the pakdirs, it must be a pakfile.
			pakwhich = 1;
		}
		else {
			// Could be either, compare to see which name comes first
			pakwhich = (FS_PathCmp( pakfiles[pakfilesi], pakdirs[pakdirsi] ) < 0);
		}

		if ( pakwhich ) {

			len = strlen( pakfiles[pakfilesi] );
			if ( !FS_IsExt( pakfiles[pakfilesi], ".pk3", len ) ) {
				// not a pk3 file
				pakfilesi++;
				continue;
			}

			// The next .pk3 file is before the next .pk3dir
			pakfile = FS_BuildOSPath( path, dir, pakfiles[pakfilesi] );
			if ( (pak = FS_LoadZipFile( pakfile ) ) == NULL ) {
				// This isn't a .pk3! Next!
				pakfilesi++;
				continue;
			}

			// store the game name for downloading
			pak->pakGamename = gamedir;

			pak->index = fs_packCount;
			pak->referenced = 0;
			pak->exclude = qfalse;

			fs_packFiles += pak->numfiles;
			fs_packCount++;

			search = Z_TagMalloc( sizeof( *search ), TAG_SEARCH_PACK );
			memset( search, 0, sizeof( *search ) );
			search->pack = pak;

			search->next = fs_searchpaths;
			fs_searchpaths = search;

			pakfilesi++;
		} else {

			len = strlen(pakdirs[pakdirsi]);

			// The next .pk3dir is before the next .pk3 file
			// But wait, this could be any directory, we're filtering to only ending with ".pk3dir" here.
			if (!FS_IsExt(pakdirs[pakdirsi], ".pk3dir", len)) {
				// This isn't a .pk3dir! Next!
				pakdirsi++;
				continue;
			}

			// add the directory to the search path
			path_len = (int) strlen( curpath ) + 1;
			path_len = PAD( path_len, sizeof( int ) );
			dir_len = PAD( len + 1, sizeof( int ) );
			len = sizeof( *search ) + sizeof( *search->dir ) + path_len + dir_len;

			search = Z_TagMalloc( len, TAG_SEARCH_DIR );
			memset( search, 0, len );
			search->dir = (directory_t*)(search + 1);
			search->dir->path = (char*)( search->dir + 1 );
			search->dir->gamedir = (char*)( search->dir->path + path_len );
			search->policy = DIR_ALLOW;

			strcpy( search->dir->path, curpath );				// c:\quake3\baseq3
			strcpy( search->dir->gamedir, pakdirs[ pakdirsi ] );// mypak.pk3dir

			search->next = fs_searchpaths;
			fs_searchpaths = search;
			fs_pk3dirCount++;

			pakdirsi++;
		}
	}

	// done
	Sys_FreeFileList( pakdirs );
	Sys_FreeFileList( pakfiles );

#if FEAT_SW3Z
	{
		int numsw3z;
		char **sw3zfiles;

		sw3zfiles = Sys_ListFiles( curpath, ".sw3z", NULL, &numsw3z, 0 );
		if ( numsw3z >= 2 )
			FS_SortFileList( sw3zfiles, numsw3z - 1 );

		for ( pakfilesi = 0; pakfilesi < numsw3z; pakfilesi++ ) {
			len = strlen( sw3zfiles[pakfilesi] );
			if ( !FS_IsExt( sw3zfiles[pakfilesi], ".sw3z", len ) )
				continue;

			pakfile = FS_BuildOSPath( path, dir, sw3zfiles[pakfilesi] );

#ifdef USE_PAK_CACHE
			/* Cache lookup: a previous load with matching size/mtime/ctime
			 * is reused directly. Mirrors the FS_LoadZipFile flow at the
			 * top of that function (PK3 path). On a feed change the
			 * pure_checksum is recomputed in place; everything else
			 * (entries[], stringTable, hashTable, FILE handle) survives. */
			pak = FS_LoadCachedPak( pakfile );
			if ( pak )
			{
				if ( pak->checksumFeed != fs_checksumFeed )
				{
					pak->headerLongs[0] = LittleLong( fs_checksumFeed );
					pak->pure_checksum = Com_BlockChecksum( pak->headerLongs,
						sizeof( pak->headerLongs[0] ) * pak->numHeaderLongs );
					pak->pure_checksum = LittleLong( pak->pure_checksum );
					pak->checksumFeed = fs_checksumFeed;
				}
				pak->touched = qtrue;
			}
			else
#endif
			{
				pak = SW3Z_LoadArchive( pakfile );
				if ( !pak )
					continue;

				/* Complete the cache fields that SW3Z_LoadArchive left for
				 * us (it can't access the static fs_checksumFeed). */
				pak->checksumFeed = fs_checksumFeed;
				pak->headerLongs[0] = LittleLong( fs_checksumFeed );
				pak->checksum = Com_BlockChecksum( pak->headerLongs + 1,
					sizeof( pak->headerLongs[0] ) * ( pak->numHeaderLongs - 1 ) );
				pak->checksum = LittleLong( pak->checksum );
				pak->pure_checksum = Com_BlockChecksum( pak->headerLongs,
					sizeof( pak->headerLongs[0] ) * pak->numHeaderLongs );
				pak->pure_checksum = LittleLong( pak->pure_checksum );

#ifdef USE_HANDLE_CACHE
				/* SW3Z_LoadArchive opened the FILE* — register it in
				 * the SW3Z handle LRU. May trigger eviction of an
				 * older idle SW3Z FILE* if the cap is reached. */
				FS_AddToHandleList_SW3Z( pak );
#endif

#ifdef USE_PAK_CACHE
				FS_InsertPakToCache( pak );
#ifdef USE_PAK_CACHE_FILE
				fs_paksReaded++;
#endif
#endif
			}

			pak->pakGamename = gamedir;
			pak->index = fs_packCount;
			pak->referenced = 0;
			pak->exclude = qfalse;

			fs_packFiles += pak->numfiles;
			fs_packCount++;

			search = Z_TagMalloc( sizeof( *search ), TAG_SEARCH_PACK );
			memset( search, 0, sizeof( *search ) );
			search->pack = pak;

			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
		Sys_FreeFileList( sw3zfiles );
	}
#endif
}


/*
================
FS_isProprietary

NOTE: This list identifies id-Software-proprietary Quake III paks for download
protection. It is engine-level only because the entries belong to the historical
Q3 / Team-Arena content set; future games shipping on the wired engine should
register their own proprietary lists at game-init time rather than baking
filenames into the VFS. Tracked under MB-13 (multi-game smoke) in
docs/wired-branding-migration.md.
================
*/
static const char *proprietaryFileList[] = {
	"baseq3/pak0.pk3",
	"baseq3/pak1.pk3",
	"baseq3/pak2.pk3",
	"baseq3/pak3.pk3",
	"baseq3/pak4.pk3",
	"baseq3/pak5.pk3",
	"baseq3/pak6.pk3",
	"baseq3/pak7.pk3",
	"baseq3/pak8.pk3",
	"missionpack/pak0.pk3",
	"missionpack/pak1.pk3",
	"missionpack/pak2.pk3",
	"missionpack/pak3.pk3",
	NULL
};

static const int	proprietaryFileCount = ARRAY_LEN( proprietaryFileList ) - 1;

qboolean FS_isProprietary(const char *pak)
{
	for (int i = 0; i < proprietaryFileCount; i++) {
		if ( !FS_FilenameCompare(pak, proprietaryFileList[i])) {
			return qtrue;
		}
	}

	return qfalse;
}


/*
================
FS_InvalidGameDir
return true if path is a reference to current directory or directory traversal
or a sub-directory
================
*/
qboolean FS_InvalidGameDir( const char *gamedir )
{
	if ( !strcmp( gamedir, "." ) || !strcmp( gamedir, ".." )
		|| strchr( gamedir, '/' ) || strchr( gamedir, '\\' ) ) {
		return qtrue;
	}

	return qfalse;
}


/*
================
FS_ComparePaks

----------------
dlstring == qtrue

Returns a list of pak files that we should download from the server. They all get stored
in the current gamedir and an FS_Restart will be fired up after we download them all.

The string is the format:

@remotename@localname [repeat]

static int		fs_numServerReferencedPaks;
static int		fs_serverReferencedPaks[MAX_REF_PAKS];
static char		*fs_serverReferencedPakNames[MAX_REF_PAKS];

----------------
dlstring == qfalse

we are not interested in a download string format, we want something human-readable
(this is used for diagnostics while connecting to a pure server)

================
*/
qboolean FS_ComparePaks( char *neededpaks, int len, qboolean dlstring ) {
	const searchpath_t	*sp;

	if (!fs_numServerReferencedPaks)
		return qfalse; // Server didn't send any pack information along

	qstring_t qs = QS_Wrap( neededpaks, len );
	qboolean havepak;

	for ( int i = 0 ; i < fs_numServerReferencedPaks ; i++ )
	{
		// Ok, see if we have this pak file
		havepak = qfalse;

		// never autodownload any of proprietary files
		if ( FS_isProprietary(fs_serverReferencedPakNames[i]) ) {
			continue;
		}

		// Make sure the server cannot make us write to non-quake3 directories.
		if ( FS_CheckDirTraversal( fs_serverReferencedPakNames[i] ) ) {
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "WARNING: Invalid download name %s\n", fs_serverReferencedPakNames[i] );
			continue;
		}

		for ( sp = fs_searchpaths ; sp ; sp = sp->next ) {
			if ( sp->pack && sp->pack->checksum == fs_serverReferencedPaks[i] ) {
				havepak = qtrue; // This is it!
				break;
			}
		}

		if ( !havepak && fs_serverReferencedPakNames[i] && *fs_serverReferencedPakNames[i] ) {
			// Don't got it

      if (dlstring)
      {
		int savedLen = QS_Len( &qs );

        // Remote name
        QS_Append( &qs, "@" );
        QS_Append( &qs, fs_serverReferencedPakNames[i] );
        QS_Append( &qs, ".pk3" );

        // Local name
        QS_Append( &qs, "@" );
        // Do we have one with the same name?
        if ( FS_SV_FileExists( va( "%s.pk3", fs_serverReferencedPakNames[i] ) ) )
        {
          char st[MAX_ZPATH];
          // We already have one called this, we need to download it to another name
          // Make something up with the checksum in it
          Com_sprintf( st, sizeof( st ), "%s.%08x.pk3", fs_serverReferencedPakNames[i], fs_serverReferencedPaks[i] );
          QS_Append( &qs, st );
        } else
        {
          QS_Append( &qs, fs_serverReferencedPakNames[i] );
          QS_Append( &qs, ".pk3" );
        }

        // If appending this pak entry overflowed the buffer, roll back and stop.
        if ( QS_Remaining( &qs ) == 0 )
	{
		QS_Truncate( &qs, savedLen );
		break;
	}
      }
      else
      {
        QS_Append( &qs, fs_serverReferencedPakNames[i] );
        QS_Append( &qs, ".pk3" );
        // Do we have one with the same name?
        if ( FS_SV_FileExists( va( "%s.pk3", fs_serverReferencedPakNames[i] ) ) )
        {
          QS_Append( &qs, " (local file exists with wrong checksum)" );
        }
        QS_AppendChar( &qs, '\n' );
      }
		}
	}

	if ( !QS_Empty( &qs ) ) {
		return qtrue;
	}

	return qfalse; // We have them all
}


/*
================
FS_Shutdown

Frees all resources.
================
*/
void FS_Shutdown( qboolean closemfp )
{
	// close opened files
	if ( closemfp )
	{
		for ( int i = 1; i < MAX_FILE_HANDLES; i++ )
		{
			if ( !fsh[i].handleFiles.file.v
#if FEAT_SW3Z
				&& !fsh[i].sw3zData
#endif
				)
				continue;

			FS_FCloseFile( i );
		}
	}

#ifdef DELAY_WRITECONFIG
	if ( fs_searchpaths )
	{
		Com_WriteConfiguration();
	}
#endif

#ifdef USE_PAK_CACHE
	FS_ResetCacheReferences();
#endif

	// free everything
	for ( searchpath_t *p = fs_searchpaths, *next; p; p = next )
	{
		next = p->next;

		if ( p->pack )
		{
#ifdef USE_PAK_CACHE
			/* Both PK3 and SW3Z packs persist in pakHashTable across
			 * FS_Restart; the FS_FreeUnusedCache() call below frees
			 * any pack not re-touched by the next directory scan.
			 * PK3 additionally needs to be unlinked from the file-
			 * handle LRU list (SW3Z packs hold their FILE* directly
			 * and are not handle-list members). */
#ifdef USE_HANDLE_CACHE
			if ( p->pack->type == PACK_PK3 && p->pack->next_h )
				FS_RemoveFromHandleList( p->pack );
#endif
#else
			FS_FreePak( p->pack );
#endif
			p->pack = NULL;
		}

		Z_Free( p );
	}

	// any FS_ calls will now be an error until reinitialized
	fs_searchpaths = NULL;
	fs_packFiles = 0;

	fs_pk3dirCount = 0;
	fs_packCount = 0;
	fs_dirCount = 0;

	Cmd_RemoveCommand( "path" );
	Cmd_RemoveCommand( "ls" );
	Cmd_RemoveCommand( "touchFile" );
	Cmd_RemoveCommand( "which" );
	Cmd_RemoveCommand( "lsof" );
	Cmd_RemoveCommand( "fs_restart" );
}

#if !FEAT_FS_PRECEDENCE
/*
================
FS_ReorderSearchPaths
================
*/
static void FS_ReorderSearchPaths( void ) {
	searchpath_t **list, **paks, **dirs;
	searchpath_t *path;
	int i, ndirs, npaks, cnt;

	cnt = fs_packCount + fs_dirCount + fs_pk3dirCount;
	if ( cnt == 0 )
		return;

	// relink path chains in following order:
	// 1. pk3dirs @ pak files
	// 2. directories
	list = (searchpath_t **)Z_Malloc( cnt * sizeof( list[0] ) );
	paks = list;
	dirs = list + fs_pk3dirCount + fs_packCount;

	npaks = ndirs = 0;
	path = fs_searchpaths;
	while ( path ) {
		if ( path->pack || path->policy != DIR_STATIC ) {
			paks[npaks++] = path;
		} else {
			dirs[ndirs++] = path;
		}
		path = path->next;
	}

	fs_searchpaths = list[0];
	for ( i = 0; i < cnt-1; i++ ) {
		list[i]->next = list[i+1];
	}
	list[cnt-1]->next = NULL;

	Z_Free( list );
}
#endif


/*
================
FS_ReorderPurePaks
NOTE TTimo: the reordering that happens here is not reflected in the cvars (\cvarlist *pak*)
  this can lead to misleading situations, see https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=540
================
*/
static void FS_ReorderPurePaks( void )
{
	fs_reordered = qfalse;

	// only relevant when connected to pure server
	if ( !fs_numServerPaks )
		return;

	searchpath_t **p_insert_index = &fs_searchpaths; // we insert in order at the beginning of the list
	for ( int i = 0 ; i < fs_numServerPaks ; i++ ) {
		searchpath_t **p_previous = p_insert_index; // track the pointer-to-current-item
		for (searchpath_t *s = *p_insert_index; s; s = s->next) {
			// the part of the list before p_insert_index has been sorted already
			if (s->pack && fs_serverPaks[i] == s->pack->checksum) {
				fs_reordered = qtrue;
				// move this element to the insert list
				*p_previous = s->next;
				s->next = *p_insert_index;
				*p_insert_index = s;
				// increment insert list
				p_insert_index = &s->next;
				break; // iterate to next server pack
			}
			p_previous = &s->next;
		}
	}
}


/*
================
FS_OwnerName
================
*/
static const char *FS_OwnerName( handleOwner_t owner )
{
	static const char *s[4]= { "SY", "QA", "CG" };
	if ( owner < H_SYSTEM )
		return "??";
	return s[owner];
}


/*
================
FS_ListOpenFiles
================
*/
static void FS_ListOpenFiles_f( void ) {
	fileHandleData_t *fh = fsh;
	for ( int i = 0; i < MAX_FILE_HANDLES; i++, fh++ ) {
		if ( !fh->handleFiles.file.v )
			continue;
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "%2i %2s %s\n", i, FS_OwnerName(fh->owner), fh->name );
	}
}


/*
=====================
FS_LoadedPakPureChecksums
=====================
*/
static int fs_numPureChecksums;
static int fs_pureChecksum[ MAX_FOUND_FILES ];

static void FS_LoadedPakPureChecksums( void )
{
	const searchpath_t *search;

	fs_numPureChecksums = 0;
	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->pack ) {
			if ( fs_numPureChecksums >= ARRAY_LEN( fs_pureChecksum ) ) {
				Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "WARNING: pure checksums overflowed\n" );
				fs_numPureChecksums = 0;
				return;
			}
			fs_pureChecksum[ fs_numPureChecksums ] = search->pack->pure_checksum;
			fs_numPureChecksums++;
		}
	}
}


/*
================
FS_IsPureChecksum
================
*/
qboolean FS_IsPureChecksum( int sum )
{
	if ( fs_numPureChecksums == 0 )
		return qtrue;

	for ( int i = 0; i < fs_numPureChecksums; i++ )
		if ( fs_pureChecksum[i] == sum )
			return qtrue;

	return qfalse;
}


#if FEAT_FS_PRECEDENCE
/*
================
FS_DeduplicateArchives

Treats all pk3/sw3z archives across directories as one virtual set.
For each unique basename (e.g., "pak0"), keeps only the highest-priority
version and removes duplicates from the search path.

Priority rules:
  1. Directory order: first seen in search path wins (HOMEDIR > DATADIR > BASEDIR)
  2. Within same dir: sw3z > pk3 (sw3z is prepended after pk3, so appears first)
  3. After dedup, re-sort archive entries by basename descending
     so "pax21" overrides "pak0"

  Before:                           After:
  ┌─────────────────────────┐      ┌─────────────────────────┐
  │ HOMEDIR/pak0.pk3        │      │ pax21.pk3 (DATADIR)     │ ← sorted desc
  │ HOMEDIR/pak1.pk3        │      │ pak8.pk3 (HOMEDIR)      │
  │ ...                     │      │ pak7.pk3 (HOMEDIR)      │
  │ DATADIR/pak0.pk3  [dup] │  →   │ ...                     │
  │ DATADIR/pax21.pk3       │      │ pak1.pk3 (HOMEDIR)      │
  │ BASEDIR/pak0.pk3  [dup] │      │ pak0.pk3 (HOMEDIR)      │
  └─────────────────────────┘      └─────────────────────────┘
================
*/
#define MAX_DEDUP_ARCHIVES 256

static void FS_DeduplicateArchives( void )
{
	searchpath_t	*s;
	const char		*basenames[MAX_DEDUP_ARCHIVES];
	int				numSeen = 0;
	int				removed = 0;
	searchpath_t	*archives[MAX_DEDUP_ARCHIVES];
	int				numArchives = 0;
	searchpath_t	*dirs = NULL, *dirsTail = NULL;
	int				i;

	/* ── Phase 1: Dedup — first-seen basename wins ── */
	searchpath_t **pp = &fs_searchpaths;
	while ( *pp ) {
		const char *basename = NULL;

		s = *pp;

		if ( s->pack )
			basename = s->pack->pakBasename;

		if ( basename ) {
			/* check if this basename was already seen */
			qboolean duplicate = qfalse;
			for ( int i = 0; i < numSeen; i++ ) {
				if ( Q_stricmp( basenames[i], basename ) == 0 ) {
					duplicate = qtrue;
					break;
				}
			}

			if ( duplicate ) {
				/* remove from search path — lower priority duplicate */
				*pp = s->next;

				/* Close file handles and free pack resources. Both PK3 and
				 * SW3Z packs are registered in pakHashTable (Phase 1+) — the
				 * cache pointer must be removed before FS_FreePak runs so
				 * a subsequent FS_FreeUnusedCache (next FS_Restart) doesn't
				 * double-free this entry. FS_FreePak dispatches by
				 * pack->type internally (SW3Z → SW3Z_CloseArchive). Order
				 * matters — read numfiles before close. */
				if ( s->pack ) {
					fs_packFiles -= s->pack->numfiles;
					fs_packCount--;
#ifdef USE_PAK_CACHE
					FS_RemoveFromCache( s->pack );
#endif
					FS_FreePak( s->pack );
					s->pack = NULL;
				}
				Z_Free( s );
				removed++;
				continue; /* don't advance pp — next element is now at *pp */
			}

			/* first occurrence — record it */
			if ( numSeen < MAX_DEDUP_ARCHIVES ) {
				basenames[numSeen++] = basename;
			}
		}

		pp = &(*pp)->next;
	}

	/* ── Phase 2: Separate into 3 groups ──
	 * archives:    pack/sw3z entries (will be sorted)
	 * dynDirs:     pk3dir entries (DIR_ALLOW/DIR_DENY — higher priority than static)
	 * staticDirs:  base game directories (DIR_STATIC — lowest priority)
	 */
	{
		searchpath_t *dynDirsTail = NULL, *staticDirsTail = NULL;
		numArchives = 0;
		dirs = NULL;       /* reuse as dynDirs head */
		dirsTail = NULL;   /* reuse as staticDirs head */

		while ( fs_searchpaths ) {
			s = fs_searchpaths;
			fs_searchpaths = s->next;
			s->next = NULL;

			if ( s->pack ) {
				/* archive entry */
				if ( numArchives < MAX_DEDUP_ARCHIVES )
					archives[numArchives++] = s;
			} else if ( s->policy != DIR_STATIC ) {
				/* pk3dir or dynamic directory — before static dirs */
				if ( !dirs ) {
					dirs = dynDirsTail = s;
				} else {
					dynDirsTail->next = s;
					dynDirsTail = s;
				}
			} else {
				/* DIR_STATIC — lowest priority */
				if ( !dirsTail ) {
					dirsTail = staticDirsTail = s;
				} else {
					staticDirsTail->next = s;
					staticDirsTail = s;
				}
			}
		}

		/* chain dynDirs → staticDirs for Phase 4 */
		if ( dirs && dirsTail ) {
			dynDirsTail->next = dirsTail;
		} else if ( !dirs ) {
			dirs = dirsTail;
		}
	}

	/* ── Phase 3: Sort archives by basename descending ── */
	/* simple insertion sort — MAX_DEDUP_ARCHIVES is small */
	for ( i = 1; i < numArchives; i++ ) {
		searchpath_t *key = archives[i];
		const char *keyName = key->pack ? key->pack->pakBasename : "";
		int j = i - 1;
		while ( j >= 0 ) {
			const char *jName = archives[j]->pack ? archives[j]->pack->pakBasename : "";
			if ( Q_stricmp( jName, keyName ) >= 0 )
				break; /* jName >= keyName → already in descending order */
			archives[j + 1] = archives[j];
			j--;
		}
		archives[j + 1] = key;
	}

	/* ── Phase 4: Rebuild search path — archives first (highest priority), then dirs ── */
	fs_searchpaths = NULL;

	/* append directories (they go to the tail = lowest priority for archive lookups) */
	if ( dirs ) {
		fs_searchpaths = dirs;
	}

	/* prepend sorted archives (first in array = highest priority = prepended last) */
	for ( i = numArchives - 1; i >= 0; i-- ) {
		archives[i]->next = fs_searchpaths;
		fs_searchpaths = archives[i];
	}

	if ( removed > 0 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_Precedence: removed %d duplicate archives, %d unique remain\n", removed, numArchives );
	}
}
#endif /* FEAT_FS_PRECEDENCE */


/*
================
FS_Startup
================
*/
static void FS_Startup( void ) {
	const char *homePath;
	int i, start, end;

	{
		static const cvarDesc_t d = CVAR_BOOL( "fs_debug", "0", 0,
			"Debugging tool for the filesystem. Run the game in debug mode. Prints additional information regarding read files into the console." );
		fs_debug = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "fs_copyfiles", "0", CVAR_INIT,
			"Whether or not to copy files when loading them into the game. Every file found in the cdpath will be copied over." );
		fs_copyfiles = Cvar_Register( &d );
	}
	fs_installpath = Cvar_Get( "fs_installpath", Sys_DefaultBasePath(), CVAR_INIT | CVAR_PROTECTED | CVAR_PRIVATE );
	Cvar_SetDescription( fs_installpath, "Write-protected CVAR specifying the path to the installation folder of the game." );
	{
		static const cvarDesc_t d = CVAR_STRING( "fs_basegame", BASEGAME, CVAR_INIT | CVAR_PROTECTED,
			"Write-protected CVAR specifying the path to the base game folder." );
		fs_basegame = Cvar_Register( &d );
	}

	if ( fs_basegame->string[0] == '\0' )
		Com_Terminate( TERM_UNRECOVERABLE, "* fs_basegame is not set *" );

#ifndef USE_HANDLE_CACHE
	{
		static const cvarDesc_t d = CVAR_INT( "fs_locked", "0", CVAR_INIT,
			"Set file handle policy for pk3 files:\n"
			" 0 - release after use, unlimited number of pk3 files can be loaded\n"
			" 1 - keep file handle locked, more consistent, total pk3 files count limited to ~1k-4k\n",
			0, 0 );
		fs_locked = Cvar_Register( &d );
	}
#endif

	homePath = Sys_DefaultHomePath();
	if ( homePath == NULL || homePath[0] == '\0' ) {
		homePath = FS_GetInstallPath();
	}

	fs_homepath = Cvar_Get( "fs_homepath", homePath, CVAR_INIT | CVAR_PROTECTED | CVAR_PRIVATE );
	Cvar_SetDescription( fs_homepath, "Directory to store user configuration and downloaded files." );

	fs_gamedirvar = Cvar_Get( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO );
	Cvar_CheckRange( fs_gamedirvar, NULL, NULL, CV_FSPATH );
	Cvar_SetDescription( fs_gamedirvar, "Specify an alternate mod directory and run the game with this mod." );

	if ( FS_IsBaseGame( fs_gamedirvar->string ) ) {
		Cvar_ForceReset( "fs_game" );
	}

	{
		static const cvarDesc_t d = CVAR_STRING( "fs_excludeReference", "", CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,
			"Exclude specified pak files from download list on client side.\n"
			"Format is <moddir>/<pakname> (without .pk3 suffix), you may list multiple entries separated by space." );
		fs_excludeReference = Cvar_Register( &d );
	}

	start = Sys_Milliseconds();

#ifdef USE_PAK_CACHE
#ifdef USE_PAK_CACHE_FILE
	FS_LoadCache();
#endif
#endif

	// add search path elements in reverse priority order
	// On macOS, paks live under Contents/Resources/ (the resource path);
	// on Linux/Windows the resource path collapses to fs_installpath.
	if (fs_installpath->string[0]) {
		FS_AddGameDirectory( FS_GetInstallResourcePath(), fs_basegame->string );
	}

	// fs_homepath is somewhat particular to *nix systems, only add if relevant
	// NOTE: same filtering below for mods and basegame
	// Comparison guard intentionally reads the raw cvar (not an accessor)
	// because it's a portability gate distinct from the FS scan target.
	if ( fs_homepath->string[0] && Q_stricmp( fs_homepath->string, fs_installpath->string ) ) {
		FS_AddGameDirectory( fs_homepath->string, fs_basegame->string );
	}

	// check for additional game folder for mods
	if ( fs_gamedirvar->string[0] != '\0' && !FS_IsBaseGame( fs_gamedirvar->string ) ) {
		if ( fs_installpath->string[0] != '\0' ) {
			FS_AddGameDirectory( FS_GetInstallResourcePath(), fs_gamedirvar->string );
		}
		if ( fs_homepath->string[0] != '\0' && Q_stricmp( fs_homepath->string, fs_installpath->string ) ) {
			FS_AddGameDirectory( fs_homepath->string, fs_gamedirvar->string );
		}
	}

#if FEAT_FS_PRECEDENCE
	// FS_DeduplicateArchives handles separation + dedup + descending sort
	// (replaces FS_ReorderSearchPaths when precedence is enabled)
	FS_DeduplicateArchives();
#else
	// reorder search paths to minimize further changes
	FS_ReorderSearchPaths();
#endif

	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=506
	// reorder the pure pk3 files according to server order
	FS_ReorderPurePaks();

	// get the pure checksums of the pk3 files loaded by the server
	FS_LoadedPakPureChecksums();

	end = Sys_Milliseconds();

	// add our commands
	Cmd_AddCommand( "path", FS_Path_f );
	Cmd_AddCommand( "ls", FS_Ls_f );
	Cmd_AddCommand( "touchFile", FS_TouchFile_f );
	Cmd_AddCommand( "lsof", FS_ListOpenFiles_f );
 	Cmd_AddCommand( "which", FS_Which_f );
	Cmd_SetCommandCompletionFunc( "which", FS_CompleteFileName );
	Cmd_AddCommand( "fs_restart", FS_Reload );

	// print the current search paths
	//FS_Path_f();
	Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "WiredCore/FS startup: %d cached pak(s), %d files in %d pak file(s) (%d ms)\n",
	            fs_paksCached, fs_packFiles, fs_packCount, end - start );


	// check original q3a files — skip when SW3Z packs are present
	// (assets are repacked into a single .sw3z, the original pak0-pak8
	// naming convention no longer applies)
#if FEAT_SW3Z
	{
		qboolean hasSW3Z = qfalse;
		const searchpath_t *sp;
		for ( sp = fs_searchpaths; sp; sp = sp->next ) {
			if ( sp->pack && sp->pack->type == PACK_SW3Z ) {
				hasSW3Z = qtrue;
				break;
			}
		}
		if ( !hasSW3Z )
#endif
		if ( FS_IsBaseGame( BASEGAME ) ) {
			FS_CheckIdPaks();
		}
#if FEAT_SW3Z
	}
#endif

#ifdef FS_MISSING
	if (missingFiles == NULL) {
		missingFiles = Sys_FOpen( FS_BuildOSPath( fs_homepath->string, "missing.txt", NULL ), "ab" );
	}
#endif

#ifdef USE_PAK_CACHE
	FS_FreeUnusedCache();
#ifdef USE_PAK_CACHE_FILE
	FS_SaveCache();
#endif
#endif
}


static void FS_PrintSearchPaths( void )
{
	const searchpath_t *path = fs_searchpaths;

	Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "\nSearch paths:\n" );

	while ( path )
	{
		if ( path->dir && path->policy == DIR_STATIC )
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), " * %s\n", path->dir->path );

		path = path->next;
	}
}


/*
===================
FS_CheckIdPaks

Checks that pak0.pk3 is present and its checksum is correct
Note: If you're building a game that doesn't depend on the
Q3 media pak0.pk3, you'll want to remove this function
===================
*/
static void FS_CheckIdPaks( void )
{
	qboolean founddemo = qfalse;
	unsigned foundPak = 0;

	for ( const searchpath_t *path = fs_searchpaths; path; path = path->next )
	{
		if ( !path->pack )
			continue;

		const char *pakBasename = path->pack->pakBasename;

		if(!Q_stricmpn( path->pack->pakGamename, BASEGAME, MAX_OSPATH )
			&& strlen(pakBasename) == 4 && !Q_stricmpn( pakBasename, "pak", 3 )
			&& pakBasename[3] >= '0' && pakBasename[3] <= '8')
		{
			if( (unsigned int)path->pack->checksum != pak_checksums[pakBasename[3]-'0'] )
			{
				FS_PrintSearchPaths();

				if(pakBasename[3] == '0')
				{
					Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "\n\n"
						"**************************************************\n"
						"ERROR: pak0.pk3 is present but its checksum (%u)\n"
						"is not correct. Please re-copy pak0.pk3 from your\n"
						"legitimate Q3 CDROM.\n"
						"**************************************************\n\n\n",
						path->pack->checksum );
				}
				else
				{
					Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "\n\n"
						"**************************************************\n"
						"ERROR: pak%d.pk3 is present but its checksum (%u)\n"
						"is not correct. Please re-install Quake 3 Arena \n"
						"Point Release v1.32 pk3 files\n"
						"**************************************************\n\n\n",
						pakBasename[3]-'0', path->pack->checksum );
				}
				Com_Terminate( TERM_UNRECOVERABLE, "\n* You need to install correct Quake III Arena files in order to play *");
			}

			foundPak |= 1<<(pakBasename[3]-'0');
		}
	}

	if(!founddemo && (foundPak & 0x1ff) != 0x1ff )
	{
		FS_PrintSearchPaths();

		if((foundPak&1) != 1 )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "\n\n"
			"pak0.pk3 is missing. Please copy it\n"
			"from your legitimate Q3 CDROM.\n");
		}

		if((foundPak&0x1fe) != 0x1fe )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "\n\n"
			"Point Release files are missing. Please\n"
			"re-install the 1.32 point release.\n");
		}

		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "\n\n"
			"Also check that your Q3 executable is in\n"
			"the correct place and that every file\n"
			"in the %s directory is present and readable.\n", BASEGAME);

		if(!fs_gamedirvar->string[0]
		|| !Q_stricmp( fs_gamedirvar->string, BASEGAME ))
			Com_Terminate( TERM_UNRECOVERABLE, "\n*** you need to install Quake III Arena in order to play ***");
	}
}


/*
=====================
FS_LoadedPakChecksums

Returns a space separated string containing the checksums of all loaded pk3 files.
Servers with sv_pure set will get this string and pass it to clients.
=====================
*/
const char *FS_LoadedPakChecksums( qboolean *overflowed ) {
	static char	info[BIG_INFO_STRING];
	const searchpath_t *search;
	char buf[ 32 ];
	char *s, *max;
	int len;

	s = info;
	info[0] = '\0';
	max = &info[sizeof(info)-1];
	*overflowed = qfalse;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( search->pack ) {
			if ( search->pack->exclude )
				continue;

			if ( info[0] )
				len = sprintf( buf, " %i", search->pack->checksum );
			else
				len = sprintf( buf, "%i", search->pack->checksum );

			if ( s + len > max ) {
				*overflowed = qtrue;
				break;
			}

			s = Q_stradd( s, buf );
		}
	}

	return info;
}


/*
=====================
FS_LoadedPakNames

Returns a space separated string containing the names of all loaded pk3 files.
Servers with sv_pure set will get this string and pass it to clients.
=====================
*/
#ifndef DEDICATED
const char *FS_LoadedPakNames( void ) {
	static char	info[BIG_INFO_STRING];
	const searchpath_t *search;
	char *s, *max;
	int len;

	s = info;
	info[0] = '\0';
	max = &info[sizeof(info)-1];

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( search->pack ) {
			if ( search->pack->exclude )
				continue;

			len = (int)strlen( search->pack->pakBasename );
			if ( info[0] )
				len++;

			if ( s + len > max )
				break;

			if ( info[0] )
				s = Q_stradd( s, " " );

			s = Q_stradd( s, search->pack->pakBasename );
		}
	}

	return info;
}
#endif


/*
=====================
FS_ReferencedPakChecksums

Returns a space separated string containing the checksums of all referenced pk3 files.
The server will send this to the clients so they can check which files should be auto-downloaded.
=====================
*/
const char *FS_ReferencedPakChecksums( void ) {
	static char	info[BIG_INFO_STRING];
	const searchpath_t *search;

	qstring_t info_qs = QS_Wrap( info, sizeof( info ) );

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( search->pack ) {
			if ( search->pack->exclude ) {
				continue;
			}
			if ( search->pack->referenced || !FS_IsBaseGame( search->pack->pakGamename ) ) {
				QS_Appendf( &info_qs, "%i ", search->pack->checksum );
			}
		}
	}

	return info;
}


/*
=====================
FS_ReferencedPakPureChecksums

Returns a space separated string containing the pure checksums of all referenced pk3 files.
Servers with sv_pure set will get this string back from clients for pure validation

The string has a specific order, "cgame ui @ ref1 ref2 ref3 ..."
=====================
*/
const char *FS_ReferencedPakPureChecksums( int maxlen ) {
	static char	info[ MAX_STRING_CHARS*2 ];
	char *s, *max;
	const searchpath_t	*search;
	int nFlags, numPaks, checksum;

	max = info + maxlen; // maxlen is always smaller than MAX_STRING_CHARS so we can overflow a bit
	s = info;
	*s = '\0';

	checksum = fs_checksumFeed;
	numPaks = 0;
	for ( nFlags = FS_CGAME_REF; nFlags; nFlags = nFlags >> 1 ) {
		if ( nFlags & FS_GENERAL_REF ) {
			// add a delimiter between must haves and general refs
			s = Q_stradd( s, "@ " );
			if ( s > max ) // client-side overflow
				break;
		}
		for ( search = fs_searchpaths ; search ; search = search->next ) {
			// is the element a pak file and has it been referenced based on flag?
			if ( search->pack && (search->pack->referenced & nFlags)) {
				s = Q_stradd( s, va( "%i ", search->pack->pure_checksum ) );
				if ( s > max ) // client-side overflow
					break;
				if ( nFlags & (FS_CGAME_REF | FS_UI_REF) ) {
					break;
				}
				checksum ^= search->pack->pure_checksum;
				numPaks++;
			}
		}
	}

	// last checksum is the encoded number of referenced pk3s
	checksum ^= numPaks;
	s = Q_stradd( s, va( "%i ", checksum ) );
	if ( s > max ) {
		// client-side overflow
		COM_WARN( LOG_CH(ch_filesystem), "WARNING: pure checksum list is too long (%i), you might be not able to play on remote server!\n", (int)(s - info) );
		*max = '\0';
	}

	return info;
}


/*
=====================
FS_ExcludeReference
=====================
*/
qboolean FS_ExcludeReference( void ) {
	const searchpath_t *search;
	const char *pakName;
	int i, nargs;
	qboolean x;

	if ( fs_excludeReference->string[0] == '\0' )
		return qfalse;

	Cmd_TokenizeStringIgnoreQuotes( fs_excludeReference->string );
	nargs = Cmd_Argc();
	x = qfalse;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->pack ) {
			if ( !search->pack->referenced ) {
				continue;
			}
			pakName = va( "%s/%s", search->pack->pakGamename, search->pack->pakBasename );
			for ( i = 0; i < nargs; i++ ) {
				if ( Q_stricmp( Cmd_Argv( i ), pakName ) == 0 ) {
					search->pack->exclude = qtrue;
					x = qtrue;
					break;
				}
			}
		}
	}

	return x;
}


/*
=====================
FS_ReferencedPakNames

Returns a space separated string containing the names of all referenced pk3 files.
The server will send this to the clients so they can check which files should be auto-downloaded.
=====================
*/
const char *FS_ReferencedPakNames( void ) {
	static char	info[BIG_INFO_STRING];
	const searchpath_t *search;
	const char *pakName;
	qstring_t info_qs = QS_Wrap( info, sizeof( info ) );

	// we want to return ALL pk3's from the fs_game path
	// and referenced one's from baseq3
	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( search->pack ) {
			if ( search->pack->exclude ) {
				continue;
			}
			if ( search->pack->referenced || !FS_IsBaseGame( search->pack->pakGamename ) ) {
				pakName = va( "%s/%s", search->pack->pakGamename, search->pack->pakBasename );
				if ( !QS_Empty( &info_qs ) ) {
					QS_AppendChar( &info_qs, ' ' );
				}
				QS_Append( &info_qs, pakName );
			}
		}
	}

	return info;
}


/*
=====================
FS_ClearPakReferences
=====================
*/
void FS_ClearPakReferences( int flags ) {
	const searchpath_t *search;

	if ( !flags ) {
		flags = -1;
	}
	for ( search = fs_searchpaths; search; search = search->next ) {
		// is the element a pak file and has it been referenced?
		if ( search->pack ) {
			search->pack->referenced &= ~flags;
		}
	}
}


/*
=====================
FS_ApplyDirPolicy

Set access rights for non-regular (pk3dir) directories
=====================
*/
static void FS_SetDirPolicy( dirPolicy_t policy ) {
	searchpath_t	*search;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->dir && search->policy != DIR_STATIC ) {
			search->policy = policy;
		}
	}
}


/*
=====================
FS_PureServerSetLoadedPaks

If the string is empty, all data sources will be allowed.
If not empty, only pk3 files that match one of the space
separated checksums will be checked for files, with the
exception of .cfg and .dat files.
=====================
*/
void FS_PureServerSetLoadedPaks( const char *pakSums, const char *pakNames ) {
	int		i, c, d;

	Cmd_TokenizeString( pakSums );

	c = Cmd_Argc();
	if ( c > ARRAY_LEN( fs_serverPaks ) ) {
		c = ARRAY_LEN( fs_serverPaks );
	}

	fs_numServerPaks = c;

	FS_SetDirPolicy( c ? DIR_DENY : DIR_ALLOW );

	for ( i = 0 ; i < c ; i++ ) {
		fs_serverPaks[i] = atoi( Cmd_Argv( i ) );
	}

	if ( fs_numServerPaks ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "Connected to a pure server.\n" );
	}
	else
	{
		if ( fs_reordered )
		{
			// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=540
			// force a restart to make sure the search order will be correct
			Com_Log( SEV_DEBUG, LOG_CH(ch_filesystem), "FS search reorder is required\n" );
			FS_Restart( fs_checksumFeed );
			return;
		}
	}

	for ( i = 0 ; i < ARRAY_LEN( fs_serverPakNames ) ; i++ ) {
		if ( fs_serverPakNames[i] ) {
			Z_Free( fs_serverPakNames[i] );
		}
		fs_serverPakNames[i] = NULL;
	}

	if ( pakNames && *pakNames ) {
		Cmd_TokenizeString( pakNames );

		d = Cmd_Argc();
		if ( d > ARRAY_LEN( fs_serverPakNames ) ) {
			d = ARRAY_LEN( fs_serverPakNames );
		}

		for ( i = 0 ; i < d ; i++ ) {
			fs_serverPakNames[i] = FS_CopyString( Cmd_Argv( i ) );
		}
	}
}


/*
=====================
FS_PureServerSetReferencedPaks

The checksums and names of the pk3 files referenced at the server
are sent to the client and stored here. The client will use these
checksums to see if any pk3 files need to be auto-downloaded.
=====================
*/
void FS_PureServerSetReferencedPaks( const char *pakSums, const char *pakNames ) {
	int		i, c, d = 0;

	Cmd_TokenizeString( pakSums );

	c = Cmd_Argc();
	if ( c > ARRAY_LEN( fs_serverReferencedPaks ) ) {
		c = ARRAY_LEN( fs_serverReferencedPaks );
	}

	for ( i = 0 ; i < c ; i++ ) {
		fs_serverReferencedPaks[i] = atoi( Cmd_Argv( i ) );
	}

	for ( i = 0 ; i < ARRAY_LEN( fs_serverReferencedPakNames ); i++ ) {
		if ( fs_serverReferencedPakNames[i] )
			Z_Free( fs_serverReferencedPakNames[i] );
		fs_serverReferencedPakNames[i] = NULL;
	}

	if ( pakNames && *pakNames ) {
		Cmd_TokenizeString( pakNames );

		d = Cmd_Argc();

		if ( d > c )
			d = c;

		for ( i = 0 ; i < d ; i++ ) {

			// Too long pak name may lose its extension during further processing
			if ( strlen( Cmd_Argv( i ) ) >= MAX_OSPATH-13 ) // + ".00000000.pk3"
				Com_Terminate( TERM_CLIENT_DROP, "Referenced pak name is too long: %s", Cmd_Argv( i ) );

			fs_serverReferencedPakNames[i] = FS_CopyString( Cmd_Argv( i ) );
		}
	}

	// ensure that there are as many checksums as there are pak names.
	if ( d < c )
		c = d;

	fs_numServerReferencedPaks = c;
}


/*
================
FS_InitFilesystem

Called only at initial startup, not when the filesystem
is resetting due to a game change
================
*/
void FS_InitFilesystem( void ) {
	// allow command line parms to override our defaults
	// we have to specially handle this, because normal command
	// line variable sets don't happen until after the filesystem
	// has already been initialized
	Com_StartupVariable( "fs_installpath" );
	Com_StartupVariable( "fs_homepath" );
	Com_StartupVariable( "fs_game" );
	Com_StartupVariable( "fs_basegame" );
	Com_StartupVariable( "fs_copyfiles" );
	Com_StartupVariable( "fs_restrict" );
#ifndef USE_HANDLE_CACHE
	Com_StartupVariable( "fs_locked" );
#endif

#ifdef _WIN32
 	_setmaxstdio( 2048 );
#endif

#if FEAT_SW3Z
	/* Eagerly initialize SW3Z's CRC32C lookup table so the runtime
	 * verify path is a pure lookup (no init check, no thread-safety
	 * concern if async loading is added later). Idempotent. */
	SW3Z_Init();
#endif

	// try to start up normally
	FS_Restart( 0 );
}


/*
================
FS_Restart
================
*/
void FS_Restart( int checksumFeed ) {

	// last valid game folder used
	static char lastValidGame[MAX_OSPATH];

	static qboolean execConfig = qfalse;

	// free anything we currently have loaded
	FS_Shutdown( qfalse );

	// set the checksum feed
	fs_checksumFeed = checksumFeed;

	// try to start up normally
	FS_Startup();

	// if we can't find default.cfg, assume that the paths are
	// busted and error out now, rather than getting an unreadable
	// graphics screen when the font fails to load
	if ( FS_ReadFile( "default.cfg", NULL ) <= 0 ) {
		// this might happen when connecting to a pure server not using BASEGAME/pak0.pk3
		// (for instance a TA demo server)
		if (lastValidGame[0]) {
			FS_PureServerSetLoadedPaks("", "");
			Cvar_Set( "fs_game", lastValidGame );
			lastValidGame[0] = '\0';
			Cvar_Set( "fs_restrict", "0" );
			execConfig = qtrue;
			FS_Restart( checksumFeed );
			Com_Terminate( TERM_CLIENT_DROP, "Invalid game folder" );
			return;
		}
		Com_Terminate( TERM_UNRECOVERABLE, "Couldn't load default.cfg" );
	}

	// new check before safeMode
	if ( Q_stricmp(fs_gamedirvar->string, lastValidGame) && execConfig ) {
		// skip the config.cfg if "safe" is on the command line
		if ( !Com_SafeMode() ) {
			Cbuf_AddText( "exec " WIRED_CONFIG_CFG "\n" );
		}
	}
	execConfig = qfalse;

	Q_strncpyz( lastValidGame, fs_gamedirvar->string, sizeof( lastValidGame ) );
}


/*
=================
FS_Reload
=================
*/
void FS_Reload( void )
{
	FS_Restart( fs_checksumFeed );
}


/*
=================
FS_ConditionalRestart
restart if necessary
=================
*/
qboolean FS_ConditionalRestart( int checksumFeed, qboolean clientRestart )
{
	qboolean gamedirChanged;
	{
		static int s_gamedirvar_mod = -1;
		gamedirChanged = ( s_gamedirvar_mod != -1 && fs_gamedirvar->modificationCount != s_gamedirvar_mod );
		s_gamedirvar_mod = fs_gamedirvar->modificationCount;
	}

	if ( gamedirChanged )
	{
		Com_GameRestart( checksumFeed, clientRestart );
		return qtrue;
	}
	if ( checksumFeed != fs_checksumFeed ) {
		FS_Restart( checksumFeed );
		return qtrue;
	}
	if ( fs_numServerPaks && !fs_reordered ) {
		FS_ReorderPurePaks();
	}

	return qfalse;
}


/*
========================================================================================

Handle based file calls for virtual machines

========================================================================================
*/

int	FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int		r;
	qboolean	sync;
	fileHandleData_t *fhd;

	if ( !qpath || !*qpath ) {
		if ( f )
			*f = FS_INVALID_HANDLE;
		return -1;
	}

	r = 0;	// file size
	sync = qfalse;

	switch( mode ) {
	case FS_READ:
		r = FS_FOpenFileRead( qpath, f, qtrue );
		break;
	case FS_WRITE:
		if ( f == NULL )
			return -1;
		*f = FS_FOpenFileWrite( qpath );
		break;
	case FS_APPEND_SYNC:
		sync = qtrue;
	case FS_APPEND:
		if ( f == NULL )
			return -1;
		*f = FS_FOpenFileAppend( qpath );
		break;
	default:
		Com_Terminate( TERM_UNRECOVERABLE, "FSH_FOpenFile: bad mode %i", mode );
		return -1;
	}

	if ( !f )
		return r;

	if ( *f == FS_INVALID_HANDLE ) {
		return -1;
	}

	fhd = &fsh[ *f ];

	fhd->handleSync = sync;

	return r;
}


int FS_FTell( fileHandle_t f ) {
	int pos;
#if FEAT_SW3Z
	if ( fsh[f].sw3zData ) {
		return fsh[f].sw3zPos;
	}
#endif
	if ( fsh[f].zipFile ) {
		pos = unztell( fsh[f].handleFiles.file.z );
	} else {
		pos = ftell( fsh[f].handleFiles.file.o );
	}
	return pos;
}


void FS_Flush( fileHandle_t f )
{
	fflush( fsh[f].handleFiles.file.o );
}


void FS_FilenameCompletion( const char *dir, const char *ext, qboolean stripExt, void(*callback)(const char *s), int flags ) {
	char	filename[ MAX_STRING_CHARS ];
	char	**filenames;
	int		nfiles;

	filenames = FS_ListFilteredFiles( dir, ext, NULL, &nfiles, flags );

	if ( nfiles >= 2 )
		FS_SortFileList( filenames, nfiles-1 );

	for( int i = 0; i < nfiles; i++ ) {

		Q_strncpyz( filename, filenames[ i ], sizeof( filename ) );
		FS_ConvertPath( filename );

		if ( stripExt ) {
			COM_StripExtension( filename, filename, sizeof( filename ) );
		}

		callback( filename );
	}
	FS_FreeFileList( filenames );
}


/*
	Secure VM functions
*/

int FS_VM_OpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode, handleOwner_t owner ) {
	int r = FS_FOpenFileByMode( qpath, f, mode );

	if ( f && *f != FS_INVALID_HANDLE )
		fsh[ *f ].owner = owner;

	return r;
}


int FS_VM_ReadFile( void *buffer, int len, fileHandle_t f, handleOwner_t owner ) {

	if ( f <= 0 || f >= MAX_FILE_HANDLES )
		return 0;

	if ( fsh[f].owner != owner )
		return 0;

	/* sw3z files have no OS file handle — data lives in sw3zData buffer */
	if ( !fsh[f].handleFiles.file.v
#if FEAT_SW3Z
		&& !fsh[f].sw3zData
#endif
		)
		return 0;

	return FS_Read( buffer, len, f );
}


void FS_VM_WriteFile( void *buffer, int len, fileHandle_t f, handleOwner_t owner ) {

	if ( f <= 0 || f >= MAX_FILE_HANDLES )
		return;

	if ( fsh[f].owner != owner || !fsh[f].handleFiles.file.v )
		return;

	FS_Write( buffer, len, f );
}


int FS_VM_SeekFile( fileHandle_t f, long offset, fsOrigin_t origin, handleOwner_t owner ) {
	if ( f <= 0 || f >= MAX_FILE_HANDLES )
		return -1;

	if ( fsh[f].owner != owner )
		return -1;

	if ( !fsh[f].handleFiles.file.v
#if FEAT_SW3Z
		&& !fsh[f].sw3zData
#endif
		)
		return -1;

	int r = FS_Seek( f, offset, origin );
	return r;
}


void FS_VM_CloseFile( fileHandle_t f, handleOwner_t owner ) {

	if ( f <= 0 || f >= MAX_FILE_HANDLES )
		return;

	if ( fsh[f].owner != owner )
		return;

	if ( !fsh[f].handleFiles.file.v
#if FEAT_SW3Z
		&& !fsh[f].sw3zData
#endif
		)
		return;

	FS_FCloseFile( f );
}


void FS_VM_CloseFiles( handleOwner_t owner )
{
	for ( int i = 1; i < MAX_FILE_HANDLES; i++ )
	{
		if ( fsh[i].owner != owner )
			continue;
		COM_WARN( LOG_CH(ch_filesystem), "%s:%i:%s leaked filehandle\n",
			FS_OwnerName( owner ), i, fsh[i].name );
		FS_FCloseFile( i );
	}
}


const char *FS_GetCurrentGameDir( void )
{
	if ( fs_gamedirvar->string[0] != '\0' )
		return fs_gamedirvar->string;

	return fs_basegame->string; // basegame
}


/* See qcommon.h for accessor contract. */
const char *FS_GetInstallPath( void )
{
	if ( fs_installpath && fs_installpath->string[0] != '\0' )
		return fs_installpath->string;
	return "";
}


const char *FS_GetInstallBinaryPath( void )
{
#ifdef __APPLE__
	static char path[ MAX_OSPATH ];
	const char *base = FS_GetInstallPath();
	if ( base[0] == '\0' )
		return "";
	Com_sprintf( path, sizeof( path ), "%s%cContents%cMacOS",
		base, PATH_SEP, PATH_SEP );
	return path;
#else
	return FS_GetInstallPath();
#endif
}


const char *FS_GetInstallResourcePath( void )
{
#ifdef __APPLE__
	static char path[ MAX_OSPATH ];
	const char *base = FS_GetInstallPath();
	if ( base[0] == '\0' )
		return "";
	Com_sprintf( path, sizeof( path ), "%s%cContents%cResources",
		base, PATH_SEP, PATH_SEP );
	return path;
#else
	return FS_GetInstallPath();
#endif
}


const char *FS_GetHomePath( void )
{
	if ( fs_homepath && fs_homepath->string[0] != '\0' )
		return fs_homepath->string;
	return FS_GetInstallPath();
}


fileHandle_t FS_PipeOpenWrite( const char *cmd, const char *filename ) {
	fileHandleData_t *fd;
	fileHandle_t f;
	const char *ospath;

	if ( !fs_searchpaths ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );
	}

	ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename );

	if ( fs_debug->integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "FS_PipeOpenWrite: %s\n", ospath );
	}

	FS_CheckFilenameIsNotAllowed( ospath, __func__, qfalse );

	f = FS_HandleForFile();
	fd = &fsh[ f ];
	FS_InitHandle( fd );

	if ( FS_CreatePath( ospath ) ) {
		return FS_INVALID_HANDLE;
	}

#ifdef _WIN32
	// NOLINTNEXTLINE(bugprone-command-processor) — FS_FOpenPipe is intentional: feeds frames to ffmpeg for AVI capture
	fd->handleFiles.file.o = _popen( cmd, "wb" );
#else
	// NOLINTNEXTLINE(bugprone-command-processor) — FS_FOpenPipe is intentional: feeds frames to ffmpeg for AVI capture
	fd->handleFiles.file.o = popen( cmd, "w" );
#endif

	if ( fd->handleFiles.file.o == NULL ) {
		return FS_INVALID_HANDLE;
	}

	Q_strncpyz( fd->name, filename, sizeof( fd->name ) );
	fd->handleSync = qfalse;
	fd->zipFile = qfalse;

	return f;
}


void FS_PipeClose( fileHandle_t f )
{
	if ( !fs_searchpaths )
		Com_Terminate( TERM_UNRECOVERABLE, "Filesystem call made without initialization" );

	if ( fsh[f].zipFile )
		return;

	if ( fsh[f].handleFiles.file.o ) {
#ifdef _WIN32
		_pclose( fsh[f].handleFiles.file.o );
#else
		pclose( fsh[f].handleFiles.file.o );
#endif
	}

	memset( &fsh[f], 0, sizeof( fsh[f] ) );
}


/*
=================
FS_LoadLibrary

Tries to load libraries within known searchpaths
=================
*/
void *FS_LoadLibrary( const char *name )
{
	const searchpath_t *sp = fs_searchpaths;
	void *libHandle = NULL;

	while ( !libHandle && sp ) {
		while ( sp && ( sp->policy != DIR_STATIC || !sp->dir ) ) {
			sp = sp->next;
		}
		if ( sp ) {
			const char *fn = FS_BuildOSPath( sp->dir->path, sp->dir->gamedir, name );
			libHandle = Sys_LoadLibrary( fn );
			sp = sp->next;
		}
	}

	if ( libHandle ) {
		Com_Log( SEV_INFO, LOG_CH(ch_filesystem), "Sys_LoadLibrary(%s): loaded\n", name );
	}

	return libHandle;
}
