enable wgpu_binding_array;

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

@group(0) @binding(0)
var<uniform> unnamed: ParticleFrame;
var<private> gl_FragCoord_1: vec4<f32>;
@group(0) @binding(4)
var sceneDepthTex: texture_2d<f32>;
@group(0) @binding(36)
var sceneDepthTex_sampler: sampler;
var<private> particleClassHandle_1: u32;
var<private> frameSlot0_1: u32;
@group(0) @binding(3)
var particleSamplers: binding_array<texture_2d<f32>, 96>;
@group(0) @binding(35)
var particleSamplers_sampler: binding_array<sampler, 96>;
var<private> fragUV_1: vec2<f32>;
var<private> frameBlend_1: f32;
var<private> frameSlot1_1: u32;
var<private> particleRenderFlags_1: u32;
var<private> outColor: vec4<f32>;
var<private> fragColor_1: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e46 = (*c);
    (*c) = max(_e46, vec3<f32>(0f, 0f, 0f));
    let _e48 = (*c);
    cutoff = (_e48 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e50 = (*c);
    lo = (_e50 / vec3(12.92f));
    let _e53 = (*c);
    hi = pow(((_e53 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e58 = hi;
    let _e59 = lo;
    let _e60 = cutoff;
    return mix(_e58, _e59, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e60));
}

fn softParticleFade_u0028_() -> f32 {
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var depthDiff: f32;

    let _e46 = unnamed.depthValid;
    if (_e46 < 0.5f) {
        return 1f;
    }
    let _e48 = gl_FragCoord_1;
    let _e51 = unnamed.invResX;
    let _e53 = unnamed.invResY;
    screenUV = (_e48.xy * vec2<f32>(_e51, _e53));
    let _e56 = screenUV;
    let _e57 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e56);
    sceneDepth = _e57.x;
    let _e59 = sceneDepth;
    if (_e59 <= 0f) {
        return 1f;
    }
    let _e62 = gl_FragCoord_1[2u];
    let _e63 = sceneDepth;
    depthDiff = (_e62 - _e63);
    let _e65 = depthDiff;
    return smoothstep(0f, 0.001f, _e65);
}

fn decodeColorTexel_u0028_vf4_u003b_u1_u003b(c_1: ptr<function, vec4<f32>>, domain: ptr<function, u32>) -> vec4<f32> {
    var param: vec3<f32>;

    let _e45 = (*domain);
    if (_e45 != 0u) {
        let _e47 = (*c_1);
        return _e47;
    }
    let _e48 = (*c_1);
    param = _e48.xyz;
    let _e50 = sRGBToLinear_u0028_vf3_u003b((&param));
    (*c_1)[0u] = _e50.x;
    (*c_1)[1u] = _e50.y;
    (*c_1)[2u] = _e50.z;
    let _e57 = (*c_1);
    return _e57;
}

fn main_1() {
    var domain_1: u32;
    var idx: u32;
    var texel: vec4<f32>;
    var param_1: vec4<f32>;
    var param_2: u32;
    var t0_: vec4<f32>;
    var param_3: vec4<f32>;
    var param_4: u32;
    var t1_: vec4<f32>;
    var param_5: vec4<f32>;
    var param_6: u32;
    var surfaceAnchored: bool;
    var fade: f32;
    var local: f32;
    var rgbScale: f32;
    var local_1: f32;
    var param_7: vec3<f32>;

    let _e59 = particleClassHandle_1;
    domain_1 = (_e59 >> bitcast<u32>(31u));
    let _e62 = frameSlot0_1;
    if (_e62 == 4294967295u) {
        let _e64 = particleClassHandle_1;
        idx = ((_e64 & 2147483647u) - 1u);
        let _e67 = idx;
        let _e70 = fragUV_1;
        let _e71 = textureSample(particleSamplers[_e67], particleSamplers_sampler[_e67], _e70);
        param_1 = _e71;
        let _e72 = domain_1;
        param_2 = _e72;
        let _e73 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_1), (&param_2));
        texel = _e73;
    } else {
        let _e74 = frameSlot0_1;
        let _e77 = fragUV_1;
        let _e78 = textureSample(particleSamplers[_e74], particleSamplers_sampler[_e74], _e77);
        param_3 = _e78;
        let _e79 = domain_1;
        param_4 = _e79;
        let _e80 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_3), (&param_4));
        t0_ = _e80;
        let _e81 = frameBlend_1;
        if (_e81 > 0f) {
            let _e83 = frameSlot1_1;
            let _e86 = fragUV_1;
            let _e87 = textureSample(particleSamplers[_e83], particleSamplers_sampler[_e83], _e86);
            param_5 = _e87;
            let _e88 = domain_1;
            param_6 = _e88;
            let _e89 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_5), (&param_6));
            t1_ = _e89;
            let _e90 = t0_;
            let _e91 = t1_;
            let _e92 = frameBlend_1;
            texel = mix(_e90, _e91, vec4(_e92));
        } else {
            let _e95 = t0_;
            texel = _e95;
        }
    }
    let _e96 = particleRenderFlags_1;
    surfaceAnchored = ((_e96 & 128u) != 0u);
    let _e99 = frameSlot0_1;
    let _e101 = surfaceAnchored;
    if ((_e99 != 4294967295u) || _e101) {
        local = 1f;
    } else {
        let _e103 = softParticleFade_u0028_();
        local = _e103;
    }
    let _e104 = local;
    fade = _e104;
    let _e105 = frameSlot0_1;
    if (_e105 != 4294967295u) {
        let _e108 = unnamed.exposureBias;
        local_1 = (0.7f / max(_e108, 0.001f));
    } else {
        local_1 = 1f;
    }
    let _e111 = local_1;
    rgbScale = _e111;
    let _e112 = texel;
    let _e114 = fragColor_1;
    param_7 = _e114.xyz;
    let _e116 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e118 = rgbScale;
    let _e119 = ((_e112.xyz * _e116) * _e118);
    let _e121 = texel[3u];
    let _e123 = fragColor_1[3u];
    let _e125 = fade;
    outColor = vec4<f32>(_e119.x, _e119.y, _e119.z, ((_e121 * _e123) * _e125));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(2) @interpolate(flat) particleClassHandle: u32, @location(3) @interpolate(flat) frameSlot0_: u32, @location(0) fragUV: vec2<f32>, @location(5) frameBlend: f32, @location(4) @interpolate(flat) frameSlot1_: u32, @location(6) @interpolate(flat) particleRenderFlags: u32, @location(1) fragColor: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    particleClassHandle_1 = particleClassHandle;
    frameSlot0_1 = frameSlot0_;
    fragUV_1 = fragUV;
    frameBlend_1 = frameBlend;
    frameSlot1_1 = frameSlot1_;
    particleRenderFlags_1 = particleRenderFlags;
    fragColor_1 = fragColor;
    main_1();
    let _e17 = outColor;
    return _e17;
}
