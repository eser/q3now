// SPDX-License-Identifier: GPL-3.0-or-later

import assert from "node:assert/strict";
import { createWiredWebClient } from "../code/web/wired_web_client_module.mjs";
import { RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION } from
  "../code/render/ral/backends/webgpu/ral_webgpu_browser_dispatch.mjs";

const memory = new WebAssembly.Memory({ initial: 2 });
const canvas = { width: 0, height: 0, style: {}, getContext() { return null; } };
const statusElement = { textContent: "", hidden: false, dataset: {} };
const callbacks = new Map(); let nextFrame = 1;
const raf = (callback) => { const id = nextFrame++; callbacks.set(id, callback); return id; };
const caf = (id) => callbacks.delete(id);
const step = async (time) => {
  const entry = callbacks.entries().next().value;
  assert.ok(entry); callbacks.delete(entry[0]); entry[1](time);
  await Promise.resolve(); await Promise.resolve();
};
const listeners = new Map();
const eventTarget = {
  addEventListener(type, fn) { listeners.set(type, fn); },
  removeEventListener(type, fn) { if (listeners.get(type) === fn) listeners.delete(type); }
};
const events = [];
const factory = async () => ({ wasmMemory: memory,
  _RalWebGpu_BrowserModuleStart(generation) {
    const request = new DataView(memory.buffer, 0x1000, 24);
    request.setUint32(0, RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION, true);
    request.setUint32(4, 1, true);
    request.setUint32(8, 24, true); request.setBigUint64(16, BigInt(generation), true);
    globalThis.wiredRalWebGpuDispatch(1, 0x1000, 24, 0x2000, 24); return 1;
  },
  _RalWebGpu_BrowserModulePoll() { return 2; },
  _GetRefAPI() {}, _WiredWebGpu_RendererPoll() { return 2; },
  _WiredWebGpu_RendererResize(width, height) {
    events.push(["resize", width, height]); canvas.width = width; canvas.height = height; return 1;
  },
  _WiredWeb_ClientStart() { events.push("client-start"); return 1; },
  _WiredWeb_ClientFrame(time) { events.push(["frame", time]); return 1; },
  _WiredWeb_ClientShutdown() { events.push("client-stop"); },
  _WiredWeb_ClientPresentationChanged() { events.push("presentation-changed"); return 1; },
  _WiredWeb_InputKey(key, down) { events.push(["key", key, down]); },
  _WiredWeb_InputChar(code) { events.push(["char", code]); },
  _WiredWeb_InputMouse(x, y) { events.push(["mouse", x, y]); },
  _RalWebGpu_BrowserModuleStop() { events.push("renderer-stop"); }
});

const client = await createWiredWebClient({ moduleFactory: factory, gpu: undefined, canvas,
  statusElement, requestAnimationFrame: raf, cancelAnimationFrame: caf, eventTarget });
assert.equal(canvas.width, 1280); assert.equal(canvas.height, 720);
await step(1);
for (let frame = 2; client.state() === "starting" && frame <= 8; ++frame)
  await step(frame);
assert.equal(client.state(), "failed");
assert.equal(client.resize({ width: 1600, height: 900 }), false);
assert.match(statusElement.textContent, /WebGPU is unavailable/);
assert.equal(statusElement.hidden, false);
client.destroy();
assert.deepEqual(events, ["renderer-stop"]);
assert.equal(listeners.size, 0);
assert.equal(globalThis.wiredRalWebGpuDispatch, undefined);

console.log("wired Web client browser ownership contract: PASS");
