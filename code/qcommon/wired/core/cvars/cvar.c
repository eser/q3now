// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cvar.c -- dynamic variable tracking

#include "q_shared.h"
#include "qcommon.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_system, "system" );

static cvar_t	*cvar_vars = NULL;
static cvar_t	*cvar_cheats;
int			cvar_modifiedFlags;

#define	MAX_CVARS	4096
static cvar_t	cvar_indexes[MAX_CVARS];
static int		cvar_numIndexes;

static int	cvar_group[ CVG_MAX ];

#define CVAR_MAX_CALLBACK_DEPTH 8
static int cvar_callbackDepth = 0;

#define FILE_HASH_SIZE		256
static	cvar_t	*hashTable[FILE_HASH_SIZE];
static	qboolean cvar_sort = qfalse;

static void Com_WriteConfig_f( void );

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = locase[(byte)fname[i]];
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}


/*
============
Cvar_ValidateName
============
*/
static qboolean Cvar_ValidateName( const char *name ) {
	if ( !name ) {
		return qfalse;
	}

	const char *s = name;
	int c;
	while ( (c = (byte)*s++) != '\0' ) {
		if ( c == '\\' || c == '\"' || c == ';' || c == '%' || c <= ' ' || c >= '~' )
			return qfalse;
	}

	if ( (s - name) >= MAX_STRING_CHARS ) {
		return qfalse;
	}

	return qtrue;
}


/*
============
Cvar_FindVar
============
*/
static cvar_t *Cvar_FindVar( const char *var_name ) {
	cvar_t	*var;
	long hash;

	if ( !var_name )
		return NULL;

	hash = generateHashValue( var_name );

	for ( var = hashTable[ hash ] ; var ; var = var->hashNext ) {
		if ( !Q_stricmp( var_name, var->name ) ) {
			return var;
		}
	}

	return NULL;
}


/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue( const char *var_name ) {
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->value;
}


/*
============
Cvar_VariableIntegerValue
============
*/
int Cvar_VariableIntegerValue( const char *var_name ) {
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->integer;
}


/*
============
Cvar_VariableString
============
*/
const char *Cvar_VariableString( const char *var_name ) {
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}


/*
============
Cvar_VariableStringBuffer
============
*/
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var) {
		*buffer = '\0';
	}
	else {
		Q_strncpyz( buffer, var->string, bufsize );
	}
}


/*
============
Cvar_VariableStringBufferSafe
============
*/
void Cvar_VariableStringBufferSafe( const char *var_name, char *buffer, int bufsize, int flag ) {
	cvar_t *var;

	var = Cvar_FindVar( var_name );
	if ( !var || var->flags & flag ) {
		*buffer = '\0';
	}
	else {
		Q_strncpyz( buffer, var->string, bufsize );
	}
}


/*
============
Cvar_Flags
============
*/
unsigned Cvar_Flags( const char *var_name )
{
	const cvar_t *var;

	if ( ( var = Cvar_FindVar( var_name ) ) == NULL )
		return CVAR_NONEXISTENT;

	return var->flags;
}


/*
============
Cvar_CommandCompletion
============
*/
void Cvar_CommandCompletion( void (*callback)(const char *s) )
{
	const cvar_t *cvar;

	for ( cvar = cvar_vars; cvar; cvar = cvar->next ) {
		if ( cvar->name && ( cvar->flags & CVAR_NOTABCOMPLETE ) == 0 ) {
			callback( cvar->name );
		}
	}
}


static qboolean Cvar_IsIntegral( const char *s ) {

	if ( *s == '-' && *(s+1) != '\0' )
		s++;

	while ( *s != '\0' ) {
		if ( *s < '0' || *s > '9' ) {
			return qfalse;
		}
		s++;
	}

	return qtrue;
}


/*
============
Cvar_Validate
============
*/
static const char *Cvar_Validate( cvar_t *var, const char *value, qboolean warn )
{
	static char intbuf[ 32 ];

	if ( var->validator == CV_NONE )
		return value;

	if ( !value )
		return value;

	const char *limit = NULL;

	if ( var->validator == CV_INTEGER || var->validator == CV_FLOAT ) {
		if ( !Q_isanumber( value ) ) {
			if ( warn )
				Com_Log( SEV_INFO, LOG_CH(ch_system), "WARNING: cvar '%s' must be numeric", var->name );
			limit = var->resetString;
		} else {
			if ( var->validator == CV_INTEGER ) {
				if ( !Cvar_IsIntegral( value ) ) {
					if ( warn )
						Com_Log( SEV_INFO, LOG_CH(ch_system), "WARNING: cvar '%s' must be integral", var->name );
					sprintf( intbuf, "%i", atoi( value ) );
					value = intbuf; // new value
				}
				int valuei = atoi( value );
				if ( var->mins && valuei < atoi( var->mins ) ) {
					limit = var->mins;
				} else if ( var->maxs && valuei > atoi( var->maxs ) ) {
					limit = var->maxs;
				}
			} else { // CV_FLOAT
				float valuef = Q_atof( value );
				if ( var->mins && valuef < Q_atof( var->mins ) ) {
					limit = var->mins;
				} else if ( var->maxs && valuef > Q_atof( var->maxs ) ) {
					limit = var->maxs;
				}
			}

			if ( warn ) {
				if ( limit && ( limit == var->mins || limit == var->maxs ) ) {
					if ( value == intbuf ) { // cast to integer
						Com_Log( SEV_INFO, LOG_CH(ch_system), " and" );
					} else {
						Com_Log( SEV_INFO, LOG_CH(ch_system), "WARNING: cvar '%s'", var->name );
					}
					Com_Log( SEV_INFO, LOG_CH(ch_system), " is out of range (%s '%s')", (limit == var->mins) ? "min" : "max", limit );
				}
			}
		} // Q_isanumber
	} // CV_INTEGER || CV_FLOAT
	// TODO: stringlist
	else if ( var->validator == CV_FSPATH ) {
		// check for directory traversal patterns
		if ( FS_InvalidGameDir( value ) ) {
			if ( warn ) {
				Com_Log( SEV_INFO, LOG_CH(ch_system), "WARNING: cvar '%s' contains invalid patterns", var->name );
			}
			// try to use current value if it is valid
			if ( !FS_InvalidGameDir( var->string ) ) {
				if ( warn ) {
					Com_Log( SEV_INFO, LOG_CH(ch_system), "\n" );
				}
				return var->string;
			}
			limit = var->resetString;
		}
	}

	if ( limit || value == intbuf ) {
		if ( !limit )
			limit = value;
		if ( warn )
			Com_Log( SEV_INFO, LOG_CH(ch_system), ", setting to '%s'\n", limit );
		return limit;
	}
	return value;
}


