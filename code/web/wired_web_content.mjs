// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

export const WIRED_WEB_CONTENT_SCHEMA_VERSION = 1;

const cachePrefix = "wired-web-content-v1-";
const requiredAssets = Object.freeze({
  pax01: "/base/pax01.sw3z",
  pax21: "/base/pax21.sw3z",
  gamecl: "/base/gameclwasm32.wasm",
  gamesv: "/base/gamesvwasm32.wasm",
  config: "/base/default.cfg",
  "visor-animation": "/base/characters/visor/models/animation.cfg",
  "authored-content": "/base/web/authored-content.wac"
});
const requiredFontFiles = Object.freeze([
  "jetbrainsmono.json", "jetbrainsmono.png", "oxanium-medium.json", "oxanium-medium.png",
  "oxanium.json", "oxanium.png", "sansman-bold-italic.json", "sansman-bold-italic.png",
  "sansman-bold.json", "sansman-bold.png", "sansman-italic.json", "sansman-italic.png",
  "sansman-medium.json", "sansman-medium.png", "sansman-regular.json", "sansman-regular.png",
  "sharetechmono.json", "sharetechmono.png", "wui_icons.json", "wui_icons.png"
]);
const requiredUiFiles = Object.freeze([
  "_tokens.wui", "assets.wui", "main.wui", "classic.wui", "perspective.wui",
  "loading_screen.wui", "overlay.wui"
]);

const hex = (bytes) => [...new Uint8Array(bytes)]
  .map(value => value.toString(16).padStart(2, "0")).join("");

async function sha256(subtle, bytes) {
  if (!subtle?.digest) throw new Error("Web Crypto SHA-256 is unavailable");
  return hex(await subtle.digest("SHA-256", bytes));
}

function validateManifest(manifest) {
  if (!manifest || manifest.schemaVersion !== WIRED_WEB_CONTENT_SCHEMA_VERSION)
    throw new Error("unsupported Wired browser content manifest schema");
  if (typeof manifest.version !== "string" || !/^[A-Za-z0-9._-]{1,80}$/.test(manifest.version))
    throw new Error("invalid Wired browser content version");
  if (!Array.isArray(manifest.assets)) throw new Error("browser content assets are missing");
  const seen = new Set();
  for (const asset of manifest.assets) {
	const expectedMount = requiredAssets[asset?.id]
		?? (String(asset?.id ?? "").startsWith("font-")
			? `/base/fonts/${String(asset.id).slice(5)}`
			: String(asset?.id ?? "").startsWith("ui-")
				? `/base/ui/${String(asset.id).slice(3)}` : undefined);
    if (!asset || expectedMount !== asset.mountPath || seen.has(asset.id))
      throw new Error(`invalid browser content asset ${String(asset?.id ?? "unknown")}`);
    if (typeof asset.url !== "string" || !asset.url || asset.url.startsWith("file:")
        || asset.url.includes("\\") || asset.url.split("/").includes(".."))
      throw new Error(`unsafe browser content URL for ${asset.id}`);
    if (!/^[0-9a-f]{64}$/.test(asset.sha256) || !Number.isSafeInteger(asset.bytes)
        || asset.bytes <= 0)
      throw new Error(`invalid browser content integrity metadata for ${asset.id}`);
    seen.add(asset.id);
  }
  for (const id of Object.keys(requiredAssets)) {
    if (!seen.has(id)) throw new Error(`required browser content asset is missing: ${id}`);
  }
	for (const file of requiredFontFiles) {
		if (!seen.has(`font-${file}`))
			throw new Error(`required browser font asset is missing: ${file}`);
	}
	for (const file of requiredUiFiles) {
		if (!seen.has(`ui-${file}`))
			throw new Error(`required browser UI source is missing: ${file}`);
	}
  return manifest;
}

function mkdirTree(FS, path) {
  if (typeof FS.mkdirTree === "function") { FS.mkdirTree(path); return; }
  let current = "";
  for (const part of path.split("/").filter(Boolean)) {
    current += `/${part}`;
    try { FS.mkdir(current); } catch { /* already exists */ }
  }
}

