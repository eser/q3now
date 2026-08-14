#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// compile.mjs — single source of truth for the renderervk shader build.
//
// Reads shaders.manifest.mjs, invokes glslangValidator on each entry,
// converts the resulting SPIR-V to a C `unsigned char NAME[]` byte
// array, and writes a single regenerated `spirv/shader_data.c`.
//
// Replaces the legacy compile.sh + compile.bat + bin2hex.c trio. The
// thin compile.sh / compile.bat wrappers exec into this script. On
// Windows the .bat wrapper handles dispatch (no chmod required); on
// POSIX the shebang above + chmod +x are sufficient.
//
// Usage:
//     node compile.mjs              # full rebuild of shader_data.c
//     node compile.mjs --verbose    # also echo each glslang invocation
//     node compile.mjs --check      # compile every entry, but DO NOT
//                                   #   write shader_data.c. Exits
//                                   #   non-zero on any failure.
//                                   #   Useful for CI / pre-commit.
//
// Requirements: Node 18+, ES modules, no npm dependencies. The Vulkan
// SDK's glslangValidator must be on PATH or VULKAN_SDK must be set.
//
// Output format: byte-for-byte identical to the legacy bin2hex.c —
// 16 bytes per line, `0xXX` uppercase hex, comma+space mid-line,
// `,\n\t` between lines, no trailing comma on the last byte. The
// committed shader_data.c was produced by bin2hex; preserving that
// format keeps regenerations diff-clean.

import { spawn, execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, writeFileSync, unlinkSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));

const HERE        = __dirname;
const SPIRV_DIR   = join(HERE, 'spirv');
const TMP_SPV     = join(SPIRV_DIR, 'data.spv');
const TMP_GLSL    = join(SPIRV_DIR, 'data.expanded.glsl');
const OUTPUT_PATH = join(SPIRV_DIR, 'shader_data.c');
const TEMPORAL_CATALOG_PATH = join(SPIRV_DIR, 'temporal_generic_catalog.inc');
const MANIFEST    = join(HERE, 'shaders.manifest.mjs');

// ── argument parsing ─────────────────────────────────────────────────

const args    = process.argv.slice(2);
const VERBOSE = args.includes('--verbose');
const CHECK   = args.includes('--check');

// ── glslangValidator discovery ───────────────────────────────────────

function findGlslang() {
	const exeName = process.platform === 'win32'
		? 'glslangValidator.exe'
		: 'glslangValidator';

	const sdk = process.env.VULKAN_SDK;
	if (sdk) {
		// Windows LunarG installer: <sdk>\Bin\glslangValidator.exe
		// POSIX (sometimes): <sdk>/bin/glslangValidator
		const candidates = [
			join(sdk, 'Bin', exeName),
			join(sdk, 'bin', exeName),
		];
		for (const c of candidates) {
			if (existsSync(c)) return c;
		}
	}

	// Fall back to PATH lookup. Node's child_process resolves bare
	// names against PATH on all platforms.
	try {
		execFileSync(exeName, ['--version'], { stdio: 'ignore' });
		return exeName;
	} catch {
		// not on PATH
	}

	console.error('ERROR: glslangValidator not found.');
	console.error('       Install the Vulkan SDK from https://vulkan.lunarg.com/sdk/home');
	console.error('       and set VULKAN_SDK or add glslangValidator to PATH.');
	process.exit(1);
}

const GLSLANG = findGlslang();

// ── byte → C hex array conversion ────────────────────────────────────
//
// Mirrors bin2hex.c exactly:
//
//     const unsigned char NAME[N] = {
//     \t0xXX, 0xXX, ..., 0xXX,         (16 per line)
//     \t0xXX, 0xXX, ...                (no trailing comma after last)
//     };\n
//
// Where:
//   - header:   `const unsigned char NAME[N] = {\n\t`
//   - mid-line: `, ` (comma + space)
//   - line wrap (every 16 bytes): `,\n\t` (instead of mid-line)
//   - final byte: nothing after it before footer
//   - footer:   `\n};\n`

