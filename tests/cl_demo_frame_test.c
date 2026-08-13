// SPDX-License-Identifier: GPL-2.0-or-later

#include "cl_demo_frame.h"
#include "q_shared.h"
#include "qcommon.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	const unsigned char *bytes;
	size_t length;
	size_t offset;
	size_t chunk;
	int failCall;
	int calls;
} memoryReader_t;

static int memory_read( void *context, void *buffer, size_t length ) {
	memoryReader_t *reader = (memoryReader_t *)context;
	size_t available;
	size_t count;

	reader->calls++;
	if ( reader->failCall > 0 && reader->calls == reader->failCall ) return -1;
	if ( reader->offset >= reader->length ) return 0;
	available = reader->length - reader->offset;
	count = length < available ? length : available;
	if ( reader->chunk > 0 && count > reader->chunk ) count = reader->chunk;
	memcpy( buffer, reader->bytes + reader->offset, count );
	reader->offset += count;
	return (int)count;
}

static int failures;

#define CHECK(expr) do { \
	if ( !(expr) ) { \
		fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
		failures++; \
	} \
} while ( 0 )

static void reject_case( const unsigned char *bytes, size_t length,
	clDemoFrameStatus_t wanted ) {
	unsigned char payload[16] = { 0 };
	clDemoFrame_t before = { 0x13579bdf, 0x2468u };
	clDemoFrame_t out = before;
	memoryReader_t reader = { bytes, length, 0, 0, 0, 0 };
	clDemoFrameStatus_t got = CL_DemoFrameRead( memory_read, &reader,
		payload, sizeof( payload ), &out );
	CHECK( got == wanted );
	CHECK( memcmp( &out, &before, sizeof( out ) ) == 0 );
}

