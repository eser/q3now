// SPDX-License-Identifier: GPL-2.0-or-later

#include "cl_demo_frame.h"

typedef enum {
	READ_EXACT_OK,
	READ_EXACT_EOF,
	READ_EXACT_SHORT,
	READ_EXACT_ERROR
} readExactStatus_t;

static readExactStatus_t CL_DemoReadExact( clDemoFrameReadFn readFn,
	void *context, unsigned char *buffer, size_t length ) {
	size_t offset = 0;

	while ( offset < length ) {
		int count = readFn( context, buffer + offset, length - offset );
		if ( count < 0 ) {
			return READ_EXACT_ERROR;
		}
		if ( count == 0 ) {
			return offset == 0 ? READ_EXACT_EOF : READ_EXACT_SHORT;
		}
		if ( (size_t)count > length - offset ) {
			return READ_EXACT_ERROR;
		}
		offset += (size_t)count;
	}
	return READ_EXACT_OK;
}

static int32_t CL_DemoLittleInt32( const unsigned char bytes[4] ) {
	uint32_t value = (uint32_t)bytes[0]
		| ( (uint32_t)bytes[1] << 8 )
		| ( (uint32_t)bytes[2] << 16 )
		| ( (uint32_t)bytes[3] << 24 );
	return (int32_t)value;
}

clDemoFrameStatus_t CL_DemoFrameRead( clDemoFrameReadFn readFn,
	void *context, void *payload, size_t payloadCapacity, clDemoFrame_t *out ) {
	unsigned char header[8];
	readExactStatus_t readStatus;
	int32_t sequence;
	int32_t payloadLength;
	clDemoFrame_t parsed;

	if ( !readFn || !out || ( payloadCapacity > 0 && !payload ) ) {
		return CL_DEMO_FRAME_IO_ERROR;
	}

	readStatus = CL_DemoReadExact( readFn, context, header, 4 );
	if ( readStatus == READ_EXACT_EOF ) return CL_DEMO_FRAME_MISSING_TERMINATOR;
	if ( readStatus == READ_EXACT_SHORT ) return CL_DEMO_FRAME_TRUNCATED_SEQUENCE;
	if ( readStatus == READ_EXACT_ERROR ) return CL_DEMO_FRAME_IO_ERROR;

	readStatus = CL_DemoReadExact( readFn, context, header + 4, 4 );
	if ( readStatus == READ_EXACT_EOF || readStatus == READ_EXACT_SHORT )
		return CL_DEMO_FRAME_TRUNCATED_LENGTH;
	if ( readStatus == READ_EXACT_ERROR ) return CL_DEMO_FRAME_IO_ERROR;

	sequence = CL_DemoLittleInt32( header );
	payloadLength = CL_DemoLittleInt32( header + 4 );
	if ( sequence == -1 || payloadLength == -1 ) {
		return ( sequence == -1 && payloadLength == -1 )
			? CL_DEMO_FRAME_END : CL_DEMO_FRAME_INVALID_TERMINATOR;
	}
	if ( payloadLength < 0 ) return CL_DEMO_FRAME_INVALID_LENGTH;
	if ( payloadLength == 0 ) return CL_DEMO_FRAME_EMPTY_PAYLOAD;
	if ( (size_t)payloadLength > payloadCapacity ) return CL_DEMO_FRAME_OVERSIZE;

	readStatus = CL_DemoReadExact( readFn, context, payload, (size_t)payloadLength );
	if ( readStatus == READ_EXACT_EOF || readStatus == READ_EXACT_SHORT )
		return CL_DEMO_FRAME_TRUNCATED_PAYLOAD;
	if ( readStatus == READ_EXACT_ERROR ) return CL_DEMO_FRAME_IO_ERROR;

	parsed.sequence = sequence;
	parsed.payloadLength = (size_t)payloadLength;
	*out = parsed;
	return CL_DEMO_FRAME_MESSAGE;
}

const char *CL_DemoFrameStatusName( clDemoFrameStatus_t status ) {
	switch ( status ) {
	case CL_DEMO_FRAME_MESSAGE: return "message";
	case CL_DEMO_FRAME_END: return "end";
	case CL_DEMO_FRAME_MISSING_TERMINATOR: return "missing-terminator";
	case CL_DEMO_FRAME_TRUNCATED_SEQUENCE: return "truncated-sequence";
	case CL_DEMO_FRAME_TRUNCATED_LENGTH: return "truncated-length";
	case CL_DEMO_FRAME_INVALID_TERMINATOR: return "invalid-terminator";
	case CL_DEMO_FRAME_INVALID_LENGTH: return "invalid-length";
	case CL_DEMO_FRAME_EMPTY_PAYLOAD: return "empty-payload";
	case CL_DEMO_FRAME_OVERSIZE: return "oversize";
	case CL_DEMO_FRAME_TRUNCATED_PAYLOAD: return "truncated-payload";
	case CL_DEMO_FRAME_IO_ERROR: return "io-error";
	default: return "unknown";
	}
}
