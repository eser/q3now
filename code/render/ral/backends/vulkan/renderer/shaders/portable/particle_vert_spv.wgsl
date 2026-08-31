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
    @location(6) @interpolate(flat) member_6: u32,
}

@id(0) override PIPELINE_BLEND_MASK: u32 = 0u;

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;
var<private> particleClassHandle: u32;
var<private> frameSlot0_: u32;
var<private> frameSlot1_: u32;
var<private> frameBlend: f32;
var<private> particleRenderFlags: u32;
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

    let _e88 = (*p).hasCurve;
    if (_e88 == 0i) {
        return 1f;
    }
    let _e90 = (*fraction);
    if (_e90 <= 0f) {
        let _e94 = (*p).samples[0i];
        return _e94;
    }
    let _e95 = (*fraction);
    if (_e95 >= 1f) {
        let _e99 = (*p).samples[7i];
        return _e99;
    }
    let _e100 = (*fraction);
    pos = (_e100 * 7f);
    let _e102 = pos;
    i0_ = i32(_e102);
    let _e104 = i0_;
    i1_ = min((_e104 + 1i), 7i);
    let _e107 = pos;
    let _e108 = i0_;
    frac = (_e107 - f32(_e108));
    let _e111 = i0_;
    let _e114 = (*p).samples[_e111];
    let _e115 = i1_;
    let _e118 = (*p).samples[_e115];
    let _e119 = frac;
    return mix(_e114, _e118, _e119);
}

fn parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b(p_1: ptr<function, ParticleParm>, fraction_1: ptr<function, f32>, jitterPick: ptr<function, f32>) -> f32 {
    var base: f32;
    var param: ParticleParm;
    var param_1: f32;
    var param_2: ParticleParm;
    var param_3: f32;

    let _e89 = (*fraction_1);
    (*fraction_1) = clamp(_e89, 0f, 1f);
    let _e92 = (*p_1).calc;
    if (_e92 == 1i) {
        let _e95 = (*p_1).val0_;
        let _e97 = (*p_1).val1_;
        let _e98 = (*fraction_1);
        base = mix(_e95, _e97, _e98);
    } else {
        let _e101 = (*p_1).calc;
        if (_e101 == 2i) {
            let _e104 = (*p_1).val0_;
            let _e106 = (*p_1).val1_;
            let _e107 = (*p_1);
            param = _e107;
            let _e108 = (*fraction_1);
            param_1 = _e108;
            let _e109 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param), (&param_1));
            base = mix(_e104, _e106, _e109);
        } else {
            let _e112 = (*p_1).calc;
            if (_e112 == 3i) {
                let _e114 = (*p_1);
                param_2 = _e114;
                let _e115 = (*fraction_1);
                param_3 = _e115;
                let _e116 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param_2), (&param_3));
                let _e118 = (*p_1).val0_;
                let _e120 = (*p_1).val1_;
                let _e121 = (*fraction_1);
                base = (_e116 * mix(_e118, _e120, _e121));
            } else {
                let _e125 = (*p_1).val0_;
                base = _e125;
            }
        }
    }
    let _e126 = base;
    let _e128 = (*p_1).variance;
    let _e129 = (*jitterPick);
    return (_e126 + (_e128 * _e129));
}

fn parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b(p_2: ptr<function, ParticleParm>) -> bool {
    var phi_155_: bool;

    let _e83 = (*p_2).calc;
    let _e84 = (_e83 == 0i);
    phi_155_ = _e84;
    if _e84 {
        let _e86 = (*p_2).val0_;
        phi_155_ = (_e86 == 0f);
    }
    let _e89 = phi_155_;
    return _e89;
}

