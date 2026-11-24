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
  setBindGroup(index, value) { assert.ok(index === 0 || index === 1); assert.ok(value);
    this.host.bindGroupIndexes.push(index); }
  drawIndexed(count, instances, firstIndex, baseVertex, firstInstance) {
    assert.equal(count, 3); assert.equal(instances, 1); assert.equal(firstIndex, 0);
    assert.equal(baseVertex, 0); assert.equal(firstInstance, 7); this.host.draws++;
  }
  dispatchWorkgroups(x, y, z) { assert.deepEqual([x, y, z], [1, 1, 1]); }
  end() { this.ended = true; }
}

class FakeEncoder {
  constructor(host) { this.host = host; this.passes = []; }
  beginRenderPass(desc) {
    assert.equal(desc.colorAttachments.length, 1);
    this.host.passDescriptors.push(desc);
    const pass = new FakePass(this.host); this.passes.push(pass); return pass;
  }
  beginComputePass(desc) {
    assert.equal(desc.timestampWrites.beginningOfPassWriteIndex, 0);
    assert.equal(desc.timestampWrites.endOfPassWriteIndex, 1);
    const pass = new FakePass(this.host); this.passes.push(pass); return pass;
  }
  resolveQuerySet(querySet, first, count, buffer, offset) {
    assert.equal(querySet.kind, "query-set");
    assert.deepEqual([first, count, buffer.kind, offset], [0, 2, "buffer", 0]);
  }
  copyBufferToBuffer(source, sourceOffset, target, targetOffset, size) {
    assert.deepEqual([source.kind, sourceOffset, target.kind, targetOffset, size],
      ["buffer", 0, "buffer", 0, 16]);
  }
  copyTextureToBuffer(source, destination, size) {
    assert.equal(source.texture.kind, "canvas-texture");
    assert.equal(destination.bytesPerRow, 5120); assert.deepEqual(size, { width: 1280, height: 720, depthOrArrayLayers: 1 });
    this.copied = true;
  }
  finish() { assert.equal(this.passes.every((pass) => pass.ended) || this.copied === true, true); return { kind: "command-buffer" }; }
}

