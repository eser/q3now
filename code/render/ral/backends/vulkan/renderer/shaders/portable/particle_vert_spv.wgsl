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

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
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
    invResX: f32,
    invResY: f32,
    depthValid: f32,
    exposureBias: f32,
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

struct Pool {
    particles: array<Particle>,
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

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec4<f32>,
    @location(2) @interpolate(flat) member_2: u32,
    @location(3) @interpolate(flat) member_3: u32,
    @location(4) @interpolate(flat) member_4: u32,
    @location(5) member_5: f32,
}

@id(0) override PIPELINE_BLEND_MASK: u32 = 0u;

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;
var<private> particleClassHandle: u32;
var<private> frameSlot0_: u32;
var<private> frameSlot1_: u32;
var<private> frameBlend: f32;
var<private> gl_InstanceIndex_1: i32;
var<private> gl_VertexIndex_1: i32;
@group(0) @binding(0)
var<uniform> unnamed_1: ParticleFrame;
@group(0) @binding(1)
var<storage> unnamed_2: Pool;
@group(0) @binding(2)
var<storage> unnamed_3: Classes;

fn parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b(p: ptr<function, ParticleParm>, fraction: ptr<function, f32>) -> f32 {
    var pos: f32;
    var i0_: i32;
    var i1_: i32;
    var frac: f32;

    let _e78 = (*p).hasCurve;
    if (_e78 == 0i) {
        return 1f;
    }
    let _e80 = (*fraction);
    if (_e80 <= 0f) {
        let _e84 = (*p).samples[0i];
        return _e84;
    }
    let _e85 = (*fraction);
    if (_e85 >= 1f) {
        let _e89 = (*p).samples[7i];
        return _e89;
    }
    let _e90 = (*fraction);
    pos = (_e90 * 7f);
    let _e92 = pos;
    i0_ = i32(_e92);
    let _e94 = i0_;
    i1_ = min((_e94 + 1i), 7i);
    let _e97 = pos;
    let _e98 = i0_;
    frac = (_e97 - f32(_e98));
    let _e101 = i0_;
    let _e104 = (*p).samples[_e101];
    let _e105 = i1_;
    let _e108 = (*p).samples[_e105];
    let _e109 = frac;
    return mix(_e104, _e108, _e109);
}

fn parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b(p_1: ptr<function, ParticleParm>, fraction_1: ptr<function, f32>, jitterPick: ptr<function, f32>) -> f32 {
    var base: f32;
    var param: ParticleParm;
    var param_1: f32;
    var param_2: ParticleParm;
    var param_3: f32;

    let _e79 = (*fraction_1);
    (*fraction_1) = clamp(_e79, 0f, 1f);
    let _e82 = (*p_1).calc;
    if (_e82 == 1i) {
        let _e85 = (*p_1).val0_;
        let _e87 = (*p_1).val1_;
        let _e88 = (*fraction_1);
        base = mix(_e85, _e87, _e88);
    } else {
        let _e91 = (*p_1).calc;
        if (_e91 == 2i) {
            let _e94 = (*p_1).val0_;
            let _e96 = (*p_1).val1_;
            let _e97 = (*p_1);
            param = _e97;
            let _e98 = (*fraction_1);
            param_1 = _e98;
            let _e99 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param), (&param_1));
            base = mix(_e94, _e96, _e99);
        } else {
            let _e102 = (*p_1).calc;
            if (_e102 == 3i) {
                let _e104 = (*p_1);
                param_2 = _e104;
                let _e105 = (*fraction_1);
                param_3 = _e105;
                let _e106 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param_2), (&param_3));
                let _e108 = (*p_1).val0_;
                let _e110 = (*p_1).val1_;
                let _e111 = (*fraction_1);
                base = (_e106 * mix(_e108, _e110, _e111));
            } else {
                let _e115 = (*p_1).val0_;
                base = _e115;
            }
        }
    }
    let _e116 = base;
    let _e118 = (*p_1).variance;
    let _e119 = (*jitterPick);
    return (_e116 + (_e118 * _e119));
}

fn parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b(p_2: ptr<function, ParticleParm>) -> bool {
    var phi_155_: bool;

    let _e73 = (*p_2).calc;
    let _e74 = (_e73 == 0i);
    phi_155_ = _e74;
    if _e74 {
        let _e76 = (*p_2).val0_;
        phi_155_ = (_e76 == 0f);
    }
    let _e79 = phi_155_;
    return _e79;
}

