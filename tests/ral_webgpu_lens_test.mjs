// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
import assert from "node:assert/strict";
import { createRalWebGpuLensOracle, LENS_READBACK_SLOTS,
  LENS_SOURCE_LIMIT } from "../code/render/ral/backends/webgpu/ral_webgpu_lens.mjs";

const tick = () => new Promise(resolve => setImmediate(resolve));
function fixture() {
  const buffers = [], maps = [], validations = [], writes = [], groups = [];
  let throws = false;
  const device = {
    pushErrorScope(kind) { assert.equal(kind, "validation"); },
    popErrorScope() { return Promise.resolve(validations.shift() ?? null); },
    createShaderModule({ code }) { assert.match(code, /texture_depth_2d/); return {}; },
    createComputePipeline() { return { getBindGroupLayout() { return {}; } }; },
    createBindGroup({ entries }) { assert.equal(entries.length, 4); return {}; },
    createBuffer({ size, usage }) {
      if (throws) throw new Error("allocation failed");
      const buffer = { bytes: new ArrayBuffer(size), destroyed: false, mapped: false,
        destroy() { assert.equal(this.destroyed, false); this.destroyed = true; },
        mapAsync() {
          assert.ok(usage & 1); assert.equal(this.mapped, false);
          return new Promise((resolve, reject) => maps.push({ buffer, resolve() {
            buffer.mapped = true; resolve();
          }, reject }));
        },
        getMappedRange() { assert.ok(this.mapped); return this.bytes; },
        unmap() { assert.ok(this.mapped); this.mapped = false; }
      };
      buffers.push(buffer); return buffer;
    },
    createCommandEncoder() {
      return { beginComputePass() { return {
        setPipeline() {}, setBindGroup() {}, end() {},
        dispatchWorkgroups(x) { groups.push(x); }
      }; }, copyBufferToBuffer() {}, finish() { return {}; } };
    },
    queue: {
      writeBuffer(buffer, offset, data) {
        assert.equal(buffer.destroyed, false); assert.equal(offset, 0);
        writes.push(new Uint8Array(data.buffer, data.byteOffset, data.byteLength).slice());
      },
      submit(commands) { assert.equal(commands.length, 1); }
      // Deliberately no onSubmittedWorkDone: oracle may not stall the whole queue.
    }
  };
  return { oracle: createRalWebGpuLensOracle(device), buffers, maps, validations,
    writes, groups, failAllocation() { throws = true; } };
}
const params = () => new Float32Array([
  0,0,0,0, 1,0,0,0, 0,1,0,0, 0,0,1,0, 1,1,4,65536/65532, 1280,720,1,0
]);
const sources = (ids = [7]) => {
  const bytes = new Uint8Array(ids.length * 32), view = new DataView(bytes.buffer);
  ids.forEach((id, i) => {
    view.setFloat32(i * 32, 100, true);
    view.setFloat32(i * 32 + 12, 4, true);
    view.setUint32(i * 32 + 16, id, true);
  });
  return bytes;
};
const depth = { createView() { return {}; } };
function complete(f, index, value = 0.5) {
  new Float32Array(f.maps[index].buffer.bytes).fill(value);
  f.maps[index].resolve();
}

{
  const f = fixture(), p = params(), s = sources();
  const a = f.oracle.begin(depth, 1, p, s);
  const b = f.oracle.begin(depth, 2, p, sources([8]));
  const c = f.oracle.begin(depth, 3, p, s);
  assert.equal(f.oracle.begin(depth, 4, p, s), null);
  assert.equal(f.buffers.length, LENS_READBACK_SLOTS * 4);
  assert.equal(a.values, null); assert.equal(a.status, "pending");
  p.fill(0); s.fill(0);
  assert.equal(new DataView(f.writes[0].buffer).getFloat32(0, true), 100);
  complete(f, 1, 0.75); await tick();
  assert.equal(b.status, "ready"); assert.equal(b.values[8], 0.75);
  assert.equal(b.values[7], 0); // Old/unsubmitted source slots are not exposed.
  assert.equal(a.status, "pending");
  const d = f.oracle.begin(depth, 4, params(), sources());
  assert.equal(f.buffers.length, 12); // Reuse completed slot, no per-frame allocation.
  f.oracle.reset();
  assert.equal(b.status, "cancelled"); assert.equal(b.values, null);
  assert.equal(a.status, "cancelled"); assert.equal(c.status, "cancelled");
  assert.equal(d.status, "cancelled");
  assert.equal(f.oracle.begin(depth, 1, params(), sources()), null);
  complete(f, 0); complete(f, 2); complete(f, 3); await tick();
  assert.equal(a.values, null); assert.equal(d.values, null);
  const e = f.oracle.begin(depth, 1, params(), sources());
  f.oracle.destroy(); f.oracle.destroy();
  complete(f, 4); await tick();
  assert.equal(e.status, "cancelled"); assert.equal(e.values, null);
  assert.ok(f.buffers.every(buffer => buffer.destroyed));
}
{
  const f = fixture();
  assert.throws(() => f.oracle.begin(depth, 1, params(), sources([7,7])), /source id/);
  assert.throws(() => f.oracle.begin(depth, 1, params(), sources([LENS_SOURCE_LIMIT])), /source id/);
  const p = params(); p[0] = NaN;
  assert.throws(() => f.oracle.begin(depth, 1, p, sources()), /non-finite/);
  assert.equal(f.buffers.length, 0);
  f.validations.push(new Error("GPU validation rejected"));
  const q = f.oracle.begin(depth, 1, params(), sources());
  await tick(); assert.equal(q.status, "pending");
  complete(f, 0); await tick();
  assert.equal(q.status, "failed"); assert.equal(q.values, null);
  const q2 = f.oracle.begin(depth, 2, params(), sources());
  f.maps[1].reject(new Error("device lost")); await tick();
  assert.equal(q2.status, "failed"); assert.equal(q2.values, null);
  const q3 = f.oracle.begin(depth, 3, params(), sources());
  complete(f, 2, NaN); await tick();
  assert.equal(q3.status, "failed");
  f.oracle.destroy(); assert.ok(f.buffers.every(buffer => buffer.destroyed));
}
{
  const f = fixture(); f.failAllocation();
  const q = f.oracle.begin(depth, 1, params(), sources());
  assert.equal(q.status, "failed"); f.oracle.destroy();
}
console.log("WebGPU lens async ownership contract: PASS (mock GPU; not visual parity)");