function fakeBrowser() {
  const lost = deferred();
  const host = { created: 0, destroyed: 0, destroyedKinds: [], writes: 0, draws: 0, submits: 0, unconfigures: 0, textureViews: [], passDescriptors: [], bindGroupIndexes: [] };
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
    features: new Set(["timestamp-query"]),
    createBuffer(descriptor) { host.created++;
      if ((descriptor?.usage & 1) !== 0) {
        const bytes = new Uint8Array(descriptor.size);
        if (descriptor.size === 16) {
          const stamps = new BigUint64Array(bytes.buffer);
          stamps[0] = 100n; stamps[1] = host.timestampEnd ?? 350100n;
        } else bytes.set([10, 20, 30, 255]);
        return { ...resource("buffer"), size:descriptor.size, usage:descriptor.usage,
          async mapAsync() { if(host.mapError)throw host.mapError; if(host.mapGate)await host.mapGate; },
          getMappedRange() { return bytes.buffer; }, unmap() { host.unmaps=(host.unmaps??0)+1; } };
      }
      return {...resource("buffer"),size:descriptor.size,usage:descriptor.usage};
    },
    createTexture() { host.created++; return { ...resource("texture"), createView(options) { host.textureViews.push(options); return { kind: "view" }; } }; },
    createSampler() { host.created++; return resource("sampler"); },
    createShaderModule({ code }) { assert.match(code, /@vertex/); return resource("shader"); },
    createBindGroupLayout() { return resource("bind-group-layout"); },
    async createRenderPipelineAsync() { return { ...resource("pipeline"),
      getBindGroupLayout(index) { assert.ok(index === 0 || index === 1); return { kind: "pipeline-bind-group-layout", index }; } }; },
    createBindGroup() { host.bindGroupsCreated = (host.bindGroupsCreated ?? 0) + 1; return resource("bind-group"); },
    createQuerySet() { host.created++; return resource("query-set"); },
    createCommandEncoder() { return new FakeEncoder(host); },
    destroy() { host.destroyed++; host.destroyedKinds.push("device"); }
  };
  const adapter = { features: new Set(["timestamp-query"]),
    async requestDevice(options) {
      assert.deepEqual(options.requiredFeatures, ["timestamp-query"]); return device;
    } };
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
const sampler = webgpu.createSampler(7, {});
const shader = webgpu.createShaderModule(7, { code: "@vertex fn main() {}" });
assert.ok(shader > 0);
const pipeline = await webgpu.createRenderPipeline(7, { vertex: {}, fragment: {} });
const bindGroup = webgpu.createBindGroup(7, { layout: {}, entries: [] });
const bindGroupLayout = webgpu.createBindGroupLayout(7, { entries: [] });
webgpu.createBindGroupFromHandles(7, bindGroupLayout, [
  { binding: 0, kind: 2, handle: texture, offset: 3, size: 1 }
]);
assert.deepEqual(browser.host.textureViews.at(-1), { baseMipLevel: 3, mipLevelCount: 1 });
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
  bindGroups: [{ index: 0, handle: bindGroup }], indexCount: 3, firstInstance: 7
}), true);
assert.equal(webgpu.recordIndexedDraw(7, pass, {
  pipelineHandle: pipeline, vertexBufferHandle: vertex, indexBufferHandle: index,
  textureHandle: texture, samplerHandle: sampler, textureBindGroupSlot: 2,
  bindGroups: [{ index: 0, handle: bindGroup }], indexCount: 3, firstInstance: 7
}), true);
assert.deepEqual(browser.host.bindGroupIndexes, [0, 1, 0]);
assert.equal(webgpu.endRenderPass(7, pass), true);
const distantPass = webgpu.beginRenderPass(7, encoder, frame.viewHandle, {
  colorLoadOp: "load", depthEnabled: true, depthLoadOp: "clear",
  depthStore: true, clearDepth: 1
});
assert.equal(webgpu.endRenderPass(7, distantPass), true);
assert.equal(browser.host.passDescriptors[0].colorAttachments[0].loadOp, "clear");
assert.equal(browser.host.passDescriptors[0].depthStencilAttachment.depthLoadOp, "clear");
assert.equal(browser.host.passDescriptors[1].colorAttachments[0].loadOp, "load");
assert.equal(browser.host.passDescriptors[1].depthStencilAttachment.depthLoadOp, "clear");
assert.equal(browser.host.passDescriptors[1].depthStencilAttachment.depthStoreOp, "store");
const commandBuffer = webgpu.finishEncoder(7, encoder);
const submission = webgpu.submit(7, commandBuffer);
assert.equal(webgpu.pollSubmission(7, submission).status, "pending");
assert.equal(webgpu.presentCanvas(7, frame.textureHandle), true);
const captured = await capture;
assert.equal(captured.width, 1280); assert.equal(captured.height, 720);
assert.deepEqual([...captured.rgba.subarray(0, 4)], [30, 20, 10, 255]);
await Promise.resolve(); await Promise.resolve();
assert.equal(webgpu.pollSubmission(7, submission).status, "ready");
assert.equal(browser.host.draws, 2); assert.equal(browser.host.submits, 1);

// Material bind groups are built once per (pipeline, group, texture, secondary, sampler) and evicted when one of those
// handles is released: a repeated draw reuses its group, a new texture builds one, a released texture never resurfaces.
{
  const target = webgpu.createTexture(7, { size: { width:1280, height:720 }, format:"rgba16float", usage:20 });
  const encoder = webgpu.createCommandEncoder(7);
  const pass = webgpu.beginRenderPass(7, encoder, target);
  const draw = (textureHandle) => webgpu.recordIndexedDraw(7, pass, {
    pipelineHandle: pipeline, vertexBufferHandle: vertex, indexBufferHandle: index,
    textureHandle, samplerHandle: sampler, textureBindGroupSlot: 2,
    bindGroups: [{ index: 0, handle: bindGroup }], indexCount: 3, firstInstance: 7 });
  const second = webgpu.createTexture(7, { size: { width: 2, height: 2 }, format: "rgba8unorm", usage: 3 });
  const built = browser.host.bindGroupsCreated;
  assert.equal(draw(texture), true); assert.equal(draw(texture), true);
  assert.equal(browser.host.bindGroupsCreated, built);
  assert.equal(draw(second), true); assert.equal(draw(second), true);
  assert.equal(browser.host.bindGroupsCreated, built + 1);
  assert.equal(webgpu.release(7, second), true);
  const third = webgpu.createTexture(7, { size: { width: 2, height: 2 }, format: "rgba8unorm", usage: 3 });
  assert.equal(draw(third), true);
  assert.equal(browser.host.bindGroupsCreated, built + 2);
  assert.throws(() => draw(second), /unknown/);
  assert.equal(webgpu.endRenderPass(7, pass), true);
  browser.host.draws -= 5;
}

