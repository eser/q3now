// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "vm_interpret_policy.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr); failures++; } } while (0)

static void check_policy( vmInterpret_t input, qboolean native, qboolean aot, vmInterpret_t wasm ) {
	vmInterpretPolicy_t out = { 7, 7, (vmInterpret_t)7 };
	CHECK( VM_InterpretPolicy( input, &out ) );
	CHECK( out.tryNative == native );
	CHECK( out.allowAot == aot );
	CHECK( out.wasmInterpret == wasm );
}

int main( void ) {
	vmInterpretPolicy_t sentinel = { 7, 7, (vmInterpret_t)7 };
	vmInterpretPolicy_t before = sentinel;

	check_policy( VMI_NATIVE, qtrue, qtrue, VMI_COMPILED );
	check_policy( VMI_BYTECODE, qfalse, qfalse, VMI_BYTECODE );
	check_policy( VMI_COMPILED, qfalse, qtrue, VMI_COMPILED );
	check_policy( (vmInterpret_t)3, qfalse, qtrue, VMI_COMPILED );
	CHECK( !VM_InterpretPolicy( (vmInterpret_t)-1, &sentinel ) );
	CHECK( memcmp( &sentinel, &before, sizeof( sentinel ) ) == 0 );
	CHECK( !VM_InterpretPolicy( VMI_NATIVE, NULL ) );
	return failures ? 1 : 0;
}
