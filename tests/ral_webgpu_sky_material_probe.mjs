// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
export async function verifySkyMaterial(runtime) {
  const check=(ok,message)=>{if(!ok)throw new Error(message);};
  const wait=async poll=>{
    let status=1;
    for(let i=0;i<240&&status===1;++i){status=poll();if(status===1)await new Promise(r=>setTimeout(r,16));}
    check(status===2,`sky poll=${status}: ${runtime.lastDispatchError()}; ${JSON.stringify(runtime.module.wiredWebModuleLog)}`);
  };
  const started=runtime.module._WiredWebGpu_RendererSmokeBegin();
  check(started===1,`sky fixture registration failed (${started}): ${runtime.lastDispatchError()}; ${JSON.stringify(runtime.module.wiredWebModuleLog)}`);
  await wait(()=>runtime.rendererPoll());
  const cases=Array.from({length:8},(_,i)=>({count:i+1,time:0,loop:1,reload:i===0?1:0}));
  for(const loop of [1,0])for(const time of [499,500,1000,1500,1500,0])
    cases.push({count:8,time,loop,reload:0});
  cases.push({count:8,time:500,loop:1,reload:1});
  const samples=[],captured=[],refreshed=[],directions=[],physicalCapture=[],tierTransitions=[];
  try {
    for(const c of cases) {
      const capture=runtime.host.captureNextCanvas(runtime.capability().generation);
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeFrame(c.count,c.time,c.loop,c.reload,0)===1,
        `sky frame rejected ${JSON.stringify(c)}: ${runtime.lastDispatchError()}`);
      await wait(()=>runtime.module._WiredWebGpu_SkyMaterialSmokePoll());
      const image=await capture;
      check(image.width===1280&&image.height===720,"sky capture extent mismatch");
      // Binary RGB is unchanged by sRGB decoding. Alpha contributes 16/255;
      // the animated last stage contributes white 32/255 on its second frame.
      const expected=[0,0,0];
      for(let i=0;i<c.count;++i){
        const animated=i===7&&(c.loop?Math.floor(c.time/500)%2:Math.min(Math.floor(c.time/500),1))===1;
        if(animated)for(let ch=0;ch<3;++ch)expected[ch]+=32;
        else expected[i%3]+=16;
      }
      let maxError=0;
      // Inspect every pixel, not only a central point or average.
      for(let p=0;p<image.rgba.length;p+=4)for(let ch=0;ch<3;++ch)
        maxError=Math.max(maxError,Math.abs(image.rgba[p+ch]-expected[ch]));
      check(maxError<=1,`sky ${JSON.stringify(c)} maxError=${maxError}; expected=${expected}; center=${image.rgba.slice((360*1280+640)*4,(360*1280+640)*4+3)}`);
      samples.push({...c,expected,maxError});
    }
    // A new world starts a one-face-per-frame authored radiance capture.
    // Its last animation stage crosses 1s while the six-face cohort is still
    // being recorded: every completed face must retain the 500ms recipe.
    let initialized=0;
    for(let step=0;step<6;++step) {
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeFrame(8,500+step*100,1,step===0?1:0,1)===1,
        `radiance frame ${step} rejected: ${runtime.lastDispatchError()}; ${JSON.stringify(runtime.module.wiredWebModuleLog)}`);
      await wait(()=>runtime.module._WiredWebGpu_SkyMaterialSmokePoll());
      const mask=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(1);
      check(mask===(1<<step)&&runtime.module._WiredWebGpu_SkyMaterialSmokeSource(2)===1,
        `radiance capture step=${step} mask=${mask} must draw one face`);
      initialized|=mask;
      const source=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(0);
      const maxError=await verifyCapturedFaces(runtime,source,initialized,[80,64,64]);
      captured.push({step,mask,initialized,maxError});
    }
    check(initialized===63,"radiance did not capture all six faces");
    // Finish SH/prefilter/publication while time is held. Advancing to a
    // different animMap frame must refresh, without reload or source edits.
    for(let step=0;step<40;++step) {
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeFrame(8,1000,1,0,1)===1,"held sky frame rejected");
      await wait(()=>runtime.module._WiredWebGpu_SkyMaterialSmokePoll());
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeSource(1)===0,"held clock recaptured sky");
    }
    initialized=0;
    for(let step=0;step<6;++step) {
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeFrame(8,2000+step*100,1,0,1)===1,"refresh frame rejected");
      await wait(()=>runtime.module._WiredWebGpu_SkyMaterialSmokePoll());
      const mask=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(1);
      check(mask===(1<<step),`animated radiance did not refresh: step=${step} mask=${mask}`);
      initialized|=mask;
      const maxError=await verifyCapturedFaces(runtime,runtime.module._WiredWebGpu_SkyMaterialSmokeSource(0),initialized,[48,48,32]);
      refreshed.push({step,mask,initialized,maxError});
    }
    // Static stages must not opt into periodic work even as time advances.
    let staticFaces=0;
    for(let step=0;step<64;++step) {
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeFrame(1,3000+step*100,1,step===0?1:0,1)===1,"static sky frame rejected");
      await wait(()=>runtime.module._WiredWebGpu_SkyMaterialSmokePoll());
      const mask=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(1);
      check(!(mask&staticFaces),"static sky recaptured despite immutable stages");
      staticFaces|=mask;
    }
    check(staticFaces===63,"static sky missed its initial capture");
    initialized=0;
    for(let step=0;step<6;++step) {
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeFrame(3,1000,1,step===0?1:0,2)===1,
        `direction frame ${step} rejected: ${runtime.lastDispatchError()}; ${JSON.stringify(runtime.module.wiredWebModuleLog)}`);
      await wait(()=>runtime.module._WiredWebGpu_SkyMaterialSmokePoll());
      const mask=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(1);
      check(mask===(1<<step)&&runtime.module._WiredWebGpu_SkyMaterialSmokeSource(2)===1,
        `direction capture step=${step} mask=${mask}`);
      initialized|=mask;
      const source=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(0);
      const maxError=await verifyCapturedFaces(runtime,source,initialized,(face,x,y)=>{
        const u=2*(x+.5)/128-1,v=2*(y+.5)/128-1;
        // Independent GL cube convention: +X,-X,+Y,-Y,+Z,-Z.
        const xyz=[[1,-v,-u],[-1,-v,u],[u,1,v],[u,-1,-v],[u,-v,1],[-u,-v,-1]][face];
        return xyz.map(c=>Math.round(255*(.5+.5*c)));
      });
      directions.push({step,mask,initialized,maxError});
    }
    check(initialized===63,"direction capture did not cover all faces");
    initialized=0;
    for(let frame=0;frame<16&&initialized!==63;++frame) {
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeFrame(3,1000,1,frame===0?1:0,3)===1,
        `physical capture frame ${frame} rejected: ${runtime.lastDispatchError()}; ${JSON.stringify(runtime.module.wiredWebModuleLog)}`);
      await wait(()=>runtime.module._WiredWebGpu_SkyMaterialSmokePoll());
      const mask=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(1);
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeSource(6)===0,"media-free physical sky dispatched froxel work");
      if(!mask)continue;
      check(runtime.module._WiredWebGpu_SkyMaterialSmokeSource(5)!==0,"capture preceded physical SKY_VIEW completion");
      const faceCount=mask.toString(2).replaceAll("0", "").length;
      check(!(mask&initialized)&&faceCount<=4&&runtime.module._WiredWebGpu_SkyMaterialSmokeSource(2)===faceCount,
        `physical capture mask=${mask} after ${initialized}`);
      initialized|=mask;
      const source=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(0);
      const maxError=await verifyCapturedFaces(runtime,source,initialized,null,{
        texture:runtime.module._WiredWebGpu_SkyMaterialSmokeSource(3),
        sampler:runtime.module._WiredWebGpu_SkyMaterialSmokeSource(4)});
      physicalCapture.push({frame,mask,initialized,maxError});
    }
    check(initialized===63,"physical capture did not complete six faces within its budget");
    // Finish the real GPU product, then keep the declaration and source
    // fixed while changing tier. Readable residency must survive withdrawal;
    // the module's frame consumer (which controls its uniform) must not.
    for(const mode of [3,4,5,3]) {
      const frames=mode===3?32:8;
      for(let frame=0;frame<frames;++frame) {
        check(runtime.module._WiredWebGpu_SkyMaterialSmokeFrame(3,1000,1,0,mode)===1,
          `sky tier mode=${mode} frame=${frame} rejected`);
        await wait(()=>runtime.module._WiredWebGpu_SkyMaterialSmokePoll());
        if(mode!==3 || frame===frames-1) {
          const admitted=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(7);
          const resident=runtime.module._WiredWebGpu_SkyMaterialSmokeSource(8);
          check(resident===1 && admitted===(mode===3?1:0),
            `sky tier mode=${mode} frame=${frame} resident=${resident} admitted=${admitted}`);
          if(mode!==3)check(runtime.module._WiredWebGpu_SkyMaterialSmokeSource(1)===0,
            "disabled sky tier performed a source capture");
          if(frame===frames-1)tierTransitions.push({mode,frames,resident,admitted});
        }
      }
    }
  } finally {runtime.module._WiredWebGpu_ExposureSmokeEnd();}
  return {frames:samples.length,width:1280,height:720,scope:"production sky -> HDR -> identity output",samples,radianceCapture:captured,refreshedCapture:refreshed,staticFrames:64,directionCapture:directions,physicalCapture,tierTransitions};
}

