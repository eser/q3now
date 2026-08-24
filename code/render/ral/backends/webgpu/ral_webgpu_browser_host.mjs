// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

export const RAL_WEBGPU_BROWSER_HOST_SCHEMA_VERSION = 1;

export class RalWebGpuBrowserHostError extends Error {
  constructor(code, message) {
    super(message);
    this.name = "RalWebGpuBrowserHostError";
    this.code = code;
  }
}

const UNSUPPORTED = "WebGPU is unavailable in this browser or execution context";
const ADAPTER_UNAVAILABLE = "No compatible WebGPU adapter is available";
const DEVICE_UNAVAILABLE = "The WebGPU device request failed";

function finitePositive(value, name) {
  if (!Number.isFinite(value) || value <= 0) {
    throw new RalWebGpuBrowserHostError("invalid-argument", `${name} must be finite and positive`);
  }
}

function normalizedLimits(limits = {}) {
  const keys = [
    "maxTextureDimension2D", "maxTextureDimension3D", "maxTextureArrayLayers",
    "maxBindGroups", "maxBindingsPerBindGroup", "maxSampledTexturesPerShaderStage",
    "maxStorageBufferBindingSize", "maxComputeInvocationsPerWorkgroup",
    "minUniformBufferOffsetAlignment", "minStorageBufferOffsetAlignment"
  ];
  return Object.freeze({ maxColorAttachments: Number(limits.maxColorAttachments ?? 4),
    ...Object.fromEntries(keys.map((key) => [key, Number(limits[key] ?? 0)])) });
}

function normalizedAdapterInfo(info = {}) {
  const text = (value) => typeof value === "string" ? value.slice(0, 256) : "";
  return Object.freeze({ vendor: text(info.vendor), architecture: text(info.architecture),
    device: text(info.device), description: text(info.description) });
}

