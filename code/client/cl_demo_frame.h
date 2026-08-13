// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Current-protocol demo frame envelope reader.
 *
 * This leaf deliberately knows nothing about the filesystem, msg_t, cgame,
 * or UI recovery.  The product and host contract compile the same parser.
 */
#ifndef CL_DEMO_FRAME_H
#define CL_DEMO_FRAME_H

#include <stddef.h>
#include <stdint.h>

typedef int (*clDemoFrameReadFn)( void *context, void *buffer, size_t length );

typedef enum {
	CL_DEMO_FRAME_MESSAGE = 0,
	CL_DEMO_FRAME_END,
	CL_DEMO_FRAME_MISSING_TERMINATOR,
	CL_DEMO_FRAME_TRUNCATED_SEQUENCE,
	CL_DEMO_FRAME_TRUNCATED_LENGTH,
	CL_DEMO_FRAME_INVALID_TERMINATOR,
	CL_DEMO_FRAME_INVALID_LENGTH,
	CL_DEMO_FRAME_EMPTY_PAYLOAD,
	CL_DEMO_FRAME_OVERSIZE,
	CL_DEMO_FRAME_TRUNCATED_PAYLOAD,
	CL_DEMO_FRAME_IO_ERROR
} clDemoFrameStatus_t;

typedef struct {
	int32_t sequence;
	size_t payloadLength;
} clDemoFrame_t;

/*
 * Reads one <little-endian int32 sequence, little-endian int32 length,
 * non-empty payload> frame.  The only valid terminator is the exact <-1,-1>
 * pair.  A current-protocol server message cannot have an empty payload.
 * On every rejection, *out remains byte-for-byte unchanged.
 */
clDemoFrameStatus_t CL_DemoFrameRead( clDemoFrameReadFn readFn,
	void *context, void *payload, size_t payloadCapacity, clDemoFrame_t *out );

const char *CL_DemoFrameStatusName( clDemoFrameStatus_t status );

#endif
