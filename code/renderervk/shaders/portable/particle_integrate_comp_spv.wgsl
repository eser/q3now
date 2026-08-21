struct ParticleParm {
    calc: i32,
    hasCurve: i32,
    val0_: f32,
    val1_: f32,
    variance: f32,
    parmPad0_: f32,
    parmPad1_: f32,
    parmPad2_: f32,
    samples: array<f32, 8>,
}

struct Particle {
    pos: vec3<f32>,
    age: f32,
    vel: vec3<f32>,
    lifetimeInv: f32,
    classHandle: u32,
    paletteIndex: u32,
    sizeJitterPick: f32,
    pad1_: u32,
    pad2_: u32,
    pad3_: u32,
    pad4_: u32,
    pad5_: u32,
}

struct ParticleFrame {
    mvp: mat4x4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    eyeWorld: vec4<f32>,
    dt: f32,
    poolSize: u32,
    numClasses: u32,
    pingPongRead: u32,
    pad0_: f32,
    pad1_: f32,
    pad2_: f32,
    pad3_: f32,
}

struct ReadPool {
    readParticles: array<Particle>,
}

struct WritePool {
    writeParticles: array<Particle>,
}

struct ParticleClassGPU {
    shader: u32,
    renderFlags: u32,
    emitMode: u32,
    scatterShape: u32,
    scatterMagnitude: f32,
    velocityShape: u32,
    axialSpeed: f32,
    cubeJitter: f32,
    coneHalfAngle: f32,
    lifetimeMean: f32,
    lifetimeJitter: f32,
    paletteCount: i32,
    colorPalette: array<vec4<f32>, 16>,
    colorEndMult: vec4<f32>,
    sizeStart: f32,
    sizeEnd: f32,
    gravityScale: f32,
    drag: f32,
    shaderBlendIsAdditive: u32,
    pad1_: u32,
    pad2_: u32,
    pad3_: u32,
    velocityBias: vec4<f32>,
    velocityBiasJitter: vec4<f32>,
    speedJitter: f32,
    sizeJitter: f32,
    colorDomain: u32,
    pad5_: u32,
    frameSlots: array<u32, 16>,
    frameCount: u32,
    frameBlend: u32,
    framePad0_: u32,
    framePad1_: u32,
    sizeParm: ParticleParm,
    alphaParm: ParticleParm,
    dragParm: ParticleParm,
    gravityParm: ParticleParm,
}

struct Classes {
    classes: array<ParticleClassGPU>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(0) 
var<uniform> unnamed: ParticleFrame;
@group(0) @binding(1) 
var<storage> unnamed_1: ReadPool;
@group(0) @binding(2) 
var<storage, read_write> unnamed_2: WritePool;
@group(0) @binding(3) 
var<storage> unnamed_3: Classes;

fn parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b(p: ptr<function, ParticleParm>, fraction: ptr<function, f32>) -> f32 {
    var pos: f32;
    var i0_: i32;
    var i1_: i32;
    var frac: f32;

    let _e62 = (*p).hasCurve;
    if (_e62 == 0i) {
        return 1f;
    }
    let _e64 = (*fraction);
    if (_e64 <= 0f) {
        let _e68 = (*p).samples[0i];
        return _e68;
    }
    let _e69 = (*fraction);
    if (_e69 >= 1f) {
        let _e73 = (*p).samples[7i];
        return _e73;
    }
    let _e74 = (*fraction);
    pos = (_e74 * 7f);
    let _e76 = pos;
    i0_ = i32(_e76);
    let _e78 = i0_;
    i1_ = min((_e78 + 1i), 7i);
    let _e81 = pos;
    let _e82 = i0_;
    frac = (_e81 - f32(_e82));
    let _e85 = i0_;
    let _e88 = (*p).samples[_e85];
    let _e89 = i1_;
    let _e92 = (*p).samples[_e89];
    let _e93 = frac;
    return mix(_e88, _e92, _e93);
}

fn parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b(p_1: ptr<function, ParticleParm>, fraction_1: ptr<function, f32>, jitterPick: ptr<function, f32>) -> f32 {
    var base: f32;
    var param: ParticleParm;
    var param_1: f32;
    var param_2: ParticleParm;
    var param_3: f32;

    let _e63 = (*fraction_1);
    (*fraction_1) = clamp(_e63, 0f, 1f);
    let _e66 = (*p_1).calc;
    if (_e66 == 1i) {
        let _e69 = (*p_1).val0_;
        let _e71 = (*p_1).val1_;
        let _e72 = (*fraction_1);
        base = mix(_e69, _e71, _e72);
    } else {
        let _e75 = (*p_1).calc;
        if (_e75 == 2i) {
            let _e78 = (*p_1).val0_;
            let _e80 = (*p_1).val1_;
            let _e81 = (*p_1);
            param = _e81;
            let _e82 = (*fraction_1);
            param_1 = _e82;
            let _e83 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param), (&param_1));
            base = mix(_e78, _e80, _e83);
        } else {
            let _e86 = (*p_1).calc;
            if (_e86 == 3i) {
                let _e88 = (*p_1);
                param_2 = _e88;
                let _e89 = (*fraction_1);
                param_3 = _e89;
                let _e90 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param_2), (&param_3));
                let _e92 = (*p_1).val0_;
                let _e94 = (*p_1).val1_;
                let _e95 = (*fraction_1);
                base = (_e90 * mix(_e92, _e94, _e95));
            } else {
                let _e99 = (*p_1).val0_;
                base = _e99;
            }
        }
    }
    let _e100 = base;
    let _e102 = (*p_1).variance;
    let _e103 = (*jitterPick);
    return (_e100 + (_e102 * _e103));
}

