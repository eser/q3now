// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

export const RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION = 1;

export const RalWebGpuBrowserOpcode = Object.freeze({
  BEGIN_ADAPTER: 1, POLL_ADAPTER: 2, BEGIN_DEVICE: 3, POLL_DEVICE: 4,
  RELEASE_DEVICE: 5, RELEASE_ADAPTER: 6, POLL_DEVICE_LOSS: 7,
  CANVAS_CONFIGURE: 8, CANVAS_UNCONFIGURE: 9, CANVAS_ACQUIRE: 10,
  CANVAS_PRESENT: 11, CREATE_BUFFER: 12, CREATE_TEXTURE: 13,
  CREATE_SAMPLER: 14, DESTROY_RESOURCE: 15, WRITE_BUFFER: 16,
  WRITE_TEXTURE: 17, BEGIN_ROUND_TRIP: 18, POLL_ROUND_TRIP: 19,
  RELEASE_OPERATION: 20, CREATE_SHADER_MODULE: 21,
  CREATE_BIND_GROUP_LAYOUT: 22, CREATE_PIPELINE_LAYOUT: 23,
  CREATE_PIPELINE: 24, DESTROY_PIPELINE_OBJECT: 25, BEGIN_ENCODER: 26,
  BEGIN_PASS: 27, RECORD_INDEXED_DRAW: 28, END_PASS: 29,
  FINISH_ENCODER: 30, SUBMIT: 31, POLL_SUBMISSION: 32,
  RELEASE_COMMAND_OBJECT: 33
});

const SIZE = Object.freeze({
  1: 24, 2: 24, 3: 32, 4: 24, 5: 32, 6: 24, 7: 24,
  8: 56, 9: 24, 10: 32, 11: 48, 12: 40, 13: 40, 14: 40,
  15: 32, 16: 56, 17: 56, 18: 72, 19: 48, 20: 24, 21: 120,
  22: 40, 23: 40, 24: 1352, 25: 32, 26: 24, 27: 40,
  28: 104, 29: 24, 30: 24, 31: 40, 32: 32, 33: 32
});
const RESPONSE_SIZE = Object.freeze({ 2: 376, 4: 40, 7: 224, 10: 32,
  12: 32, 13: 32, 14: 32, 18: 32, 19: 32, 21: 32, 22: 32,
  23: 32, 24: 32, 26: 32, 27: 32, 30: 32, 31: 32, 32: 32 });
const encoder = new TextEncoder(), decoder = new TextDecoder("utf-8", { fatal: true });
const u64 = (view, offset) => {
  const value = view.getBigUint64(offset, true);
  if (value > BigInt(Number.MAX_SAFE_INTEGER)) throw new RangeError("u64 exceeds JS safe integer");
  return Number(value);
};
const put64 = (view, offset, value) => view.setBigUint64(offset, BigInt(value), true);
const bool = (value) => { if (value !== 0 && value !== 1) throw new RangeError("invalid boolean"); return value === 1; };
const oneOf = (value, values, name) => {
  const result = values[value]; if (result === undefined) throw new RangeError(`invalid ${name}`); return result;
};
const FORMATS = { 1: "r8unorm", 2: "rg8unorm", 3: "rgba8unorm", 4: "rgba8unorm-srgb",
  5: "bgra8unorm", 6: "bgra8unorm-srgb", 12: "r16float", 13: "rg16float",
  14: "rgba16float", 17: "r32float", 18: "rg32float", 19: "rgb32float",
  20: "rgba32float", 21: "rgba8uint", 22: "depth16unorm", 23: "depth24plus-stencil8",
  24: "depth32float", 25: "depth32float-stencil8", 28: "bc1-rgba-unorm",
  29: "bc1-rgba-unorm-srgb", 30: "bc3-rgba-unorm", 31: "bc3-rgba-unorm-srgb",
  32: "bc4-r-unorm", 33: "bc5-rg-unorm", 34: "bc6h-rgb-ufloat",
  35: "bc7-rgba-unorm", 36: "bc7-rgba-unorm-srgb", 37: "astc-4x4-unorm",
  38: "astc-4x4-unorm-srgb", 39: "etc2-rgba8unorm", 40: "etc2-rgba8unorm-srgb" };
