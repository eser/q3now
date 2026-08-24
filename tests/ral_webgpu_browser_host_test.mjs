// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from "node:assert/strict";
import {
  RAL_WEBGPU_BROWSER_HOST_SCHEMA_VERSION,
  RalWebGpuBrowserHostError,
  createRalWebGpuBrowserHost
} from "../code/render/ral/backends/webgpu/ral_webgpu_browser_host.mjs";

const deferred = () => {
  let resolve;
  const promise = new Promise((done) => { resolve = done; });
  return { promise, resolve };
};

class FakePass {
  constructor(host) { this.host = host; this.ended = false; }
  setPipeline(value) { assert.ok(value); }
  setVertexBuffer(slot, value) { assert.equal(slot, 0); assert.ok(value); }
  setIndexBuffer(value, format) { assert.ok(value); assert.equal(format, "uint32"); }
  setBindGroup(index, value) { assert.equal(index, 0); assert.ok(value); }
  drawIndexed(count, instances) { assert.equal(count, 3); assert.equal(instances, 1); this.host.draws++; }
  end() { this.ended = true; }
}

class FakeEncoder {
  constructor(host) { this.host = host; this.pass = null; }
  beginRenderPass(desc) { assert.equal(desc.colorAttachments.length, 1); return (this.pass = new FakePass(this.host)); }
  copyTextureToBuffer(source, destination, size) {
    assert.equal(source.texture.kind, "canvas-texture");
    assert.equal(destination.bytesPerRow, 5120); assert.deepEqual(size, { width: 1280, height: 720, depthOrArrayLayers: 1 });
    this.copied = true;
  }
  finish() { assert.equal(this.pass?.ended === true || this.copied === true, true); return { kind: "command-buffer" }; }
}

function fakeBrowser() {
  const lost = deferred();
  const host = { created: 0, destroyed: 0, destroyedKinds: [], writes: 0, draws: 0, submits: 0, unconfigures: 0 };
  const resource = (kind) => ({ kind, destroy() { host.destroyed++; host.destroyedKinds.push(kind); } });
  const queue = {
    writeBuffer(buffer, offset, data) { assert.equal(buffer.kind, "buffer"); assert.equal(offset, 0); assert.ok(data.byteLength); host.writes++; },
    writeTexture(target, data, layout, size) { assert.equal(target.texture.kind, "texture"); assert.ok(data.byteLength && layout.bytesPerRow && size.width); host.writes++; },
    submit(items) { assert.equal(items.length, 1); host.submits++; },
    onSubmittedWorkDone() { return Promise.resolve(); }
  };
  const device = {
    limits: { maxTextureDimension2D: 8192, maxBindGroups: 4, maxStorageBufferBindingSize: 134217728 },
    queue, lost: lost.promise,
    createBuffer(descriptor) { host.created++;
      if ((descriptor?.usage & 1) !== 0) {
        const bytes = new Uint8Array(descriptor.size); bytes.set([10, 20, 30, 255]);
        return { ...resource("buffer"), async mapAsync() {}, getMappedRange() { return bytes.buffer; }, unmap() {} };
      }
      return resource("buffer");
    },
    createTexture() { host.created++; return { ...resource("texture"), createView() { return { kind: "view" }; } }; },
    createSampler() { host.created++; return resource("sampler"); },
    createShaderModule({ code }) { assert.match(code, /@vertex/); return resource("shader"); },
    async createRenderPipelineAsync() { return resource("pipeline"); },
    createBindGroup() { return resource("bind-group"); },
    createCommandEncoder() { return new FakeEncoder(host); },
    destroy() { host.destroyed++; host.destroyedKinds.push("device"); }
  };
  const adapter = { async requestDevice() { return device; } };
  const gpu = {
    async requestAdapter() { return adapter; },
    getPreferredCanvasFormat() { return "bgra8unorm"; }
  };
  const context = {
    configure(desc) { assert.equal(desc.device, device); assert.equal(desc.format, "bgra8unorm"); assert.equal(desc.usage, 17); },
    unconfigure() { host.unconfigures++; },
    getCurrentTexture() { return { kind: "canvas-texture", createView() { return { kind: "view" }; } }; }
  };
  const canvas = { width: 0, height: 0, style: {}, getContext(kind) { assert.equal(kind, "webgpu"); return context; } };
  return { gpu, canvas, lost, host };
}