fn parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b(p_2: ptr<function, ParticleParm>) -> bool {
    var phi_158_: bool;

    let _e57 = (*p_2).calc;
    let _e58 = (_e57 == 0i);
    phi_158_ = _e58;
    if _e58 {
        let _e60 = (*p_2).val0_;
        phi_158_ = (_e60 == 0f);
    }
    let _e63 = phi_158_;
    return _e63;
}

fn deadParticle_u0028_() -> Particle {
    var p_3: Particle;

    p_3.pos = vec3<f32>(0f, 0f, 0f);
    p_3.age = 0f;
    p_3.vel = vec3<f32>(0f, 0f, 0f);
    p_3.lifetimeInv = 0f;
    p_3.classHandle = 0u;
    p_3.paletteIndex = 0u;
    p_3.sizeJitterPick = 0f;
    p_3.pad1_ = 0u;
    p_3.pad2_ = 0u;
    p_3.pad3_ = 0u;
    p_3.pad4_ = 0u;
    p_3.pad5_ = 0u;
    let _e68 = p_3;
    return _e68;
}

fn main_1() {
    var idx: u32;
    var p_4: Particle;
    var c: ParticleClassGPU;
    var gScale: f32;
    var param_4: ParticleParm;
    var local: f32;
    var param_5: ParticleParm;
    var param_6: f32;
    var param_7: f32;
    var dragV: f32;
    var param_8: ParticleParm;
    var local_1: f32;
    var param_9: ParticleParm;
    var param_10: f32;
    var param_11: f32;
    var phi_250_: bool;
    var phi_259_: bool;

    let _e71 = gl_GlobalInvocationID_1[0u];
    idx = _e71;
    let _e72 = idx;
    let _e74 = unnamed.poolSize;
    if (_e72 >= _e74) {
        return;
    }
    let _e76 = idx;
    let _e79 = unnamed_1.readParticles[_e76];
    p_4.pos = _e79.pos;
    p_4.age = _e79.age;
    p_4.vel = _e79.vel;
    p_4.lifetimeInv = _e79.lifetimeInv;
    p_4.classHandle = _e79.classHandle;
    p_4.paletteIndex = _e79.paletteIndex;
    p_4.sizeJitterPick = _e79.sizeJitterPick;
    p_4.pad1_ = _e79.pad1_;
    p_4.pad2_ = _e79.pad2_;
    p_4.pad3_ = _e79.pad3_;
    p_4.pad4_ = _e79.pad4_;
    p_4.pad5_ = _e79.pad5_;
    let _e105 = p_4.classHandle;
    let _e106 = (_e105 == 0u);
    phi_250_ = _e106;
    if !(_e106) {
        let _e109 = p_4.age;
        phi_250_ = (_e109 >= 1f);
    }
    let _e112 = phi_250_;
    phi_259_ = _e112;
    if !(_e112) {
        let _e115 = p_4.classHandle;
        let _e117 = unnamed.numClasses;
        phi_259_ = (_e115 > _e117);
    }
    let _e120 = phi_259_;
    if _e120 {
        let _e121 = idx;
        let _e122 = deadParticle_u0028_();
        unnamed_2.writeParticles[_e121].pos = _e122.pos;
        unnamed_2.writeParticles[_e121].age = _e122.age;
        unnamed_2.writeParticles[_e121].vel = _e122.vel;
        unnamed_2.writeParticles[_e121].lifetimeInv = _e122.lifetimeInv;
        unnamed_2.writeParticles[_e121].classHandle = _e122.classHandle;
        unnamed_2.writeParticles[_e121].paletteIndex = _e122.paletteIndex;
        unnamed_2.writeParticles[_e121].sizeJitterPick = _e122.sizeJitterPick;
        unnamed_2.writeParticles[_e121].pad1_ = _e122.pad1_;
        unnamed_2.writeParticles[_e121].pad2_ = _e122.pad2_;
        unnamed_2.writeParticles[_e121].pad3_ = _e122.pad3_;
        unnamed_2.writeParticles[_e121].pad4_ = _e122.pad4_;
        unnamed_2.writeParticles[_e121].pad5_ = _e122.pad5_;
        return;
    }
    let _e150 = p_4.classHandle;
    let _e154 = unnamed_3.classes[(_e150 - 1u)];
    c.shader = _e154.shader;
    c.renderFlags = _e154.renderFlags;
    c.emitMode = _e154.emitMode;
    c.scatterShape = _e154.scatterShape;
    c.scatterMagnitude = _e154.scatterMagnitude;
    c.velocityShape = _e154.velocityShape;
    c.axialSpeed = _e154.axialSpeed;
    c.cubeJitter = _e154.cubeJitter;
    c.coneHalfAngle = _e154.coneHalfAngle;
    c.lifetimeMean = _e154.lifetimeMean;
    c.lifetimeJitter = _e154.lifetimeJitter;
    c.paletteCount = _e154.paletteCount;
    c.colorPalette[0i] = _e154.colorPalette[0];
    c.colorPalette[1i] = _e154.colorPalette[1];
    c.colorPalette[2i] = _e154.colorPalette[2];
    c.colorPalette[3i] = _e154.colorPalette[3];
    c.colorPalette[4i] = _e154.colorPalette[4];
    c.colorPalette[5i] = _e154.colorPalette[5];
    c.colorPalette[6i] = _e154.colorPalette[6];
    c.colorPalette[7i] = _e154.colorPalette[7];
    c.colorPalette[8i] = _e154.colorPalette[8];
    c.colorPalette[9i] = _e154.colorPalette[9];
    c.colorPalette[10i] = _e154.colorPalette[10];
    c.colorPalette[11i] = _e154.colorPalette[11];
    c.colorPalette[12i] = _e154.colorPalette[12];
    c.colorPalette[13i] = _e154.colorPalette[13];
    c.colorPalette[14i] = _e154.colorPalette[14];
    c.colorPalette[15i] = _e154.colorPalette[15];
    c.colorEndMult = _e154.colorEndMult;
    c.sizeStart = _e154.sizeStart;
    c.sizeEnd = _e154.sizeEnd;
    c.gravityScale = _e154.gravityScale;
    c.drag = _e154.drag;
    c.shaderBlendIsAdditive = _e154.shaderBlendIsAdditive;
    c.pad1_ = _e154.pad1_;
    c.pad2_ = _e154.pad2_;
    c.pad3_ = _e154.pad3_;
    c.velocityBias = _e154.velocityBias;
    c.velocityBiasJitter = _e154.velocityBiasJitter;
    c.speedJitter = _e154.speedJitter;
    c.sizeJitter = _e154.sizeJitter;
    c.colorDomain = _e154.colorDomain;
    c.pad5_ = _e154.pad5_;
    c.frameSlots[0i] = _e154.frameSlots[0];
    c.frameSlots[1i] = _e154.frameSlots[1];
    c.frameSlots[2i] = _e154.frameSlots[2];
    c.frameSlots[3i] = _e154.frameSlots[3];
    c.frameSlots[4i] = _e154.frameSlots[4];
    c.frameSlots[5i] = _e154.frameSlots[5];
    c.frameSlots[6i] = _e154.frameSlots[6];
    c.frameSlots[7i] = _e154.frameSlots[7];
    c.frameSlots[8i] = _e154.frameSlots[8];
    c.frameSlots[9i] = _e154.frameSlots[9];
    c.frameSlots[10i] = _e154.frameSlots[10];
    c.frameSlots[11i] = _e154.frameSlots[11];
    c.frameSlots[12i] = _e154.frameSlots[12];
    c.frameSlots[13i] = _e154.frameSlots[13];
    c.frameSlots[14i] = _e154.frameSlots[14];
    c.frameSlots[15i] = _e154.frameSlots[15];
    c.frameCount = _e154.frameCount;
    c.frameBlend = _e154.frameBlend;
    c.framePad0_ = _e154.framePad0_;
    c.framePad1_ = _e154.framePad1_;
    c.sizeParm.calc = _e154.sizeParm.calc;
    c.sizeParm.hasCurve = _e154.sizeParm.hasCurve;
    c.sizeParm.val0_ = _e154.sizeParm.val0_;
    c.sizeParm.val1_ = _e154.sizeParm.val1_;
    c.sizeParm.variance = _e154.sizeParm.variance;
    c.sizeParm.parmPad0_ = _e154.sizeParm.parmPad0_;
    c.sizeParm.parmPad1_ = _e154.sizeParm.parmPad1_;
    c.sizeParm.parmPad2_ = _e154.sizeParm.parmPad2_;
    c.sizeParm.samples[0i] = _e154.sizeParm.samples[0];
    c.sizeParm.samples[1i] = _e154.sizeParm.samples[1];
    c.sizeParm.samples[2i] = _e154.sizeParm.samples[2];
    c.sizeParm.samples[3i] = _e154.sizeParm.samples[3];
    c.sizeParm.samples[4i] = _e154.sizeParm.samples[4];
    c.sizeParm.samples[5i] = _e154.sizeParm.samples[5];
    c.sizeParm.samples[6i] = _e154.sizeParm.samples[6];
    c.sizeParm.samples[7i] = _e154.sizeParm.samples[7];
    c.alphaParm.calc = _e154.alphaParm.calc;
    c.alphaParm.hasCurve = _e154.alphaParm.hasCurve;
    c.alphaParm.val0_ = _e154.alphaParm.val0_;
    c.alphaParm.val1_ = _e154.alphaParm.val1_;
    c.alphaParm.variance = _e154.alphaParm.variance;
    c.alphaParm.parmPad0_ = _e154.alphaParm.parmPad0_;
    c.alphaParm.parmPad1_ = _e154.alphaParm.parmPad1_;
    c.alphaParm.parmPad2_ = _e154.alphaParm.parmPad2_;
    c.alphaParm.samples[0i] = _e154.alphaParm.samples[0];
    c.alphaParm.samples[1i] = _e154.alphaParm.samples[1];
    c.alphaParm.samples[2i] = _e154.alphaParm.samples[2];
    c.alphaParm.samples[3i] = _e154.alphaParm.samples[3];
    c.alphaParm.samples[4i] = _e154.alphaParm.samples[4];
    c.alphaParm.samples[5i] = _e154.alphaParm.samples[5];
    c.alphaParm.samples[6i] = _e154.alphaParm.samples[6];
    c.alphaParm.samples[7i] = _e154.alphaParm.samples[7];
    c.dragParm.calc = _e154.dragParm.calc;
    c.dragParm.hasCurve = _e154.dragParm.hasCurve;
    c.dragParm.val0_ = _e154.dragParm.val0_;
    c.dragParm.val1_ = _e154.dragParm.val1_;
    c.dragParm.variance = _e154.dragParm.variance;
    c.dragParm.parmPad0_ = _e154.dragParm.parmPad0_;
    c.dragParm.parmPad1_ = _e154.dragParm.parmPad1_;
    c.dragParm.parmPad2_ = _e154.dragParm.parmPad2_;
    c.dragParm.samples[0i] = _e154.dragParm.samples[0];
    c.dragParm.samples[1i] = _e154.dragParm.samples[1];
    c.dragParm.samples[2i] = _e154.dragParm.samples[2];
    c.dragParm.samples[3i] = _e154.dragParm.samples[3];
    c.dragParm.samples[4i] = _e154.dragParm.samples[4];
    c.dragParm.samples[5i] = _e154.dragParm.samples[5];
    c.dragParm.samples[6i] = _e154.dragParm.samples[6];
    c.dragParm.samples[7i] = _e154.dragParm.samples[7];
    c.gravityParm.calc = _e154.gravityParm.calc;
    c.gravityParm.hasCurve = _e154.gravityParm.hasCurve;
    c.gravityParm.val0_ = _e154.gravityParm.val0_;
    c.gravityParm.val1_ = _e154.gravityParm.val1_;
    c.gravityParm.variance = _e154.gravityParm.variance;
    c.gravityParm.parmPad0_ = _e154.gravityParm.parmPad0_;
    c.gravityParm.parmPad1_ = _e154.gravityParm.parmPad1_;
    c.gravityParm.parmPad2_ = _e154.gravityParm.parmPad2_;
    c.gravityParm.samples[0i] = _e154.gravityParm.samples[0];
    c.gravityParm.samples[1i] = _e154.gravityParm.samples[1];
    c.gravityParm.samples[2i] = _e154.gravityParm.samples[2];
    c.gravityParm.samples[3i] = _e154.gravityParm.samples[3];
    c.gravityParm.samples[4i] = _e154.gravityParm.samples[4];
    c.gravityParm.samples[5i] = _e154.gravityParm.samples[5];
    c.gravityParm.samples[6i] = _e154.gravityParm.samples[6];
    c.gravityParm.samples[7i] = _e154.gravityParm.samples[7];
    let _e430 = c.gravityParm;
    param_4 = _e430;
    let _e431 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_4));
    if _e431 {
        let _e433 = c.gravityScale;
        local = _e433;
    } else {
        let _e435 = c.gravityParm;
        param_5 = _e435;
        let _e437 = p_4.age;
        param_6 = _e437;
        let _e439 = p_4.sizeJitterPick;
        param_7 = _e439;
        let _e440 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_5), (&param_6), (&param_7));
        local = _e440;
    }
    let _e441 = local;
    gScale = _e441;
    let _e443 = c.dragParm;
    param_8 = _e443;
    let _e444 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_8));
    if _e444 {
        let _e446 = c.drag;
        local_1 = _e446;
    } else {
        let _e448 = c.dragParm;
        param_9 = _e448;
        let _e450 = p_4.age;
        param_10 = _e450;
        let _e452 = p_4.sizeJitterPick;
        param_11 = _e452;
        let _e453 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_9), (&param_10), (&param_11));
        local_1 = _e453;
    }
    let _e454 = local_1;
    dragV = _e454;
    let _e455 = gScale;
    let _e458 = unnamed.dt;
    let _e462 = p_4.vel[2u];
    p_4.vel[2u] = (_e462 + ((-800f * _e455) * _e458));
    let _e466 = dragV;
    let _e469 = unnamed.dt;
    let _e473 = p_4.vel;
    p_4.vel = (_e473 * exp((-(_e466) * _e469)));
    let _e477 = p_4.vel;
    let _e479 = unnamed.dt;
    let _e482 = p_4.pos;
    p_4.pos = (_e482 + (_e477 * _e479));
    let _e486 = unnamed.dt;
    let _e488 = p_4.lifetimeInv;
    let _e491 = p_4.age;
    p_4.age = (_e491 + (_e486 * _e488));
    let _e495 = p_4.age;
    if (_e495 >= 1f) {
        let _e497 = idx;
        let _e498 = deadParticle_u0028_();
        unnamed_2.writeParticles[_e497].pos = _e498.pos;
        unnamed_2.writeParticles[_e497].age = _e498.age;
        unnamed_2.writeParticles[_e497].vel = _e498.vel;
        unnamed_2.writeParticles[_e497].lifetimeInv = _e498.lifetimeInv;
        unnamed_2.writeParticles[_e497].classHandle = _e498.classHandle;
        unnamed_2.writeParticles[_e497].paletteIndex = _e498.paletteIndex;
        unnamed_2.writeParticles[_e497].sizeJitterPick = _e498.sizeJitterPick;
        unnamed_2.writeParticles[_e497].pad1_ = _e498.pad1_;
        unnamed_2.writeParticles[_e497].pad2_ = _e498.pad2_;
        unnamed_2.writeParticles[_e497].pad3_ = _e498.pad3_;
        unnamed_2.writeParticles[_e497].pad4_ = _e498.pad4_;
        unnamed_2.writeParticles[_e497].pad5_ = _e498.pad5_;
        return;
    }
    let _e525 = idx;
    let _e526 = p_4;
    unnamed_2.writeParticles[_e525].pos = _e526.pos;
    unnamed_2.writeParticles[_e525].age = _e526.age;
    unnamed_2.writeParticles[_e525].vel = _e526.vel;
    unnamed_2.writeParticles[_e525].lifetimeInv = _e526.lifetimeInv;
    unnamed_2.writeParticles[_e525].classHandle = _e526.classHandle;
    unnamed_2.writeParticles[_e525].paletteIndex = _e526.paletteIndex;
    unnamed_2.writeParticles[_e525].sizeJitterPick = _e526.sizeJitterPick;
    unnamed_2.writeParticles[_e525].pad1_ = _e526.pad1_;
    unnamed_2.writeParticles[_e525].pad2_ = _e526.pad2_;
    unnamed_2.writeParticles[_e525].pad3_ = _e526.pad3_;
    unnamed_2.writeParticles[_e525].pad4_ = _e526.pad4_;
    unnamed_2.writeParticles[_e525].pad5_ = _e526.pad5_;
    return;
}

@compute @workgroup_size(64, 1, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
