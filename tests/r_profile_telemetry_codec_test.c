// SPDX-License-Identifier: GPL-3.0-or-later
#include "r_profile_telemetry.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	rProfileTelemetryFrame_t in, out, sentinel;
	uint8_t packet[R_PROFILE_TELEMETRY_PACKET_MAX], token[16];
	size_t bytes = 0;
	memset(&in,0,sizeof(in));
	assert(R_ProfileTelemetryParseToken("00112233445566778899aabbccddeeff",token));
	memcpy(in.capability,token,16); memset(in.session,0xa5,16);
	in.kind=R_PROFILE_PACKET_SAMPLE; in.status=R_PROFILE_STATUS_ACTIVE;
	in.producerGeneration=3; in.topologyEpoch=7; in.sequence=42; in.producerTickUsec=99;
	in.laneCount=2; strcpy(in.labels[0],"smaa_edges"); strcpy(in.labels[1],"wired.present");
	in.durationMs[0]=1.25; in.durationMs[1]=0.5;
	assert(R_ProfileTelemetryEncode(&in,packet,sizeof(packet),&bytes));
	assert(bytes < R_PROFILE_TELEMETRY_PACKET_MAX && packet[0]=='W' && packet[1]=='P' && packet[2]=='T' && packet[3]=='1');
	memset(&out,0,sizeof(out)); assert(R_ProfileTelemetryDecode(packet,bytes,&out));
	assert(out.sequence==42 && out.laneCount==2 && strcmp(out.labels[0],"smaa_edges")==0 && out.durationMs[1]==0.5);
	memset(&sentinel,0x5a,sizeof(sentinel)); out=sentinel;
	assert(!R_ProfileTelemetryDecode(packet,bytes-1,&out)); assert(memcmp(&out,&sentinel,sizeof(out))==0);
	packet[12]^=1; assert(!R_ProfileTelemetryDecode(packet,bytes,&out)); packet[12]^=1;
	in.durationMs[0]=INFINITY; assert(!R_ProfileTelemetryEncode(&in,packet,sizeof(packet),&bytes));
	in.durationMs[0]=NAN; assert(!R_ProfileTelemetryEncode(&in,packet,sizeof(packet),&bytes));
	in.durationMs[0]=1.0; strcpy(in.labels[1],in.labels[0]); assert(!R_ProfileTelemetryEncode(&in,packet,sizeof(packet),&bytes));
	in.kind=R_PROFILE_PACKET_STATUS; in.status=R_PROFILE_STATUS_WAITING; in.laneCount=0;
	assert(R_ProfileTelemetryEncode(&in,packet,sizeof(packet),&bytes)); assert(R_ProfileTelemetryDecode(packet,bytes,&out));
	puts("r_profile_telemetry_codec_test: PASS"); return 0;
}
