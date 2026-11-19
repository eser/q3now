// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Native forward-Z scene-depth oracle. Visual flare recipes stay in WiredFX.
export const LENS_SOURCE_LIMIT = 352;
export const LENS_READBACK_SLOTS = 3;
export const LENS_VISIBILITY_WGSL = `
struct Source { originRadius: vec4<f32>, identity: vec4<u32> }
struct Params {
  origin: vec4<f32>, forward: vec4<f32>, left: vec4<f32>,
  up: vec4<f32>, projection: vec4<f32>, viewportOpticsCount: vec4<f32>
}
@group(0) @binding(0) var<storage, read> sources: array<Source>;
@group(0) @binding(1) var<storage, read_write> results: array<f32>;
@group(0) @binding(2) var<uniform> p: Params;
@group(0) @binding(3) var sceneDepth: texture_depth_2d;
@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) invocation: vec3<u32>) {
  let index = invocation.x;
  if (index >= bitcast<u32>(p.viewportOpticsCount.w)) { return; }
  let source = sources[index];
  let id = source.identity.x;
  if (id >= 352u) { return; }
  results[id] = 0.0;
  let delta = source.originRadius.xyz - p.origin.xyz;
  let forward = dot(delta, p.forward.xyz);
  if (forward <= p.projection.z) { return; }
  let ndc = vec2<f32>(-dot(delta, p.left.xyz) * p.projection.x,
    dot(delta, p.up.xyz) * p.projection.y) / forward;
  if (any(abs(ndc) > vec2<f32>(1.05))) { return; }
  let viewport = vec2<f32>(textureDimensions(sceneDepth));
  let center = (ndc * vec2<f32>(0.5, -0.5) + 0.5) * viewport;
  let radius = clamp(source.originRadius.w / forward *
    p.projection.y * viewport.y * 0.5, 1.0, 16.0);
  let taps = array<vec2<f32>, 9>(
    vec2<f32>(0.0), vec2<f32>(1.0, 0.0), vec2<f32>(-1.0, 0.0),
    vec2<f32>(0.0, 1.0), vec2<f32>(0.0, -1.0),
    vec2<f32>(0.7071, 0.7071), vec2<f32>(-0.7071, 0.7071),
    vec2<f32>(0.7071, -0.7071), vec2<f32>(-0.7071, -0.7071));
  // WebGPU world projection has a finite far plane; use its exact depth scale.
  let sourceDepth = p.projection.w * (forward - p.projection.z) / forward;
  var visible = 0.0;
  for (var tap = 0u; tap < 9u; tap++) {
    let pixel = vec2<i32>(clamp(center + taps[tap] * radius,
      vec2<f32>(0.0), viewport - 1.0));
    let depth = textureLoad(sceneDepth, pixel, 0);
    visible += select(0.0, 1.0, sourceDepth <= depth + 0.0015);
  }
  results[id] = visible / 9.0 * clamp(p.viewportOpticsCount.z, 0.0, 1.0);
}
`;