const VERTEX_FORMATS = { 1: "unorm8", 2: "unorm8x2", 3: "unorm8x4", 12: "float16",
  13: "float16x2", 14: "float16x4", 17: "float32", 18: "float32x2",
  19: "float32x3", 20: "float32x4", 21: "uint8x4" };
const COMPARE = ["never", "less", "equal", "less-equal", "greater", "not-equal", "greater-equal", "always"];
const STENCIL = ["keep", "zero", "replace", "increment-clamp", "decrement-clamp", "invert", "increment-wrap", "decrement-wrap"];
const BLEND_FACTOR = ["zero", "one", "src", "one-minus-src", "dst", "one-minus-dst", "src-alpha", "one-minus-src-alpha", "dst-alpha", "one-minus-dst-alpha", "src-alpha-saturated"];
const BLEND_OP = ["add", "subtract", "reverse-subtract", "min", "max"];
const TOPOLOGY = ["point-list", "line-list", "line-strip", "triangle-list", "triangle-strip"];
const CULL = ["none", "front", "back"], FRONT = ["ccw", "cw"];
const VIEW = { 1: "2d", 2: "2d-array", 3: "cube", 4: "cube-array", 5: "3d" };
const SAMPLE = { 1: "float", 2: "unfilterable-float", 3: "depth", 4: "sint", 5: "uint" };