function bytesToHexC(bytes, name) {
	const LINE = 16;
	const out  = [];
	out.push(`const unsigned char ${name}[${bytes.length}] = {\n\t`);
	for (let i = 0; i < bytes.length; i++) {
		const hex = bytes[i].toString(16).toUpperCase().padStart(2, '0');
		out.push('0x', hex);
		if (i < bytes.length - 1) {
			const written = i + 1;
			out.push(written % LINE === 0 ? ',\n\t' : ', ');
		}
	}
	out.push('\n};\n');
	return out.join('');
}

// ── #include expansion ───────────────────────────────────────────────
//
// Minimal directive support: a line of the form
//
//     #include "relative/path.glsl"
//
// (double-quoted, relative) is replaced inline by the contents of the
// named file, resolved relative to the *including* file's directory.
// Single-level only: an #include that appears inside an included file
// is left verbatim — glslang then reports it as an unknown directive,
// which is the intended "surface it" behaviour. No include guards, no
// <system> includes, no recursion / cycle detection (deliberately
// minimal — revisit if nested includes are ever needed in practice).
//
// Returns the expanded source string, or null when the file has no
// #include directive — the caller then hands the original path to
// glslang untouched, keeping non-#include shaders byte-identical.

const INCLUDE_RE = /^[ \t]*#include[ \t]+"([^"]+)"[ \t]*$/;

function expandIncludes(source) {
	if (!existsSync(source)) return null;          // let glslang report the missing source
	const text = readFileSync(source, 'utf8');
	if (!text.includes('#include')) return null;   // fast path — nothing to expand

	const srcDir  = dirname(source);
	let   changed = false;

	const out = text.split('\n').map((line) => {
		const m = INCLUDE_RE.exec(line);
		if (!m) return line;
		const incPath = join(srcDir, m[1]);
		if (!existsSync(incPath)) {
			console.error(`ERROR: #include target not found: "${m[1]}"`);
			console.error(`       included from: ${source}`);
			console.error(`       resolved to:   ${incPath}`);
			process.exit(1);
		}
		changed = true;
		return readFileSync(incPath, 'utf8').replace(/\n$/, '');
	});

	return changed ? out.join('\n') : null;
}

// ── glslang invocation ───────────────────────────────────────────────

function spawnSync(file, args, { capture = true } = {}) {
	return new Promise((resolve) => {
		const child = spawn(file, args, {
			stdio: capture ? ['ignore', 'pipe', 'pipe'] : 'inherit',
		});
		let stdout = '';
		let stderr = '';
		if (capture) {
			child.stdout.on('data', (d) => { stdout += d.toString(); });
			child.stderr.on('data', (d) => { stderr += d.toString(); });
		}
		child.on('error', (err) => {
			resolve({ code: -1, stdout, stderr: stderr + String(err) });
		});
		child.on('close', (code) => {
			resolve({ code, stdout, stderr });
		});
	});
}

async function compileOne(entry) {
	const { stage, source, defines = [], output } = entry;

	// Expand any `#include` directives into a temp file. Sources with no
	// includes are handed to glslang verbatim — byte-identical to the
	// pre-#include behaviour, so untouched shaders regenerate diff-clean.
	const expanded = expandIncludes(source);
	const input    = expanded === null ? source : TMP_GLSL;
	if (expanded !== null) {
		writeFileSync(TMP_GLSL, expanded);
	}

	const args = [
		'-S', stage,
		'-V',
		'-o', TMP_SPV,
		input,
		...defines.map((d) => `-D${d}`),
	];

	if (VERBOSE) {
		console.log(`==> ${output}`);
		console.log(`    ${GLSLANG} ${args.join(' ')}`);
	}

	const { code, stdout, stderr } = await spawnSync(GLSLANG, args);
	if (code !== 0) {
		console.error(`ERROR: shader compilation failed: ${output}`);
		console.error(`       source:  ${source}`);
		if (defines.length) console.error(`       defines: ${defines.join(' ')}`);
		console.error(`       command: ${GLSLANG} ${args.join(' ')}`);
		if (stdout) console.error('--- stdout ---\n' + stdout);
		if (stderr) console.error('--- stderr ---\n' + stderr);
		process.exit(1);
	}

	const spv = readFileSync(TMP_SPV);
	return { text: bytesToHexC(spv, output), size: spv.length };
}