// No waitUntilCompleted or queue-wide wait. Each slot is reusable only after
// its own map/unmap completes. Reset invalidates results without reusing buffers
// still owned by the GPU; destruction also makes late callbacks inert.
export function createRalWebGpuLensOracle(device) {
  const usage = globalThis.GPUBufferUsage ??
    { MAP_READ: 1, COPY_SRC: 4, COPY_DST: 8, UNIFORM: 64, STORAGE: 128 };
  let pipeline = null, destroyed = false, epoch = 1, lastFrame = 0;
  const slots = Array.from({ length: LENS_READBACK_SLOTS }, () =>
    ({ busy: false, buffers: null, query: null }));
  const destroyBuffers = (slot) => {
    if (!slot.buffers) return;
    for (const buffer of Object.values(slot.buffers)) buffer.destroy();
    slot.buffers = null;
  };
  const invalidate = () => {
    ++epoch; lastFrame = 0;
    for (const slot of slots) {
      if (slot.query) { slot.query.status = "cancelled"; slot.query.values = null; }
    }
  };
  return Object.freeze({
    begin(depth, frame, params, packedSources) {
      if (destroyed) throw new Error("lens oracle destroyed");
      if (!Number.isSafeInteger(frame) || frame <= lastFrame)
        throw new RangeError("lens frame must increase within the current scope");
      if (!(params instanceof Float32Array) || params.length !== 24 ||
          !(packedSources instanceof Uint8Array) || !packedSources.byteLength ||
          packedSources.byteLength % 32 || packedSources.byteLength > LENS_SOURCE_LIMIT * 32)
        throw new RangeError("lens input layout");
      if (!depth) throw new TypeError("lens scene depth missing");
      for (let i = 0; i < 23; i++)
        if (!Number.isFinite(params[i])) throw new RangeError("non-finite lens view");
      if (params[16] <= 0 || params[17] <= 0 || params[18] <= 0 || params[19] < 1 ||
          params[20] <= 0 || params[21] <= 0 || params[22] < 0 || params[22] > 1)
        throw new RangeError("lens projection/viewport/optics");
      const count = packedSources.byteLength / 32;
      const sourceView = new DataView(packedSources.buffer,
        packedSources.byteOffset, packedSources.byteLength);
      const active = new Set();
      for (let i = 0; i < count; i++) {
        const at = i * 32, id = sourceView.getUint32(at + 16, true);
        if (id >= LENS_SOURCE_LIMIT || active.has(id)) throw new RangeError("lens source id");
        for (let c = 0; c < 4; c++)
          if (!Number.isFinite(sourceView.getFloat32(at + c * 4, true)))
            throw new RangeError("non-finite lens source");
        if (sourceView.getFloat32(at + 12, true) < 0) throw new RangeError("negative lens radius");
        active.add(id);
      }
      const slot = slots.find((entry) => !entry.busy);
      if (!slot) return null; // Explicit backpressure, never overwrite in-flight input.
      const query = { status: "pending", frame, values: null, error: "" };
      const queryEpoch = epoch;
      const uniform = params.slice();
      new Uint32Array(uniform.buffer)[23] = count;
      slot.busy = true; slot.query = query; lastFrame = frame;
      const current = () => !destroyed && epoch === queryEpoch &&
        query.status === "pending";
      const fail = (error) => {
        if (current()) {
          query.status = "failed"; query.values = null;
          query.error = String(error?.message ?? error);
        }
      };
      let scope = false;
      try {
        device.pushErrorScope("validation"); scope = true;
        if (!pipeline) pipeline = device.createComputePipeline({
          label: "Wired lens scene-depth visibility",
          layout: "auto", compute: {
            module: device.createShaderModule({ code: LENS_VISIBILITY_WGSL }),
            entryPoint: "main" }
        });
        if (!slot.buffers) {
          slot.buffers = {};
          slot.buffers.input = device.createBuffer({
            size: LENS_SOURCE_LIMIT * 32, usage: usage.STORAGE | usage.COPY_DST });
          slot.buffers.result = device.createBuffer({
            size: LENS_SOURCE_LIMIT * 4, usage: usage.STORAGE | usage.COPY_SRC });
          slot.buffers.params = device.createBuffer({
            size: 96, usage: usage.UNIFORM | usage.COPY_DST });
          slot.buffers.readback = device.createBuffer({
            size: LENS_SOURCE_LIMIT * 4, usage: usage.MAP_READ | usage.COPY_DST });
        }
        const b = slot.buffers;
        device.queue.writeBuffer(b.input, 0, packedSources);
        device.queue.writeBuffer(b.params, 0, uniform);
        const group = device.createBindGroup({ layout: pipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: { buffer: b.input } },
            { binding: 1, resource: { buffer: b.result } },
            { binding: 2, resource: { buffer: b.params } },
            { binding: 3, resource: depth.createView() }
          ] });
        const encoder = device.createCommandEncoder();
        const pass = encoder.beginComputePass();
        pass.setPipeline(pipeline); pass.setBindGroup(0, group);
        pass.dispatchWorkgroups(Math.ceil(count / 64)); pass.end();
        encoder.copyBufferToBuffer(b.result, 0, b.readback, 0, LENS_SOURCE_LIMIT * 4);
        device.queue.submit([encoder.finish()]);
        const validation = device.popErrorScope(); scope = false;
        // Register map immediately after submit: validation and mapping settle
        // together before the slot can be reused, including rejection paths.
        Promise.allSettled([validation, b.readback.mapAsync(
          globalThis.GPUMapMode?.READ ?? 1)]).then(([checked, mapped]) => {
          try {
            if (checked.status === "rejected") throw checked.reason;
            if (checked.value) throw checked.value;
            if (mapped.status === "rejected") throw mapped.reason;
            if (!current()) return;
            const values = new Float32Array(b.readback.getMappedRange());
            const result = new Float32Array(LENS_SOURCE_LIMIT);
            for (const id of active) {
              if (!Number.isFinite(values[id]) || values[id] < 0 || values[id] > 1)
                throw new Error("invalid native lens visibility");
              result[id] = values[id];
            }
            query.values = result; query.status = "ready";
          } catch (error) { fail(error); }
          finally {
            if (!destroyed && mapped.status === "fulfilled") b.readback.unmap();
            slot.busy = false; slot.query = null;
          }
        });
      } catch (error) {
        if (scope) Promise.resolve(device.popErrorScope()).catch(() => {});
        fail(error);
        // Failed construction/encoding never publishes a buffer set for reuse.
        destroyBuffers(slot); slot.busy = false; slot.query = null;
      }
      // Previously completed queries also become unreadable after a scope
      // reset. Callers cannot mutate the internal completion state.
      return Object.freeze({
        frame,
        get status() { return destroyed || epoch !== queryEpoch ? "cancelled" : query.status; },
        get values() { return destroyed || epoch !== queryEpoch ? null : query.values; },
        get error() { return query.error; }
      });
    },
    reset() { if (!destroyed) invalidate(); },
    destroy() {
      if (destroyed) return;
      invalidate(); destroyed = true;
      for (const slot of slots) destroyBuffers(slot);
      pipeline = null;
    }
  });
}
