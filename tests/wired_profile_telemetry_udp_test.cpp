// SPDX-License-Identifier: GPL-3.0-or-later
#include "wired_profile_telemetry_udp.h"

#ifdef _WIN32
#include <winsock2.h>
using send_socket_t=SOCKET;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
using send_socket_t=int;
#endif

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

static void Send(send_socket_t s,std::uint16_t port,const rProfileTelemetryFrame_t &frame){
	uint8_t packet[R_PROFILE_TELEMETRY_PACKET_MAX];size_t bytes=0;sockaddr_in to{};
	assert(R_ProfileTelemetryEncode(&frame,packet,sizeof(packet),&bytes));to.sin_family=AF_INET;to.sin_port=htons(port);to.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	assert(sendto(s,reinterpret_cast<const char*>(packet),(int)bytes,0,reinterpret_cast<sockaddr*>(&to),sizeof(to))==(int)bytes);
}

template<typename Predicate>
static wiredProfileTelemetryUdpReceipt_t Pump(wiredProfileTelemetryUdp_t *receiver,std::uint64_t now,Predicate done){
	wiredProfileTelemetryUdpReceipt_t receipt{};
	for(int i=0;i<100;++i){WiredProfileTelemetryUdp_Poll(receiver,now+i,&receipt);if(done(receipt))return receipt;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
	assert(false&&"timed out waiting for loopback telemetry");return receipt;
}

int main(){
	const char *hex="00112233445566778899aabbccddeeff";uint8_t token[16];std::uint16_t port=0;
	wiredProfileTelemetryUdp_t *receiver=nullptr;assert(R_ProfileTelemetryParseToken(hex,token));
	assert(WiredProfileTelemetryUdp_Open(0,hex,&receiver,&port)&&port);
	send_socket_t sender=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);assert(sender>=0);
	rProfileTelemetryFrame_t f{};memcpy(f.capability,token,16);memset(f.session,0x11,16);
	f.kind=R_PROFILE_PACKET_SAMPLE;f.status=R_PROFILE_STATUS_ACTIVE;f.producerGeneration=1;f.topologyEpoch=1;f.laneCount=2;f.sequence=1;f.producerTickUsec=100;
	strcpy(f.labels[0],"smaa_edges");strcpy(f.labels[1],"wired.present");f.durationMs[0]=1.0;f.durationMs[1]=2.0;
	Send(sender,port,f);wiredProfileTelemetryUdpReceipt_t receipt=Pump(receiver,1000,[](const auto&r){return r.acceptedSequence==1;});
	assert(receipt.live&&receipt.acceptedSequence==1&&receipt.sessionChanges==1);
	ralProfileSnapshot_t snapshot{};double history[RAL_PROFILE_HISTORY_CAPACITY][RAL_PROFILE_MAX_LANES]{};
	assert(WiredProfileTelemetryUdp_Snapshot(receiver,&snapshot,&history[0][0],RAL_PROFILE_HISTORY_CAPACITY,&receipt));
	assert(snapshot.laneCount==2&&snapshot.latestMs[1]==2.0&&receipt.historyFrames==1);
	Send(sender,port,f);receipt=Pump(receiver,1100,[](const auto&r){return r.rejectedPackets==1;});assert(receipt.acceptedSequence==1);
	f.sequence=2;f.durationMs[0]=3.0;Send(sender,port,f);receipt=Pump(receiver,1200,[](const auto&r){return r.acceptedSequence==2;});
	assert(WiredProfileTelemetryUdp_Snapshot(receiver,&snapshot,&history[0][0],RAL_PROFILE_HISTORY_CAPACITY,&receipt)&&receipt.historyFrames==2&&snapshot.latestMs[0]==3.0);
	WiredProfileTelemetryUdp_Poll(receiver,2000000,&receipt);assert(!receipt.live&&receipt.staleTransitions==1);
	memset(f.session,0x22,16);f.producerGeneration=1;f.topologyEpoch=2;f.sequence=1;strcpy(f.labels[0],"smaa_blend");
	Send(sender,port,f);receipt=Pump(receiver,2000100,[](const auto&r){return r.sessionChanges==2&&r.acceptedSequence==1;});assert(receipt.live);
	assert(WiredProfileTelemetryUdp_Snapshot(receiver,&snapshot,&history[0][0],RAL_PROFILE_HISTORY_CAPACITY,&receipt)&&receipt.historyFrames==1);
	f.kind=R_PROFILE_PACKET_TERMINAL;f.status=R_PROFILE_STATUS_RENDERER_STOPPED;f.laneCount=0;Send(sender,port,f);
	receipt=Pump(receiver,2000200,[](const auto&r){return !r.live&&r.status==R_PROFILE_STATUS_RENDERER_STOPPED;});
#ifdef _WIN32
	closesocket(sender);
#else
	close(sender);
#endif
	WiredProfileTelemetryUdp_Close(receiver);std::puts("wired_profile_telemetry_udp_test: PASS");return 0;
}
