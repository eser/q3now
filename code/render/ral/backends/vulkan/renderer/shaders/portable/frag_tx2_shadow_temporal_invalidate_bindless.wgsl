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
override override_type_4_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_4_5: bool = (tex_mode == 1i);
override override_type_4_6: bool = (tex_mode == 2i);
override override_type_4_7: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_8: bool = (discard_mode == 1i);
override override_type_4_9: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
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

    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e87 + 0.5f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e94 = fogType;
    let _e97 = fogType;
    return (((_e92 > 0.5f) && (_e94 >= 1i)) && (_e97 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e87 = wired_advanced_fog_enabled_u0028_();
    if !(_e87) {
        return 0f;
    }
    let _e90 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e90, 0.000001f));
    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e95 + 0.5f));
    let _e98 = fogType_1;
    if (_e98 == 1i) {
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e102 <= 0f) {
            return 0f;
        }
        let _e104 = viewDepth;
        let _e107 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e104 / _e107), 0f, 1f);
    }
    let _e112 = unnamed.advancedFogColorDensity[3u];
    let _e114 = viewDepth;
    opticalDepth = (max(_e112, 0f) * _e114);
    let _e116 = fogType_1;
    if (_e116 == 2i) {
        let _e118 = opticalDepth;
        return clamp((1f - exp(-(_e118))), 0f, 1f);
    }
    let _e123 = opticalDepth;
    let _e124 = opticalDepth;
    return clamp((1f - exp(-((_e123 * _e124)))), 0f, 1f);
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

    let _e94 = (*c);
    let _e97 = unnamed.cascadeMVP[_e94];
    let _e98 = (*worldPos);
    sc4_ = (_e97 * vec4<f32>(_e98.x, _e98.y, _e98.z, 1f));
    let _e104 = sc4_;
    let _e107 = sc4_[3u];
    sc = (_e104.xyz / vec3(_e107));
    let _e110 = sc;
    let _e114 = ((_e110.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e114.x;
    sc[1u] = _e114.y;
    let _e120 = sc[0u];
    let _e121 = (_e120 < 0f);
    phi_340_ = _e121;
    if !(_e121) {
        let _e124 = sc[0u];
        phi_340_ = (_e124 > 1f);
    }
    let _e127 = phi_340_;
    phi_347_ = _e127;
    if !(_e127) {
        let _e130 = sc[1u];
        phi_347_ = (_e130 < 0f);
    }
    let _e133 = phi_347_;
    phi_354_ = _e133;
    if !(_e133) {
        let _e136 = sc[1u];
        phi_354_ = (_e136 > 1f);
    }
    let _e139 = phi_354_;
    phi_361_ = _e139;
    if !(_e139) {
        let _e142 = sc[2u];
        phi_361_ = (_e142 > 1f);
    }
    let _e145 = phi_361_;
    if _e145 {
        return 1f;
    }
    let _e146 = (*c);
    layer = f32(_e146);
    let _e148 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e148).xy));
    let _e155 = sc[2u];
    currentDepth = (_e155 - shadow_bias);
    shadow = 0f;
    if override_type_4_ {
        let _e157 = currentDepth;
        let _e158 = sc;
        let _e159 = _e158.xy;
        let _e160 = layer;
        let _e163 = vec3<f32>(_e159.x, _e159.y, _e160);
        let _e169 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e163.x, _e163.y), i32(_e163.z));
        shadow = step(_e157, _e169.x);
    } else {
        if override_type_4_1 {
            let _e172 = currentDepth;
            let _e173 = sc;
            let _e174 = _e173.xy;
            let _e175 = layer;
            let _e178 = vec3<f32>(_e174.x, _e174.y, _e175);
            let _e184 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e178.x, _e178.y), i32(_e178.z));
            let _e187 = shadow;
            shadow = (_e187 + step(_e172, _e184.x));
            let _e189 = currentDepth;
            let _e190 = sc;
            let _e193 = texelSize[0u];
            let _e195 = (_e190.xy + vec2<f32>(_e193, 0f));
            let _e196 = layer;
            let _e199 = vec3<f32>(_e195.x, _e195.y, _e196);
            let _e205 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e199.x, _e199.y), i32(_e199.z));
            let _e208 = shadow;
            shadow = (_e208 + step(_e189, _e205.x));
            let _e210 = currentDepth;
            let _e211 = sc;
            let _e214 = texelSize[0u];
            let _e216 = (_e211.xy - vec2<f32>(_e214, 0f));
            let _e217 = layer;
            let _e220 = vec3<f32>(_e216.x, _e216.y, _e217);
            let _e226 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e220.x, _e220.y), i32(_e220.z));
            let _e229 = shadow;
            shadow = (_e229 + step(_e210, _e226.x));
            let _e231 = currentDepth;
            let _e232 = sc;
            let _e235 = texelSize[1u];
            let _e237 = (_e232.xy + vec2<f32>(0f, _e235));
            let _e238 = layer;
            let _e241 = vec3<f32>(_e237.x, _e237.y, _e238);
            let _e247 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e241.x, _e241.y), i32(_e241.z));
            let _e250 = shadow;
            shadow = (_e250 + step(_e231, _e247.x));
            let _e252 = currentDepth;
            let _e253 = sc;
            let _e256 = texelSize[1u];
            let _e258 = (_e253.xy - vec2<f32>(0f, _e256));
            let _e259 = layer;
            let _e262 = vec3<f32>(_e258.x, _e258.y, _e259);
            let _e268 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e262.x, _e262.y), i32(_e262.z));
            let _e271 = shadow;
            shadow = (_e271 + step(_e252, _e268.x));
            let _e273 = shadow;
            shadow = (_e273 / 5f);
        } else {
            x = -1i;
            loop {
                let _e275 = x;
                if (_e275 <= 1i) {
                    y = -1i;
                    loop {
                        let _e277 = y;
                        if (_e277 <= 1i) {
                            let _e279 = currentDepth;
                            let _e280 = sc;
                            let _e282 = x;
                            let _e284 = y;
                            let _e287 = texelSize;
                            let _e289 = (_e280.xy + (vec2<f32>(f32(_e282), f32(_e284)) * _e287));
                            let _e290 = layer;
                            let _e293 = vec3<f32>(_e289.x, _e289.y, _e290);
                            let _e299 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e293.x, _e293.y), i32(_e293.z));
                            let _e302 = shadow;
                            shadow = (_e302 + step(_e279, _e299.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e304 = y;
                            y = (_e304 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e306 = x;
                    x = (_e306 + 1i);
                }
            }
            let _e308 = shadow;
            shadow = (_e308 / 9f);
        }
    }
    let _e310 = shadow;
    return _e310;
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

    let _e101 = unnamed.cascadeSplits;
    let _e102 = (*viewDepth_1);
    cmp = step(_e101, vec4(_e102));
    let _e106 = cmp[0u];
    let _e108 = cmp[1u];
    let _e111 = cmp[2u];
    let _e114 = cmp[3u];
    cascade = min(i32((((_e106 + _e108) + _e111) + _e114)), 3i);
    let _e118 = cascade;
    (*outCascade) = _e118;
    let _e119 = cascade;
    if (_e119 == 0i) {
        local = 0f;
    } else {
        let _e121 = cascade;
        let _e126 = unnamed.cascadeSplits[max((_e121 - 1i), 0i)];
        local = _e126;
    }
    let _e127 = local;
    prevSplit = _e127;
    let _e128 = cascade;
    let _e131 = unnamed.cascadeSplits[_e128];
    farSplit = _e131;
    let _e132 = farSplit;
    let _e133 = prevSplit;
    blendRange = max((0.1f * (_e132 - _e133)), 1f);
    let _e137 = farSplit;
    let _e138 = (*viewDepth_1);
    let _e140 = blendRange;
    blendT = clamp(((_e137 - _e138) / _e140), 0f, 1f);
    let _e143 = cascade;
    param = _e143;
    let _e144 = (*worldPos_1);
    param_1 = _e144;
    let _e145 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e145;
    let _e146 = cascade;
    param_2 = min((_e146 + 1i), 3i);
    let _e149 = (*worldPos_1);
    param_3 = _e149;
    let _e150 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e150;
    let _e151 = s1_;
    let _e152 = s0_;
    let _e153 = blendT;
    return mix(_e151, _e152, _e153);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e95 = unnamed.packed_indices[1i][3u];
    let _e101 = unnamed.packed_indices[1i][3u];
    let _e106 = (*lm_uv);
    let _e107 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e101 >> bitcast<u32>(12i)) & 255u)], _e106);
    sun_mask = _e107.x;
    let _e109 = sun_mask;
    if (_e109 > 0.001f) {
        let _e111 = shadowData_1;
        param_4 = _e111.xyz;
        let _e114 = shadowData_1[3u];
        param_5 = _e114;
        let _e115 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e116 = param_6;
        ignoredCascade = _e116;
        shadow_1 = _e115;
        let _e117 = shadow_1;
        let _e118 = sun_mask;
        let _e120 = (*rgb);
        (*rgb) = (_e120 * mix(1f, _e117, _e118));
    }
    let _e122 = (*rgb);
    return _e122;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_4_2 {
        let _e91 = (*rgb_1);
        param_7 = _e91;
        let _e92 = frag_tex_coord0_1;
        param_8 = _e92;
        let _e93 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e93;
    }
    if override_type_4_3 {
        let _e94 = (*rgb_1);
        param_9 = _e94;
        let _e95 = frag_tex_coord1_1;
        param_10 = _e95;
        let _e96 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e96;
    }
    if override_type_4_4 {
        let _e97 = (*rgb_1);
        param_11 = _e97;
        let _e98 = frag_tex_coord2_1;
        param_12 = _e98;
        let _e99 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e99;
    }
    let _e100 = (*rgb_1);
    return _e100;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e87 = (*rgb_2);
    let _e90 = unnamed.worldLightParams[0u];
    boosted = (_e87 * _e90);
    let _e93 = boosted[0u];
    let _e95 = boosted[1u];
    let _e97 = boosted[2u];
    peak = max(_e93, max(_e95, _e97));
    let _e100 = peak;
    if (_e100 > 1f) {
        let _e102 = peak;
        let _e103 = boosted;
        boosted = (_e103 / vec3(_e102));
    }
    let _e106 = boosted;
    return _e106;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e88 = (*c_1);
    (*c_1) = max(_e88, vec3<f32>(0f, 0f, 0f));
    let _e90 = (*c_1);
    cutoff = (_e90 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e92 = (*c_1);
    lo = (_e92 / vec3(12.92f));
    let _e95 = (*c_1);
    hi = pow(((_e95 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e100 = hi;
    let _e101 = lo;
    let _e102 = cutoff;
    return mix(_e100, _e101, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e102));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e100 = (*role);
    let _e102 = (*role);
    let _e107 = unnamed.packed_indices[(_e100 / 4u)][(_e102 % 4u)];
    let _e112 = (*uv);
    let _e113 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    c_2 = _e113;
    let _e114 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e114))) == 0i) {
        let _e119 = c_2;
        param_13 = _e119.xyz;
        let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e121.x;
        c_2[1u] = _e121.y;
        c_2[2u] = _e121.z;
    }
    let _e128 = (*slot);
    if (lightmap_slot == (_e128 + 1i)) {
        let _e131 = c_2;
        param_14 = _e131.xyz;
        let _e133 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e133.x;
        c_2[1u] = _e133.y;
        c_2[2u] = _e133.z;
    }
    let _e140 = c_2;
    return _e140;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_15: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var sampledColor1_: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var sampledColor2_: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var param_25: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_26: f32;

    let _e112 = frag_color0In_1;
    param_15 = _e112.xyz;
    let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e116 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e114.x, _e114.y, _e114.z, _e116);
    param_16 = 0u;
    let _e121 = frag_tex_coord0_1;
    param_17 = _e121;
    param_18 = 0i;
    let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor0_ = _e122;
    param_19 = 1u;
    let _e123 = frag_tex_coord1_1;
    param_20 = _e123;
    param_21 = 1i;
    let _e124 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor1_ = _e124;
    param_22 = 2u;
    let _e125 = frag_tex_coord2_1;
    param_23 = _e125;
    param_24 = 2i;
    let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor2_ = _e126;
    let _e127 = sampledColor0_;
    let _e128 = frag_color0_;
    color0_ = (_e127 * _e128);
    if override_type_4_5 {
        let _e130 = sampledColor1_;
        color1_ = _e130;
        let _e131 = sampledColor2_;
        color2_ = _e131;
        let _e132 = color0_;
        let _e134 = color1_;
        let _e137 = color2_;
        let _e139 = ((_e132.xyz + _e134.xyz) + _e137.xyz);
        let _e141 = color0_[3u];
        let _e143 = color1_[3u];
        let _e146 = color2_[3u];
        base = vec4<f32>(_e139.x, _e139.y, _e139.z, ((_e141 * _e143) * _e146));
    } else {
        if override_type_4_6 {
            let _e152 = sampledColor1_;
            let _e153 = frag_color0_;
            color1_1 = (_e152 * _e153);
            let _e155 = sampledColor2_;
            let _e156 = frag_color0_;
            color2_1 = (_e155 * _e156);
            let _e158 = color0_;
            let _e160 = color1_1;
            let _e163 = color2_1;
            let _e165 = ((_e158.xyz + _e160.xyz) + _e163.xyz);
            let _e167 = color0_[3u];
            let _e169 = color1_1[3u];
            let _e172 = color2_1[3u];
            base = vec4<f32>(_e165.x, _e165.y, _e165.z, ((_e167 * _e169) * _e172));
        } else {
            let _e178 = sampledColor1_;
            color1_2 = _e178;
            let _e179 = sampledColor2_;
            color2_2 = _e179;
            let _e180 = color0_;
            let _e182 = color1_2;
            let _e185 = color2_2;
            let _e187 = ((_e180.xyz * _e182.xyz) * _e185.xyz);
            base[0u] = _e187.x;
            base[1u] = _e187.y;
            base[2u] = _e187.z;
            let _e195 = color0_[3u];
            let _e197 = color1_2[3u];
            let _e200 = color2_2[3u];
            base[3u] = ((_e195 * _e197) * _e200);
        }
    }
    let _e203 = base;
    param_25 = _e203.xyz;
    let _e205 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e205.x;
    base[1u] = _e205.y;
    base[2u] = _e205.z;
    if override_type_4_7 {
        let _e214 = unnamed.worldLightParams[1u];
        wetness = clamp(_e214, 0f, 1f);
        let _e218 = unnamed.worldLightParams[2u];
        frost = clamp(_e218, 0f, 1f);
        let _e220 = base;
        luminance = dot(_e220.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e223 = wetness;
        let _e225 = base;
        let _e227 = (_e225.xyz * mix(1f, 0.82f, _e223));
        base[0u] = _e227.x;
        base[1u] = _e227.y;
        base[2u] = _e227.z;
        let _e234 = base;
        let _e236 = luminance;
        let _e238 = luminance;
        let _e240 = luminance;
        let _e242 = frost;
        let _e245 = mix(_e234.xyz, vec3<f32>((_e236 * 0.88f), (_e238 * 0.94f), _e240), vec3((_e242 * 0.55f)));
        base[0u] = _e245.x;
        base[1u] = _e245.y;
        base[2u] = _e245.z;
    }
    let _e254 = unnamed.materialFactors[0u];
    if (_e254 >= 0f) {
        let _e257 = unnamed.materialBaseColor;
        let _e258 = base;
        base = (_e258 * _e257);
    }
    let _e260 = color0_;
    let _e263 = unnamed.emissionRadiance;
    let _e266 = base;
    let _e268 = (_e266.xyz + (_e260.xyz * _e263.xyz));
    base[0u] = _e268.x;
    base[1u] = _e268.y;
    base[2u] = _e268.z;
    let _e275 = wired_advanced_fog_enabled_u0028_();
    if _e275 {
        let _e276 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e276;
        let _e277 = base;
        let _e280 = unnamed.advancedFogColorDensity;
        let _e282 = fogAmount;
        let _e284 = mix(_e277.xyz, _e280.xyz, vec3(_e282));
        base[0u] = _e284.x;
        base[1u] = _e284.y;
        base[2u] = _e284.z;
    }
    if override_type_4_8 {
        let _e292 = base[3u];
        if (_e292 == 0f) {
            discard;
        }
    } else {
        if override_type_4_9 {
            let _e294 = base;
            let _e296 = base;
            if (dot(_e294.xyz, _e296.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e300 = base;
    out_color = _e300;
    param_26 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_26));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
