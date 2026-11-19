// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
// Real browser-host GPU prerequisite test. This is not the renderer Product
// integration: no native GPU device access or alternate exposure shader here.
import {createRalWebGpuBrowserHost} from
  "../code/render/ral/backends/webgpu/ral_webgpu_browser_host.mjs";
const canvas=document.querySelector("#wired-canvas"), result=document.querySelector("#result");
const host=createRalWebGpuBrowserHost({gpu:navigator.gpu,canvas}), gen=1;
const publish=(status,data)=>{result.dataset.status=status;result.textContent=status+"\n"+JSON.stringify(data,null,2);};
const check=(condition,message)=>{if(!condition)throw new Error(message);};
const shaderRoot="../code/render/ral/backends/vulkan/renderer/shaders/portable/";
async function shader(name){
  const response=await fetch(shaderRoot+name+".wgsl");
  check(response.ok,"missing canonical shader: "+name);
  return host.createShaderModule(gen,{label:name,code:await response.text()});
}
const buffer=(size,usage)=>host.createBuffer(gen,{size,usage});
const layout=entries=>host.createBindGroupLayout(gen,{entries});
const group=(l,entries)=>host.createBindGroupFromHandles(gen,l,entries.map(([binding,kind,handle,size])=>
  ({binding,kind,handle,offset:0,size:size??0})));
const binding=(binding,visibility,kind,minBindingSize)=>({binding,visibility,
  ...(kind==="texture"?{texture:{sampleType:"float"}}:kind==="sampler"?{sampler:{type:"filtering"}}:
    {buffer:{type:kind,minBindingSize}})});
const compute=(moduleHandle,l)=>host.createComputePipelineFromHandles(gen,{
  layoutHandle:host.createPipelineLayout(gen,[l]),compute:{moduleHandle,entryPoint:"main"}});
