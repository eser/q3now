// SPDX-License-Identifier: GPL-2.0-or-later

#include "../code/qcommon/mod_manifest.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect( bool condition, const char *name ) {
	if ( !condition ) {
		fprintf( stderr, "FAIL: %s\n", name );
		failures++;
	}
}

static void parse_canonical_v1( void ) {
	static const char json[] =
		"{\"schema\":\"wired.sw3z.package/v1\",\"id\":\"wired.game-client\","
		"\"version\":\"1.2.0\",\"owner\":\"wired\",\"role\":\"client\","
		"\"depends\":[{\"id\":\"wired.shared\",\"version\":\"1.0\"}],"
		"\"provides\":[\"wired.cgame\"],\"compatibility\":{\"os\":[\"any\"],"
		"\"arch\":[\"any\"],\"abi\":\"wired-vm-v1\",\"engineMin\":\"0.80\"},"
		"\"files\":[{\"path\":\"pax-client.sw3z\","
		"\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
		"\"size\":42}]}";
	wiredPackageManifest_t manifest;
	char error[128];
	expect( WiredPackageManifest_Parse( json, strlen( json ), &manifest, error, sizeof( error ) ), "canonical manifest parses" );
	expect( !strcmp( manifest.id, "wired.game-client" ), "identity retained" );
	expect( manifest.role == WIRED_PACKAGE_ROLE_CLIENT, "client role retained" );
	expect( manifest.dependencyCount == 1 && manifest.provideCount == 1, "dependency vocabulary retained" );
	expect( manifest.osCount == 1 && manifest.archCount == 1 && !strcmp( manifest.abi, "wired-vm-v1" ), "compatibility retained" );
	expect( manifest.fileCount == 1 && manifest.files[0].size == 42, "physical artifact retained" );
}

static void reject_invalid_manifests( void ) {
	static const char badRole[] = "{\"schema\":\"wired.sw3z.package/v1\",\"id\":\"x\",\"version\":\"1\",\"owner\":\"wired\",\"role\":\"admin\",\"compatibility\":{\"os\":[\"any\"],\"arch\":[\"any\"],\"abi\":\"v1\"}}";
	static const char traversal[] = "{\"schema\":\"wired.sw3z.package/v1\",\"id\":\"x\",\"version\":\"1\",\"owner\":\"wired\",\"role\":\"shared\",\"compatibility\":{\"os\":[\"any\"],\"arch\":[\"any\"],\"abi\":\"v1\"},\"files\":[{\"path\":\"../x.sw3z\",\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"size\":1}]}";
	static const char looseBool[] = "{\"schema\":\"wired.sw3z.package/v1\",\"id\":\"x\",\"version\":\"1\",\"owner\":\"wired\",\"role\":\"client\",\"depends\":[{\"id\":\"y\",\"optional\":1}],\"compatibility\":{\"os\":[\"any\"],\"arch\":[\"any\"],\"abi\":\"v1\"}}";
	wiredPackageManifest_t manifest;
	char error[128];
	expect( !WiredPackageManifest_Parse( badRole, strlen( badRole ), &manifest, error, sizeof( error ) ), "unknown role rejected" );
	expect( !WiredPackageManifest_Parse( traversal, strlen( traversal ), &manifest, error, sizeof( error ) ), "traversal rejected" );
	expect( !WiredPackageManifest_Parse( looseBool, strlen( looseBool ), &manifest, error, sizeof( error ) ), "non-boolean optional rejected" );
}

static void validate_manifest_set( void ) {
	static const char providerJson[] = "{\"schema\":\"wired.sw3z.package/v1\",\"id\":\"wired.shared\",\"version\":\"1\",\"owner\":\"wired\",\"role\":\"shared\",\"provides\":[\"wired.assets\"],\"compatibility\":{\"os\":[\"any\"],\"arch\":[\"any\"],\"abi\":\"v1\"}}";
	static const char clientJson[] = "{\"schema\":\"wired.sw3z.package/v1\",\"id\":\"wired.client\",\"version\":\"1\",\"owner\":\"wired\",\"role\":\"client\",\"depends\":[{\"id\":\"wired.assets\",\"version\":\"1\"}],\"compatibility\":{\"os\":[\"any\"],\"arch\":[\"any\"],\"abi\":\"v1\"}}";
	static const char missingJson[] = "{\"schema\":\"wired.sw3z.package/v1\",\"id\":\"wired.broken\",\"version\":\"1\",\"owner\":\"wired\",\"role\":\"client\",\"depends\":[{\"id\":\"missing\"}],\"compatibility\":{\"os\":[\"any\"],\"arch\":[\"any\"],\"abi\":\"v1\"}}";
	wiredPackageManifest_t manifests[2];
	char error[128];
	expect( WiredPackageManifest_Parse( providerJson, strlen( providerJson ), &manifests[0], error, sizeof( error ) ), "provider parses" );
	expect( WiredPackageManifest_Parse( clientJson, strlen( clientJson ), &manifests[1], error, sizeof( error ) ), "consumer parses" );
	expect( WiredPackageManifest_ValidateSet( manifests, 2, error, sizeof( error ) ), "capability dependency resolves" );
	expect( WiredPackageManifest_Parse( missingJson, strlen( missingJson ), &manifests[1], error, sizeof( error ) ), "missing consumer parses" );
	expect( !WiredPackageManifest_ValidateSet( manifests, 2, error, sizeof( error ) ), "missing dependency rejected" );
}

int main( void ) {
	parse_canonical_v1();
	reject_invalid_manifests();
	validate_manifest_set();
	if ( failures ) return 1;
	puts( "mod manifest contract: PASS" );
	return 0;
}
