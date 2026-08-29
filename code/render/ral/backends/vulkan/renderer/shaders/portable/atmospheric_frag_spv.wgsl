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
var<private> fragUV_1: vec2<f32>;
var<private> fragColor_1: vec4<f32>;
var<private> outColor: vec4<f32>;

fn displayVisibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_92_: bool;
    var phi_99_: bool;

    let _e37 = (*sceneColor);
    let _e41 = unnamed.viewLeft[3u];
    exposed = (max(_e37, vec3<f32>(0f, 0f, 0f)) * _e41);
    let _e43 = exposed;
    luminance = dot(_e43, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e45 = luminance;
    let _e46 = (_e45 > 0f);
    phi_92_ = _e46;
    if _e46 {
        let _e47 = luminance;
        let _e50 = unnamed.eyeWorld[3u];
        phi_92_ = (_e47 < _e50);
    }
    let _e53 = phi_92_;
    phi_99_ = _e53;
    if _e53 {
        let _e56 = unnamed.viewUp[3u];
        phi_99_ = (_e56 != 1f);
    }
    let _e59 = phi_99_;
    if _e59 {
        let _e60 = luminance;
        let _e63 = unnamed.eyeWorld[3u];
        let _e67 = unnamed.viewUp[3u];
        let _e71 = unnamed.eyeWorld[3u];
        curved = (pow((_e60 / _e63), _e67) * _e71);
        let _e73 = curved;
        let _e74 = luminance;
        let _e76 = exposed;
        exposed = (_e76 * (_e73 / _e74));
    }
    let _e78 = exposed;
    return _e78;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e37 = (*c);
    (*c) = max(_e37, vec3<f32>(0f, 0f, 0f));
    let _e39 = (*c);
    cutoff = (_e39 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e41 = (*c);
    lo = (_e41 / vec3(12.92f));
    let _e44 = (*c);
    hi = pow(((_e44 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e49 = hi;
    let _e50 = lo;
    let _e51 = cutoff;
    return mix(_e49, _e50, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e51));
}

fn softParticleFade_u0028_() -> f32 {
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var depthDiff: f32;

    let _e38 = unnamed.renderParams[2u];
    if (_e38 < 0.5f) {
        return 1f;
    }
    let _e40 = gl_FragCoord_1;
    let _e43 = unnamed.renderParams;
    screenUV = (_e40.xy * _e43.xy);
    let _e46 = screenUV;
    let _e47 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e46);
    sceneDepth = _e47.x;
    let _e49 = sceneDepth;
    if (_e49 <= 0f) {
        return 1f;
    }
    let _e52 = gl_FragCoord_1[2u];
    let _e53 = sceneDepth;
    depthDiff = (_e52 - _e53);
    let _e55 = depthDiff;
    return smoothstep(0f, 0.001f, _e55);
}

fn main_1() {
    var edge: f32;
    var alpha: f32;
    var rgb: vec3<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e39 = fragUV_1[0u];
    edge = (1f - smoothstep(0f, 0.5f, abs((_e39 - 0.5f))));
    let _e45 = fragColor_1[3u];
    let _e46 = edge;
    let _e48 = softParticleFade_u0028_();
    alpha = ((_e45 * _e46) * _e48);
    let _e50 = fragColor_1;
    param = _e50.xyz;
    let _e52 = sRGBToLinear_u0028_vf3_u003b((&param));
    param_1 = _e52;
    let _e53 = displayVisibility_u0028_vf3_u003b((&param_1));
    rgb = _e53;
    let _e54 = rgb;
    let _e55 = alpha;
    outColor = vec4<f32>(_e54.x, _e54.y, _e54.z, _e55);
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) fragUV: vec2<f32>, @location(1) fragColor: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fragUV_1 = fragUV;
    fragColor_1 = fragColor;
    main_1();
    let _e7 = outColor;
    return _e7;
}