// Read the production source array through a trivial textureLoad shader.
// Every texel of every completed face is sampled; no shared sky evaluator in
// the oracle. Expected may be a constant RGB or an independent face/texel
// reference; the latter detects orientation as well as layer selection.
async function verifyCapturedFaces(runtime,source,mask,expected,physical=null) {
  if(!source)throw new Error("missing production radiance source");
  const host=runtime.host,g=runtime.capability().generation,owned=[];
  const keep=id=>{owned.push(id);return id;};
  try {
    const entries=[{binding:0,visibility:GPUShaderStage.FRAGMENT,texture:{sampleType:"float",viewDimension:"2d-array"}}];
    if(physical)entries.push({binding:1,visibility:GPUShaderStage.FRAGMENT,texture:{sampleType:"float"}},
      {binding:2,visibility:GPUShaderStage.FRAGMENT,sampler:{type:"filtering"}});
    const layout=keep(host.createBindGroupLayout(g,{entries}));
    const pipelineLayout=keep(host.createPipelineLayout(g,[layout]));
    const shader=keep(host.createShaderModule(g,{code:`
      @group(0) @binding(0) var source:texture_2d_array<f32>;
      ${physical?`@group(0) @binding(1) var lut:texture_2d<f32>;
      @group(0) @binding(2) var lutSampler:sampler;`:''}
      @vertex fn vs(@location(0) p:vec2<f32>)->@builtin(position) vec4<f32>{return vec4<f32>(p,0,1);}
      @fragment fn fs(@builtin(position) p:vec4<f32>)->@location(0) vec4<f32>{
        ${physical?`
        // Two 3x2 mosaics: production source at left, independent reference
        // at right. All 128x128 texels per face are represented in each.
        let x=p.x%640.0; let row=i32(p.y/360.0);
        let face=i32(x*3.0/640.0)+row*3;
        let texel=vec2<i32>(i32(fract(x*3.0/640.0)*128.0),i32((p.y%360.0)*128.0/360.0));
        if(p.x<640.0){return textureLoad(source,texel,face,0);}
        let u=2.0*(f32(texel.x)+.5)/128.0-1.0; let v=2.0*(f32(texel.y)+.5)/128.0-1.0;
        var d=vec3<f32>(1,-v,-u);
        switch face {case 1:{d=vec3<f32>(-1,-v,u);}case 2:{d=vec3<f32>(u,1,v);}
          case 3:{d=vec3<f32>(u,-1,-v);}case 4:{d=vec3<f32>(u,-v,1);}
          case 5:{d=vec3<f32>(-u,-v,-1);}default:{}}
        let authored=.5+.5*d; let n=normalize(d);
        let uv=vec2<f32>(atan2(n.y,n.x)/6.28318530718+.5,asin(n.z)/3.14159265359+.5);
        let light=textureSampleLevel(lut,lutSampler,uv,0).rgb;
        let weights=vec3<f32>(.2126,.7152,.0722);
        let a=dot(authored,weights); let e=dot(max(light,vec3<f32>(0)),weights);
        let tint=select(vec3<f32>(1),light/max(e,.00001),e>.00001);
        let result=authored*clamp(e/max(a,.18),.65,1.75)*mix(vec3<f32>(1),clamp(tint,vec3<f32>(.5),vec3<f32>(2)),.12)+light*.08;
        return vec4<f32>(result,1);
        `:`
        let face=i32(p.x*6.0/1280.0);
        let uv=vec2<i32>(i32(fract(p.x*6.0/1280.0)*128.0),i32(p.y*128.0/720.0));
        return textureLoad(source,uv,face,0);
        `}
      }`}));
    const pipeline=keep(host.createRenderPipelineFromHandles(g,{layoutHandle:pipelineLayout,
      vertex:{moduleHandle:shader,entryPoint:"vs",buffers:[{arrayStride:8,attributes:[{shaderLocation:0,offset:0,format:"float32x2"}]}]},
      fragment:{moduleHandle:shader,entryPoint:"fs",targets:[{format:host.receipt(g).canvasFormat}]},primitive:{topology:"triangle-list"}}));
    const bindings=[{binding:0,kind:2,handle:source,offset:0,size:0}];
    if(physical)bindings.push({binding:1,kind:2,handle:physical.texture,offset:0,size:0},
      {binding:2,kind:3,handle:physical.sampler,offset:0,size:0});
    const group=keep(host.createBindGroupFromHandles(g,layout,bindings));
    const vertex=keep(host.createBuffer(g,{size:24,usage:GPUBufferUsage.VERTEX|GPUBufferUsage.COPY_DST}));
    const index=keep(host.createBuffer(g,{size:12,usage:GPUBufferUsage.INDEX|GPUBufferUsage.COPY_DST}));
    host.writeBuffer(g,vertex,0,new Float32Array([-1,-1,3,-1,-1,3]));
    host.writeBuffer(g,index,0,new Uint32Array([0,1,2]));
    const capture=host.captureNextCanvas(g),frame=host.acquireCanvasTexture(g);
    const encoder=keep(host.createCommandEncoder(g));
    const pass=host.beginRenderPass(g,encoder,frame.viewHandle,{depthEnabled:false});
    host.recordIndexedDraw(g,pass,{pipelineHandle:pipeline,vertexBufferHandle:vertex,indexBufferHandle:index,
      indexCount:3,bindGroups:[{index:0,handle:group}]});
    host.endRenderPass(g,pass);
    const commands=keep(host.finishEncoder(g,encoder)),submit=keep(host.submit(g,commands));
    host.presentCanvas(g,frame.textureHandle);
    const image=await capture;
    let status="pending";
    for(let i=0;i<240&&status==="pending";++i){status=host.pollSubmission(g,submit).status;
      if(status==="pending")await new Promise(r=>setTimeout(r,16));}
    if(status!=="ready")throw new Error(`radiance sample submit=${status}`);
    let maxError=0,worst=null,physicalChange=0;
    if(physical) {
      for(let y=0;y<720;++y)for(let x=0;x<640;++x) {
        const face=Math.floor((x+.5)*3/640)+3*Math.floor((y+.5)/360);
        if(!(mask&(1<<face)))continue;
        const fx=(x+.5)*3/640,tx=Math.floor((fx-Math.floor(fx))*128),ty=Math.floor(((y+.5)%360)*128/360);
        const u=2*(tx+.5)/128-1,v=2*(ty+.5)/128-1;
        const d=[[1,-v,-u],[-1,-v,u],[u,1,v],[u,-1,-v],[u,-v,1],[-u,-v,-1]][face];
        for(let c=0;c<3;++c) {
          const actual=image.rgba[(y*1280+x)*4+c],reference=image.rgba[(y*1280+x+640)*4+c];
          physicalChange=Math.max(physicalChange,Math.abs(reference-Math.round(255*(.5+.5*d[c]))));
          const error=Math.abs(actual-reference);
          if(error>maxError){maxError=error;worst={face,x,y,c,actual,reference};}
        }
      }
    } else {
    for(let y=0;y<720;++y)for(let x=0;x<1280;++x) {
      const facePosition=(x+.5)*6/1280,face=Math.floor(facePosition);
      if(!(mask&(1<<face)))continue;
      const rgb=typeof expected==="function"?expected(face,
        Math.floor((facePosition-face)*128),Math.floor((y+.5)*128/720)):expected;
      for(let ch=0;ch<3;++ch)maxError=Math.max(maxError,Math.abs(image.rgba[(y*1280+x)*4+ch]-rgb[ch]));
    }
    }
    if(physical && physicalChange<=10)throw new Error("physical oracle did not meaningfully differ from authored sky");
    if(maxError>1)throw new Error(`production radiance mask=${mask} maxError=${maxError} expected=${expected} worst=${JSON.stringify(worst)}`);
    return maxError;
  } finally {for(const id of owned.reverse())host.release(g,id);}
}