/*
============
Cvar_Get

If the variable already exists, the value will not be set unless CVAR_ROM
The flags will be or'ed in if the variable exists.
============
*/
cvar_t *Cvar_Get( const char *var_name, const char *var_value, int flags ) {
	cvar_t	*var;
	long	hash;
	int	index;

	if ( !var_name || !var_value ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Cvar_Get: NULL parameter" );
	}

	if ( !Cvar_ValidateName( var_name ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

#if 0 // FIXME: values with backslash happen
	if ( !Cvar_ValidateString( var_value ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "invalid cvar value string: %s\n", var_value );
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar (var_name);

	if(var)
	{
		int vm_created = (flags & CVAR_VM_CREATED);
		var_value = Cvar_Validate(var, var_value, qfalse);

		// Make sure the game code cannot mark engine-added variables as gamecode vars
		if(var->flags & CVAR_VM_CREATED)
		{
			if ( !vm_created )
				var->flags &= ~CVAR_VM_CREATED;
		}
		else if (!(var->flags & CVAR_USER_CREATED))
		{
			if ( vm_created )
				flags &= ~CVAR_VM_CREATED;
		}

		// if the C code is now specifying a variable that the user already
		// set a value for, take the new value as the reset value
		if(var->flags & CVAR_USER_CREATED)
		{
			var->flags &= ~CVAR_USER_CREATED;
			Z_Free( var->resetString );
			var->resetString = CopyString( var_value );

			if ( flags & CVAR_ROM )
			{
				// this variable was set by the user,
				// so force it to value given by the engine.

				if(var->latchedString)
					Z_Free(var->latchedString);

				var->latchedString = CopyString(var_value);
			}
		}

		// Make sure servers cannot mark engine-added variables as SERVER_CREATED
		if ( var->flags & CVAR_SERVER_CREATED )
		{
			if ( !( flags & CVAR_SERVER_CREATED ) ) {
				// reset server-created flag
				var->flags &= ~CVAR_SERVER_CREATED;
				if ( vm_created ) {
					// reset to state requested by local VM module
					var->flags &= ~CVAR_ROM;
					Z_Free( var->resetString );
					var->resetString = CopyString( var_value );
					if ( var->latchedString )
						Z_Free( var->latchedString );
					var->latchedString = CopyString( var_value );
				}
			}
		}
		else
		{
			if ( flags & CVAR_SERVER_CREATED )
				flags &= ~CVAR_SERVER_CREATED;
		}

		// Remember whether this cvar was created by a command-line +set
		// before OR'ing in the new flags; we clear the bit immediately
		// because it's only meaningful until the matching Cvar_Get.
		{
			const qboolean cmdLineCreated = ( var->flags & CVAR_CMDLINE_CREATED ) != 0;
			var->flags |= flags;
			var->flags &= ~CVAR_CMDLINE_CREATED;

			// only allow one non-empty reset string without a warning
			if ( !var->resetString[0] ) {
				// we don't have a reset string yet
				Z_Free( var->resetString );
				var->resetString = CopyString( var_value );
			} else if ( var_value[0] && strcmp( var->resetString, var_value ) != 0 ) {
				Com_Log( SEV_DEBUG, LOG_CH(ch_system), "Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
					var_name, var->resetString, var_value );
			}

			// if we have a latched string, take that value now
			if ( var->latchedString ) {
				char *s;

				s = var->latchedString;
				var->latchedString = NULL;	// otherwise cvar_set2 would free it
				Cvar_Set2( var_name, s, qtrue );
				Z_Free( s );
			}

			// when registering an already-existing cvar with CVAR_ROM,
			// PRESERVE its current runtime value.
			// Re-registration must NOT reset it back to var_value, otherwise
			// engine-managed cvars like `mapname` get clobbered to their
			// default the moment any code path re-registers them.
			//
			// CVAR_INIT also defers to the runtime value when the cvar was
			// first created from the command line, so `+set sv_pure 0` wins
			// over a later engine-side default of "1".
			(void)cmdLineCreated;

			// ZOID--needs to be set so that cvars the game sets as
			// SERVERINFO get sent to clients
			cvar_modifiedFlags |= flags;
		}

		return var;
	}

	//
	// allocate a new cvar
	//

	// find a free cvar
	for(index = 0; index < MAX_CVARS; index++)
	{
		if(!cvar_indexes[index].name)
			break;
	}

	if(index >= MAX_CVARS)
	{
		if(!com_errorEntered)
			Com_Terminate( TERM_UNRECOVERABLE, "Error: Too many cvars, cannot create a new one!");

		return NULL;
	}

	var = &cvar_indexes[index];

	if(index >= cvar_numIndexes)
		cvar_numIndexes = index + 1;

	var->name = CopyString( var_name );
	var->string = CopyString( var_value );
	var->modificationCount = 1;
	var->value = Q_atof( var->string );
	var->integer = atoi( var->string );
	var->resetString = CopyString( var_value );
	var->validator = CV_NONE;
	var->description = NULL;
	var->group = CVG_NONE;
	cvar_group[ var->group ] = 1;

	// link the variable in
	var->next = cvar_vars;
	if ( cvar_vars )
		cvar_vars->prev = var;

	var->prev = NULL;
	cvar_vars = var;

	var->flags = flags;
	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= var->flags;

	hash = generateHashValue(var_name);
	var->hashIndex = hash;

	var->hashNext = hashTable[hash];
	if ( hashTable[hash] )
		hashTable[hash]->hashPrev = var;

	var->hashPrev = NULL;
	hashTable[hash] = var;

	 // sort on write
	cvar_sort = qtrue;

	return var;
}


static void Cvar_QSortByName( cvar_t **a, int n )
{
	cvar_t *m = a[ n>>1 ];
	int i = 0, j = n;

	do {
		// sort in descending order
		while ( strcmp( a[i]->name, m->name ) > 0 ) i++;
		while ( strcmp( a[j]->name, m->name ) < 0 ) j--;

		if ( i <= j ) {
			cvar_t *temp = a[i];
			a[i] = a[j];
			a[j] = temp;
			i++;
			j--;
		}
	} while ( i <= j );

	if ( j > 0 ) Cvar_QSortByName( a, j );
	if ( n > i ) Cvar_QSortByName( a+i, n-i );
}


static void Cvar_Sort( void )
{
	cvar_t *list[ MAX_CVARS ];
	int count = 0;

	for ( cvar_t *var = cvar_vars; var; var = var->next ) {
		if ( var->name ) {
			list[ count++ ] = var;
		} else {
			Com_Terminate( TERM_UNRECOVERABLE, "%s: NULL cvar name", __func__ );
		}
	}

	if ( count < 2 ) {
		return; // nothing to sort
	}

	Cvar_QSortByName( &list[0], count-1 );

	cvar_vars = NULL;

	// relink cvars
	for ( int i = 0; i < count; i++ ) {
		cvar_t *var = list[ i ];
		// link the variable in
		var->next = cvar_vars;
		if ( cvar_vars )
			cvar_vars->prev = var;
		var->prev = NULL;
		cvar_vars = var;
	}
}


/*
============
Cvar_Print

Prints the value, default, and latched string of the given variable
============
*/
static void Cvar_Print( const cvar_t *v ) {

	Com_Log( SEV_INFO, LOG_CH(ch_system), "\"%s\" is:\"%s" S_COLOR_WHITE "\"",
		v->name, v->string );

	if ( !( v->flags & CVAR_ROM ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), " default:\"%s" S_COLOR_WHITE "\"",
			v->resetString );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_system), "\n");

	if ( v->latchedString ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "latched: \"%s\"\n", v->latchedString );
	}

	if ( v->description ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "%s\n", v->description );
	}
}


// -------------------------------------------------------------------------
// Typed-validation helpers (used by Cvar_ValidateTyped and Cvar_Set2)
// -------------------------------------------------------------------------

static qboolean Cvar_ParseBool( const char *value, int *outInteger )
{
    if ( !Q_stricmp(value,"1") || !Q_stricmp(value,"true")
      || !Q_stricmp(value,"yes") || !Q_stricmp(value,"on") ) {
        *outInteger = 1;
        return qtrue;
    }
    if ( !Q_stricmp(value,"0") || !Q_stricmp(value,"false")
      || !Q_stricmp(value,"no") || !Q_stricmp(value,"off") ) {
        *outInteger = 0;
        return qtrue;
    }
    return qfalse;
}

static qboolean Cvar_ParseInt( const char *value, int *outInteger )
{
    char *endptr;
    long  result;
    if ( !value || !*value ) return qfalse;
    result = strtol( value, &endptr, 10 );
    if ( *endptr != '\0' ) return qfalse; // trailing garbage
    *outInteger = (int)result;
    return qtrue;
}

static qboolean Cvar_ParseFloat( const char *value, float *outFloat )
{
    char  *endptr;
    float  result;
    if ( !value || !*value ) return qfalse;
    result = strtof( value, &endptr );
    if ( *endptr != '\0' ) return qfalse; // trailing garbage
    *outFloat = result;
    return qtrue;
}

// Returns a comma-separated list of valid enum values for error messages.
// Uses a static buffer — only called on the rejection (cold) path.
static const char *Cvar_EnumValuesString( const cvar_t *var )
{
    static char buf[512];
    int i, n = 0;
    buf[0] = '\0';
    for ( i = 0; i < var->enumCount; i++ ) {
        int len;
        if ( i > 0 && n < (int)sizeof(buf) - 3 ) {
            buf[n++] = ',';
            buf[n++] = ' ';
        }
        len = (int)strlen( var->enumValues[i] );
        if ( n + len >= (int)sizeof(buf) - 1 ) break;
        memcpy( buf + n, var->enumValues[i], len );
        n += len;
    }
    buf[n] = '\0';
    return buf;
}

