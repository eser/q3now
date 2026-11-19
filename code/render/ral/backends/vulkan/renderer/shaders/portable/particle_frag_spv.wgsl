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

struct ParticleAtmosphere {
    atmEyeDensity: vec4<f32>,
    atmColorVisibility: vec4<f32>,
    atmHeightCloud: vec4<f32>,
    atmFroxelGrid: vec4<u32>,
    atmControl: vec4<f32>,
}

struct ParticleMedia {
    atmMedia: array<vec4<f32>>,
}

@group(0) @binding(0)
var<uniform> unnamed: ParticleFrame;
var<private> gl_FragCoord_1: vec4<f32>;
@group(0) @binding(4)
var sceneDepthTex: texture_2d<f32>;
@group(0) @binding(36)
var sceneDepthTex_sampler: sampler;
@group(0) @binding(5)
var<uniform> unnamed_1: ParticleAtmosphere;
@group(0) @binding(6)
var<storage> unnamed_2: ParticleMedia;
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
var<private> fragColor_1: vec4<f32>;
var<private> outColor: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e63 = (*c);
    (*c) = max(_e63, vec3<f32>(0f, 0f, 0f));
    let _e65 = (*c);
    cutoff = (_e65 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e67 = (*c);
    lo = (_e67 / vec3(12.92f));
    let _e70 = (*c);
    hi = pow(((_e70 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e75 = hi;
    let _e76 = lo;
    let _e77 = cutoff;
    return mix(_e75, _e76, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e77));
}

fn particleMedia_u0028_() -> vec4<f32> {
    var farDistance: f32;
    var near: f32;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var local: f32;
    var sceneForward: f32;
    var local_1: f32;
    var local_2: f32;
    var cell: vec2<u32>;
    var slice: f32;
    var z: u32;
    var density: f32;
    var heightWeight: f32;
    var transmission: f32;
    var phi_123_: bool;
    var phi_130_: bool;

    let _e75 = unnamed_1.atmFroxelGrid[2u];
    let _e76 = (_e75 == 0u);
    phi_123_ = _e76;
    if _e76 {
        let _e79 = unnamed_1.atmEyeDensity[3u];
        phi_123_ = (_e79 <= 0f);
    }
    let _e82 = phi_123_;
    phi_130_ = _e82;
    if _e82 {
        let _e85 = unnamed_1.atmColorVisibility[3u];
        phi_130_ = (_e85 <= 0f);
    }
    let _e88 = phi_130_;
    if _e88 {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e91 = unnamed_1.atmColorVisibility[3u];
    farDistance = max(_e91, 4.01f);
    let _e95 = unnamed_1.atmControl[0u];
    near = max(_e95, 0.001f);
    let _e97 = gl_FragCoord_1;
    let _e100 = unnamed.invResX;
    let _e102 = unnamed.invResY;
    screenUV = (_e97.xy * vec2<f32>(_e100, _e102));
    let _e106 = unnamed.depthValid;
    if (abs(_e106) < 0.5f) {
        let _e110 = gl_FragCoord_1[2u];
        local = _e110;
    } else {
        let _e111 = screenUV;
        let _e112 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e111);
        local = _e112.x;
    }
    let _e114 = local;
    sceneDepth = _e114;
    let _e117 = unnamed_1.atmControl[1u];
    if (_e117 < 0f) {
        let _e119 = sceneDepth;
        if (_e119 >= 0.99999f) {
            let _e121 = farDistance;
            local_1 = (_e121 * 64f);
        } else {
            let _e123 = near;
            let _e124 = sceneDepth;
            local_1 = (_e123 / max((1f - _e124), 0.000001f));
        }
        let _e128 = local_1;
        sceneForward = _e128;
    } else {
        let _e129 = sceneDepth;
        if (_e129 <= 0.000001f) {
            let _e131 = farDistance;
            local_2 = (_e131 * 64f);
        } else {
            let _e133 = near;
            let _e134 = sceneDepth;
            local_2 = (_e133 / _e134);
        }
        let _e136 = local_2;
        sceneForward = _e136;
    }
    let _e139 = unnamed_1.atmFroxelGrid[2u];
    if (_e139 > 0u) {
        let _e141 = gl_FragCoord_1;
        let _e147 = unnamed_1.atmFroxelGrid;
        cell = min((vec2<u32>(_e141.xy) / vec2(16u)), (_e147.xy - vec2<u32>(1u, 1u)));
        let _e151 = sceneForward;
        let _e155 = farDistance;
        slice = clamp((log((max(_e151, 4f) / 4f)) / log((_e155 / 4f))), 0f, 0.9999f);
        let _e160 = slice;
        let _e163 = unnamed_1.atmFroxelGrid[2u];
        let _e169 = unnamed_1.atmFroxelGrid[2u];
        z = min(u32((_e160 * f32(_e163))), (_e169 - 1u));
        let _e174 = unnamed_1.atmFroxelGrid[3u];
        let _e175 = z;
        let _e178 = unnamed_1.atmFroxelGrid[1u];
        let _e181 = cell[1u];
        let _e185 = unnamed_1.atmFroxelGrid[0u];
        let _e189 = cell[0u];
        let _e193 = unnamed_2.atmMedia[((_e174 + (((_e175 * _e178) + _e181) * _e185)) + _e189)];
        return _e193;
    }
    let _e196 = unnamed_1.atmEyeDensity[3u];
    density = _e196;
    let _e199 = unnamed_1.atmColorVisibility[3u];
    if (_e199 > 0f) {
        let _e201 = density;
        let _e204 = unnamed_1.atmColorVisibility[3u];
        density = max(_e201, (3.912023f / _e204));
    }
    let _e207 = density;
    if (_e207 <= 0f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e211 = unnamed_1.atmHeightCloud[1u];
    let _e215 = unnamed_1.atmEyeDensity[2u];
    let _e218 = unnamed_1.atmHeightCloud[0u];
    heightWeight = exp((-(_e211) * max((_e215 - _e218), 0f)));
    let _e223 = density;
    let _e225 = heightWeight;
    let _e227 = sceneForward;
    let _e228 = farDistance;
    transmission = clamp(exp(((-(_e223) * _e225) * min(_e227, _e228))), 0f, 1f);
    let _e234 = unnamed_1.atmColorVisibility;
    let _e236 = transmission;
    let _e238 = (_e234.xyz * (1f - _e236));
    let _e239 = transmission;
    return vec4<f32>(_e238.x, _e238.y, _e238.z, _e239);
}

fn softParticleFade_u0028_() -> f32 {
    var screenUV_1: vec2<f32>;
    var sceneDepth_1: f32;
    var depthDiff: f32;
    var phi_87_: bool;

    let _e63 = unnamed.depthValid;
    if (abs(_e63) < 0.5f) {
        return 1f;
    }
    let _e66 = gl_FragCoord_1;
    let _e69 = unnamed.invResX;
    let _e71 = unnamed.invResY;
    screenUV_1 = (_e66.xy * vec2<f32>(_e69, _e71));
    let _e74 = screenUV_1;
    let _e75 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e74);
    sceneDepth_1 = _e75.x;
    let _e78 = unnamed.depthValid;
    let _e80 = sceneDepth_1;
    let _e82 = ((_e78 > 0f) && (_e80 <= 0f));
    phi_87_ = _e82;
    if !(_e82) {
        let _e85 = unnamed.depthValid;
        let _e87 = sceneDepth_1;
        phi_87_ = ((_e85 < 0f) && (_e87 >= 1f));
    }
    let _e91 = phi_87_;
    if _e91 {
        return 1f;
    }
    let _e93 = gl_FragCoord_1[2u];
    let _e94 = sceneDepth_1;
    let _e97 = unnamed.depthValid;
    depthDiff = ((_e93 - _e94) * sign(_e97));
    let _e100 = depthDiff;
    return smoothstep(0f, 0.001f, _e100);
}

fn decodeColorTexel_u0028_vf4_u003b_u1_u003b(c_1: ptr<function, vec4<f32>>, domain: ptr<function, u32>) -> vec4<f32> {
    var param: vec3<f32>;

    let _e62 = (*domain);
    if (_e62 != 0u) {
        let _e64 = (*c_1);
        return _e64;
    }
    let _e65 = (*c_1);
    param = _e65.xyz;
    let _e67 = sRGBToLinear_u0028_vf3_u003b((&param));
    (*c_1)[0u] = _e67.x;
    (*c_1)[1u] = _e67.y;
    (*c_1)[2u] = _e67.z;
    let _e74 = (*c_1);
    return _e74;
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
    var local_3: f32;
    var rgbScale: f32;
    var local_4: f32;
    var media: vec4<f32>;
    var rgb: vec3<f32>;
    var param_7: vec3<f32>;
    var local_5: vec3<f32>;

    let _e79 = particleClassHandle_1;
    domain_1 = (_e79 >> bitcast<u32>(31u));
    let _e82 = frameSlot0_1;
    if (_e82 == 4294967295u) {
        let _e84 = particleClassHandle_1;
        idx = ((_e84 & 2147483647u) - 1u);
        let _e87 = idx;
        let _e90 = fragUV_1;
        let _e91 = textureSample(particleSamplers[_e87], particleSamplers_sampler[_e87], _e90);
        param_1 = _e91;
        let _e92 = domain_1;
        param_2 = _e92;
        let _e93 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_1), (&param_2));
        texel = _e93;
    } else {
        let _e94 = frameSlot0_1;
        let _e97 = fragUV_1;
        let _e98 = textureSample(particleSamplers[_e94], particleSamplers_sampler[_e94], _e97);
        param_3 = _e98;
        let _e99 = domain_1;
        param_4 = _e99;
        let _e100 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_3), (&param_4));
        t0_ = _e100;
        let _e101 = frameBlend_1;
        if (_e101 > 0f) {
            let _e103 = frameSlot1_1;
            let _e106 = fragUV_1;
            let _e107 = textureSample(particleSamplers[_e103], particleSamplers_sampler[_e103], _e106);
            param_5 = _e107;
            let _e108 = domain_1;
            param_6 = _e108;
            let _e109 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_5), (&param_6));
            t1_ = _e109;
            let _e110 = t0_;
            let _e111 = t1_;
            let _e112 = frameBlend_1;
            texel = mix(_e110, _e111, vec4(_e112));
        } else {
            let _e115 = t0_;
            texel = _e115;
        }
    }
    let _e116 = particleRenderFlags_1;
    surfaceAnchored = ((_e116 & 128u) != 0u);
    let _e119 = frameSlot0_1;
    let _e121 = surfaceAnchored;
    if ((_e119 != 4294967295u) || _e121) {
        local_3 = 1f;
    } else {
        let _e123 = softParticleFade_u0028_();
        local_3 = _e123;
    }
    let _e124 = local_3;
    fade = _e124;
    let _e125 = frameSlot0_1;
    if (_e125 != 4294967295u) {
        let _e128 = unnamed.exposureBias;
        local_4 = (0.7f / max(_e128, 0.001f));
    } else {
        local_4 = 1f;
    }
    let _e131 = local_4;
    rgbScale = _e131;
    let _e132 = particleMedia_u0028_();
    media = _e132;
    let _e133 = texel;
    let _e135 = fragColor_1;
    param_7 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e139 = rgbScale;
    rgb = ((_e133.xyz * _e137) * _e139);
    let _e141 = particleRenderFlags_1;
    if ((_e141 & 2u) != 0u) {
        let _e144 = rgb;
        let _e146 = media[3u];
        local_5 = (_e144 * _e146);
    } else {
        let _e148 = rgb;
        let _e150 = media[3u];
        let _e152 = media;
        local_5 = ((_e148 * _e150) + _e152.xyz);
    }
    let _e155 = local_5;
    rgb = _e155;
    let _e156 = rgb;
    let _e158 = texel[3u];
    let _e160 = fragColor_1[3u];
    let _e162 = fade;
    outColor = vec4<f32>(_e156.x, _e156.y, _e156.z, ((_e158 * _e160) * _e162));
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
