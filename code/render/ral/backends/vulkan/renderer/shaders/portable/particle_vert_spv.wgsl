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

    let _e94 = (*p).hasCurve;
    if (_e94 == 0i) {
        return 1f;
    }
    let _e96 = (*fraction);
    if (_e96 <= 0f) {
        let _e100 = (*p).samples[0i];
        return _e100;
    }
    let _e101 = (*fraction);
    if (_e101 >= 1f) {
        let _e105 = (*p).samples[7i];
        return _e105;
    }
    let _e106 = (*fraction);
    pos = (_e106 * 7f);
    let _e108 = pos;
    i0_ = i32(_e108);
    let _e110 = i0_;
    i1_ = min((_e110 + 1i), 7i);
    let _e113 = pos;
    let _e114 = i0_;
    frac = (_e113 - f32(_e114));
    let _e117 = i0_;
    let _e120 = (*p).samples[_e117];
    let _e121 = i1_;
    let _e124 = (*p).samples[_e121];
    let _e125 = frac;
    return mix(_e120, _e124, _e125);
}

fn parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b(p_1: ptr<function, ParticleParm>, fraction_1: ptr<function, f32>, jitterPick: ptr<function, f32>) -> f32 {
    var base: f32;
    var param: ParticleParm;
    var param_1: f32;
    var param_2: ParticleParm;
    var param_3: f32;

    let _e95 = (*fraction_1);
    (*fraction_1) = clamp(_e95, 0f, 1f);
    let _e98 = (*p_1).calc;
    if (_e98 == 1i) {
        let _e101 = (*p_1).val0_;
        let _e103 = (*p_1).val1_;
        let _e104 = (*fraction_1);
        base = mix(_e101, _e103, _e104);
    } else {
        let _e107 = (*p_1).calc;
        if (_e107 == 2i) {
            let _e110 = (*p_1).val0_;
            let _e112 = (*p_1).val1_;
            let _e113 = (*p_1);
            param = _e113;
            let _e114 = (*fraction_1);
            param_1 = _e114;
            let _e115 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param), (&param_1));
            base = mix(_e110, _e112, _e115);
        } else {
            let _e118 = (*p_1).calc;
            if (_e118 == 3i) {
                let _e120 = (*p_1);
                param_2 = _e120;
                let _e121 = (*fraction_1);
                param_3 = _e121;
                let _e122 = parmSampleCurve_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b((&param_2), (&param_3));
                let _e124 = (*p_1).val0_;
                let _e126 = (*p_1).val1_;
                let _e127 = (*fraction_1);
                base = (_e122 * mix(_e124, _e126, _e127));
            } else {
                let _e131 = (*p_1).val0_;
                base = _e131;
            }
        }
    }
    let _e132 = base;
    let _e134 = (*p_1).variance;
    let _e135 = (*jitterPick);
    return (_e132 + (_e134 * _e135));
}

