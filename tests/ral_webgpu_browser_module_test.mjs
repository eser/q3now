// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import assert from "node:assert/strict";
import { createRalWebGpuBrowserModule, recreateRalWebGpuBrowserModule,
  RAL_WEBGPU_BROWSER_DEFAULT_MODE } from "../code/render/ral/backends/webgpu/ral_webgpu_browser_module.mjs";
import { RAL_WEBGPU_BROWSER_ABI_SCHEMA_VERSION as ABI }
  from "../code/render/ral/backends/webgpu/ral_webgpu_browser_dispatch.mjs";

const memory = new WebAssembly.Memory({ initial: 2 });
const canvas = { width:0, height:0, style:{}, getContext(){ return null; } };
const events = [];
const beginAdapter = (generation) => { const request=new DataView(memory.buffer,0x1000,24);
	  request.setUint32(0,ABI,true);request.setUint32(4,1,true);request.setUint32(8,24,true);
  request.setBigUint64(16,BigInt(generation),true);
  assert.equal(globalThis.wiredRalWebGpuDispatch(1,0x1000,24,0x2000,24),1); };
const factory = async ({ noInitialRun }) => { assert.equal(noInitialRun,true); events.push("factory");
  return { wasmMemory:memory, _RalWebGpu_BrowserModuleStart(generation,canvasIdentity,width,height) {
    assert.equal(typeof globalThis.wiredRalWebGpuDispatch,"function"); events.push("start");
    assert.ok(generation>0 && canvasIdentity>0); assert.equal(width,1280); assert.equal(height,720);
    beginAdapter(generation); return 1;
  }, _RalWebGpu_BrowserModulePoll(){ return 2; },
  _GetRefAPI(){}, _WiredWebGpu_RendererPoll(){ return 2; },
  _WiredWebGpu_RendererResize(width,height){ canvas.width=width;canvas.height=height;return 1; },
  _RalWebGpu_BrowserModuleStop(){ events.push("stop"); } }; };

const first = await createRalWebGpuBrowserModule({ moduleFactory:factory, canvas,
  generation:7, canvasIdentity:0x7000 });
assert.deepEqual(first.mode,RAL_WEBGPU_BROWSER_DEFAULT_MODE);
assert.equal(canvas.width,1280); assert.equal(canvas.height,720);
assert.deepEqual(events,["factory","start"]);
assert.equal(first.rendererPoll(),2);
assert.equal(first.resize({width:1600,height:900}),true);
assert.equal(canvas.width,1600);assert.equal(canvas.height,900);
await Promise.resolve(); await Promise.resolve();
assert.equal(first.capability().status,"unsupported");
assert.match(first.capability().reason,/WebGPU is unavailable/);
const second = await recreateRalWebGpuBrowserModule(first,{ moduleFactory:factory,
  canvas, canvasIdentity:0x7000 });
assert.deepEqual(events,["factory","start","stop","factory","start"]);
await Promise.resolve(); await Promise.resolve();
assert.equal(second.capability().generation,8); assert.equal(second.capability().status,"unsupported"); second.destroy();
assert.equal(globalThis.wiredRalWebGpuDispatch,undefined);

const rejected = await createRalWebGpuBrowserModule({ moduleFactory:async()=>({
	wasmMemory:memory,_RalWebGpu_BrowserModulePoll(){return 0;},
	_GetRefAPI(){},_WiredWebGpu_RendererPoll(){return 2;},
	_WiredWebGpu_RendererResize(){return 0;},
	_RalWebGpu_BrowserModuleStart(){return 0;} }),canvas,generation:9 });
assert.equal(rejected.capability().status,"failed");
assert.match(rejected.capability().reason,/rejected startup/);
assert.equal(globalThis.wiredRalWebGpuDispatch,undefined);
await assert.rejects(()=>createRalWebGpuBrowserModule({moduleFactory:async()=>({wasmMemory:memory}),canvas,generation:10}),
  /renderer module exports are unavailable/);
console.log("ral WebGPU browser module bootstrap contract: PASS");
