// gen_crosshair_plus.mjs — deterministic symmetric '+' crosshair generator
// (dispatch 5.22 S3). Emits a 32x32 RGBA PNG: white gapped vertical +
// horizontal ticks about the texture center + a 2x2 center dot, fully
// transparent background. The engine tints via cl_crosshairColor and
// stretch-blits the whole texture, so the source is white + symmetric.
// Regenerate: node tests/visual/scripts/gen_crosshair_plus.mjs <out.png>
import { deflateSync } from "node:zlib";
import { writeFileSync } from "node:fs";

const N = 32;
const out = process.argv[2] || "modfiles/gfx/2d/crosshairDefault.png";

// CRC32 (PNG polynomial 0xEDB88320)
const crcTable = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
function crc32(buf) {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = crcTable[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length, 0);
  const t = Buffer.from(type, "ascii");
  const body = Buffer.concat([t, data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(body), 0);
  return Buffer.concat([len, body, crc]);
}

// pixel test — white where on an arm or the filled center, else transparent.
// Dispatch 5.29: 2px arms (cols/rows 15-16) spanning 5..12 & 20..27, plus a
// 4x4 FILLED center dot (14..17) so the exact texture center is opaque after
// the 32->48 stretch and the '+' clears the crosshair region SSIM gate.
// Gapped to match the mockup '+' (qw-screens.jsx: arms 8..18 & 30..40, dot @24).
const lo1 = 5, hi1 = 12, lo2 = 20, hi2 = 27, a0 = 15, a1 = 16, d0 = 14, d1 = 17;
function on(x, y) {
  const vbar = (x === a0 || x === a1) && ((y >= lo1 && y <= hi1) || (y >= lo2 && y <= hi2));
  const hbar = (y === a0 || y === a1) && ((x >= lo1 && x <= hi1) || (x >= lo2 && x <= hi2));
  const dot  = (x >= d0 && x <= d1) && (y >= d0 && y <= d1);
  return vbar || hbar || dot;
}

// TGA branch (dispatch 5.31 S3): the hand-rolled PNG registered a valid shader
// handle but rendered as empty via the shader `map` path; a dead-simple
// uncompressed 32-bit TGA removes all decoder ambiguity. Symmetric '+', so the
// top-down vs bottom-up origin is immaterial.
if (out.endsWith(".tga")) {
  const hdr = Buffer.from([
    0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    N & 0xff, (N >> 8) & 0xff, N & 0xff, (N >> 8) & 0xff, 32, 0x28,
  ]);
  const px = Buffer.alloc(N * N * 4);
  let o = 0;
  for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
    const w = on(x, y) ? 255 : 0;
    px[o++] = w; px[o++] = w; px[o++] = w; px[o++] = w; // B G R A (white / clear)
  }
  writeFileSync(out, Buffer.concat([hdr, px]));
  console.log(`wrote ${out} (TGA 32-bit ${N}x${N})`);
  process.exit(0);
}

const raw = Buffer.alloc(N * (1 + N * 4));
for (let y = 0; y < N; y++) {
  const row = y * (1 + N * 4);
  raw[row] = 0; // filter: None
  for (let x = 0; x < N; x++) {
    const p = row + 1 + x * 4;
    const w = on(x, y) ? 255 : 0;
    raw[p] = w; raw[p + 1] = w; raw[p + 2] = w; raw[p + 3] = w;
  }
}

const sig = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(N, 0); ihdr.writeUInt32BE(N, 4);
ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
const png = Buffer.concat([
  sig,
  chunk("IHDR", ihdr),
  chunk("IDAT", deflateSync(raw, { level: 9 })),
  chunk("IEND", Buffer.alloc(0)),
]);
writeFileSync(out, png);
console.log(`wrote ${out} (${png.length} bytes, ${N}x${N} RGBA)`);
