// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

export const WIRED_LIGHTING_COOK_OPFS_SCHEMA_VERSION = 1;

const cacheKeyPattern = /^wrlight-v1-[0-9]+-[0-9a-f]{16}-[0-9a-f]{16}$/;
const stemPattern = /^[A-Za-z0-9_-]{1,255}$/;

function bytesView(bytes) {
  if (bytes instanceof Uint8Array && bytes.byteLength > 0) return bytes;
  if (ArrayBuffer.isView(bytes) && bytes.byteLength > 0) {
    return new Uint8Array(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  }
  if (bytes instanceof ArrayBuffer && bytes.byteLength > 0) return new Uint8Array(bytes);
  throw new TypeError("lighting cook publication requires non-empty bytes");
}

function sidecarName(worldStem, kind) {
  if (!stemPattern.test(worldStem)) throw new TypeError("invalid lighting world stem");
  if (kind === "directional") return `${worldStem}.wlight`;
  if (kind === "irradiance") return `${worldStem}.wprobe`;
  throw new TypeError("invalid lighting sidecar kind");
}

export function createWiredLightingCookOpfs(directory) {
  if (!directory || typeof directory.getFileHandle !== "function") {
    throw new TypeError("an OPFS directory handle is required");
  }
  let serial = 0;

  const writeNamedAtomic = async (name, input) => {
    const bytes = bytesView(input);
    const temporaryName = `.${name}.tmp-${++serial}`;
    const temporary = await directory.getFileHandle(temporaryName, { create: true });
    if (typeof temporary.move !== "function") {
      await directory.removeEntry?.(temporaryName).catch?.(() => {});
      throw new Error("OPFS atomic move is unavailable; publication refused");
    }
    try {
      const writable = await temporary.createWritable({ keepExistingData: false });
      await writable.write(bytes);
      await writable.close();
      await temporary.move(name);
    } catch (error) {
      await directory.removeEntry?.(temporaryName).catch?.(() => {});
      throw error;
    }
    return Object.freeze({ schemaVersion: WIRED_LIGHTING_COOK_OPFS_SCHEMA_VERSION,
      name, byteLength: bytes.byteLength, ready: true });
  };

  const readNamed = async (name) => {
    const handle = await directory.getFileHandle(name);
    const file = await handle.getFile();
    const bytes = new Uint8Array(await file.arrayBuffer());
    if (!bytes.byteLength) throw new Error("empty lighting cook product");
    return bytes;
  };

  return Object.freeze({
    async writeAtomic(keyText, bytes) {
      if (!cacheKeyPattern.test(keyText)) throw new TypeError("invalid lighting cache key");
      return writeNamedAtomic(`${keyText}.wrcache`, bytes);
    },
    async read(keyText) {
      if (!cacheKeyPattern.test(keyText)) throw new TypeError("invalid lighting cache key");
      return readNamed(`${keyText}.wrcache`);
    },
    async publishSidecar(worldStem, kind, bytes) {
      return writeNamedAtomic(sidecarName(worldStem, kind), bytes);
    },
    async readSidecar(worldStem, kind) {
      return readNamed(sidecarName(worldStem, kind));
    }
  });
}
