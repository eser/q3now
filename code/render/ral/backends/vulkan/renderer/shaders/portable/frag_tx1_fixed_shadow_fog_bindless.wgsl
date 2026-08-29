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
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
override override_type_3_3: bool = (lightmap_slot == 2i);
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
override override_type_3_6: bool = (lightmap_slot != 0i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

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
    var phi_304_: bool;
    var phi_311_: bool;
    var phi_318_: bool;
    var phi_325_: bool;

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
    phi_304_ = _e121;
    if !(_e121) {
        let _e124 = sc[0u];
        phi_304_ = (_e124 > 1f);
    }
    let _e127 = phi_304_;
    phi_311_ = _e127;
    if !(_e127) {
        let _e130 = sc[1u];
        phi_311_ = (_e130 < 0f);
    }
    let _e133 = phi_311_;
    phi_318_ = _e133;
    if !(_e133) {
        let _e136 = sc[1u];
        phi_318_ = (_e136 > 1f);
    }
    let _e139 = phi_318_;
    phi_325_ = _e139;
    if !(_e139) {
        let _e142 = sc[2u];
        phi_325_ = (_e142 > 1f);
    }
    let _e145 = phi_325_;
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
    if override_type_3_ {
        let _e157 = currentDepth;
        let _e158 = sc;
        let _e159 = _e158.xy;
        let _e160 = layer;
        let _e163 = vec3<f32>(_e159.x, _e159.y, _e160);
        let _e169 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e163.x, _e163.y), i32(_e163.z));
        shadow = step(_e157, _e169.x);
    } else {
        if override_type_3_1 {
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

    if override_type_3_2 {
        let _e89 = (*rgb_1);
        param_7 = _e89;
        let _e90 = frag_tex_coord0_1;
        param_8 = _e90;
        let _e91 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e91;
    }
    if override_type_3_3 {
        let _e92 = (*rgb_1);
        param_9 = _e92;
        let _e93 = frag_tex_coord1_1;
        param_10 = _e93;
        let _e94 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e94;
    }
    let _e95 = (*rgb_1);
    return _e95;
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
    var param_11: vec3<f32>;

    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e111 = (*uv);
    let _e112 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    c_2 = _e112;
    let _e113 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e113))) == 0i) {
        let _e118 = c_2;
        param_11 = _e118.xyz;
        let _e120 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e120.x;
        c_2[1u] = _e120.y;
        c_2[2u] = _e120.z;
    }
    let _e127 = (*slot);
    if (lightmap_slot == (_e127 + 1i)) {
        let _e132 = unnamed.worldLightParams[0u];
        let _e133 = c_2;
        let _e135 = (_e133.xyz * _e132);
        c_2[0u] = _e135.x;
        c_2[1u] = _e135.y;
        c_2[2u] = _e135.z;
    }
    let _e142 = c_2;
    return _e142;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_2: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var param_24: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e111 = unnamed.packed_indices[0i][3u];
    let _e117 = unnamed.packed_indices[0i][3u];
    let _e122 = fog_tex_coord_1;
    let _e123 = textureSample(wired_bindless_images[(_e111 & 4095u)], wired_bindless_samplers[((_e117 >> bitcast<u32>(12i)) & 255u)], _e122);
    fog = _e123;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_12 = 0u;
    let _e128 = frag_tex_coord0_1;
    param_13 = _e128;
    param_14 = 0i;
    let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    let _e130 = frag_color;
    color0_ = (_e129 * _e130);
    if override_type_3_4 {
        param_15 = 1u;
        let _e132 = frag_tex_coord1_1;
        param_16 = _e132;
        param_17 = 1i;
        let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        color1_ = _e133;
        let _e134 = color0_;
        let _e136 = color1_;
        let _e138 = (_e134.xyz + _e136.xyz);
        let _e140 = color0_[3u];
        let _e142 = color1_[3u];
        base = vec4<f32>(_e138.x, _e138.y, _e138.z, (_e140 * _e142));
    } else {
        if override_type_3_5 {
            param_18 = 1u;
            let _e148 = frag_tex_coord1_1;
            param_19 = _e148;
            param_20 = 1i;
            let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            let _e150 = frag_color;
            color1_1 = (_e149 * _e150);
            let _e152 = color0_;
            let _e154 = color1_1;
            let _e156 = (_e152.xyz + _e154.xyz);
            let _e158 = color0_[3u];
            let _e160 = color1_1[3u];
            base = vec4<f32>(_e156.x, _e156.y, _e156.z, (_e158 * _e160));
        } else {
            param_21 = 1u;
            let _e166 = frag_tex_coord1_1;
            param_22 = _e166;
            param_23 = 1i;
            let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            let _e168 = frag_color;
            color1_2 = (_e167 * _e168);
            let _e170 = color0_;
            let _e172 = color1_2;
            let _e174 = (_e170.xyz * _e172.xyz);
            base[0u] = _e174.x;
            base[1u] = _e174.y;
            base[2u] = _e174.z;
            let _e182 = color0_[3u];
            let _e184 = color1_2[3u];
            base[3u] = (_e182 * _e184);
        }
    }
    let _e187 = base;
    param_24 = _e187.xyz;
    let _e189 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_24));
    base[0u] = _e189.x;
    base[1u] = _e189.y;
    base[2u] = _e189.z;
    if override_type_3_6 {
        let _e198 = unnamed.worldLightParams[1u];
        wetness = clamp(_e198, 0f, 1f);
        let _e202 = unnamed.worldLightParams[2u];
        frost = clamp(_e202, 0f, 1f);
        let _e204 = base;
        luminance = dot(_e204.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e207 = wetness;
        let _e209 = base;
        let _e211 = (_e209.xyz * mix(1f, 0.82f, _e207));
        base[0u] = _e211.x;
        base[1u] = _e211.y;
        base[2u] = _e211.z;
        let _e218 = base;
        let _e220 = luminance;
        let _e222 = luminance;
        let _e224 = luminance;
        let _e226 = frost;
        let _e229 = mix(_e218.xyz, vec3<f32>((_e220 * 0.88f), (_e222 * 0.94f), _e224), vec3((_e226 * 0.55f)));
        base[0u] = _e229.x;
        base[1u] = _e229.y;
        base[2u] = _e229.z;
    }
    let _e236 = color0_;
    let _e239 = unnamed.emissionRadiance;
    let _e242 = base;
    let _e244 = (_e242.xyz + (_e236.xyz * _e239.xyz));
    base[0u] = _e244.x;
    base[1u] = _e244.y;
    base[2u] = _e244.z;
    let _e251 = wired_advanced_fog_enabled_u0028_();
    if _e251 {
        let _e252 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e252;
        if override_type_3_7 {
            let _e253 = fogAmount;
            let _e255 = base;
            let _e257 = (_e255.xyz * (1f - _e253));
            base[0u] = _e257.x;
            base[1u] = _e257.y;
            base[2u] = _e257.z;
        } else {
            if override_type_3_8 {
                let _e264 = fogAmount;
                let _e266 = base;
                base = (_e266 * (1f - _e264));
            } else {
                if override_type_3_9 {
                    let _e268 = fogAmount;
                    let _e271 = base[3u];
                    base[3u] = (_e271 * (1f - _e268));
                } else {
                    let _e274 = base;
                    let _e277 = unnamed.advancedFogColorDensity;
                    let _e279 = fogAmount;
                    let _e281 = mix(_e274.xyz, _e277.xyz, vec3(_e279));
                    base[0u] = _e281.x;
                    base[1u] = _e281.y;
                    base[2u] = _e281.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e288 = base;
            let _e291 = fog[3u];
            let _e293 = (_e288.xyz * (1f - _e291));
            base[0u] = _e293.x;
            base[1u] = _e293.y;
            base[2u] = _e293.z;
        } else {
            if override_type_3_11 {
                let _e300 = base;
                let _e302 = fog[3u];
                base = (_e300 * (1f - _e302));
            } else {
                if override_type_3_12 {
                    let _e306 = base[3u];
                    let _e308 = fog[3u];
                    base[3u] = (_e306 * (1f - _e308));
                } else {
                    let _e312 = base;
                    let _e313 = fog;
                    let _e315 = unnamed.fogColor;
                    let _e318 = fog[3u];
                    base = mix(_e312, (_e313 * _e315), vec4(_e318));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e322 = base[3u];
        if (_e322 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e324 = base;
            let _e326 = base;
            if (dot(_e324.xyz, _e326.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e330 = base;
    out_color = _e330;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e11 = out_color;
    return _e11;
}