const timingEncoder = webgpu.createCommandEncoder(7);
// Owned HDR target: exact extent permits the same scene-depth authority;
// postprocess can omit depth. Cache one view and reject sampled-only targets.
{
  const hdr = webgpu.createTexture(7, { size: { width:1280, height:720 },
    format:"rgba16float", usage:20 });
  const small = webgpu.createTexture(7, { size: { width:640, height:360 },
    format:"rgba16float", usage:20 });
  const encoder = webgpu.createCommandEncoder(7);
  assert.throws(() => webgpu.beginRenderPass(7,encoder,texture), /render attachment/);
  assert.throws(() => webgpu.beginRenderPass(7,encoder,small), /extents differ/);
  const viewsBefore=browser.host.textureViews.length;
  let pass=webgpu.beginRenderPass(7,encoder,hdr);
  const firstView=browser.host.passDescriptors.at(-1).colorAttachments[0].view;
  assert.equal(webgpu.endRenderPass(7,pass),true);
  pass=webgpu.beginRenderPass(7,encoder,hdr,{depthEnabled:false,colorLoadOp:"load"});
  const desc=browser.host.passDescriptors.at(-1);
  assert.equal(desc.colorAttachments[0].view,firstView);
  assert.equal(desc.depthStencilAttachment,undefined);
  assert.equal(webgpu.endRenderPass(7,pass),true);
  // First pass creates color and shared-depth views; second reuses color.
  assert.equal(browser.host.textureViews.length,viewsBefore+2);
  pass=webgpu.beginRenderPass(7,encoder,small,{depthEnabled:false});
  assert.equal(webgpu.endRenderPass(7,pass),true);
  webgpu.release(7,webgpu.finishEncoder(7,encoder));
  webgpu.release(7,hdr); webgpu.release(7,small);
  const invalidEncoder=webgpu.createCommandEncoder(7);
  assert.throws(() => webgpu.beginRenderPass(7,invalidEncoder,hdr), /unknown/);
  webgpu.release(7,invalidEncoder);
}
// Cube capture selects one exact mip/layer. Cached views are distinct per
// subresource, and invalid selections cannot allocate a view or begin a pass.
{
  const cube = webgpu.createTexture(7, {size:{width:128,height:128,depthOrArrayLayers:6},
    mipLevelCount:3, format:"rgba16float",usage:20});
  const encoder = webgpu.createCommandEncoder(7), cached = [];
  const start = browser.host.textureViews.length;
  for (let cycle=0; cycle<2; ++cycle) for (let mip=0; mip<3; ++mip) for (let layer=0; layer<6; ++layer) {
    const pass = webgpu.beginRenderPass(7,encoder,cube,{depthEnabled:false,targetMipLevel:mip,targetArrayLayer:layer});
    const view = browser.host.passDescriptors.at(-1).colorAttachments[0].view;
    const index=mip*6+layer;
    if (!cycle) {
      assert.equal(cached.includes(view),false); cached[index]=view;
      assert.deepEqual(browser.host.textureViews.at(-1),{dimension:"2d",baseMipLevel:mip,mipLevelCount:1,baseArrayLayer:layer,arrayLayerCount:1});
    } else assert.equal(view,cached[index]);
    webgpu.endRenderPass(7,pass);
  }
  assert.equal(browser.host.textureViews.length,start+18);
  const count=browser.host.passDescriptors.length;
  for (const descriptor of [{targetMipLevel:3},{targetArrayLayer:6},{targetMipLevel:-1},
      {targetArrayLayer:0.5},{targetMipLevel:NaN},{targetArrayLayer:Infinity}])
    assert.throws(()=>webgpu.beginRenderPass(7,encoder,cube,{depthEnabled:false,...descriptor}),/subresource/);
  assert.equal(browser.host.textureViews.length,start+18);
  assert.equal(browser.host.passDescriptors.length,count);
  assert.throws(()=>webgpu.beginRenderPass(7,encoder,cube,{targetArrayLayer:5}),/extents differ/);
  webgpu.release(7,webgpu.finishEncoder(7,encoder)); webgpu.release(7,cube);
}
const computePass = webgpu.beginComputePass(7, timingEncoder,
  { timestampWrites: true });
