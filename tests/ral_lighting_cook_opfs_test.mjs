// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

import assert from "node:assert/strict";
import { createWiredLightingCookOpfs } from
  "../code/tools/lighting_cook/lighting_cook_opfs.mjs";

class FakeFileHandle {
  constructor(directory, name) { this.directory = directory; this.name = name; }
  async createWritable() {
    let staged = null;
    return { write: async (bytes) => { staged = new Uint8Array(bytes); },
      close: async () => { this.directory.files.set(this.name, staged); } };
  }
  async move(name) {
    const bytes = this.directory.files.get(this.name);
    if (!bytes) throw new Error("temporary product missing");
    this.directory.files.set(name, bytes);
    this.directory.files.delete(this.name);
    this.name = name;
  }
  async getFile() {
    const bytes = this.directory.files.get(this.name);
    if (!bytes) throw new Error("product missing");
    return { arrayBuffer: async () => bytes.slice().buffer };
  }
}

class FakeDirectory {
  constructor() { this.files = new Map([["source.map", new Uint8Array([9, 8, 7])]]); }
  async getFileHandle(name, options = {}) {
    if (!this.files.has(name) && !options.create) throw new Error("not found");
    if (options.create && !this.files.has(name)) this.files.set(name, new Uint8Array());
    return new FakeFileHandle(this, name);
  }
  async removeEntry(name) { this.files.delete(name); }
}

const directory = new FakeDirectory();
const opfs = createWiredLightingCookOpfs(directory);
const key = "wrlight-v1-1-0000000000000001-0000000000000002";
const artifact = new Uint8Array([1, 2, 3, 4]);
await opfs.writeAtomic(key, artifact);
assert.deepEqual(await opfs.read(key), artifact);
await opfs.publishSidecar("arena7", "directional", artifact);
await opfs.publishSidecar("arena7", "irradiance", new Uint8Array([5, 6, 7]));
assert.deepEqual(await opfs.readSidecar("arena7", "directional"), artifact);
assert.deepEqual(await opfs.readSidecar("arena7", "irradiance"), new Uint8Array([5, 6, 7]));
assert.deepEqual(directory.files.get("source.map"), new Uint8Array([9, 8, 7]));
assert.equal([...directory.files.keys()].some((name) => name.includes(".tmp-")), false);
await assert.rejects(() => opfs.publishSidecar("../arena7", "directional", artifact), TypeError);
await assert.rejects(() => opfs.writeAtomic("invalid", artifact), TypeError);
console.log("ral lighting cook OPFS atomic publication contract: PASS");
