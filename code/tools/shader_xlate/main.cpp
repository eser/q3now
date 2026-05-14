// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// shader_xlate — Phase 7.3b offline shader-translation tool. Reads a SPIR-V
// blob, emits MSL / GLSL 430 / GLSL ES 300 / WGSL alongside, per
// docs/phase-7-hal-design.md §16.2:
//
//   SPIRV-Cross (C++ API, vendored at src/libs/SPIRV-Cross/) → MSL, GLSL, GLSL ES
//   naga CLI    (shelled out — "Option B2": build-time tool, no Rust at runtime) → WGSL
//
// Usage:
//   shader_xlate <input.spv> <output_dir>
//     produces:
//       <output_dir>/<base>.msl
//       <output_dir>/<base>.glsl430
//       <output_dir>/<base>.glsles300
//       <output_dir>/<base>.wgsl    (only if `naga` is on PATH)
//
// Per-target status is logged in a parseable form:
//   [xlate] <base> <target>=ok
//   [xlate] <base> <target>=FAIL: <message>
//   [xlate] <base> wgsl=skip(naga unavailable)
//
// Exit codes:
//   0 — all SPIRV-Cross targets succeeded (naga-skipped doesn't count as failure)
//   1 — input couldn't be loaded
//   2 — one or more SPIRV-Cross targets failed

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "spirv_glsl.hpp"
#include "spirv_msl.hpp"

namespace {

std::vector<uint32_t> read_spirv( const char *path ) {
	std::ifstream f( path, std::ios::binary | std::ios::ate );
	if ( !f ) { std::fprintf( stderr, "[xlate] ERROR: cannot open %s\n", path ); std::exit( 1 ); }
	std::streamsize n = f.tellg();
	f.seekg( 0 );
	if ( n <= 0 || ( n % 4 ) != 0 ) {
		std::fprintf( stderr, "[xlate] ERROR: %s is %lld bytes -- not a SPIR-V word multiple\n", path, (long long)n );
		std::exit( 1 );
	}
	std::vector<uint32_t> words( (size_t)n / 4 );
	f.read( reinterpret_cast<char *>( words.data() ), n );
	if ( !words.empty() && words[0] != 0x07230203u ) {
		std::fprintf( stderr, "[xlate] ERROR: %s -- bad SPIR-V magic 0x%08X (expected 0x07230203)\n", path, words[0] );
		std::exit( 1 );
	}
	return words;
}

std::string base_name( const std::string &path ) {
	size_t slash = path.find_last_of( "/\\" );
	std::string b = ( slash == std::string::npos ) ? path : path.substr( slash + 1 );
	size_t dot = b.find_last_of( '.' );
	if ( dot != std::string::npos ) b = b.substr( 0, dot );
	return b;
}

void write_file( const std::string &path, const std::string &content ) {
	std::ofstream o( path, std::ios::binary );
	o.write( content.data(), (std::streamsize)content.size() );
}

// SPIRV-Cross Compiler instances are single-shot (compile() mutates state).
// A fresh instance per backend keeps failures isolated.
bool xlate_glsl( const std::vector<uint32_t> &words, uint32_t version, bool es,
                 const std::string &base, const std::string &out_path, const char *targetTag ) {
	try {
		spirv_cross::CompilerGLSL c( words );
		// GLSL ES compute shaders need ESSL 3.10+. Auto-bump for compute models.
		if ( es && c.get_execution_model() == spv::ExecutionModelGLCompute && version < 310 )
			version = 310;
		spirv_cross::CompilerGLSL::Options o = c.get_common_options();
		o.version          = version;
		o.es               = es;
		o.vulkan_semantics = false;        // emit non-Vulkan GLSL — push constants → uniform block (§8.3)
		o.enable_420pack_extension = !es;  // GL 4.30 supports layout(binding=); ESSL 3.0 doesn't (3.1+ does)
		c.set_common_options( o );
		std::string src = c.compile();
		write_file( out_path, src );
		std::printf( "[xlate] %s %s=ok\n", base.c_str(), targetTag );
		return true;
	} catch ( const std::exception &e ) {
		std::printf( "[xlate] %s %s=FAIL: %s\n", base.c_str(), targetTag, e.what() );
		return false;
	}
}

bool xlate_msl( const std::vector<uint32_t> &words, const std::string &base, const std::string &out_path ) {
	try {
		spirv_cross::CompilerMSL c( words );
		spirv_cross::CompilerMSL::Options o = c.get_msl_options();
		o.platform    = spirv_cross::CompilerMSL::Options::macOS;
		o.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version( 2, 0 );  // texture arrays + argument buffers tier 2 need MSL 2.0+ (macOS 10.13/iOS 11, 2017)
		c.set_msl_options( o );
		std::string src = c.compile();
		write_file( out_path, src );
		std::printf( "[xlate] %s msl=ok\n", base.c_str() );
		return true;
	} catch ( const std::exception &e ) {
		std::printf( "[xlate] %s msl=FAIL: %s\n", base.c_str(), e.what() );
		return false;
	}
}

// WGSL via the `naga` CLI (B2 build-time tool, no Rust runtime linkage).
// Returns: 1 = ok, 0 = naga unavailable (skip), -1 = naga failed.
int xlate_wgsl_via_naga( const std::string &in_path, const std::string &base, const std::string &out_path ) {
	// probe for the CLI
	if ( std::system( "naga --version >"
#ifdef _WIN32
		"NUL 2>NUL"
#else
		"/dev/null 2>/dev/null"
#endif
		) != 0 ) {
		std::printf( "[xlate] %s wgsl=skip(naga unavailable)\n", base.c_str() );
		return 0;
	}
	std::string cmd = "naga \"" + in_path + "\" \"" + out_path + "\"";
	int rc = std::system( cmd.c_str() );
	if ( rc != 0 ) {
		std::printf( "[xlate] %s wgsl=FAIL: naga returned %d\n", base.c_str(), rc );
		return -1;
	}
	std::printf( "[xlate] %s wgsl=ok\n", base.c_str() );
	return 1;
}

} // namespace

int main( int argc, char **argv ) {
	if ( argc < 3 ) {
		std::fprintf( stderr,
			"shader_xlate — offline SPIR-V → MSL / GLSL / GLSL ES / WGSL translator (Phase 7.3b)\n"
			"usage: %s <input.spv> <output_dir>\n", argv[0] );
		return 1;
	}
	const std::string in_path = argv[1];
	const std::string out_dir = argv[2];
	const std::string base    = base_name( in_path );

	std::vector<uint32_t> words = read_spirv( in_path.c_str() );

	int failed = 0;
	if ( !xlate_msl ( words, base, out_dir + "/" + base + ".msl"        ) )           failed++;
	if ( !xlate_glsl( words, 430, false, base, out_dir + "/" + base + ".glsl430",   "glsl430"   ) ) failed++;
	if ( !xlate_glsl( words, 300, true,  base, out_dir + "/" + base + ".glsles300", "glsles300" ) ) failed++;
	int wgsl = xlate_wgsl_via_naga( in_path, base, out_dir + "/" + base + ".wgsl" );
	if ( wgsl < 0 ) failed++;  // naga error (not "skip")

	return ( failed > 0 ) ? 2 : 0;
}
