// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
// Actual Product output, not a replacement shader or JavaScript renderer.
export async function verifyBeamCoverage(runtime) {
  const check=(ok,message)=>{if(!ok)throw new Error(message);};
  const wait=async poll=>{
    let status=1;
    for(let i=0;i<240&&status===1;++i){
      status=poll();
      if(status===1)await new Promise(resolve=>setTimeout(resolve,16));
    }
    check(status===2,`beam Product poll=${status}: ${runtime.lastDispatchError()}`);
  };
  check(runtime.module._WiredWebGpu_RendererSmokeBegin()===1,"beam fixture registration failed");
  await wait(()=>runtime.rendererPoll());
  const samples=[];
  try {
    for(const additive of [0,1])for(const textureAlpha of [0,128,255])for(const descriptorAlpha of [0,64,128,255]){
      const captured=runtime.host.captureNextCanvas(runtime.capability().generation);
      check(runtime.module._WiredWebGpu_BeamCoverageSmokeFrame(textureAlpha,descriptorAlpha,additive)===1,
        "beam coverage frame rejected");
      await wait(()=>runtime.module._WiredWebGpu_BeamCoverageSmokePoll());
      const image=await captured;
      check(image.width===1280&&image.height===720,"beam capture extent mismatch");
      // White texel * straight white descriptor, black destination, identity
      // UNORM output: either blend mode must apply the two alphas once only.
      const expected=Math.round(textureAlpha*descriptorAlpha/255);
      const pixels=[];
      for(const y of [310,360,410])for(const x of [384,512,640,768,896]){
        const offset=(y*1280+x)*4;
        const pixel=[...image.rgba.slice(offset,offset+3)];
        check(pixel.every(v=>Math.abs(v-expected)<=1),
          `beam coverage texture=${textureAlpha} descriptor=${descriptorAlpha} additive=${additive} at ${x},${y}: ${pixel}, expected ${expected}`);
        pixels.push(pixel);
      }
      const outside=(20*1280+20)*4;
      check(image.rgba.slice(outside,outside+3).every(v=>v===0),"beam altered clear background");
      samples.push({additive,textureAlpha,descriptorAlpha,expected,
        minimum:Math.min(...pixels.flat()),maximum:Math.max(...pixels.flat())});
    }
    const linear=c=>c<=.04045 ? c/12.92 : ((c+.055)/1.055)**2.4;
    for(const mode of [2,3]) {
      const captured=runtime.host.captureNextCanvas(runtime.capability().generation);
      check(runtime.module._WiredWebGpu_BeamCoverageSmokeFrame(255,255,mode)===1,"beam stage/colour frame rejected");
      await wait(()=>runtime.module._WiredWebGpu_BeamCoverageSmokePoll());
      const image=await captured, observations=[];
      check(image.width===1280&&image.height===720,"beam stage capture extent mismatch");
      for(const x of [384,512,640,768,896]) {
        // Identity view points along +X, left is +Y: end is left at x=320,
        // start right at x=960. Interpolate authored sRGB, THEN decode.
        const u=(960-(x+.5))/640;
        const expected=mode===2 ? [linear(64/255)*255,linear(128/255)*128,0]
          : Array(3).fill(linear(.12345+(.87654-.12345)*u)*255);
        const offset=(360*1280+x)*4, pixel=[...image.rgba.slice(offset,offset+3)];
        check(pixel.every((v,c)=>Math.abs(v-Math.round(expected[c]))<=1),
          `beam mode=${mode} at ${x}: ${pixel}, expected ${expected}`);
        observations.push({x,pixel,expected:expected.map(Math.round)});
      }
      samples.push({mode,observations});
    }
    // Homogeneous extinction gives an independent Beer-Lambert oracle. Beam
    // corners are (8,+/-4,+/-1), sprite corners (8,+/-4,+/-4). The analytic
    // path interpolates their radial distances; FULL integrates log-depth
    // cells through slice 16 (forward depth 8, near 4, far 64, 64 slices).
    for(const fogMode of [3,4,1,2])for(const emission of [0,255])for(const alpha of [128,255]) {
      const captured=runtime.host.captureNextCanvas(runtime.capability().generation);
      check(runtime.module._WiredWebGpu_BeamCoverageSmokeFrame(255,alpha,1,fogMode,emission)===1,
        `fog frame rejected: mode=${fogMode}`);
      await wait(()=>runtime.module._WiredWebGpu_BeamCoverageSmokePoll());
      const image=await captured;
      const full=fogMode%2===0, sprite=fogMode>=3;
      const length=full ? 4*16**(17/64)-4 : Math.sqrt(sprite?96:81);
      const expected=Math.round(emission*Math.exp(-.1*length)*alpha/255);
      const observations=[];
      for(const y of [340,360,380])for(const x of [448,544,640,736,832]) {
        const offset=(y*1280+x)*4, pixel=[...image.rgba.slice(offset,offset+3)];
        check(pixel.every(v=>Math.abs(v-expected)<=2),
          `fog mode=${fogMode} emission=${emission} alpha=${alpha} at ${x},${y}: ${pixel}, expected ${expected}`);
        observations.push(pixel);
      }
      samples.push({fogMode,emission,alpha,expected,observations});
    }
  } finally {runtime.module._WiredWebGpu_ExposureSmokeEnd();}
  return {frames:samples.length,width:1280,height:720,scope:"beam Product -> native blend -> HDR -> identity output",samples};
}
