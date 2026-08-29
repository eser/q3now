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
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
override override_type_3_6: bool = (override_type_3_4 || override_type_3_5);
override override_type_3_7: bool = (tex_mode == 3i);
override override_type_3_8: bool = (tex_mode == 4i);
override override_type_3_9: bool = (tex_mode == 5i);
override override_type_3_10: bool = (tex_mode == 6i);
override override_type_3_11: bool = (tex_mode == 7i);
override override_type_3_12: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_13: bool = (acff == 1i);
override override_type_3_14: bool = (acff == 2i);
override override_type_3_15: bool = (acff == 3i);
override override_type_3_16: bool = (acff == 1i);
override override_type_3_17: bool = (acff == 2i);
override override_type_3_18: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_19: bool = (discard_mode == 1i);
override override_type_3_20: bool = (discard_mode == 2i);
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

    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e95 + 0.5f));
    let _e100 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e102 = fogType;
    let _e105 = fogType;
    return (((_e100 > 0.5f) && (_e102 >= 1i)) && (_e105 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e95 = wired_advanced_fog_enabled_u0028_();
    if !(_e95) {
        return 0f;
    }
    let _e98 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e98, 0.000001f));
    let _e103 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e103 + 0.5f));
    let _e106 = fogType_1;
    if (_e106 == 1i) {
        let _e110 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e110 <= 0f) {
            return 0f;
        }
        let _e112 = viewDepth;
        let _e115 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e112 / _e115), 0f, 1f);
    }
    let _e120 = unnamed.advancedFogColorDensity[3u];
    let _e122 = viewDepth;
    opticalDepth = (max(_e120, 0f) * _e122);
    let _e124 = fogType_1;
    if (_e124 == 2i) {
        let _e126 = opticalDepth;
        return clamp((1f - exp(-(_e126))), 0f, 1f);
    }
    let _e131 = opticalDepth;
    let _e132 = opticalDepth;
    return clamp((1f - exp(-((_e131 * _e132)))), 0f, 1f);
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

    let _e102 = (*c);
    let _e105 = unnamed.cascadeMVP[_e102];
    let _e106 = (*worldPos);
    sc4_ = (_e105 * vec4<f32>(_e106.x, _e106.y, _e106.z, 1f));
    let _e112 = sc4_;
    let _e115 = sc4_[3u];
    sc = (_e112.xyz / vec3(_e115));
    let _e118 = sc;
    let _e122 = ((_e118.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e122.x;
    sc[1u] = _e122.y;
    let _e128 = sc[0u];
    let _e129 = (_e128 < 0f);
    phi_304_ = _e129;
    if !(_e129) {
        let _e132 = sc[0u];
        phi_304_ = (_e132 > 1f);
    }
    let _e135 = phi_304_;
    phi_311_ = _e135;
    if !(_e135) {
        let _e138 = sc[1u];
        phi_311_ = (_e138 < 0f);
    }
    let _e141 = phi_311_;
    phi_318_ = _e141;
    if !(_e141) {
        let _e144 = sc[1u];
        phi_318_ = (_e144 > 1f);
    }
    let _e147 = phi_318_;
    phi_325_ = _e147;
    if !(_e147) {
        let _e150 = sc[2u];
        phi_325_ = (_e150 > 1f);
    }
    let _e153 = phi_325_;
    if _e153 {
        return 1f;
    }
    let _e154 = (*c);
    layer = f32(_e154);
    let _e156 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e156).xy));
    let _e163 = sc[2u];
    currentDepth = (_e163 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e165 = currentDepth;
        let _e166 = sc;
        let _e167 = _e166.xy;
        let _e168 = layer;
        let _e171 = vec3<f32>(_e167.x, _e167.y, _e168);
        let _e177 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e171.x, _e171.y), i32(_e171.z));
        shadow = step(_e165, _e177.x);
    } else {
        if override_type_3_1 {
            let _e180 = currentDepth;
            let _e181 = sc;
            let _e182 = _e181.xy;
            let _e183 = layer;
            let _e186 = vec3<f32>(_e182.x, _e182.y, _e183);
            let _e192 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e186.x, _e186.y), i32(_e186.z));
            let _e195 = shadow;
            shadow = (_e195 + step(_e180, _e192.x));
            let _e197 = currentDepth;
            let _e198 = sc;
            let _e201 = texelSize[0u];
            let _e203 = (_e198.xy + vec2<f32>(_e201, 0f));
            let _e204 = layer;
            let _e207 = vec3<f32>(_e203.x, _e203.y, _e204);
            let _e213 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e207.x, _e207.y), i32(_e207.z));
            let _e216 = shadow;
            shadow = (_e216 + step(_e197, _e213.x));
            let _e218 = currentDepth;
            let _e219 = sc;
            let _e222 = texelSize[0u];
            let _e224 = (_e219.xy - vec2<f32>(_e222, 0f));
            let _e225 = layer;
            let _e228 = vec3<f32>(_e224.x, _e224.y, _e225);
            let _e234 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e228.x, _e228.y), i32(_e228.z));
            let _e237 = shadow;
            shadow = (_e237 + step(_e218, _e234.x));
            let _e239 = currentDepth;
            let _e240 = sc;
            let _e243 = texelSize[1u];
            let _e245 = (_e240.xy + vec2<f32>(0f, _e243));
            let _e246 = layer;
            let _e249 = vec3<f32>(_e245.x, _e245.y, _e246);
            let _e255 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e249.x, _e249.y), i32(_e249.z));
            let _e258 = shadow;
            shadow = (_e258 + step(_e239, _e255.x));
            let _e260 = currentDepth;
            let _e261 = sc;
            let _e264 = texelSize[1u];
            let _e266 = (_e261.xy - vec2<f32>(0f, _e264));
            let _e267 = layer;
            let _e270 = vec3<f32>(_e266.x, _e266.y, _e267);
            let _e276 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e270.x, _e270.y), i32(_e270.z));
            let _e279 = shadow;
            shadow = (_e279 + step(_e260, _e276.x));
            let _e281 = shadow;
            shadow = (_e281 / 5f);
        } else {
            x = -1i;
            loop {
                let _e283 = x;
                if (_e283 <= 1i) {
                    y = -1i;
                    loop {
                        let _e285 = y;
                        if (_e285 <= 1i) {
                            let _e287 = currentDepth;
                            let _e288 = sc;
                            let _e290 = x;
                            let _e292 = y;
                            let _e295 = texelSize;
                            let _e297 = (_e288.xy + (vec2<f32>(f32(_e290), f32(_e292)) * _e295));
                            let _e298 = layer;
                            let _e301 = vec3<f32>(_e297.x, _e297.y, _e298);
                            let _e307 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e301.x, _e301.y), i32(_e301.z));
                            let _e310 = shadow;
                            shadow = (_e310 + step(_e287, _e307.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e312 = y;
                            y = (_e312 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e314 = x;
                    x = (_e314 + 1i);
                }
            }
            let _e316 = shadow;
            shadow = (_e316 / 9f);
        }
    }
    let _e318 = shadow;
    return _e318;
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

    let _e109 = unnamed.cascadeSplits;
    let _e110 = (*viewDepth_1);
    cmp = step(_e109, vec4(_e110));
    let _e114 = cmp[0u];
    let _e116 = cmp[1u];
    let _e119 = cmp[2u];
    let _e122 = cmp[3u];
    cascade = min(i32((((_e114 + _e116) + _e119) + _e122)), 3i);
    let _e126 = cascade;
    (*outCascade) = _e126;
    let _e127 = cascade;
    if (_e127 == 0i) {
        local = 0f;
    } else {
        let _e129 = cascade;
        let _e134 = unnamed.cascadeSplits[max((_e129 - 1i), 0i)];
        local = _e134;
    }
    let _e135 = local;
    prevSplit = _e135;
    let _e136 = cascade;
    let _e139 = unnamed.cascadeSplits[_e136];
    farSplit = _e139;
    let _e140 = farSplit;
    let _e141 = prevSplit;
    blendRange = max((0.1f * (_e140 - _e141)), 1f);
    let _e145 = farSplit;
    let _e146 = (*viewDepth_1);
    let _e148 = blendRange;
    blendT = clamp(((_e145 - _e146) / _e148), 0f, 1f);
    let _e151 = cascade;
    param = _e151;
    let _e152 = (*worldPos_1);
    param_1 = _e152;
    let _e153 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e153;
    let _e154 = cascade;
    param_2 = min((_e154 + 1i), 3i);
    let _e157 = (*worldPos_1);
    param_3 = _e157;
    let _e158 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e158;
    let _e159 = s1_;
    let _e160 = s0_;
    let _e161 = blendT;
    return mix(_e159, _e160, _e161);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e103 = unnamed.packed_indices[1i][3u];
    let _e109 = unnamed.packed_indices[1i][3u];
    let _e114 = (*lm_uv);
    let _e115 = textureSample(wired_bindless_images[(_e103 & 4095u)], wired_bindless_samplers[((_e109 >> bitcast<u32>(12i)) & 255u)], _e114);
    sun_mask = _e115.x;
    let _e117 = sun_mask;
    if (_e117 > 0.001f) {
        let _e119 = shadowData_1;
        param_4 = _e119.xyz;
        let _e122 = shadowData_1[3u];
        param_5 = _e122;
        let _e123 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e124 = param_6;
        ignoredCascade = _e124;
        shadow_1 = _e123;
        let _e125 = shadow_1;
        let _e126 = sun_mask;
        let _e128 = (*rgb);
        (*rgb) = (_e128 * mix(1f, _e125, _e126));
    }
    let _e130 = (*rgb);
    return _e130;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e97 = (*rgb_1);
        param_7 = _e97;
        let _e98 = frag_tex_coord0_1;
        param_8 = _e98;
        let _e99 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e99;
    }
    if override_type_3_3 {
        let _e100 = (*rgb_1);
        param_9 = _e100;
        let _e101 = frag_tex_coord1_1;
        param_10 = _e101;
        let _e102 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e102;
    }
    let _e103 = (*rgb_1);
    return _e103;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e96 = (*c_1);
    (*c_1) = max(_e96, vec3<f32>(0f, 0f, 0f));
    let _e98 = (*c_1);
    cutoff = (_e98 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e100 = (*c_1);
    lo = (_e100 / vec3(12.92f));
    let _e103 = (*c_1);
    hi = pow(((_e103 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e108 = hi;
    let _e109 = lo;
    let _e110 = cutoff;
    return mix(_e108, _e109, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e110));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e107 = (*role);
    let _e109 = (*role);
    let _e114 = unnamed.packed_indices[(_e107 / 4u)][(_e109 % 4u)];
    let _e119 = (*uv);
    let _e120 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    c_2 = _e120;
    let _e121 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e121))) == 0i) {
        let _e126 = c_2;
        param_11 = _e126.xyz;
        let _e128 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e128.x;
        c_2[1u] = _e128.y;
        c_2[2u] = _e128.z;
    }
    let _e135 = (*slot);
    if (lightmap_slot == (_e135 + 1i)) {
        let _e140 = unnamed.worldLightParams[0u];
        let _e141 = c_2;
        let _e143 = (_e141.xyz * _e140);
        c_2[0u] = _e143.x;
        c_2[1u] = _e143.y;
        c_2[2u] = _e143.z;
    }
    let _e150 = c_2;
    return _e150;
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
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e138 = unnamed.packed_indices[0i][3u];
    let _e144 = unnamed.packed_indices[0i][3u];
    let _e149 = fog_tex_coord_1;
    let _e150 = textureSample(wired_bindless_images[(_e138 & 4095u)], wired_bindless_samplers[((_e144 >> bitcast<u32>(12i)) & 255u)], _e149);
    fog = _e150;
    let _e151 = frag_color0In_1;
    param_12 = _e151.xyz;
    let _e153 = sRGBToLinear_u0028_vf3_u003b((&param_12));
    let _e155 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e153.x, _e153.y, _e153.z, _e155);
    let _e160 = frag_color1In_1;
    param_13 = _e160.xyz;
    let _e162 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e164 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e162.x, _e162.y, _e162.z, _e164);
    param_14 = 0u;
    let _e169 = frag_tex_coord0_1;
    param_15 = _e169;
    param_16 = 0i;
    let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    let _e171 = frag_color0_;
    color0_ = (_e170 * _e171);
    if override_type_3_6 {
        param_17 = 1u;
        let _e173 = frag_tex_coord1_1;
        param_18 = _e173;
        param_19 = 1i;
        let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
        let _e175 = frag_color1_;
        color1_ = (_e174 * _e175);
        let _e177 = color0_;
        let _e179 = color1_;
        let _e181 = (_e177.xyz + _e179.xyz);
        let _e183 = color0_[3u];
        let _e185 = color1_[3u];
        base = vec4<f32>(_e181.x, _e181.y, _e181.z, (_e183 * _e185));
    } else {
        if override_type_3_7 {
            param_20 = 1u;
            let _e191 = frag_tex_coord1_1;
            param_21 = _e191;
            param_22 = 1i;
            let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            let _e193 = frag_color1_;
            color1_1 = (_e192 * _e193);
            let _e196 = color0_[3u];
            let _e197 = color0_;
            color0_ = (_e197 * _e196);
            let _e200 = color1_1[3u];
            let _e201 = color1_1;
            color1_1 = (_e201 * _e200);
            let _e203 = color0_;
            let _e205 = color1_1;
            let _e207 = (_e203.xyz + _e205.xyz);
            let _e209 = color0_[3u];
            let _e211 = color1_1[3u];
            base = vec4<f32>(_e207.x, _e207.y, _e207.z, (_e209 * _e211));
        } else {
            if override_type_3_8 {
                param_23 = 1u;
                let _e217 = frag_tex_coord1_1;
                param_24 = _e217;
                param_25 = 1i;
                let _e218 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                let _e219 = frag_color1_;
                color1_2 = (_e218 * _e219);
                let _e222 = color0_[3u];
                let _e224 = color0_;
                color0_ = (_e224 * (1f - _e222));
                let _e227 = color1_2[3u];
                let _e229 = color1_2;
                color1_2 = (_e229 * (1f - _e227));
                let _e231 = color0_;
                let _e233 = color1_2;
                let _e235 = (_e231.xyz + _e233.xyz);
                let _e237 = color0_[3u];
                let _e239 = color1_2[3u];
                base = vec4<f32>(_e235.x, _e235.y, _e235.z, (_e237 * _e239));
            } else {
                if override_type_3_9 {
                    param_26 = 1u;
                    let _e245 = frag_tex_coord1_1;
                    param_27 = _e245;
                    param_28 = 1i;
                    let _e246 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                    let _e247 = frag_color1_;
                    color1_3 = (_e246 * _e247);
                    let _e249 = color0_;
                    let _e250 = color1_3;
                    let _e252 = color1_3[3u];
                    base = mix(_e249, _e250, vec4(_e252));
                } else {
                    if override_type_3_10 {
                        param_29 = 1u;
                        let _e255 = frag_tex_coord1_1;
                        param_30 = _e255;
                        param_31 = 1i;
                        let _e256 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                        let _e257 = frag_color1_;
                        color1_4 = (_e256 * _e257);
                        let _e259 = color1_4;
                        let _e260 = color0_;
                        let _e262 = color1_4[3u];
                        base = mix(_e259, _e260, vec4(_e262));
                    } else {
                        if override_type_3_11 {
                            param_32 = 1u;
                            let _e265 = frag_tex_coord1_1;
                            param_33 = _e265;
                            param_34 = 1i;
                            let _e266 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                            let _e267 = frag_color1_;
                            color1_5 = (_e266 * _e267);
                            let _e269 = color1_5;
                            let _e271 = color1_5[3u];
                            let _e274 = color0_;
                            base = ((_e269 + vec4(_e271)) * _e274);
                        } else {
                            param_35 = 1u;
                            let _e276 = frag_tex_coord1_1;
                            param_36 = _e276;
                            param_37 = 1i;
                            let _e277 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                            let _e278 = frag_color1_;
                            color1_6 = (_e277 * _e278);
                            let _e280 = color0_;
                            let _e282 = color1_6;
                            let _e284 = (_e280.xyz * _e282.xyz);
                            base[0u] = _e284.x;
                            base[1u] = _e284.y;
                            base[2u] = _e284.z;
                            let _e292 = color0_[3u];
                            let _e294 = color1_6[3u];
                            base[3u] = (_e292 * _e294);
                        }
                    }
                }
            }
        }
    }
    let _e297 = base;
    param_38 = _e297.xyz;
    let _e299 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_38));
    base[0u] = _e299.x;
    base[1u] = _e299.y;
    base[2u] = _e299.z;
    if override_type_3_12 {
        let _e308 = unnamed.worldLightParams[1u];
        wetness = clamp(_e308, 0f, 1f);
        let _e312 = unnamed.worldLightParams[2u];
        frost = clamp(_e312, 0f, 1f);
        let _e314 = base;
        luminance = dot(_e314.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e317 = wetness;
        let _e319 = base;
        let _e321 = (_e319.xyz * mix(1f, 0.82f, _e317));
        base[0u] = _e321.x;
        base[1u] = _e321.y;
        base[2u] = _e321.z;
        let _e328 = base;
        let _e330 = luminance;
        let _e332 = luminance;
        let _e334 = luminance;
        let _e336 = frost;
        let _e339 = mix(_e328.xyz, vec3<f32>((_e330 * 0.88f), (_e332 * 0.94f), _e334), vec3((_e336 * 0.55f)));
        base[0u] = _e339.x;
        base[1u] = _e339.y;
        base[2u] = _e339.z;
    }
    let _e346 = color0_;
    let _e349 = unnamed.emissionRadiance;
    let _e352 = base;
    let _e354 = (_e352.xyz + (_e346.xyz * _e349.xyz));
    base[0u] = _e354.x;
    base[1u] = _e354.y;
    base[2u] = _e354.z;
    let _e361 = wired_advanced_fog_enabled_u0028_();
    if _e361 {
        let _e362 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e362;
        if override_type_3_13 {
            let _e363 = fogAmount;
            let _e365 = base;
            let _e367 = (_e365.xyz * (1f - _e363));
            base[0u] = _e367.x;
            base[1u] = _e367.y;
            base[2u] = _e367.z;
        } else {
            if override_type_3_14 {
                let _e374 = fogAmount;
                let _e376 = base;
                base = (_e376 * (1f - _e374));
            } else {
                if override_type_3_15 {
                    let _e378 = fogAmount;
                    let _e381 = base[3u];
                    base[3u] = (_e381 * (1f - _e378));
                } else {
                    let _e384 = base;
                    let _e387 = unnamed.advancedFogColorDensity;
                    let _e389 = fogAmount;
                    let _e391 = mix(_e384.xyz, _e387.xyz, vec3(_e389));
                    base[0u] = _e391.x;
                    base[1u] = _e391.y;
                    base[2u] = _e391.z;
                }
            }
        }
    } else {
        if override_type_3_16 {
            let _e398 = base;
            let _e401 = fog[3u];
            let _e403 = (_e398.xyz * (1f - _e401));
            base[0u] = _e403.x;
            base[1u] = _e403.y;
            base[2u] = _e403.z;
        } else {
            if override_type_3_17 {
                let _e410 = base;
                let _e412 = fog[3u];
                base = (_e410 * (1f - _e412));
            } else {
                if override_type_3_18 {
                    let _e416 = base[3u];
                    let _e418 = fog[3u];
                    base[3u] = (_e416 * (1f - _e418));
                } else {
                    let _e422 = base;
                    let _e423 = fog;
                    let _e425 = unnamed.fogColor;
                    let _e428 = fog[3u];
                    base = mix(_e422, (_e423 * _e425), vec4(_e428));
                }
            }
        }
    }
    if override_type_3_19 {
        let _e432 = base[3u];
        if (_e432 == 0f) {
            discard;
        }
    } else {
        if override_type_3_20 {
            let _e434 = base;
            let _e436 = base;
            if (dot(_e434.xyz, _e436.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e440 = base;
    out_color = _e440;
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
