#include "server.h"
#include "../qcommon/crypto.h"

static cvar_t *sv_wiredRconPassword;

static qboolean SV_RconIsHexDigest( const char *s ) {
	int i;

	if ( !s || strlen( s ) != COM_SHA256_HEX_LEN ) {
		return qfalse;
	}

	for ( i = 0; i < COM_SHA256_HEX_LEN; i++ ) {
		char c = s[i];
		qboolean hex =
			( c >= '0' && c <= '9' ) ||
			( c >= 'a' && c <= 'f' ) ||
			( c >= 'A' && c <= 'F' );
		if ( !hex ) {
			return qfalse;
		}
	}

	return qtrue;
}

static rconSession_t *SV_FindRconSession( const netadr_t *from ) {
	int i;

	for ( i = 0; i < MAX_RCON_SESSIONS; i++ ) {
		rconSession_t *s = &svs.rconSessions[i];
		if ( s->challenge[0] == '\0' ) {
			continue;
		}
		if ( NET_CompareAdr( from, &s->addr ) ) {
			return s;
		}
	}

	return NULL;
}

static rconSession_t *SV_CreateRconSession( const netadr_t *from ) {
	rconSession_t *slot = NULL;
	int i;

	for ( i = 0; i < MAX_RCON_SESSIONS; i++ ) {
		rconSession_t *s = &svs.rconSessions[i];

		if ( s->challenge[0] == '\0' ) {
			slot = s;
			break;
		}

		if ( svs.time - s->lastActivity > RCON_SESSION_TIMEOUT ) {
			slot = s;
			break;
		}
	}

	if ( !slot ) {
		slot = &svs.rconSessions[0];
	}

	Com_Memset( slot, 0, sizeof( *slot ) );
	slot->addr = *from;
	slot->challengeTime = svs.time;
	slot->lastActivity = svs.time;

	return slot;
}

void SV_RconCleanupSessions( void ) {
	int i;

	for ( i = 0; i < MAX_RCON_SESSIONS; i++ ) {
		rconSession_t *s = &svs.rconSessions[i];
		if ( s->challenge[0] == '\0' ) {
			continue;
		}
		if ( svs.time - s->lastActivity > RCON_SESSION_TIMEOUT ) {
			Com_Memset( s, 0, sizeof( *s ) );
		}
	}
}

void SV_RconAuth( const netadr_t *from ) {
	rconSession_t *session = SV_FindRconSession( from );

	if ( !sv_wiredRconPassword ) {
		sv_wiredRconPassword = Cvar_Get( "sv_wiredRconPassword", "", CVAR_TEMP );
		Cvar_SetDescription( sv_wiredRconPassword, "Wired RCON password for challenge-response authentication." );
	}

	if ( !sv_wiredRconPassword->string[0] ) {
		NET_OutOfBandPrint( NS_SERVER, from, "rconAuthResult fail" );
		return;
	}

	if ( !session ) {
		session = SV_CreateRconSession( from );
	}

	if ( session->failCount >= RCON_MAX_FAILURES && svs.time - session->lastFailTime < RCON_LOCKOUT_TIME ) {
		NET_OutOfBandPrint( NS_SERVER, from, "rconAuthResult fail" );
		return;
	}

	if ( svs.time - session->lastFailTime >= RCON_LOCKOUT_TIME ) {
		session->failCount = 0;
	}

	Com_RandomHexString( session->challenge, RCON_CHALLENGE_LEN );
	session->challengeTime = svs.time;
	session->lastActivity = svs.time;
	session->authenticated = qfalse;

	NET_OutOfBandPrint( NS_SERVER, from, "rconChallenge %s", session->challenge );
}

void SV_RconVerify( const netadr_t *from, const char *hmacHex ) {
	rconSession_t *session = SV_FindRconSession( from );
	char expected[ COM_SHA256_HEX_LEN + 1 ];

	if ( !sv_wiredRconPassword ) {
		sv_wiredRconPassword = Cvar_Get( "sv_wiredRconPassword", "", CVAR_TEMP );
		Cvar_SetDescription( sv_wiredRconPassword, "Wired RCON password for challenge-response authentication." );
	}

	if ( !session || !SV_RconIsHexDigest( hmacHex ) ) {
		NET_OutOfBandPrint( NS_SERVER, from, "rconAuthResult fail" );
		return;
	}

	if ( !sv_wiredRconPassword->string[0] ) {
		NET_OutOfBandPrint( NS_SERVER, from, "rconAuthResult fail" );
		return;
	}

	if ( svs.time - session->challengeTime > RCON_LOCKOUT_TIME ) {
		session->authenticated = qfalse;
		session->challenge[0] = '\0';
		NET_OutOfBandPrint( NS_SERVER, from, "rconAuthResult fail" );
		return;
	}

	Com_HMAC_SHA256_Hex( sv_wiredRconPassword->string, session->challenge, expected );

	if ( Q_stricmp( hmacHex, expected ) != 0 ) {
		session->failCount++;
		session->lastFailTime = svs.time;
		session->authenticated = qfalse;
		NET_OutOfBandPrint( NS_SERVER, from, "rconAuthResult fail" );
		return;
	}

	session->authenticated = qtrue;
	session->lastActivity = svs.time;
	session->failCount = 0;
	NET_OutOfBandPrint( NS_SERVER, from, "rconAuthResult ok" );
}

qboolean SV_RconAuthorized( const netadr_t *from ) {
	rconSession_t *session = SV_FindRconSession( from );

	if ( !session ) {
		return qfalse;
	}

	if ( !session->authenticated ) {
		return qfalse;
	}

	if ( svs.time - session->lastActivity > RCON_SESSION_TIMEOUT ) {
		Com_Memset( session, 0, sizeof( *session ) );
		return qfalse;
	}

	session->lastActivity = svs.time;
	return qtrue;
}

void SV_RconExecute( const netadr_t *from, const char *luaCode ) {
	char output[1024];
	qboolean ok;

	if ( !svs.rconLua.initialized ) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\nWired RCON Lua is not initialized.\n" );
		return;
	}

	svs.rconLua.currentClient = *from;
	ok = SV_RconLua_Execute( luaCode, output, sizeof( output ) );

	if ( !ok ) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\n%s\n", output[0] ? output : "execution failed" );
		return;
	}

	if ( output[0] ) {
		NET_OutOfBandPrint( NS_SERVER, from, "print\n%s", output );
	} else {
		NET_OutOfBandPrint( NS_SERVER, from, "print\n" );
	}
}
