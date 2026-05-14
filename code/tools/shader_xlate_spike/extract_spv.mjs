// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// extract_spv.mjs — pull SPIR-V blobs out of a generated shader_data.c-style C
// file (the `const unsigned char NAME[N] = { 0x.., ... };` arrays compile.mjs
// emits) into standalone .spv files, so the shader_xlate_spike test exe has real
// q3now SPIR-V to chew on. Throwaway helper for the Phase 7 spike.
//
//   node extract_spv.mjs <shader_data.c> <outdir> [symbol ...]
//     with no symbols: extracts every array in the file.
//
// (python3 isn't on PATH in the dev image; the project already uses node for
//  compile.mjs, so node it is.)

import fs from "node:fs";
import path from "node:path";

const args = process.argv.slice( 2 );
if ( args.length < 2 ) {
	console.error( "usage: node extract_spv.mjs <shader_data.c> <outdir> [symbol ...]" );
	process.exit( 1 );
}
const [ srcPath, outDir, ...wantList ] = args;
const want = new Set( wantList );
fs.mkdirSync( outDir, { recursive: true } );

const src = fs.readFileSync( srcPath, "utf8" );

// Split on the array-declaration boundary; each chunk after the first starts
// with "NAME[N] = { ...bytes... };". Robust to multi-line byte rows.
const re = /const\s+unsigned\s+char\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*(\d+)\s*\]\s*=\s*\{([\s\S]*?)\}\s*;/g;
let m, count = 0;
while ( ( m = re.exec( src ) ) !== null ) {
	const [ , name, lenStr, body ] = m;
	if ( want.size && !want.has( name ) ) continue;
	const len = parseInt( lenStr, 10 );
	const bytes = body
		.split( "," )
		.map( s => s.trim() )
		.filter( s => s.length > 0 )
		.map( s => parseInt( s, 16 ) );          // emitted as 0xXX
	if ( bytes.length !== len ) {
		console.error( `WARN ${name}: parsed ${bytes.length} bytes, declared ${len}` );
	}
	if ( bytes.length % 4 !== 0 ) {
		console.error( `WARN ${name}: ${bytes.length} bytes — not a SPIR-V word multiple` );
	}
	const magic = bytes.length >= 4
		? "0x" + bytes.slice( 0, 4 ).reverse().map( b => b.toString( 16 ).padStart( 2, "0" ) ).join( "" )
		: "(short)";
	const outFile = path.join( outDir, name + ".spv" );
	fs.writeFileSync( outFile, Buffer.from( bytes ) );
	console.log( `${name} -> ${outFile}  (${bytes.length} bytes, magic ${magic})` );
	count++;
}
if ( count === 0 ) { console.error( "no matching arrays found" ); process.exit( 2 ); }
console.log( `extracted ${count} blob(s)` );