export async function createWiredWebContentModule({ moduleFactory, manifestUrl,
    moduleOptions = {}, fetch: fetchImpl = globalThis.fetch?.bind(globalThis),
    caches: cacheStorage = globalThis.caches,
    crypto: cryptoImpl = globalThis.crypto,
    publish = () => {} } = {}) {
  if (typeof moduleFactory !== "function") throw new TypeError("an Emscripten module factory is required");
  if (typeof fetchImpl !== "function") throw new TypeError("browser fetch is required");
  if (!manifestUrl) throw new TypeError("a browser content manifest URL is required");

  const emit = (state, detail = {}) => publish(Object.freeze({ state, ...detail }));
  try {
    emit("manifest-download");
    const manifestResponse = await fetchImpl(manifestUrl, { cache: "no-store" });
    if (!manifestResponse?.ok) throw new Error(`browser content manifest download failed (${manifestResponse?.status ?? 0})`);
    const manifestBytes = await manifestResponse.arrayBuffer();
    const manifest = validateManifest(JSON.parse(new TextDecoder().decode(manifestBytes)));
    const manifestHash = await sha256(cryptoImpl?.subtle, manifestBytes);
    const cacheName = `${cachePrefix}${manifest.version}-${manifestHash.slice(0, 16)}`;
    emit("manifest-verified", { version: manifest.version, manifestHash });

    let cache = null;
    if (cacheStorage?.open && cacheStorage?.keys && cacheStorage?.delete) {
      for (const name of await cacheStorage.keys()) {
        if (name.startsWith(cachePrefix) && name !== cacheName) await cacheStorage.delete(name);
      }
      cache = await cacheStorage.open(cacheName);
      emit("cache-ready", { version: manifest.version });
    }

    const verified = [];
    for (const asset of manifest.assets) {
      const assetUrl = new URL(asset.url, manifestUrl).href;
      let response = cache ? await cache.match(assetUrl) : null;
      let source = response ? "cache" : "network";
      if (!response) {
        emit("asset-download", { asset: asset.id });
        response = await fetchImpl(assetUrl, { cache: "no-store" });
        if (!response?.ok) throw new Error(`browser content download failed for ${asset.id} (${response?.status ?? 0})`);
      } else emit("asset-cache-hit", { asset: asset.id });

      let bytes = await response.arrayBuffer();
      let digest = await sha256(cryptoImpl?.subtle, bytes);
      if ((bytes.byteLength !== asset.bytes || digest !== asset.sha256) && source === "cache") {
        await cache.delete(assetUrl);
        emit("asset-cache-invalid", { asset: asset.id });
        response = await fetchImpl(assetUrl, { cache: "no-store" });
        if (!response?.ok) throw new Error(`browser content refresh failed for ${asset.id} (${response?.status ?? 0})`);
        bytes = await response.arrayBuffer();
        digest = await sha256(cryptoImpl?.subtle, bytes);
        source = "network";
      }
      if (bytes.byteLength !== asset.bytes || digest !== asset.sha256)
        throw new Error(`browser content integrity check failed for ${asset.id}`);
      emit("asset-verified", { asset: asset.id, source, bytes: bytes.byteLength });
      if (cache && source === "network") await cache.put(assetUrl,
        new Response(bytes, { status: 200, headers: { "content-type": "application/octet-stream" } }));
      verified.push({ asset, bytes: new Uint8Array(bytes) });
    }

    const module = await moduleFactory(moduleOptions);
    if (!module?.FS?.writeFile) throw new Error("Emscripten filesystem is unavailable");
    emit("mounting", { version: manifest.version });
    for (const { asset, bytes } of verified) {
      mkdirTree(module.FS, asset.mountPath.slice(0, asset.mountPath.lastIndexOf("/")));
      module.FS.writeFile(asset.mountPath, bytes);
    }
    const receipt = Object.freeze({ schemaVersion: WIRED_WEB_CONTENT_SCHEMA_VERSION,
      version: manifest.version, manifestHash, cacheName,
      assets: Object.freeze(verified.map(({ asset }) => Object.freeze({
        id: asset.id, mountPath: asset.mountPath, sha256: asset.sha256, bytes: asset.bytes
      }))) });
    Object.defineProperty(module, "wiredWebContentReceipt", { value: receipt, enumerable: true });
    emit("ready", { version: manifest.version, assets: receipt.assets.length });
    return module;
  } catch (error) {
    emit("failed", { reason: String(error?.message ?? error) });
    throw error;
  }
}