fn emitDegenerate_u0028_() {
    unnamed.gl_Position = vec4<f32>(0f, 0f, 0f, 1f);
    fragUV = vec2<f32>(0f, 0f);
    fragColor = vec4<f32>(0f, 0f, 0f, 0f);
    particleClassHandle = 1u;
    frameSlot0_ = 4294967295u;
    frameSlot1_ = 4294967295u;
    frameBlend = 0f;
    particleRenderFlags = 0u;
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
    var ageSeconds: f32;
    var trailTime: f32;
    var head: vec3<f32>;
    var tail: vec3<f32>;
    var motion: vec3<f32>;
    var motionLength: f32;
    var midpoint: vec3<f32>;
    var viewDirection: vec3<f32>;
    var widthAxis: vec3<f32>;
    var widthLength: f32;
    var alongTrail: f32;
    var worldPos: vec3<f32>;
    var indexable_2: array<vec2<f32>, 6>;
    var fr: f32;
    var last: u32;
    var frame0_: u32;
    var frame1_: u32;
    var local: f32;
    var phi_258_: bool;
    var phi_267_: bool;
    var phi_711_: bool;

    let _e121 = gl_InstanceIndex_1;
    particleIdx = bitcast<u32>(_e121);
    let _e123 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e123) % 6u);
    let _e126 = particleIdx;
    let _e128 = unnamed_1.poolSize;
    if (_e126 >= _e128) {
        emitDegenerate_u0028_();
        return;
    }
    let _e130 = particleIdx;
    let _e133 = unnamed_2.particles[_e130];
    p_3.pos = _e133.pos;
    p_3.age = _e133.age;
    p_3.vel = _e133.vel;
    p_3.lifetimeInv = _e133.lifetimeInv;
    p_3.classHandle = _e133.classHandle;
    p_3.paletteIndex = _e133.paletteIndex;
    p_3.sizeJitterPick = _e133.sizeJitterPick;
    p_3.pad1_ = _e133.pad1_;
    p_3.pad2_ = _e133.pad2_;
    p_3.pad3_ = _e133.pad3_;
    p_3.pad4_ = _e133.pad4_;
    p_3.pad5_ = _e133.pad5_;
    let _e159 = p_3.classHandle;
    let _e160 = (_e159 == 0u);
    phi_258_ = _e160;
    if !(_e160) {
        let _e163 = p_3.age;
        phi_258_ = (_e163 >= 1f);
    }
    let _e166 = phi_258_;
    phi_267_ = _e166;
    if !(_e166) {
        let _e169 = p_3.classHandle;
        let _e171 = unnamed_1.numClasses;
        phi_267_ = (_e169 > _e171);
    }
    let _e174 = phi_267_;
    if _e174 {
        emitDegenerate_u0028_();
        return;
    }
    let _e176 = p_3.classHandle;
    let _e180 = unnamed_3.classes[(_e176 - 1u)];
    c.shader = _e180.shader;
    c.renderFlags = _e180.renderFlags;
    c.emitMode = _e180.emitMode;
    c.scatterShape = _e180.scatterShape;
    c.scatterMagnitude = _e180.scatterMagnitude;
    c.velocityShape = _e180.velocityShape;
    c.axialSpeed = _e180.axialSpeed;
    c.cubeJitter = _e180.cubeJitter;
    c.coneHalfAngle = _e180.coneHalfAngle;
    c.lifetimeMean = _e180.lifetimeMean;
    c.lifetimeJitter = _e180.lifetimeJitter;
    c.paletteCount = _e180.paletteCount;
    c.colorPalette[0i] = _e180.colorPalette[0];
    c.colorPalette[1i] = _e180.colorPalette[1];
    c.colorPalette[2i] = _e180.colorPalette[2];
    c.colorPalette[3i] = _e180.colorPalette[3];
    c.colorPalette[4i] = _e180.colorPalette[4];
    c.colorPalette[5i] = _e180.colorPalette[5];
    c.colorPalette[6i] = _e180.colorPalette[6];
    c.colorPalette[7i] = _e180.colorPalette[7];
    c.colorPalette[8i] = _e180.colorPalette[8];
    c.colorPalette[9i] = _e180.colorPalette[9];
    c.colorPalette[10i] = _e180.colorPalette[10];
    c.colorPalette[11i] = _e180.colorPalette[11];
    c.colorPalette[12i] = _e180.colorPalette[12];
    c.colorPalette[13i] = _e180.colorPalette[13];
    c.colorPalette[14i] = _e180.colorPalette[14];
    c.colorPalette[15i] = _e180.colorPalette[15];
    c.colorEndMult = _e180.colorEndMult;
    c.sizeStart = _e180.sizeStart;
    c.sizeEnd = _e180.sizeEnd;
    c.gravityScale = _e180.gravityScale;
    c.drag = _e180.drag;
    c.shaderBlendIsAdditive = _e180.shaderBlendIsAdditive;
    c.pad1_ = _e180.pad1_;
    c.pad2_ = _e180.pad2_;
    c.pad3_ = _e180.pad3_;
    c.velocityBias = _e180.velocityBias;
    c.velocityBiasJitter = _e180.velocityBiasJitter;
    c.speedJitter = _e180.speedJitter;
    c.sizeJitter = _e180.sizeJitter;
    c.colorDomain = _e180.colorDomain;
    c.pad5_ = _e180.pad5_;
    c.frameSlots[0i] = _e180.frameSlots[0];
    c.frameSlots[1i] = _e180.frameSlots[1];
    c.frameSlots[2i] = _e180.frameSlots[2];
    c.frameSlots[3i] = _e180.frameSlots[3];
    c.frameSlots[4i] = _e180.frameSlots[4];
    c.frameSlots[5i] = _e180.frameSlots[5];
    c.frameSlots[6i] = _e180.frameSlots[6];
    c.frameSlots[7i] = _e180.frameSlots[7];
    c.frameSlots[8i] = _e180.frameSlots[8];
    c.frameSlots[9i] = _e180.frameSlots[9];
    c.frameSlots[10i] = _e180.frameSlots[10];
    c.frameSlots[11i] = _e180.frameSlots[11];
    c.frameSlots[12i] = _e180.frameSlots[12];
    c.frameSlots[13i] = _e180.frameSlots[13];
    c.frameSlots[14i] = _e180.frameSlots[14];
    c.frameSlots[15i] = _e180.frameSlots[15];
    c.frameCount = _e180.frameCount;
    c.frameBlend = _e180.frameBlend;
    c.framePad0_ = _e180.framePad0_;
    c.framePad1_ = _e180.framePad1_;
    c.sizeParm.calc = _e180.sizeParm.calc;
    c.sizeParm.hasCurve = _e180.sizeParm.hasCurve;
    c.sizeParm.val0_ = _e180.sizeParm.val0_;
    c.sizeParm.val1_ = _e180.sizeParm.val1_;
    c.sizeParm.variance = _e180.sizeParm.variance;
    c.sizeParm.parmPad0_ = _e180.sizeParm.parmPad0_;
    c.sizeParm.parmPad1_ = _e180.sizeParm.parmPad1_;
    c.sizeParm.parmPad2_ = _e180.sizeParm.parmPad2_;
    c.sizeParm.samples[0i] = _e180.sizeParm.samples[0];
    c.sizeParm.samples[1i] = _e180.sizeParm.samples[1];
    c.sizeParm.samples[2i] = _e180.sizeParm.samples[2];
    c.sizeParm.samples[3i] = _e180.sizeParm.samples[3];
    c.sizeParm.samples[4i] = _e180.sizeParm.samples[4];
    c.sizeParm.samples[5i] = _e180.sizeParm.samples[5];
    c.sizeParm.samples[6i] = _e180.sizeParm.samples[6];
    c.sizeParm.samples[7i] = _e180.sizeParm.samples[7];
    c.alphaParm.calc = _e180.alphaParm.calc;
    c.alphaParm.hasCurve = _e180.alphaParm.hasCurve;
    c.alphaParm.val0_ = _e180.alphaParm.val0_;
    c.alphaParm.val1_ = _e180.alphaParm.val1_;
    c.alphaParm.variance = _e180.alphaParm.variance;
    c.alphaParm.parmPad0_ = _e180.alphaParm.parmPad0_;
    c.alphaParm.parmPad1_ = _e180.alphaParm.parmPad1_;
    c.alphaParm.parmPad2_ = _e180.alphaParm.parmPad2_;
    c.alphaParm.samples[0i] = _e180.alphaParm.samples[0];
    c.alphaParm.samples[1i] = _e180.alphaParm.samples[1];
    c.alphaParm.samples[2i] = _e180.alphaParm.samples[2];
    c.alphaParm.samples[3i] = _e180.alphaParm.samples[3];
    c.alphaParm.samples[4i] = _e180.alphaParm.samples[4];
    c.alphaParm.samples[5i] = _e180.alphaParm.samples[5];
    c.alphaParm.samples[6i] = _e180.alphaParm.samples[6];
    c.alphaParm.samples[7i] = _e180.alphaParm.samples[7];
    c.dragParm.calc = _e180.dragParm.calc;
    c.dragParm.hasCurve = _e180.dragParm.hasCurve;
    c.dragParm.val0_ = _e180.dragParm.val0_;
    c.dragParm.val1_ = _e180.dragParm.val1_;
    c.dragParm.variance = _e180.dragParm.variance;
    c.dragParm.parmPad0_ = _e180.dragParm.parmPad0_;
    c.dragParm.parmPad1_ = _e180.dragParm.parmPad1_;
    c.dragParm.parmPad2_ = _e180.dragParm.parmPad2_;
    c.dragParm.samples[0i] = _e180.dragParm.samples[0];
    c.dragParm.samples[1i] = _e180.dragParm.samples[1];
    c.dragParm.samples[2i] = _e180.dragParm.samples[2];
    c.dragParm.samples[3i] = _e180.dragParm.samples[3];
    c.dragParm.samples[4i] = _e180.dragParm.samples[4];
    c.dragParm.samples[5i] = _e180.dragParm.samples[5];
    c.dragParm.samples[6i] = _e180.dragParm.samples[6];
    c.dragParm.samples[7i] = _e180.dragParm.samples[7];
    c.gravityParm.calc = _e180.gravityParm.calc;
    c.gravityParm.hasCurve = _e180.gravityParm.hasCurve;
    c.gravityParm.val0_ = _e180.gravityParm.val0_;
    c.gravityParm.val1_ = _e180.gravityParm.val1_;
    c.gravityParm.variance = _e180.gravityParm.variance;
    c.gravityParm.parmPad0_ = _e180.gravityParm.parmPad0_;
    c.gravityParm.parmPad1_ = _e180.gravityParm.parmPad1_;
    c.gravityParm.parmPad2_ = _e180.gravityParm.parmPad2_;
    c.gravityParm.samples[0i] = _e180.gravityParm.samples[0];
    c.gravityParm.samples[1i] = _e180.gravityParm.samples[1];
    c.gravityParm.samples[2i] = _e180.gravityParm.samples[2];
    c.gravityParm.samples[3i] = _e180.gravityParm.samples[3];
    c.gravityParm.samples[4i] = _e180.gravityParm.samples[4];
    c.gravityParm.samples[5i] = _e180.gravityParm.samples[5];
    c.gravityParm.samples[6i] = _e180.gravityParm.samples[6];
    c.gravityParm.samples[7i] = _e180.gravityParm.samples[7];
    let _e456 = c.shaderBlendIsAdditive;
    if (_e456 != PIPELINE_BLEND_MASK) {
        emitDegenerate_u0028_();
        return;
    }
    let _e458 = vertInQuad;
    indexable = array<f32, 6>(1f, 1f, -1f, -1f, 1f, -1f);
    let _e460 = indexable[_e458];
    sx = _e460;
    let _e461 = vertInQuad;
    indexable_1 = array<f32, 6>(-1f, 1f, -1f, -1f, 1f, 1f);
    let _e463 = indexable_1[_e461];
    uy = _e463;
    let _e465 = p_3.paletteIndex;
    let _e467 = c.paletteCount;
    paletteIdx = (_e465 % bitcast<u32>(max(_e467, 1i)));
    let _e471 = paletteIdx;
    let _e474 = c.colorPalette[_e471];
    baseColor = _e474;
    let _e475 = baseColor;
    let _e477 = c.colorEndMult;
    endColor = (_e475 * _e477);
    let _e479 = baseColor;
    let _e480 = endColor;
    let _e482 = p_3.age;
    color = mix(_e479, _e480, vec4(_e482));
    let _e486 = c.alphaParm;
    param_4 = _e486;
    let _e487 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_4));
    if !(_e487) {
        let _e490 = c.alphaParm;
        param_5 = _e490;
        let _e492 = p_3.age;
        param_6 = _e492;
        let _e494 = p_3.sizeJitterPick;
        param_7 = _e494;
        let _e495 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_5), (&param_6), (&param_7));
        color[3u] = _e495;
    }
    let _e498 = p_3.pad5_;
    let _e500 = color;
    color = (_e500 * unpack4x8unorm(_e498));
    let _e503 = c.sizeStart;
    let _e505 = p_3.sizeJitterPick;
    effectiveStart = (_e503 + _e505);
    let _e507 = effectiveStart;
    let _e509 = c.sizeEnd;
    let _e511 = p_3.age;
    size = mix(_e507, _e509, _e511);
    let _e514 = c.sizeParm;
    param_8 = _e514;
    let _e515 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_8));
    if !(_e515) {
        let _e518 = c.sizeParm;
        param_9 = _e518;
        let _e520 = p_3.age;
        param_10 = _e520;
        let _e522 = p_3.sizeJitterPick;
        param_11 = _e522;
        let _e523 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_9), (&param_10), (&param_11));
        size = _e523;
    }
    let _e525 = c.renderFlags;
    let _e527 = ((_e525 & 64u) != 0u);
    phi_711_ = _e527;
    if _e527 {
        let _e529 = p_3.vel;
        let _e531 = p_3.vel;
        phi_711_ = (dot(_e529, _e531) > 0.0001f);
    }
    let _e535 = phi_711_;
    if _e535 {
        let _e537 = p_3.age;
        let _e539 = p_3.lifetimeInv;
        ageSeconds = (_e537 / max(_e539, 0.000001f));
        let _e542 = ageSeconds;
        trailTime = min(_e542, 0.1f);
        let _e545 = p_3.pos;
        head = _e545;
        let _e546 = head;
        let _e548 = p_3.vel;
        let _e549 = trailTime;
        tail = (_e546 - (_e548 * _e549));
        let _e553 = c.gravityScale;
        let _e555 = trailTime;
        let _e557 = trailTime;
        let _e560 = tail[2u];
        tail[2u] = (_e560 - (((400f * _e553) * _e555) * _e557));
        let _e563 = head;
        let _e564 = tail;
        motion = (_e563 - _e564);
        let _e566 = motion;
        motionLength = length(_e566);
        let _e568 = head;
        let _e569 = tail;
        midpoint = ((_e568 + _e569) * 0.5f);
        let _e573 = unnamed_1.eyeWorld;
        let _e575 = midpoint;
        viewDirection = normalize((_e573.xyz - _e575));
        let _e578 = motion;
        let _e579 = motionLength;
        let _e583 = viewDirection;
        widthAxis = cross((_e578 / vec3(max(_e579, 0.000001f))), _e583);
        let _e585 = widthAxis;
        widthLength = length(_e585);
        let _e587 = motionLength;
        let _e589 = widthLength;
        if ((_e587 > 0.001f) && (_e589 > 0.001f)) {
            let _e592 = widthLength;
            let _e593 = widthAxis;
            widthAxis = (_e593 / vec3(_e592));
            let _e596 = uy;
            alongTrail = ((_e596 + 1f) * 0.5f);
            let _e599 = tail;
            let _e600 = head;
            let _e601 = alongTrail;
            let _e604 = widthAxis;
            let _e605 = sx;
            let _e606 = size;
            worldPos = (mix(_e599, _e600, vec3(_e601)) + (_e604 * (_e605 * _e606)));
        } else {
            let _e611 = p_3.pos;
            let _e613 = unnamed_1.viewLeft;
            let _e615 = sx;
            let _e616 = size;
            let _e621 = unnamed_1.viewUp;
            let _e623 = uy;
            let _e624 = size;
            worldPos = ((_e611 + (_e613.xyz * (_e615 * _e616))) + (_e621.xyz * (_e623 * _e624)));
        }
    } else {
        let _e629 = p_3.pos;
        let _e631 = unnamed_1.viewLeft;
        let _e633 = sx;
        let _e634 = size;
        let _e639 = unnamed_1.viewUp;
        let _e641 = uy;
        let _e642 = size;
        worldPos = ((_e629 + (_e631.xyz * (_e633 * _e634))) + (_e639.xyz * (_e641 * _e642)));
    }
    let _e647 = unnamed_1.mvp;
    let _e648 = worldPos;
    unnamed.gl_Position = (_e647 * vec4<f32>(_e648.x, _e648.y, _e648.z, 1f));
    let _e655 = vertInQuad;
    indexable_2 = array<vec2<f32>, 6>(vec2<f32>(0f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 1f), vec2<f32>(1f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 0f));
    let _e657 = indexable_2[_e655];
    fragUV = _e657;
    let _e658 = color;
    fragColor = _e658;
    let _e660 = p_3.classHandle;
    let _e662 = c.colorDomain;
    particleClassHandle = (_e660 | (_e662 << bitcast<u32>(31u)));
    let _e667 = c.renderFlags;
    particleRenderFlags = _e667;
    let _e669 = c.frameCount;
    if (_e669 > 1u) {
        let _e672 = p_3.age;
        let _e674 = c.frameCount;
        fr = (_e672 * f32(_e674));
        let _e678 = c.frameCount;
        last = (_e678 - 1u);
        let _e680 = fr;
        let _e682 = last;
        frame0_ = u32(clamp(floor(_e680), 0f, f32(_e682)));
        let _e686 = frame0_;
        let _e688 = last;
        frame1_ = min((_e686 + 1u), _e688);
        let _e690 = frame0_;
        let _e693 = c.frameSlots[_e690];
        frameSlot0_ = _e693;
        let _e694 = frame1_;
        let _e697 = c.frameSlots[_e694];
        frameSlot1_ = _e697;
        let _e699 = c.frameBlend;
        if (_e699 != 0u) {
            let _e701 = fr;
            local = fract(_e701);
        } else {
            local = 0f;
        }
        let _e703 = local;
        frameBlend = _e703;
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
    let _e16 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e16);
    let _e18 = unnamed.gl_Position;
    let _e19 = fragUV;
    let _e20 = fragColor;
    let _e21 = particleClassHandle;
    let _e22 = frameSlot0_;
    let _e23 = frameSlot1_;
    let _e24 = frameBlend;
    let _e25 = particleRenderFlags;
    return VertexOutput(_e18, _e19, _e20, _e21, _e22, _e23, _e24, _e25);
}
