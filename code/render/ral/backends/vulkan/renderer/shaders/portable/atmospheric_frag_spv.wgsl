enable wgpu_binding_array;

struct AtmFrame {
    mvp: mat4x4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    eyeWorld: vec4<f32>,
    dt: f32,
    time: f32,
    poolSize: u32,
    pingPongRead: u32,
    boundsMin: vec4<f32>,
    boundsMax: vec4<f32>,
    worldMins: vec2<f32>,
    worldMaxs: vec2<f32>,
    invGridStep: vec2<f32>,
    gridSize: u32,
    atmType: u32,
    distance: f32,
    computePad0_: f32,
    computePad1_: f32,
    computePad2_: f32,
    windGust: vec4<f32>,
    precipitation: vec4<f32>,
    dustAsh: f32,
    indoorExposure: f32,
    climateSeed: u32,
    climatePad: u32,
    climate: vec4<f32>,
    surfaceClimate: vec4<f32>,
    sun: vec4<f32>,
    moon: vec4<f32>,
    ambientCloud: vec4<f32>,
    cloudMedia: vec4<f32>,
    effectMeta: vec4<f32>,
    effectWorkloads: array<vec4<f32>, 48>,
    renderParams: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> unnamed: AtmFrame;
var<private> gl_FragCoord_1: vec4<f32>;
@group(0) @binding(2)
var sceneDepthTex: texture_2d<f32>;
@group(0) @binding(34)
var sceneDepthTex_sampler: sampler;
@group(0) @binding(3)
var effectTextures: binding_array<texture_2d<f32>, 8>;
@group(0) @binding(35)
var effectTextures_sampler: binding_array<sampler, 8>;
var<private> fragUV_1: vec2<f32>;
var<private> fragColor_1: vec4<f32>;
var<private> fragFamily_1: u32;
var<private> outColor: vec4<f32>;

fn effectTexel_u0028_u1_u003b_vf2_u003b(effect: ptr<function, u32>, uv: ptr<function, vec2<f32>>) -> vec4<f32> {
    let _e45 = (*effect);
    switch bitcast<i32>(_e45) {
        case 0: {
            let _e53 = (*uv);
            let _e54 = textureSample(effectTextures[0i], effectTextures_sampler[0i], _e53);
            return _e54;
        }
        case 1: {
            let _e57 = (*uv);
            let _e58 = textureSample(effectTextures[1i], effectTextures_sampler[1i], _e57);
            return _e58;
        }
        case 2: {
            let _e61 = (*uv);
            let _e62 = textureSample(effectTextures[2i], effectTextures_sampler[2i], _e61);
            return _e62;
        }
        case 3: {
            let _e65 = (*uv);
            let _e66 = textureSample(effectTextures[3i], effectTextures_sampler[3i], _e65);
            return _e66;
        }
        case 4: {
            let _e69 = (*uv);
            let _e70 = textureSample(effectTextures[4i], effectTextures_sampler[4i], _e69);
            return _e70;
        }
        case 5: {
            let _e73 = (*uv);
            let _e74 = textureSample(effectTextures[5i], effectTextures_sampler[5i], _e73);
            return _e74;
        }
        case 6: {
            let _e77 = (*uv);
            let _e78 = textureSample(effectTextures[6i], effectTextures_sampler[6i], _e77);
            return _e78;
        }
        default: {
            let _e49 = (*uv);
            let _e50 = textureSample(effectTextures[7i], effectTextures_sampler[7i], _e49);
            return _e50;
        }
    }
}

fn displayVisibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_99_: bool;
    var phi_106_: bool;

    let _e47 = (*sceneColor);
    let _e51 = unnamed.viewLeft[3u];
    exposed = (max(_e47, vec3<f32>(0f, 0f, 0f)) * _e51);
    let _e53 = exposed;
    luminance = dot(_e53, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e55 = luminance;
    let _e56 = (_e55 > 0f);
    phi_99_ = _e56;
    if _e56 {
        let _e57 = luminance;
        let _e60 = unnamed.eyeWorld[3u];
        phi_99_ = (_e57 < _e60);
    }
    let _e63 = phi_99_;
    phi_106_ = _e63;
    if _e63 {
        let _e66 = unnamed.viewUp[3u];
        phi_106_ = (_e66 != 1f);
    }
    let _e69 = phi_106_;
    if _e69 {
        let _e70 = luminance;
        let _e73 = unnamed.eyeWorld[3u];
        let _e77 = unnamed.viewUp[3u];
        let _e81 = unnamed.eyeWorld[3u];
        curved = (pow((_e70 / _e73), _e77) * _e81);
        let _e83 = curved;
        let _e84 = luminance;
        let _e86 = exposed;
        exposed = (_e86 * (_e83 / _e84));
    }
    let _e88 = exposed;
    return _e88;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e47 = (*c);
    (*c) = max(_e47, vec3<f32>(0f, 0f, 0f));
    let _e49 = (*c);
    cutoff = (_e49 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e51 = (*c);
    lo = (_e51 / vec3(12.92f));
    let _e54 = (*c);
    hi = pow(((_e54 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e59 = hi;
    let _e60 = lo;
    let _e61 = cutoff;
    return mix(_e59, _e60, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e61));
}

fn softParticleFade_u0028_() -> f32 {
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var depthDiff: f32;

    let _e48 = unnamed.renderParams[2u];
    if (_e48 < 0.5f) {
        return 1f;
    }
    let _e50 = gl_FragCoord_1;
    let _e53 = unnamed.renderParams;
    screenUV = (_e50.xy * _e53.xy);
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

fn main_1() {
    var edge: f32;
    var alpha: f32;
    var rgb: vec3<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;
    var texel: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;

    let _e52 = fragUV_1[0u];
    edge = (1f - smoothstep(0f, 0.5f, abs((_e52 - 0.5f))));
    let _e58 = fragColor_1[3u];
    let _e59 = edge;
    let _e61 = softParticleFade_u0028_();
    alpha = ((_e58 * _e59) * _e61);
    let _e63 = fragColor_1;
    param = _e63.xyz;
    let _e65 = sRGBToLinear_u0028_vf3_u003b((&param));
    param_1 = _e65;
    let _e66 = displayVisibility_u0028_vf3_u003b((&param_1));
    rgb = _e66;
    let _e67 = fragFamily_1;
    if (_e67 >= 16u) {
        let _e69 = fragFamily_1;
        param_2 = (_e69 - 16u);
        let _e71 = fragUV_1;
        param_3 = _e71;
        let _e72 = effectTexel_u0028_u1_u003b_vf2_u003b((&param_2), (&param_3));
        texel = _e72;
        let _e73 = texel;
        let _e75 = rgb;
        rgb = (_e75 * _e73.xyz);
        let _e78 = fragColor_1[3u];
        let _e79 = softParticleFade_u0028_();
        let _e82 = texel[3u];
        alpha = ((_e78 * _e79) * _e82);
    }
    let _e84 = rgb;
    let _e85 = alpha;
    outColor = vec4<f32>(_e84.x, _e84.y, _e84.z, _e85);
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) fragUV: vec2<f32>, @location(1) fragColor: vec4<f32>, @location(2) @interpolate(flat) fragFamily: u32) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fragUV_1 = fragUV;
    fragColor_1 = fragColor;
    fragFamily_1 = fragFamily;
    main_1();
    let _e9 = outColor;
    return _e9;
}
