// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Real browser-host render-pass test, not the production sky material oracle.
// Distinct face/mip clears plus a single-subresource rewrite must survive
// sampling the complete array. No copy-to-an-intermediate render target.
export async function verifyRenderSubresources(runtime) {
  const host=runtime.host, g=runtime.capability().generation, owned=[];
  const keep=id=>{owned.push(id);return id;};
  const check=(ok,msg)=>{if(!ok)throw new Error(msg);};
  const colors=Array.from({length:18},(_,i)=>[(i+1)/32,(18-i)/32,(i%3+1)/4]);
  host.configureCanvas(g,{cssWidth:1280,cssHeight:720,dpr:1});
  try {
    const cube=keep(host.createTexture(g,{size:{width:128,height:128,depthOrArrayLayers:6},
      mipLevelCount:3,format:"rgba16float",usage:GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.TEXTURE_BINDING}));
    const layout=keep(host.createBindGroupLayout(g,{entries:[{binding:0,visibility:GPUShaderStage.FRAGMENT,
      texture:{sampleType:"float",viewDimension:"2d-array"}}]}));
    const pipelineLayout=keep(host.createPipelineLayout(g,[layout]));
    const shader=keep(host.createShaderModule(g,{code:`
      @group(0) @binding(0) var source:texture_2d_array<f32>;
      @vertex fn vs(@location(0) p:vec2<f32>)->@builtin(position) vec4<f32>{return vec4<f32>(p,0,1);}
      @fragment fn fs(@builtin(position) p:vec4<f32>)->@location(0) vec4<f32>{
        let layer=i32(p.x*6.0/1280.0); let mip=i32(p.y*3.0/720.0);
        return textureLoad(source,vec2<i32>(0),layer,mip);
      }`}));
    const pipeline=keep(host.createRenderPipelineFromHandles(g,{layoutHandle:pipelineLayout,
      vertex:{moduleHandle:shader,entryPoint:"vs",buffers:[{arrayStride:8,attributes:[{shaderLocation:0,offset:0,format:"float32x2"}]}]},
      fragment:{moduleHandle:shader,entryPoint:"fs",targets:[{format:host.receipt(g).canvasFormat}]},
      primitive:{topology:"triangle-list"}}));
    const group=keep(host.createBindGroupFromHandles(g,layout,[{binding:0,kind:2,handle:cube,offset:0,size:0}]));
    const vertex=keep(host.createBuffer(g,{size:24,usage:GPUBufferUsage.VERTEX|GPUBufferUsage.COPY_DST}));
    const index=keep(host.createBuffer(g,{size:12,usage:GPUBufferUsage.INDEX|GPUBufferUsage.COPY_DST}));
    host.writeBuffer(g,vertex,0,new Float32Array([-1,-1,3,-1,-1,3]));
    host.writeBuffer(g,index,0,new Uint32Array([0,1,2]));
    const frames=[];
    for(let cycle=0;cycle<2;++cycle) {
      if(cycle) colors[9]=[0.875,0.125,0.625];
      const capture=host.captureNextCanvas(g), frame=host.acquireCanvasTexture(g);
      const encoder=host.createCommandEncoder(g);
      for(let n=0;n<18;++n) {
        if(cycle&&n!==9)continue;
        const [r,green,b]=colors[n];
        const pass=host.beginRenderPass(g,encoder,cube,{depthEnabled:false,
          targetMipLevel:Math.floor(n/6),targetArrayLayer:n%6,clearValue:{r,g:green,b,a:1}});
        host.endRenderPass(g,pass);
      }
      const output=host.beginRenderPass(g,encoder,frame.viewHandle,{depthEnabled:false});
      host.recordIndexedDraw(g,output,{pipelineHandle:pipeline,vertexBufferHandle:vertex,indexBufferHandle:index,
        indexCount:3,bindGroups:[{index:0,handle:group}]});
      host.endRenderPass(g,output);
      const commands=host.finishEncoder(g,encoder), submission=host.submit(g,commands);
      host.presentCanvas(g,frame.textureHandle);
      const image=await capture;
      let status;
      for(let i=0;i<240;++i){status=host.pollSubmission(g,submission).status;if(status!=="pending")break;
        await new Promise(resolve=>setTimeout(resolve,16));}
      check(status==="ready",`render subresource GPU status=${status}`);
      check(image.width===1280&&image.height===720,"render subresource capture extent");
      let maxError=0;
      for(let y=0;y<720;++y)for(let x=0;x<1280;++x) {
        const expected=colors[Math.floor(y/240)*6+Math.floor((x+.5)*6/1280)];
        for(let c=0;c<3;++c)maxError=Math.max(maxError,
          Math.abs(image.rgba[(y*1280+x)*4+c]-Math.round(expected[c]*255)));
      }
      check(maxError<=1,`render subresource cycle=${cycle} maxError=${maxError}`);
      frames.push({cycle,maxError,rewrittenSubresources:cycle?1:18});
      host.release(g,submission);host.release(g,commands);host.release(g,encoder);
    }
    return {layers:6,mips:3,width:1280,height:720,scope:"browser-host array attachment selection and preservation",frames};
  } finally {for(const id of owned.reverse())host.release(g,id);}
}
