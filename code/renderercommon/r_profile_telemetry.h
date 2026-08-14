// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_R_PROFILE_TELEMETRY_H
#define WIRED_R_PROFILE_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define R_PROFILE_TELEMETRY_SAMPLE_VERSION 1u
#define R_PROFILE_TELEMETRY_WIRE_VERSION 1u
#define R_PROFILE_TELEMETRY_MAGIC 0x31545057u /* WPT1, little endian */
#define R_PROFILE_TELEMETRY_MAX_LANES 16u
#define R_PROFILE_TELEMETRY_LABEL_BYTES 32u
#define R_PROFILE_TELEMETRY_TOKEN_BYTES 16u
#define R_PROFILE_TELEMETRY_PACKET_MAX 1200u

typedef enum {
	R_PROFILE_STATUS_UNAVAILABLE = 0,
	R_PROFILE_STATUS_SAMPLING_DISABLED = 1,
	R_PROFILE_STATUS_WAITING = 2,
	R_PROFILE_STATUS_ACTIVE = 3,
	R_PROFILE_STATUS_RENDERER_STOPPED = 4
} rProfileTelemetryStatus_t;

/* Pointer-free renderer ABI payload. The renderer deep-copies labels and the
 * latest completed semantic sample; the client owns transport and history. */
typedef struct {
	uint32_t structSize;
	uint32_t version;
	uint32_t status;
	uint32_t producerGeneration;
	uint32_t topologyEpoch;
	uint32_t laneCount;
	uint64_t sequence;
	uint64_t producedUsec;
	char labels[R_PROFILE_TELEMETRY_MAX_LANES][R_PROFILE_TELEMETRY_LABEL_BYTES];
	double durationMs[R_PROFILE_TELEMETRY_MAX_LANES];
} refGpuProfileSample_t;

typedef enum {
	R_PROFILE_PACKET_STATUS = 1,
	R_PROFILE_PACKET_SAMPLE = 2,
	R_PROFILE_PACKET_TERMINAL = 3
} rProfileTelemetryPacketKind_t;

typedef struct {
	uint8_t capability[R_PROFILE_TELEMETRY_TOKEN_BYTES];
	uint8_t session[R_PROFILE_TELEMETRY_TOKEN_BYTES];
	uint8_t kind;
	uint8_t status;
	uint32_t producerGeneration;
	uint32_t topologyEpoch;
	uint32_t laneCount;
	uint64_t sequence;
	uint64_t producerTickUsec;
	char labels[R_PROFILE_TELEMETRY_MAX_LANES][R_PROFILE_TELEMETRY_LABEL_BYTES];
	double durationMs[R_PROFILE_TELEMETRY_MAX_LANES];
} rProfileTelemetryFrame_t;

/* Explicit little-endian codec. No raw struct bytes cross the process
 * boundary. Failed operations leave outputs untouched. */
int R_ProfileTelemetryEncode(const rProfileTelemetryFrame_t *frame,
	uint8_t *packet, size_t packetCapacity, size_t *packetBytes);
int R_ProfileTelemetryDecode(const uint8_t *packet, size_t packetBytes,
	rProfileTelemetryFrame_t *frame);
int R_ProfileTelemetryParseToken(const char *hex,
	uint8_t token[R_PROFILE_TELEMETRY_TOKEN_BYTES]);
void R_ProfileTelemetryFormatToken(
	const uint8_t token[R_PROFILE_TELEMETRY_TOKEN_BYTES], char out[33]);

#ifdef __cplusplus
}
#endif

#endif
