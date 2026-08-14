// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "client.h"
#include "../renderercommon/r_profile_telemetry.h"
LOG_DECLARE_CHANNEL( ch_client, "client" );

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET profile_socket_t;
#define PROFILE_INVALID_SOCKET INVALID_SOCKET
#define profile_close closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int profile_socket_t;
#define PROFILE_INVALID_SOCKET (-1)
#define profile_close close
#endif

#include <string.h>

static cvar_t *cl_profileTelemetryPort;
static cvar_t *cl_profileTelemetryToken;
static profile_socket_t profileSocket = PROFILE_INVALID_SOCKET;
static struct sockaddr_in profileDestination;
static uint8_t profileCapability[16];
static uint8_t profileSession[16];
static uint64_t profileLastSequence;
static uint64_t profileLastHeartbeatUsec;
static uint32_t profileLastGeneration;
static int profileConfiguredPort;

static void CL_ProfileTelemetry_CloseSocket( void ) {
	if ( profileSocket != PROFILE_INVALID_SOCKET ) profile_close( profileSocket );
	profileSocket = PROFILE_INVALID_SOCKET;
	profileConfiguredPort = 0;
}

static qboolean CL_ProfileTelemetry_Configure( void ) {
	int port;
	if ( !cl_profileTelemetryPort || !cl_profileTelemetryToken ) return qfalse;
	port = cl_profileTelemetryPort->integer;
	if ( port < 1024 || port > 65535
	  || !R_ProfileTelemetryParseToken( cl_profileTelemetryToken->string, profileCapability ) ) {
		CL_ProfileTelemetry_CloseSocket();
		return qfalse;
	}
	if ( profileSocket != PROFILE_INVALID_SOCKET && profileConfiguredPort == port ) return qtrue;
	CL_ProfileTelemetry_CloseSocket();
	profileSocket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( profileSocket == PROFILE_INVALID_SOCKET ) return qfalse;
#ifdef _WIN32
	{
		u_long nonblocking = 1;
		if ( ioctlsocket( profileSocket, FIONBIO, &nonblocking ) != 0 ) {
			CL_ProfileTelemetry_CloseSocket(); return qfalse;
		}
	}
#else
	{
		int flags = fcntl( profileSocket, F_GETFL, 0 );
		if ( flags < 0 || fcntl( profileSocket, F_SETFL, flags | O_NONBLOCK ) != 0 ) {
			CL_ProfileTelemetry_CloseSocket(); return qfalse;
		}
	}
#endif
	memset( &profileDestination, 0, sizeof( profileDestination ) );
	profileDestination.sin_family = AF_INET;
	profileDestination.sin_port = htons( (uint16_t)port );
	profileDestination.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
	profileConfiguredPort = port;
	Com_Log( SEV_INFO, LOG_CH(ch_client),
		"profileTelemetry: action=sender-ready destination=127.0.0.1:%d\n", port );
	return qtrue;
}

static void CL_ProfileTelemetry_Send( uint8_t kind, uint8_t status,
		const refGpuProfileSample_t *sample ) {
	rProfileTelemetryFrame_t frame;
	uint8_t packet[R_PROFILE_TELEMETRY_PACKET_MAX];
	size_t packetBytes = 0;
	uint32_t i;
	if ( !CL_ProfileTelemetry_Configure() ) return;
	memset( &frame, 0, sizeof( frame ) );
	memcpy( frame.capability, profileCapability, sizeof( frame.capability ) );
	memcpy( frame.session, profileSession, sizeof( frame.session ) );
	frame.kind = kind;
	frame.status = status;
	frame.producerTickUsec = (uint64_t)Sys_Microseconds();
	if ( sample && kind == R_PROFILE_PACKET_SAMPLE ) {
		frame.producerGeneration = sample->producerGeneration;
		frame.topologyEpoch = sample->topologyEpoch;
		frame.laneCount = sample->laneCount;
		frame.sequence = sample->sequence;
		for ( i = 0; i < sample->laneCount; ++i ) {
			memcpy( frame.labels[i], sample->labels[i], R_PROFILE_TELEMETRY_LABEL_BYTES );
			frame.durationMs[i] = sample->durationMs[i];
		}
	} else if ( sample ) {
		frame.producerGeneration = sample->producerGeneration;
		frame.sequence = sample->sequence;
	}
	if ( !R_ProfileTelemetryEncode( &frame, packet, sizeof( packet ), &packetBytes ) ) return;
	(void)sendto( profileSocket, (const char *)packet, (int)packetBytes, 0,
		(const struct sockaddr *)&profileDestination, sizeof( profileDestination ) );
}

