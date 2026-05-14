// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// shader_xlate_spike — Phase 7 pre-flight throwaway.
//
// Validates that the shader-translation toolchain (per phase7-hal-design.md
// §16.2: SPIRV-Cross for MSL/GLSL/GLES, naga for WGSL) can be vendored, built,
// linked, and invoked. NOT part of the engine. Lives in code/tools/ ; nothing
// links against it. See README.md.
//
// Usage:
//   shader_xlate_spike <input.spv> [outdir]
//     translates <input.spv> to:
//       <outdir>/<base>.msl      (Metal Shading Language, macOS)
//       <outdir>/<base>.430.glsl (desktop GL, #version 430)
//       <outdir>/<base>.300es.glsl (WebGL2, #version 300 es)
//       <outdir>/<base>.wgsl     (via the `naga` CLI if found on PATH; else a note)
//     and prints the first ~10 lines of each to stdout.
//   outdir defaults to the input file's directory.
//
// SPIRV-Cross C++ API is used (not the C API) because that is how the eventual
// HAL would link it (a Compiler subclass per backend, options struct, compile()).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#include "spirv_glsl.hpp"   // spirv_cross::CompilerGLSL
#include "spirv_msl.hpp"    // spirv_cross::CompilerMSL

namespace {

std::vector<uint32_t> read_spirv( const char *path ) {
	std::ifstream f( path, std::ios::binary | std::ios::ate );
	if ( !f ) { std::fprintf( stderr, "ERROR: cannot open %s\n", path ); std::exit( 2 ); }
	std::streamsize n = f.tellg();
	f.seekg( 0 );
	if ( n <= 0 || ( n % 4 ) != 0 ) {
		std::fprintf( stderr, "ERROR: %s is %lld bytes — not a SPIR-V word multiple\n", path, (long long)n );
		std::exit( 2 );
	}
	std::vector<uint32_t> words( (size_t)n / 4 );
	f.read( reinterpret_cast<char *>( words.data() ), n );
	if ( !words.empty() && words[0] != 0x07230203u ) {
		// SPIR-V is little-endian; on every Wired target the host is too, so a
		// magic mismatch means the file isn't SPIR-V (not an endian issue).
		std::fprintf( stderr, "ERROR: %s — bad SPIR-V magic 0x%08X (expected 0x07230203)\n", path, words[0] );
		std::exit( 2 );
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

std::string dir_name( const std::string &path ) {
	size_t slash = path.find_last_of( "/\\" );
	return ( slash == std::string::npos ) ? std::string( "." ) : path.substr( 0, slash );
}

void write_file( const std::string &path, const std::string &content ) {
	std::ofstream o( path, std::ios::binary );
	o.write( content.data(), (std::streamsize)content.size() );
}

void print_head( const char *label, const std::string &src, int nlines ) {
	std::printf( "\n----- %s (%zu bytes, first %d lines) -----\n", label, src.size(), nlines );
	std::istringstream is( src );
	std::string line; int i = 0;
	while ( i < nlines && std::getline( is, line ) ) { std::printf( "%s\n", line.c_str() ); ++i; }
	std::printf( "----- (end of head) -----\n" );
}

// Each SPIRV-Cross Compiler is single-shot (compile() mutates internal state),
// so a fresh instance is built per backend. Translation failures throw
// spirv_cross::CompilerError — caught per backend so one unsupported construct
// doesn't abort the whole spike.
bool try_glsl( const std::vector<uint32_t> &words, uint32_t version, bool es,
               const char *label, const std::string &out_path ) {
	try {
		spirv_cross::CompilerGLSL c( words );
		spirv_cross::CompilerGLSL::Options o = c.get_common_options();
		o.version = version;
		o.es = es;
		c.set_common_options( o );
		std::string src = c.compile();
		write_file( out_path, src );
		print_head( label, src, 10 );
		return true;
	} catch ( const std::exception &e ) {
		std::printf( "\n----- %s — TRANSLATION FAILED: %s -----\n", label, e.what() );
		return false;
	}
}

bool try_msl( const std::vector<uint32_t> &words, const char *label, const std::string &out_path ) {
	try {
		spirv_cross::CompilerMSL c( words );
		spirv_cross::CompilerMSL::Options o = c.get_msl_options();
		o.platform = spirv_cross::CompilerMSL::Options::macOS;   // also: iOS
		c.set_msl_options( o );
		std::string src = c.compile();
		write_file( out_path, src );
		print_head( label, src, 10 );
		return true;
	} catch ( const std::exception &e ) {
		std::printf( "\n----- %s — TRANSLATION FAILED: %s -----\n", label, e.what() );
		return false;
	}
}

// WGSL is NOT a SPIRV-Cross backend — naga owns it. Per phase7-hal-design.md
// §16.2 the spike's WGSL path shells out to the `naga` CLI (the "Option B2"
// build-time-tool model: same shape as compile.mjs → shader_data.c, no runtime
// Rust linkage). If `naga` isn't on PATH we just say so.
bool try_wgsl_via_naga( const std::string &in_path, const char *label, const std::string &out_path ) {
	// Probe for the CLI.
	if ( std::system( "naga --version >"
#ifdef _WIN32
		"NUL 2>NUL"
#else
		"/dev/null 2>/dev/null"
#endif
	) != 0 ) {
		std::printf( "\n----- %s — naga CLI not found on PATH -----\n"
		             "  WGSL output is the naga \"Option B2\" build-time path: install the Rust\n"
		             "  toolchain (rustup) then `cargo install naga-cli`, or vendor the `naga`\n"
		             "  crate at code/libs/naga/ and `cargo build -p naga-cli --release`. naga is\n"
		             "  a *build-time* tool only — no Rust is linked into the engine. See README.md\n"
		             "  + the spike report.\n"
		             "----- (no WGSL produced this run) -----\n", label );
		return false;
	}
	// naga-cli infers in/out formats from extensions: `naga in.spv out.wgsl`.
	std::string cmd = "naga \"" + in_path + "\" \"" + out_path + "\"";
	int rc = std::system( cmd.c_str() );
	if ( rc != 0 ) {
		std::printf( "\n----- %s — naga returned %d for: %s -----\n", label, rc, cmd.c_str() );
		return false;
	}
	std::ifstream f( out_path, std::ios::binary );
	std::stringstream ss; ss << f.rdbuf();
	std::string src = ss.str();
	print_head( label, src, 10 );
	return true;
}

} // namespace

int main( int argc, char **argv ) {
	if ( argc < 2 ) {
		std::fprintf( stderr,
			"shader_xlate_spike — Phase 7 toolchain pre-flight (throwaway)\n"
			"usage: %s <input.spv> [outdir]\n", argv[0] );
		return 1;
	}
	const std::string in_path  = argv[1];
	const std::string out_dir  = ( argc >= 3 ) ? std::string( argv[2] ) : dir_name( in_path );
	const std::string base     = base_name( in_path );

	std::vector<uint32_t> words = read_spirv( in_path.c_str() );
	std::printf( "shader_xlate_spike: %s — %zu SPIR-V words (%zu bytes), magic ok\n",
	             in_path.c_str(), words.size(), words.size() * 4 );

	int ok = 0, total = 4;
	ok += try_msl ( words,                "MSL (Metal, macOS)",     out_dir + "/" + base + ".msl"        );
	ok += try_glsl( words, 430, false,    "GLSL 430 (desktop GL)",  out_dir + "/" + base + ".430.glsl"   );
	ok += try_glsl( words, 300, true,     "GLSL ES 300 (WebGL2)",   out_dir + "/" + base + ".300es.glsl" );
	ok += try_wgsl_via_naga( in_path,     "WGSL (via naga CLI)",    out_dir + "/" + base + ".wgsl"       );

	std::printf( "\nshader_xlate_spike: %d/%d backends produced output for %s\n", ok, total, base.c_str() );
	// Exit 0 as long as the SPIRV-Cross backends worked; a missing `naga` CLI is
	// not a failure of the spike (it's an environment note — see the report).
	return ( ok >= 3 ) ? 0 : 1;
}