assert.equal(webgpu.recordComputeDispatch(7, computePass, {
  pipelineHandle: pipeline, bindGroups: [{ index: 0, handle: bindGroup }],
  groupCountX: 1, groupCountY: 1, groupCountZ: 1
}), true);
assert.equal(webgpu.endComputePass(7, computePass), true);
const timingCommandBuffer = webgpu.finishEncoder(7, timingEncoder);
const timingSubmission = webgpu.submit(7, timingCommandBuffer);
assert.equal(webgpu.pollSubmission(7, timingSubmission).timingState, "pending");
await Promise.resolve(); await Promise.resolve(); await Promise.resolve();
const timingReceipt = webgpu.pollSubmission(7, timingSubmission);
assert.equal(timingReceipt.status, "ready");
assert.equal(timingReceipt.timingState, "valid");
assert.equal(timingReceipt.gpuDurationNanoseconds, 350000);

// Quantized timestamps can coincide. This is not a failed GPU submission
// and must not be reported as a fabricated positive timing sample.
for (const [end, expectedStatus, expectedTiming] of [
  [100n, "ready", "unavailable"],
  [99n, "failed", "failed"],
  [100n + BigInt(Number.MAX_SAFE_INTEGER) + 1n, "failed", "failed"]
]) {
  browser.host.timestampEnd = end;
  const encoder = webgpu.createCommandEncoder(7);
  const pass = webgpu.beginComputePass(7, encoder, { timestampWrites: true });
  webgpu.endComputePass(7, pass);
  const submitted = webgpu.submit(7, webgpu.finishEncoder(7, encoder));
  for (let i = 0; i < 8; ++i) await Promise.resolve();
  const receipt = webgpu.pollSubmission(7, submitted);
  assert.equal(receipt.status, expectedStatus);
  assert.equal(receipt.timingState, expectedTiming);
  assert.equal(receipt.gpuDurationNanoseconds, 0);
}

// Readback is read-only and owns staging even on mapping failure/device loss.
{
  const source=webgpu.createBuffer(7,{size:16,usage:4});
  const writes=browser.host.writes, destroyed=browser.host.destroyed;
  assert.equal((await webgpu.readBuffer(7,source,16)).byteLength,16);
  assert.equal(browser.host.writes,writes);
  assert.equal(browser.host.destroyed,destroyed+1);
  const created=browser.host.created;
  for(const size of [0,3,20,NaN])await assert.rejects(webgpu.readBuffer(7,source,size));
  await assert.rejects(webgpu.readBuffer(7,index,16)); // no COPY_SRC
  await assert.rejects(webgpu.readBuffer(8,source,16));
  assert.equal(browser.host.created,created);
  browser.host.mapError=new Error("map rejected");
  const unmaps=browser.host.unmaps;
  await assert.rejects(webgpu.readBuffer(7,source,16),/map rejected/);
  assert.equal(browser.host.destroyed,destroyed+2);
  assert.equal(browser.host.unmaps,unmaps);
  delete browser.host.mapError;
}
{
  const b=fakeBrowser(), h=createRalWebGpuBrowserHost({gpu:b.gpu,canvas:b.canvas});
  await h.initialize(17);
  const source=h.createBuffer(17,{size:16,usage:4}), gate=deferred();
  b.host.mapGate=gate.promise;
  const reading=h.readBuffer(17,source,16);
  b.lost.resolve({reason:"unknown",message:"during readback"});
  await Promise.resolve();await Promise.resolve();gate.resolve();
  await assert.rejects(reading,error=>error.code==="device-lost");
  assert.equal(b.host.destroyed,1);assert.equal(b.host.unmaps,1);
  h.destroy(17);
  assert.ok(b.host.destroyed>=b.host.created);
}

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
