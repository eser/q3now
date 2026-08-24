// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import { createRalWebGpuBrowserHost } from
  "../code/render/ral/backends/webgpu/ral_webgpu_browser_host.mjs";

const result = document.querySelector("#result"), canvas = document.querySelector("#wired-canvas");
const publish = (status, detail) => { result.dataset.status = status; result.textContent = `${status}\n${JSON.stringify(detail,null,2)}`; };
try {
  const host = createRalWebGpuBrowserHost({ gpu:navigator.gpu, canvas, devicePixelRatio:()=>1 });
  const ready = await host.initialize(7);
  if (!ready.ready) { publish("UNSUPPORTED",{ reason:ready.reason,userAgent:navigator.userAgent }); }
  else {
    const usage=GPUBufferUsage, vertex=host.createBuffer(7,{size:24,usage:usage.VERTEX|usage.COPY_DST});
    const index=host.createBuffer(7,{size:8,usage:usage.INDEX|usage.COPY_DST});
    host.writeBuffer(7,vertex,0,new Float32Array([-0.6,-0.5,0.6,-0.5,0,0.6]));
    host.writeBuffer(7,index,0,new Uint16Array([0,1,2,0]));
    const vs=host.createShaderModule(7,{code:"@vertex fn main(@location(0) p: vec2<f32>) -> @builtin(position) vec4<f32> { return vec4<f32>(p,0.0,1.0); }"});
    const fs=host.createShaderModule(7,{code:"@fragment fn main() -> @location(0) vec4<f32> { return vec4<f32>(0.15,0.65,1.0,1.0); }"});
    const layout=host.createPipelineLayout(7,[]);
    const pipeline=host.createRenderPipelineFromHandles(7,{layoutHandle:layout,
      vertex:{moduleHandle:vs,entryPoint:"main",buffers:[{arrayStride:8,attributes:[{shaderLocation:0,offset:0,format:"float32x2"}]}]},
      fragment:{moduleHandle:fs,entryPoint:"main",targets:[{format:ready.canvasFormat}]},
      primitive:{topology:"triangle-list"}});
    const configured=host.configureCanvas(7,{cssWidth:1280,cssHeight:720,dpr:1});
    const frame=host.acquireCanvasTexture(7),encoder=host.createCommandEncoder(7);
    const pass=host.beginRenderPass(7,encoder,frame.textureHandle);
    host.recordIndexedDraw(7,pass,{pipelineHandle:pipeline,vertexBufferHandle:vertex,
      indexBufferHandle:index,indexFormat:"uint16",indexCount:3});
    host.endRenderPass(7,pass); const command=host.finishEncoder(7,encoder);
    const submission=host.submit(7,command); host.presentCanvas(7,frame.textureHandle);
    for(let i=0;i<120 && host.pollSubmission(7,submission).status==="pending";++i)
      await new Promise(resolve=>setTimeout(resolve,16));
    const completion=host.pollSubmission(7,submission);
    const resized=host.configureCanvas(7,{cssWidth:640,cssHeight:360,dpr:2});
    if(completion.status!=="ready" || configured.pixelWidth!==1280 || configured.pixelHeight!==720
        || resized.pixelWidth!==1280 || resized.pixelHeight!==720) throw new Error("receipt mismatch");
    publish("PASS",{schemaVersion:1,generation:7,width:configured.pixelWidth,height:configured.pixelHeight,
      resizeWidth:resized.pixelWidth,resizeHeight:resized.pixelHeight,submission:completion.status,
      userAgent:navigator.userAgent});
  }
} catch(error) { publish("FAIL",{message:String(error?.message??error),stack:String(error?.stack??"")}); }