// Cvar_ValidateTyped: type-aware validation for Cvar_Set2.
// Returns qtrue if value is accepted.
// On acceptance, *outInteger and *outFloat receive the parsed numeric values.
// *outNormalized receives a canonical string if the input needs normalization
// (bool "true"→"1"; enum "3"→"circle"); NULL means no normalization needed.
// On rejection, a SEV_WARN is logged and qfalse is returned.
// CVT_STRING always returns qtrue.
static qboolean Cvar_ValidateTyped( cvar_t *var, const char *value,
                                     int *outInteger, float *outFloat,
                                     const char **outNormalized )
{
    *outNormalized = NULL;

    switch ( var->type ) {
    case CVT_STRING:
        *outInteger = atoi( value );
        *outFloat   = (float)atof( value );
        return qtrue;

    case CVT_BOOL: {
        if ( !Cvar_ParseBool( value, outInteger ) ) {
            Com_Log( SEV_WARN, LOG_CH(ch_system),
                "%s: invalid boolean '%s' (valid: 0, 1, true, false, yes, no, on, off)\n",
                var->name, value );
            return qfalse;
        }
        *outFloat      = (float)*outInteger;
        *outNormalized = *outInteger ? "1" : "0";
        return qtrue;
    }

    case CVT_INT: {
        if ( !Cvar_ParseInt( value, outInteger ) ) {
            Com_Log( SEV_WARN, LOG_CH(ch_system),
                "%s: '%s' is not a valid integer\n", var->name, value );
            return qfalse;
        }
        if ( var->typeMin != var->typeMax ) { // min==max==0 means no range check
            if ( *outInteger < (int)var->typeMin || *outInteger > (int)var->typeMax ) {
                Com_Log( SEV_WARN, LOG_CH(ch_system),
                    "%s: value %d out of range [%d, %d]\n",
                    var->name, *outInteger, (int)var->typeMin, (int)var->typeMax );
                return qfalse;
            }
        }
        *outFloat = (float)*outInteger;
        return qtrue;
    }

    case CVT_FLOAT: {
        if ( !Cvar_ParseFloat( value, outFloat ) ) {
            Com_Log( SEV_WARN, LOG_CH(ch_system),
                "%s: '%s' is not a valid number\n", var->name, value );
            return qfalse;
        }
        if ( var->typeMin != var->typeMax ) {
            if ( *outFloat < var->typeMin || *outFloat > var->typeMax ) {
                Com_Log( SEV_WARN, LOG_CH(ch_system),
                    "%s: value %g out of range [%g, %g]\n",
                    var->name, *outFloat, var->typeMin, var->typeMax );
                return qfalse;
            }
        }
        *outInteger = (int)*outFloat;
        return qtrue;
    }

    case CVT_ENUM: {
        int idx;
        for ( idx = 0; idx < var->enumCount; idx++ ) {
            if ( Q_stricmp( value, var->enumValues[idx] ) == 0 ) {
                *outInteger    = idx;
                *outFloat      = (float)idx;
                *outNormalized = var->enumValues[idx];
                return qtrue;
            }
        }
        Com_Log( SEV_WARN, LOG_CH(ch_system),
            "%s: '%s' is not a valid option (valid: %s)\n",
            var->name, value, Cvar_EnumValuesString( var ) );
        return qfalse;
    }
    } // switch

    return qfalse;
}


/*
============
Cvar_Set2
============
*/
cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force ) {
	cvar_t     *var;
	int         newInteger = 0;
	float       newFloat   = 0.0f;
	const char *normalized = NULL;

//	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "Cvar_Set2: %s %s\n", var_name, value );

	if ( !Cvar_ValidateName( var_name ) )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_system), "invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

#if 0	// FIXME
	if ( value && !Cvar_ValidateString( value ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "invalid cvar value string: %s\n", value );
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar( var_name );
	if ( !var )
	{
		if ( !value )
			return NULL;
		// create it
		if ( !force )
			return Cvar_Get( var_name, value, CVAR_USER_CREATED );
		return Cvar_Get( var_name, value, 0 );
	}

	if ( var->flags & (CVAR_ROM | CVAR_INIT | CVAR_CHEAT) && !force )
	{
		if ( var->flags & CVAR_ROM )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_system), "%s is read only.\n", var_name );
			return var;
		}

		if ( var->flags & CVAR_INIT )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_system), "%s is write protected.\n", var_name );
			return var;
		}

		if ( (var->flags & CVAR_CHEAT) && cvar_cheats && !cvar_cheats->integer )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_system), "%s is cheat protected.\n", var_name );
			return var;
		}

	}

	if ( !value )
		value = var->resetString;

	value = Cvar_Validate( var, value, qtrue );

	// Typed validation: reject invalid values before ANY state change.
	// CVT_STRING cvars and legacy (Cvar_Get) cvars skip this entirely.
	// Normalization (bool "true"→"1"; enum index→canonical name) is applied
	// here so all downstream paths (latch storage, string copy) use the
	// canonical form.
	if ( var->type != CVT_STRING ) {
		if ( !Cvar_ValidateTyped( var, value, &newInteger, &newFloat, &normalized ) ) {
			return var; // rejected — cvar value unchanged
		}
		if ( normalized ) {
			value = normalized;
		}
	}

	if ( (var->flags & CVAR_LATCH) && var->latchedString )
	{
		if ( strcmp( value, var->string ) == 0 )
		{
			Z_Free( var->latchedString );
			var->latchedString = NULL;
			return var;
		}

		if ( strcmp( value, var->latchedString ) == 0 )
			return var;
	}
	else if ( strcmp( value, var->string ) == 0 )
		return var;

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= var->flags;

	if ( !force )
	{
		if ( var->flags & CVAR_LATCH )
		{
			if ( var->latchedString )
			{
				if ( strcmp( value, var->latchedString ) == 0 )
					return var;
				Z_Free( var->latchedString );
			}
			else
			{
				if ( strcmp( value, var->string ) == 0 )
					return var;
			}

			Com_Log( SEV_INFO, LOG_CH(ch_system), "%s will be changed upon restarting.\n", var_name );
			var->latchedString = CopyString( value );
			var->modificationCount++;
			cvar_group[ var->group ] = 1;
			return var;
		}
	}
	else
	{
		if ( var->latchedString )
		{
			Z_Free( var->latchedString );
			var->latchedString = NULL;
		}
	}

	if ( strcmp( value, var->string ) == 0 )
		return var; // not changed

	var->modificationCount++;
	cvar_group[ var->group ] = 1;

	Z_Free( var->string ); // free the old value string

	var->string = CopyString( value );
	// For typed cvars use the pre-parsed values from Cvar_ValidateTyped above —
	// no double-parsing, and the correct typed semantics (e.g. enum index).
	// For CVT_STRING (legacy) fall back to runtime parsing.
	if ( var->type != CVT_STRING ) {
		var->value   = newFloat;
		var->integer = newInteger;
	} else {
		var->value   = Q_atof( var->string );
		var->integer = atoi( var->string );
	}

	if ( var->onChange ) {
		if ( cvar_callbackDepth < CVAR_MAX_CALLBACK_DEPTH ) {
			cvar_callbackDepth++;
			var->onChange( var );
			cvar_callbackDepth--;
		} else {
			Com_Log( SEV_WARN, LOG_CH(ch_system), "Cvar callback depth %d exceeded for '%s'\n",
				CVAR_MAX_CALLBACK_DEPTH, var->name );
		}
	}

	return var;
}


/*
============
Cvar_CheatsWereDisabled
============
*/
void Cvar_CheatsWereDisabled(void) {
    cvar_t *var;

	for (var = cvar_vars; var; var = var->next) {
        if (!(var->flags & CVAR_CHEAT))
            continue;

		if (strcmp(var->string, var->resetString) == 0)
            continue;

		Com_Log( SEV_INFO, LOG_CH(ch_system), "%s reset to default.\n", var->name);
        Cvar_Set2(var->name, var->resetString, qtrue); // force reset
    }
}


/*
============
Cvar_Set
============
*/
void Cvar_Set( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qtrue);
}


/*
============
Cvar_SetSafe
============
*/
void Cvar_SetSafe( const char *var_name, const char *value )
{
	unsigned flags = Cvar_Flags( var_name );
	qboolean force = qtrue;

	if ( flags != CVAR_NONEXISTENT )
	{
		if ( flags & ( CVAR_PROTECTED | CVAR_PRIVATE ) )
		{
			if( value )
				COM_WARN( LOG_CH(ch_system), "Restricted source tried to set "
					"\"%s\" to \"%s\"\n", var_name, value );
			else
				COM_WARN( LOG_CH(ch_system), "Restricted source tried to "
					"modify \"%s\"\n", var_name );
			return;
		}

		// don't let VMs or server change engine latched cvars instantly
		//if ( ( flags & CVAR_LATCH ) && !( flags & CVAR_VM_CREATED ) )
		//{
		//	force = qfalse;
		//}
	}

	Cvar_Set2( var_name, value, force );
}


/*
============
Cvar_SetLatched
============
*/
void Cvar_SetLatched( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qfalse);
}


/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue( const char *var_name, float value) {
	char	val[32];

	if ( value == (int)value ) {
		Com_sprintf (val, sizeof(val), "%i",(int)value);
	} else {
		Com_sprintf (val, sizeof(val), "%f",value);
	}
	Cvar_Set (var_name, val);
}


