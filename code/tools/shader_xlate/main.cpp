// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// shader_xlate — offline shader-translation tool. Reads a SPIR-V
// blob, emits MSL / GLSL 430 / GLSL ES 300 / WGSL alongside, per
// docs/phase-7-hal-design.md §16.2:
//
//   SPIRV-Cross (C++ API, vendored at src/libs/SPIRV-Cross/) → MSL, GLSL, GLSL ES
//   naga CLI    (direct child process — build-time tool, no Rust at runtime) → WGSL
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
#include <stdexcept>

#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "spirv_glsl.hpp"
#include "spirv_msl.hpp"
#include "spirv_reflect.hpp"

#ifndef WIRED_SPIRV_CROSS_REVISION
#define WIRED_SPIRV_CROSS_REVISION "unknown"
#endif
#ifndef WIRED_NAGA_VERSION
#define WIRED_NAGA_VERSION "unknown"
#endif

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
	if ( !o ) throw std::runtime_error( "cannot open output: " + path );
	o.write( content.data(), (std::streamsize)content.size() );
	if ( !o ) throw std::runtime_error( "cannot write output: " + path );
}

std::string read_text_file( const std::string &path ) {
	std::ifstream input( path, std::ios::binary );
	if ( !input ) throw std::runtime_error( "cannot open output: " + path );
	return std::string( std::istreambuf_iterator<char>( input ),
		std::istreambuf_iterator<char>() );
}

// Naga represents SPIR-V push constants as the non-WebGPU `immediate` address
// space. Canonical WGSL instead reserves the first unused group-0 binding and
// carries the exact same bytes through a uniform buffer, matching
// ralShaderInlineDataAbi_t. This deterministic rewrite is part of the pinned
// translator identity; it never guesses a binding at runtime.
void lower_wgsl_immediate_to_uniform( const std::string &path ) {
	std::string source = read_text_file( path );
	const std::string immediate = "var<immediate>";
	const size_t position = source.find( immediate );
	if ( position == std::string::npos ) return;
	if ( source.find( immediate, position + immediate.size() ) != std::string::npos )
		throw std::runtime_error( "multiple WGSL immediate blocks" );
	bool used[64] = {};
	const std::string prefix = "@group(0) @binding(";
	for ( size_t at = source.find( prefix ); at != std::string::npos;
			at = source.find( prefix, at + prefix.size() ) ) {
		const size_t begin = at + prefix.size();
		char *end = nullptr;
		const unsigned long value = std::strtoul( source.c_str() + begin, &end, 10 );
		if ( end != source.c_str() + begin && value < 64u ) used[value] = true;
	}
	uint32_t binding = 0;
	while ( binding < 64u && used[binding] ) ++binding;
	if ( binding == 64u ) throw std::runtime_error( "no WGSL inline-uniform binding" );
	const std::string replacement = "@group(0) @binding(" + std::to_string( binding )
		+ ")\nvar<uniform>";
	source.replace( position, immediate.size(), replacement );
	write_file( path, source );
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
		// Metal 3 is required by the legacy bindless corpus because image and
		// sampler descriptor arrays intentionally overlap one Vulkan binding.
		o.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version( 3, 0 );
		o.argument_buffers = true;
		o.argument_buffers_tier = spirv_cross::CompilerMSL::Options::ArgumentBuffersTier::Tier2;
		c.set_msl_options( o );
		// Runtime descriptor arrays exist in multiple legacy set roles. A single
		// deterministic device-address-space policy avoids per-module ABI drift;
		// the future Metal backend binds the same MTLBuffer-backed argument-buffer
		// representation for all eight portable RAL set roles.
		for ( uint32_t set = 0; set < 8u; ++set )
			c.set_argument_buffer_device_address_space( set, true );
		std::string src = c.compile();
		write_file( out_path, src );
		std::printf( "[xlate] %s msl=ok\n", base.c_str() );
		return true;
	} catch ( const std::exception &e ) {
		std::printf( "[xlate] %s msl=FAIL: %s\n", base.c_str(), e.what() );
		return false;
	}
}

// WGSL via the exact pinned `naga` CLI (B2 build-time tool, no Rust runtime
// linkage). Returns: 1 = ok, 0 = unavailable, -1 = translation/version failed.
int pinned_naga_available() {
	static int cached = -2;
	if ( cached != -2 ) return cached;
#ifdef _WIN32
	FILE *pipe = _popen( "naga --version 2>NUL", "r" );
#else
	FILE *pipe = popen( "naga --version 2>/dev/null", "r" );
#endif
	if ( !pipe ) return cached = 0;
	char version[128] = {};
	const bool read_ok = std::fgets( version, sizeof( version ), pipe ) != nullptr;
#ifdef _WIN32
	const int close_result = _pclose( pipe );
#else
	const int close_result = pclose( pipe );
#endif
	if ( !read_ok || close_result != 0 ) return cached = 0;
	version[ std::strcspn( version, "\r\n" ) ] = '\0';
	if ( std::strcmp( version, WIRED_NAGA_VERSION ) != 0 ) {
		std::fprintf( stderr, "[xlate] ERROR: naga version %s, expected %s\n",
			version, WIRED_NAGA_VERSION );
		return cached = -1;
	}
	return cached = 1;
}