void CL_ProfileTelemetry_Init( void ) {
	cl_profileTelemetryPort = Cvar_Get( "cl_profileTelemetryPort", "0", CVAR_TEMP );
	cl_profileTelemetryToken = Cvar_Get( "cl_profileTelemetryToken", "", CVAR_TEMP | CVAR_PROTECTED );
	Cvar_SetDescription( cl_profileTelemetryPort, "Default-off localhost UDP destination port for the standalone RAL profile tool." );
	Cvar_SetDescription( cl_profileTelemetryToken, "32-hex capability token for the standalone RAL profile tool session." );
	Com_RandomBytes( profileSession, sizeof( profileSession ) );
}

void CL_ProfileTelemetry_RendererStarted( void ) {
	Com_RandomBytes( profileSession, sizeof( profileSession ) );
	profileLastSequence = 0;
	profileLastGeneration = 0;
	profileLastHeartbeatUsec = 0;
}

void CL_ProfileTelemetry_AfterEndFrame( const refexport_t *renderer ) {
	refGpuProfileSample_t sample;
	uint64_t now = (uint64_t)Sys_Microseconds();
	if ( !CL_ProfileTelemetry_Configure() ) return;
	memset( &sample, 0, sizeof( sample ) );
	sample.structSize = sizeof( sample );
	if ( renderer && renderer->GetGpuProfileSample
	  && renderer->GetGpuProfileSample( &sample )
	  && sample.version == R_PROFILE_TELEMETRY_SAMPLE_VERSION ) {
		if ( sample.status == R_PROFILE_STATUS_ACTIVE
		  && ( sample.producerGeneration != profileLastGeneration
		    || sample.sequence > profileLastSequence ) ) {
			qboolean firstSample = profileLastSequence == 0;
			CL_ProfileTelemetry_Send( R_PROFILE_PACKET_SAMPLE, R_PROFILE_STATUS_ACTIVE, &sample );
			profileLastGeneration = sample.producerGeneration;
			profileLastSequence = sample.sequence;
			profileLastHeartbeatUsec = now;
			if ( firstSample ) {
				Com_Log( SEV_INFO, LOG_CH(ch_client),
					"profileTelemetry: action=first-sample generation=%u sequence=%llu lanes=%u\n",
					sample.producerGeneration, (unsigned long long)sample.sequence,
					sample.laneCount );
			}
			return;
		}
		if ( now - profileLastHeartbeatUsec >= 500000u ) {
			CL_ProfileTelemetry_Send( R_PROFILE_PACKET_STATUS, (uint8_t)sample.status, &sample );
			profileLastHeartbeatUsec = now;
		}
	} else if ( now - profileLastHeartbeatUsec >= 500000u ) {
		CL_ProfileTelemetry_Send( R_PROFILE_PACKET_STATUS, R_PROFILE_STATUS_UNAVAILABLE, NULL );
		profileLastHeartbeatUsec = now;
	}
}

void CL_ProfileTelemetry_RendererStopping( void ) {
	if ( profileSocket != PROFILE_INVALID_SOCKET )
		CL_ProfileTelemetry_Send( R_PROFILE_PACKET_TERMINAL, R_PROFILE_STATUS_RENDERER_STOPPED, NULL );
	profileLastSequence = 0;
	profileLastGeneration = 0;
}

void CL_ProfileTelemetry_Shutdown( void ) {
	CL_ProfileTelemetry_RendererStopping();
	CL_ProfileTelemetry_CloseSocket();
}
