// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import createEmscriptenModule from "../build/ral_webgpu_browser_module.mjs";
import { createRalWebGpuBrowserModule } from
  "../code/render/ral/backends/webgpu/ral_webgpu_browser_module.mjs";

const result=document.querySelector("#result"),canvas=document.querySelector("#wired-canvas");
const publish=(status,detail)=>{result.dataset.status=status;result.textContent=`${status}\n${JSON.stringify(detail,null,2)}`;};
try {
  const runtime=await createRalWebGpuBrowserModule({moduleFactory:createEmscriptenModule,
    gpu:navigator.gpu,canvas,generation:7,canvasIdentity:0x7000});
  let poll=1;
  for(let i=0;i<240 && poll===1;++i){poll=runtime.rendererPoll();if(poll===1)await new Promise(resolve=>setTimeout(resolve,16));}
  const state=runtime.capability();
  if(state.status==="unsupported") publish("UNSUPPORTED",{...state,userAgent:navigator.userAgent});
  else if(poll===2 && state.status==="ready" && canvas.width===1280 && canvas.height===720){
    if(typeof runtime.module._GetRefAPI!=="function" || typeof runtime.rendererPoll!=="function")
      throw new Error("WebGPU renderer refexport boundary is unavailable");
    const smokeStarted=runtime.module._RalWebGpu_BrowserModuleSmokeBegin();let smoke=smokeStarted?1:0;
    for(let i=0;i<240&&smoke===1;++i){smoke=runtime.module._RalWebGpu_BrowserModuleSmokePoll();
      if(smoke===1)await new Promise(resolve=>setTimeout(resolve,16));}
    let rendererStarted=0,rendererSmoke=0;
    if(smoke===2){rendererStarted=runtime.module._WiredWebGpu_RendererSmokeBegin();rendererSmoke=rendererStarted?1:0;
      for(let i=0;i<240&&rendererSmoke===1;++i){rendererSmoke=runtime.module._WiredWebGpu_RendererSmokePoll();
        if(rendererSmoke===1)await new Promise(resolve=>setTimeout(resolve,16));}}
    if(smoke===2&&rendererSmoke===2)publish("PASS",{schemaVersion:1,generation:state.generation,width:canvas.width,height:canvas.height,
      pollStatus:poll,smokeStatus:smoke,rendererSmoke,transaction:["resource-write","atmosphere-uniform","froxel-storage-arena","compute-bind-group","wgsl-compute-pipeline","four-dispatch-atmosphere","temporal-integrate","weather-ping-pong-pool","weather-compute-dispatch","weather-instanced-draw","wgsl-fragment-composite","indexed-draw","submit","present"],
      rendererModule:["GetRefAPI","BeginRegistration","world-ui-submission","ProductRender","async-present","reverse-shutdown"],
      userAgent:navigator.userAgent});
    else publish("FAIL",{poll,state,smokeStarted,smoke,width:canvas.width,height:canvas.height,
      rendererStarted,rendererSmoke,dispatchError:runtime.lastDispatchError()});
  }
  else publish("FAIL",{poll,state,width:canvas.width,height:canvas.height});
  globalThis.wiredWebGpuProbeRuntime=runtime;
} catch(error){publish("FAIL",{message:String(error?.message??error),stack:String(error?.stack??"")});}
