// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// Independent optical oracle, not a copy of the WGSL kernel or packet builder.
const clamp=x=>Math.max(0,Math.min(1,x));
const smooth=(a,b,x)=>{const t=clamp((x-a)/(b-a));return t*t*(3-2*t);};
const f=Math.fround, frac=x=>x-Math.floor(x);
function segmentDistance(world,start,end) {
  const axis=end.map((v,c)=>v-start[c]),delta=world.map((v,c)=>v-start[c]);
  const span=Math.hypot(...axis);
  if(!span)return Math.hypot(...delta);
  const direction=axis.map(v=>v/span);
  const along=Math.max(0,Math.min(span,delta.reduce((sum,v,c)=>sum+v*direction[c],0)));
  return Math.hypot(...delta.map((v,c)=>v-direction[c]*along));
}
function noise(p) {
  const cell=p.map(Math.floor), t=p.map((v,c)=>smooth(0,1,v-cell[c]));
  let result=0;
  for(let corner=0;corner<8;++corner) {
    let weight=1;
    const q=cell.map((v,c)=>{const bit=(corner>>c)&1;weight*=bit?t[c]:1-t[c];return frac(f((v+bit)*f(.1031)));});
    const d=f(f(f(q[0]*f(q[1]+f(33.33)))+f(q[1]*f(q[2]+f(33.33))))+f(q[2]*f(q[0]+f(33.33))));
    const h=f(f(f(f(q[0]+d)+q[1])+d)*f(q[2]+d));
    result+=weight*frac(h);
  }
  return result;
}
function oracle(camera,mode,frame,previous,reuse) {
  const a=camera?.65:0, forward=[Math.cos(a),0,Math.sin(a)], up=[-Math.sin(a),0,Math.cos(a)];
  const eye=camera?[45,-31,23]:[0,0,0];
  const origin=eye.map((v,c)=>v+forward[c]*40+up[c]*12);
  let start=origin,end=origin.map((v,c)=>v+(mode===4||mode===5
    ? forward[c]*80+(c===1?32:0) : mode===7?forward[c]*.0001:0));
  if(mode===5)[start,end]=[end,start];
  const albedo=[.72,.82,.92], sun=[1,.5,1/3], ambient=[.2,.4,.6], moon=[0,.05,.1];
  const output=new Float64Array(240*4);
  let litCells=0,cloudCells=0,tubeOnlyCells=0;
  for(let y=0;y<3;++y)for(let x=0;x<5;++x) {
    let transmission=1, radiance=[0,0,0];
    for(let z=0;z<16;++z) {
      const d=4*128**((z+.5)/16);
      const world=eye.map((v,c)=>v+d*(forward[c]-(c===1?1:0)*(2*(x+.5)/5-1)
        -up[c]*Math.tan(70*Math.PI/360)*(2*(y+.5)/3-1)));
      let extinction=(.001+.0004*frame)*Math.exp(-(mode?.003:0)*Math.max(world[2],0));
      let scatter=albedo.map(v=>v*extinction);
      const delta=world.map((v,c)=>v-origin[c]);
      if(mode===1||mode===2) {
        const weight=(mode===2?1-smooth(.85,1,Math.max(...delta.map(Math.abs))/16)
          :1-smooth(23.8,28,Math.hypot(...delta)))*Math.exp(-.015*Math.max(delta[2],0));
        extinction+=.007*weight;
        scatter=scatter.map((v,c)=>v+weight*(.007*(.2+.2*c)+(c===0?.004:0)));
      }
      const distance=mode>=4?segmentDistance(world,start,end):Math.hypot(...delta);
      const local=mode?Math.max(1-distance/30,0)**2:0;
      if(local>0)++litCells;
      if(mode>=4&&local>0&&Math.hypot(...world.map((v,c)=>v-start[c]))>30)++tubeOnlyCells;
      scatter=scatter.map((v,c)=>v*(ambient[c]+sun[c]*.6+moon[c]+local*[1,.3,.05][c]));
      if(mode===3) {
        const base=eye[2]+96,top=eye[2]+384,span=288;
        const envelope=smooth(base,base+span*.18,world[2])*(1-smooth(top-span*.18,top,world[2]));
        const p=world.map((v,c)=>(v+(c===0?2:0))/(span*.32));
        const cloud=smooth(.30,.52,noise(p))*envelope*.012;
        const ray=world.map((v,c)=>v-eye[c]);
        const phase=Math.max(ray[2]/Math.hypot(...ray),0)**4;
        scatter=scatter.map((v,c)=>v+cloud*[.92,.95,1][c]*(ambient[c]
          +sun[c]*Math.exp(-cloud*span*.18)*(.35+.65*phase)*.6));
        extinction+=cloud;if(cloud>.0001)++cloudCells;
      }
      const step=4*(128**((z+1)/16)-128**(z/16)), t=Math.exp(-extinction*step);
      radiance=radiance.map((v,c)=>v+scatter[c]*transmission*(1-t)/Math.max(extinction,.00001));
      transmission*=t;
      const index=((z*3+y)*5+x)*4;
      [...radiance,transmission].forEach((v,c)=>output[index+c]=reuse?.1*v+.9*previous[index+c]:v);
    }
  }
  return {output,litCells,cloudCells,tubeOnlyCells};
}
export async function verifyAtmosphere(runtime) {
  const m=runtime.module, check=(ok,msg)=>{if(!ok)throw new Error(msg);};
  check(m._wired_atmosphere_probe_begin()===1,"atmosphere executor create failed");
  const samples=[];let pending=false;
  try {
    for(let camera=0;camera<2;++camera)for(let mode=0;mode<8;++mode) {
      let previous;
      for(let frame=0;frame<6;++frame) {
        const begin=m._wired_atmosphere_probe_frame(camera,mode,frame);
        check(begin===1,`atmosphere begin=${begin}: ${runtime.lastDispatchError()}`);pending=true;
        if(frame===3)check(m._wired_atmosphere_probe_query(1)===0,"in-flight invalidation ignored");
        let status=1;
        for(let i=0;i<240&&status===1;++i) {
          status=m._wired_atmosphere_probe_poll();
          if(status===1)await new Promise(resolve=>setTimeout(resolve,16));
        }
        check(status===2,`atmosphere poll=${status}: ${runtime.lastDispatchError()}`);pending=false;
        const reuse=frame===1||frame===3||frame===5;
        check(m._wired_atmosphere_probe_query(1)===(frame===3?0:1),"completion resurrected invalidated atmosphere history");
        check(m._wired_atmosphere_probe_query(2)===Number(reuse),`unexpected history reuse frame=${frame}`);
        check(m._wired_atmosphere_probe_query(3)===(mode===3?5:4),"canonical dispatch count mismatch");
        const bytes=await runtime.host.readBuffer(runtime.capability().generation,m._wired_atmosphere_probe_query(0),240*16);
        const actual=new Float32Array(bytes.buffer,bytes.byteOffset,240*4);
        const expected=oracle(camera,mode,frame,previous,reuse);let maxError=0;
        for(let i=0;i<actual.length;++i) {
          const error=Math.abs(actual[i]-expected.output[i]);
          check(Number.isFinite(actual[i])&&error<.001,
            `atmosphere camera=${camera} mode=${mode} frame=${frame} component=${i}: ${actual[i]} expected ${expected.output[i]}`);
          maxError=Math.max(maxError,error);
        }
        previous=expected.output;
        samples.push({camera,mode,frame,reuse,maxError,litCells:expected.litCells,cloudCells:expected.cloudCells,
          tubeOnlyCells:expected.tubeOnlyCells});
      }
    }
    check(samples.some(s=>s.litCells>0)&&samples.some(s=>s.cloudCells>0)
      &&samples.some(s=>s.tubeOnlyCells>0),"oracle missed light/cloud/tube-only coverage");
  } finally {
    // Never free an executor while its GPU submission is still pending.
    if(!pending)check(m._wired_atmosphere_probe_end()===1,"atmosphere executor release failed");
  }
  return {frames:samples.length,froxels:samples.length*240,components:samples.length*960,
    scope:"canonical WebGPU executor numeric oracle, not whole-map visual acceptance",samples};
}