fn emitDegenerate_u0028_() {
    unnamed.gl_Position = vec4<f32>(0f, 0f, 0f, 1f);
    fragUV = vec2<f32>(0f, 0f);
    fragColor = vec4<f32>(0f, 0f, 0f, 0f);
    particleClassHandle = 1u;
    frameSlot0_ = 4294967295u;
    frameSlot1_ = 4294967295u;
    frameBlend = 0f;
    return;
}

fn main_1() {
    var particleIdx: u32;
    var vertInQuad: u32;
    var p_3: Particle;
    var c: ParticleClassGPU;
    var sx: f32;
    var indexable: array<f32, 6>;
    var uy: f32;
    var indexable_1: array<f32, 6>;
    var paletteIdx: u32;
    var baseColor: vec4<f32>;
    var endColor: vec4<f32>;
    var color: vec4<f32>;
    var param_4: ParticleParm;
    var param_5: ParticleParm;
    var param_6: f32;
    var param_7: f32;
    var effectiveStart: f32;
    var size: f32;
    var param_8: ParticleParm;
    var param_9: ParticleParm;
    var param_10: f32;
    var param_11: f32;
    var worldPos: vec3<f32>;
    var indexable_2: array<vec2<f32>, 6>;
    var fr: f32;
    var last: u32;
    var frame0_: u32;
    var frame1_: u32;
    var local: f32;
    var phi_257_: bool;
    var phi_266_: bool;

    let _e100 = gl_InstanceIndex_1;
    particleIdx = bitcast<u32>(_e100);
    let _e102 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e102) % 6u);
    let _e105 = particleIdx;
    let _e107 = unnamed_1.poolSize;
    if (_e105 >= _e107) {
        emitDegenerate_u0028_();
        return;
    }
    let _e109 = particleIdx;
    let _e112 = unnamed_2.particles[_e109];
    p_3.pos = _e112.pos;
    p_3.age = _e112.age;
    p_3.vel = _e112.vel;
    p_3.lifetimeInv = _e112.lifetimeInv;
    p_3.classHandle = _e112.classHandle;
    p_3.paletteIndex = _e112.paletteIndex;
    p_3.sizeJitterPick = _e112.sizeJitterPick;
    p_3.pad1_ = _e112.pad1_;
    p_3.pad2_ = _e112.pad2_;
    p_3.pad3_ = _e112.pad3_;
    p_3.pad4_ = _e112.pad4_;
    p_3.pad5_ = _e112.pad5_;
    let _e138 = p_3.classHandle;
    let _e139 = (_e138 == 0u);
    phi_257_ = _e139;
    if !(_e139) {
        let _e142 = p_3.age;
        phi_257_ = (_e142 >= 1f);
    }
    let _e145 = phi_257_;
    phi_266_ = _e145;
    if !(_e145) {
        let _e148 = p_3.classHandle;
        let _e150 = unnamed_1.numClasses;
        phi_266_ = (_e148 > _e150);
    }
    let _e153 = phi_266_;
    if _e153 {
        emitDegenerate_u0028_();
        return;
    }
    let _e155 = p_3.classHandle;
    let _e159 = unnamed_3.classes[(_e155 - 1u)];
    c.shader = _e159.shader;
    c.renderFlags = _e159.renderFlags;
    c.emitMode = _e159.emitMode;
    c.scatterShape = _e159.scatterShape;
    c.scatterMagnitude = _e159.scatterMagnitude;
    c.velocityShape = _e159.velocityShape;
    c.axialSpeed = _e159.axialSpeed;
    c.cubeJitter = _e159.cubeJitter;
    c.coneHalfAngle = _e159.coneHalfAngle;
    c.lifetimeMean = _e159.lifetimeMean;
    c.lifetimeJitter = _e159.lifetimeJitter;
    c.paletteCount = _e159.paletteCount;
    c.colorPalette[0i] = _e159.colorPalette[0];
    c.colorPalette[1i] = _e159.colorPalette[1];
    c.colorPalette[2i] = _e159.colorPalette[2];
    c.colorPalette[3i] = _e159.colorPalette[3];
    c.colorPalette[4i] = _e159.colorPalette[4];
    c.colorPalette[5i] = _e159.colorPalette[5];
    c.colorPalette[6i] = _e159.colorPalette[6];
    c.colorPalette[7i] = _e159.colorPalette[7];
    c.colorPalette[8i] = _e159.colorPalette[8];
    c.colorPalette[9i] = _e159.colorPalette[9];
    c.colorPalette[10i] = _e159.colorPalette[10];
    c.colorPalette[11i] = _e159.colorPalette[11];
    c.colorPalette[12i] = _e159.colorPalette[12];
    c.colorPalette[13i] = _e159.colorPalette[13];
    c.colorPalette[14i] = _e159.colorPalette[14];
    c.colorPalette[15i] = _e159.colorPalette[15];
    c.colorEndMult = _e159.colorEndMult;
    c.sizeStart = _e159.sizeStart;
    c.sizeEnd = _e159.sizeEnd;
    c.gravityScale = _e159.gravityScale;
    c.drag = _e159.drag;
    c.shaderBlendIsAdditive = _e159.shaderBlendIsAdditive;
    c.pad1_ = _e159.pad1_;
    c.pad2_ = _e159.pad2_;
    c.pad3_ = _e159.pad3_;
    c.velocityBias = _e159.velocityBias;
    c.velocityBiasJitter = _e159.velocityBiasJitter;
    c.speedJitter = _e159.speedJitter;
    c.sizeJitter = _e159.sizeJitter;
    c.colorDomain = _e159.colorDomain;
    c.pad5_ = _e159.pad5_;
    c.frameSlots[0i] = _e159.frameSlots[0];
    c.frameSlots[1i] = _e159.frameSlots[1];
    c.frameSlots[2i] = _e159.frameSlots[2];
    c.frameSlots[3i] = _e159.frameSlots[3];
    c.frameSlots[4i] = _e159.frameSlots[4];
    c.frameSlots[5i] = _e159.frameSlots[5];
    c.frameSlots[6i] = _e159.frameSlots[6];
    c.frameSlots[7i] = _e159.frameSlots[7];
    c.frameSlots[8i] = _e159.frameSlots[8];
    c.frameSlots[9i] = _e159.frameSlots[9];
    c.frameSlots[10i] = _e159.frameSlots[10];
    c.frameSlots[11i] = _e159.frameSlots[11];
    c.frameSlots[12i] = _e159.frameSlots[12];
    c.frameSlots[13i] = _e159.frameSlots[13];
    c.frameSlots[14i] = _e159.frameSlots[14];
    c.frameSlots[15i] = _e159.frameSlots[15];
    c.frameCount = _e159.frameCount;
    c.frameBlend = _e159.frameBlend;
    c.framePad0_ = _e159.framePad0_;
    c.framePad1_ = _e159.framePad1_;
    c.sizeParm.calc = _e159.sizeParm.calc;
    c.sizeParm.hasCurve = _e159.sizeParm.hasCurve;
    c.sizeParm.val0_ = _e159.sizeParm.val0_;
    c.sizeParm.val1_ = _e159.sizeParm.val1_;
    c.sizeParm.variance = _e159.sizeParm.variance;
    c.sizeParm.parmPad0_ = _e159.sizeParm.parmPad0_;
    c.sizeParm.parmPad1_ = _e159.sizeParm.parmPad1_;
    c.sizeParm.parmPad2_ = _e159.sizeParm.parmPad2_;
    c.sizeParm.samples[0i] = _e159.sizeParm.samples[0];
    c.sizeParm.samples[1i] = _e159.sizeParm.samples[1];
    c.sizeParm.samples[2i] = _e159.sizeParm.samples[2];
    c.sizeParm.samples[3i] = _e159.sizeParm.samples[3];
    c.sizeParm.samples[4i] = _e159.sizeParm.samples[4];
    c.sizeParm.samples[5i] = _e159.sizeParm.samples[5];
    c.sizeParm.samples[6i] = _e159.sizeParm.samples[6];
    c.sizeParm.samples[7i] = _e159.sizeParm.samples[7];
    c.alphaParm.calc = _e159.alphaParm.calc;
    c.alphaParm.hasCurve = _e159.alphaParm.hasCurve;
    c.alphaParm.val0_ = _e159.alphaParm.val0_;
    c.alphaParm.val1_ = _e159.alphaParm.val1_;
    c.alphaParm.variance = _e159.alphaParm.variance;
    c.alphaParm.parmPad0_ = _e159.alphaParm.parmPad0_;
    c.alphaParm.parmPad1_ = _e159.alphaParm.parmPad1_;
    c.alphaParm.parmPad2_ = _e159.alphaParm.parmPad2_;
    c.alphaParm.samples[0i] = _e159.alphaParm.samples[0];
    c.alphaParm.samples[1i] = _e159.alphaParm.samples[1];
    c.alphaParm.samples[2i] = _e159.alphaParm.samples[2];
    c.alphaParm.samples[3i] = _e159.alphaParm.samples[3];
    c.alphaParm.samples[4i] = _e159.alphaParm.samples[4];
    c.alphaParm.samples[5i] = _e159.alphaParm.samples[5];
    c.alphaParm.samples[6i] = _e159.alphaParm.samples[6];
    c.alphaParm.samples[7i] = _e159.alphaParm.samples[7];
    c.dragParm.calc = _e159.dragParm.calc;
    c.dragParm.hasCurve = _e159.dragParm.hasCurve;
    c.dragParm.val0_ = _e159.dragParm.val0_;
    c.dragParm.val1_ = _e159.dragParm.val1_;
    c.dragParm.variance = _e159.dragParm.variance;
    c.dragParm.parmPad0_ = _e159.dragParm.parmPad0_;
    c.dragParm.parmPad1_ = _e159.dragParm.parmPad1_;
    c.dragParm.parmPad2_ = _e159.dragParm.parmPad2_;
    c.dragParm.samples[0i] = _e159.dragParm.samples[0];
    c.dragParm.samples[1i] = _e159.dragParm.samples[1];
    c.dragParm.samples[2i] = _e159.dragParm.samples[2];
    c.dragParm.samples[3i] = _e159.dragParm.samples[3];
    c.dragParm.samples[4i] = _e159.dragParm.samples[4];
    c.dragParm.samples[5i] = _e159.dragParm.samples[5];
    c.dragParm.samples[6i] = _e159.dragParm.samples[6];
    c.dragParm.samples[7i] = _e159.dragParm.samples[7];
    c.gravityParm.calc = _e159.gravityParm.calc;
    c.gravityParm.hasCurve = _e159.gravityParm.hasCurve;
    c.gravityParm.val0_ = _e159.gravityParm.val0_;
    c.gravityParm.val1_ = _e159.gravityParm.val1_;
    c.gravityParm.variance = _e159.gravityParm.variance;
    c.gravityParm.parmPad0_ = _e159.gravityParm.parmPad0_;
    c.gravityParm.parmPad1_ = _e159.gravityParm.parmPad1_;
    c.gravityParm.parmPad2_ = _e159.gravityParm.parmPad2_;
    c.gravityParm.samples[0i] = _e159.gravityParm.samples[0];
    c.gravityParm.samples[1i] = _e159.gravityParm.samples[1];
    c.gravityParm.samples[2i] = _e159.gravityParm.samples[2];
    c.gravityParm.samples[3i] = _e159.gravityParm.samples[3];
    c.gravityParm.samples[4i] = _e159.gravityParm.samples[4];
    c.gravityParm.samples[5i] = _e159.gravityParm.samples[5];
    c.gravityParm.samples[6i] = _e159.gravityParm.samples[6];
    c.gravityParm.samples[7i] = _e159.gravityParm.samples[7];
    let _e435 = c.shaderBlendIsAdditive;
    if (_e435 != PIPELINE_BLEND_MASK) {
        emitDegenerate_u0028_();
        return;
    }
    let _e437 = vertInQuad;
    indexable = array<f32, 6>(1f, 1f, -1f, -1f, 1f, -1f);
    let _e439 = indexable[_e437];
    sx = _e439;
    let _e440 = vertInQuad;
    indexable_1 = array<f32, 6>(-1f, 1f, -1f, -1f, 1f, 1f);
    let _e442 = indexable_1[_e440];
    uy = _e442;
    let _e444 = p_3.paletteIndex;
    let _e446 = c.paletteCount;
    paletteIdx = (_e444 % bitcast<u32>(max(_e446, 1i)));
    let _e450 = paletteIdx;
    let _e453 = c.colorPalette[_e450];
    baseColor = _e453;
    let _e454 = baseColor;
    let _e456 = c.colorEndMult;
    endColor = (_e454 * _e456);
    let _e458 = baseColor;
    let _e459 = endColor;
    let _e461 = p_3.age;
    color = mix(_e458, _e459, vec4(_e461));
    let _e465 = c.alphaParm;
    param_4 = _e465;
    let _e466 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_4));
    if !(_e466) {
        let _e469 = c.alphaParm;
        param_5 = _e469;
        let _e471 = p_3.age;
        param_6 = _e471;
        let _e473 = p_3.sizeJitterPick;
        param_7 = _e473;
        let _e474 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_5), (&param_6), (&param_7));
        color[3u] = _e474;
    }
    let _e477 = p_3.pad5_;
    let _e479 = color;
    color = (_e479 * unpack4x8unorm(_e477));
    let _e482 = c.sizeStart;
    let _e484 = p_3.sizeJitterPick;
    effectiveStart = (_e482 + _e484);
    let _e486 = effectiveStart;
    let _e488 = c.sizeEnd;
    let _e490 = p_3.age;
    size = mix(_e486, _e488, _e490);
    let _e493 = c.sizeParm;
    param_8 = _e493;
    let _e494 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_8));
    if !(_e494) {
        let _e497 = c.sizeParm;
        param_9 = _e497;
        let _e499 = p_3.age;
        param_10 = _e499;
        let _e501 = p_3.sizeJitterPick;
        param_11 = _e501;
        let _e502 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_9), (&param_10), (&param_11));
        size = _e502;
    }
    let _e504 = p_3.pos;
    let _e506 = unnamed_1.viewLeft;
    let _e508 = sx;
    let _e509 = size;
    let _e514 = unnamed_1.viewUp;
    let _e516 = uy;
    let _e517 = size;
    worldPos = ((_e504 + (_e506.xyz * (_e508 * _e509))) + (_e514.xyz * (_e516 * _e517)));
    let _e522 = unnamed_1.mvp;
    let _e523 = worldPos;
    unnamed.gl_Position = (_e522 * vec4<f32>(_e523.x, _e523.y, _e523.z, 1f));
    let _e530 = vertInQuad;
    indexable_2 = array<vec2<f32>, 6>(vec2<f32>(0f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 1f), vec2<f32>(1f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 0f));
    let _e532 = indexable_2[_e530];
    fragUV = _e532;
    let _e533 = color;
    fragColor = _e533;
    let _e535 = p_3.classHandle;
    let _e537 = c.colorDomain;
    particleClassHandle = (_e535 | (_e537 << bitcast<u32>(31u)));
    let _e542 = c.frameCount;
    if (_e542 > 1u) {
        let _e545 = p_3.age;
        let _e547 = c.frameCount;
        fr = (_e545 * f32(_e547));
        let _e551 = c.frameCount;
        last = (_e551 - 1u);
        let _e553 = fr;
        let _e555 = last;
        frame0_ = u32(clamp(floor(_e553), 0f, f32(_e555)));
        let _e559 = frame0_;
        let _e561 = last;
        frame1_ = min((_e559 + 1u), _e561);
        let _e563 = frame0_;
        let _e566 = c.frameSlots[_e563];
        frameSlot0_ = _e566;
        let _e567 = frame1_;
        let _e570 = c.frameSlots[_e567];
        frameSlot1_ = _e570;
        let _e572 = c.frameBlend;
        if (_e572 != 0u) {
            let _e574 = fr;
            local = fract(_e574);
        } else {
            local = 0f;
        }
        let _e576 = local;
        frameBlend = _e576;
    } else {
        frameSlot0_ = 4294967295u;
        frameSlot1_ = 4294967295u;
        frameBlend = 0f;
    }
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e15 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e15);
    let _e17 = unnamed.gl_Position;
    let _e18 = fragUV;
    let _e19 = fragColor;
    let _e20 = particleClassHandle;
    let _e21 = frameSlot0_;
    let _e22 = frameSlot1_;
    let _e23 = frameBlend;
    return VertexOutput(_e17, _e18, _e19, _e20, _e21, _e22, _e23);
}
