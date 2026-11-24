enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_cascadeMVP: array<vec4<f32>, 1>,
    cascadeMVP: array<mat4x4<f32>, 4>,
    cascadeSplits: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 8>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
    emissionRadiance: vec4<f32>,
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: vec4<f32>,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_6_: bool = (shadow_pcf <= 1i);
override override_type_6_1: bool = (shadow_pcf <= 5i);
override override_type_6_2: bool = (lightmap_slot == 1i);
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
override override_type_6_3: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_6_4: bool = (acff == 1i);
override override_type_6_5: bool = (acff == 2i);
override override_type_6_6: bool = (acff == 3i);
override override_type_6_7: bool = (acff == 1i);
override override_type_6_8: bool = (acff == 2i);
override override_type_6_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_10: bool = (discard_mode == 1i);
override override_type_6_11: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
@group(2) @binding(0)
var shadowMap: texture_2d_array<f32>;
@group(2) @binding(32)
var shadowMap_sampler: sampler;
var<private> shadowData_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e91 = (*sceneColor)[0u];
    let _e93 = (*sceneColor)[1u];
    let _e95 = (*sceneColor)[2u];
    let _e100 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e91, max(_e93, _e95)), 0f, 1f), 0f, 0f, clamp(_e100, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e91 + 0.5f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e98 = fogType;
    let _e101 = fogType;
    return (((_e96 > 0.5f) && (_e98 >= 1i)) && (_e101 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e91 = wired_advanced_fog_enabled_u0028_();
    if !(_e91) {
        return 0f;
    }
    let _e94 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e94, 0.000001f));
    let _e99 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e99 + 0.5f));
    let _e102 = fogType_1;
    if (_e102 == 1i) {
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e106 <= 0f) {
            return 0f;
        }
        let _e108 = viewDepth;
        let _e111 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e108 / _e111), 0f, 1f);
    }
    let _e116 = unnamed.advancedFogColorDensity[3u];
    let _e118 = viewDepth;
    opticalDepth = (max(_e116, 0f) * _e118);
    let _e120 = fogType_1;
    if (_e120 == 2i) {
        let _e122 = opticalDepth;
        return clamp((1f - exp(-(_e122))), 0f, 1f);
    }
    let _e127 = opticalDepth;
    let _e128 = opticalDepth;
    return clamp((1f - exp(-((_e127 * _e128)))), 0f, 1f);
}

