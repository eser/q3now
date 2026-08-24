// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from "node:assert/strict";
import { createRalWebGpuBrowserDispatch, RalWebGpuBrowserOpcode as OP }
  from "../code/render/ral/backends/webgpu/ral_webgpu_browser_dispatch.mjs";

const memory = new WebAssembly.Memory({ initial: 4 });
const REQUEST = 0x1000, RESPONSE = 0x3000, DATA = 0x5000;
const sizes = { 1:24,2:24,3:32,4:24,5:32,6:24,7:24,8:56,9:24,10:32,11:48,
  12:40,13:40,14:40,15:32,16:56,17:56,18:72,19:48,20:24,21:120,22:40,
  23:40,24:1352,25:32,26:24,27:40,28:104,29:24,30:24,31:40,32:32,33:32 };
const responseSizes = { 2:376,4:40,7:224,10:32,12:32,13:32,14:32,18:32,19:32,
  21:32,22:32,23:32,24:32,26:32,27:32,30:32,31:32,32:32 };
const seen = new Set();
const put64 = (view, offset, value) => view.setBigUint64(offset, BigInt(value), true);
const get64 = (view, offset) => Number(view.getBigUint64(offset, true));

class FakeHost {
  constructor() { this.generation = 0; this.next = 10; this.entries = new Map(); this.log = [];
    this.submissions = new Map(); this.failRoundTrip = false; }
  put(kind) { const id = this.next++; this.entries.set(id, kind); this.log.push(`create:${kind}`); return id; }
  need(id, kind) { assert.equal(this.entries.get(id), kind); }
  async initialize(generation) { this.generation = generation; this.adapter = this.put("adapter");
    this.device = this.put("device"); this.queue = this.put("queue"); return this.receipt(generation); }
  receipt(generation) { assert.equal(generation, this.generation); return { ready: true, status: "ready",
    generation, adapterHandle: this.adapter, deviceHandle: this.device, queueHandle: this.queue,
    limits: { maxColorAttachments:8, maxTextureDimension2D:8192, maxTextureDimension3D:2048,
      maxTextureArrayLayers:256, maxComputeInvocationsPerWorkgroup:256,
      maxSampledTexturesPerShaderStage:16, maxBindGroups:4, maxBindingsPerBindGroup:64,
      maxStorageBufferBindingSize:134217728, minUniformBufferOffsetAlignment:256,
      minStorageBufferOffsetAlignment:256 }, lost: null }; }
  createBuffer(g, desc) { assert.equal(g,7); assert.ok(desc.size && desc.usage); return this.put("buffer"); }
  createTexture(g, desc) { assert.equal(g,7); assert.equal(desc.size.width,64); return this.put("texture"); }
  createSampler(g, desc) { assert.equal(g,7); assert.ok(desc); return this.put("sampler"); }
  writeBuffer(g,id,offset,data) { assert.equal(g,7); this.need(id,"buffer"); assert.equal(offset,0); assert.ok(data.length); this.log.push("write:buffer"); }
  writeTexture(g,id,data,layout,size) { assert.equal(g,7); this.need(id,"texture"); assert.ok(data.length && layout.bytesPerRow && size.height); this.log.push("write:texture"); }
  async roundTrip(g,upload,readback,data) { assert.equal(g,7); this.need(upload,"buffer"); this.need(readback,"buffer");
    if (this.failRoundTrip) throw new Error("fixture rejection"); return data.slice(); }
  createShaderModule(g,desc) { assert.equal(g,7); assert.match(desc.code,/fn main/); return this.put("shader"); }
  createBindGroupLayout(g,desc) { assert.equal(g,7); assert.deepEqual(desc.entries,[]); return this.put("bind-group-layout"); }
  createPipelineLayout(g,handles) { assert.equal(g,7); assert.deepEqual(handles,[]); return this.put("pipeline-layout"); }
  createRenderPipelineFromHandles(g,desc) { assert.equal(g,7); this.need(desc.layoutHandle,"pipeline-layout");
    this.need(desc.vertex.moduleHandle,"shader"); this.need(desc.fragment.moduleHandle,"shader");
    assert.equal(desc.primitive.topology,"triangle-list"); assert.equal(desc.vertex.buffers[0].attributes[0].format,"float32x3");
    return this.put("pipeline"); }
  createComputePipelineFromHandles() { throw new Error("not used"); }
  configureCanvas(g,desc) { assert.equal(g,7); assert.equal(desc.cssWidth,1280); assert.equal(desc.cssHeight,720); this.log.push("canvas:configure"); return { suspended:false }; }
  unconfigureCanvas(g) { assert.equal(g,7); this.log.push("canvas:unconfigure"); }
  acquireCanvasTexture(g) { assert.equal(g,7); return { textureHandle:this.put("canvas-texture") }; }
  presentCanvas(g,id) { assert.equal(g,7); this.need(id,"canvas-texture"); this.release(g,id); this.log.push("canvas:present"); }
  createCommandEncoder(g) { assert.equal(g,7); return this.put("encoder"); }
  beginRenderPass(g,encoder,target) { assert.equal(g,7); this.need(encoder,"encoder"); this.need(target,"canvas-texture"); return this.put("render-pass"); }
  beginComputePass() { throw new Error("not used"); }
  recordIndexedDraw(g,pass,draw) { assert.equal(g,7); this.need(pass,"render-pass"); this.need(draw.pipelineHandle,"pipeline");
    this.need(draw.vertexBufferHandle,"buffer"); this.need(draw.indexBufferHandle,"buffer"); this.need(draw.textureHandle,"texture");
    this.need(draw.samplerHandle,"sampler"); assert.equal(draw.indexCount,3); this.log.push("draw"); }
  endRenderPass(g,id) { assert.equal(g,7); this.need(id,"render-pass"); this.release(g,id); }
  endComputePass() { throw new Error("not used"); }
  finishEncoder(g,id) { assert.equal(g,7); this.need(id,"encoder"); this.release(g,id); return this.put("command-buffer"); }
  submit(g,id) { assert.equal(g,7); this.need(id,"command-buffer"); this.release(g,id); const result=this.put("submission");
    const state={status:"pending"}; this.submissions.set(result,state); Promise.resolve().then(()=>{state.status="ready";}); return result; }
  pollSubmission(g,id) { assert.equal(g,7); return this.submissions.get(id); }
  release(g,id) { assert.equal(g,7); const existed=this.entries.delete(id); if(existed)this.log.push("release"); return existed; }
  destroy(g) { assert.equal(g,7); for (const id of [...this.entries.keys()].reverse()) this.release(g,id); this.generation=0; this.log.push("destroy"); }
}