int run_pinned_naga( const std::string &in_path, const std::string &out_path ) {
#ifdef _WIN32
	return static_cast<int>( _spawnlp( _P_WAIT, "naga", "naga",
		"--capabilities", "all", "--input-kind", "spv",
		in_path.c_str(), out_path.c_str(), nullptr ) );
#else
	const pid_t child = fork();
	if ( child < 0 ) return -1;
	if ( child == 0 ) {
		execlp( "naga", "naga", "--capabilities", "all", "--input-kind", "spv",
			in_path.c_str(), out_path.c_str(), static_cast<char *>( nullptr ) );
		_exit( 127 );
	}
	int status = 0;
	if ( waitpid( child, &status, 0 ) != child ) return -1;
	return WIFEXITED( status ) ? WEXITSTATUS( status ) : -1;
#endif
}

int xlate_wgsl_via_naga( const std::string &in_path,
		const std::string &base, const std::string &out_path ) {
	const int available = pinned_naga_available();
	if ( available == 0 ) {
		std::printf( "[xlate] %s wgsl=skip(naga unavailable)\n", base.c_str() );
		return 0;
	}
	if ( available < 0 ) return -1;
	// Wired's pinned Naga patch adds the scalar specialization operations and
	// combined image/sampler split used by the production corpus. Keeping
	// SPIR-V as Naga's direct input preserves binding-array, non-uniform and
	// override semantics instead of inventing an ABI through an intermediate.
	const int rc = run_pinned_naga( in_path, out_path );
	if ( rc != 0 ) {
		std::printf( "[xlate] %s wgsl=FAIL: naga returned %d\n", base.c_str(), rc );
		return -1;
	}
	try { lower_wgsl_immediate_to_uniform( out_path ); }
	catch ( const std::exception &e ) {
		std::printf( "[xlate] %s wgsl=FAIL: %s\n", base.c_str(), e.what() );
		return -1;
	}
	std::printf( "[xlate] %s wgsl=ok\n", base.c_str() );
	return 1;
}

} // namespace

int main( int argc, char **argv ) {
	int arg = 1;
	bool require_wgsl = false;
	bool reflect_only = false;
	bool emit_msl = true;
	bool emit_glsl430 = true;
	bool emit_glsles300 = true;
	bool emit_wgsl = true;
	while ( arg < argc ) {
		if ( std::strcmp( argv[arg], "--require-wgsl" ) == 0 ) require_wgsl = true;
		else if ( std::strcmp( argv[arg], "--reflect-only" ) == 0 ) reflect_only = true;
		else if ( std::strcmp( argv[arg], "--version" ) == 0 ) {
			std::printf( "wired-shader-xlate/2 spirv-cross/%s naga/%s\n",
				WIRED_SPIRV_CROSS_REVISION, WIRED_NAGA_VERSION );
			return 0;
		}
		else if ( std::strcmp( argv[arg], "--targets" ) == 0 ) {
			if ( ++arg >= argc ) { std::fprintf( stderr, "[xlate] ERROR: --targets needs a value\n" ); return 1; }
			const std::string targets = "," + std::string( argv[arg] ) + ",";
			emit_msl = targets.find( ",msl," ) != std::string::npos;
			emit_glsl430 = targets.find( ",glsl430," ) != std::string::npos;
			emit_glsles300 = targets.find( ",glsles300," ) != std::string::npos;
			emit_wgsl = targets.find( ",wgsl," ) != std::string::npos;
			if ( !emit_msl && !emit_glsl430 && !emit_glsles300 && !emit_wgsl ) {
				std::fprintf( stderr, "[xlate] ERROR: --targets selected no known target\n" ); return 1;
			}
		}
		else break;
		arg++;
	}
	if ( require_wgsl && reflect_only ) {
		std::fprintf( stderr, "[xlate] ERROR: --require-wgsl and --reflect-only are mutually exclusive\n" );
		return 1;
	}
	if ( argc - arg < 2 ) {
		std::fprintf( stderr,
			"shader_xlate — offline SPIR-V → MSL / GLSL / GLSL ES / WGSL translator (Phase 7.3b)\n"
			"usage: %s [--require-wgsl] [--targets msl,glsl430,glsles300,wgsl] [--reflect-only] <input.spv> <output-dir|reflection.json>\n", argv[0] );
		return 1;
	}
	const std::string in_path = argv[arg];
	const std::string out_dir = argv[arg + 1];
	const std::string base    = base_name( in_path );

	std::vector<uint32_t> words = read_spirv( in_path.c_str() );
	if ( reflect_only ) {
		try {
			spirv_cross::CompilerReflection compiler( words );
			compiler.set_format( "json" );
			write_file( out_dir, compiler.compile() );
			std::printf( "[xlate] %s reflection=ok\n", base.c_str() );
			return 0;
		}
		catch ( const std::exception &e ) {
			std::printf( "[xlate] %s reflection=FAIL: %s\n", base.c_str(), e.what() );
			return 2;
		}
	}

	int failed = 0;
	if ( emit_msl && !xlate_msl( words, base, out_dir + "/" + base + ".msl" ) ) failed++;
	if ( emit_glsl430 && !xlate_glsl( words, 430, false, base,
			out_dir + "/" + base + ".glsl430", "glsl430" ) ) failed++;
	if ( emit_glsles300 && !xlate_glsl( words, 300, true, base,
			out_dir + "/" + base + ".glsles300", "glsles300" ) ) failed++;
	if ( emit_wgsl ) {
		const int wgsl = xlate_wgsl_via_naga( in_path, base,
			out_dir + "/" + base + ".wgsl" );
		if ( wgsl < 0 || ( require_wgsl && wgsl == 0 ) ) failed++;
	} else if ( require_wgsl ) {
		std::fprintf( stderr, "[xlate] ERROR: --require-wgsl requires wgsl in --targets\n" );
		failed++;
	}

	return ( failed > 0 ) ? 2 : 0;
}