/*
============
Cvar_SetIntegerValue
============
*/
void Cvar_SetIntegerValue( const char *var_name, int value ) {
	char	val[32];

	sprintf( val, "%i", value );
	Cvar_Set( var_name, val );
}


/*
============
Cvar_SetValueSafe
============
*/
void Cvar_SetValueSafe( const char *var_name, float value )
{
	char val[32];

	if( Q_isintegral( value ) )
		Com_sprintf( val, sizeof(val), "%i", (int)value );
	else
		Com_sprintf( val, sizeof(val), "%f", value );
	Cvar_SetSafe( var_name, val );
}


/*
============
Cvar_Reset
============
*/
void Cvar_Reset( const char *var_name ) {
	Cvar_Set2( var_name, NULL, qfalse );
}

/*
============
Cvar_ForceReset
============
*/
void Cvar_ForceReset(const char *var_name)
{
	Cvar_Set2(var_name, NULL, qtrue);
}


/*
============
Cvar_LatchedVariableStringBuffer
============
*/
void Cvar_LatchedVariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	cvar_t *var = Cvar_FindVar( var_name );
	if ( var && var->latchedString ) {
		Q_strncpyz( buffer, var->latchedString, bufsize );
	} else {
		buffer[0] = '\0';
	}
}


/*
============
Cvar_DefaultVariableStringBuffer
============
*/
void Cvar_DefaultVariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	cvar_t *var = Cvar_FindVar( var_name );
	if ( var && var->resetString ) {
		Q_strncpyz( buffer, var->resetString, bufsize );
	} else {
		buffer[0] = '\0';
	}
}


/*
============
Cvar_SetDefault
============
*/
void Cvar_SetDefault( const char *var_name, const char *value ) {
	cvar_t *var = Cvar_FindVar( var_name );
	if ( var ) {
		Z_Free( var->resetString );
		var->resetString = CopyString( value );
	}
}


/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command( void ) {
	cvar_t	*v;

	// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v) {
		return qfalse;
	}

	// perform a variable print or set
	if ( Cmd_Argc() == 1 ) {
		Cvar_Print( v );
		return qtrue;
	}

	// set the value if forcing isn't required
	Cvar_Set2( v->name, Cmd_ArgsFrom( 1 ), qfalse );
	return qtrue;
}


/*
============
Cvar_Print_f

Prints the contents of a cvar
(preferred over Cvar_Command where cvar names and commands conflict)
============
*/
static void Cvar_Print_f( void )
{
	const char *name;
	cvar_t *cv;

	if(Cmd_Argc() != 2)
	{
		Com_Log( SEV_INFO, LOG_CH(ch_system), "usage: print <variable>\n");
		return;
	}

	name = Cmd_Argv(1);

	cv = Cvar_FindVar(name);

	if(cv)
		Cvar_Print(cv);
	else
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Cvar %s does not exist.\n", name);
}


/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding, optionally through a list of
given values
============
*/
static void Cvar_Toggle_f( void ) {
	int		i, c;
	const char	*curval;

	c = Cmd_Argc();
	if ( c < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "usage: toggle <variable> [value1, value2, ...]\n" );
		return;
	}

	if ( c == 2 ) {
		Cvar_Set2( Cmd_Argv( 1 ), va( "%d", !Cvar_VariableValue( Cmd_Argv( 1 ) ) ),
			qfalse );
		return;
	}

	if ( c == 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "toggle: nothing to toggle to\n" );
		return;
	}

	curval = Cvar_VariableString( Cmd_Argv( 1 ) );

	// don't bother checking the last arg for a match since the desired
	// behaviour is the same as no match (set to the first argument)
	for ( i = 2; i + 1 < c; i++ ) {
		if ( strcmp( curval, Cmd_Argv( i ) ) == 0 ) {
			Cvar_Set2( Cmd_Argv( 1 ), Cmd_Argv(i + 1), qfalse );
			return;
		}
	}

	// fallback
	Cvar_Set2( Cmd_Argv( 1 ), Cmd_Argv( 2 ), qfalse );
}


/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
static void Cvar_Set_f( void ) {
	int		c;
	const char	*cmd;
	cvar_t	*v;

	c = Cmd_Argc();
	cmd = Cmd_Argv(0);

	if ( c < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "usage: %s <variable> <value>\n", cmd);
		return;
	}
	if ( c == 2 ) {
		Cvar_Print_f();
		return;
	}

	v = Cvar_Set2 (Cmd_Argv(1), Cmd_ArgsFrom(2), qfalse);
	if( !v ) {
		return;
	}
	switch( cmd[3] ) {
		case 'a':
			if( !( v->flags & CVAR_ARCHIVE ) ) {
				v->flags |= CVAR_ARCHIVE;
				cvar_modifiedFlags |= CVAR_ARCHIVE;
			}
			break;
		case 'u':
			if( !( v->flags & CVAR_USERINFO ) ) {
				v->flags |= CVAR_USERINFO;
				cvar_modifiedFlags |= CVAR_USERINFO;
			}
			break;
		case 's':
			if( !( v->flags & CVAR_SERVERINFO ) ) {
				v->flags |= CVAR_SERVERINFO;
				cvar_modifiedFlags |= CVAR_SERVERINFO;
			}
			break;
	}
}


/*
============
Cvar_Reset_f
============
*/
static void Cvar_Reset_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "usage: reset <variable>\n");
		return;
	}
	Cvar_Reset( Cmd_Argv( 1 ) );
}


// returns NULL for non-existent "-" argument
static const char *GetValue( int index, int *ival, float *fval )
{
	static char buf[ MAX_CVAR_VALUE_STRING ];
	const char *cmd;
	cvar_t	*var;

	cmd = Cmd_Argv( index );

	if ( ( *cmd == '-' && *(cmd+1) == '\0' ) || *cmd == '\0' ) {
		*ival = 0;
		*fval = 0.0f;
		buf[0] = '\0';
		return NULL;
	}

	var = Cvar_FindVar( cmd );
	if ( !var ) // cvar not found, return string
	{
		*ival = atoi( cmd );
		*fval = Q_atof( cmd );
		Q_strncpyz( buf, cmd, sizeof( buf ) );
		return buf;
	}
	// found cvar, extract values
	*ival = var->integer;
	*fval = var->value;
	Q_strncpyz( buf, var->string, sizeof( buf ) );
	return buf;
}


typedef enum {
	FT_BAD = 0,
	FT_ADD,
	FT_SUB,
	FT_MUL,
	FT_DIV,
	FT_MOD,
	FT_SIN,
	FT_COS,
	FT_RAND,
} funcType_t;


static funcType_t GetFuncType( void )
{
	const char *cmd;
	cmd = Cmd_Argv( 1 );
	if ( !Q_stricmp( cmd, "add" ) )
		return FT_ADD;
	if ( !Q_stricmp( cmd, "sub" ) )
		return FT_SUB;
	if ( !Q_stricmp( cmd, "mul" ) )
		return FT_MUL;
	if ( !Q_stricmp( cmd, "div" ) )
		return FT_DIV;
	if ( !Q_stricmp( cmd, "mod" ) )
		return FT_MOD;
	if ( !Q_stricmp( cmd, "sin" ) )
		return FT_SIN;
	if ( !Q_stricmp( cmd, "cos" ) )
		return FT_COS;
	if ( !Q_stricmp( cmd, "rand" ) )
		return FT_RAND;

	return FT_BAD;
}


static qboolean AllowEmptyCvar( funcType_t ftype )
{
	switch ( ftype ) {
		case FT_ADD:
		case FT_SUB:
		case FT_MUL:
		case FT_DIV:
		case FT_MOD:
			return qfalse;
		default:
			return qtrue;
	};
}


static void Cvar_Op( funcType_t ftype, int *ival, float *fval )
{
	int imod;
	float fmod;

	GetValue( 3, &imod, &fmod ); // index 3: value

	switch ( ftype ) {
		case FT_ADD:
			*ival += imod;
			*fval += fmod;
			break;
		case FT_SUB:
			*ival -= imod;
			*fval -= fmod;
			break;
		case FT_MUL:
			*ival *= imod;
			*fval *= fmod;
			break;
		case FT_DIV:
			if ( imod )
				*ival /= imod;
			if ( fmod )
				*fval /= fmod;
			break;
		case FT_MOD:
			if ( imod ) {
				*ival %= imod;
				*fval = (float)( (int)*fval % imod ); // FIXME: use float
			}
			break;

		case FT_SIN:
				*ival = sin( imod );
				*fval = sin( fmod );
				break;

		case FT_COS:
				*ival = cos( imod );
				*fval = cos( fmod );
				break;
		default:
			break;
	}

	if ( Cmd_Argc() > 4 ) { // low bound
		int icap; float fcap;
		if ( GetValue( 4, &icap, &fcap ) ) {
			if ( *ival < icap ) *ival = icap;
			if ( *fval < fcap ) *fval = fcap;
		}
	}
	if ( Cmd_Argc() > 5 ) { // high bound
		int icap; float fcap;
		if ( GetValue( 5, &icap, &fcap ) ) {
			if ( *ival > icap ) *ival = icap;
			if ( *fval > fcap ) *fval = fcap;
		}
	}
}


