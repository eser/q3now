// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

import createEmscriptenModule from "../build/ral_webgpu_browser_module.mjs";
import { createRalWebGpuBrowserModule } from
  "../code/render/ral/backends/webgpu/ral_webgpu_browser_module.mjs";
import { verifyExposureProduct } from "./ral_webgpu_exposure_product_probe.mjs";
import { verifyBeamCoverage } from "./ral_webgpu_beam_coverage_probe.mjs";
import { verifyUiLifecycle } from "./ral_webgpu_ui_lifecycle_probe.mjs";
import { verifySkyMaterial } from "./ral_webgpu_sky_material_probe.mjs";
import { verifyRenderSubresources } from "./ral_webgpu_render_subresource_probe.mjs";
import { verifySkyAtlas } from "./ral_webgpu_sky_atlas_probe.mjs";
import { verifyWeatherGround } from "./ral_webgpu_weather_ground_probe.mjs";
import { verifyAtmosphere } from "./ral_webgpu_atmosphere_probe.mjs";

const result=document.querySelector("#result"),canvas=document.querySelector("#wired-canvas");
const publish=(status,detail)=>{result.dataset.status=status;result.textContent=`${status}\n${JSON.stringify(detail,null,2)}`;};
try {
  const runtime=await createRalWebGpuBrowserModule({moduleFactory:createEmscriptenModule,
    gpu:navigator.gpu,canvas,generation:7,canvasIdentity:0x7000});
  let poll=1;
  for(let i=0;i<240 && poll===1;++i){poll=runtime.rendererPoll();if(poll===1)await new Promise(resolve=>setTimeout(resolve,16));}
  const state=runtime.capability();
  if(state.status==="unsupported") publish("UNSUPPORTED",{...state,userAgent:navigator.userAgent});
  else if(poll===2 && state.status==="ready" && new URLSearchParams(location.search).get("probe")==="atmosphere") {
    const atmosphere=await verifyAtmosphere(runtime);
    publish("PASS",{atmosphere,userAgent:navigator.userAgent});
  }
  else if(poll===2 && state.status==="ready" && canvas.width===1280 && canvas.height===720
    && new URLSearchParams(location.search).get("probe")==="weather-ground"){
    const weatherGround=await verifyWeatherGround(runtime);
    publish("PASS",{weatherGround,userAgent:navigator.userAgent});
  }
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
    const uiLifecycle=smoke===2&&rendererSmoke===2 ? await verifyUiLifecycle(runtime) : null;
    const exposureProduct=smoke===2&&rendererSmoke===2 ? await verifyExposureProduct(runtime) : null;
    const beamCoverage=smoke===2&&rendererSmoke===2 ? await verifyBeamCoverage(runtime) : null;
    const skyMaterial=smoke===2&&rendererSmoke===2 ? await verifySkyMaterial(runtime) : null;
    const renderSubresources=smoke===2&&rendererSmoke===2 ? await verifyRenderSubresources(runtime) : null;
    const skyAtlas=smoke===2&&rendererSmoke===2 ? await verifySkyAtlas(runtime) : null;
    const weatherGround=smoke===2&&rendererSmoke===2 ? await verifyWeatherGround(runtime) : null;
    const atmosphere=smoke===2&&rendererSmoke===2 ? await verifyAtmosphere(runtime) : null;
    if(smoke===2&&rendererSmoke===2)publish("PASS",{schemaVersion:1,generation:state.generation,width:canvas.width,height:canvas.height,uiLifecycle,exposureProduct,beamCoverage,skyMaterial,renderSubresources,skyAtlas,weatherGround,atmosphere,
      pollStatus:poll,smokeStatus:smoke,rendererSmoke,transaction:["resource-write","explicit-texture-usage","physical-sky-lut-storage","bounded-sky-compute","sky-submission-completion","atmosphere-uniform","froxel-storage-arena","compute-bind-group","wgsl-compute-pipeline","four-dispatch-atmosphere","temporal-integrate","weather-ping-pong-pool","weather-compute-dispatch","weather-instanced-draw","wgsl-fragment-composite","indexed-draw","submit","present"],
      rendererModule:["GetRefAPI","BeginRegistration","world-ui-submission","ProductRender","no-world-teleport-ui-zero-bsp-upload","teleport-metering-off-world-return","scene-depth-lens-dispatch","frame-exact-native-lens-readback","scope-reset-no-stale-visibility","async-present","reverse-shutdown"],
      userAgent:navigator.userAgent});
    else publish("FAIL",{poll,state,smokeStarted,smoke,width:canvas.width,height:canvas.height,
      rendererStarted,rendererSmoke,dispatchError:runtime.lastDispatchError(),
      moduleLog:runtime.module.wiredWebModuleLog});
  }
  else publish("FAIL",{poll,state,width:canvas.width,height:canvas.height});
  globalThis.wiredWebGpuProbeRuntime=runtime;
} catch(error){publish("FAIL",{message:String(error?.message??error),stack:String(error?.stack??"")});}
