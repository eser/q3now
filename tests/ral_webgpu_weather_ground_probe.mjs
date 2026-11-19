// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
// Public renderer upload -> native compute -> native weather draw -> canvas.
export async function verifyWeatherGround(runtime) {
  const check=(ok,message)=>{if(!ok)throw new Error(message);};
  const wait=async poll=>{
    let status=1;
    for(let i=0;i<240&&status===1;++i){
      status=poll();
      if(status===1)await new Promise(resolve=>setTimeout(resolve,16));
    }
    check(status===2,`weather ground poll=${status}: ${runtime.lastDispatchError()}`);
  };
  check(runtime.module._WiredWebGpu_RendererSmokeBegin()===1,"weather ground registration failed");
  await wait(()=>runtime.rendererPoll());
  const samples=[];
  try {
    for(let step=0;step<11;++step){
      const captured=runtime.host.captureNextCanvas(runtime.capability().generation);
      check(runtime.module._WiredWebGpu_WeatherGroundSmokeFrame(step)===1,
        `weather ground frame ${step} rejected: ${runtime.module.wiredWebModuleLog.slice(-6)}`);
      await wait(()=>runtime.module._WiredWebGpu_WeatherGroundSmokePoll());
      const image=await captured;
      check(image.width===1280&&image.height===720,"weather ground extent mismatch");
      let lit=0,below=0,belowSlope=0,nearSlope=0;
      for(let y=0;y<720;++y)for(let x=0;x<1280;++x){
        const offset=(y*1280+x)*4;
        if(image.rgba[offset]>1||image.rgba[offset+1]>1||image.rgba[offset+2]>1){
          ++lit;if(y>400)++below;
          if(step===6||step===7){
            // Independent perspective projection of z=+/-y_world/2.
            // Camera looks along +X, FOV 90x60. Permit only the projected
            // three-unit billboard radius, not wrong cell/axis sampling.
            const sign=step===6?1:-1;
            const boundary=360-sign*(360/Math.tan(Math.PI/6))*.5*(1-(x+.5)/640);
            if(y>boundary+40)++belowSlope;
            if(y<boundary-40&&y>boundary-100)++nearSlope;
          }
        }
      }
      if([1,2,5,8].includes(step)) check(lit===0,`roof/dead particles still drawn: step=${step} lit=${lit}`);
      else check(lit>100,`open weather missing: step=${step} lit=${lit}`);
      if(step===3) check(below===0,`particles drawn below ground: ${below}`);
      if(step===6||step===7){
        check(belowSlope===0,`wrong slope collision: step=${step} below=${belowSlope}`);
        check(nearSlope>100,`slope weather over-culled: step=${step} near=${nearSlope}`);
      }
      samples.push({step,lit,below,belowSlope,nearSlope});
    }
  } finally {runtime.module._WiredWebGpu_ExposureSmokeEnd();}
  return {scope:"public heightgrid -> GPU flat/sloped collision -> unload/reload -> live-only draw",samples};
}
