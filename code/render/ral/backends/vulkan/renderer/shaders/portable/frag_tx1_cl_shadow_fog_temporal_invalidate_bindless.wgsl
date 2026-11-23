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
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_4_: bool = (shadow_pcf <= 1i);
override override_type_4_1: bool = (shadow_pcf <= 5i);
override override_type_4_2: bool = (lightmap_slot == 1i);
override override_type_4_3: bool = (lightmap_slot == 2i);
@id(6) override tex_mode: i32 = 0i;
override override_type_4_4: bool = (tex_mode == 1i);
override override_type_4_5: bool = (tex_mode == 2i);
override override_type_4_6: bool = (override_type_4_4 || override_type_4_5);
override override_type_4_7: bool = (tex_mode == 3i);
override override_type_4_8: bool = (tex_mode == 4i);
override override_type_4_9: bool = (tex_mode == 5i);
override override_type_4_10: bool = (tex_mode == 6i);
override override_type_4_11: bool = (tex_mode == 7i);
override override_type_4_12: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_13: bool = (acff == 1i);
override override_type_4_14: bool = (acff == 2i);
override override_type_4_15: bool = (acff == 3i);
override override_type_4_16: bool = (acff == 1i);
override override_type_4_17: bool = (acff == 2i);
override override_type_4_18: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_19: bool = (discard_mode == 1i);
override override_type_4_20: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e103 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e103 + 0.5f));
    let _e108 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e110 = fogType;
    let _e113 = fogType;
    return (((_e108 > 0.5f) && (_e110 >= 1i)) && (_e113 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e103 = wired_advanced_fog_enabled_u0028_();
    if !(_e103) {
        return 0f;
    }
    let _e106 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e106, 0.000001f));
    let _e111 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e111 + 0.5f));
    let _e114 = fogType_1;
    if (_e114 == 1i) {
        let _e118 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e118 <= 0f) {
            return 0f;
        }
        let _e120 = viewDepth;
        let _e123 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e120 / _e123), 0f, 1f);
    }
    let _e128 = unnamed.advancedFogColorDensity[3u];
    let _e130 = viewDepth;
    opticalDepth = (max(_e128, 0f) * _e130);
    let _e132 = fogType_1;
    if (_e132 == 2i) {
        let _e134 = opticalDepth;
        return clamp((1f - exp(-(_e134))), 0f, 1f);
    }
    let _e139 = opticalDepth;
    let _e140 = opticalDepth;
    return clamp((1f - exp(-((_e139 * _e140)))), 0f, 1f);
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
    var phi_340_: bool;
    var phi_347_: bool;
    var phi_354_: bool;
    var phi_361_: bool;

    let _e110 = (*c);
    let _e113 = unnamed.cascadeMVP[_e110];
    let _e114 = (*worldPos);
    sc4_ = (_e113 * vec4<f32>(_e114.x, _e114.y, _e114.z, 1f));
    let _e120 = sc4_;
    let _e123 = sc4_[3u];
    sc = (_e120.xyz / vec3(_e123));
    let _e126 = sc;
    let _e130 = ((_e126.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e130.x;
    sc[1u] = _e130.y;
    let _e136 = sc[0u];
    let _e137 = (_e136 < 0f);
    phi_340_ = _e137;
    if !(_e137) {
        let _e140 = sc[0u];
        phi_340_ = (_e140 > 1f);
    }
    let _e143 = phi_340_;
    phi_347_ = _e143;
    if !(_e143) {
        let _e146 = sc[1u];
        phi_347_ = (_e146 < 0f);
    }
    let _e149 = phi_347_;
    phi_354_ = _e149;
    if !(_e149) {
        let _e152 = sc[1u];
        phi_354_ = (_e152 > 1f);
    }
    let _e155 = phi_354_;
    phi_361_ = _e155;
    if !(_e155) {
        let _e158 = sc[2u];
        phi_361_ = (_e158 > 1f);
    }
    let _e161 = phi_361_;
    if _e161 {
        return 1f;
    }
    let _e162 = (*c);
    layer = f32(_e162);
    let _e164 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e164).xy));
    let _e171 = sc[2u];
    currentDepth = (_e171 - shadow_bias);
    shadow = 0f;
    if override_type_4_ {
        let _e173 = currentDepth;
        let _e174 = sc;
        let _e175 = _e174.xy;
        let _e176 = layer;
        let _e179 = vec3<f32>(_e175.x, _e175.y, _e176);
        let _e185 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e179.x, _e179.y), i32(_e179.z));
        shadow = step(_e173, _e185.x);
    } else {
        if override_type_4_1 {
            let _e188 = currentDepth;
            let _e189 = sc;
            let _e190 = _e189.xy;
            let _e191 = layer;
            let _e194 = vec3<f32>(_e190.x, _e190.y, _e191);
            let _e200 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e194.x, _e194.y), i32(_e194.z));
            let _e203 = shadow;
            shadow = (_e203 + step(_e188, _e200.x));
            let _e205 = currentDepth;
            let _e206 = sc;
            let _e209 = texelSize[0u];
            let _e211 = (_e206.xy + vec2<f32>(_e209, 0f));
            let _e212 = layer;
            let _e215 = vec3<f32>(_e211.x, _e211.y, _e212);
            let _e221 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e215.x, _e215.y), i32(_e215.z));
            let _e224 = shadow;
            shadow = (_e224 + step(_e205, _e221.x));
            let _e226 = currentDepth;
            let _e227 = sc;
            let _e230 = texelSize[0u];
            let _e232 = (_e227.xy - vec2<f32>(_e230, 0f));
            let _e233 = layer;
            let _e236 = vec3<f32>(_e232.x, _e232.y, _e233);
            let _e242 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e236.x, _e236.y), i32(_e236.z));
            let _e245 = shadow;
            shadow = (_e245 + step(_e226, _e242.x));
            let _e247 = currentDepth;
            let _e248 = sc;
            let _e251 = texelSize[1u];
            let _e253 = (_e248.xy + vec2<f32>(0f, _e251));
            let _e254 = layer;
            let _e257 = vec3<f32>(_e253.x, _e253.y, _e254);
            let _e263 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e257.x, _e257.y), i32(_e257.z));
            let _e266 = shadow;
            shadow = (_e266 + step(_e247, _e263.x));
            let _e268 = currentDepth;
            let _e269 = sc;
            let _e272 = texelSize[1u];
            let _e274 = (_e269.xy - vec2<f32>(0f, _e272));
            let _e275 = layer;
            let _e278 = vec3<f32>(_e274.x, _e274.y, _e275);
            let _e284 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e278.x, _e278.y), i32(_e278.z));
            let _e287 = shadow;
            shadow = (_e287 + step(_e268, _e284.x));
            let _e289 = shadow;
            shadow = (_e289 / 5f);
        } else {
            x = -1i;
            loop {
                let _e291 = x;
                if (_e291 <= 1i) {
                    y = -1i;
                    loop {
                        let _e293 = y;
                        if (_e293 <= 1i) {
                            let _e295 = currentDepth;
                            let _e296 = sc;
                            let _e298 = x;
                            let _e300 = y;
                            let _e303 = texelSize;
                            let _e305 = (_e296.xy + (vec2<f32>(f32(_e298), f32(_e300)) * _e303));
                            let _e306 = layer;
                            let _e309 = vec3<f32>(_e305.x, _e305.y, _e306);
                            let _e315 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e309.x, _e309.y), i32(_e309.z));
                            let _e318 = shadow;
                            shadow = (_e318 + step(_e295, _e315.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e320 = y;
                            y = (_e320 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e322 = x;
                    x = (_e322 + 1i);
                }
            }
            let _e324 = shadow;
            shadow = (_e324 / 9f);
        }
    }
    let _e326 = shadow;
    return _e326;
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

    let _e117 = unnamed.cascadeSplits;
    let _e118 = (*viewDepth_1);
    cmp = step(_e117, vec4(_e118));
    let _e122 = cmp[0u];
    let _e124 = cmp[1u];
    let _e127 = cmp[2u];
    let _e130 = cmp[3u];
    cascade = min(i32((((_e122 + _e124) + _e127) + _e130)), 3i);
    let _e134 = cascade;
    (*outCascade) = _e134;
    let _e135 = cascade;
    if (_e135 == 0i) {
        local = 0f;
    } else {
        let _e137 = cascade;
        let _e142 = unnamed.cascadeSplits[max((_e137 - 1i), 0i)];
        local = _e142;
    }
    let _e143 = local;
    prevSplit = _e143;
    let _e144 = cascade;
    let _e147 = unnamed.cascadeSplits[_e144];
    farSplit = _e147;
    let _e148 = farSplit;
    let _e149 = prevSplit;
    blendRange = max((0.1f * (_e148 - _e149)), 1f);
    let _e153 = farSplit;
    let _e154 = (*viewDepth_1);
    let _e156 = blendRange;
    blendT = clamp(((_e153 - _e154) / _e156), 0f, 1f);
    let _e159 = cascade;
    param = _e159;
    let _e160 = (*worldPos_1);
    param_1 = _e160;
    let _e161 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e161;
    let _e162 = cascade;
    param_2 = min((_e162 + 1i), 3i);
    let _e165 = (*worldPos_1);
    param_3 = _e165;
    let _e166 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e166;
    let _e167 = s1_;
    let _e168 = s0_;
    let _e169 = blendT;
    return mix(_e167, _e168, _e169);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e111 = unnamed.packed_indices[1i][3u];
    let _e117 = unnamed.packed_indices[1i][3u];
    let _e122 = (*lm_uv);
    let _e123 = textureSample(wired_bindless_images[(_e111 & 4095u)], wired_bindless_samplers[((_e117 >> bitcast<u32>(12i)) & 255u)], _e122);
    sun_mask = _e123.x;
    let _e125 = sun_mask;
    if (_e125 > 0.001f) {
        let _e127 = shadowData_1;
        param_4 = _e127.xyz;
        let _e130 = shadowData_1[3u];
        param_5 = _e130;
        let _e131 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e132 = param_6;
        ignoredCascade = _e132;
        shadow_1 = _e131;
        let _e133 = shadow_1;
        let _e134 = sun_mask;
        let _e136 = (*rgb);
        (*rgb) = (_e136 * mix(1f, _e133, _e134));
    }
    let _e138 = (*rgb);
    return _e138;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_4_2 {
        let _e105 = (*rgb_1);
        param_7 = _e105;
        let _e106 = frag_tex_coord0_1;
        param_8 = _e106;
        let _e107 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e107;
    }
    if override_type_4_3 {
        let _e108 = (*rgb_1);
        param_9 = _e108;
        let _e109 = frag_tex_coord1_1;
        param_10 = _e109;
        let _e110 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e110;
    }
    let _e111 = (*rgb_1);
    return _e111;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e103 = (*rgb_2);
    let _e106 = unnamed.worldLightParams[0u];
    boosted = (_e103 * _e106);
    let _e109 = boosted[0u];
    let _e111 = boosted[1u];
    let _e113 = boosted[2u];
    peak = max(_e109, max(_e111, _e113));
    let _e116 = peak;
    if (_e116 > 1f) {
        let _e118 = peak;
        let _e119 = boosted;
        boosted = (_e119 / vec3(_e118));
    }
    let _e122 = boosted;
    return _e122;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e104 = (*c_1);
    (*c_1) = max(_e104, vec3<f32>(0f, 0f, 0f));
    let _e106 = (*c_1);
    cutoff = (_e106 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e108 = (*c_1);
    lo = (_e108 / vec3(12.92f));
    let _e111 = (*c_1);
    hi = pow(((_e111 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e116 = hi;
    let _e117 = lo;
    let _e118 = cutoff;
    return mix(_e116, _e117, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e118));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

    let _e106 = (*role);
    let _e108 = (*role);
    let _e113 = unnamed.packed_indices[(_e106 / 4u)][(_e108 % 4u)];
    let _e116 = (*role);
    let _e118 = (*role);
    let _e123 = unnamed.packed_indices[(_e116 / 4u)][(_e118 % 4u)];
    let _e128 = (*uv);
    let _e129 = textureSample(wired_bindless_images[(_e113 & 4095u)], wired_bindless_samplers[((_e123 >> bitcast<u32>(12i)) & 255u)], _e128);
    c_2 = _e129;
    let _e130 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e130))) == 0i) {
        let _e135 = c_2;
        param_11 = _e135.xyz;
        let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e137.x;
        c_2[1u] = _e137.y;
        c_2[2u] = _e137.z;
    }
    let _e144 = (*slot);
    if (lightmap_slot == (_e144 + 1i)) {
        let _e147 = c_2;
        param_12 = _e147.xyz;
        let _e149 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e149.x;
        c_2[1u] = _e149.y;
        c_2[2u] = _e149.z;
    }
    let _e156 = c_2;
    return _e156;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_13: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_14: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var sampledColor1_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var param_21: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_22: f32;

    let _e131 = unnamed.packed_indices[0i][3u];
    let _e137 = unnamed.packed_indices[0i][3u];
    let _e142 = fog_tex_coord_1;
    let _e143 = textureSample(wired_bindless_images[(_e131 & 4095u)], wired_bindless_samplers[((_e137 >> bitcast<u32>(12i)) & 255u)], _e142);
    fog = _e143;
    let _e144 = frag_color0In_1;
    param_13 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e148 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e146.x, _e146.y, _e146.z, _e148);
    let _e153 = frag_color1In_1;
    param_14 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e157 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    param_15 = 0u;
    let _e162 = frag_tex_coord0_1;
    param_16 = _e162;
    param_17 = 0i;
    let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
    sampledColor0_ = _e163;
    param_18 = 1u;
    let _e164 = frag_tex_coord1_1;
    param_19 = _e164;
    param_20 = 1i;
    let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor1_ = _e165;
    let _e166 = sampledColor0_;
    let _e167 = frag_color0_;
    color0_ = (_e166 * _e167);
    if override_type_4_6 {
        let _e169 = sampledColor1_;
        let _e170 = frag_color1_;
        color1_ = (_e169 * _e170);
        let _e172 = color0_;
        let _e174 = color1_;
        let _e176 = (_e172.xyz + _e174.xyz);
        let _e178 = color0_[3u];
        let _e180 = color1_[3u];
        base = vec4<f32>(_e176.x, _e176.y, _e176.z, (_e178 * _e180));
    } else {
        if override_type_4_7 {
            let _e186 = sampledColor1_;
            let _e187 = frag_color1_;
            color1_1 = (_e186 * _e187);
            let _e190 = color0_[3u];
            let _e191 = color0_;
            color0_ = (_e191 * _e190);
            let _e194 = color1_1[3u];
            let _e195 = color1_1;
            color1_1 = (_e195 * _e194);
            let _e197 = color0_;
            let _e199 = color1_1;
            let _e201 = (_e197.xyz + _e199.xyz);
            let _e203 = color0_[3u];
            let _e205 = color1_1[3u];
            base = vec4<f32>(_e201.x, _e201.y, _e201.z, (_e203 * _e205));
        } else {
            if override_type_4_8 {
                let _e211 = sampledColor1_;
                let _e212 = frag_color1_;
                color1_2 = (_e211 * _e212);
                let _e215 = color0_[3u];
                let _e217 = color0_;
                color0_ = (_e217 * (1f - _e215));
                let _e220 = color1_2[3u];
                let _e222 = color1_2;
                color1_2 = (_e222 * (1f - _e220));
                let _e224 = color0_;
                let _e226 = color1_2;
                let _e228 = (_e224.xyz + _e226.xyz);
                let _e230 = color0_[3u];
                let _e232 = color1_2[3u];
                base = vec4<f32>(_e228.x, _e228.y, _e228.z, (_e230 * _e232));
            } else {
                if override_type_4_9 {
                    let _e238 = sampledColor1_;
                    let _e239 = frag_color1_;
                    color1_3 = (_e238 * _e239);
                    let _e241 = color0_;
                    let _e242 = color1_3;
                    let _e244 = color1_3[3u];
                    base = mix(_e241, _e242, vec4(_e244));
                } else {
                    if override_type_4_10 {
                        let _e247 = sampledColor1_;
                        let _e248 = frag_color1_;
                        color1_4 = (_e247 * _e248);
                        let _e250 = color1_4;
                        let _e251 = color0_;
                        let _e253 = color1_4[3u];
                        base = mix(_e250, _e251, vec4(_e253));
                    } else {
                        if override_type_4_11 {
                            let _e256 = sampledColor1_;
                            let _e257 = frag_color1_;
                            color1_5 = (_e256 * _e257);
                            let _e259 = color1_5;
                            let _e261 = color1_5[3u];
                            let _e264 = color0_;
                            base = ((_e259 + vec4(_e261)) * _e264);
                        } else {
                            let _e266 = sampledColor1_;
                            let _e267 = frag_color1_;
                            color1_6 = (_e266 * _e267);
                            let _e269 = color0_;
                            let _e271 = color1_6;
                            let _e273 = (_e269.xyz * _e271.xyz);
                            base[0u] = _e273.x;
                            base[1u] = _e273.y;
                            base[2u] = _e273.z;
                            let _e281 = color0_[3u];
                            let _e283 = color1_6[3u];
                            base[3u] = (_e281 * _e283);
                        }
                    }
                }
            }
        }
    }
    let _e286 = base;
    param_21 = _e286.xyz;
    let _e288 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_21));
    base[0u] = _e288.x;
    base[1u] = _e288.y;
    base[2u] = _e288.z;
    if override_type_4_12 {
        let _e297 = unnamed.worldLightParams[1u];
        wetness = clamp(_e297, 0f, 1f);
        let _e301 = unnamed.worldLightParams[2u];
        frost = clamp(_e301, 0f, 1f);
        let _e303 = base;
        luminance = dot(_e303.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e306 = wetness;
        let _e308 = base;
        let _e310 = (_e308.xyz * mix(1f, 0.82f, _e306));
        base[0u] = _e310.x;
        base[1u] = _e310.y;
        base[2u] = _e310.z;
        let _e317 = base;
        let _e319 = luminance;
        let _e321 = luminance;
        let _e323 = luminance;
        let _e325 = frost;
        let _e328 = mix(_e317.xyz, vec3<f32>((_e319 * 0.88f), (_e321 * 0.94f), _e323), vec3((_e325 * 0.55f)));
        base[0u] = _e328.x;
        base[1u] = _e328.y;
        base[2u] = _e328.z;
    }
    let _e337 = unnamed.materialFactors[0u];
    if (_e337 >= 0f) {
        let _e340 = unnamed.materialBaseColor;
        let _e341 = base;
        base = (_e341 * _e340);
    }
    let _e343 = color0_;
    let _e346 = unnamed.emissionRadiance;
    let _e349 = base;
    let _e351 = (_e349.xyz + (_e343.xyz * _e346.xyz));
    base[0u] = _e351.x;
    base[1u] = _e351.y;
    base[2u] = _e351.z;
    let _e358 = wired_advanced_fog_enabled_u0028_();
    if _e358 {
        let _e359 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e359;
        if override_type_4_13 {
            let _e360 = fogAmount;
            let _e362 = base;
            let _e364 = (_e362.xyz * (1f - _e360));
            base[0u] = _e364.x;
            base[1u] = _e364.y;
            base[2u] = _e364.z;
        } else {
            if override_type_4_14 {
                let _e371 = fogAmount;
                let _e373 = base;
                base = (_e373 * (1f - _e371));
            } else {
                if override_type_4_15 {
                    let _e375 = fogAmount;
                    let _e378 = base[3u];
                    base[3u] = (_e378 * (1f - _e375));
                } else {
                    let _e381 = base;
                    let _e384 = unnamed.advancedFogColorDensity;
                    let _e386 = fogAmount;
                    let _e388 = mix(_e381.xyz, _e384.xyz, vec3(_e386));
                    base[0u] = _e388.x;
                    base[1u] = _e388.y;
                    base[2u] = _e388.z;
                }
            }
        }
    } else {
        if override_type_4_16 {
            let _e395 = base;
            let _e398 = fog[3u];
            let _e400 = (_e395.xyz * (1f - _e398));
            base[0u] = _e400.x;
            base[1u] = _e400.y;
            base[2u] = _e400.z;
        } else {
            if override_type_4_17 {
                let _e407 = base;
                let _e409 = fog[3u];
                base = (_e407 * (1f - _e409));
            } else {
                if override_type_4_18 {
                    let _e413 = base[3u];
                    let _e415 = fog[3u];
                    base[3u] = (_e413 * (1f - _e415));
                } else {
                    let _e419 = base;
                    let _e420 = fog;
                    let _e422 = unnamed.fogColor;
                    let _e425 = fog[3u];
                    base = mix(_e419, (_e420 * _e422), vec4(_e425));
                }
            }
        }
    }
    if override_type_4_19 {
        let _e429 = base[3u];
        if (_e429 == 0f) {
            discard;
        }
    } else {
        if override_type_4_20 {
            let _e431 = base;
            let _e433 = base;
            if (dot(_e431.xyz, _e433.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e437 = base;
    out_color = _e437;
    param_22 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_22));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
