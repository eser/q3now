// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// Convert a 32bpp / 24bpp uncompressed TGA to a binary PPM (P6) so the
// image can be visually inspected by tools that don't parse TGA.
import fs from 'node:fs';
import path from 'node:path';

const [, , inPath, outPath] = process.argv;
if (!inPath || !outPath) {
    console.error('usage: node tools/tga2ppm.mjs <input.tga> <output.ppm>');
    process.exit(2);
}
const buf = fs.readFileSync(inPath);
const idLen = buf[0];
const colorMapType = buf[1];
const imgType = buf[2];
const w = buf.readUInt16LE(12);
const h = buf.readUInt16LE(14);
const bpp = buf[16];
const desc = buf[17];
const yFlip = (desc & 0x20) === 0; // origin at bottom-left when bit5 is clear → need vertical flip
if (colorMapType !== 0 || imgType !== 2) {
    console.error(`unsupported TGA: colorMapType=${colorMapType} imgType=${imgType}`);
    process.exit(1);
}
if (bpp !== 24 && bpp !== 32) {
    console.error(`unsupported bpp=${bpp}`);
    process.exit(1);
}
const pixOff = 18 + idLen;
const pix = buf.slice(pixOff);
const Bpp = bpp / 8;
const out = Buffer.alloc(w * h * 3);
for (let y = 0; y < h; y++) {
    const srcRow = yFlip ? (h - 1 - y) : y;
    for (let x = 0; x < w; x++) {
        const s = (srcRow * w + x) * Bpp;
        const d = (y * w + x) * 3;
        // TGA is BGR(A); PPM is RGB
        out[d + 0] = pix[s + 2];
        out[d + 1] = pix[s + 1];
        out[d + 2] = pix[s + 0];
    }
}
const header = Buffer.from(`P6\n${w} ${h}\n255\n`, 'ascii');
fs.writeFileSync(outPath, Buffer.concat([header, out]));
console.log(`wrote ${outPath} (${w}x${h}, ${(out.length/1024)|0} KiB pixels)`);