fn sampleCascade_u0028_i1_u003b_vf3_u003b(c: ptr<function, i32>, worldPos: ptr<function, vec3<f32>>) -> f32 {
    var sc4_: vec4<f32>;
    var sc: vec3<f32>;
    var layer: f32;
    var texelSize: vec2<f32>;
    var currentDepth: f32;
    var shadow: f32;
    var x: i32;
    var y: i32;
    var phi_355_: bool;
    var phi_362_: bool;
    var phi_369_: bool;
    var phi_376_: bool;

    let _e98 = (*c);
    let _e101 = unnamed.cascadeMVP[_e98];
    let _e102 = (*worldPos);
    sc4_ = (_e101 * vec4<f32>(_e102.x, _e102.y, _e102.z, 1f));
    let _e108 = sc4_;
    let _e111 = sc4_[3u];
    sc = (_e108.xyz / vec3(_e111));
    let _e114 = sc;
    let _e118 = ((_e114.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e118.x;
    sc[1u] = _e118.y;
    let _e124 = sc[0u];
    let _e125 = (_e124 < 0f);
    phi_355_ = _e125;
    if !(_e125) {
        let _e128 = sc[0u];
        phi_355_ = (_e128 > 1f);
    }
    let _e131 = phi_355_;
    phi_362_ = _e131;
    if !(_e131) {
        let _e134 = sc[1u];
        phi_362_ = (_e134 < 0f);
    }
    let _e137 = phi_362_;
    phi_369_ = _e137;
    if !(_e137) {
        let _e140 = sc[1u];
        phi_369_ = (_e140 > 1f);
    }
    let _e143 = phi_369_;
    phi_376_ = _e143;
    if !(_e143) {
        let _e146 = sc[2u];
        phi_376_ = (_e146 > 1f);
    }
    let _e149 = phi_376_;
    if _e149 {
        return 1f;
    }
    let _e150 = (*c);
    layer = f32(_e150);
    let _e152 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e152).xy));
    let _e159 = sc[2u];
    currentDepth = (_e159 - shadow_bias);
    shadow = 0f;
    if override_type_6_ {
        let _e161 = currentDepth;
        let _e162 = sc;
        let _e163 = _e162.xy;
        let _e164 = layer;
        let _e167 = vec3<f32>(_e163.x, _e163.y, _e164);
        let _e173 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e167.x, _e167.y), i32(_e167.z));
        shadow = step(_e161, _e173.x);
    } else {
        if override_type_6_1 {
            let _e176 = currentDepth;
            let _e177 = sc;
            let _e178 = _e177.xy;
            let _e179 = layer;
            let _e182 = vec3<f32>(_e178.x, _e178.y, _e179);
            let _e188 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e182.x, _e182.y), i32(_e182.z));
            let _e191 = shadow;
            shadow = (_e191 + step(_e176, _e188.x));
            let _e193 = currentDepth;
            let _e194 = sc;
            let _e197 = texelSize[0u];
            let _e199 = (_e194.xy + vec2<f32>(_e197, 0f));
            let _e200 = layer;
            let _e203 = vec3<f32>(_e199.x, _e199.y, _e200);
            let _e209 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e203.x, _e203.y), i32(_e203.z));
            let _e212 = shadow;
            shadow = (_e212 + step(_e193, _e209.x));
            let _e214 = currentDepth;
            let _e215 = sc;
            let _e218 = texelSize[0u];
            let _e220 = (_e215.xy - vec2<f32>(_e218, 0f));
            let _e221 = layer;
            let _e224 = vec3<f32>(_e220.x, _e220.y, _e221);
            let _e230 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e224.x, _e224.y), i32(_e224.z));
            let _e233 = shadow;
            shadow = (_e233 + step(_e214, _e230.x));
            let _e235 = currentDepth;
            let _e236 = sc;
            let _e239 = texelSize[1u];
            let _e241 = (_e236.xy + vec2<f32>(0f, _e239));
            let _e242 = layer;
            let _e245 = vec3<f32>(_e241.x, _e241.y, _e242);
            let _e251 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e245.x, _e245.y), i32(_e245.z));
            let _e254 = shadow;
            shadow = (_e254 + step(_e235, _e251.x));
            let _e256 = currentDepth;
            let _e257 = sc;
            let _e260 = texelSize[1u];
            let _e262 = (_e257.xy - vec2<f32>(0f, _e260));
            let _e263 = layer;
            let _e266 = vec3<f32>(_e262.x, _e262.y, _e263);
            let _e272 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e266.x, _e266.y), i32(_e266.z));
            let _e275 = shadow;
            shadow = (_e275 + step(_e256, _e272.x));
            let _e277 = shadow;
            shadow = (_e277 / 5f);
        } else {
            x = -1i;
            loop {
                let _e279 = x;
                if (_e279 <= 1i) {
                    y = -1i;
                    loop {
                        let _e281 = y;
                        if (_e281 <= 1i) {
                            let _e283 = currentDepth;
                            let _e284 = sc;
                            let _e286 = x;
                            let _e288 = y;
                            let _e291 = texelSize;
                            let _e293 = (_e284.xy + (vec2<f32>(f32(_e286), f32(_e288)) * _e291));
                            let _e294 = layer;
                            let _e297 = vec3<f32>(_e293.x, _e293.y, _e294);
                            let _e303 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e297.x, _e297.y), i32(_e297.z));
                            let _e306 = shadow;
                            shadow = (_e306 + step(_e283, _e303.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e308 = y;
                            y = (_e308 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e310 = x;
                    x = (_e310 + 1i);
                }
            }
            let _e312 = shadow;
            shadow = (_e312 / 9f);
        }
    }
    let _e314 = shadow;
    return _e314;
}

fn sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b(worldPos_1: ptr<function, vec3<f32>>, viewDepth_1: ptr<function, f32>, outCascade: ptr<function, i32>) -> f32 {
    var cmp: vec4<f32>;
    var cascade: i32;
    var prevSplit: f32;
    var local: f32;
    var farSplit: f32;
    var blendRange: f32;
    var blendT: f32;
    var s0_: f32;
    var param: i32;
    var param_1: vec3<f32>;
    var s1_: f32;
    var param_2: i32;
    var param_3: vec3<f32>;

    let _e105 = unnamed.cascadeSplits;
    let _e106 = (*viewDepth_1);
    cmp = step(_e105, vec4(_e106));
    let _e110 = cmp[0u];
    let _e112 = cmp[1u];
    let _e115 = cmp[2u];
    let _e118 = cmp[3u];
    cascade = min(i32((((_e110 + _e112) + _e115) + _e118)), 3i);
    let _e122 = cascade;
    (*outCascade) = _e122;
    let _e123 = cascade;
    if (_e123 == 0i) {
        local = 0f;
    } else {
        let _e125 = cascade;
        let _e130 = unnamed.cascadeSplits[max((_e125 - 1i), 0i)];
        local = _e130;
    }
    let _e131 = local;
    prevSplit = _e131;
    let _e132 = cascade;
    let _e135 = unnamed.cascadeSplits[_e132];
    farSplit = _e135;
    let _e136 = farSplit;
    let _e137 = prevSplit;
    blendRange = max((0.1f * (_e136 - _e137)), 1f);
    let _e141 = farSplit;
    let _e142 = (*viewDepth_1);
    let _e144 = blendRange;
    blendT = clamp(((_e141 - _e142) / _e144), 0f, 1f);
    let _e147 = cascade;
    param = _e147;
    let _e148 = (*worldPos_1);
    param_1 = _e148;
    let _e149 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e149;
    let _e150 = cascade;
    param_2 = min((_e150 + 1i), 3i);
    let _e153 = (*worldPos_1);
    param_3 = _e153;
    let _e154 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e154;
    let _e155 = s1_;
    let _e156 = s0_;
    let _e157 = blendT;
    return mix(_e155, _e156, _e157);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e99 = unnamed.packed_indices[1i][3u];
    let _e105 = unnamed.packed_indices[1i][3u];
    let _e110 = (*lm_uv);
    let _e111 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    sun_mask = _e111.x;
    let _e113 = sun_mask;
    if (_e113 > 0.001f) {
        let _e115 = shadowData_1;
        param_4 = _e115.xyz;
        let _e118 = shadowData_1[3u];
        param_5 = _e118;
        let _e119 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e120 = param_6;
        ignoredCascade = _e120;
        shadow_1 = _e119;
        let _e121 = shadow_1;
        let _e122 = sun_mask;
        let _e124 = (*rgb);
        (*rgb) = (_e124 * mix(1f, _e121, _e122));
    }
    let _e126 = (*rgb);
    return _e126;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_6_2 {
        let _e91 = (*rgb_1);
        param_7 = _e91;
        let _e92 = frag_tex_coord0_1;
        param_8 = _e92;
        let _e93 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e93;
    }
    let _e94 = (*rgb_1);
    return _e94;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e91 = (*rgb_2);
    let _e94 = unnamed.worldLightParams[0u];
    boosted = (_e91 * _e94);
    let _e97 = boosted[0u];
    let _e99 = boosted[1u];
    let _e101 = boosted[2u];
    peak = max(_e97, max(_e99, _e101));
    let _e104 = peak;
    if (_e104 > 1f) {
        let _e106 = peak;
        let _e107 = boosted;
        boosted = (_e107 / vec3(_e106));
    }
    let _e110 = boosted;
    return _e110;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e92 = (*c_1);
    (*c_1) = max(_e92, vec3<f32>(0f, 0f, 0f));
    let _e94 = (*c_1);
    cutoff = (_e94 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e96 = (*c_1);
    lo = (_e96 / vec3(12.92f));
    let _e99 = (*c_1);
    hi = pow(((_e99 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e104 = hi;
    let _e105 = lo;
    let _e106 = cutoff;
    return mix(_e104, _e105, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e106));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

    let _e94 = (*role);
    let _e96 = (*role);
    let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
    let _e104 = (*role);
    let _e106 = (*role);
    let _e111 = unnamed.packed_indices[(_e104 / 4u)][(_e106 % 4u)];
    let _e116 = (*uv);
    let _e117 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e111 >> bitcast<u32>(12i)) & 255u)], _e116);
    c_2 = _e117;
    let _e118 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e118))) == 0i) {
        let _e123 = c_2;
        param_9 = _e123.xyz;
        let _e125 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e125.x;
        c_2[1u] = _e125.y;
        c_2[2u] = _e125.z;
    }
    let _e132 = (*slot);
    if (lightmap_slot == (_e132 + 1i)) {
        let _e135 = c_2;
        param_10 = _e135.xyz;
        let _e137 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e137.x;
        c_2[1u] = _e137.y;
        c_2[2u] = _e137.z;
    }
    let _e144 = c_2;
    return _e144;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_14: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_15: f32;
    var param_16: vec4<f32>;

    let _e106 = unnamed.packed_indices[0i][3u];
    let _e112 = unnamed.packed_indices[0i][3u];
    let _e117 = fog_tex_coord_1;
    let _e118 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    fog = _e118;
    param_11 = 0u;
    let _e119 = frag_tex_coord0_1;
    param_12 = _e119;
    param_13 = 0i;
    let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor0_ = _e120;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e125 = sampledColor0_;
    let _e126 = frag_color;
    color0_ = (_e125 * _e126);
    let _e128 = color0_;
    base = _e128;
    let _e129 = color0_;
    base = _e129;
    let _e130 = base;
    param_14 = _e130.xyz;
    let _e132 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e132.x;
    base[1u] = _e132.y;
    base[2u] = _e132.z;
    if override_type_6_3 {
        let _e141 = unnamed.worldLightParams[1u];
        wetness = clamp(_e141, 0f, 1f);
        let _e145 = unnamed.worldLightParams[2u];
        frost = clamp(_e145, 0f, 1f);
        let _e147 = base;
        luminance = dot(_e147.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e150 = wetness;
        let _e152 = base;
        let _e154 = (_e152.xyz * mix(1f, 0.82f, _e150));
        base[0u] = _e154.x;
        base[1u] = _e154.y;
        base[2u] = _e154.z;
        let _e161 = base;
        let _e163 = luminance;
        let _e165 = luminance;
        let _e167 = luminance;
        let _e169 = frost;
        let _e172 = mix(_e161.xyz, vec3<f32>((_e163 * 0.88f), (_e165 * 0.94f), _e167), vec3((_e169 * 0.55f)));
        base[0u] = _e172.x;
        base[1u] = _e172.y;
        base[2u] = _e172.z;
    }
    let _e181 = unnamed.materialFactors[0u];
    if (_e181 >= 0f) {
        let _e184 = unnamed.materialBaseColor;
        let _e185 = base;
        base = (_e185 * _e184);
    }
    let _e187 = color0_;
    let _e190 = unnamed.emissionRadiance;
    let _e193 = base;
    let _e195 = (_e193.xyz + (_e187.xyz * _e190.xyz));
    base[0u] = _e195.x;
    base[1u] = _e195.y;
    base[2u] = _e195.z;
    let _e202 = wired_advanced_fog_enabled_u0028_();
    if _e202 {
        let _e203 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e203;
        if override_type_6_4 {
            let _e204 = fogAmount;
            let _e206 = base;
            let _e208 = (_e206.xyz * (1f - _e204));
            base[0u] = _e208.x;
            base[1u] = _e208.y;
            base[2u] = _e208.z;
        } else {
            if override_type_6_5 {
                let _e215 = fogAmount;
                let _e217 = base;
                base = (_e217 * (1f - _e215));
            } else {
                if override_type_6_6 {
                    let _e219 = fogAmount;
                    let _e222 = base[3u];
                    base[3u] = (_e222 * (1f - _e219));
                } else {
                    let _e225 = base;
                    let _e228 = unnamed.advancedFogColorDensity;
                    let _e230 = fogAmount;
                    let _e232 = mix(_e225.xyz, _e228.xyz, vec3(_e230));
                    base[0u] = _e232.x;
                    base[1u] = _e232.y;
                    base[2u] = _e232.z;
                }
            }
        }
    } else {
        if override_type_6_7 {
            let _e239 = base;
            let _e242 = fog[3u];
            let _e244 = (_e239.xyz * (1f - _e242));
            base[0u] = _e244.x;
            base[1u] = _e244.y;
            base[2u] = _e244.z;
        } else {
            if override_type_6_8 {
                let _e251 = base;
                let _e253 = fog[3u];
                base = (_e251 * (1f - _e253));
            } else {
                if override_type_6_9 {
                    let _e257 = base[3u];
                    let _e259 = fog[3u];
                    base[3u] = (_e257 * (1f - _e259));
                } else {
                    let _e263 = base;
                    let _e264 = fog;
                    let _e266 = unnamed.fogColor;
                    let _e269 = fog[3u];
                    base = mix(_e263, (_e264 * _e266), vec4(_e269));
                }
            }
        }
    }
    if override_type_6_10 {
        let _e273 = base[3u];
        if (_e273 == 0f) {
            discard;
        }
    } else {
        if override_type_6_11 {
            let _e275 = base;
            let _e277 = base;
            if (dot(_e275.xyz, _e277.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e281 = base;
    out_color = _e281;
    param_15 = 1f;
    let _e282 = base;
    param_16 = _e282;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_15), (&param_16));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