static void Cvar_Rand( int *ival, float *fval )
{
	*ival = rand();
	*fval = *ival;

	if ( Cmd_Argc() > 3 ) { // base
		int icap; float fcap;
		if ( GetValue( 3, &icap, &fcap ) ) {
			*ival += icap;
			*fval = *ival;
		}
	}
	if ( Cmd_Argc() > 4 ) { // modulus
		int icap; float fcap;
		if ( GetValue( 4, &icap, &fcap ) ) {
			if ( icap ) {
				*ival %= icap;
				*fval = *ival;
			}
		}
	}
}


static void Cvar_Func_f( void ) {

	if ( Cmd_Argc() < 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "usage: \n" \
			"  \\varfunc <add|sub|mul|div|mod|sin|cos> <cvar> <value> [lo.cap] [hi.cap]\n" \
			"  \\varfunc rand <cvar> [base] [modulus]\n" );
		return;
	}

	//     0     1     2      3      4        5
	// \varfunc <op> <cvar> <val> [lo-cap] [hi-cap]

	// \varfunc rand <cvar> [base] [modulus]

	funcType_t ftype = GetFuncType(); // index 1: function type
	if ( ftype == FT_BAD ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "%s: unknown function %s\n", Cmd_Argv( 0 ), Cmd_Argv( 1 ) );
		return;
	}

	const char *cvar_name = Cmd_Argv( 2 ); // index 2: cvar name
	cvar_t *cvar = Cvar_FindVar( cvar_name );
	if ( !cvar ) {
		if ( !AllowEmptyCvar( ftype ) )	{
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Cvar '%s' does not exist.\n", cvar_name );
			return; // FIXME: allow cvar creation for some functions?
		}
	} else if ( cvar->flags & ( CVAR_INIT | CVAR_ROM | CVAR_PROTECTED ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Cvar '%s' is write-protected.\n", cvar_name );
		return;
	}

	int ival;
	float fval;
	if ( cvar ) {
		fval = cvar->value;
		ival = cvar->integer;
	} else {
		fval = 0.0;
		ival = 0;
	}

	if ( ftype == FT_RAND )
		Cvar_Rand( &ival, &fval );
	else
		Cvar_Op( ftype, &ival, &fval ); // apply modification

	char value[ 64 ];
	if ( cvar && cvar->validator == CV_INTEGER ) {
		sprintf( value, "%i", ival );
	} else {
		if ( (int)fval == fval )
			sprintf( value, "%i", (int)fval );
		else
			sprintf( value, "%f", fval );
	}

	Cvar_Set2( cvar_name, value, qfalse );
}


/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to qtrue.

When writeAll is qtrue every cvar with a non-default value is written,
regardless of CVAR_ARCHIVE — used by "writeconfig -f" to capture the
complete engine state.
============
*/
void Cvar_WriteVariables( fileHandle_t f, qboolean writeAll )
{
	if ( cvar_sort ) {
		cvar_sort = qfalse;
		Cvar_Sort();
	}

	for (cvar_t *var = cvar_vars; var; var = var->next)
	{
		if ( !var->name || Q_stricmp( var->name, "cl_cdkey" ) == 0 )
			continue;

		/* skip protected / read-only / init-only cvars when dumping
		   every variable — they cannot be restored from a config */
		if ( writeAll && ( var->flags & (CVAR_ROM | CVAR_INIT | CVAR_PROTECTED) ) )
			continue;

		if ( writeAll || (var->flags & CVAR_ARCHIVE) ) {
			// write the latched value, even if it hasn't taken effect yet
			const char *value = var->latchedString ? var->latchedString : var->string;
			char buffer[MAX_CMD_LINE];
			if ( strlen( var->name ) + strlen( value ) + 10 > sizeof( buffer ) ) {
				COM_WARN( LOG_CH(ch_system), "WARNING: %svalue of variable \"%s\" too long to write to file\n",
					value == var->latchedString ? "latched " : "", var->name );
				continue;
			}
			if ( (var->flags & CVAR_NODEFAULT) && !strcmp( value, var->resetString ) ) {
				continue;
			}
			/* skip default-valued cvars in the full dump to keep the
			   output readable */
			if ( writeAll && var->resetString && !strcmp( value, var->resetString ) ) {
				continue;
			}
			int len = Com_sprintf( buffer, sizeof( buffer ), "seta %s \"%s\"" Q_NEWLINE, var->name, value );

			FS_Write( buffer, len, f );
		}
	}
}


/*
============
Cvar_FindVarPublic

Public accessor around the internal Cvar_FindVar hash lookup — used by
the help system and other modules that must inspect an existing cvar by
name without modifying it.
============
*/
cvar_t *Cvar_FindVarPublic( const char *var_name )
{
	return Cvar_FindVar( var_name );
}


/*
============
Cvar_ForEach

Invokes callback for every registered cvar.  The callback must not
unregister the cvar being iterated.
============
*/
void Cvar_ForEach( void (*callback)( cvar_t *var, void *userdata ), void *userdata )
{
	cvar_t *var;

	if ( callback == NULL )
		return;

	if ( cvar_sort ) {
		cvar_sort = qfalse;
		Cvar_Sort();
	}

	for ( var = cvar_vars; var; var = var->next ) {
		if ( !var->name )
			continue;
		callback( var, userdata );
	}
}


static void Cvar_TypeInfoString( const cvar_t *var, char *buf, int bufsize ) {
	switch ( var->type ) {
	case CVT_STRING:
		Q_strncpyz( buf, "string", bufsize );
		break;
	case CVT_BOOL:
		Q_strncpyz( buf, "bool", bufsize );
		break;
	case CVT_INT:
		if ( var->typeMin != var->typeMax )
			Com_sprintf( buf, bufsize, "int[%d-%d]", (int)var->typeMin, (int)var->typeMax );
		else
			Q_strncpyz( buf, "int", bufsize );
		break;
	case CVT_FLOAT:
		if ( var->typeMin != var->typeMax )
			Com_sprintf( buf, bufsize, "float[%g-%g]", var->typeMin, var->typeMax );
		else
			Q_strncpyz( buf, "float", bufsize );
		break;
	case CVT_ENUM:
		Com_sprintf( buf, bufsize, "enum(%d)", var->enumCount );
		break;
	default:
		Q_strncpyz( buf, "?", bufsize );
		break;
	}
	if ( var->flags & CVAR_LATCH ) {
		int _n = (int)strlen(buf);
		Com_sprintf( buf + _n, bufsize - _n, " latch" );
	}
}


/*
============
Cvar_List_f
============
*/
static void Cvar_List_f( void ) {
	cvar_t		*var;
	int			i;
	const char	*match;
	int			typeCounts[5] = { 0 };
	char		flags[11];
	char		valBuf[12];
	char		typeInfo[24];
	char		descBuf[44];
	const char	*desc;
	int			descLen;

	// sort to get more predictable output
	if ( cvar_sort ) {
		cvar_sort = qfalse;
		Cvar_Sort();
	}

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	i = 0;
	for ( var = cvar_vars; var; var = var->next )
	{
		if ( !var->name || ( match && !Com_Filter( match, var->name ) ) )
			continue;

		// --- flags column (9 chars + null) ---
		flags[0] = ( var->flags & CVAR_SERVERINFO ) ? 'S' : ' ';
		flags[1] = ( var->flags & CVAR_SYSTEMINFO ) ? 's' : ' ';
		flags[2] = ( var->flags & CVAR_USERINFO )   ? 'U' : ' ';
		flags[3] = ( var->flags & CVAR_ROM )        ? 'R' : ' ';
		flags[4] = ( var->flags & CVAR_INIT )       ? 'I' : ' ';
		flags[5] = ( var->flags & CVAR_ARCHIVE )    ? 'A' : ' ';
		flags[6] = ( var->flags & CVAR_LATCH )      ? 'L' : ' ';
		flags[7] = ( var->flags & CVAR_CHEAT )      ? 'C' : ' ';
		flags[8] = ( var->flags & CVAR_USER_CREATED ) ? '?' : ' ';
		flags[9] = '\0';

		// --- value column (truncate to 10 chars) ---
		if ( strlen( var->string ) > 10 ) {
			Com_sprintf( valBuf, sizeof( valBuf ), "%.7s...", var->string );
		} else {
			Q_strncpyz( valBuf, var->string, sizeof( valBuf ) );
		}

		// --- type info column ---
		Cvar_TypeInfoString( var, typeInfo, sizeof( typeInfo ) );

		// --- description column (truncate to 40 chars) ---
		desc = ( var->description && var->description[0] ) ? var->description : "";
		descLen = (int)strlen( desc );
		if ( descLen > 40 ) {
			Com_sprintf( descBuf, sizeof( descBuf ), "%.37s...", desc );
		} else {
			Q_strncpyz( descBuf, desc, sizeof( descBuf ) );
		}

		Com_Log( SEV_INFO, LOG_CH(ch_system),
			"%s  %-24s %-11s %-21s %s\n",
			flags, var->name, valBuf, typeInfo, descBuf );

		if ( var->type >= CVT_STRING && var->type <= CVT_ENUM )
			typeCounts[ var->type ]++;
		i++;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_system), "\n  Total: %d cvars  (%d string, %d bool, %d int, %d float, %d enum)\n",
		i,
		typeCounts[CVT_STRING], typeCounts[CVT_BOOL],
		typeCounts[CVT_INT], typeCounts[CVT_FLOAT], typeCounts[CVT_ENUM] );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "  %d cvar indexes\n", cvar_numIndexes );
}


