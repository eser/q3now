// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import { createRalWebGpuLensOracle } from "./ral_webgpu_lens.mjs";

export const RAL_WEBGPU_BROWSER_HOST_SCHEMA_VERSION = 2;

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

function normalizedLimits(limits = {}, features = null) {
  const keys = [
    "maxTextureDimension2D", "maxTextureDimension3D", "maxTextureArrayLayers",
    "maxBindGroups", "maxBindingsPerBindGroup", "maxSampledTexturesPerShaderStage",
    "maxStorageBufferBindingSize", "maxComputeInvocationsPerWorkgroup",
    "minUniformBufferOffsetAlignment", "minStorageBufferOffsetAlignment"
  ];
  return Object.freeze({ maxColorAttachments: Number(limits.maxColorAttachments ?? 4),
    timestampQueries: features?.has?.("timestamp-query") === true,
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
  let canvasDepthTexture = null;
  let lensOracle = null;
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
      const deviceOptions = { ...(options.device ?? {}) };
      if ( adapter.features?.has?.("timestamp-query") === true ) {
        const requiredFeatures = new Set(deviceOptions.requiredFeatures ?? []);
        requiredFeatures.add("timestamp-query");
        deviceOptions.requiredFeatures = [...requiredFeatures];
      }
      let device;
      try { device = await adapter.requestDevice(deviceOptions); } catch { device = null; }
      if (!device) { status = "failed"; reason = DEVICE_UNAVAILABLE; return currentReceipt(); }
      device.addEventListener?.("uncapturederror", (event) => {
        const message = String(event?.error?.message ?? event?.message ?? "WebGPU validation error");
        if (!uncapturedError) uncapturedError = message;
        else if (!uncapturedError.includes(message))
          uncapturedError = `${uncapturedError}\n${message}`.slice(0, 2048);
      });
      limits = normalizedLimits(device.limits, device.features);
      put("device", device); put("queue", device.queue, { destroyable: false });
      canvasFormat = typeof gpu.getPreferredCanvasFormat === "function"
        ? gpu.getPreferredCanvasFormat() : "bgra8unorm";
      status = "ready"; reason = "";
      Promise.resolve(device.lost).then((info) => {
        if (status === "destroyed") return;
        lensOracle?.destroy(); lensOracle = null;
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
        { metadata: { size: { ...descriptor.size }, usage: descriptor.usage,
          dimension: descriptor.dimension ?? "2d", mipLevelCount: descriptor.mipLevelCount ?? 1 } });
    },
    createSampler(expectedGeneration, descriptor = {}) {
      assertReady(expectedGeneration); return put("sampler", getDevice().createSampler({ ...descriptor }));
    },
    createShaderModule(expectedGeneration, descriptor) {
      assertReady(expectedGeneration);
      if (typeof descriptor?.code !== "string" || descriptor.code.length === 0) {
        throw new RalWebGpuBrowserHostError("invalid-argument", "WGSL code is required");
      }
      const module = getDevice().createShaderModule({ ...descriptor });
      if (typeof module.getCompilationInfo === "function") {
        Promise.resolve(module.getCompilationInfo()).then((info) => {
          const failures = [...(info?.messages ?? [])].filter((message) =>
            message.type === "error").map((message) =>
            `WGSL ${descriptor.label ?? "module"}:${message.lineNum ?? 0}:${message.linePos ?? 0}: ${message.message}`);
          if (failures.length) {
            const detail = failures.join("\n");
            uncapturedError = uncapturedError ? `${uncapturedError}\n${detail}`.slice(0, 4096)
              : detail.slice(0, 4096);
          }
        });
      }
      return put("shader", module);
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
    createBindGroupFromHandles(expectedGeneration, layoutHandle, entries) {
      assertReady(expectedGeneration);
      const converted = entries.map((entry) => {
        let resource;
        if (entry.kind === 1) {
          resource = { buffer: get(entry.handle, "buffer"), offset: entry.offset };
          if (entry.size) resource.size = entry.size;
        } else if (entry.kind === 2) {
          const texture = get(entry.handle, "texture");
          resource = entry.offset || entry.size
            ? texture.createView({ baseMipLevel: entry.offset,
                mipLevelCount: entry.size || 1 })
            : texture.createView();
        } else if (entry.kind === 3) {
          resource = get(entry.handle, "sampler");
        } else if (entry.kind === 4) {
          // The canvas depth (scene depth of the frame) for compute passes that read it; it has no handle of its own.
          if (!canvasDepthTexture) throw new RalWebGpuBrowserHostError("host-failure", "scene depth unavailable");
          resource = canvasDepthTexture.createView({ aspect: "depth-only" });
        } else throw new RalWebGpuBrowserHostError("invalid-argument", "bind resource kind");
        return { binding: entry.binding, resource };
      });
      return put("bind-group", getDevice().createBindGroup({
        layout: get(layoutHandle, "bind-group-layout"), entries: converted
      }));
    },
    writeBuffer(expectedGeneration, bufferHandle, offset, data) {
      assertReady(expectedGeneration); getQueue().writeBuffer(get(bufferHandle, "buffer"), offset, data); return true;
    },
    writeTexture(expectedGeneration, textureHandle, data, layout, size, mipLevel = 0) {
      assertReady(expectedGeneration);
      const texture = get(textureHandle, "texture");
      getQueue().writeTexture({ texture, mipLevel }, data, layout, size); return true;
    },
    // Explicit diagnostic readback only; never uploads over the source or runs
    // in the production frame loop. Staging is owned across the async mapping.
    async readBuffer(expectedGeneration, bufferHandle, byteSize) {
      assertReady(expectedGeneration);
      const source = get(bufferHandle, "buffer");
      if (!Number.isSafeInteger(byteSize) || byteSize <= 0 || byteSize % 4
          || byteSize > source.size || !(source.usage & 4))
        throw new RalWebGpuBrowserHostError("invalid-argument", "readback bounds/usage");
      const staging = getDevice().createBuffer({size:byteSize,usage:1|8}); // MAP_READ | COPY_DST
      let mapped = false;
      try {
        const encoder = getDevice().createCommandEncoder();
        encoder.copyBufferToBuffer(source,0,staging,0,byteSize);
        getQueue().submit([encoder.finish()]);
        await staging.mapAsync(1,0,byteSize); mapped = true;
        assertReady(expectedGeneration);
        return new Uint8Array(staging.getMappedRange(0,byteSize)).slice();
      } finally {
        if (mapped) staging.unmap();
        staging.destroy();
      }
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
        lensOracle?.reset();
        canvasContext?.unconfigure?.(); canvasDepthTexture?.destroy?.();
        canvasDepthTexture = null; configured = null;
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
      /* Display encoding is hardware sRGB like the native drawables: the
       * canvas keeps its unorm storage (readback stays byte-exact) while every
       * render pass targets an -srgb view, so the tonemapped scene and the
       * UI both write linear light and the view encodes once. */
      const viewFormat = /^(bgra8unorm|rgba8unorm)$/.test(format) ? `${format}-srgb` : format;
      canvasContext.configure({ device: getDevice(), format, alphaMode, colorSpace,
        viewFormats: viewFormat === format ? [] : [viewFormat],
        usage: (globalThis.GPUTextureUsage?.RENDER_ATTACHMENT ?? 16)
          | (globalThis.GPUTextureUsage?.COPY_SRC ?? 1) });
      canvasDepthTexture?.destroy?.();
      lensOracle?.reset();
      canvasDepthTexture = getDevice().createTexture({
        label: "Wired WebGPU canvas depth",
        size: { width: pixelWidth, height: pixelHeight, depthOrArrayLayers: 1 },
        format: "depth32float",
        usage: (globalThis.GPUTextureUsage?.RENDER_ATTACHMENT ?? 16)
          | (globalThis.GPUTextureUsage?.TEXTURE_BINDING ?? 4)
      });
      configured = Object.freeze({ generation, suspended: false, cssWidth, cssHeight, dpr,
        pixelWidth, pixelHeight, format, viewFormat, alphaMode, colorSpace });
      return configured;
    },
    acquireCanvasTexture(expectedGeneration) {
      assertReady(expectedGeneration);
      if (!configured || !canvasContext) throw new RalWebGpuBrowserHostError("not-configured", "WebGPU canvas is not configured");
      const texture = canvasContext.getCurrentTexture();
      const textureHandle = put("canvas-texture", texture, { destroyable: false });
      const view = configured.viewFormat !== configured.format
        ? texture.createView({ format: configured.viewFormat }) : texture.createView();
      const viewHandle = put("texture-view", view, { destroyable: false });
      canvasFrames.set(textureHandle, viewHandle);
      return Object.freeze({ generation, textureHandle, viewHandle, ...configured });
    },
    unconfigureCanvas(expectedGeneration) {
      assertReady(expectedGeneration); canvasContext?.unconfigure?.();
      lensOracle?.reset();
      canvasDepthTexture?.destroy?.(); canvasDepthTexture = null;
      configured = null; return true;
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
    beginLensVisibility(expectedGeneration, frameGeneration, params, sources) {
      assertReady(expectedGeneration);
      if (!configured || configured.suspended || !canvasDepthTexture)
        throw new RalWebGpuBrowserHostError("not-configured", "lens scene depth unavailable");
      if (!lensOracle) lensOracle = createRalWebGpuLensOracle(getDevice());
      const query = lensOracle.begin(canvasDepthTexture, frameGeneration, params, sources);
      return query ? put("lens-query", query, { destroyable: false }) : 0;
    },
    pollLensVisibility(expectedGeneration, handle) {
      assertReady(expectedGeneration);
      const query = get(handle, "lens-query");
      return Object.freeze({ ...query, values: query.values?.slice() ?? null });
    },
    resetLensVisibility(expectedGeneration) {
      assertReady(expectedGeneration); lensOracle?.reset();
    },
    createCommandEncoder(expectedGeneration, descriptor = {}) {
      assertReady(expectedGeneration);
      const device = getDevice();
      const validationScope = typeof device.pushErrorScope === "function"
        && typeof device.popErrorScope === "function";
      if (validationScope) device.pushErrorScope("validation");
      return put("encoder", device.createCommandEncoder(descriptor),
        { metadata: { validationScope } });
    },
    beginRenderPass(expectedGeneration, encoderHandle, viewHandle, descriptor = {}) {
      assertReady(expectedGeneration);
      const encoder = get(encoderHandle, "encoder");
      const canvasTextureHandle = canvasFrames.has(viewHandle) ? viewHandle
        : [...canvasFrames].find(([, candidateView]) => candidateView === viewHandle)?.[0];
      let resolvedView;
      const mip = descriptor.targetMipLevel ?? 0, layer = descriptor.targetArrayLayer ?? 0;
      if (!Number.isInteger(mip) || !Number.isInteger(layer) || mip < 0 || layer < 0)
        throw new RalWebGpuBrowserHostError("invalid-argument", "invalid render subresource");
      // Owned render textures use their resource identity, just like sampled
      // textures. Keep the view cached on that resource; no extra handle or
      // hidden allocation per frame, and no canvas substitution on failure.
      const target = entries.get(viewHandle);
      if (target?.kind === "texture") {
        const texture = get(viewHandle, "texture");
        const metadata = target.metadata;
        if (!(metadata.usage & (globalThis.GPUTextureUsage?.RENDER_ATTACHMENT ?? 16))
            || metadata.dimension !== "2d")
          throw new RalWebGpuBrowserHostError("invalid-argument", "texture is not a 2D render attachment");
        if (mip >= (metadata.mipLevelCount ?? 1) || layer >= (metadata.size.depthOrArrayLayers ?? 1))
          throw new RalWebGpuBrowserHostError("invalid-argument", "render subresource out of bounds");
        // The existing scene-depth authority is canvas-scoped. A postprocess
        // target can omit depth; a scene target must match its exact extent.
        if ((descriptor.depthEnabled ?? true) && (!configured
            || Math.max(1, Math.floor(metadata.size.width / 2 ** mip)) !== configured.pixelWidth
            || Math.max(1, Math.floor(metadata.size.height / 2 ** mip)) !== configured.pixelHeight))
          throw new RalWebGpuBrowserHostError("invalid-argument", "render target and scene depth extents differ");
        metadata.renderViews ??= new Map();
        const key = mip * (metadata.size.depthOrArrayLayers ?? 1) + layer;
        if (!metadata.renderViews.has(key)) metadata.renderViews.set(key, texture.createView({
          dimension: "2d", baseMipLevel: mip, mipLevelCount: 1,
          baseArrayLayer: layer, arrayLayerCount: 1 }));
        resolvedView = metadata.renderViews.get(key);
      } else {
        if (mip || layer)
          throw new RalWebGpuBrowserHostError("invalid-argument", "canvas view has no selectable subresource");
        resolvedView = canvasFrames.has(viewHandle)
          ? get(canvasFrames.get(viewHandle), "texture-view") : get(viewHandle, "texture-view");
      }
      const colorAttachment = { view: resolvedView,
        loadOp: descriptor.colorLoadOp ?? "clear", storeOp: "store",
        clearValue: descriptor.clearValue ?? { r: 0, g: 0, b: 0, a: 1 }, ...(descriptor.colorAttachment ?? {}) };
      const passDescriptor = { colorAttachments: [colorAttachment], ...(descriptor.pass ?? {}) };
      if (descriptor.depthEnabled ?? true) {
        if (!canvasDepthTexture) throw new RalWebGpuBrowserHostError("not-configured", "WebGPU canvas depth target is unavailable");
        passDescriptor.depthStencilAttachment = {
          view: canvasDepthTexture.createView(),
          depthLoadOp: descriptor.depthLoadOp ?? "clear",
          depthStoreOp: descriptor.depthStore === false ? "discard" : "store",
          depthClearValue: descriptor.clearDepth ?? 1
        };
      }
      const passHandle = put("render-pass",
        encoder.beginRenderPass(passDescriptor),
        { destroyable: false });
      if (canvasTextureHandle) encoderCanvasFrames.set(encoderHandle, canvasTextureHandle);
      passEncoders.set(passHandle, encoderHandle);
      if (!encoderDrawCounts.has(encoderHandle)) encoderDrawCounts.set(encoderHandle, 0);
      return passHandle;
    },
    beginComputePass(expectedGeneration, encoderHandle, descriptor = {}) {
      assertReady(expectedGeneration);
      const encoder = get(encoderHandle, "encoder");
      let timing = null, passDescriptor = {};
      if (descriptor.timestampWrites === true) {
        if (!limits.timestampQueries) {
          throw new RalWebGpuBrowserHostError("unsupported-feature",
            "timestamp-query was requested but is unavailable");
        }
        const usage = globalThis.GPUBufferUsage ?? {
          MAP_READ: 1, COPY_SRC: 4, COPY_DST: 8, QUERY_RESOLVE: 512
        };
        let querySet = null, resolve = null, readback = null;
        try {
          querySet = getDevice().createQuerySet({ type: "timestamp", count: 2 });
          resolve = getDevice().createBuffer({ size: 16,
            usage: usage.QUERY_RESOLVE | usage.COPY_SRC });
          readback = getDevice().createBuffer({ size: 16,
            usage: usage.COPY_DST | usage.MAP_READ });
          timing = { querySet, resolve, readback };
          passDescriptor = { timestampWrites: { querySet,
            beginningOfPassWriteIndex: 0, endOfPassWriteIndex: 1 } };
          entries.get(encoderHandle).metadata.timing = timing;
        } catch (error) {
          querySet?.destroy?.(); resolve?.destroy?.(); readback?.destroy?.();
          throw error;
        }
      }
      return put("compute-pass", encoder.beginComputePass(passDescriptor),
        { destroyable: false });
    },
    recordIndexedDraw(expectedGeneration, passHandle, draw) {
      assertReady(expectedGeneration);
      const pass = get(passHandle, "render-pass");
      const pipeline = get(draw.pipelineHandle, "pipeline");
      const vertex = get(draw.vertexBufferHandle, "buffer");
      const index = get(draw.indexBufferHandle, "buffer");
	  const texture = draw.textureHandle ? get(draw.textureHandle, "texture") : null;
      const secondary = draw.secondaryTextureHandle ? get(draw.secondaryTextureHandle, "texture") : null;
	  const sampler = draw.samplerHandle ? get(draw.samplerHandle, "sampler") : null;
      finitePositive(draw.indexCount, "index count");
      pass.setPipeline(pipeline); pass.setVertexBuffer(draw.vertexSlot ?? 0, vertex, draw.vertexOffset ?? 0);
      pass.setIndexBuffer(index, draw.indexFormat ?? "uint32", draw.indexOffset ?? 0);
	  if (draw.textureBindGroupSlot && texture && sampler) {
		const materialGroup = draw.textureBindGroupSlot - 1;
		/* The implicit material group is texture @0 + sampler @1; a draw that
		 * carries a secondary texture (world lightmap) declares it at @2. */
		const entries = [
		  { binding: 0, resource: texture.createView() }, { binding: 1, resource: sampler }
		];
		if (secondary) entries.push({ binding: 2, resource: secondary.createView() });
		const group = getDevice().createBindGroup({ layout: pipeline.getBindGroupLayout(materialGroup), entries });
		pass.setBindGroup(materialGroup, group);
	  }
      for (const binding of draw.bindGroups ?? []) pass.setBindGroup(binding.index, get(binding.handle, "bind-group"), binding.offsets ?? []);
      pass.drawIndexed(draw.indexCount, draw.instanceCount ?? 1, draw.firstIndex ?? 0,
        draw.baseVertex ?? 0, draw.firstInstance ?? 0);
      const encoderHandle = passEncoders.get(passHandle);
      if (encoderHandle) encoderDrawCounts.set(encoderHandle,
        (encoderDrawCounts.get(encoderHandle) ?? 0) + 1);
      return true;
    },
    recordComputeDispatch(expectedGeneration, passHandle, dispatch) {
      assertReady(expectedGeneration);
      const pass = get(passHandle, "compute-pass");
      pass.setPipeline(get(dispatch.pipelineHandle, "pipeline"));
      for (const binding of dispatch.bindGroups ?? []) {
        pass.setBindGroup(binding.index, get(binding.handle, "bind-group"), binding.offsets ?? []);
      }
      finitePositive(dispatch.groupCountX, "compute group count x");
      finitePositive(dispatch.groupCountY, "compute group count y");
      finitePositive(dispatch.groupCountZ, "compute group count z");
      pass.dispatchWorkgroups(dispatch.groupCountX, dispatch.groupCountY, dispatch.groupCountZ);
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
      const encoderMetadata = entries.get(encoderHandle)?.metadata;
      const validationScope = encoderMetadata?.validationScope === true;
      const timing = encoderMetadata?.timing ?? null;
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
      if (timing) {
        encoder.resolveQuerySet(timing.querySet, 0, 2, timing.resolve, 0);
        encoder.copyBufferToBuffer(timing.resolve, 0, timing.readback, 0, 16);
      }
      const commandBuffer = encoder.finish(descriptor);
      encoderCanvasFrames.delete(encoderHandle); encoderDrawCounts.delete(encoderHandle);
      release(encoderHandle);
      return put("command-buffer", commandBuffer, { destroyable: false,
        metadata: { canvasCapture, validationScope, timing } });
    },
    submit(expectedGeneration, commandBufferHandle) {
      assertReady(expectedGeneration);
      const queue = getQueue(), commandBuffer = get(commandBufferHandle, "command-buffer");
      const commandMetadata = entries.get(commandBufferHandle)?.metadata;
      const canvasCapture = commandMetadata?.canvasCapture;
      const timing = commandMetadata?.timing ?? null;
      queue.submit([commandBuffer]); release(commandBufferHandle);
      if (canvasCapture) canvasCaptureCandidates.push(canvasCapture);
      const submission = { status: "pending", error: "",
        timingState: timing ? "pending" : "unavailable",
        gpuDurationNanoseconds: 0 };
      const destroyTiming = () => {
        timing?.querySet?.destroy?.(); timing?.resolve?.destroy?.();
        timing?.readback?.destroy?.();
      };
      const validation = commandMetadata?.validationScope
        ? Promise.resolve(getDevice().popErrorScope()) : Promise.resolve(null);
      Promise.all([validation, Promise.resolve(queue.onSubmittedWorkDone())]).then(async ([error]) => {
        if (error) throw error;
        if (timing) {
          await timing.readback.mapAsync(globalThis.GPUMapMode?.READ ?? 1, 0, 16);
          const values = new BigUint64Array(timing.readback.getMappedRange(0, 16));
          const begin = values[0], end = values[1];
          timing.readback.unmap();
          if (end < begin || end - begin > BigInt(Number.MAX_SAFE_INTEGER)) {
            throw new RalWebGpuBrowserHostError("invalid-timestamp",
              "WebGPU timestamp query returned an invalid interval");
          }
          submission.gpuDurationNanoseconds = Number(end - begin);
          // Browser timestamp quantization can collapse a short interval.
          // Keep GPU completion independent of an unavailable timing sample.
          submission.timingState = end === begin ? "unavailable" : "valid";
        }
        submission.status = "ready"; destroyTiming();
      }).catch((error) => {
        submission.status = "failed"; submission.timingState = timing ? "failed" : "unavailable";
        submission.gpuDurationNanoseconds = 0;
        submission.error = String(error?.message ?? error ?? uncapturedError); destroyTiming();
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
      canvasContext?.unconfigure?.(); canvasDepthTexture?.destroy?.();
      canvasDepthTexture = null; configured = null;
      canvasFrames.clear();
      lensOracle?.destroy(); lensOracle = null;
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