const host = new FakeHost();
const bridge = createRalWebGpuBrowserDispatch({ host, memory, canvasIdentity:0x7000 });
function call(op, write = () => {}, expect = 1) {
  const bytes=sizes[op], responseBytes=responseSizes[op]??24;
  const view=new DataView(memory.buffer,REQUEST,bytes); new Uint8Array(memory.buffer,REQUEST,bytes).fill(0);
  view.setUint32(0,1,true); view.setUint32(4,op,true); view.setUint32(8,bytes,true); write(view);
  new Uint8Array(memory.buffer,RESPONSE,responseBytes).fill(0xa5);
  const result=bridge.dispatch(op,REQUEST,bytes,RESPONSE,responseBytes); assert.equal(result,expect);
  if(expect) { seen.add(op); const out=new DataView(memory.buffer,RESPONSE,responseBytes);
    assert.equal(out.getUint32(0,true),1); assert.equal(out.getUint32(4,true),op); assert.equal(out.getUint32(8,true),responseBytes); return out; }
  assert.ok(new Uint8Array(memory.buffer,RESPONSE,responseBytes).every((v)=>v===0xa5)); return null;
}

call(OP.BEGIN_ADAPTER,(v)=>put64(v,16,7)); await Promise.resolve(); await Promise.resolve();
let out=call(OP.POLL_ADAPTER,(v)=>put64(v,16,7)); const adapter=get64(out,24); assert.equal(out.getUint32(16,true),2);
call(OP.BEGIN_DEVICE,(v)=>{put64(v,16,adapter);put64(v,24,8);});
out=call(OP.POLL_DEVICE,(v)=>put64(v,16,8)); const device=get64(out,24),queue=get64(out,32);
call(OP.POLL_DEVICE_LOSS,(v)=>put64(v,16,7));
call(OP.CANVAS_CONFIGURE,(v)=>{put64(v,16,0x7000);put64(v,24,device);v.setUint32(32,1280,true);v.setUint32(36,720,true);v.setUint32(40,5,true);v.setUint32(48,1,true);});
out=call(OP.CANVAS_ACQUIRE,(v)=>{put64(v,16,0x7000);put64(v,24,1);}); const frame=get64(out,24);
const createBuffer=(usage,size=64)=>{const r=call(OP.CREATE_BUFFER,(v)=>{put64(v,16,device);put64(v,24,size);v.setUint32(32,usage,true);v.setUint32(36,1,true);});return get64(r,24);};
const vertex=createBuffer(4), index=createBuffer(8), upload=createBuffer(3), readback=createBuffer(67);
out=call(OP.CREATE_TEXTURE,(v)=>{put64(v,16,device);v.setUint32(24,64,true);v.setUint32(28,1,true);v.setUint32(32,1,true);v.setUint32(36,4,true);}); const texture=get64(out,24);
out=call(OP.CREATE_SAMPLER,(v)=>{put64(v,16,device);v.setUint32(24,1,true);v.setUint32(28,1,true);v.setUint32(32,1,true);}); const sampler=get64(out,24);
new Uint8Array(memory.buffer,DATA,256).fill(7);
call(OP.WRITE_BUFFER,(v)=>{put64(v,16,queue);put64(v,24,vertex);put64(v,32,0);put64(v,40,DATA);put64(v,48,64);});
call(OP.WRITE_TEXTURE,(v)=>{put64(v,16,queue);put64(v,24,texture);put64(v,32,DATA);put64(v,40,256);v.setUint32(48,256,true);v.setUint32(52,1,true);});
out=call(OP.BEGIN_ROUND_TRIP,(v)=>{put64(v,16,device);put64(v,24,queue);put64(v,32,upload);put64(v,40,readback);put64(v,48,DATA);put64(v,56,64);put64(v,64,2);}); const operation=get64(out,24);
await Promise.resolve(); await Promise.resolve(); out=call(OP.POLL_ROUND_TRIP,(v)=>{put64(v,16,operation);put64(v,24,2);put64(v,32,DATA+512);put64(v,40,64);}); assert.equal(out.getUint32(16,true),2);
host.failRoundTrip=true;
out=call(OP.BEGIN_ROUND_TRIP,(v)=>{put64(v,16,device);put64(v,24,queue);put64(v,32,upload);put64(v,40,readback);put64(v,48,DATA);put64(v,56,64);put64(v,64,4);}); const rejectedOperation=get64(out,24);
await Promise.resolve(); await Promise.resolve(); out=call(OP.POLL_ROUND_TRIP,(v)=>{put64(v,16,rejectedOperation);put64(v,24,4);put64(v,32,DATA+768);put64(v,40,64);}); assert.equal(out.getUint32(16,true),3);
call(OP.RELEASE_OPERATION,(v)=>put64(v,16,rejectedOperation)); host.failRoundTrip=false;
const wgsl="@vertex fn main() -> @builtin(position) vec4<f32> { return vec4<f32>(); }";
new TextEncoder().encodeInto(wgsl,new Uint8Array(memory.buffer,DATA+1024,wgsl.length));
const shader=()=>{const r=call(OP.CREATE_SHADER_MODULE,(v)=>{put64(v,16,device);put64(v,24,DATA+1024);v.setUint32(48,1,true);v.setUint32(52,wgsl.length,true);new Uint8Array(v.buffer,v.byteOffset+56,5).set(new TextEncoder().encode("main\0"));});return get64(r,24);};
const vs=shader(),fs=shader(); out=call(OP.CREATE_BIND_GROUP_LAYOUT,(v)=>{put64(v,16,device);put64(v,24,0);}); const bgl=get64(out,24);
out=call(OP.CREATE_PIPELINE_LAYOUT,(v)=>{put64(v,16,device);put64(v,24,0);}); const layout=get64(out,24);
out=call(OP.CREATE_PIPELINE,(v)=>{put64(v,16,device);put64(v,24,layout);put64(v,32,vs);put64(v,40,fs);v.setUint32(112,1,true);v.setUint32(116,2,true);
  v.setUint32(124,3,true);v.setUint32(160,0x3f800000,true);v.setUint32(236,3,true);v.setUint32(268,1,true);v.setUint32(276,1,true);
  v.setUint32(280,1,true);v.setUint32(284,1,true);v.setUint32(288,1,true);v.setUint32(296,0,true);v.setUint32(300,12,true);
  v.setUint32(552,0,true);v.setUint32(556,0,true);v.setUint32(560,19,true);v.setUint32(836,15,true);v.setUint32(840,1,true);}); const pipeline=get64(out,24);
