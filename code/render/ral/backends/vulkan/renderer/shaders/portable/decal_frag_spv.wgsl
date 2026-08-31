enable wgpu_binding_array;

struct DecalFrame {
    mvp: mat4x4<f32>,
    timeCount: vec4<f32>,
    invMvp: mat4x4<f32>,
    reconParams: vec4<f32>,
}

struct SurfaceClimateTile {
    key: vec2<i32>,
    meta_: vec2<u32>,
    climate: vec4<f32>,
}

struct SurfaceClimateTable {
    surfaceClimateTiles: array<SurfaceClimateTile, 256>,
}

@id(0) override DECAL_BLEND_MODE: u32 = 0u;
override override_type_6_: bool = (DECAL_BLEND_MODE == 2u);

@group(0) @binding(0)
var<uniform> unnamed: DecalFrame;
@group(0) @binding(4)
var<storage> unnamed_1: SurfaceClimateTable;
var<private> fragUV_1: vec2<f32>;
var<private> fragNoProject_1: u32;
var<private> gl_FragCoord_1: vec4<f32>;
@group(0) @binding(3)
var sceneDepthTex: texture_2d<f32>;
@group(0) @binding(35)
var sceneDepthTex_sampler: sampler;
var<private> fragDecalOrigin_1: vec3<f32>;
var<private> fragDecalRadius_1: f32;
var<private> fragDecalTangent_1: vec3<f32>;
var<private> fragDecalBitangent_1: vec3<f32>;
var<private> fragDecalNormal_1: vec3<f32>;
@group(0) @binding(2)
var decalTextures: binding_array<texture_2d<f32>, 64>;
@group(0) @binding(34)
var decalTextures_sampler: binding_array<sampler, 64>;
var<private> fragTextureIndex_1: u32;
var<private> fragColor_1: vec4<f32>;
var<private> outColor: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c);
    (*c) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn surfaceClimateAt_u0028_vf2_u003b(worldXY: ptr<function, vec2<f32>>) -> vec4<f32> {
    var wanted: vec2<i32>;
    var count: u32;
    var low: u32;
    var high: u32;
    var step_: u32;
    var middle: u32;
    var key: vec2<i32>;
    var less: bool;
    var phi_142_: bool;
    var phi_143_: bool;
    var phi_166_: bool;

    let _e65 = (*worldXY);
    wanted = vec2<i32>(floor((_e65 / vec2(128f))));
    let _e72 = unnamed.reconParams[3u];
    count = min(u32(_e72), 256u);
    low = 0u;
    let _e75 = count;
    high = _e75;
    step_ = 0u;
    loop {
        let _e76 = step_;
        if (_e76 < 8u) {
            let _e78 = low;
            let _e79 = high;
            if (_e78 >= _e79) {
                break;
            }
            let _e81 = low;
            let _e82 = high;
            middle = ((_e81 + _e82) >> bitcast<u32>(1u));
            let _e86 = middle;
            let _e90 = unnamed_1.surfaceClimateTiles[_e86].key;
            key = _e90;
            let _e92 = key[0u];
            let _e94 = wanted[0u];
            let _e95 = (_e92 < _e94);
            phi_143_ = _e95;
            if !(_e95) {
                let _e98 = key[0u];
                let _e100 = wanted[0u];
                let _e101 = (_e98 == _e100);
                phi_142_ = _e101;
                if _e101 {
                    let _e103 = key[1u];
                    let _e105 = wanted[1u];
                    phi_142_ = (_e103 < _e105);
                }
                let _e108 = phi_142_;
                phi_143_ = _e108;
            }
            let _e110 = phi_143_;
            less = _e110;
            let _e111 = less;
            if _e111 {
                let _e112 = middle;
                low = (_e112 + 1u);
            } else {
                let _e114 = middle;
                high = _e114;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e115 = step_;
            step_ = (_e115 + bitcast<u32>(1i));
        }
    }
    let _e118 = low;
    let _e119 = count;
    let _e120 = (_e118 < _e119);
    phi_166_ = _e120;
    if _e120 {
        let _e121 = low;
        let _e125 = unnamed_1.surfaceClimateTiles[_e121].key;
        let _e126 = wanted;
        phi_166_ = all((_e125 == _e126));
    }
    let _e130 = phi_166_;
    if _e130 {
        let _e131 = low;
        let _e135 = unnamed_1.surfaceClimateTiles[_e131].climate;
        return _e135;
    }
    return vec4<f32>(0f, 0f, 0f, 0f);
}

fn main_1() {
    var decalUV: vec2<f32>;
    var localSurfaceClimate: vec4<f32>;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var ndc: vec3<f32>;
    var worldH: vec4<f32>;
    var worldPos: vec3<f32>;
    var param: vec2<f32>;
    var local: vec3<f32>;
    var invR: f32;
    var local_1: f32;
    var u: f32;
    var v: f32;
    var w: f32;
    var halfThickness: f32;
    var texel: vec4<f32>;
    var rgb: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: vec3<f32>;
    var alpha: f32;
    var localWetness: f32;
    var localFrost: f32;
    var localSnow: f32;
    var luminance: f32;
    var accumulationVisibility: f32;
    var coverage: f32;
    var phi_302_: bool;
    var phi_310_: bool;

    let _e82 = fragUV_1;
    decalUV = _e82;
    localSurfaceClimate = vec4<f32>(0f, 0f, 0f, 0f);
    let _e83 = fragNoProject_1;
    if (_e83 == 0u) {
        let _e87 = unnamed.reconParams[2u];
        if (_e87 <= 0.5f) {
            discard;
        }
        let _e89 = gl_FragCoord_1;
        let _e92 = unnamed.reconParams;
        screenUV = (_e89.xy * _e92.xy);
        let _e95 = screenUV;
        let _e96 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e95);
        sceneDepth = _e96.x;
        let _e98 = sceneDepth;
        if (_e98 <= 0f) {
            discard;
        }
        let _e100 = screenUV;
        let _e103 = ((_e100 * 2f) - vec2(1f));
        let _e104 = sceneDepth;
        ndc = vec3<f32>(_e103.x, _e103.y, _e104);
        let _e109 = unnamed.invMvp;
        let _e110 = ndc;
        worldH = (_e109 * vec4<f32>(_e110.x, _e110.y, _e110.z, 1f));
        let _e116 = worldH;
        let _e119 = worldH[3u];
        worldPos = (_e116.xyz / vec3(_e119));
        let _e122 = worldPos;
        param = _e122.xy;
        let _e124 = surfaceClimateAt_u0028_vf2_u003b((&param));
        localSurfaceClimate = _e124;
        let _e125 = worldPos;
        let _e126 = fragDecalOrigin_1;
        local = (_e125 - _e126);
        let _e128 = fragDecalRadius_1;
        if (_e128 > 0f) {
            let _e130 = fragDecalRadius_1;
            local_1 = (1f / _e130);
        } else {
            local_1 = 0f;
        }
        let _e132 = local_1;
        invR = _e132;
        let _e133 = local;
        let _e134 = fragDecalTangent_1;
        let _e136 = invR;
        u = (dot(_e133, _e134) * _e136);
        let _e138 = local;
        let _e139 = fragDecalBitangent_1;
        let _e141 = invR;
        v = (dot(_e138, _e139) * _e141);
        let _e143 = local;
        let _e144 = fragDecalNormal_1;
        w = dot(_e143, _e144);
        let _e146 = fragDecalRadius_1;
        halfThickness = (_e146 * 0.5f);
        let _e148 = u;
        let _e150 = (abs(_e148) > 1f);
        phi_302_ = _e150;
        if !(_e150) {
            let _e152 = v;
            phi_302_ = (abs(_e152) > 1f);
        }
        let _e156 = phi_302_;
        phi_310_ = _e156;
        if !(_e156) {
            let _e158 = w;
            let _e160 = halfThickness;
            phi_310_ = (abs(_e158) > _e160);
        }
        let _e163 = phi_310_;
        if _e163 {
            discard;
        }
        let _e164 = u;
        let _e165 = v;
        decalUV = ((vec2<f32>(_e164, _e165) * 0.5f) + vec2(0.5f));
    }
    let _e170 = fragTextureIndex_1;
    let _e173 = decalUV;
    let _e174 = textureSample(decalTextures[_e170], decalTextures_sampler[_e170], _e173);
    texel = _e174;
    let _e175 = texel;
    param_1 = _e175.xyz;
    let _e177 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e178 = fragColor_1;
    param_2 = _e178.xyz;
    let _e180 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    rgb = (_e177 * _e180);
    let _e183 = texel[3u];
    let _e185 = fragColor_1[3u];
    alpha = (_e183 * _e185);
    let _e188 = localSurfaceClimate[0u];
    let _e190 = localSurfaceClimate[3u];
    localWetness = clamp((_e188 + (0.5f * _e190)), 0f, 1f);
    let _e195 = localSurfaceClimate[1u];
    localFrost = clamp(_e195, 0f, 1f);
    let _e198 = localSurfaceClimate[2u];
    localSnow = clamp(_e198, 0f, 1f);
    let _e200 = rgb;
    luminance = dot(_e200, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e202 = localWetness;
    let _e204 = rgb;
    rgb = (_e204 * mix(1f, 0.86f, _e202));
    let _e206 = rgb;
    let _e207 = luminance;
    let _e209 = luminance;
    let _e211 = luminance;
    let _e213 = localFrost;
    rgb = mix(_e206, vec3<f32>((_e207 * 0.88f), (_e209 * 0.94f), _e211), vec3((_e213 * 0.45f)));
    let _e217 = localSnow;
    accumulationVisibility = mix(1f, 0.65f, _e217);
    let _e219 = accumulationVisibility;
    let _e220 = alpha;
    alpha = (_e220 * _e219);
    if override_type_6_ {
        let _e223 = rgb[0u];
        let _e225 = rgb[1u];
        let _e227 = rgb[2u];
        coverage = max(_e223, max(_e225, _e227));
        let _e230 = coverage;
        let _e232 = fragColor_1[3u];
        let _e234 = accumulationVisibility;
        coverage = clamp(((_e230 * _e232) * _e234), 0f, 0.42f);
        let _e237 = coverage;
        outColor = vec4<f32>(0f, 0f, 0f, _e237);
    } else {
        let _e239 = rgb;
        let _e240 = alpha;
        outColor = vec4<f32>(_e239.x, _e239.y, _e239.z, _e240);
    }
    return;
}

@fragment
fn main(@location(0) fragUV: vec2<f32>, @location(8) @interpolate(flat) fragNoProject: u32, @builtin(position) gl_FragCoord: vec4<f32>, @location(3) @interpolate(flat) fragDecalOrigin: vec3<f32>, @location(7) @interpolate(flat) fragDecalRadius: f32, @location(4) @interpolate(flat) fragDecalTangent: vec3<f32>, @location(5) @interpolate(flat) fragDecalBitangent: vec3<f32>, @location(6) @interpolate(flat) fragDecalNormal: vec3<f32>, @location(2) @interpolate(flat) fragTextureIndex: u32, @location(1) fragColor: vec4<f32>) -> @location(0) vec4<f32> {
    fragUV_1 = fragUV;
    fragNoProject_1 = fragNoProject;
    gl_FragCoord_1 = gl_FragCoord;
    fragDecalOrigin_1 = fragDecalOrigin;
    fragDecalRadius_1 = fragDecalRadius;
    fragDecalTangent_1 = fragDecalTangent;
    fragDecalBitangent_1 = fragDecalBitangent;
    fragDecalNormal_1 = fragDecalNormal;
    fragTextureIndex_1 = fragTextureIndex;
    fragColor_1 = fragColor;
    main_1();
    let _e21 = outColor;
    return _e21;
}
