// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
// Product-level GPU history acceptance; only public host capture and renderer
// fixture calls. No direct device access, injected history or replacement WGSL.
export async function verifyExposureProduct(runtime) {
  const check=(ok,message)=>{if(!ok)throw new Error(message);};
  const wait=async poll=>{
    let status=1;
    for(let i=0;i<240&&status===1;++i){
      status=poll();
      if(status===1)await new Promise(resolve=>setTimeout(resolve,16));
    }
    check(status===2,`exposure Product poll=${status}: ${runtime.lastDispatchError()}`);
  };
  const samples=[];
  for(const ui of [0,1]) {
    check(runtime.module._WiredWebGpu_RendererSmokeBegin()===1,"exposure fixture registration failed");
    await wait(()=>runtime.rendererPoll());
    const pixels=[];
    for(let step=0;step<56;++step){
      const captured=runtime.host.captureNextCanvas(runtime.capability().generation);
      check(runtime.module._WiredWebGpu_ExposureSmokeFrame(step,ui)===1,
        `exposure frame ${step} rejected: ${runtime.module.wiredWebModuleLog.slice(-4)}`);
      await wait(()=>runtime.module._WiredWebGpu_ExposureSmokePoll());
      const image=await captured;
      check(image.width===1280&&image.height===720,"exposure capture extent mismatch");
      const offset=(360*1280+960)*4;
      const pixel=[...image.rgba.slice(offset,offset+3)];
      check(pixel.every(v=>Math.abs(v-pixel[0])<=1),`non-neutral scene sample: ${pixel}`);
      if(ui){
        const left=(360*1280+320)*4;
        check(image.rgba.slice(left,left+3).every(v=>v===255),`UI metered/tonemapped at ${step}`);
        check(pixel.every((v,c)=>v===samples[0].pixels[step][c]),`UI changed exposure at ${step}`);
      }
      pixels.push(pixel);
    }
    const baseline=pixels[0][0];
    // 64/255 sRGB -> ~0.051 linear -> 13/255 in the identity UNORM target.
    // r_tonemap=0 must ignore the fixture's tone-specific multiplier 5.2.
    check(Math.abs(baseline-13)<=1,`identity exposure / disabled-tonemap mismatch: ${baseline}`);
    check(pixels[16][0]<baseline-2,`adaptation did not darken: ${baseline} -> ${pixels[16]}`);
    for(const [step,previous] of [[17,16],[19,18],[55,54]])
      check(pixels[step].every((v,c)=>v===pixels[previous][c]),`pause changed pixels at ${step}`);
    for(const step of [18,36,54])
      check(pixels[step].every((v,c)=>v===pixels[0][c]),`reset at ${step} differs from identity: ${pixels[step]} vs ${pixels[0]}`);
    check(pixels[53].every(v=>v===0),`no-world gap retained scene: ${pixels[53]}`);
    samples.push({ui,pixels});
    runtime.module._WiredWebGpu_ExposureSmokeEnd();
  }
  return {width:1280,height:720,frames:112,scope:"production renderer -> HDR -> exposure -> tonemap -> UI",
    checks:["identity","adaptation","pause","camera-cut","rewind","no-world-gap","world-return-reset","UI-exclusion","tonemap-zero-identity"],samples};
}
