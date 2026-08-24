// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "r_profile_telemetry.h"

#include <math.h>
#include <string.h>

#define WIRE_FIXED_BYTES 72u

static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) { put16(p,(uint16_t)v); put16(p+2,(uint16_t)(v>>16)); }
static void put64(uint8_t *p, uint64_t v) { put32(p,(uint32_t)v); put32(p+4,(uint32_t)(v>>32)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1]<<8)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p) | ((uint32_t)get16(p+2)<<16); }
static uint64_t get64(const uint8_t *p) { return (uint64_t)get32(p) | ((uint64_t)get32(p+4)<<32); }

static uint32_t crc32_bytes(const uint8_t *data, size_t size) {
	uint32_t crc = 0xffffffffu;
	size_t i; int bit;
	for (i=0; i<size; ++i) {
		crc ^= data[i];
		for (bit=0; bit<8; ++bit) crc = (crc>>1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc&1));
	}
	return ~crc;
}

static int valid_label(const char label[R_PROFILE_TELEMETRY_LABEL_BYTES]) {
	size_t n;
	for (n=0; n<R_PROFILE_TELEMETRY_LABEL_BYTES; ++n) if (!label[n]) return n != 0;
	return 0;
}

static int validate_frame(const rProfileTelemetryFrame_t *f) {
	uint32_t i, j;
	if (!f || f->kind < R_PROFILE_PACKET_STATUS || f->kind > R_PROFILE_PACKET_TERMINAL
	 || f->status > R_PROFILE_STATUS_RENDERER_STOPPED || f->laneCount > R_PROFILE_TELEMETRY_MAX_LANES)
		return 0;
	if (f->kind == R_PROFILE_PACKET_SAMPLE) {
		if (f->status != R_PROFILE_STATUS_ACTIVE || !f->producerGeneration || !f->sequence
		 || !f->topologyEpoch || !f->laneCount) return 0;
		for (i=0; i<f->laneCount; ++i) {
			if (!valid_label(f->labels[i]) || !isfinite(f->durationMs[i]) || f->durationMs[i] < 0.0) return 0;
			for (j=0; j<i; ++j) if (strcmp(f->labels[i], f->labels[j]) == 0) return 0;
		}
	} else if (f->laneCount != 0) return 0;
	return 1;
}

int R_ProfileTelemetryEncode(const rProfileTelemetryFrame_t *f,
	uint8_t *packet, size_t capacity, size_t *packetBytes) {
	uint8_t tmp[R_PROFILE_TELEMETRY_PACKET_MAX];
	size_t bytes, offset; uint32_t i; uint64_t bits;
	if (!packet || !packetBytes || !validate_frame(f)) return 0;
	bytes = WIRE_FIXED_BYTES + (size_t)f->laneCount * (R_PROFILE_TELEMETRY_LABEL_BYTES + 8u) + 4u;
	if (bytes > capacity || bytes > sizeof(tmp) || bytes > UINT16_MAX) return 0;
	memset(tmp,0,bytes);
	put32(tmp,R_PROFILE_TELEMETRY_MAGIC); put16(tmp+4,R_PROFILE_TELEMETRY_WIRE_VERSION); put16(tmp+6,(uint16_t)bytes);
	tmp[8]=f->kind; tmp[9]=f->status; tmp[10]=(uint8_t)f->laneCount;
	memcpy(tmp+12,f->capability,16); memcpy(tmp+28,f->session,16);
	put32(tmp+44,f->producerGeneration); put32(tmp+48,f->topologyEpoch);
	put64(tmp+52,f->sequence); put64(tmp+60,f->producerTickUsec);
	offset=WIRE_FIXED_BYTES;
	for (i=0;i<f->laneCount;++i) {
		memcpy(tmp+offset,f->labels[i],R_PROFILE_TELEMETRY_LABEL_BYTES); offset+=R_PROFILE_TELEMETRY_LABEL_BYTES;
		memcpy(&bits,&f->durationMs[i],sizeof(bits)); put64(tmp+offset,bits); offset+=8;
	}
	put32(tmp+bytes-4,crc32_bytes(tmp,bytes-4));
	memcpy(packet,tmp,bytes); *packetBytes=bytes; return 1;
}

int R_ProfileTelemetryDecode(const uint8_t *packet, size_t bytes,
	rProfileTelemetryFrame_t *out) {
	rProfileTelemetryFrame_t f; size_t expected, offset; uint32_t i; uint64_t bits;
	if (!packet || !out || bytes < WIRE_FIXED_BYTES+4 || bytes > R_PROFILE_TELEMETRY_PACKET_MAX
	 || get32(packet)!=R_PROFILE_TELEMETRY_MAGIC || get16(packet+4)!=R_PROFILE_TELEMETRY_WIRE_VERSION
	 || get16(packet+6)!=bytes || get32(packet+bytes-4)!=crc32_bytes(packet,bytes-4)) return 0;
	memset(&f,0,sizeof(f)); f.kind=packet[8]; f.status=packet[9]; f.laneCount=packet[10];
	if (packet[11] || f.laneCount>R_PROFILE_TELEMETRY_MAX_LANES) return 0;
	expected=WIRE_FIXED_BYTES+(size_t)f.laneCount*(R_PROFILE_TELEMETRY_LABEL_BYTES+8u)+4u;
	if (expected!=bytes) return 0;
	memcpy(f.capability,packet+12,16); memcpy(f.session,packet+28,16);
	f.producerGeneration=get32(packet+44); f.topologyEpoch=get32(packet+48);
	f.sequence=get64(packet+52); f.producerTickUsec=get64(packet+60); offset=WIRE_FIXED_BYTES;
	for(i=0;i<f.laneCount;++i){
		memcpy(f.labels[i],packet+offset,R_PROFILE_TELEMETRY_LABEL_BYTES); offset+=R_PROFILE_TELEMETRY_LABEL_BYTES;
		bits=get64(packet+offset); offset+=8; memcpy(&f.durationMs[i],&bits,sizeof(bits));
	}
	if(!validate_frame(&f)) return 0; *out=f; return 1;
}

static int hex_value(char c) { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
int R_ProfileTelemetryParseToken(const char *hex,uint8_t token[16]) {
	uint8_t tmp[16]; int i,a,b; if(!hex||!token||strlen(hex)!=32)return 0;
	for(i=0;i<16;++i){a=hex_value(hex[i*2]);b=hex_value(hex[i*2+1]);if(a<0||b<0)return 0;tmp[i]=(uint8_t)((a<<4)|b);} memcpy(token,tmp,16);return 1;
}
void R_ProfileTelemetryFormatToken(const uint8_t token[16],char out[33]) {
	static const char h[]="0123456789abcdef"; int i; if(!out)return; if(!token){out[0]=0;return;}
	for(i=0;i<16;++i){out[i*2]=h[token[i]>>4];out[i*2+1]=h[token[i]&15];}out[32]=0;
}
