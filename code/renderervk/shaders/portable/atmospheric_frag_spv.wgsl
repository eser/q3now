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
    invResX: f32,
    invResY: f32,
    depthValid: f32,
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

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e31 = (*c);
    (*c) = max(_e31, vec3<f32>(0f, 0f, 0f));
    let _e33 = (*c);
    cutoff = (_e33 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e35 = (*c);
    lo = (_e35 / vec3(12.92f));
    let _e38 = (*c);
    hi = pow(((_e38 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e43 = hi;
    let _e44 = lo;
    let _e45 = cutoff;
    return mix(_e43, _e44, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e45));
}

fn softParticleFade_u0028_() -> f32 {
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var depthDiff: f32;

    let _e31 = unnamed.depthValid;
    if (_e31 < 0.5f) {
        return 1f;
    }
    let _e33 = gl_FragCoord_1;
    let _e36 = unnamed.invResX;
    let _e38 = unnamed.invResY;
    screenUV = (_e33.xy * vec2<f32>(_e36, _e38));
    let _e41 = screenUV;
    let _e42 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e41);
    sceneDepth = _e42.x;
    let _e44 = sceneDepth;
    if (_e44 <= 0f) {
        return 1f;
    }
    let _e47 = gl_FragCoord_1[2u];
    let _e48 = sceneDepth;
    depthDiff = (_e47 - _e48);
    let _e50 = depthDiff;
    return smoothstep(0f, 0.001f, _e50);
}

fn main_1() {
    var edge: f32;
    var alpha: f32;
    var rgb: vec3<f32>;
    var param: vec3<f32>;

    let _e32 = fragUV_1[0u];
    edge = (1f - smoothstep(0f, 0.5f, abs((_e32 - 0.5f))));
    let _e38 = fragColor_1[3u];
    let _e39 = edge;
    let _e41 = softParticleFade_u0028_();
    alpha = ((_e38 * _e39) * _e41);
    let _e43 = fragColor_1;
    param = _e43.xyz;
    let _e45 = sRGBToLinear_u0028_vf3_u003b((&param));
    rgb = _e45;
    let _e46 = rgb;
    let _e47 = alpha;
    outColor = vec4<f32>(_e46.x, _e46.y, _e46.z, _e47);
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