fn parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b(p_2: ptr<function, ParticleParm>) -> bool {
    var phi_155_: bool;

    let _e89 = (*p_2).calc;
    let _e90 = (_e89 == 0i);
    phi_155_ = _e90;
    if _e90 {
        let _e92 = (*p_2).val0_;
        phi_155_ = (_e92 == 0f);
    }
    let _e95 = phi_155_;
    return _e95;
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
    var normal: vec3<f32>;
    var reference: vec3<f32>;
    var tangent: vec3<f32>;
    var bitangent: vec3<f32>;
    var roll: f32;
    var cr: f32;
    var sr: f32;
    var rolledTangent: vec3<f32>;
    var rolledBitangent: vec3<f32>;
    var indexable_2: array<vec2<f32>, 6>;
    var fr: f32;
    var last: u32;
    var frame0_: u32;
    var frame1_: u32;
    var local: f32;
    var phi_258_: bool;
    var phi_267_: bool;
    var phi_711_: bool;
    var phi_842_: bool;

    let _e136 = gl_InstanceIndex_1;
    particleIdx = bitcast<u32>(_e136);
    let _e138 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e138) % 6u);
    let _e141 = particleIdx;
    let _e143 = unnamed_1.poolSize;
    if (_e141 >= _e143) {
        emitDegenerate_u0028_();
        return;
    }
    let _e145 = particleIdx;
    let _e148 = unnamed_2.particles[_e145];
    p_3.pos = _e148.pos;
    p_3.age = _e148.age;
    p_3.vel = _e148.vel;
    p_3.lifetimeInv = _e148.lifetimeInv;
    p_3.classHandle = _e148.classHandle;
    p_3.paletteIndex = _e148.paletteIndex;
    p_3.sizeJitterPick = _e148.sizeJitterPick;
    p_3.pad1_ = _e148.pad1_;
    p_3.pad2_ = _e148.pad2_;
    p_3.pad3_ = _e148.pad3_;
    p_3.pad4_ = _e148.pad4_;
    p_3.pad5_ = _e148.pad5_;
    let _e174 = p_3.classHandle;
    let _e175 = (_e174 == 0u);
    phi_258_ = _e175;
    if !(_e175) {
        let _e178 = p_3.age;
        phi_258_ = (_e178 >= 1f);
    }
    let _e181 = phi_258_;
    phi_267_ = _e181;
    if !(_e181) {
        let _e184 = p_3.classHandle;
        let _e186 = unnamed_1.numClasses;
        phi_267_ = (_e184 > _e186);
    }
    let _e189 = phi_267_;
    if _e189 {
        emitDegenerate_u0028_();
        return;
    }
    let _e191 = p_3.classHandle;
    let _e195 = unnamed_3.classes[(_e191 - 1u)];
    c.shader = _e195.shader;
    c.renderFlags = _e195.renderFlags;
    c.emitMode = _e195.emitMode;
    c.scatterShape = _e195.scatterShape;
    c.scatterMagnitude = _e195.scatterMagnitude;
    c.velocityShape = _e195.velocityShape;
    c.axialSpeed = _e195.axialSpeed;
    c.cubeJitter = _e195.cubeJitter;
    c.coneHalfAngle = _e195.coneHalfAngle;
    c.lifetimeMean = _e195.lifetimeMean;
    c.lifetimeJitter = _e195.lifetimeJitter;
    c.paletteCount = _e195.paletteCount;
    c.colorPalette[0i] = _e195.colorPalette[0];
    c.colorPalette[1i] = _e195.colorPalette[1];
    c.colorPalette[2i] = _e195.colorPalette[2];
    c.colorPalette[3i] = _e195.colorPalette[3];
    c.colorPalette[4i] = _e195.colorPalette[4];
    c.colorPalette[5i] = _e195.colorPalette[5];
    c.colorPalette[6i] = _e195.colorPalette[6];
    c.colorPalette[7i] = _e195.colorPalette[7];
    c.colorPalette[8i] = _e195.colorPalette[8];
    c.colorPalette[9i] = _e195.colorPalette[9];
    c.colorPalette[10i] = _e195.colorPalette[10];
    c.colorPalette[11i] = _e195.colorPalette[11];
    c.colorPalette[12i] = _e195.colorPalette[12];
    c.colorPalette[13i] = _e195.colorPalette[13];
    c.colorPalette[14i] = _e195.colorPalette[14];
    c.colorPalette[15i] = _e195.colorPalette[15];
    c.colorEndMult = _e195.colorEndMult;
    c.sizeStart = _e195.sizeStart;
    c.sizeEnd = _e195.sizeEnd;
    c.gravityScale = _e195.gravityScale;
    c.drag = _e195.drag;
    c.shaderBlendIsAdditive = _e195.shaderBlendIsAdditive;
    c.pad1_ = _e195.pad1_;
    c.pad2_ = _e195.pad2_;
    c.pad3_ = _e195.pad3_;
    c.velocityBias = _e195.velocityBias;
    c.velocityBiasJitter = _e195.velocityBiasJitter;
    c.speedJitter = _e195.speedJitter;
    c.sizeJitter = _e195.sizeJitter;
    c.colorDomain = _e195.colorDomain;
    c.pad5_ = _e195.pad5_;
    c.frameSlots[0i] = _e195.frameSlots[0];
    c.frameSlots[1i] = _e195.frameSlots[1];
    c.frameSlots[2i] = _e195.frameSlots[2];
    c.frameSlots[3i] = _e195.frameSlots[3];
    c.frameSlots[4i] = _e195.frameSlots[4];
    c.frameSlots[5i] = _e195.frameSlots[5];
    c.frameSlots[6i] = _e195.frameSlots[6];
    c.frameSlots[7i] = _e195.frameSlots[7];
    c.frameSlots[8i] = _e195.frameSlots[8];
    c.frameSlots[9i] = _e195.frameSlots[9];
    c.frameSlots[10i] = _e195.frameSlots[10];
    c.frameSlots[11i] = _e195.frameSlots[11];
    c.frameSlots[12i] = _e195.frameSlots[12];
    c.frameSlots[13i] = _e195.frameSlots[13];
    c.frameSlots[14i] = _e195.frameSlots[14];
    c.frameSlots[15i] = _e195.frameSlots[15];
    c.frameCount = _e195.frameCount;
    c.frameBlend = _e195.frameBlend;
    c.framePad0_ = _e195.framePad0_;
    c.framePad1_ = _e195.framePad1_;
    c.sizeParm.calc = _e195.sizeParm.calc;
    c.sizeParm.hasCurve = _e195.sizeParm.hasCurve;
    c.sizeParm.val0_ = _e195.sizeParm.val0_;
    c.sizeParm.val1_ = _e195.sizeParm.val1_;
    c.sizeParm.variance = _e195.sizeParm.variance;
    c.sizeParm.parmPad0_ = _e195.sizeParm.parmPad0_;
    c.sizeParm.parmPad1_ = _e195.sizeParm.parmPad1_;
    c.sizeParm.parmPad2_ = _e195.sizeParm.parmPad2_;
    c.sizeParm.samples[0i] = _e195.sizeParm.samples[0];
    c.sizeParm.samples[1i] = _e195.sizeParm.samples[1];
    c.sizeParm.samples[2i] = _e195.sizeParm.samples[2];
    c.sizeParm.samples[3i] = _e195.sizeParm.samples[3];
    c.sizeParm.samples[4i] = _e195.sizeParm.samples[4];
    c.sizeParm.samples[5i] = _e195.sizeParm.samples[5];
    c.sizeParm.samples[6i] = _e195.sizeParm.samples[6];
    c.sizeParm.samples[7i] = _e195.sizeParm.samples[7];
    c.alphaParm.calc = _e195.alphaParm.calc;
    c.alphaParm.hasCurve = _e195.alphaParm.hasCurve;
    c.alphaParm.val0_ = _e195.alphaParm.val0_;
    c.alphaParm.val1_ = _e195.alphaParm.val1_;
    c.alphaParm.variance = _e195.alphaParm.variance;
    c.alphaParm.parmPad0_ = _e195.alphaParm.parmPad0_;
    c.alphaParm.parmPad1_ = _e195.alphaParm.parmPad1_;
    c.alphaParm.parmPad2_ = _e195.alphaParm.parmPad2_;
    c.alphaParm.samples[0i] = _e195.alphaParm.samples[0];
    c.alphaParm.samples[1i] = _e195.alphaParm.samples[1];
    c.alphaParm.samples[2i] = _e195.alphaParm.samples[2];
    c.alphaParm.samples[3i] = _e195.alphaParm.samples[3];
    c.alphaParm.samples[4i] = _e195.alphaParm.samples[4];
    c.alphaParm.samples[5i] = _e195.alphaParm.samples[5];
    c.alphaParm.samples[6i] = _e195.alphaParm.samples[6];
    c.alphaParm.samples[7i] = _e195.alphaParm.samples[7];
    c.dragParm.calc = _e195.dragParm.calc;
    c.dragParm.hasCurve = _e195.dragParm.hasCurve;
    c.dragParm.val0_ = _e195.dragParm.val0_;
    c.dragParm.val1_ = _e195.dragParm.val1_;
    c.dragParm.variance = _e195.dragParm.variance;
    c.dragParm.parmPad0_ = _e195.dragParm.parmPad0_;
    c.dragParm.parmPad1_ = _e195.dragParm.parmPad1_;
    c.dragParm.parmPad2_ = _e195.dragParm.parmPad2_;
    c.dragParm.samples[0i] = _e195.dragParm.samples[0];
    c.dragParm.samples[1i] = _e195.dragParm.samples[1];
    c.dragParm.samples[2i] = _e195.dragParm.samples[2];
    c.dragParm.samples[3i] = _e195.dragParm.samples[3];
    c.dragParm.samples[4i] = _e195.dragParm.samples[4];
    c.dragParm.samples[5i] = _e195.dragParm.samples[5];
    c.dragParm.samples[6i] = _e195.dragParm.samples[6];
    c.dragParm.samples[7i] = _e195.dragParm.samples[7];
    c.gravityParm.calc = _e195.gravityParm.calc;
    c.gravityParm.hasCurve = _e195.gravityParm.hasCurve;
    c.gravityParm.val0_ = _e195.gravityParm.val0_;
    c.gravityParm.val1_ = _e195.gravityParm.val1_;
    c.gravityParm.variance = _e195.gravityParm.variance;
    c.gravityParm.parmPad0_ = _e195.gravityParm.parmPad0_;
    c.gravityParm.parmPad1_ = _e195.gravityParm.parmPad1_;
    c.gravityParm.parmPad2_ = _e195.gravityParm.parmPad2_;
    c.gravityParm.samples[0i] = _e195.gravityParm.samples[0];
    c.gravityParm.samples[1i] = _e195.gravityParm.samples[1];
    c.gravityParm.samples[2i] = _e195.gravityParm.samples[2];
    c.gravityParm.samples[3i] = _e195.gravityParm.samples[3];
    c.gravityParm.samples[4i] = _e195.gravityParm.samples[4];
    c.gravityParm.samples[5i] = _e195.gravityParm.samples[5];
    c.gravityParm.samples[6i] = _e195.gravityParm.samples[6];
    c.gravityParm.samples[7i] = _e195.gravityParm.samples[7];
    let _e471 = c.shaderBlendIsAdditive;
    if (_e471 != PIPELINE_BLEND_MASK) {
        emitDegenerate_u0028_();
        return;
    }
    let _e473 = vertInQuad;
    indexable = array<f32, 6>(1f, 1f, -1f, -1f, 1f, -1f);
    let _e475 = indexable[_e473];
    sx = _e475;
    let _e476 = vertInQuad;
    indexable_1 = array<f32, 6>(-1f, 1f, -1f, -1f, 1f, 1f);
    let _e478 = indexable_1[_e476];
    uy = _e478;
    let _e480 = p_3.paletteIndex;
    let _e482 = c.paletteCount;
    paletteIdx = (_e480 % bitcast<u32>(max(_e482, 1i)));
    let _e486 = paletteIdx;
    let _e489 = c.colorPalette[_e486];
    baseColor = _e489;
    let _e490 = baseColor;
    let _e492 = c.colorEndMult;
    endColor = (_e490 * _e492);
    let _e494 = baseColor;
    let _e495 = endColor;
    let _e497 = p_3.age;
    color = mix(_e494, _e495, vec4(_e497));
    let _e501 = c.alphaParm;
    param_4 = _e501;
    let _e502 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_4));
    if !(_e502) {
        let _e505 = c.alphaParm;
        param_5 = _e505;
        let _e507 = p_3.age;
        param_6 = _e507;
        let _e509 = p_3.sizeJitterPick;
        param_7 = _e509;
        let _e510 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_5), (&param_6), (&param_7));
        color[3u] = _e510;
    }
    let _e513 = p_3.pad5_;
    let _e515 = color;
    color = (_e515 * unpack4x8unorm(_e513));
    let _e518 = c.sizeStart;
    let _e520 = p_3.sizeJitterPick;
    effectiveStart = (_e518 + _e520);
    let _e522 = effectiveStart;
    let _e524 = c.sizeEnd;
    let _e526 = p_3.age;
    size = mix(_e522, _e524, _e526);
    let _e529 = c.sizeParm;
    param_8 = _e529;
    let _e530 = parmIsUnset_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b((&param_8));
    if !(_e530) {
        let _e533 = c.sizeParm;
        param_9 = _e533;
        let _e535 = p_3.age;
        param_10 = _e535;
        let _e537 = p_3.sizeJitterPick;
        param_11 = _e537;
        let _e538 = parmEval_u0028_struct_u002d_ParticleParm_u002d_i1_u002d_i1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u002d_f1_u005b_8_u005d_1_u003b_f1_u003b_f1_u003b((&param_9), (&param_10), (&param_11));
        size = _e538;
    }
    let _e540 = c.renderFlags;
    let _e542 = ((_e540 & 64u) != 0u);
    phi_711_ = _e542;
    if _e542 {
        let _e544 = p_3.vel;
        let _e546 = p_3.vel;
        phi_711_ = (dot(_e544, _e546) > 0.0001f);
    }
    let _e550 = phi_711_;
    if _e550 {
        let _e552 = p_3.age;
        let _e554 = p_3.lifetimeInv;
        ageSeconds = (_e552 / max(_e554, 0.000001f));
        let _e557 = ageSeconds;
        trailTime = min(_e557, 0.1f);
        let _e560 = p_3.pos;
        head = _e560;
        let _e561 = head;
        let _e563 = p_3.vel;
        let _e564 = trailTime;
        tail = (_e561 - (_e563 * _e564));
        let _e568 = c.gravityScale;
        let _e570 = trailTime;
        let _e572 = trailTime;
        let _e575 = tail[2u];
        tail[2u] = (_e575 - (((400f * _e568) * _e570) * _e572));
        let _e578 = head;
        let _e579 = tail;
        motion = (_e578 - _e579);
        let _e581 = motion;
        motionLength = length(_e581);
        let _e583 = head;
        let _e584 = tail;
        midpoint = ((_e583 + _e584) * 0.5f);
        let _e588 = unnamed_1.eyeWorld;
        let _e590 = midpoint;
        viewDirection = normalize((_e588.xyz - _e590));
        let _e593 = motion;
        let _e594 = motionLength;
        let _e598 = viewDirection;
        widthAxis = cross((_e593 / vec3(max(_e594, 0.000001f))), _e598);
        let _e600 = widthAxis;
        widthLength = length(_e600);
        let _e602 = motionLength;
        let _e604 = widthLength;
        if ((_e602 > 0.001f) && (_e604 > 0.001f)) {
            let _e607 = widthLength;
            let _e608 = widthAxis;
            widthAxis = (_e608 / vec3(_e607));
            let _e611 = uy;
            alongTrail = ((_e611 + 1f) * 0.5f);
            let _e614 = tail;
            let _e615 = head;
            let _e616 = alongTrail;
            let _e619 = widthAxis;
            let _e620 = sx;
            let _e621 = size;
            worldPos = (mix(_e614, _e615, vec3(_e616)) + (_e619 * (_e620 * _e621)));
        } else {
            let _e626 = p_3.pos;
            let _e628 = unnamed_1.viewLeft;
            let _e630 = sx;
            let _e631 = size;
            let _e636 = unnamed_1.viewUp;
            let _e638 = uy;
            let _e639 = size;
            worldPos = ((_e626 + (_e628.xyz * (_e630 * _e631))) + (_e636.xyz * (_e638 * _e639)));
        }
    } else {
        let _e644 = c.renderFlags;
        let _e646 = ((_e644 & 256u) != 0u);
        phi_842_ = _e646;
        if _e646 {
            let _e648 = p_3.vel;
            let _e650 = p_3.vel;
            phi_842_ = (dot(_e648, _e650) > 0.0001f);
        }
        let _e654 = phi_842_;
        if _e654 {
            let _e656 = p_3.vel;
            normal = normalize(_e656);
            let _e659 = normal[2u];
            reference = select(vec3<f32>(0f, 1f, 0f), vec3<f32>(0f, 0f, 1f), vec3((abs(_e659) < 0.92f)));
            let _e664 = reference;
            let _e665 = normal;
            tangent = normalize(cross(_e664, _e665));
            let _e668 = normal;
            let _e669 = tangent;
            bitangent = cross(_e668, _e669);
            let _e672 = p_3.pad3_;
            roll = (f32((_e672 & 1023u)) * 0.0061359233f);
            let _e676 = roll;
            cr = cos(_e676);
            let _e678 = roll;
            sr = sin(_e678);
            let _e680 = tangent;
            let _e681 = cr;
            let _e683 = bitangent;
            let _e684 = sr;
            rolledTangent = ((_e680 * _e681) + (_e683 * _e684));
            let _e687 = bitangent;
            let _e688 = cr;
            let _e690 = tangent;
            let _e691 = sr;
            rolledBitangent = ((_e687 * _e688) - (_e690 * _e691));
            let _e695 = p_3.pos;
            let _e696 = rolledTangent;
            let _e697 = sx;
            let _e698 = size;
            let _e702 = rolledBitangent;
            let _e703 = uy;
            let _e704 = size;
            worldPos = ((_e695 + (_e696 * (_e697 * _e698))) + (_e702 * (_e703 * _e704)));
        } else {
            let _e709 = p_3.pos;
            let _e711 = unnamed_1.viewLeft;
            let _e713 = sx;
            let _e714 = size;
            let _e719 = unnamed_1.viewUp;
            let _e721 = uy;
            let _e722 = size;
            worldPos = ((_e709 + (_e711.xyz * (_e713 * _e714))) + (_e719.xyz * (_e721 * _e722)));
        }
    }
    let _e727 = unnamed_1.mvp;
    let _e728 = worldPos;
    unnamed.gl_Position = (_e727 * vec4<f32>(_e728.x, _e728.y, _e728.z, 1f));
    let _e735 = vertInQuad;
    indexable_2 = array<vec2<f32>, 6>(vec2<f32>(0f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 1f), vec2<f32>(1f, 1f), vec2<f32>(0f, 0f), vec2<f32>(1f, 0f));
    let _e737 = indexable_2[_e735];
    fragUV = _e737;
    let _e738 = color;
    fragColor = _e738;
    let _e740 = p_3.classHandle;
    let _e742 = c.colorDomain;
    particleClassHandle = (_e740 | (_e742 << bitcast<u32>(31u)));
    let _e747 = c.renderFlags;
    particleRenderFlags = _e747;
    let _e749 = c.frameCount;
    if (_e749 > 1u) {
        let _e752 = p_3.age;
        let _e754 = c.frameCount;
        fr = (_e752 * f32(_e754));
        let _e758 = c.frameCount;
        last = (_e758 - 1u);
        let _e760 = fr;
        let _e762 = last;
        frame0_ = u32(clamp(floor(_e760), 0f, f32(_e762)));
        let _e766 = frame0_;
        let _e768 = last;
        frame1_ = min((_e766 + 1u), _e768);
        let _e770 = frame0_;
        let _e773 = c.frameSlots[_e770];
        frameSlot0_ = _e773;
        let _e774 = frame1_;
        let _e777 = c.frameSlots[_e774];
        frameSlot1_ = _e777;
        let _e779 = c.frameBlend;
        if (_e779 != 0u) {
            let _e781 = fr;
            local = fract(_e781);
        } else {
            local = 0f;
        }
        let _e783 = local;
        frameBlend = _e783;
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