const browser = fakeBrowser();
const webgpu = createRalWebGpuBrowserHost({ gpu: browser.gpu, canvas: browser.canvas, devicePixelRatio: () => 2 });
const ready = await webgpu.initialize(7, {});
assert.equal(ready.schemaVersion, RAL_WEBGPU_BROWSER_HOST_SCHEMA_VERSION);
assert.equal(ready.ready, true);
assert.equal(ready.limits.maxTextureDimension2D, 8192);
assert.equal(ready.canvasFormat, "bgra8unorm");

const vertex = webgpu.createBuffer(7, { size: 64, usage: 1 });
const index = webgpu.createBuffer(7, { size: 64, usage: 2 });
const texture = webgpu.createTexture(7, { size: { width: 2, height: 2 }, format: "rgba8unorm", usage: 3 });
webgpu.createSampler(7, {});
const shader = webgpu.createShaderModule(7, { code: "@vertex fn main() {}" });
assert.ok(shader > 0);
const pipeline = await webgpu.createRenderPipeline(7, { vertex: {}, fragment: {} });
const bindGroup = webgpu.createBindGroup(7, { layout: {}, entries: [] });
webgpu.writeBuffer(7, vertex, 0, new Uint8Array([1, 2, 3, 4]));
webgpu.writeTexture(7, texture, new Uint8Array(256), { bytesPerRow: 256 }, { width: 2, height: 2 });
assert.equal(browser.host.writes, 2);

let configured = webgpu.configureCanvas(7, { cssWidth: 640, cssHeight: 360 });
assert.equal(configured.pixelWidth, 1280); assert.equal(configured.pixelHeight, 720);
assert.equal(browser.canvas.style.width, "640px"); assert.equal(browser.canvas.style.height, "360px");
assert.equal(webgpu.configureCanvas(7, { cssWidth: 0, cssHeight: 0 }).suspended, true);
configured = webgpu.configureCanvas(7, { cssWidth: 1280, cssHeight: 720, dpr: 1 });
assert.equal(configured.pixelWidth, 1280); assert.equal(configured.pixelHeight, 720);
const frame = webgpu.acquireCanvasTexture(7);
const capture = webgpu.captureNextCanvas(7);
const encoder = webgpu.createCommandEncoder(7);
const pass = webgpu.beginRenderPass(7, encoder, frame.viewHandle);
assert.equal(webgpu.recordIndexedDraw(7, pass, {
  pipelineHandle: pipeline, vertexBufferHandle: vertex, indexBufferHandle: index,
  bindGroups: [{ index: 0, handle: bindGroup }], indexCount: 3
}), true);
assert.equal(webgpu.endRenderPass(7, pass), true);
const commandBuffer = webgpu.finishEncoder(7, encoder);
const submission = webgpu.submit(7, commandBuffer);
assert.equal(webgpu.pollSubmission(7, submission).status, "pending");
assert.equal(webgpu.presentCanvas(7, frame.textureHandle), true);
const captured = await capture;
assert.equal(captured.width, 1280); assert.equal(captured.height, 720);
assert.deepEqual([...captured.rgba.subarray(0, 4)], [30, 20, 10, 255]);
await Promise.resolve(); await Promise.resolve();
assert.equal(webgpu.pollSubmission(7, submission).status, "ready");
assert.equal(browser.host.draws, 1); assert.equal(browser.host.submits, 1);

assert.throws(() => webgpu.createBuffer(8, { size: 4, usage: 1 }),
  (error) => error instanceof RalWebGpuBrowserHostError && error.code === "stale-generation");
assert.throws(() => webgpu.writeBuffer(7, 0x7fffffff, 0, new Uint8Array(4)),
  (error) => error instanceof RalWebGpuBrowserHostError && error.code === "unknown-handle");
browser.lost.resolve({ reason: "unknown", message: "fixture loss" });
await Promise.resolve(); await Promise.resolve();
assert.equal(webgpu.receipt(7).status, "lost");
assert.equal(webgpu.receipt(7).lost.message, "fixture loss");
assert.throws(() => webgpu.createBuffer(7, { size: 4, usage: 1 }),
  (error) => error instanceof RalWebGpuBrowserHostError && error.code === "device-lost");
assert.equal(webgpu.destroy(7), true);
assert.ok(browser.host.destroyed >= browser.host.created);
assert.equal(browser.host.destroyedKinds.at(-1), "device");

const unavailable = createRalWebGpuBrowserHost();
const unsupported = await unavailable.initialize(9);
assert.equal(unsupported.status, "unsupported"); assert.equal(unsupported.ready, false);
assert.match(unsupported.reason, /^WebGPU is unavailable/);
assert.equal(unavailable.destroy(9), true);

console.log("ral WebGPU browser host contract: PASS");