out=call(OP.BEGIN_ENCODER,(v)=>put64(v,16,device)); const commandEncoder=get64(out,24);
out=call(OP.BEGIN_PASS,(v)=>{put64(v,16,commandEncoder);put64(v,24,frame);v.setUint32(32,1,true);}); const pass=get64(out,24);
call(OP.RECORD_INDEXED_DRAW,(v)=>{put64(v,16,pass);put64(v,24,pipeline);put64(v,32,vertex);put64(v,40,index);put64(v,48,texture);put64(v,64,sampler);v.setBigUint64(72,0xfedcba9876543210n,true);v.setUint32(80,1,true);v.setUint32(84,1,true);v.setUint32(92,3,true);v.setUint32(96,1,true);});
call(OP.END_PASS,(v)=>put64(v,16,pass)); out=call(OP.FINISH_ENCODER,(v)=>put64(v,16,commandEncoder)); const commandBuffer=get64(out,24);
out=call(OP.SUBMIT,(v)=>{put64(v,16,queue);put64(v,24,commandBuffer);put64(v,32,3);}); const submission=get64(out,24);
call(OP.CANVAS_PRESENT,(v)=>{put64(v,16,0x7000);put64(v,24,frame);put64(v,32,1);put64(v,40,3);});
await Promise.resolve(); out=call(OP.POLL_SUBMISSION,(v)=>{put64(v,16,submission);put64(v,24,3);}); assert.equal(out.getUint32(16,true),2);
call(OP.RELEASE_OPERATION,(v)=>put64(v,16,operation));
for(const [kind,id] of [[1,vertex],[1,index],[1,upload],[1,readback],[2,texture],[3,sampler]]) call(OP.DESTROY_RESOURCE,(v)=>{v.setUint32(16,kind,true);put64(v,24,id);});
for(const [kind,id] of [[4,pipeline],[3,layout],[2,bgl],[1,fs],[1,vs]]) call(OP.DESTROY_PIPELINE_OBJECT,(v)=>{v.setUint32(16,kind,true);put64(v,24,id);});
call(OP.RELEASE_COMMAND_OBJECT,(v)=>{v.setUint32(16,4,true);put64(v,24,submission);});
call(OP.CANVAS_UNCONFIGURE,(v)=>put64(v,16,0x7000));
const sentinel=new Uint8Array(memory.buffer,RESPONSE,24); call(OP.WRITE_BUFFER,(v)=>{put64(v,16,queue);put64(v,24,0x7fffffff);put64(v,40,DATA);put64(v,48,4);},0); assert.ok(sentinel.every(v=>v===0xa5));
assert.equal(bridge.dispatch(OP.BEGIN_ADAPTER,memory.buffer.byteLength-8,24,RESPONSE,24),0);
call(OP.RELEASE_DEVICE,(v)=>{put64(v,16,device);put64(v,24,queue);});
call(OP.RELEASE_ADAPTER,(v)=>put64(v,16,adapter));
assert.deepEqual([...seen].sort((a,b)=>a-b),Array.from({length:33},(_,i)=>i+1));
assert.equal(host.log.at(-1),"destroy");
console.log("ral WebGPU browser linear-memory dispatch contract: PASS");