/*
============
Cvar_ListModified_f
============
*/
static void Cvar_ListModified_f( void ) {
	cvar_t	*var;
	int		totalModified;
	const char *value;
	const char *match;

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	totalModified = 0;
	for (var = cvar_vars ; var ; var = var->next)
	{
		if ( !var->name || !var->modificationCount )
			continue;

		value = var->latchedString ? var->latchedString : var->string;
		if ( !strcmp( value, var->resetString ) )
			continue;

		totalModified++;

		if (match && !Com_Filter(match, var->name))
			continue;

		if (var->flags & CVAR_SERVERINFO) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "S");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}
		if (var->flags & CVAR_SYSTEMINFO) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "s");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}
		if (var->flags & CVAR_USERINFO) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "U");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}
		if (var->flags & CVAR_ROM) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "R");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}
		if (var->flags & CVAR_INIT) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "I");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}
		if (var->flags & CVAR_ARCHIVE) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "A");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}
		if (var->flags & CVAR_LATCH) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "L");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}
		if (var->flags & CVAR_CHEAT) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "C");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}
		if (var->flags & CVAR_USER_CREATED) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "?");
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), " ");
		}

		Com_Log( SEV_INFO, LOG_CH(ch_system), " %s \"%s\", default \"%s\"\n", var->name, value, var->resetString);
	}

	Com_Log( SEV_INFO, LOG_CH(ch_system), "\n%i total modified cvars\n", totalModified);
}


/*
============
Cvar_Unset

Unsets a cvar
============
*/
static cvar_t *Cvar_Unset( cvar_t *cv )
{
	cvar_t *next = cv->next;

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= cv->flags;

	if ( cv->name )
		Z_Free( cv->name );
	if ( cv->string )
		Z_Free( cv->string );
	if ( cv->latchedString )
		Z_Free( cv->latchedString );
	if ( cv->resetString )
		Z_Free( cv->resetString );
	if ( cv->description )
		Z_Free( cv->description );
	if ( cv->mins )
		Z_Free( cv->mins );
	if ( cv->maxs )
		Z_Free( cv->maxs );

	if ( cv->prev )
		cv->prev->next = cv->next;
	else
		cvar_vars = cv->next;
	if ( cv->next )
		cv->next->prev = cv->prev;

	if ( cv->hashPrev )
		cv->hashPrev->hashNext = cv->hashNext;
	else
		hashTable[cv->hashIndex] = cv->hashNext;
	if ( cv->hashNext )
		cv->hashNext->hashPrev = cv->hashPrev;

	memset( cv, '\0', sizeof( *cv ) );

	return next;
}


/*
============
Cvar_Unset_f

Unsets a userdefined cvar
============
*/
static void Cvar_Unset_f( void )
{
	cvar_t *cv;

	if ( Cmd_Argc() != 2 )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Usage: %s <varname>\n", Cmd_Argv( 0 ) );
		return;
	}

	cv = Cvar_FindVar( Cmd_Argv( 1 ) );

	if ( !cv )
		return;

	if ( cv->flags & CVAR_USER_CREATED )
		Cvar_Unset( cv );
	else
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Error: %s: Variable %s is not user created.\n",
			Cmd_Argv( 0 ), cv->name );
}


/*
============
Cvar_Restart

Resets all cvars to their hardcoded values and removes userdefined variables
and variables added via the VMs if requested.
============
*/

void Cvar_Restart( qboolean unsetVM )
{
	cvar_t *curvar = cvar_vars;

	while(curvar)
	{
		if((curvar->flags & CVAR_USER_CREATED) ||
			(unsetVM && (curvar->flags & CVAR_VM_CREATED)))
		{
			// throw out any variables the user/vm created
			curvar = Cvar_Unset(curvar);
			continue;
		}

		if(!(curvar->flags & (CVAR_ROM | CVAR_INIT | CVAR_NORESTART)))
		{
			// Just reset the rest to their default values.
			Cvar_Set2(curvar->name, curvar->resetString, qfalse);
		}

		curvar = curvar->next;
	}
}


static void Cvar_Trim( qboolean verbose )
{
	cvar_t *curvar = cvar_vars;
	while ( curvar )
	{
		if ( curvar->flags & CVAR_USER_CREATED )
		{
			// throw out any variables the user created
			if ( verbose )
				Com_Log( SEV_INFO, LOG_CH(ch_system), "unset cvar" S_COLOR_YELLOW " %s\n", curvar->name );

			curvar = Cvar_Unset( curvar );
			continue;
		}

		curvar = curvar->next;
	}
}


/*
============
Cvar_Restart_f

Resets all cvars to their hardcoded values
============
*/
static void Cvar_Restart_f( void )
{
	Cvar_Restart( qfalse );
}


/*
============
Cvar_Trim_f

Removes all user-created cvars
This will only accept to run when both the server and client are running unless forced
============
*/
static void Cvar_Trim_f( void )
{
	qboolean forced = qfalse;
	qboolean verbose = qtrue;

	for ( int i = 1; i < Cmd_Argc(); i++ )
	{
		const char *s = Cmd_Argv( i );
		if ( *s == '-' )
		{
			s++;
			while ( *s != '\0' )
			{
				if ( *s == 'f' ) // force cleanup
					forced = qtrue;
				else if ( *s == 's' ) // silent mode
					verbose = qfalse;
				s++;
			}
		}
	}

#ifdef DEDICATED
	if ( ( com_sv_running && com_sv_running->integer ) || forced )
#else
	if ( ( com_cl_running && com_cl_running->integer && com_sv_running && com_sv_running->integer ) || forced )
#endif
	{
		Cvar_Trim( verbose );
		return;
	}

#ifdef DEDICATED
	COM_WARN( LOG_CH(ch_system), " You're not running a server, so not all subsystems/VMs are loaded.\n" );
#else
	COM_WARN( LOG_CH(ch_system), " You're not running a listen server, so not all subsystems/VMs are loaded.\n" );
#endif
	COM_WARN( LOG_CH(ch_system), " This means you'd remove cvars that are probably best kept around.\n" );
	COM_WARN( LOG_CH(ch_system), " If you don't care, you can force the call by running '\\%s -f'.\n", Cmd_Argv(0) );
	COM_WARN( LOG_CH(ch_system), " You've been warned.\n" );
}


/*
=====================
Cvar_InfoString
=====================
*/
const char *Cvar_InfoString( int bit, qboolean *truncated )
{
	static char	info[ MAX_INFO_STRING ];
	const cvar_t *user_vars[ MAX_CVARS ];
	const cvar_t *vm_vars[ MAX_CVARS ];

	// sort to get more predictable output
	if ( cvar_sort )
	{
		cvar_sort = qfalse;
		Cvar_Sort();
	}

	info[0] = '\0';
	int user_count = 0;
	int vm_count = 0;
	qboolean allSet = qtrue; // this will be qfalse on overflow

	for ( const cvar_t *var = cvar_vars; var; var = var->next )
	{
		if ( var->name && ( var->flags & bit ) )
		{
			// put vm/user-created cvars to the end
			if ( var->flags & ( CVAR_USER_CREATED | CVAR_VM_CREATED ) )
			{
				if ( var->flags & CVAR_USER_CREATED )
					user_vars[ user_count++ ] = var;
				else
					vm_vars[ vm_count++ ] = var;
			}
			else
			{
				allSet &= Info_SetValueForKey( info, var->name, var->string );
			}
		}
	}

	// add vm-created cvars
	for ( int i = 0; i < vm_count; i++ )
	{
		const cvar_t *var = vm_vars[ i ];
		allSet &= Info_SetValueForKey( info, var->name, var->string );
	}

	// add user-created cvars
	for ( int i = 0; i < user_count; i++ )
	{
		const cvar_t *var = user_vars[ i ];
		allSet &= Info_SetValueForKey( info, var->name, var->string );
	}

	if ( truncated )
	{
		*truncated = !allSet;
	}

	return info;
}


