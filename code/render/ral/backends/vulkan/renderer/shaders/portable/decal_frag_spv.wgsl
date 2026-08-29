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

    let _e57 = (*c);
    (*c) = max(_e57, vec3<f32>(0f, 0f, 0f));
    let _e59 = (*c);
    cutoff = (_e59 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e61 = (*c);
    lo = (_e61 / vec3(12.92f));
    let _e64 = (*c);
    hi = pow(((_e64 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e69 = hi;
    let _e70 = lo;
    let _e71 = cutoff;
    return mix(_e69, _e70, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e71));
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

    let _e62 = (*worldXY);
    wanted = vec2<i32>(floor((_e62 / vec2(128f))));
    let _e69 = unnamed.reconParams[3u];
    count = min(u32(_e69), 256u);
    low = 0u;
    let _e72 = count;
    high = _e72;
    step_ = 0u;
    loop {
        let _e73 = step_;
        if (_e73 < 8u) {
            let _e75 = low;
            let _e76 = high;
            if (_e75 >= _e76) {
                break;
            }
            let _e78 = low;
            let _e79 = high;
            middle = ((_e78 + _e79) >> bitcast<u32>(1u));
            let _e83 = middle;
            let _e87 = unnamed_1.surfaceClimateTiles[_e83].key;
            key = _e87;
            let _e89 = key[0u];
            let _e91 = wanted[0u];
            let _e92 = (_e89 < _e91);
            phi_143_ = _e92;
            if !(_e92) {
                let _e95 = key[0u];
                let _e97 = wanted[0u];
                let _e98 = (_e95 == _e97);
                phi_142_ = _e98;
                if _e98 {
                    let _e100 = key[1u];
                    let _e102 = wanted[1u];
                    phi_142_ = (_e100 < _e102);
                }
                let _e105 = phi_142_;
                phi_143_ = _e105;
            }
            let _e107 = phi_143_;
            less = _e107;
            let _e108 = less;
            if _e108 {
                let _e109 = middle;
                low = (_e109 + 1u);
            } else {
                let _e111 = middle;
                high = _e111;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e112 = step_;
            step_ = (_e112 + bitcast<u32>(1i));
        }
    }
    let _e115 = low;
    let _e116 = count;
    let _e117 = (_e115 < _e116);
    phi_166_ = _e117;
    if _e117 {
        let _e118 = low;
        let _e122 = unnamed_1.surfaceClimateTiles[_e118].key;
        let _e123 = wanted;
        phi_166_ = all((_e122 == _e123));
    }
    let _e127 = phi_166_;
    if _e127 {
        let _e128 = low;
        let _e132 = unnamed_1.surfaceClimateTiles[_e128].climate;
        return _e132;
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
    var phi_195_: bool;
    var phi_302_: bool;
    var phi_310_: bool;

    let _e77 = fragUV_1;
    decalUV = _e77;
    localSurfaceClimate = vec4<f32>(0f, 0f, 0f, 0f);
    let _e80 = unnamed.reconParams[2u];
    let _e81 = (_e80 > 0.5f);
    phi_195_ = _e81;
    if _e81 {
        let _e82 = fragNoProject_1;
        phi_195_ = (_e82 == 0u);
    }
    let _e85 = phi_195_;
    if _e85 {
        let _e86 = gl_FragCoord_1;
        let _e89 = unnamed.reconParams;
        screenUV = (_e86.xy * _e89.xy);
        let _e92 = screenUV;
        let _e93 = textureSample(sceneDepthTex, sceneDepthTex_sampler, _e92);
        sceneDepth = _e93.x;
        let _e95 = sceneDepth;
        if (_e95 <= 0f) {
            discard;
        }
        let _e97 = screenUV;
        let _e100 = ((_e97 * 2f) - vec2(1f));
        let _e101 = sceneDepth;
        ndc = vec3<f32>(_e100.x, _e100.y, _e101);
        let _e106 = unnamed.invMvp;
        let _e107 = ndc;
        worldH = (_e106 * vec4<f32>(_e107.x, _e107.y, _e107.z, 1f));
        let _e113 = worldH;
        let _e116 = worldH[3u];
        worldPos = (_e113.xyz / vec3(_e116));
        let _e119 = worldPos;
        param = _e119.xy;
        let _e121 = surfaceClimateAt_u0028_vf2_u003b((&param));
        localSurfaceClimate = _e121;
        let _e122 = worldPos;
        let _e123 = fragDecalOrigin_1;
        local = (_e122 - _e123);
        let _e125 = fragDecalRadius_1;
        if (_e125 > 0f) {
            let _e127 = fragDecalRadius_1;
            local_1 = (1f / _e127);
        } else {
            local_1 = 0f;
        }
        let _e129 = local_1;
        invR = _e129;
        let _e130 = local;
        let _e131 = fragDecalTangent_1;
        let _e133 = invR;
        u = (dot(_e130, _e131) * _e133);
        let _e135 = local;
        let _e136 = fragDecalBitangent_1;
        let _e138 = invR;
        v = (dot(_e135, _e136) * _e138);
        let _e140 = local;
        let _e141 = fragDecalNormal_1;
        w = dot(_e140, _e141);
        let _e143 = fragDecalRadius_1;
        halfThickness = (_e143 * 0.5f);
        let _e145 = u;
        let _e147 = (abs(_e145) > 1f);
        phi_302_ = _e147;
        if !(_e147) {
            let _e149 = v;
            phi_302_ = (abs(_e149) > 1f);
        }
        let _e153 = phi_302_;
        phi_310_ = _e153;
        if !(_e153) {
            let _e155 = w;
            let _e157 = halfThickness;
            phi_310_ = (abs(_e155) > _e157);
        }
        let _e160 = phi_310_;
        if _e160 {
            discard;
        }
        let _e161 = u;
        let _e162 = v;
        decalUV = ((vec2<f32>(_e161, _e162) * 0.5f) + vec2(0.5f));
    }
    let _e167 = fragTextureIndex_1;
    let _e170 = decalUV;
    let _e171 = textureSample(decalTextures[_e167], decalTextures_sampler[_e167], _e170);
    texel = _e171;
    let _e172 = texel;
    param_1 = _e172.xyz;
    let _e174 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e175 = fragColor_1;
    param_2 = _e175.xyz;
    let _e177 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    rgb = (_e174 * _e177);
    let _e180 = texel[3u];
    let _e182 = fragColor_1[3u];
    alpha = (_e180 * _e182);
    let _e185 = localSurfaceClimate[0u];
    let _e187 = localSurfaceClimate[3u];
    localWetness = clamp((_e185 + (0.5f * _e187)), 0f, 1f);
    let _e192 = localSurfaceClimate[1u];
    localFrost = clamp(_e192, 0f, 1f);
    let _e195 = localSurfaceClimate[2u];
    localSnow = clamp(_e195, 0f, 1f);
    let _e197 = rgb;
    luminance = dot(_e197, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e199 = localWetness;
    let _e201 = rgb;
    rgb = (_e201 * mix(1f, 0.86f, _e199));
    let _e203 = rgb;
    let _e204 = luminance;
    let _e206 = luminance;
    let _e208 = luminance;
    let _e210 = localFrost;
    rgb = mix(_e203, vec3<f32>((_e204 * 0.88f), (_e206 * 0.94f), _e208), vec3((_e210 * 0.45f)));
    let _e214 = localSnow;
    let _e216 = alpha;
    alpha = (_e216 * mix(1f, 0.65f, _e214));
    let _e218 = rgb;
    let _e219 = alpha;
    outColor = vec4<f32>(_e218.x, _e218.y, _e218.z, _e219);
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
