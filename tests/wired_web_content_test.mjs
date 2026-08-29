// SPDX-License-Identifier: GPL-3.0-or-later

import assert from "node:assert/strict";
import { webcrypto } from "node:crypto";
import { createWiredWebContentModule } from "../code/web/wired_web_content.mjs";

const encoder = new TextEncoder();
const fontFiles = [
  "jetbrainsmono.json", "jetbrainsmono.png",
  "oxanium-medium.json", "oxanium-medium.png",
  "oxanium.json", "oxanium.png",
  "sansman-bold-italic.json", "sansman-bold-italic.png",
  "sansman-bold.json", "sansman-bold.png",
  "sansman-italic.json", "sansman-italic.png",
  "sansman-medium.json", "sansman-medium.png",
  "sansman-regular.json", "sansman-regular.png",
  "sharetechmono.json", "sharetechmono.png",
  "wui_icons.json", "wui_icons.png"
];
const uiFiles = ["_tokens.wui", "assets.wui", "main.wui", "classic.wui", "perspective.wui",
  "loading_screen.wui", "overlay.wui"];
const payloads = new Map([
  ["https://content.test/pax01.sw3z", encoder.encode("arena17-bsp")],
  ["https://content.test/pax21.sw3z", encoder.encode("wired-assets")],
  ["https://content.test/gameclwasm32.wasm", encoder.encode("gamecl-browser-module")],
  ["https://content.test/gamesvwasm32.wasm", encoder.encode("gamesv-browser-module")],
  ["https://content.test/default.cfg", encoder.encode("seta r_mode -1")],
  ["https://content.test/visor-animation.cfg", encoder.encode("0 30 0 25")],
  ["https://content.test/authored-content.wac", encoder.encode("WAC1|compiled-authored-content")]
]);
for (const file of fontFiles)
  payloads.set(`https://content.test/fonts/${file}`, encoder.encode(`font:${file}`));
for (const file of uiFiles)
  payloads.set(`https://content.test/ui/${file}`, encoder.encode(`ui:${file}`));
const digest = async (bytes) => Buffer.from(await webcrypto.subtle.digest("SHA-256", bytes)).toString("hex");
const ids = ["pax01", "pax21", "gamecl", "gamesv", "config", "visor-animation", "authored-content",
  ...fontFiles.map(file => `font-${file}`), ...uiFiles.map(file => `ui-${file}`)];
const paths = ["/base/pax01.sw3z", "/base/pax21.sw3z", "/base/gameclwasm32.wasm",
  "/base/gamesvwasm32.wasm", "/base/default.cfg",
  "/base/characters/visor/models/animation.cfg", "/base/web/authored-content.wac",
  ...fontFiles.map(file => `/base/fonts/${file}`), ...uiFiles.map(file => `/base/ui/${file}`)];
const expectedAssetCount = paths.length;
const makeManifest = async (version) => ({ schemaVersion: 1, version,
  assets: await Promise.all(ids.map(async (id, index) => {
    const url = id === "visor-animation" ? "https://content.test/visor-animation.cfg"
      : id.startsWith("font-") ? `https://content.test/fonts/${id.slice(5)}`
      : id.startsWith("ui-") ? `https://content.test/ui/${id.slice(3)}`
      : `https://content.test/${paths[index].split("/").at(-1)}`;
    const bytes = payloads.get(url);
    return { id, url, mountPath: paths[index], bytes: bytes.byteLength,
      sha256: await digest(bytes) };
  })) });

class MemoryCache {
  constructor() { this.items = new Map(); }
  async match(url) { return this.items.get(url)?.clone(); }
  async put(url, response) { this.items.set(url, response.clone()); }
  async delete(url) { return this.items.delete(url); }
}
class MemoryCaches {
  constructor() { this.stores = new Map(); this.deleted = []; }
  async keys() { return [...this.stores.keys()]; }
  async open(name) { if (!this.stores.has(name)) this.stores.set(name, new MemoryCache()); return this.stores.get(name); }
  async delete(name) { this.deleted.push(name); return this.stores.delete(name); }
}
const makeFS = () => ({ files: new Map(), mkdir() {},
  writeFile(path, bytes) { this.files.set(path, new Uint8Array(bytes)); } });

const caches = new MemoryCaches();
let manifest = await makeManifest("build-41");
let assetDownloads = 0;
const fetch = async (url) => {
  if (url === "https://content.test/manifest.json")
    return new Response(JSON.stringify(manifest));
  assetDownloads++;
  const bytes = payloads.get(String(url));
  return bytes ? new Response(bytes) : new Response("missing", { status: 404 });
};
const states = [];
const boot = () => createWiredWebContentModule({
  moduleFactory: async () => ({ FS: makeFS() }),
  manifestUrl: "https://content.test/manifest.json", fetch, caches,
  crypto: webcrypto, publish: event => states.push(event)
});

const first = await boot();
assert.equal(assetDownloads, expectedAssetCount);
assert.deepEqual([...first.FS.files.keys()], paths);
assert.equal(first.wiredWebContentReceipt.version, "build-41");
assert.ok(states.some(event => event.state === "asset-download"));
assert.equal(states.at(-1).state, "ready");

states.length = 0;
const second = await boot();
assert.equal(assetDownloads, expectedAssetCount);
assert.equal(second.wiredWebContentReceipt.manifestHash, first.wiredWebContentReceipt.manifestHash);
assert.equal(states.filter(event => event.state === "asset-cache-hit").length, expectedAssetCount);

manifest = await makeManifest("build-42");
await boot();
assert.equal(assetDownloads, expectedAssetCount * 2);
assert.equal(caches.deleted.length, 1);

manifest.assets[0].sha256 = "0".repeat(64);
await assert.rejects(boot(), /integrity check failed for pax01/);
assert.equal(states.at(-1).state, "failed");

console.log("wired Web production content bootstrap contract: PASS");
