// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "wired_profile_telemetry_udp.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
static constexpr socket_t invalid_socket = INVALID_SOCKET;
static void close_socket(socket_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t invalid_socket = -1;
static void close_socket(socket_t s) { close(s); }
#endif

#include <cstring>
#include <cstdio>
#include <new>

struct wiredProfileTelemetryUdp_t {
	socket_t socket = invalid_socket;
	std::uint8_t capability[16]{};
	std::uint8_t session[16]{};
	char labels[R_PROFILE_TELEMETRY_MAX_LANES][R_PROFILE_TELEMETRY_LABEL_BYTES]{};
	ralProfileAccumulator_t accumulator{};
	wiredProfileTelemetryUdpReceipt_t receipt{};
	std::uint64_t lastPacketUsec = 0;
	std::uint64_t lastSequence = 0;
	std::uint32_t generation = 0;
	std::uint32_t topology = 0;
	bool hasSession = false;
#ifdef _WIN32
	bool winsockStarted = false;
#endif
};

static bool SetNonblocking(socket_t s) {
#ifdef _WIN32
	u_long value=1; return ioctlsocket(s,FIONBIO,&value)==0;
#else
	int flags=fcntl(s,F_GETFL,0); return flags>=0 && fcntl(s,F_SETFL,flags|O_NONBLOCK)==0;
#endif
}

int WiredProfileTelemetryUdp_Open( std::uint16_t port, const char *token,
	wiredProfileTelemetryUdp_t **out, std::uint16_t *boundPort ) {
	wiredProfileTelemetryUdp_t *r; sockaddr_in address{}; socklen_t addressBytes=sizeof(address);
	const char *failure="invalid arguments";
	if(!out || !boundPort) return 0; *out=nullptr; *boundPort=0;
	r=new(std::nothrow) wiredProfileTelemetryUdp_t; if(!r) return 0;
	if(!R_ProfileTelemetryParseToken(token,r->capability)) { std::fprintf(stderr,"profile telemetry: invalid 32-hex token\n"); delete r; return 0; }
#ifdef _WIN32
	WSADATA data{}; if(WSAStartup(MAKEWORD(2,2),&data)!=0){delete r;return 0;} r->winsockStarted=true;
#endif
	failure="socket"; r->socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if(r->socket==invalid_socket || !SetNonblocking(r->socket)) goto fail;
	address.sin_family=AF_INET; address.sin_port=htons(port); address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	failure="bind";
	if(bind(r->socket,reinterpret_cast<const sockaddr*>(&address),sizeof(address))!=0) goto fail;
	failure="getsockname";
	if(getsockname(r->socket,reinterpret_cast<sockaddr*>(&address),&addressBytes)!=0) goto fail;
	*boundPort=ntohs(address.sin_port); Ral_ProfileAccumulatorInit(&r->accumulator); *out=r; return 1;
fail:
#ifdef _WIN32
	std::fprintf(stderr,"profile telemetry: %s failed error=%d\n",failure,(int)WSAGetLastError());
#else
	std::fprintf(stderr,"profile telemetry: %s failed error=%d (%s)\n",failure,errno,std::strerror(errno));
#endif
	if(r->socket!=invalid_socket) close_socket(r->socket);
#ifdef _WIN32
	if(r->winsockStarted) WSACleanup();
#endif
	delete r; return 0;
}

void WiredProfileTelemetryUdp_Close(wiredProfileTelemetryUdp_t *r){
	if(!r)return; if(r->socket!=invalid_socket)close_socket(r->socket);
#ifdef _WIN32
	if(r->winsockStarted)WSACleanup();
#endif
	delete r;
}

static void ResetProducer(wiredProfileTelemetryUdp_t *r,const rProfileTelemetryFrame_t &f){
	std::memcpy(r->session,f.session,16); r->hasSession=true; r->generation=f.producerGeneration;
	r->topology=0; r->lastSequence=0; std::memset(r->labels,0,sizeof(r->labels));
	Ral_ProfileAccumulatorInit(&r->accumulator); r->receipt.sessionChanges++;
}

void WiredProfileTelemetryUdp_Poll(wiredProfileTelemetryUdp_t *r,std::uint64_t now,
	wiredProfileTelemetryUdpReceipt_t *out){
	std::uint8_t packet[R_PROFILE_TELEMETRY_PACKET_MAX]; unsigned drained=0;
	if(!r)return;
	while(drained++<64){sockaddr_in source{};socklen_t sourceBytes=sizeof(source);
		int n=(int)recvfrom(r->socket,reinterpret_cast<char*>(packet),sizeof(packet),0,reinterpret_cast<sockaddr*>(&source),&sourceBytes);
		if(n<0)break;
		rProfileTelemetryFrame_t f{};
		if(source.sin_family!=AF_INET || ntohl(source.sin_addr.s_addr)!=INADDR_LOOPBACK
		 || !R_ProfileTelemetryDecode(packet,(size_t)n,&f)
		 || std::memcmp(f.capability,r->capability,16)!=0){r->receipt.rejectedPackets++;continue;}
		if(!r->hasSession || std::memcmp(f.session,r->session,16)!=0) ResetProducer(r,f);
		r->lastPacketUsec=now; r->receipt.status=(rProfileTelemetryStatus_t)f.status;
		if(f.kind==R_PROFILE_PACKET_TERMINAL){r->receipt.live=false;continue;}
		if(f.kind!=R_PROFILE_PACKET_SAMPLE)continue;
		if(f.producerGeneration!=r->generation){ResetProducer(r,f);r->generation=f.producerGeneration;}
		if(f.sequence<=r->lastSequence){r->receipt.rejectedPackets++;continue;}
		if(r->lastSequence && f.sequence!=r->lastSequence+1){
			Ral_ProfileAccumulatorInit(&r->accumulator);std::memset(r->labels,0,sizeof(r->labels));r->topology=0;
		}
		if(r->topology!=f.topologyEpoch){
			Ral_ProfileAccumulatorInit(&r->accumulator); std::memset(r->labels,0,sizeof(r->labels)); r->topology=f.topologyEpoch;
			for(std::uint32_t i=0;i<f.laneCount;++i)std::memcpy(r->labels[i],f.labels[i],R_PROFILE_TELEMETRY_LABEL_BYTES);
		}else{
			for(std::uint32_t i=0;i<f.laneCount;++i)if(std::strcmp(r->labels[i],f.labels[i])!=0){r->receipt.rejectedPackets++;goto next_packet;}
		}
		{const char *labelPtrs[R_PROFILE_TELEMETRY_MAX_LANES]{};for(std::uint32_t i=0;i<f.laneCount;++i)labelPtrs[i]=r->labels[i];
		 if(Ral_ProfileAccumulatorAdd(&r->accumulator,labelPtrs,f.durationMs,f.laneCount)<0){r->receipt.rejectedPackets++;continue;}}
		r->lastSequence=f.sequence;r->receipt.acceptedSequence=f.sequence;r->receipt.producerGeneration=f.producerGeneration;
		r->receipt.topologyEpoch=f.topologyEpoch;r->receipt.status=R_PROFILE_STATUS_ACTIVE;r->receipt.live=true;
	next_packet: ;
	}
	if(r->lastPacketUsec && now-r->lastPacketUsec>1500000u && r->receipt.live){r->receipt.live=false;r->receipt.staleTransitions++;}
	if(out)*out=r->receipt;
}

int WiredProfileTelemetryUdp_Snapshot(wiredProfileTelemetryUdp_t *r,ralProfileSnapshot_t *snapshot,
	double *history,std::uint32_t capacity,wiredProfileTelemetryUdpReceipt_t *receipt){
	ralProfileSnapshot_t snap; uint32_t frames;
	if(!r||!snapshot||!history||!capacity||!r->receipt.live||!Ral_ProfileAccumulatorSnapshot(&r->accumulator,&snap))return 0;
	frames=Ral_ProfileAccumulatorCopyHistory(&r->accumulator,history,capacity);if(!frames)return 0;
	*snapshot=snap;r->receipt.historyFrames=frames;if(receipt)*receipt=r->receipt;return 1;
}
