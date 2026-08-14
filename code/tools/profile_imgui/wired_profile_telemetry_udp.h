// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_PROFILE_TELEMETRY_UDP_H
#define WIRED_PROFILE_TELEMETRY_UDP_H

#include "ral_profile.h"
#include "r_profile_telemetry.h"

#include <cstdint>

struct wiredProfileTelemetryUdp_t;

struct wiredProfileTelemetryUdpReceipt_t {
	std::uint64_t acceptedSequence;
	std::uint64_t sessionChanges;
	std::uint64_t rejectedPackets;
	std::uint64_t staleTransitions;
	std::uint32_t producerGeneration;
	std::uint32_t topologyEpoch;
	std::uint32_t historyFrames;
	rProfileTelemetryStatus_t status;
	bool live;
};

int WiredProfileTelemetryUdp_Open( std::uint16_t port, const char *capabilityHex,
	wiredProfileTelemetryUdp_t **outReceiver, std::uint16_t *outBoundPort );
void WiredProfileTelemetryUdp_Close( wiredProfileTelemetryUdp_t *receiver );
void WiredProfileTelemetryUdp_Poll( wiredProfileTelemetryUdp_t *receiver,
	std::uint64_t nowUsec, wiredProfileTelemetryUdpReceipt_t *receipt );
int WiredProfileTelemetryUdp_Snapshot( wiredProfileTelemetryUdp_t *receiver,
	ralProfileSnapshot_t *snapshot, double *history, std::uint32_t historyCapacity,
	wiredProfileTelemetryUdpReceipt_t *receipt );

#endif