export function createRalWebGpuBrowserDispatch({ host, memory, canvasIdentity = 1,
    bufferUsage = globalThis.GPUBufferUsage, textureUsage = globalThis.GPUTextureUsage } = {}) {
  if (!host || !memory || !Number.isSafeInteger(canvasIdentity) || canvasIdentity <= 0) {
    throw new TypeError("host, memory and a positive canvasIdentity are required");
  }
  const bufferFlags = bufferUsage ?? { MAP_READ: 1, COPY_SRC: 4, COPY_DST: 8,
    INDEX: 16, VERTEX: 32, UNIFORM: 64, STORAGE: 128 };
  const textureFlags = textureUsage ?? { COPY_SRC: 1, COPY_DST: 2, TEXTURE_BINDING: 4, RENDER_ATTACHMENT: 16 };
  let init = null, nextOperation = 0x100000, lastError = "", lastFailure = "";
  const operations = new Map(), submissions = new Map(), frames = new Map();
  const buffer = () => typeof memory === "function" ? memory() : memory.buffer;
  const region = (offset, bytes) => {
    const data = buffer();
    if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(bytes) || offset < 0 || bytes < 0
        || offset + bytes > data.byteLength) throw new RangeError("linear-memory bounds");
    return new DataView(data, offset, bytes);
  };
  const bytesAt = (offset, bytes) => new Uint8Array(buffer(), offset, bytes);
  const stringAt = (offset, bytes) => decoder.decode(bytesAt(offset, bytes));
  const cString = (view, offset, capacity) => {
    const data = new Uint8Array(view.buffer, view.byteOffset + offset, capacity);
    const end = data.indexOf(0); if (end < 0) throw new RangeError("unterminated string");
    return decoder.decode(data.subarray(0, end));
  };
  const receipt = () => { if (!init?.generation) throw new Error("not initialized"); return host.receipt(init.generation); };
  const exact = (actual, expected, name) => { if (actual !== expected) throw new RangeError(`stale ${name}`); };
  const response = (opcode, size) => {
    const data = new ArrayBuffer(size), view = new DataView(data);
    view.setUint32(0, 1, true); view.setUint32(4, opcode, true);
    view.setUint32(8, size, true); return { data: new Uint8Array(data), view };
  };
  const status = (opcode, accepted = true) => { const out = response(opcode, 24); out.view.setUint32(16, accepted ? 1 : 0, true); return out; };
  const identity = (opcode, value) => { const out = response(opcode, 32); out.view.setUint32(16, 1, true); put64(out.view, 24, value); return out; };
  const writeString = (view, offset, capacity, text) => {
    const data = encoder.encode(String(text ?? "")); if (data.length >= capacity) throw new RangeError("string capacity");
    new Uint8Array(view.buffer, offset, capacity).set(data);
  };
  const validateDevice = (view, offset = 16) => exact(u64(view, offset), receipt().deviceHandle, "device");
  const validateQueue = (view, offset = 16) => exact(u64(view, offset), receipt().queueHandle, "queue");
  const format = (value) => oneOf(value, FORMATS, "format");
  const bufferUsageValue = (usage) => {
    let out = 0; if (usage & 1) out |= bufferFlags.COPY_SRC; if (usage & 2) out |= bufferFlags.COPY_DST;
    if (usage & 4) out |= bufferFlags.VERTEX; if (usage & 8) out |= bufferFlags.INDEX;
    if (usage & 16) out |= bufferFlags.UNIFORM; if (usage & 32) out |= bufferFlags.STORAGE;
    if (usage & 64) out |= bufferFlags.MAP_READ; if (!out || (usage & ~127)) throw new RangeError("buffer usage"); return out;
  };
  const bindingEntry = (view, offset) => {
    const binding = view.getUint32(offset, true), kind = view.getUint32(offset + 4, true);
    const count = view.getUint32(offset + 8, true), visibility = view.getUint32(offset + 12, true);
    if (!count || !visibility || (visibility & ~7)) throw new RangeError("binding domain");
    const entry = { binding, visibility }; if (count > 1) entry.count = count;
    const minBindingSize = u64(view, offset + 16), dynamic = bool(view.getUint32(offset + 40, true));
    if (kind <= 3) entry.buffer = { type: [null, "uniform", "read-only-storage", "storage"][kind],
      hasDynamicOffset: dynamic, minBindingSize };
    else if (kind === 4) entry.texture = { viewDimension: oneOf(view.getUint32(offset + 24, true), VIEW, "view dimension"),
      sampleType: oneOf(view.getUint32(offset + 28, true), SAMPLE, "sample type") };
    else if (kind === 5 || kind === 6) entry.storageTexture = { access: kind === 5 ? "read-only" : "write-only",
      format: format(view.getUint32(offset + 32, true)), viewDimension: oneOf(view.getUint32(offset + 24, true), VIEW, "view dimension") };
    else if (kind === 7 || kind === 8) entry.sampler = { type: kind === 7 ? "filtering" : "comparison" };
    else throw new RangeError("binding class"); return entry;
  };
  const pipelineDescriptor = (view) => {
    validateDevice(view); const layoutHandle = u64(view, 24), kind = view.getUint32(112, true);
    const modules = view.getUint32(116, true), layouts = view.getUint32(120, true);
    if ((kind === 1 && modules !== 2) || (kind === 2 && modules !== 1) || layouts > 8) throw new RangeError("pipeline cardinality");
    const moduleHandles = Array.from({ length: modules }, (_, i) => u64(view, 32 + i * 8));
    const constants = {}; const specCount = view.getUint32(292, true); if (specCount > 32) throw new RangeError("spec constants");
    for (let i = 0; i < specCount; ++i) constants[view.getUint32(1096 + i * 8, true)] = view.getUint32(1100 + i * 8, true);
    if (kind === 2) return { kind, descriptor: { layoutHandle, compute: { moduleHandle: moduleHandles[0], entryPoint: "main", constants } } };
    const bindingCount = view.getUint32(280, true), attributeCount = view.getUint32(284, true);
    const blendCount = view.getUint32(288, true), colorCount = view.getUint32(268, true);
    if (bindingCount > 16 || attributeCount > 16 || blendCount > 8 || colorCount > 8 || blendCount !== colorCount) throw new RangeError("graphics cardinality");
    const attributesByBinding = new Map();
    for (let i = 0; i < attributeCount; ++i) { const at = 552 + i * 16, binding = view.getUint32(at + 4, true);
      const list = attributesByBinding.get(binding) ?? []; list.push({ shaderLocation: view.getUint32(at, true),
        format: oneOf(view.getUint32(at + 8, true), VERTEX_FORMATS, "vertex format"), offset: view.getUint32(at + 12, true) });
      attributesByBinding.set(binding, list); }
    const buffers = [];
    for (let i = 0; i < bindingCount; ++i) { const at = 296 + i * 16, binding = view.getUint32(at, true);
      buffers[binding] = { arrayStride: view.getUint32(at + 4, true), stepMode: view.getUint32(at + 8, true) ? "instance" : "vertex",
        attributes: attributesByBinding.get(binding) ?? [] }; }
    const targets = [];
    for (let i = 0; i < colorCount; ++i) { const at = 808 + i * 36, enabled = bool(view.getUint32(at, true));
      const target = { format: format(view.getUint32(236 + i * 4, true)), writeMask: view.getUint32(at + 28, true) };
      if (enabled) target.blend = { color: { srcFactor: oneOf(view.getUint32(at + 4, true), BLEND_FACTOR, "blend factor"),
        dstFactor: oneOf(view.getUint32(at + 8, true), BLEND_FACTOR, "blend factor"), operation: oneOf(view.getUint32(at + 12, true), BLEND_OP, "blend op") },
        alpha: { srcFactor: oneOf(view.getUint32(at + 16, true), BLEND_FACTOR, "blend factor"),
          dstFactor: oneOf(view.getUint32(at + 20, true), BLEND_FACTOR, "blend factor"), operation: oneOf(view.getUint32(at + 24, true), BLEND_OP, "blend op") } };
      targets.push(target); }
    if (view.getUint32(128, true) !== 0 || bool(view.getUint32(156, true))
        || view.getFloat32(160, true) !== 1) throw new RangeError("unsupported WebGPU raster state");
    bool(view.getUint32(140, true)); bool(view.getUint32(164, true));
    bool(view.getUint32(168, true)); bool(view.getUint32(176, true));
    const descriptor = { layoutHandle, vertex: { moduleHandle: moduleHandles[0], entryPoint: "main", buffers, constants },
      fragment: { moduleHandle: moduleHandles[1], entryPoint: "main", targets, constants },
      primitive: { topology: oneOf(view.getUint32(124, true), TOPOLOGY, "topology"),
        cullMode: oneOf(view.getUint32(132, true), CULL, "cull mode"), frontFace: oneOf(view.getUint32(136, true), FRONT, "front face") },
      multisample: { count: view.getUint32(276, true) } };
    const depthFormat = view.getUint32(272, true);
    if (depthFormat) descriptor.depthStencil = { format: format(depthFormat), depthWriteEnabled: bool(view.getUint32(168, true)),
      depthCompare: oneOf(view.getUint32(172, true), COMPARE, "compare op") };
    return { kind, descriptor };
  };

  const handlers = {
    1(view) { const generation = u64(view, 16); if (init) throw new RangeError("adapter request already begun");
      init = { generation, status: 1, result: null }; Promise.resolve(host.initialize(generation, {})).then(
        (value) => { init.result = value;
          init.status = value.ready ? 2 : value.status === "unsupported" ? 3 : 4; },
        () => { init.status = 4; }); return status(1); },
    2(view) { exact(u64(view, 16), init?.generation, "generation"); const out = response(2, 376); out.view.setUint32(16, init.status, true);
      if (init.result) { put64(out.view, 24, init.result.adapterHandle); out.view.setUint32(20, 2, true);
        const limits = init.result.limits ?? {}; out.view.setUint32(40, limits.maxColorAttachments ?? 0, true);
        out.view.setUint32(44, limits.maxTextureDimension2D ?? 0, true); out.view.setUint32(48, limits.maxTextureDimension3D ?? 0, true);
        out.view.setUint32(52, limits.maxTextureArrayLayers ?? 0, true); out.view.setUint32(56, limits.maxComputeInvocationsPerWorkgroup ?? 0, true);
        out.view.setUint32(60, limits.maxSampledTexturesPerShaderStage ?? 0, true); out.view.setUint32(64, limits.maxBindGroups ?? 0, true);
        out.view.setUint32(68, limits.maxBindingsPerBindGroup ?? 0, true); put64(out.view, 72, limits.maxStorageBufferBindingSize ?? 0);
        put64(out.view, 80, limits.minUniformBufferOffsetAlignment ?? 0); put64(out.view, 88, limits.minStorageBufferOffsetAlignment ?? 0);
        writeString(out.view, 120, 128, "Browser WebGPU"); writeString(out.view, 248, 128, "WebGPU adapter"); }
      return out; },
    3(view) { exact(u64(view, 24), init?.generation + 1, "generation"); exact(u64(view, 16), init?.result?.adapterHandle, "adapter"); return status(3); },
    4(view) { exact(u64(view, 16), init?.generation + 1, "generation"); const out = response(4, 40); out.view.setUint32(16, init.status, true);
      if (init.result?.ready) { put64(out.view, 24, init.result.deviceHandle); put64(out.view, 32, init.result.queueHandle); } return out; },
    5(view) { const r = receipt(); exact(u64(view, 16), r.deviceHandle, "device"); exact(u64(view, 24), r.queueHandle, "queue"); return status(5); },
    6(view) { exact(u64(view, 16), receipt().adapterHandle, "adapter"); host.destroy(init.generation); init = null; return status(6); },
    7(view) { exact(u64(view, 16), init?.generation, "generation"); const r = receipt(), out = response(7, 224); put64(out.view, 16, r.deviceHandle);
      out.view.setUint32(24, r.lost ? 1 : 0, true); out.view.setUint32(28, r.lost ? 1 : 0, true); writeString(out.view, 32, 192, r.lost?.message ?? ""); return out; },
    8(view) { exact(u64(view, 16), canvasIdentity, "canvas"); validateDevice(view, 24); if (view.getUint32(44, true) !== 0) throw new RangeError("unsupported canvas color space"); const configured = host.configureCanvas(init.generation,
      { cssWidth: view.getUint32(32, true), cssHeight: view.getUint32(36, true), dpr: 1,
        format: format(view.getUint32(40, true)), colorSpace: view.getUint32(44, true) === 0 ? "srgb" : "display-p3",
        alphaMode: bool(view.getUint32(48, true)) ? "opaque" : "premultiplied" }); return status(8, configured.suspended !== undefined); },
    9(view) { exact(u64(view, 16), canvasIdentity, "canvas"); host.unconfigureCanvas(init.generation); return status(9); },
    10(view) { exact(u64(view, 16), canvasIdentity, "canvas"); const generation = u64(view, 24), frame = host.acquireCanvasTexture(init.generation);
      frames.set(frame.textureHandle, generation); const out = response(10, 32); out.view.setUint32(16, 1, true); put64(out.view, 24, frame.textureHandle); return out; },
    11(view) { exact(u64(view, 16), canvasIdentity, "canvas"); const texture = u64(view, 24); exact(u64(view, 32), frames.get(texture), "frame");
      if (!submissions.has(u64(view, 40))) throw new RangeError("unknown submission generation"); host.presentCanvas(init.generation, texture); frames.delete(texture); return status(11); },
    12(view) { validateDevice(view); return identity(12, host.createBuffer(init.generation, { size: u64(view, 24), usage: bufferUsageValue(view.getUint32(32, true)) })); },
    13(view) { validateDevice(view); const width = view.getUint32(24, true), height = view.getUint32(28, true), depth = view.getUint32(32, true);
      if (!width || !height || !depth || view.getUint32(36, true) !== 4) throw new RangeError("texture extent"); return identity(13, host.createTexture(init.generation,
        { size: { width, height, depthOrArrayLayers: depth }, format: "rgba8unorm", usage: textureFlags.COPY_SRC | textureFlags.COPY_DST | textureFlags.TEXTURE_BINDING })); },
    14(view) { validateDevice(view); return identity(14, host.createSampler(init.generation, { minFilter: bool(view.getUint32(24, true)) ? "linear" : "nearest",
      magFilter: bool(view.getUint32(28, true)) ? "linear" : "nearest", addressModeU: bool(view.getUint32(32, true)) ? "clamp-to-edge" : "repeat",
      addressModeV: bool(view.getUint32(32, true)) ? "clamp-to-edge" : "repeat" })); },
    15(view) { const kind = view.getUint32(16, true); if (kind < 1 || kind > 3) throw new RangeError("resource kind"); host.release(init.generation, u64(view, 24)); return status(15); },
    16(view) { validateQueue(view); host.writeBuffer(init.generation, u64(view, 24), u64(view, 32), bytesAt(u64(view, 40), u64(view, 48)).slice()); return status(16); },
    17(view) { validateQueue(view); const size = u64(view, 40), rows = view.getUint32(52, true), bpr = view.getUint32(48, true);
      if (!size || !rows || !bpr || bpr % 256 || size !== bpr * rows) throw new RangeError("texture write layout");
      host.writeTexture(init.generation, u64(view, 24), bytesAt(u64(view, 32), size).slice(), { bytesPerRow: bpr, rowsPerImage: rows },
        { width: bpr / 4, height: rows, depthOrArrayLayers: 1 }); return status(17); },
    18(view) { validateDevice(view); validateQueue(view, 24); const generation = u64(view, 64), id = nextOperation++;
      const operation = { generation, status: 1, data: null }; operations.set(id, operation);
      Promise.resolve(host.roundTrip(init.generation, u64(view, 32), u64(view, 40), bytesAt(u64(view, 48), u64(view, 56)).slice())).then(
        (data) => { operation.data = new Uint8Array(data); operation.status = 2; }, () => { operation.status = 3; }); return identity(18, id); },
    19(view) { const id = u64(view, 16), operation = operations.get(id); if (!operation) throw new RangeError("unknown operation");
      exact(u64(view, 24), operation.generation, "operation"); const out = response(19, 32); out.view.setUint32(16, operation.status, true);
      if (operation.status === 2) { if (operation.data.length > u64(view, 40)) throw new RangeError("readback capacity"); bytesAt(u64(view, 32), operation.data.length).set(operation.data);
        put64(out.view, 24, operation.data.length); } return out; },
    20(view) { operations.delete(u64(view, 16)); return status(20); },
    21(view) { validateDevice(view); const code = stringAt(u64(view, 24), view.getUint32(52, true)); const entryPoint = cString(view, 56, 64);
      if (![1,2,4].includes(view.getUint32(48, true)) || entryPoint !== "main") throw new RangeError("shader module domain");
      return identity(21, host.createShaderModule(init.generation, { code, label: entryPoint })); },
    22(view) { validateDevice(view); const count = view.getUint32(36, true); if (count > 64) throw new RangeError("binding count");
      const offset = u64(view, 24), entries = Array.from({ length: count }, (_, i) => bindingEntry(region(offset + i * 44, 44), 0));
      return identity(22, host.createBindGroupLayout(init.generation, { entries })); },
    23(view) { validateDevice(view); const count = view.getUint32(32, true); if (count > 8) throw new RangeError("layout count");
      const offset = u64(view, 24), layouts = Array.from({ length: count }, (_, i) => u64(region(offset + i * 8, 8), 0));
      return identity(23, host.createPipelineLayout(init.generation, layouts)); },
    24(view) { const { kind, descriptor } = pipelineDescriptor(view); const value = kind === 1
      ? host.createRenderPipelineFromHandles(init.generation, descriptor) : host.createComputePipelineFromHandles(init.generation, descriptor);
      return identity(24, value); },
    25(view) { const kind = view.getUint32(16, true); if (kind < 1 || kind > 4) throw new RangeError("pipeline object kind"); host.release(init.generation, u64(view, 24)); return status(25); },
    26(view) { validateDevice(view); return identity(26, host.createCommandEncoder(init.generation)); },
    27(view) { const encoderHandle = u64(view, 16), target = u64(view, 24), kind = view.getUint32(32, true);
      if (kind === 1) return identity(27, host.beginRenderPass(init.generation, encoderHandle, target));
      if (kind === 2) return identity(27, host.beginComputePass(init.generation, encoderHandle)); throw new RangeError("pass kind"); },
    28(view) { const textured = bool(view.getUint32(84, true)), drawKind = view.getUint32(80, true);
      const contentDigest = view.getBigUint64(72, true);
      if (drawKind < 1 || drawKind > 4 || contentDigest === 0n) throw new RangeError("draw domain"); host.recordIndexedDraw(init.generation, u64(view, 16), {
      pipelineHandle: u64(view, 24), vertexBufferHandle: u64(view, 32), indexBufferHandle: u64(view, 40),
      textureHandle: textured ? u64(view, 48) : 0, secondaryTextureHandle: u64(view, 56), samplerHandle: textured ? u64(view, 64) : 0,
      firstIndex: view.getUint32(88, true), indexCount: view.getUint32(92, true), instanceCount: view.getUint32(96, true), indexFormat: "uint32" }); return status(28); },
    29(view) { const id = u64(view, 16); try { host.endRenderPass(init.generation, id); } catch { host.endComputePass(init.generation, id); } return status(29); },
    30(view) { return identity(30, host.finishEncoder(init.generation, u64(view, 16))); },
    31(view) { validateQueue(view); const generation = u64(view, 32), id = host.submit(init.generation, u64(view, 24)); submissions.set(generation, id); return identity(31, id); },
    32(view) { const id = u64(view, 16), generation = u64(view, 24); exact(submissions.get(generation), id, "submission");
      const state = host.pollSubmission(init.generation, id);
      if (state.status === "failed" && state.error) lastFailure = state.error;
      const out = response(32, 32); out.view.setUint32(16, state.status === "pending" ? 1 : state.status === "ready" ? 2 : 3, true); return out; },
    33(view) { const kind = view.getUint32(16, true); if (kind < 1 || kind > 4) throw new RangeError("command object kind"); host.release(init.generation, u64(view, 24)); return status(33); }
  };

  return Object.freeze({
    lastError() { return lastError; },
    lastFailure() { return lastFailure; },
    dispatch(opcode, requestOffset, requestBytes, responseOffset, responseBytes) {
      try {
        if (!Number.isInteger(opcode) || !handlers[opcode] || requestBytes !== SIZE[opcode]) return 0;
        const expectedResponse = RESPONSE_SIZE[opcode] ?? 24; if (responseBytes !== expectedResponse) return 0;
        const request = region(requestOffset, requestBytes);
        if (request.getUint32(0, true) !== 1 || request.getUint32(4, true) !== opcode
            || request.getUint32(8, true) !== requestBytes || request.getUint32(12, true) !== 0) return 0;
        region(responseOffset, responseBytes);
        const out = handlers[opcode](request);
        if (out.data.byteLength !== responseBytes) return 0;
        bytesAt(responseOffset, responseBytes).set(out.data); lastError = ""; return 1;
      } catch (error) { lastError = String(error?.message ?? error);
        lastFailure = lastError; return 0; }
    }
  });
}

export function installRalWebGpuBrowserDispatch(options) {
  const bridge = createRalWebGpuBrowserDispatch(options);
  const previous = globalThis.wiredRalWebGpuDispatch;
  globalThis.wiredRalWebGpuDispatch = bridge.dispatch;
  return Object.freeze({ bridge, uninstall() {
    if (globalThis.wiredRalWebGpuDispatch === bridge.dispatch) {
      if (previous === undefined) delete globalThis.wiredRalWebGpuDispatch;
      else globalThis.wiredRalWebGpuDispatch = previous;
    }
  } });
}