int main( void ) {
	static const unsigned char message[] = {
		0x78,0x56,0x34,0x12, 0x03,0x00,0x00,0x00, 0xaa,0xbb,0xcc
	};
	static const unsigned char end[] = {
		0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff
	};
	static const unsigned char shortSequence[] = { 0x01,0x00 };
	static const unsigned char shortLength[] = { 0x01,0,0,0, 0x04,0 };
	static const unsigned char invalidNegative[] = {
		0x01,0,0,0, 0xfe,0xff,0xff,0xff
	};
	static const unsigned char emptyPayloadThenEnd[] = {
		0x01,0,0,0, 0x00,0,0,0,
		0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff
	};
	static const unsigned char semanticPayloadThenEnd[] = {
		0x01,0,0,0, 0x02,0,0,0, 0xaa,0x24,
		0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff
	};
	static const unsigned char snapshotAreamaskPayloadThenEnd[] = {
		0x01,0,0,0, 0x05,0,0,0, 0xaa,0xbf,0xaa,0x92,0x00,
		0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff
	};
	static const int snapshotAreamaskWantedSymbols[] = {
		0,0,0,0, 7, 0,0,0,0, 0,0, 255
	};
	static const unsigned char falseEnd[] = {
		0x01,0,0,0, 0xff,0xff,0xff,0xff
	};
	static const unsigned char falseEnd2[] = {
		0xff,0xff,0xff,0xff, 0x00,0,0,0
	};
	static const unsigned char oversize[] = {
		0xdf,0x9b,0x57,0x13, 0x11,0x00,0x00,0x00, 0xee
	};
	static const unsigned char shortPayload[] = {
		0x01,0,0,0, 0x04,0,0,0, 0xaa,0xbb
	};
	unsigned char payload[16] = { 0 };
	unsigned int semanticSymbols[5] = { 0 };
	unsigned int snapshotAreamaskSymbols[12] = { 0 };
	unsigned char snapshotAreamaskEncoded[16] = { 0 };
	int semanticBit = 0;
	int snapshotAreamaskBit = 0;
	int snapshotAreamaskEncodedBit = 0;
	clDemoFrame_t out = { 0 };
	memoryReader_t reader = { message, sizeof( message ), 0, 1, 0, 0 };

	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_MESSAGE );
	CHECK( out.sequence == 0x12345678 );
	CHECK( out.payloadLength == 3 );
	CHECK( payload[0] == 0xaa && payload[1] == 0xbb && payload[2] == 0xcc );

	reader = (memoryReader_t){ end, sizeof( end ), 0, 2, 0, 0 };
	out = (clDemoFrame_t){ 7, 9 };
	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_END );
	CHECK( out.sequence == 7 && out.payloadLength == 9 );

	reject_case( NULL, 0, CL_DEMO_FRAME_MISSING_TERMINATOR );
	reject_case( shortSequence, sizeof( shortSequence ), CL_DEMO_FRAME_TRUNCATED_SEQUENCE );
	reject_case( shortLength, sizeof( shortLength ), CL_DEMO_FRAME_TRUNCATED_LENGTH );
	reject_case( invalidNegative, sizeof( invalidNegative ), CL_DEMO_FRAME_INVALID_LENGTH );
	reader = (memoryReader_t){ emptyPayloadThenEnd, sizeof( emptyPayloadThenEnd ), 0, 0, 0, 0 };
	memset( payload, 0xa5, sizeof( payload ) );
	out = (clDemoFrame_t){ 17, 19 };
	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_EMPTY_PAYLOAD );
	CHECK( reader.offset == 8 );
	CHECK( out.sequence == 17 && out.payloadLength == 19 );
	for ( size_t i = 0; i < sizeof( payload ); i++ ) CHECK( payload[i] == 0xa5 );
	reader = (memoryReader_t){ semanticPayloadThenEnd,
		sizeof( semanticPayloadThenEnd ), 0, 0, 0, 0 };
	out = (clDemoFrame_t){ 0 };
	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_MESSAGE );
	CHECK( out.sequence == 1 && out.payloadLength == 2 );
	CHECK( payload[0] == 0xaa && payload[1] == 0x24 );
	CHECK( reader.offset == 10 );
	for ( size_t i = 0; i < sizeof( semanticSymbols ) / sizeof( semanticSymbols[0] ); i++ ) {
		semanticBit += HuffmanGetSymbol( &semanticSymbols[i], payload, semanticBit );
	}
	CHECK( semanticSymbols[0] == 0 && semanticSymbols[1] == 0 );
	CHECK( semanticSymbols[2] == 0 && semanticSymbols[3] == 0 );
	CHECK( semanticSymbols[4] == 255 );
	CHECK( semanticBit == 14 );
	CHECK( ( ( semanticBit >> 3 ) + 1 ) <= (int)out.payloadLength );
	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_END );
	CHECK( reader.offset == sizeof( semanticPayloadThenEnd ) );

	for ( size_t i = 0; i < sizeof( snapshotAreamaskWantedSymbols ) /
			sizeof( snapshotAreamaskWantedSymbols[0] ); i++ ) {
		snapshotAreamaskEncodedBit += HuffmanPutSymbol( snapshotAreamaskEncoded,
			(uint32_t)snapshotAreamaskEncodedBit, snapshotAreamaskWantedSymbols[i] );
	}
	CHECK( snapshotAreamaskEncodedBit == 32 );
	CHECK( memcmp( snapshotAreamaskEncoded,
		snapshotAreamaskPayloadThenEnd + 8, 5 ) == 0 );
	reader = (memoryReader_t){ snapshotAreamaskPayloadThenEnd,
		sizeof( snapshotAreamaskPayloadThenEnd ), 0, 0, 0, 0 };
	out = (clDemoFrame_t){ 0 };
	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_MESSAGE );
	CHECK( out.sequence == 1 && out.payloadLength == 5 );
	CHECK( memcmp( payload, snapshotAreamaskEncoded, 5 ) == 0 );
	CHECK( reader.offset == 13 );
	for ( size_t i = 0; i < sizeof( snapshotAreamaskSymbols ) /
			sizeof( snapshotAreamaskSymbols[0] ); i++ ) {
		snapshotAreamaskBit += HuffmanGetSymbol( &snapshotAreamaskSymbols[i],
			payload, snapshotAreamaskBit );
		CHECK( snapshotAreamaskSymbols[i] ==
			(unsigned int)snapshotAreamaskWantedSymbols[i] );
	}
	CHECK( snapshotAreamaskBit == 32 );
	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_END );
	CHECK( reader.offset == sizeof( snapshotAreamaskPayloadThenEnd ) );
	reject_case( falseEnd, sizeof( falseEnd ), CL_DEMO_FRAME_INVALID_TERMINATOR );
	reject_case( falseEnd2, sizeof( falseEnd2 ), CL_DEMO_FRAME_INVALID_TERMINATOR );
	reject_case( oversize, sizeof( oversize ), CL_DEMO_FRAME_OVERSIZE );
	reject_case( shortPayload, sizeof( shortPayload ), CL_DEMO_FRAME_TRUNCATED_PAYLOAD );
	reader = (memoryReader_t){ oversize, sizeof( oversize ), 0, 0, 0, 0 };
	out = (clDemoFrame_t){ 17, 19 };
	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_OVERSIZE );
	CHECK( reader.offset == 8 );
	CHECK( out.sequence == 17 && out.payloadLength == 19 );

	reader = (memoryReader_t){ message, sizeof( message ), 0, 0, 2, 0 };
	out = (clDemoFrame_t){ 11, 13 };
	CHECK( CL_DemoFrameRead( memory_read, &reader, payload, sizeof( payload ), &out )
		== CL_DEMO_FRAME_IO_ERROR );
	CHECK( out.sequence == 11 && out.payloadLength == 13 );

	CHECK( strcmp( CL_DemoFrameStatusName( CL_DEMO_FRAME_OVERSIZE ), "oversize" ) == 0 );
	CHECK( strcmp( CL_DemoFrameStatusName( CL_DEMO_FRAME_EMPTY_PAYLOAD ), "empty-payload" ) == 0 );
	CHECK( strcmp( CL_DemoFrameStatusName( (clDemoFrameStatus_t)999 ), "unknown" ) == 0 );

	if ( failures ) return 1;
	puts( "demo frame contract: PASS" );
	return 0;
}
