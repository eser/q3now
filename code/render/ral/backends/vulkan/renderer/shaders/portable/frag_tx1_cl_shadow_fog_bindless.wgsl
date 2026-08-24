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
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
override override_type_3_6: bool = (override_type_3_4 || override_type_3_5);
override override_type_3_7: bool = (tex_mode == 3i);
override override_type_3_8: bool = (tex_mode == 4i);
override override_type_3_9: bool = (tex_mode == 5i);
override override_type_3_10: bool = (tex_mode == 6i);
override override_type_3_11: bool = (tex_mode == 7i);
@id(10) override acff: i32 = 0i;
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
override override_type_3_15: bool = (acff == 1i);
override override_type_3_16: bool = (acff == 2i);
override override_type_3_17: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_18: bool = (discard_mode == 1i);
override override_type_3_19: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e85 + 0.5f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e92 = fogType;
    let _e95 = fogType;
    return (((_e90 > 0.5f) && (_e92 >= 1i)) && (_e95 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e85 = wired_advanced_fog_enabled_u0028_();
    if !(_e85) {
        return 0f;
    }
    let _e88 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e88, 0.000001f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e93 + 0.5f));
    let _e96 = fogType_1;
    if (_e96 == 1i) {
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e100 <= 0f) {
            return 0f;
        }
        let _e102 = viewDepth;
        let _e105 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e102 / _e105), 0f, 1f);
    }
    let _e110 = unnamed.advancedFogColorDensity[3u];
    let _e112 = viewDepth;
    opticalDepth = (max(_e110, 0f) * _e112);
    let _e114 = fogType_1;
    if (_e114 == 2i) {
        let _e116 = opticalDepth;
        return clamp((1f - exp(-(_e116))), 0f, 1f);
    }
    let _e121 = opticalDepth;
    let _e122 = opticalDepth;
    return clamp((1f - exp(-((_e121 * _e122)))), 0f, 1f);
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

    let _e92 = (*c);
    let _e95 = unnamed.cascadeMVP[_e92];
    let _e96 = (*worldPos);
    sc4_ = (_e95 * vec4<f32>(_e96.x, _e96.y, _e96.z, 1f));
    let _e102 = sc4_;
    let _e105 = sc4_[3u];
    sc = (_e102.xyz / vec3(_e105));
    let _e108 = sc;
    let _e112 = ((_e108.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e112.x;
    sc[1u] = _e112.y;
    let _e118 = sc[0u];
    let _e119 = (_e118 < 0f);
    phi_304_ = _e119;
    if !(_e119) {
        let _e122 = sc[0u];
        phi_304_ = (_e122 > 1f);
    }
    let _e125 = phi_304_;
    phi_311_ = _e125;
    if !(_e125) {
        let _e128 = sc[1u];
        phi_311_ = (_e128 < 0f);
    }
    let _e131 = phi_311_;
    phi_318_ = _e131;
    if !(_e131) {
        let _e134 = sc[1u];
        phi_318_ = (_e134 > 1f);
    }
    let _e137 = phi_318_;
    phi_325_ = _e137;
    if !(_e137) {
        let _e140 = sc[2u];
        phi_325_ = (_e140 > 1f);
    }
    let _e143 = phi_325_;
    if _e143 {
        return 1f;
    }
    let _e144 = (*c);
    layer = f32(_e144);
    let _e146 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e146).xy));
    let _e153 = sc[2u];
    currentDepth = (_e153 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e155 = currentDepth;
        let _e156 = sc;
        let _e157 = _e156.xy;
        let _e158 = layer;
        let _e161 = vec3<f32>(_e157.x, _e157.y, _e158);
        let _e167 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e161.x, _e161.y), i32(_e161.z));
        shadow = step(_e155, _e167.x);
    } else {
        if override_type_3_1 {
            let _e170 = currentDepth;
            let _e171 = sc;
            let _e172 = _e171.xy;
            let _e173 = layer;
            let _e176 = vec3<f32>(_e172.x, _e172.y, _e173);
            let _e182 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e176.x, _e176.y), i32(_e176.z));
            let _e185 = shadow;
            shadow = (_e185 + step(_e170, _e182.x));
            let _e187 = currentDepth;
            let _e188 = sc;
            let _e191 = texelSize[0u];
            let _e193 = (_e188.xy + vec2<f32>(_e191, 0f));
            let _e194 = layer;
            let _e197 = vec3<f32>(_e193.x, _e193.y, _e194);
            let _e203 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e197.x, _e197.y), i32(_e197.z));
            let _e206 = shadow;
            shadow = (_e206 + step(_e187, _e203.x));
            let _e208 = currentDepth;
            let _e209 = sc;
            let _e212 = texelSize[0u];
            let _e214 = (_e209.xy - vec2<f32>(_e212, 0f));
            let _e215 = layer;
            let _e218 = vec3<f32>(_e214.x, _e214.y, _e215);
            let _e224 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e218.x, _e218.y), i32(_e218.z));
            let _e227 = shadow;
            shadow = (_e227 + step(_e208, _e224.x));
            let _e229 = currentDepth;
            let _e230 = sc;
            let _e233 = texelSize[1u];
            let _e235 = (_e230.xy + vec2<f32>(0f, _e233));
            let _e236 = layer;
            let _e239 = vec3<f32>(_e235.x, _e235.y, _e236);
            let _e245 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e239.x, _e239.y), i32(_e239.z));
            let _e248 = shadow;
            shadow = (_e248 + step(_e229, _e245.x));
            let _e250 = currentDepth;
            let _e251 = sc;
            let _e254 = texelSize[1u];
            let _e256 = (_e251.xy - vec2<f32>(0f, _e254));
            let _e257 = layer;
            let _e260 = vec3<f32>(_e256.x, _e256.y, _e257);
            let _e266 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e260.x, _e260.y), i32(_e260.z));
            let _e269 = shadow;
            shadow = (_e269 + step(_e250, _e266.x));
            let _e271 = shadow;
            shadow = (_e271 / 5f);
        } else {
            x = -1i;
            loop {
                let _e273 = x;
                if (_e273 <= 1i) {
                    y = -1i;
                    loop {
                        let _e275 = y;
                        if (_e275 <= 1i) {
                            let _e277 = currentDepth;
                            let _e278 = sc;
                            let _e280 = x;
                            let _e282 = y;
                            let _e285 = texelSize;
                            let _e287 = (_e278.xy + (vec2<f32>(f32(_e280), f32(_e282)) * _e285));
                            let _e288 = layer;
                            let _e291 = vec3<f32>(_e287.x, _e287.y, _e288);
                            let _e297 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e291.x, _e291.y), i32(_e291.z));
                            let _e300 = shadow;
                            shadow = (_e300 + step(_e277, _e297.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e302 = y;
                            y = (_e302 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e304 = x;
                    x = (_e304 + 1i);
                }
            }
            let _e306 = shadow;
            shadow = (_e306 / 9f);
        }
    }
    let _e308 = shadow;
    return _e308;
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

    let _e99 = unnamed.cascadeSplits;
    let _e100 = (*viewDepth_1);
    cmp = step(_e99, vec4(_e100));
    let _e104 = cmp[0u];
    let _e106 = cmp[1u];
    let _e109 = cmp[2u];
    let _e112 = cmp[3u];
    cascade = min(i32((((_e104 + _e106) + _e109) + _e112)), 3i);
    let _e116 = cascade;
    (*outCascade) = _e116;
    let _e117 = cascade;
    if (_e117 == 0i) {
        local = 0f;
    } else {
        let _e119 = cascade;
        let _e124 = unnamed.cascadeSplits[max((_e119 - 1i), 0i)];
        local = _e124;
    }
    let _e125 = local;
    prevSplit = _e125;
    let _e126 = cascade;
    let _e129 = unnamed.cascadeSplits[_e126];
    farSplit = _e129;
    let _e130 = farSplit;
    let _e131 = prevSplit;
    blendRange = max((0.1f * (_e130 - _e131)), 1f);
    let _e135 = farSplit;
    let _e136 = (*viewDepth_1);
    let _e138 = blendRange;
    blendT = clamp(((_e135 - _e136) / _e138), 0f, 1f);
    let _e141 = cascade;
    param = _e141;
    let _e142 = (*worldPos_1);
    param_1 = _e142;
    let _e143 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e143;
    let _e144 = cascade;
    param_2 = min((_e144 + 1i), 3i);
    let _e147 = (*worldPos_1);
    param_3 = _e147;
    let _e148 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e148;
    let _e149 = s1_;
    let _e150 = s0_;
    let _e151 = blendT;
    return mix(_e149, _e150, _e151);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e93 = unnamed.packed_indices[1i][3u];
    let _e99 = unnamed.packed_indices[1i][3u];
    let _e104 = (*lm_uv);
    let _e105 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    sun_mask = _e105.x;
    let _e107 = sun_mask;
    if (_e107 > 0.001f) {
        let _e109 = shadowData_1;
        param_4 = _e109.xyz;
        let _e112 = shadowData_1[3u];
        param_5 = _e112;
        let _e113 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e114 = param_6;
        ignoredCascade = _e114;
        shadow_1 = _e113;
        let _e115 = shadow_1;
        let _e116 = sun_mask;
        let _e118 = (*rgb);
        (*rgb) = (_e118 * mix(1f, _e115, _e116));
    }
    let _e120 = (*rgb);
    return _e120;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e87 = (*rgb_1);
        param_7 = _e87;
        let _e88 = frag_tex_coord0_1;
        param_8 = _e88;
        let _e89 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e89;
    }
    if override_type_3_3 {
        let _e90 = (*rgb_1);
        param_9 = _e90;
        let _e91 = frag_tex_coord1_1;
        param_10 = _e91;
        let _e92 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e92;
    }
    let _e93 = (*rgb_1);
    return _e93;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e86 = (*c_1);
    (*c_1) = max(_e86, vec3<f32>(0f, 0f, 0f));
    let _e88 = (*c_1);
    cutoff = (_e88 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e90 = (*c_1);
    lo = (_e90 / vec3(12.92f));
    let _e93 = (*c_1);
    hi = pow(((_e93 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e98 = hi;
    let _e99 = lo;
    let _e100 = cutoff;
    return mix(_e98, _e99, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e100));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e109 = (*uv);
    let _e110 = textureSample(wired_bindless_images[(_e94 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    c_2 = _e110;
    let _e111 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e111))) == 0i) {
        let _e116 = c_2;
        param_11 = _e116.xyz;
        let _e118 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e118.x;
        c_2[1u] = _e118.y;
        c_2[2u] = _e118.z;
    }
    let _e125 = (*slot);
    if (lightmap_slot == (_e125 + 1i)) {
        let _e130 = unnamed.worldLightParams[0u];
        let _e131 = c_2;
        let _e133 = (_e131.xyz * _e130);
        c_2[0u] = _e133.x;
        c_2[1u] = _e133.y;
        c_2[2u] = _e133.z;
    }
    let _e140 = c_2;
    return _e140;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_12: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_13: vec3<f32>;
    var color0_: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color1_2: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var color1_3: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color1_4: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
    var color1_5: vec4<f32>;
    var param_32: u32;
    var param_33: vec2<f32>;
    var param_34: i32;
    var color1_6: vec4<f32>;
    var param_35: u32;
    var param_36: vec2<f32>;
    var param_37: i32;
    var param_38: vec3<f32>;
    var fogAmount: f32;

    let _e125 = unnamed.packed_indices[0i][3u];
    let _e131 = unnamed.packed_indices[0i][3u];
    let _e136 = fog_tex_coord_1;
    let _e137 = textureSample(wired_bindless_images[(_e125 & 4095u)], wired_bindless_samplers[((_e131 >> bitcast<u32>(12i)) & 255u)], _e136);
    fog = _e137;
    let _e138 = frag_color0In_1;
    param_12 = _e138.xyz;
    let _e140 = sRGBToLinear_u0028_vf3_u003b((&param_12));
    let _e142 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e140.x, _e140.y, _e140.z, _e142);
    let _e147 = frag_color1In_1;
    param_13 = _e147.xyz;
    let _e149 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e151 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e149.x, _e149.y, _e149.z, _e151);
    param_14 = 0u;
    let _e156 = frag_tex_coord0_1;
    param_15 = _e156;
    param_16 = 0i;
    let _e157 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    let _e158 = frag_color0_;
    color0_ = (_e157 * _e158);
    if override_type_3_6 {
        param_17 = 1u;
        let _e160 = frag_tex_coord1_1;
        param_18 = _e160;
        param_19 = 1i;
        let _e161 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
        let _e162 = frag_color1_;
        color1_ = (_e161 * _e162);
        let _e164 = color0_;
        let _e166 = color1_;
        let _e168 = (_e164.xyz + _e166.xyz);
        let _e170 = color0_[3u];
        let _e172 = color1_[3u];
        base = vec4<f32>(_e168.x, _e168.y, _e168.z, (_e170 * _e172));
    } else {
        if override_type_3_7 {
            param_20 = 1u;
            let _e178 = frag_tex_coord1_1;
            param_21 = _e178;
            param_22 = 1i;
            let _e179 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            let _e180 = frag_color1_;
            color1_1 = (_e179 * _e180);
            let _e183 = color0_[3u];
            let _e184 = color0_;
            color0_ = (_e184 * _e183);
            let _e187 = color1_1[3u];
            let _e188 = color1_1;
            color1_1 = (_e188 * _e187);
            let _e190 = color0_;
            let _e192 = color1_1;
            let _e194 = (_e190.xyz + _e192.xyz);
            let _e196 = color0_[3u];
            let _e198 = color1_1[3u];
            base = vec4<f32>(_e194.x, _e194.y, _e194.z, (_e196 * _e198));
        } else {
            if override_type_3_8 {
                param_23 = 1u;
                let _e204 = frag_tex_coord1_1;
                param_24 = _e204;
                param_25 = 1i;
                let _e205 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                let _e206 = frag_color1_;
                color1_2 = (_e205 * _e206);
                let _e209 = color0_[3u];
                let _e211 = color0_;
                color0_ = (_e211 * (1f - _e209));
                let _e214 = color1_2[3u];
                let _e216 = color1_2;
                color1_2 = (_e216 * (1f - _e214));
                let _e218 = color0_;
                let _e220 = color1_2;
                let _e222 = (_e218.xyz + _e220.xyz);
                let _e224 = color0_[3u];
                let _e226 = color1_2[3u];
                base = vec4<f32>(_e222.x, _e222.y, _e222.z, (_e224 * _e226));
            } else {
                if override_type_3_9 {
                    param_26 = 1u;
                    let _e232 = frag_tex_coord1_1;
                    param_27 = _e232;
                    param_28 = 1i;
                    let _e233 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                    let _e234 = frag_color1_;
                    color1_3 = (_e233 * _e234);
                    let _e236 = color0_;
                    let _e237 = color1_3;
                    let _e239 = color1_3[3u];
                    base = mix(_e236, _e237, vec4(_e239));
                } else {
                    if override_type_3_10 {
                        param_29 = 1u;
                        let _e242 = frag_tex_coord1_1;
                        param_30 = _e242;
                        param_31 = 1i;
                        let _e243 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                        let _e244 = frag_color1_;
                        color1_4 = (_e243 * _e244);
                        let _e246 = color1_4;
                        let _e247 = color0_;
                        let _e249 = color1_4[3u];
                        base = mix(_e246, _e247, vec4(_e249));
                    } else {
                        if override_type_3_11 {
                            param_32 = 1u;
                            let _e252 = frag_tex_coord1_1;
                            param_33 = _e252;
                            param_34 = 1i;
                            let _e253 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                            let _e254 = frag_color1_;
                            color1_5 = (_e253 * _e254);
                            let _e256 = color1_5;
                            let _e258 = color1_5[3u];
                            let _e261 = color0_;
                            base = ((_e256 + vec4(_e258)) * _e261);
                        } else {
                            param_35 = 1u;
                            let _e263 = frag_tex_coord1_1;
                            param_36 = _e263;
                            param_37 = 1i;
                            let _e264 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                            let _e265 = frag_color1_;
                            color1_6 = (_e264 * _e265);
                            let _e267 = color0_;
                            let _e269 = color1_6;
                            let _e271 = (_e267.xyz * _e269.xyz);
                            base[0u] = _e271.x;
                            base[1u] = _e271.y;
                            base[2u] = _e271.z;
                            let _e279 = color0_[3u];
                            let _e281 = color1_6[3u];
                            base[3u] = (_e279 * _e281);
                        }
                    }
                }
            }
        }
    }
    let _e284 = base;
    param_38 = _e284.xyz;
    let _e286 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_38));
    base[0u] = _e286.x;
    base[1u] = _e286.y;
    base[2u] = _e286.z;
    let _e293 = wired_advanced_fog_enabled_u0028_();
    if _e293 {
        let _e294 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e294;
        if override_type_3_12 {
            let _e295 = fogAmount;
            let _e297 = base;
            let _e299 = (_e297.xyz * (1f - _e295));
            base[0u] = _e299.x;
            base[1u] = _e299.y;
            base[2u] = _e299.z;
        } else {
            if override_type_3_13 {
                let _e306 = fogAmount;
                let _e308 = base;
                base = (_e308 * (1f - _e306));
            } else {
                if override_type_3_14 {
                    let _e310 = fogAmount;
                    let _e313 = base[3u];
                    base[3u] = (_e313 * (1f - _e310));
                } else {
                    let _e316 = base;
                    let _e319 = unnamed.advancedFogColorDensity;
                    let _e321 = fogAmount;
                    let _e323 = mix(_e316.xyz, _e319.xyz, vec3(_e321));
                    base[0u] = _e323.x;
                    base[1u] = _e323.y;
                    base[2u] = _e323.z;
                }
            }
        }
    } else {
        if override_type_3_15 {
            let _e330 = base;
            let _e333 = fog[3u];
            let _e335 = (_e330.xyz * (1f - _e333));
            base[0u] = _e335.x;
            base[1u] = _e335.y;
            base[2u] = _e335.z;
        } else {
            if override_type_3_16 {
                let _e342 = base;
                let _e344 = fog[3u];
                base = (_e342 * (1f - _e344));
            } else {
                if override_type_3_17 {
                    let _e348 = base[3u];
                    let _e350 = fog[3u];
                    base[3u] = (_e348 * (1f - _e350));
                } else {
                    let _e354 = base;
                    let _e355 = fog;
                    let _e357 = unnamed.fogColor;
                    let _e360 = fog[3u];
                    base = mix(_e354, (_e355 * _e357), vec4(_e360));
                }
            }
        }
    }
    if override_type_3_18 {
        let _e364 = base[3u];
        if (_e364 == 0f) {
            discard;
        }
    } else {
        if override_type_3_19 {
            let _e366 = base;
            let _e368 = base;
            if (dot(_e366.xyz, _e368.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e372 = base;
    out_color = _e372;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    main_1();
    let _e15 = out_color;
    return _e15;
}
