// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// Canonical production WGSL fragment, synthetic asymmetric face pixels and an
// independent face-local bilinear oracle. This is not full map registration.
export async function verifySkyAtlas(runtime) {
  const host=runtime.host, g=runtime.capability().generation, owned=[];
  const keep=id=>{owned.push(id);return id;};
  const check=(ok,msg)=>{if(!ok)throw new Error(msg);};
  host.configureCanvas(g,{cssWidth:1280,cssHeight:720,dpr:1});
  try {
    const response=await fetch('../code/render/ral/backends/vulkan/renderer/shaders/portable/ral_sky_material_frag_spv.wgsl');
    check(response.ok,'canonical sky WGSL missing');
    const fragment=keep(host.createShaderModule(g,{code:await response.text()}));
    const vertexShader=keep(host.createShaderModule(g,{code:`
      struct V { @builtin(position) p:vec4<f32>, @location(0) uv:vec2<f32>,
        @location(1) lm:vec2<f32>, @location(2) color:vec4<f32>,
        @location(3) world:vec3<f32>, @location(4) normal:vec3<f32> }
      @vertex fn main(@location(0) p:vec2<f32>, @location(1) ray:vec3<f32>)->V {
        var o:V; o.p=vec4<f32>(p,0,1); o.uv=vec2<f32>(0); o.lm=vec2<f32>(0);
        o.color=vec4<f32>(1); o.world=ray; o.normal=vec3<f32>(0,0,1); return o;
      }`}));
    const uniformLayout=(binding,size)=>keep(host.createBindGroupLayout(g,{entries:[{
      binding,visibility:GPUShaderStage.FRAGMENT,buffer:{type:'uniform',minBindingSize:size}}]}));
    const drawLayout=uniformLayout(0,256), skyLayout=uniformLayout(1,3616);
    const textureLayout=keep(host.createBindGroupLayout(g,{entries:Array.from({length:20},(_,binding)=>({
      binding,visibility:GPUShaderStage.FRAGMENT,...(binding%2?{sampler:{type:'filtering'}}:{texture:{sampleType:'float'}})}))}));
    const layout=keep(host.createPipelineLayout(g,[drawLayout,textureLayout,skyLayout]));
    const pipeline=keep(host.createRenderPipelineFromHandles(g,{layoutHandle:layout,
      vertex:{moduleHandle:vertexShader,entryPoint:'main',buffers:[{arrayStride:20,attributes:[
        {shaderLocation:0,offset:0,format:'float32x2'},{shaderLocation:1,offset:8,format:'float32x3'}]}]},
      fragment:{moduleHandle:fragment,entryPoint:'main',targets:[{format:host.receipt(g).canvasFormat}]},
      primitive:{topology:'triangle-list'}}));
    const buffer=(data,usage)=>{const id=keep(host.createBuffer(g,{size:data.byteLength,usage:usage|GPUBufferUsage.COPY_DST}));
      host.writeBuffer(g,id,0,data);return id;};
    const draw=new Float32Array(64); draw[7]=draw[11]=draw[15]=draw[35]=1;
    const sky=new Uint32Array(904); sky[1]=1;
    const drawBuffer=buffer(draw,GPUBufferUsage.UNIFORM), skyBuffer=buffer(sky,GPUBufferUsage.UNIFORM);
    const group=(l,entries)=>keep(host.createBindGroupFromHandles(g,l,entries));
    const drawGroup=group(drawLayout,[{binding:0,kind:1,handle:drawBuffer,offset:0,size:256}]);
    const skyGroup=group(skyLayout,[{binding:1,kind:1,handle:skyBuffer,offset:0,size:3616}]);
    const pixels=new Uint8Array(12*8*4);
    for(let f=0;f<6;++f)for(let y=0;y<4;++y)for(let x=0;x<4;++x) {
      const offset=((Math.floor(f/3)*4+y)*12+(f%3)*4+x)*4;
      pixels.set([f*24+x*8,f*16+y*12,f*8+x*4+y*6,255],offset);
    }
    const atlas=keep(host.createTexture(g,{size:{width:12,height:8,depthOrArrayLayers:1},format:'rgba8unorm',
      usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST}));
    host.writeTexture(g,atlas,pixels,{bytesPerRow:48,rowsPerImage:8},{width:12,height:8,depthOrArrayLayers:1});
    const sampler=keep(host.createSampler(g,{minFilter:'linear',magFilter:'linear',addressModeU:'clamp-to-edge',addressModeV:'clamp-to-edge'}));
    const textureGroup=group(textureLayout,Array.from({length:20},(_,binding)=>({
      binding,kind:binding%2?3:2,handle:binding%2?sampler:atlas,offset:0,size:0})));
    const verts=[], indices=[];
    for(let f=0;f<6;++f) {
      for(const [s,t] of [[0,0],[1,0],[0,1],[1,1]]) {
        const u=2*s-1,v=2*t-1;
        const ray=[[1,-u,-v],[u,1,-v],[-1,u,-v],[-u,-1,-v],[v,-u,1],[-v,-u,-1]][f];
        verts.push((f+s)/3-1,1-2*t,...ray);
      }
      indices.push(...[0,2,1,1,2,3].map(i=>i+f*4));
    }
    const vertices=buffer(new Float32Array(verts),GPUBufferUsage.VERTEX);
    const index=buffer(new Uint32Array(indices),GPUBufferUsage.INDEX);
    const capture=host.captureNextCanvas(g), frame=host.acquireCanvasTexture(g);
    const encoder=keep(host.createCommandEncoder(g));
    const pass=host.beginRenderPass(g,encoder,frame.viewHandle,{depthEnabled:false});
    host.recordIndexedDraw(g,pass,{pipelineHandle:pipeline,vertexBufferHandle:vertices,indexBufferHandle:index,
      indexCount:36,bindGroups:[drawGroup,textureGroup,skyGroup].map((handle,index)=>({index,handle}))});
    host.endRenderPass(g,pass);
    const commands=keep(host.finishEncoder(g,encoder)), submission=keep(host.submit(g,commands));
    host.presentCanvas(g,frame.textureHandle);
    const image=await capture;
    let status;
    for(let n=0;n<240;++n){status=host.pollSubmission(g,submission).status;if(status!=='pending')break;
      await new Promise(resolve=>setTimeout(resolve,16));}
    check(status==='ready',`sky atlas submission=${status}`);
    check(image.width===1280&&image.height===720,'sky atlas capture extent');
    let maxError=0;
    for(let y=0;y<720;++y)for(let x=0;x<1280;++x) {
      const position=(x+.5)*6/1280, f=Math.floor(position);
      const sx=Math.min(3,Math.max(0,(position-f)*4-.5));
      const sy=Math.min(3,Math.max(0,(y+.5)*4/720-.5));
      const expected=[f*24+sx*8,f*16+sy*12,f*8+sx*4+sy*6];
      for(let c=0;c<3;++c)maxError=Math.max(maxError,Math.abs(image.rgba[(y*1280+x)*4+c]-Math.round(expected[c])));
    }
    check(maxError<=1,`sky atlas face orientation/edge bilinear maxError=${maxError}`);
    return {faces:6,faceSize:4,pixels:1280*720,maxError,scope:'canonical WGSL outer-atlas orientation and texel-clamped bilinear edges'};
  } finally {for(const id of owned.reverse())host.release(g,id);}
}