const fullscreen=`struct V { @builtin(position) position:vec4<f32>, @location(0) uv:vec2<f32> }
@vertex fn main(@builtin(vertex_index) i:u32)->V {
  var p=array<vec2<f32>,3>(vec2<f32>(-1,-1),vec2<f32>(3,-1),vec2<f32>(-1,3));
  var o:V; o.position=vec4<f32>(p[i],0,1); o.uv=vec2<f32>((p[i].x+1)*0.5,(1-p[i].y)*0.5); return o;
}`;
try {
  const ready=await host.initialize(gen,{});
  if(!ready.ready){publish("UNSUPPORTED",ready);}
  else {
    host.configureCanvas(gen,{cssWidth:1280,cssHeight:720,dpr:1,format:ready.canvasFormat});
    const hdr=host.createTexture(gen,{size:{width:1280,height:720,depthOrArrayLayers:1},format:"rgba16float",
      usage:GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.TEXTURE_BINDING});
    const sampler=host.createSampler(gen,{minFilter:"linear",magFilter:"linear"});
    const hist=buffer(1024,GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST);
    const accum=buffer(4,GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST);
    const block=buffer(64,GPUBufferUsage.STORAGE|GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST);
    const dims=buffer(16,GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST);
    const delta=buffer(16,GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST);
    const vertices=buffer(16,GPUBufferUsage.VERTEX|GPUBufferUsage.COPY_DST);
    const indices=buffer(12,GPUBufferUsage.INDEX|GPUBufferUsage.COPY_DST);
    host.writeBuffer(gen,indices,0,new Uint32Array([0,1,2]));
    host.writeBuffer(gen,dims,0,new Uint32Array([1280,720,0,0]));
    host.writeBuffer(gen,delta,0,new Float32Array([1,0,0,0]));
    const histLayout=layout([binding(0,4,"texture"),binding(1,4,"sampler"),
      binding(2,4,"storage",1024),binding(3,4,"uniform",16)]);
    const reduceLayout=layout([binding(0,4,"read-only-storage",1024),binding(1,4,"storage",4),
      binding(2,4,"storage",64),binding(3,4,"uniform",16)]);
    const textureLayout=layout([binding(0,2,"texture"),binding(32,2,"sampler")]);
    const emptyLayout=layout([]), exposureLayout=layout([binding(0,2,"uniform",64)]);
    const histGroup=group(histLayout,[[0,2,hdr],[1,3,sampler],[2,1,hist,1024],[3,1,dims,16]]);
    const reduceGroup=group(reduceLayout,[[0,1,hist,1024],[1,1,accum,4],[2,1,block,64],[3,1,delta,16]]);
    const textureGroup=group(textureLayout,[[0,2,hdr],[32,3,sampler]]);
    const emptyGroup=group(emptyLayout,[]), exposureGroup=group(exposureLayout,[[0,1,block,64]]);
    const histPipeline=compute(await shader("hdr_histogram_comp_spv"),histLayout);
    const reducePipeline=compute(await shader("exposure_comp_spv"),reduceLayout);
    const vertexModule=host.createShaderModule(gen,{label:"fullscreen fixture",code:fullscreen});
    const tonePipeline=host.createRenderPipelineFromHandles(gen,{
      layoutHandle:host.createPipelineLayout(gen,[textureLayout,emptyLayout,exposureLayout]),
      vertex:{moduleHandle:vertexModule,entryPoint:"main"},
      fragment:{moduleHandle:await shader("tonemap_tonemap_frag_spv"),entryPoint:"main",
        constants:{17:4,18:1},targets:[{format:ready.canvasFormat}]},primitive:{topology:"triangle-list"}});
    const uiModule=host.createShaderModule(gen,{label:"meter exclusion fixture",code:
      "@fragment fn main(@builtin(position) p:vec4<f32>)->@location(0) vec4<f32> { if(p.x>640){discard;} return vec4<f32>(1,1,1,1); }"});
    const uiPipeline=host.createRenderPipelineFromHandles(gen,{
      layoutHandle:host.createPipelineLayout(gen,[]),vertex:{moduleHandle:vertexModule,entryPoint:"main"},
      fragment:{moduleHandle:uiModule,entryPoint:"main",targets:[{format:ready.canvasFormat}]},primitive:{topology:"triangle-list"}});
    const samples=[];
    for(const enabled of [false,true])for(const brightness of [1,1.4,5.2])for(const ui of [false,true]){
      const values=new Float32Array(16); values.set([brightness,0.5,0,0,100,100,0.01,16]);
      new Int32Array(values.buffer)[8]=enabled?1:0;
      values[9]=brightness;values[14]=1;values[15]=0.18;
      host.writeBuffer(gen,block,0,values);
      host.writeBuffer(gen,accum,0,new Float32Array([1]));
      host.writeBuffer(gen,hist,0,new Uint32Array(256));
      const frame=host.acquireCanvasTexture(gen), captured=host.captureNextCanvas(gen);
      const encoder=host.createCommandEncoder(gen);
      let pass=host.beginRenderPass(gen,encoder,hdr,{clearValue:{r:4,g:2,b:1,a:1},depthEnabled:true});
      host.endRenderPass(gen,pass);
      if(enabled){
        pass=host.beginComputePass(gen,encoder);
        for(const [pipelineHandle,handle,x,y] of [[histPipeline,histGroup,80,45],[reducePipeline,reduceGroup,1,1]])
          host.recordComputeDispatch(gen,pass,{pipelineHandle,bindGroups:[{index:0,handle}],groupCountX:x,groupCountY:y,groupCountZ:1});
        host.endComputePass(gen,pass);
      }
      pass=host.beginRenderPass(gen,encoder,frame.viewHandle,{depthEnabled:false});
      host.recordIndexedDraw(gen,pass,{pipelineHandle:tonePipeline,vertexBufferHandle:vertices,indexBufferHandle:indices,
        indexCount:3,bindGroups:[textureGroup,emptyGroup,exposureGroup].map((handle,index)=>({index,handle}))});
      host.endRenderPass(gen,pass);
      if(ui){
        pass=host.beginRenderPass(gen,encoder,frame.viewHandle,{depthEnabled:false,colorLoadOp:"load"});
        host.recordIndexedDraw(gen,pass,{pipelineHandle:uiPipeline,vertexBufferHandle:vertices,indexBufferHandle:indices,indexCount:3});
        host.endRenderPass(gen,pass);
      }
      const submitted=host.submit(gen,host.finishEncoder(gen,encoder));
      host.presentCanvas(gen,frame.textureHandle);
      const image=await captured;
      let receipt=host.pollSubmission(gen,submitted);
      for(let n=0;n<240&&receipt.status==="pending";++n){await new Promise(r=>setTimeout(r,16));receipt=host.pollSubmission(gen,submitted);}
      check(receipt.status==="ready",JSON.stringify(receipt));
      check(!host.receipt(gen).error,host.receipt(gen).error);
      // The constant HDR source occupies one histogram bin. Compare the GPU
      // output against the canonical bin/reduction/Reinhard equations (±2 LSB).
      const luma=4*0.2126+2*0.7152+0.0722;
      const bin=Math.floor(Math.min(1,Math.max(0,(Math.log2(luma)+10)/12))*255+0.5);
      const exposure=enabled?0.5/2**(-10+bin/255*12):1;
      const expected=[4,2,1].map(c=>Math.round(c*exposure*brightness/(1+c*exposure*brightness)*255));
      const offset=(360*1280+960)*4, actual=[...image.rgba.subarray(offset,offset+3)];
      check(actual.every((v,i)=>Math.abs(v-expected[i])<=2),`HDR mismatch ${JSON.stringify({enabled,brightness,ui,actual,expected})}`);
      if(ui){const p=(360*1280+320)*4;check(image.rgba.slice(p,p+3).every(v=>v===255),"UI unexpectedly metered/tonemapped");}
      samples.push({enabled,brightness,ui,actual,expected});
      host.release(gen,submitted);
    }
    publish("PASS",{width:1280,height:720,scope:"browser-host GPU prerequisite, not Product integration",samples});
  }
}catch(error){publish("FAIL",{message:String(error?.message??error),stack:String(error?.stack??"")});}
finally{host.destroy();}
