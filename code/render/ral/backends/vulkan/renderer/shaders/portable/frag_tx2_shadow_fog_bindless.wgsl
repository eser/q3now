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
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
override override_type_3_3: bool = (lightmap_slot == 2i);
override override_type_3_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_5: bool = (tex_mode == 1i);
override override_type_3_6: bool = (tex_mode == 2i);
@id(10) override acff: i32 = 0i;
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
override override_type_3_10: bool = (acff == 1i);
override override_type_3_11: bool = (acff == 2i);
override override_type_3_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_13: bool = (discard_mode == 1i);
override override_type_3_14: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e78 + 0.5f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e85 = fogType;
    let _e88 = fogType;
    return (((_e83 > 0.5f) && (_e85 >= 1i)) && (_e88 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e78 = wired_advanced_fog_enabled_u0028_();
    if !(_e78) {
        return 0f;
    }
    let _e81 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e81, 0.000001f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e86 + 0.5f));
    let _e89 = fogType_1;
    if (_e89 == 1i) {
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e93 <= 0f) {
            return 0f;
        }
        let _e95 = viewDepth;
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e95 / _e98), 0f, 1f);
    }
    let _e103 = unnamed.advancedFogColorDensity[3u];
    let _e105 = viewDepth;
    opticalDepth = (max(_e103, 0f) * _e105);
    let _e107 = fogType_1;
    if (_e107 == 2i) {
        let _e109 = opticalDepth;
        return clamp((1f - exp(-(_e109))), 0f, 1f);
    }
    let _e114 = opticalDepth;
    let _e115 = opticalDepth;
    return clamp((1f - exp(-((_e114 * _e115)))), 0f, 1f);
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
    var phi_304_: bool;
    var phi_311_: bool;
    var phi_318_: bool;
    var phi_325_: bool;

    let _e85 = (*c);
    let _e88 = unnamed.cascadeMVP[_e85];
    let _e89 = (*worldPos);
    sc4_ = (_e88 * vec4<f32>(_e89.x, _e89.y, _e89.z, 1f));
    let _e95 = sc4_;
    let _e98 = sc4_[3u];
    sc = (_e95.xyz / vec3(_e98));
    let _e101 = sc;
    let _e105 = ((_e101.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e105.x;
    sc[1u] = _e105.y;
    let _e111 = sc[0u];
    let _e112 = (_e111 < 0f);
    phi_304_ = _e112;
    if !(_e112) {
        let _e115 = sc[0u];
        phi_304_ = (_e115 > 1f);
    }
    let _e118 = phi_304_;
    phi_311_ = _e118;
    if !(_e118) {
        let _e121 = sc[1u];
        phi_311_ = (_e121 < 0f);
    }
    let _e124 = phi_311_;
    phi_318_ = _e124;
    if !(_e124) {
        let _e127 = sc[1u];
        phi_318_ = (_e127 > 1f);
    }
    let _e130 = phi_318_;
    phi_325_ = _e130;
    if !(_e130) {
        let _e133 = sc[2u];
        phi_325_ = (_e133 > 1f);
    }
    let _e136 = phi_325_;
    if _e136 {
        return 1f;
    }
    let _e137 = (*c);
    layer = f32(_e137);
    let _e139 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e139).xy));
    let _e146 = sc[2u];
    currentDepth = (_e146 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e148 = currentDepth;
        let _e149 = sc;
        let _e150 = _e149.xy;
        let _e151 = layer;
        let _e154 = vec3<f32>(_e150.x, _e150.y, _e151);
        let _e160 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e154.x, _e154.y), i32(_e154.z));
        shadow = step(_e148, _e160.x);
    } else {
        if override_type_3_1 {
            let _e163 = currentDepth;
            let _e164 = sc;
            let _e165 = _e164.xy;
            let _e166 = layer;
            let _e169 = vec3<f32>(_e165.x, _e165.y, _e166);
            let _e175 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e169.x, _e169.y), i32(_e169.z));
            let _e178 = shadow;
            shadow = (_e178 + step(_e163, _e175.x));
            let _e180 = currentDepth;
            let _e181 = sc;
            let _e184 = texelSize[0u];
            let _e186 = (_e181.xy + vec2<f32>(_e184, 0f));
            let _e187 = layer;
            let _e190 = vec3<f32>(_e186.x, _e186.y, _e187);
            let _e196 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e190.x, _e190.y), i32(_e190.z));
            let _e199 = shadow;
            shadow = (_e199 + step(_e180, _e196.x));
            let _e201 = currentDepth;
            let _e202 = sc;
            let _e205 = texelSize[0u];
            let _e207 = (_e202.xy - vec2<f32>(_e205, 0f));
            let _e208 = layer;
            let _e211 = vec3<f32>(_e207.x, _e207.y, _e208);
            let _e217 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e211.x, _e211.y), i32(_e211.z));
            let _e220 = shadow;
            shadow = (_e220 + step(_e201, _e217.x));
            let _e222 = currentDepth;
            let _e223 = sc;
            let _e226 = texelSize[1u];
            let _e228 = (_e223.xy + vec2<f32>(0f, _e226));
            let _e229 = layer;
            let _e232 = vec3<f32>(_e228.x, _e228.y, _e229);
            let _e238 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e232.x, _e232.y), i32(_e232.z));
            let _e241 = shadow;
            shadow = (_e241 + step(_e222, _e238.x));
            let _e243 = currentDepth;
            let _e244 = sc;
            let _e247 = texelSize[1u];
            let _e249 = (_e244.xy - vec2<f32>(0f, _e247));
            let _e250 = layer;
            let _e253 = vec3<f32>(_e249.x, _e249.y, _e250);
            let _e259 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e253.x, _e253.y), i32(_e253.z));
            let _e262 = shadow;
            shadow = (_e262 + step(_e243, _e259.x));
            let _e264 = shadow;
            shadow = (_e264 / 5f);
        } else {
            x = -1i;
            loop {
                let _e266 = x;
                if (_e266 <= 1i) {
                    y = -1i;
                    loop {
                        let _e268 = y;
                        if (_e268 <= 1i) {
                            let _e270 = currentDepth;
                            let _e271 = sc;
                            let _e273 = x;
                            let _e275 = y;
                            let _e278 = texelSize;
                            let _e280 = (_e271.xy + (vec2<f32>(f32(_e273), f32(_e275)) * _e278));
                            let _e281 = layer;
                            let _e284 = vec3<f32>(_e280.x, _e280.y, _e281);
                            let _e290 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e284.x, _e284.y), i32(_e284.z));
                            let _e293 = shadow;
                            shadow = (_e293 + step(_e270, _e290.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e295 = y;
                            y = (_e295 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e297 = x;
                    x = (_e297 + 1i);
                }
            }
            let _e299 = shadow;
            shadow = (_e299 / 9f);
        }
    }
    let _e301 = shadow;
    return _e301;
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

    let _e92 = unnamed.cascadeSplits;
    let _e93 = (*viewDepth_1);
    cmp = step(_e92, vec4(_e93));
    let _e97 = cmp[0u];
    let _e99 = cmp[1u];
    let _e102 = cmp[2u];
    let _e105 = cmp[3u];
    cascade = min(i32((((_e97 + _e99) + _e102) + _e105)), 3i);
    let _e109 = cascade;
    (*outCascade) = _e109;
    let _e110 = cascade;
    if (_e110 == 0i) {
        local = 0f;
    } else {
        let _e112 = cascade;
        let _e117 = unnamed.cascadeSplits[max((_e112 - 1i), 0i)];
        local = _e117;
    }
    let _e118 = local;
    prevSplit = _e118;
    let _e119 = cascade;
    let _e122 = unnamed.cascadeSplits[_e119];
    farSplit = _e122;
    let _e123 = farSplit;
    let _e124 = prevSplit;
    blendRange = max((0.1f * (_e123 - _e124)), 1f);
    let _e128 = farSplit;
    let _e129 = (*viewDepth_1);
    let _e131 = blendRange;
    blendT = clamp(((_e128 - _e129) / _e131), 0f, 1f);
    let _e134 = cascade;
    param = _e134;
    let _e135 = (*worldPos_1);
    param_1 = _e135;
    let _e136 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e136;
    let _e137 = cascade;
    param_2 = min((_e137 + 1i), 3i);
    let _e140 = (*worldPos_1);
    param_3 = _e140;
    let _e141 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e141;
    let _e142 = s1_;
    let _e143 = s0_;
    let _e144 = blendT;
    return mix(_e142, _e143, _e144);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e86 = unnamed.packed_indices[1i][3u];
    let _e92 = unnamed.packed_indices[1i][3u];
    let _e97 = (*lm_uv);
    let _e98 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    sun_mask = _e98.x;
    let _e100 = sun_mask;
    if (_e100 > 0.001f) {
        let _e102 = shadowData_1;
        param_4 = _e102.xyz;
        let _e105 = shadowData_1[3u];
        param_5 = _e105;
        let _e106 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e107 = param_6;
        ignoredCascade = _e107;
        shadow_1 = _e106;
        let _e108 = shadow_1;
        let _e109 = sun_mask;
        let _e111 = (*rgb);
        (*rgb) = (_e111 * mix(1f, _e108, _e109));
    }
    let _e113 = (*rgb);
    return _e113;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e82 = (*rgb_1);
        param_7 = _e82;
        let _e83 = frag_tex_coord0_1;
        param_8 = _e83;
        let _e84 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e84;
    }
    if override_type_3_3 {
        let _e85 = (*rgb_1);
        param_9 = _e85;
        let _e86 = frag_tex_coord1_1;
        param_10 = _e86;
        let _e87 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e87;
    }
    if override_type_3_4 {
        let _e88 = (*rgb_1);
        param_11 = _e88;
        let _e89 = frag_tex_coord2_1;
        param_12 = _e89;
        let _e90 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e90;
    }
    let _e91 = (*rgb_1);
    return _e91;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e79 = (*c_1);
    (*c_1) = max(_e79, vec3<f32>(0f, 0f, 0f));
    let _e81 = (*c_1);
    cutoff = (_e81 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e83 = (*c_1);
    lo = (_e83 / vec3(12.92f));
    let _e86 = (*c_1);
    hi = pow(((_e86 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e91 = hi;
    let _e92 = lo;
    let _e93 = cutoff;
    return mix(_e91, _e92, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e93));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;

    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e102 = (*uv);
    let _e103 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    c_2 = _e103;
    let _e104 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e104))) == 0i) {
        let _e109 = c_2;
        param_13 = _e109.xyz;
        let _e111 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e111.x;
        c_2[1u] = _e111.y;
        c_2[2u] = _e111.z;
    }
    let _e118 = (*slot);
    if (lightmap_slot == (_e118 + 1i)) {
        let _e123 = unnamed.worldLightParams[0u];
        let _e124 = c_2;
        let _e126 = (_e124.xyz * _e123);
        c_2[0u] = _e126.x;
        c_2[1u] = _e126.y;
        c_2[2u] = _e126.z;
    }
    let _e133 = c_2;
    return _e133;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_14: vec3<f32>;
    var color0_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color2_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color2_1: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color1_2: vec4<f32>;
    var param_30: u32;
    var param_31: vec2<f32>;
    var param_32: i32;
    var color2_2: vec4<f32>;
    var param_33: u32;
    var param_34: vec2<f32>;
    var param_35: i32;
    var param_36: vec3<f32>;
    var fogAmount: f32;

    let _e112 = unnamed.packed_indices[0i][3u];
    let _e118 = unnamed.packed_indices[0i][3u];
    let _e123 = fog_tex_coord_1;
    let _e124 = textureSample(wired_bindless_images[(_e112 & 4095u)], wired_bindless_samplers[((_e118 >> bitcast<u32>(12i)) & 255u)], _e123);
    fog = _e124;
    let _e125 = frag_color0In_1;
    param_14 = _e125.xyz;
    let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e129 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e127.x, _e127.y, _e127.z, _e129);
    param_15 = 0u;
    let _e134 = frag_tex_coord0_1;
    param_16 = _e134;
    param_17 = 0i;
    let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
    let _e136 = frag_color0_;
    color0_ = (_e135 * _e136);
    if override_type_3_5 {
        param_18 = 1u;
        let _e138 = frag_tex_coord1_1;
        param_19 = _e138;
        param_20 = 1i;
        let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
        color1_ = _e139;
        param_21 = 2u;
        let _e140 = frag_tex_coord2_1;
        param_22 = _e140;
        param_23 = 2i;
        let _e141 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
        color2_ = _e141;
        let _e142 = color0_;
        let _e144 = color1_;
        let _e147 = color2_;
        let _e149 = ((_e142.xyz + _e144.xyz) + _e147.xyz);
        let _e151 = color0_[3u];
        let _e153 = color1_[3u];
        let _e156 = color2_[3u];
        base = vec4<f32>(_e149.x, _e149.y, _e149.z, ((_e151 * _e153) * _e156));
    } else {
        if override_type_3_6 {
            param_24 = 1u;
            let _e162 = frag_tex_coord1_1;
            param_25 = _e162;
            param_26 = 1i;
            let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
            let _e164 = frag_color0_;
            color1_1 = (_e163 * _e164);
            param_27 = 2u;
            let _e166 = frag_tex_coord2_1;
            param_28 = _e166;
            param_29 = 2i;
            let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
            let _e168 = frag_color0_;
            color2_1 = (_e167 * _e168);
            let _e170 = color0_;
            let _e172 = color1_1;
            let _e175 = color2_1;
            let _e177 = ((_e170.xyz + _e172.xyz) + _e175.xyz);
            let _e179 = color0_[3u];
            let _e181 = color1_1[3u];
            let _e184 = color2_1[3u];
            base = vec4<f32>(_e177.x, _e177.y, _e177.z, ((_e179 * _e181) * _e184));
        } else {
            param_30 = 1u;
            let _e190 = frag_tex_coord1_1;
            param_31 = _e190;
            param_32 = 1i;
            let _e191 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
            color1_2 = _e191;
            param_33 = 2u;
            let _e192 = frag_tex_coord2_1;
            param_34 = _e192;
            param_35 = 2i;
            let _e193 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_33), (&param_34), (&param_35));
            color2_2 = _e193;
            let _e194 = color0_;
            let _e196 = color1_2;
            let _e199 = color2_2;
            let _e201 = ((_e194.xyz * _e196.xyz) * _e199.xyz);
            base[0u] = _e201.x;
            base[1u] = _e201.y;
            base[2u] = _e201.z;
            let _e209 = color0_[3u];
            let _e211 = color1_2[3u];
            let _e214 = color2_2[3u];
            base[3u] = ((_e209 * _e211) * _e214);
        }
    }
    let _e217 = base;
    param_36 = _e217.xyz;
    let _e219 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_36));
    base[0u] = _e219.x;
    base[1u] = _e219.y;
    base[2u] = _e219.z;
    let _e226 = wired_advanced_fog_enabled_u0028_();
    if _e226 {
        let _e227 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e227;
        if override_type_3_7 {
            let _e228 = fogAmount;
            let _e230 = base;
            let _e232 = (_e230.xyz * (1f - _e228));
            base[0u] = _e232.x;
            base[1u] = _e232.y;
            base[2u] = _e232.z;
        } else {
            if override_type_3_8 {
                let _e239 = fogAmount;
                let _e241 = base;
                base = (_e241 * (1f - _e239));
            } else {
                if override_type_3_9 {
                    let _e243 = fogAmount;
                    let _e246 = base[3u];
                    base[3u] = (_e246 * (1f - _e243));
                } else {
                    let _e249 = base;
                    let _e252 = unnamed.advancedFogColorDensity;
                    let _e254 = fogAmount;
                    let _e256 = mix(_e249.xyz, _e252.xyz, vec3(_e254));
                    base[0u] = _e256.x;
                    base[1u] = _e256.y;
                    base[2u] = _e256.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e263 = base;
            let _e266 = fog[3u];
            let _e268 = (_e263.xyz * (1f - _e266));
            base[0u] = _e268.x;
            base[1u] = _e268.y;
            base[2u] = _e268.z;
        } else {
            if override_type_3_11 {
                let _e275 = base;
                let _e277 = fog[3u];
                base = (_e275 * (1f - _e277));
            } else {
                if override_type_3_12 {
                    let _e281 = base[3u];
                    let _e283 = fog[3u];
                    base[3u] = (_e281 * (1f - _e283));
                } else {
                    let _e287 = base;
                    let _e288 = fog;
                    let _e290 = unnamed.fogColor;
                    let _e293 = fog[3u];
                    base = mix(_e287, (_e288 * _e290), vec4(_e293));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e297 = base[3u];
        if (_e297 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e299 = base;
            let _e301 = base;
            if (dot(_e299.xyz, _e301.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e305 = base;
    out_color = _e305;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e15 = out_color;
    return _e15;
}