// ── main ─────────────────────────────────────────────────────────────

async function main() {
	mkdirSync(SPIRV_DIR, { recursive: true });

	const manifest = await import(`file://${MANIFEST}`);
	const shaders = manifest.default;
	const temporalPairs = manifest.temporalGenericPairs;
	if (!Array.isArray(shaders) || shaders.length === 0) {
		console.error(`ERROR: ${MANIFEST} did not export a non-empty array.`);
		process.exit(1);
	}

	// Detect duplicate output names (would corrupt shader_data.c).
	const seen = new Set();
	for (const e of shaders) {
		if (seen.has(e.output)) {
			console.error(`ERROR: duplicate output symbol in manifest: ${e.output}`);
			process.exit(1);
		}
		seen.add(e.output);
	}

	process.chdir(HERE);

	console.log(`==> Compiling ${shaders.length} shader entries via ${GLSLANG}...`);

	let combined = '';
	const sizes = new Map();
	for (const entry of shaders) {
		const result = await compileOne(entry);
		combined += result.text;
		sizes.set(entry.output, result.size);
	}
	if (!Array.isArray(temporalPairs) || temporalPairs.length !== 40)
		throw new Error('temporalGenericPairs must contain exactly 40 pairs');
	const temporalSymbols = new Set();
	const catalogSymbols = new Set();
	for (const p of temporalPairs) {
		temporalSymbols.add(p.temporalVertex);
		temporalSymbols.add(p.temporalWriteFragment);
		temporalSymbols.add(p.temporalInvalidateFragment);
		catalogSymbols.add(p.ordinaryVertex);
		catalogSymbols.add(p.ordinaryFragment);
		catalogSymbols.add(p.temporalVertex);
		catalogSymbols.add(p.temporalWriteFragment);
		catalogSymbols.add(p.temporalInvalidateFragment);
	}
	if (temporalSymbols.size !== 76) throw new Error('temporal blob cardinality drift');
	let temporalCatalog = '/* Generated by compile.mjs from shaders.manifest.mjs. Do not edit. */\n';
	for (const symbol of [...catalogSymbols].sort())
		temporalCatalog += `VK_TEMPORAL_BLOB(${symbol}, ${sizes.get(symbol)}u)\n`;
	temporalCatalog += '\n';
	for (const p of temporalPairs) {
		const family = p.family.toUpperCase();
		temporalCatalog += `VK_TEMPORAL_PAIR(${p.tx}, ${family}, ${p.env ? 1 : 0}, ${p.shaderFog ? 1 : 0}, ${p.ordinaryVertex}, ${p.ordinaryFragment}, ${p.temporalVertex}, ${p.temporalWriteFragment}, ${p.temporalInvalidateFragment})\n`;
	}

	// Cleanup temp files whether we wrote shader_data.c or not.
	try { unlinkSync(TMP_SPV);  } catch { /* best-effort */ }
	try { unlinkSync(TMP_GLSL); } catch { /* best-effort */ }

	if (CHECK) {
		if (!existsSync(OUTPUT_PATH) || readFileSync(OUTPUT_PATH, 'utf8') !== combined
				|| !existsSync(TEMPORAL_CATALOG_PATH)
				|| readFileSync(TEMPORAL_CATALOG_PATH, 'utf8') !== temporalCatalog) {
			console.error('ERROR: generated shader outputs are stale; run node compile.mjs and commit them.');
			process.exit(1);
		}
		console.log(`==> --check OK: ${shaders.length} shaders compiled and shader_data.c is fresh.`);
		return;
	}

	writeFileSync(OUTPUT_PATH, combined);
	writeFileSync(TEMPORAL_CATALOG_PATH, temporalCatalog);
	console.log(`==> Done. Wrote ${OUTPUT_PATH} (${combined.length} bytes).`);
}

main().catch((err) => {
	console.error('FATAL:', err);
	process.exit(1);
});