/*
=====================
Cvar_InfoString_Big

  handles large info strings ( CS_SYSTEMINFO )
=====================
*/
const char *Cvar_InfoString_Big( int bit, qboolean *truncated )
{
	static char	info[BIG_INFO_STRING];

	info[0] = '\0';
	qboolean allSet = qtrue;

	for ( const cvar_t *var = cvar_vars; var; var = var->next )
	{
		if ( var->name && (var->flags & bit) )
			allSet &= Info_SetValueForKey_s( info, sizeof( info ), var->name, var->string );
	}

	if ( truncated )
	{
		*truncated = !allSet;
	}

	return info;
}


/*
=====================
Cvar_InfoStringBuffer
=====================
*/
void Cvar_InfoStringBuffer( int bit, char* buff, int buffsize ) {
	Q_strncpyz( buff, Cvar_InfoString( bit, NULL ), buffsize );
}


/*
=====================
Cvar_CheckRange
=====================
*/
void Cvar_CheckRange( cvar_t *var, const char *mins, const char *maxs, cvarValidator_t type )
{
	if ( type >= CV_MAX ) {
		COM_WARN( LOG_CH(ch_system), "Invalid validation type %i for %s\n", type, var->name );
		return;
	}

	if ( var->mins ) {
		Z_Free( var->mins );
		var->mins = NULL;
	}
	if ( var->maxs ) {
		Z_Free( var->maxs );
		var->maxs = NULL;
	}

	var->validator = type;

	if ( type == CV_NONE )
		return;

	if ( mins )
		var->mins = CopyString( mins );

	if ( maxs )
		var->maxs = CopyString( maxs );

	// Force an initial range check
	Cvar_Set( var->name, var->string );
}


/*
=====================
Cvar_SetDescription
=====================
*/
void Cvar_SetDescription( cvar_t *var, const char *var_description )
{
	if( var_description && var_description[0] != '\0' )
	{
		if( var->description != NULL )
		{
			Z_Free( var->description );
		}
		var->description = CopyString( var_description );
	}
}


/*
=====================
Cvar_SetDescription
=====================
*/
void Cvar_SetDescription2( const char *var_name, const char* var_description )
{
	cvar_t *var;

	var = Cvar_FindVar( var_name );
	if ( !var || !var_description )
		return;

	if ( strlen( var_description ) >= MAX_CVAR_VALUE_STRING )
		return;

	if ( var_description[0] != '\0' )
	{
		if ( var->description != NULL )
		{
			Z_Free( var->description );
		}
		var->description = CopyString( var_description );
	}
}


/*
=====================
Cvar_SetGroup
=====================
*/
void Cvar_SetGroup( cvar_t *var, cvarGroup_t group ) {
	if ( group < CVG_MAX ) {
		var->group = group;
	} else {
		Com_Terminate( TERM_CLIENT_DROP, "Bad group index %i for %s", group, var->name );
	}
}


/*
=====================
Cvar_CheckGroup
=====================
*/
int Cvar_CheckGroup( cvarGroup_t group ) {
	if ( group < CVG_MAX ) {
		return cvar_group[ group ];
	}
	return 0;
}


/*
=====================
Cvar_ResetGroup
=====================
*/
void Cvar_ResetGroup( cvarGroup_t group, qboolean resetModifiedFlags ) {
	(void)resetModifiedFlags;
	if ( group < CVG_MAX ) {
		cvar_group[ group ] = 0;
	}
}


/*
=====================
Cvar_VM_Register

Slightly modified Cvar_Get for the interpreted modules (game/cgame syscall path).
=====================
*/
#define INVALID_FLAGS ( CVAR_USER_CREATED | CVAR_SERVER_CREATED | CVAR_PROTECTED | CVAR_PRIVATE | CVAR_MODIFIED | CVAR_NONEXISTENT )
void Cvar_VM_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags, int privateFlag )
{
	cvar_t	*cv;

	// There is code in Cvar_Get to prevent CVAR_ROM cvars being changed by the
	// user. In other words CVAR_ARCHIVE and CVAR_ROM are mutually exclusive
	// flags. Unfortunately some historical game code (including single player
	// baseq3) sets both flags. We unset CVAR_ROM for such cvars.
	if ((flags & (CVAR_ARCHIVE | CVAR_ROM)) == (CVAR_ARCHIVE | CVAR_ROM)) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_system), S_COLOR_YELLOW "WARNING: Unsetting CVAR_ROM from cvar '%s', "
			"since it is also CVAR_ARCHIVE\n", varName );
		flags &= ~CVAR_ROM;
	}

	// Don't allow VM to specify a different creator or other internal flags.
	if ( flags & INVALID_FLAGS ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_system), S_COLOR_YELLOW "WARNING: VM tried to set invalid flags 0x%02x on cvar '%s'\n", ( flags & INVALID_FLAGS ), varName );
		flags &= ~INVALID_FLAGS;
	}

	cv = Cvar_FindVar( varName );

	// Don't modify cvar if it's protected.
	if ( cv && ( cv->flags & ( CVAR_PROTECTED | CVAR_PRIVATE ) ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_system), S_COLOR_YELLOW "WARNING: VM tried to register protected cvar '%s' with value '%s'%s\n",
			varName, defaultValue, ( flags & ~cv->flags ) != 0 ? " and new flags" : "" );
		if ( cv->flags & CVAR_PRIVATE ) {
			if ( privateFlag ) {
				return;
			}
		}
	} else {
		cv = Cvar_Get( varName, defaultValue, flags | CVAR_VM_CREATED );
	}

	if (!vmCvar)
		return;

	if (!cv)
		return; // table full during error handling — handle stays uninitialized, skip update

	vmCvar->handle = cv - cvar_indexes;
	vmCvar->modificationCount = -1;

	Cvar_Update( vmCvar, 0 );
}


/*
=====================
Cvar_Register

Typed single-call registration. Creates or adopts an existing cvar and stamps the
typed fields (type, range, enum list, description, callback) from the descriptor.
If the cvar already exists with a non-CVT_STRING type (a prior Cvar_Register call),
the type and range are NOT overwritten — the first typed registration wins.
=====================
*/
cvar_t *Cvar_Register( const cvarDesc_t *desc )
{
	cvar_t *var;
	int     i;

	if ( !desc || !desc->name || !desc->defaultValue ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_system), "Cvar_Register: NULL descriptor or required field\n" );
		return NULL;
	}

	var = Cvar_Get( desc->name, desc->defaultValue, desc->flags );
	if ( !var ) {
		return NULL;
	}

	// First typed registration wins — don't downgrade a CVT_INT cvar to CVT_STRING
	// if a second Cvar_Get / Cvar_Register call arrives for the same name.
	if ( var->type == CVT_STRING && desc->type != CVT_STRING ) {
		var->type    = desc->type;
		var->typeMin = desc->min;
		var->typeMax = desc->max;
	}

	// Enum values: always update so the latest static array pointer is used.
	if ( desc->type == CVT_ENUM && desc->enumValues ) {
		var->enumValues = desc->enumValues;
		var->enumCount  = 0;
		for ( i = 0; desc->enumValues[i]; i++ ) {
			var->enumCount++;
		}
	}

	// Callback: set if not already set (first registration wins).
	if ( !var->onChange && desc->onChange ) {
		var->onChange = desc->onChange;
	}

	// Description: forward to existing API so storage is managed consistently.
	if ( desc->description && desc->description[0] ) {
		Cvar_SetDescription( var, desc->description );
	}

	// Debug: catch programming errors — an invalid default value means the
	// descriptor is wrong, not the user.
#ifdef _DEBUG
	if ( var->type != CVT_STRING ) {
		int         testInt;
		float       testFloat;
		const char *testNorm;
		if ( !Cvar_ValidateTyped( var, desc->defaultValue, &testInt, &testFloat, &testNorm ) ) {
			Com_Terminate( TERM_CLIENT_DROP,
				"Cvar_Register: default value '%s' is invalid for '%s' (type %d)\n",
				desc->defaultValue, desc->name, (int)var->type );
		}
	}