export function createRalWebGpuBrowserHost({ gpu, canvas = null, devicePixelRatio = () => 1 } = {}) {
  let generation = 0;
  let nextHandle = 1;
  let status = "idle";
  let reason = "";
  let uncapturedError = "";
  let lostInfo = null;
  let limits = Object.freeze({});
  let adapterInfo = normalizedAdapterInfo();
  let canvasContext = null;
  let canvasFormat = null;
  let configured = null;
  let pendingCanvasCapture = null;
  const canvasFrames = new Map();
  const encoderCanvasFrames = new Map();
  const passEncoders = new Map();
  const encoderDrawCounts = new Map();
  const canvasCaptureCandidates = [];
  const entries = new Map();
  const order = [];

  const put = (kind, object, options = {}) => {
    if (!object) throw new RalWebGpuBrowserHostError("host-failure", `cannot register empty ${kind}`);
    const handle = nextHandle++;
    entries.set(handle, { kind, object, generation,
      metadata: options.metadata ?? null,
      destroyable: options.destroyable !== false });
    order.push(handle);
    return handle;
  };
  const get = (handle, kind, expectedGeneration = generation) => {
    if (expectedGeneration !== generation || generation === 0) {
      throw new RalWebGpuBrowserHostError("stale-generation", "WebGPU host generation is stale");
    }
    const entry = entries.get(handle);
    if (!entry || entry.generation !== generation || (kind && entry.kind !== kind)) {
      throw new RalWebGpuBrowserHostError("unknown-handle", `unknown ${kind ?? "object"} handle`);
    }
    return entry.object;
  };
  const release = (handle) => {
    const entry = entries.get(handle);
    if (!entry) return false;
    entries.delete(handle);
    if (entry.destroyable && typeof entry.object.destroy === "function") entry.object.destroy();
    return true;
  };
  const currentReceipt = () => Object.freeze({
    schemaVersion: RAL_WEBGPU_BROWSER_HOST_SCHEMA_VERSION,
    generation,
    status,
    reason,
    error: uncapturedError,
    adapterHandle: [...entries].find(([, value]) => value.kind === "adapter")?.[0] ?? 0,
    deviceHandle: [...entries].find(([, value]) => value.kind === "device")?.[0] ?? 0,
    queueHandle: [...entries].find(([, value]) => value.kind === "queue")?.[0] ?? 0,
    canvasFormat,
    adapterInfo,
    limits,
    lost: lostInfo ? Object.freeze({ ...lostInfo }) : null,
    ready: status === "ready"
  });
  const getDevice = (required = true) => {
    const entry = [...entries.values()].find((value) => value.kind === "device");
    if (!entry && required) throw new RalWebGpuBrowserHostError("not-ready", "WebGPU device is not ready");
    return entry?.object ?? null;
  };
  const getQueue = () => {
    const entry = [...entries.values()].find((value) => value.kind === "queue");
    if (!entry) throw new RalWebGpuBrowserHostError("not-ready", "WebGPU queue is not ready");
    return entry.object;
  };
  const assertReady = (expectedGeneration) => {
    if (expectedGeneration !== generation) get(0, null, expectedGeneration);
    if (status !== "ready" || lostInfo) {
      throw new RalWebGpuBrowserHostError(lostInfo ? "device-lost" : "not-ready",
        lostInfo?.message ?? "WebGPU host is not ready");
    }
  };

  return Object.freeze({
    async initialize(requestedGeneration, options = {}) {
      if (!Number.isSafeInteger(requestedGeneration) || requestedGeneration <= 0 || generation !== 0) {
        throw new RalWebGpuBrowserHostError("invalid-generation", "initial generation must be a positive safe integer");
      }
      generation = requestedGeneration;
      if (!gpu || typeof gpu.requestAdapter !== "function") {
        status = "unsupported"; reason = UNSUPPORTED; return currentReceipt();
      }
      status = "requesting-adapter";
      let adapter;
      try { adapter = await gpu.requestAdapter(options.adapter ?? {}); } catch { adapter = null; }
      if (!adapter) { status = "unsupported"; reason = ADAPTER_UNAVAILABLE; return currentReceipt(); }
      adapterInfo = normalizedAdapterInfo(adapter.info);
      put("adapter", adapter, { destroyable: false });
      status = "requesting-device";
      let device;
      try { device = await adapter.requestDevice(options.device ?? {}); } catch { device = null; }
      if (!device) { status = "failed"; reason = DEVICE_UNAVAILABLE; return currentReceipt(); }
      device.addEventListener?.("uncapturederror", (event) => {
        uncapturedError = String(event?.error?.message ?? event?.message ?? "WebGPU validation error");
      });
      limits = normalizedLimits(device.limits);
      put("device", device); put("queue", device.queue, { destroyable: false });
      canvasFormat = typeof gpu.getPreferredCanvasFormat === "function"
        ? gpu.getPreferredCanvasFormat() : "bgra8unorm";
      status = "ready"; reason = "";
      Promise.resolve(device.lost).then((info) => {
        if (status === "destroyed") return;
        lostInfo = { reason: String(info?.reason ?? "unknown"), message: String(info?.message ?? "WebGPU device lost") };
        status = "lost"; reason = lostInfo.message;
      });
      return currentReceipt();
    },
    receipt(expectedGeneration) {
      if (expectedGeneration !== generation || generation === 0) get(0, null, expectedGeneration);
      return currentReceipt();
    },
    createBuffer(expectedGeneration, descriptor) {
      assertReady(expectedGeneration); finitePositive(descriptor?.size, "buffer size");
      return put("buffer", getDevice().createBuffer({ ...descriptor }));
    },
    createTexture(expectedGeneration, descriptor) {
      assertReady(expectedGeneration);
      if (!descriptor?.size) throw new RalWebGpuBrowserHostError("invalid-argument", "texture size is required");
      return put("texture", getDevice().createTexture({ ...descriptor }),
        { metadata: { size: { ...descriptor.size } } });
    },
    createSampler(expectedGeneration, descriptor = {}) {
      assertReady(expectedGeneration); return put("sampler", getDevice().createSampler({ ...descriptor }));
    },
    createShaderModule(expectedGeneration, descriptor) {
      assertReady(expectedGeneration);
      if (typeof descriptor?.code !== "string" || descriptor.code.length === 0) {
        throw new RalWebGpuBrowserHostError("invalid-argument", "WGSL code is required");
      }
      return put("shader", getDevice().createShaderModule({ ...descriptor }));
    },
    createBindGroupLayout(expectedGeneration, descriptor) {
      assertReady(expectedGeneration);
      return put("bind-group-layout", getDevice().createBindGroupLayout(descriptor));
    },
    createPipelineLayout(expectedGeneration, layoutHandles) {
      assertReady(expectedGeneration);
      const bindGroupLayouts = layoutHandles.map((handle) => get(handle, "bind-group-layout"));
      return put("pipeline-layout", getDevice().createPipelineLayout({ bindGroupLayouts }));
    },
    createRenderPipelineFromHandles(expectedGeneration, descriptor) {
      assertReady(expectedGeneration);
      const converted = { ...descriptor, layout: get(descriptor.layoutHandle, "pipeline-layout"),
        vertex: { ...descriptor.vertex, module: get(descriptor.vertex.moduleHandle, "shader") },
        fragment: { ...descriptor.fragment, module: get(descriptor.fragment.moduleHandle, "shader") } };
      delete converted.layoutHandle; delete converted.vertex.moduleHandle;
      delete converted.fragment.moduleHandle;
      return put("pipeline", getDevice().createRenderPipeline(converted));
    },
    createComputePipelineFromHandles(expectedGeneration, descriptor) {
      assertReady(expectedGeneration);
      const converted = { ...descriptor, layout: get(descriptor.layoutHandle, "pipeline-layout"),
        compute: { ...descriptor.compute, module: get(descriptor.compute.moduleHandle, "shader") } };
      delete converted.layoutHandle; delete converted.compute.moduleHandle;
      return put("pipeline", getDevice().createComputePipeline(converted));
    },
    async createRenderPipeline(expectedGeneration, descriptor) {
      assertReady(expectedGeneration);
      const device = getDevice();
      const pipeline = typeof device.createRenderPipelineAsync === "function"
        ? await device.createRenderPipelineAsync(descriptor) : device.createRenderPipeline(descriptor);
      assertReady(expectedGeneration); return put("pipeline", pipeline);
    },
    createBindGroup(expectedGeneration, descriptor) {
      assertReady(expectedGeneration); return put("bind-group", getDevice().createBindGroup(descriptor));
    },
    writeBuffer(expectedGeneration, bufferHandle, offset, data) {
      assertReady(expectedGeneration); getQueue().writeBuffer(get(bufferHandle, "buffer"), offset, data); return true;
    },
    writeTexture(expectedGeneration, textureHandle, data, layout, size) {
      assertReady(expectedGeneration);
      const entry = entries.get(textureHandle);
      const texture = get(textureHandle, "texture");
      /* The browser ABI models exact whole-resource uploads and carries the
       * padded row layout, not the logical texture width. Keep the authoritative
       * extent with the host-owned texture instead of deriving width from the
       * 256-byte-aligned bytesPerRow (which turns a padded 1x1 upload into 64x1). */
      const copySize = entry?.metadata?.size ?? size;
      getQueue().writeTexture({ texture }, data, layout, copySize); return true;
    },
    async roundTrip(expectedGeneration, uploadHandle, readbackHandle, data) {
      assertReady(expectedGeneration);
      const upload = get(uploadHandle, "buffer"), readback = get(readbackHandle, "buffer");
      const queue = getQueue(); queue.writeBuffer(upload, 0, data);
      const encoder = getDevice().createCommandEncoder();
      encoder.copyBufferToBuffer(upload, 0, readback, 0, data.byteLength);
      queue.submit([encoder.finish()]); await readback.mapAsync(1, 0, data.byteLength);
      const result = new Uint8Array(readback.getMappedRange(0, data.byteLength)).slice();
      readback.unmap(); assertReady(expectedGeneration); return result;
    },
    configureCanvas(expectedGeneration, { cssWidth, cssHeight, dpr = devicePixelRatio(), format = canvasFormat,
      alphaMode = "opaque", colorSpace = "srgb" }) {
      assertReady(expectedGeneration);
      if (!canvas || typeof canvas.getContext !== "function") {
        throw new RalWebGpuBrowserHostError("canvas-unavailable", "WebGPU canvas is unavailable");
      }
      if (cssWidth === 0 || cssHeight === 0) {
        canvasContext?.unconfigure?.(); configured = null;
        return Object.freeze({ generation, suspended: true, cssWidth, cssHeight, pixelWidth: 0, pixelHeight: 0 });
      }
      finitePositive(cssWidth, "canvas width"); finitePositive(cssHeight, "canvas height"); finitePositive(dpr, "device pixel ratio");
      const pixelWidth = Math.round(cssWidth * dpr), pixelHeight = Math.round(cssHeight * dpr);
      if (pixelWidth <= 0 || pixelHeight <= 0 || pixelWidth > 16384 || pixelHeight > 16384) {
        throw new RalWebGpuBrowserHostError("capacity", "canvas drawing-buffer extent is unsupported");
      }
      canvasContext ??= canvas.getContext("webgpu");
      if (!canvasContext) throw new RalWebGpuBrowserHostError("canvas-unavailable", "WebGPU canvas context is unavailable");
      canvas.width = pixelWidth; canvas.height = pixelHeight;
      if (canvas.style) { canvas.style.width = `${cssWidth}px`; canvas.style.height = `${cssHeight}px`; }
      canvasContext.configure({ device: getDevice(), format, alphaMode, colorSpace,
        usage: (globalThis.GPUTextureUsage?.RENDER_ATTACHMENT ?? 16)
          | (globalThis.GPUTextureUsage?.COPY_SRC ?? 1) });
      configured = Object.freeze({ generation, suspended: false, cssWidth, cssHeight, dpr,
        pixelWidth, pixelHeight, format, alphaMode, colorSpace });
      return configured;
    },
    acquireCanvasTexture(expectedGeneration) {
      assertReady(expectedGeneration);
      if (!configured || !canvasContext) throw new RalWebGpuBrowserHostError("not-configured", "WebGPU canvas is not configured");
      const texture = canvasContext.getCurrentTexture();
      const textureHandle = put("canvas-texture", texture, { destroyable: false });
      const viewHandle = put("texture-view", texture.createView(), { destroyable: false });
      canvasFrames.set(textureHandle, viewHandle);
      return Object.freeze({ generation, textureHandle, viewHandle, ...configured });
    },
    unconfigureCanvas(expectedGeneration) {
      assertReady(expectedGeneration); canvasContext?.unconfigure?.(); configured = null; return true;
    },
    presentCanvas(expectedGeneration, textureHandle) {
      assertReady(expectedGeneration);
      const viewHandle = canvasFrames.get(textureHandle);
      if (!viewHandle) throw new RalWebGpuBrowserHostError("unknown-handle", "unknown canvas frame");
      const candidates = canvasCaptureCandidates.filter((candidate) =>
        candidate.textureHandle === textureHandle);
      if (pendingCanvasCapture && candidates.length > 0) {
        const request = pendingCanvasCapture;
        pendingCanvasCapture = null;
        const selected = candidates.at(-1);
        for (let index = canvasCaptureCandidates.length - 1; index >= 0; --index) {
          if (canvasCaptureCandidates[index].textureHandle === textureHandle)
            canvasCaptureCandidates.splice(index, 1);
        }
        Promise.resolve(selected.readback.mapAsync(globalThis.GPUMapMode?.READ ?? 1,
          0, selected.bytesPerRow * selected.height)).then(() => {
          const mapped = new Uint8Array(selected.readback.getMappedRange(0,
            selected.bytesPerRow * selected.height));
          const rgba = new Uint8Array(selected.width * selected.height * 4);
          for (let y = 0; y < selected.height; ++y) {
            const source = mapped.subarray(y * selected.bytesPerRow,
              y * selected.bytesPerRow + selected.width * 4);
            rgba.set(source, y * selected.width * 4);
          }
          if (selected.sourceFormat.startsWith("bgra")) {
            for (let offset = 0; offset < rgba.length; offset += 4) {
              const red = rgba[offset]; rgba[offset] = rgba[offset + 2]; rgba[offset + 2] = red;
            }
          }
          selected.readback.unmap(); selected.readback.destroy?.();
          Promise.resolve(getQueue().onSubmittedWorkDone()).then(() => {
            for (const candidate of candidates.slice(0, -1)) candidate.readback.destroy?.();
          });
          request.resolve(Object.freeze({ generation, width: selected.width,
            height: selected.height, format: "rgba8unorm", rgba,
            capturePasses: candidates.length }));
        }, (error) => {
          for (const candidate of candidates) candidate.readback.destroy?.();
          request.reject(new RalWebGpuBrowserHostError("capture-failed",
            String(error?.message ?? error ?? "WebGPU canvas capture failed")));
        });
      }
      release(viewHandle); release(textureHandle); canvasFrames.delete(textureHandle); return true;
    },
    captureNextCanvas(expectedGeneration) {
      assertReady(expectedGeneration);
      if (!configured) throw new RalWebGpuBrowserHostError("not-configured", "WebGPU canvas is not configured");
      if (pendingCanvasCapture) throw new RalWebGpuBrowserHostError("capture-pending", "WebGPU canvas capture is already pending");
      return new Promise((resolve, reject) => { pendingCanvasCapture = { resolve, reject }; });
    },
    createCommandEncoder(expectedGeneration, descriptor = {}) {
      assertReady(expectedGeneration); return put("encoder", getDevice().createCommandEncoder(descriptor));
    },
    beginRenderPass(expectedGeneration, encoderHandle, viewHandle, descriptor = {}) {
      assertReady(expectedGeneration);
      const canvasTextureHandle = canvasFrames.has(viewHandle) ? viewHandle
        : [...canvasFrames].find(([, candidateView]) => candidateView === viewHandle)?.[0];
      if (canvasTextureHandle) encoderCanvasFrames.set(encoderHandle, canvasTextureHandle);
      const resolvedView = canvasFrames.has(viewHandle)
        ? get(canvasFrames.get(viewHandle), "texture-view") : get(viewHandle, "texture-view");
      const colorAttachment = { view: resolvedView, loadOp: "clear", storeOp: "store",
        clearValue: descriptor.clearValue ?? { r: 0, g: 0, b: 0, a: 1 }, ...(descriptor.colorAttachment ?? {}) };
      const passHandle = put("render-pass", get(encoderHandle, "encoder").beginRenderPass({
        colorAttachments: [colorAttachment], ...(descriptor.pass ?? {})
      }), { destroyable: false });
      passEncoders.set(passHandle, encoderHandle);
      encoderDrawCounts.set(encoderHandle, 0);
      return passHandle;
    },
    beginComputePass(expectedGeneration, encoderHandle, descriptor = {}) {
      assertReady(expectedGeneration);
      return put("compute-pass", get(encoderHandle, "encoder").beginComputePass(descriptor),
        { destroyable: false });
    },
    recordIndexedDraw(expectedGeneration, passHandle, draw) {
      assertReady(expectedGeneration);
      const pass = get(passHandle, "render-pass");
      const pipeline = get(draw.pipelineHandle, "pipeline");
      const vertex = get(draw.vertexBufferHandle, "buffer");
      const index = get(draw.indexBufferHandle, "buffer");
      if (draw.textureHandle) get(draw.textureHandle, "texture");
      if (draw.secondaryTextureHandle) get(draw.secondaryTextureHandle, "texture");
      if (draw.samplerHandle) get(draw.samplerHandle, "sampler");
      finitePositive(draw.indexCount, "index count");
      pass.setPipeline(pipeline); pass.setVertexBuffer(draw.vertexSlot ?? 0, vertex, draw.vertexOffset ?? 0);
      pass.setIndexBuffer(index, draw.indexFormat ?? "uint32", draw.indexOffset ?? 0);
      for (const binding of draw.bindGroups ?? []) pass.setBindGroup(binding.index, get(binding.handle, "bind-group"), binding.offsets ?? []);
      pass.drawIndexed(draw.indexCount, draw.instanceCount ?? 1, draw.firstIndex ?? 0,
        draw.baseVertex ?? 0, draw.firstInstance ?? 0);
      const encoderHandle = passEncoders.get(passHandle);
      if (encoderHandle) encoderDrawCounts.set(encoderHandle,
        (encoderDrawCounts.get(encoderHandle) ?? 0) + 1);
      return true;
    },
    endRenderPass(expectedGeneration, passHandle) {
      assertReady(expectedGeneration); get(passHandle, "render-pass").end();
      passEncoders.delete(passHandle); return release(passHandle);
    },
    endComputePass(expectedGeneration, passHandle) {
      assertReady(expectedGeneration); get(passHandle, "compute-pass").end(); return release(passHandle);
    },
    finishEncoder(expectedGeneration, encoderHandle, descriptor = {}) {
      assertReady(expectedGeneration);
      const encoder = get(encoderHandle, "encoder");
      let canvasCapture = null;
      const textureHandle = encoderCanvasFrames.get(encoderHandle);
      if (pendingCanvasCapture && textureHandle && configured
          && (encoderDrawCounts.get(encoderHandle) ?? 0) > 0) {
        const width = configured.pixelWidth, height = configured.pixelHeight;
        const bytesPerRow = Math.ceil(width * 4 / 256) * 256;
        const readback = getDevice().createBuffer({ size: bytesPerRow * height,
          usage: (globalThis.GPUBufferUsage?.MAP_READ ?? 1)
            | (globalThis.GPUBufferUsage?.COPY_DST ?? 8) });
        encoder.copyTextureToBuffer({ texture: get(textureHandle, "canvas-texture") },
          { buffer: readback, bytesPerRow, rowsPerImage: height },
          { width, height, depthOrArrayLayers: 1 });
        canvasCapture = { textureHandle, readback, bytesPerRow, width, height,
          sourceFormat: configured.format };
      }
      const commandBuffer = encoder.finish(descriptor);
      encoderCanvasFrames.delete(encoderHandle); encoderDrawCounts.delete(encoderHandle);
      release(encoderHandle);
      return put("command-buffer", commandBuffer, { destroyable: false,
        metadata: canvasCapture ? { canvasCapture } : null });
    },
    submit(expectedGeneration, commandBufferHandle) {
      assertReady(expectedGeneration);
      const queue = getQueue(), commandBuffer = get(commandBufferHandle, "command-buffer");
      const canvasCapture = entries.get(commandBufferHandle)?.metadata?.canvasCapture;
      queue.submit([commandBuffer]); release(commandBufferHandle);
      if (canvasCapture) canvasCaptureCandidates.push(canvasCapture);
      const submission = { status: "pending", error: "" };
      Promise.resolve(queue.onSubmittedWorkDone()).then(() => { submission.status = "ready"; }, (error) => {
        submission.status = "failed"; submission.error = String(error?.message ?? error ?? uncapturedError);
      });
      return put("submission", submission, { destroyable: false });
    },
    pollSubmission(expectedGeneration, submissionHandle) {
      assertReady(expectedGeneration); return Object.freeze({ ...get(submissionHandle, "submission") });
    },
    release(expectedGeneration, handle) { assertReady(expectedGeneration); return release(handle); },
    destroy(expectedGeneration) {
      if (expectedGeneration !== generation || generation === 0) get(0, null, expectedGeneration);
      pendingCanvasCapture?.reject(new RalWebGpuBrowserHostError("destroyed", "WebGPU host was destroyed during capture"));
      pendingCanvasCapture = null;
      canvasContext?.unconfigure?.(); configured = null;
      canvasFrames.clear();
      encoderCanvasFrames.clear();
      passEncoders.clear(); encoderDrawCounts.clear();
      for (const candidate of canvasCaptureCandidates) candidate.readback.destroy?.();
      canvasCaptureCandidates.length = 0;
      for (let i = order.length - 1; i >= 0; --i) release(order[i]);
      entries.clear(); order.length = 0; limits = Object.freeze({}); uncapturedError = "";
      status = "destroyed"; reason = ""; generation = 0;
      return true;
    }
  });
}
