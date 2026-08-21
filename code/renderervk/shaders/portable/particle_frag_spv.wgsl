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
var<private> outColor: vec4<f32>;
var<private> fragColor_1: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e44 = (*c);
    (*c) = max(_e44, vec3<f32>(0f, 0f, 0f));
    let _e46 = (*c);
    cutoff = (_e46 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e48 = (*c);
    lo = (_e48 / vec3(12.92f));
    let _e51 = (*c);
    hi = pow(((_e51 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e56 = hi;
    let _e57 = lo;
    let _e58 = cutoff;
    return mix(_e56, _e57, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e58));
}

fn softParticleFade_u0028_() -> f32 {
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var depthDiff: f32;

    let _e44 = unnamed.depthValid;
    if (_e44 < 0.5f) {
        return 1f;
    }
    let _e46 = gl_FragCoord_1;
    let _e49 = unnamed.invResX;
    let _e51 = unnamed.invResY;
    screenUV = (_e46.xy * vec2<f32>(_e49, _e51));
    let _e54 = screenUV;
    let _e55 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e54);
    sceneDepth = _e55.x;
    let _e57 = sceneDepth;
    if (_e57 <= 0f) {
        return 1f;
    }
    let _e60 = gl_FragCoord_1[2u];
    let _e61 = sceneDepth;
    depthDiff = (_e60 - _e61);
    let _e63 = depthDiff;
    return smoothstep(0f, 0.001f, _e63);
}

fn decodeColorTexel_u0028_vf4_u003b_u1_u003b(c_1: ptr<function, vec4<f32>>, domain: ptr<function, u32>) -> vec4<f32> {
    var param: vec3<f32>;

    let _e43 = (*domain);
    if (_e43 != 0u) {
        let _e45 = (*c_1);
        return _e45;
    }
    let _e46 = (*c_1);
    param = _e46.xyz;
    let _e48 = sRGBToLinear_u0028_vf3_u003b((&param));
    (*c_1)[0u] = _e48.x;
    (*c_1)[1u] = _e48.y;
    (*c_1)[2u] = _e48.z;
    let _e55 = (*c_1);
    return _e55;
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
    var fade: f32;
    var local: f32;
    var rgbScale: f32;
    var local_1: f32;
    var param_7: vec3<f32>;

    let _e56 = particleClassHandle_1;
    domain_1 = (_e56 >> bitcast<u32>(31u));
    let _e59 = frameSlot0_1;
    if (_e59 == 4294967295u) {
        let _e61 = particleClassHandle_1;
        idx = ((_e61 & 2147483647u) - 1u);
        let _e64 = idx;
        let _e67 = fragUV_1;
        let _e68 = textureSample(particleSamplers[_e64], particleSamplers_sampler[_e64], _e67);
        param_1 = _e68;
        let _e69 = domain_1;
        param_2 = _e69;
        let _e70 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_1), (&param_2));
        texel = _e70;
    } else {
        let _e71 = frameSlot0_1;
        let _e74 = fragUV_1;
        let _e75 = textureSample(particleSamplers[_e71], particleSamplers_sampler[_e71], _e74);
        param_3 = _e75;
        let _e76 = domain_1;
        param_4 = _e76;
        let _e77 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_3), (&param_4));
        t0_ = _e77;
        let _e78 = frameBlend_1;
        if (_e78 > 0f) {
            let _e80 = frameSlot1_1;
            let _e83 = fragUV_1;
            let _e84 = textureSample(particleSamplers[_e80], particleSamplers_sampler[_e80], _e83);
            param_5 = _e84;
            let _e85 = domain_1;
            param_6 = _e85;
            let _e86 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_5), (&param_6));
            t1_ = _e86;
            let _e87 = t0_;
            let _e88 = t1_;
            let _e89 = frameBlend_1;
            texel = mix(_e87, _e88, vec4(_e89));
        } else {
            let _e92 = t0_;
            texel = _e92;
        }
    }
    let _e93 = frameSlot0_1;
    if (_e93 != 4294967295u) {
        local = 1f;
    } else {
        let _e95 = softParticleFade_u0028_();
        local = _e95;
    }
    let _e96 = local;
    fade = _e96;
    let _e97 = frameSlot0_1;
    if (_e97 != 4294967295u) {
        let _e100 = unnamed.exposureBias;
        local_1 = (0.7f / max(_e100, 0.001f));
    } else {
        local_1 = 1f;
    }
    let _e103 = local_1;
    rgbScale = _e103;
    let _e104 = texel;
    let _e106 = fragColor_1;
    param_7 = _e106.xyz;
    let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e110 = rgbScale;
    let _e111 = ((_e104.xyz * _e108) * _e110);
    let _e113 = texel[3u];
    let _e115 = fragColor_1[3u];
    let _e117 = fade;
    outColor = vec4<f32>(_e111.x, _e111.y, _e111.z, ((_e113 * _e115) * _e117));
    return;
}

@fragment 
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(2) @interpolate(flat) particleClassHandle: u32, @location(3) @interpolate(flat) frameSlot0_: u32, @location(0) fragUV: vec2<f32>, @location(5) frameBlend: f32, @location(4) @interpolate(flat) frameSlot1_: u32, @location(1) fragColor: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    particleClassHandle_1 = particleClassHandle;
    frameSlot0_1 = frameSlot0_;
    fragUV_1 = fragUV;
    frameBlend_1 = frameBlend;
    frameSlot1_1 = frameSlot1_;
    fragColor_1 = fragColor;
    main_1();
    let _e15 = outColor;
    return _e15;
}