#endif

	return var;
}


/*
=====================
Cvar_RegisterTable

Batch registration from a descriptor table. handles[i] = Cvar_Register(&table[i]).
handles may be NULL if individual pointers are not needed.
=====================
*/
void Cvar_RegisterTable( const cvarDesc_t *table, int count, cvar_t **handles )
{
	int i;

	if ( !table || count <= 0 ) return;

	for ( i = 0; i < count; i++ ) {
		cvar_t *v = Cvar_Register( &table[i] );
		if ( handles ) {
			handles[i] = v;
		}
	}
}


/*
=====================
Cvar_Update

updates an interpreted modules' version of a cvar
=====================
*/
void Cvar_Update( vmCvar_t *vmCvar, int privateFlag ) {
	size_t	len;
	cvar_t	*cv = NULL;
	assert(vmCvar);

	if ( (unsigned)vmCvar->handle >= MAX_CVARS ) {
		Com_Terminate( TERM_CLIENT_DROP, "Cvar_Update: handle out of range" );
	}

	cv = cvar_indexes + vmCvar->handle;

	if ( cv->modificationCount == vmCvar->modificationCount ) {
		return;
	}
	if ( !cv->string ) {
		return;		// variable might have been cleared by a cvar_restart
	}
	if ( cv->flags & CVAR_PRIVATE ) {
		if ( privateFlag ) {
			return;
		}
	}
	vmCvar->modificationCount = cv->modificationCount;

	len = strlen( cv->string );
	if ( len + 1 > MAX_CVAR_VALUE_STRING ) {
		COM_WARN( LOG_CH(ch_system), "Cvar_Update: src %s length %d exceeds MAX_CVAR_VALUE_STRING - truncate\n",
			cv->string, (int)len );
	}

	Q_strncpyz( vmCvar->string, cv->string, sizeof( vmCvar->string ) );

	vmCvar->value = cv->value;
	vmCvar->integer = cv->integer;
}


/*
==================
Cvar_CompleteCvarName
==================
*/
static const cvar_t *s_completingVar = NULL;

static void Cvar_EnumValueEnumerator( void (*cb)( const char *s ) ) {
	int i;
	if ( !s_completingVar ) return;
	for ( i = 0; i < s_completingVar->enumCount; i++ ) {
		cb( s_completingVar->enumValues[i] );
	}
}

static void Cvar_BoolValueEnumerator( void (*cb)( const char *s ) ) {
	cb( "0" );
	cb( "1" );
}

void Cvar_CompleteCvarName( const char *args, int argNum )
{
	if ( argNum == 2 )
	{
		// Skip "<cmd> "
		const char *p = Com_SkipTokens( args, 1, " " );

		if ( p > args )
			Field_CompleteCommand( p, qfalse, qtrue );
	}
	else if ( argNum == 3 )
	{
		// Complete the value for the cvar named by the second argument.
		// Cmd_TokenizeStringIgnoreQuotes was already called in Field_CompleteCommand,
		// so Cmd_Argv(1) is the cvar name.
		const char *cvarName = Cmd_Argv( 1 );
		const cvar_t *var = Cvar_FindVar( cvarName );
		if ( !var )
			return;

		switch ( var->type ) {
		case CVT_ENUM:
			s_completingVar = var;
			Field_CompleteList( Cvar_EnumValueEnumerator );
			s_completingVar = NULL;
			break;
		case CVT_BOOL:
			Field_CompleteList( Cvar_BoolValueEnumerator );
			break;
		case CVT_INT:
			if ( var->typeMin != var->typeMax ) {
				Com_Log( SEV_INFO, LOG_CH(ch_system), "  %s  [range: %d - %d, current: %s]\n",
					var->name, (int)var->typeMin, (int)var->typeMax, var->string );
			}
			break;
		case CVT_FLOAT:
			if ( var->typeMin != var->typeMax ) {
				Com_Log( SEV_INFO, LOG_CH(ch_system), "  %s  [range: %g - %g, current: %s]\n",
					var->name, var->typeMin, var->typeMax, var->string );
			}
			break;
		default:
			break;
		}
	}
}


/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void)
{
	memset(cvar_indexes, '\0', sizeof(cvar_indexes));
	memset(hashTable, '\0', sizeof(hashTable));

	Cmd_AddCommand ("print", Cvar_Print_f);
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_SetCommandCompletionFunc( "toggle", Cvar_CompleteCvarName );
	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc( "set", Cvar_CompleteCvarName );
	Cmd_AddCommand ("sets", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc( "sets", Cvar_CompleteCvarName );
	Cmd_AddCommand ("setu", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc( "setu", Cvar_CompleteCvarName );
	Cmd_AddCommand ("seta", Cvar_Set_f);
	Cmd_SetCommandCompletionFunc( "seta", Cvar_CompleteCvarName );
	Cmd_AddCommand ("reset", Cvar_Reset_f);
	Cmd_SetCommandCompletionFunc( "reset", Cvar_CompleteCvarName );
	Cmd_AddCommand ("unset", Cvar_Unset_f);
	Cmd_SetCommandCompletionFunc("unset", Cvar_CompleteCvarName);

	Cmd_AddCommand( "varfunc", Cvar_Func_f );

	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("cvar_modified", Cvar_ListModified_f);
	Cmd_AddCommand ("cvar_restart", Cvar_Restart_f);
	/* cvar_restart takes a cvar name (optionally) */
	Cmd_SetCommandCompletionFunc( "cvar_restart", Cvar_CompleteCvarName );
	Cmd_AddCommand ("cvar_trim", Cvar_Trim_f);

	Cmd_AddCommand( "writeconfig", Com_WriteConfig_f );
	Cmd_SetCommandCompletionFunc( "writeconfig", Cmd_CompleteWriteCfgName );
}


//===========================================================================
// Config-file write helpers — moved from common.c Phase 1 Group 3
//===========================================================================

static void Com_WriteConfigToFile( const char *filename ) {
	fileHandle_t	f;

	f = FS_FOpenFileWrite( filename );
	if ( f == FS_INVALID_HANDLE ) {
		if ( !FS_ResetReadOnlyAttribute( filename ) || ( f = FS_FOpenFileWrite( filename ) ) == FS_INVALID_HANDLE ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Couldn't write %s.\n", filename );
			return;
		}
	}

	FS_Printf( f, "// generated by quake, do not modify" Q_NEWLINE );
#ifndef DEDICATED
	Key_WriteBindings( f );
#endif
	Cvar_WriteVariables( f, qfalse );
	FS_FCloseFile( f );
}


static void Com_WriteConfigToFileForced( const char *filename ) {
	fileHandle_t	f;

	f = FS_FOpenFileWrite( filename );
	if ( f == FS_INVALID_HANDLE ) {
		if ( !FS_ResetReadOnlyAttribute( filename ) || ( f = FS_FOpenFileWrite( filename ) ) == FS_INVALID_HANDLE ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Couldn't write %s.\n", filename );
			return;
		}
	}

	FS_Printf( f, "// generated by quake (full dump), do not modify" Q_NEWLINE );
#ifndef DEDICATED
	Key_WriteBindings( f );
#endif
	Cvar_WriteVariables( f, qtrue );
	FS_FCloseFile( f );
}


void Com_WriteConfiguration( void ) {
	if ( !com_fullyInitialized ) {
		return;
	}

	if ( !(cvar_modifiedFlags & CVAR_ARCHIVE ) ) {
		return;
	}
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	Com_WriteConfigToFile( WIRED_CONFIG_CFG );
}


static void Com_WriteConfig_f( void ) {
	char	filename[MAX_QPATH];
	const char *ext;
	const char *nameArg;
	qboolean writeAll = qfalse;
	int argc = Cmd_Argc();

	if ( argc != 2 && argc != 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Usage: writeconfig [-f] <filename>\n"
		            "  -f   also write non-archived (non-default) cvars\n" );
		return;
	}

	if ( argc == 3 ) {
		if ( !strcmp( Cmd_Argv( 1 ), "-f" ) ) {
			writeAll = qtrue;
			nameArg = Cmd_Argv( 2 );
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Usage: writeconfig [-f] <filename>\n" );
			return;
		}
	} else {
		nameArg = Cmd_Argv( 1 );
	}

	Q_strncpyz( filename, nameArg, sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );

	if ( !FS_AllowedExtension( filename, qfalse, &ext ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "%s: Invalid filename extension: '%s'.\n", __func__, ext );
		return;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_system), "Writing %s%s.\n", filename, writeAll ? " (full dump)" : "" );
	if ( writeAll ) {
		Com_WriteConfigToFileForced( filename );
	} else {
		Com_WriteConfigToFile( filename );
	}
}
