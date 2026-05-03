/*
===========================================================================
l_log.c — botlib log sink (OTel pipeline)

All output is routed through Com_Log(SEV_TRACE, LOG_CAT_BOTLIB, ...) and
filtered by the log_cat_botlib cvar (default: inherits log_severity).

Log_Open / Log_Shutdown / Log_Flush are kept as no-ops so call sites in
be_ai_chat.c and be_ai_goal.c do not need changes.

Log_FilePointer always returns NULL; callers that do
  fp = Log_FilePointer(); if (!fp) return;
silently skip their direct-fprintf debug dumps, which is correct —
those dumps were only meaningful when the botlib stdio log was open.
===========================================================================
*/

#include <stdio.h>         /* FILE* — required for Log_FilePointer signature */

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "l_log.h"

void Log_Open( const char *filename )
{
    (void)filename;
}

void Log_Shutdown( void )
{
}

void QDECL Log_Write( char *fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    Com_Logv( SEV_TRACE, LOG_CAT_BOTLIB, fmt, ap );
    va_end( ap );
}

FILE *Log_FilePointer( void )
{
    return NULL;
}

void Log_Flush( void )
{
}
