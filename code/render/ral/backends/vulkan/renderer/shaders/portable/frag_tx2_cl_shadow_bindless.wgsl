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
override override_type_3_7: bool = (override_type_3_5 || override_type_3_6);
override override_type_3_8: bool = (tex_mode == 3i);
override override_type_3_9: bool = (tex_mode == 4i);
override override_type_3_10: bool = (tex_mode == 5i);
override override_type_3_11: bool = (tex_mode == 6i);
override override_type_3_12: bool = (tex_mode == 7i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e80 + 0.5f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e87 = fogType;
    let _e90 = fogType;
    return (((_e85 > 0.5f) && (_e87 >= 1i)) && (_e90 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e80 = wired_advanced_fog_enabled_u0028_();
    if !(_e80) {
        return 0f;
    }
    let _e83 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e83, 0.000001f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e88 + 0.5f));
    let _e91 = fogType_1;
    if (_e91 == 1i) {
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e95 <= 0f) {
            return 0f;
        }
        let _e97 = viewDepth;
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e97 / _e100), 0f, 1f);
    }
    let _e105 = unnamed.advancedFogColorDensity[3u];
    let _e107 = viewDepth;
    opticalDepth = (max(_e105, 0f) * _e107);
    let _e109 = fogType_1;
    if (_e109 == 2i) {
        let _e111 = opticalDepth;
        return clamp((1f - exp(-(_e111))), 0f, 1f);
    }
    let _e116 = opticalDepth;
    let _e117 = opticalDepth;
    return clamp((1f - exp(-((_e116 * _e117)))), 0f, 1f);
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

    let _e87 = (*c);
    let _e90 = unnamed.cascadeMVP[_e87];
    let _e91 = (*worldPos);
    sc4_ = (_e90 * vec4<f32>(_e91.x, _e91.y, _e91.z, 1f));
    let _e97 = sc4_;
    let _e100 = sc4_[3u];
    sc = (_e97.xyz / vec3(_e100));
    let _e103 = sc;
    let _e107 = ((_e103.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e107.x;
    sc[1u] = _e107.y;
    let _e113 = sc[0u];
    let _e114 = (_e113 < 0f);
    phi_304_ = _e114;
    if !(_e114) {
        let _e117 = sc[0u];
        phi_304_ = (_e117 > 1f);
    }
    let _e120 = phi_304_;
    phi_311_ = _e120;
    if !(_e120) {
        let _e123 = sc[1u];
        phi_311_ = (_e123 < 0f);
    }
    let _e126 = phi_311_;
    phi_318_ = _e126;
    if !(_e126) {
        let _e129 = sc[1u];
        phi_318_ = (_e129 > 1f);
    }
    let _e132 = phi_318_;
    phi_325_ = _e132;
    if !(_e132) {
        let _e135 = sc[2u];
        phi_325_ = (_e135 > 1f);
    }
    let _e138 = phi_325_;
    if _e138 {
        return 1f;
    }
    let _e139 = (*c);
    layer = f32(_e139);
    let _e141 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e141).xy));
    let _e148 = sc[2u];
    currentDepth = (_e148 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e150 = currentDepth;
        let _e151 = sc;
        let _e152 = _e151.xy;
        let _e153 = layer;
        let _e156 = vec3<f32>(_e152.x, _e152.y, _e153);
        let _e162 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e156.x, _e156.y), i32(_e156.z));
        shadow = step(_e150, _e162.x);
    } else {
        if override_type_3_1 {
            let _e165 = currentDepth;
            let _e166 = sc;
            let _e167 = _e166.xy;
            let _e168 = layer;
            let _e171 = vec3<f32>(_e167.x, _e167.y, _e168);
            let _e177 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e171.x, _e171.y), i32(_e171.z));
            let _e180 = shadow;
            shadow = (_e180 + step(_e165, _e177.x));
            let _e182 = currentDepth;
            let _e183 = sc;
            let _e186 = texelSize[0u];
            let _e188 = (_e183.xy + vec2<f32>(_e186, 0f));
            let _e189 = layer;
            let _e192 = vec3<f32>(_e188.x, _e188.y, _e189);
            let _e198 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e192.x, _e192.y), i32(_e192.z));
            let _e201 = shadow;
            shadow = (_e201 + step(_e182, _e198.x));
            let _e203 = currentDepth;
            let _e204 = sc;
            let _e207 = texelSize[0u];
            let _e209 = (_e204.xy - vec2<f32>(_e207, 0f));
            let _e210 = layer;
            let _e213 = vec3<f32>(_e209.x, _e209.y, _e210);
            let _e219 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e213.x, _e213.y), i32(_e213.z));
            let _e222 = shadow;
            shadow = (_e222 + step(_e203, _e219.x));
            let _e224 = currentDepth;
            let _e225 = sc;
            let _e228 = texelSize[1u];
            let _e230 = (_e225.xy + vec2<f32>(0f, _e228));
            let _e231 = layer;
            let _e234 = vec3<f32>(_e230.x, _e230.y, _e231);
            let _e240 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e234.x, _e234.y), i32(_e234.z));
            let _e243 = shadow;
            shadow = (_e243 + step(_e224, _e240.x));
            let _e245 = currentDepth;
            let _e246 = sc;
            let _e249 = texelSize[1u];
            let _e251 = (_e246.xy - vec2<f32>(0f, _e249));
            let _e252 = layer;
            let _e255 = vec3<f32>(_e251.x, _e251.y, _e252);
            let _e261 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e255.x, _e255.y), i32(_e255.z));
            let _e264 = shadow;
            shadow = (_e264 + step(_e245, _e261.x));
            let _e266 = shadow;
            shadow = (_e266 / 5f);
        } else {
            x = -1i;
            loop {
                let _e268 = x;
                if (_e268 <= 1i) {
                    y = -1i;
                    loop {
                        let _e270 = y;
                        if (_e270 <= 1i) {
                            let _e272 = currentDepth;
                            let _e273 = sc;
                            let _e275 = x;
                            let _e277 = y;
                            let _e280 = texelSize;
                            let _e282 = (_e273.xy + (vec2<f32>(f32(_e275), f32(_e277)) * _e280));
                            let _e283 = layer;
                            let _e286 = vec3<f32>(_e282.x, _e282.y, _e283);
                            let _e292 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e286.x, _e286.y), i32(_e286.z));
                            let _e295 = shadow;
                            shadow = (_e295 + step(_e272, _e292.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e297 = y;
                            y = (_e297 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e299 = x;
                    x = (_e299 + 1i);
                }
            }
            let _e301 = shadow;
            shadow = (_e301 / 9f);
        }
    }
    let _e303 = shadow;
    return _e303;
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

    let _e94 = unnamed.cascadeSplits;
    let _e95 = (*viewDepth_1);
    cmp = step(_e94, vec4(_e95));
    let _e99 = cmp[0u];
    let _e101 = cmp[1u];
    let _e104 = cmp[2u];
    let _e107 = cmp[3u];
    cascade = min(i32((((_e99 + _e101) + _e104) + _e107)), 3i);
    let _e111 = cascade;
    (*outCascade) = _e111;
    let _e112 = cascade;
    if (_e112 == 0i) {
        local = 0f;
    } else {
        let _e114 = cascade;
        let _e119 = unnamed.cascadeSplits[max((_e114 - 1i), 0i)];
        local = _e119;
    }
    let _e120 = local;
    prevSplit = _e120;
    let _e121 = cascade;
    let _e124 = unnamed.cascadeSplits[_e121];
    farSplit = _e124;
    let _e125 = farSplit;
    let _e126 = prevSplit;
    blendRange = max((0.1f * (_e125 - _e126)), 1f);
    let _e130 = farSplit;
    let _e131 = (*viewDepth_1);
    let _e133 = blendRange;
    blendT = clamp(((_e130 - _e131) / _e133), 0f, 1f);
    let _e136 = cascade;
    param = _e136;
    let _e137 = (*worldPos_1);
    param_1 = _e137;
    let _e138 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e138;
    let _e139 = cascade;
    param_2 = min((_e139 + 1i), 3i);
    let _e142 = (*worldPos_1);
    param_3 = _e142;
    let _e143 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e143;
    let _e144 = s1_;
    let _e145 = s0_;
    let _e146 = blendT;
    return mix(_e144, _e145, _e146);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e88 = unnamed.packed_indices[1i][3u];
    let _e94 = unnamed.packed_indices[1i][3u];
    let _e99 = (*lm_uv);
    let _e100 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    sun_mask = _e100.x;
    let _e102 = sun_mask;
    if (_e102 > 0.001f) {
        let _e104 = shadowData_1;
        param_4 = _e104.xyz;
        let _e107 = shadowData_1[3u];
        param_5 = _e107;
        let _e108 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e109 = param_6;
        ignoredCascade = _e109;
        shadow_1 = _e108;
        let _e110 = shadow_1;
        let _e111 = sun_mask;
        let _e113 = (*rgb);
        (*rgb) = (_e113 * mix(1f, _e110, _e111));
    }
    let _e115 = (*rgb);
    return _e115;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e84 = (*rgb_1);
        param_7 = _e84;
        let _e85 = frag_tex_coord0_1;
        param_8 = _e85;
        let _e86 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e86;
    }
    if override_type_3_3 {
        let _e87 = (*rgb_1);
        param_9 = _e87;
        let _e88 = frag_tex_coord1_1;
        param_10 = _e88;
        let _e89 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e89;
    }
    if override_type_3_4 {
        let _e90 = (*rgb_1);
        param_11 = _e90;
        let _e91 = frag_tex_coord2_1;
        param_12 = _e91;
        let _e92 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e92;
    }
    let _e93 = (*rgb_1);
    return _e93;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e81 = (*c_1);
    (*c_1) = max(_e81, vec3<f32>(0f, 0f, 0f));
    let _e83 = (*c_1);
    cutoff = (_e83 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e85 = (*c_1);
    lo = (_e85 / vec3(12.92f));
    let _e88 = (*c_1);
    hi = pow(((_e88 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e93 = hi;
    let _e94 = lo;
    let _e95 = cutoff;
    return mix(_e93, _e94, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e95));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;

    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e104 = (*uv);
    let _e105 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    c_2 = _e105;
    let _e106 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e106))) == 0i) {
        let _e111 = c_2;
        param_13 = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e113.x;
        c_2[1u] = _e113.y;
        c_2[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e125 = unnamed.worldLightParams[0u];
        let _e126 = c_2;
        let _e128 = (_e126.xyz * _e125);
        c_2[0u] = _e128.x;
        c_2[1u] = _e128.y;
        c_2[2u] = _e128.z;
    }
    let _e135 = c_2;
    return _e135;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_14: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_15: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_16: vec3<f32>;
    var color0_: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color1_: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color2_: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color2_1: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
    var color1_2: vec4<f32>;
    var param_32: u32;
    var param_33: vec2<f32>;
    var param_34: i32;
    var color2_2: vec4<f32>;
    var param_35: u32;
    var param_36: vec2<f32>;
    var param_37: i32;
    var color1_3: vec4<f32>;
    var param_38: u32;
    var param_39: vec2<f32>;
    var param_40: i32;
    var color2_3: vec4<f32>;
    var param_41: u32;
    var param_42: vec2<f32>;
    var param_43: i32;
    var color1_4: vec4<f32>;
    var param_44: u32;
    var param_45: vec2<f32>;
    var param_46: i32;
    var color2_4: vec4<f32>;
    var param_47: u32;
    var param_48: vec2<f32>;
    var param_49: i32;
    var color1_5: vec4<f32>;
    var param_50: u32;
    var param_51: vec2<f32>;
    var param_52: i32;
    var color2_5: vec4<f32>;
    var param_53: u32;
    var param_54: vec2<f32>;
    var param_55: i32;
    var color1_6: vec4<f32>;
    var param_56: u32;
    var param_57: vec2<f32>;
    var param_58: i32;
    var color2_6: vec4<f32>;
    var param_59: u32;
    var param_60: vec2<f32>;
    var param_61: i32;
    var param_62: vec3<f32>;
    var fogAmount: f32;

    let _e146 = frag_color0In_1;
    param_14 = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e150 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e148.x, _e148.y, _e148.z, _e150);
    let _e155 = frag_color1In_1;
    param_15 = _e155.xyz;
    let _e157 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e159 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e157.x, _e157.y, _e157.z, _e159);
    let _e164 = frag_color2In_1;
    param_16 = _e164.xyz;
    let _e166 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e168 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e166.x, _e166.y, _e166.z, _e168);
    param_17 = 0u;
    let _e173 = frag_tex_coord0_1;
    param_18 = _e173;
    param_19 = 0i;
    let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    let _e175 = frag_color0_;
    color0_ = (_e174 * _e175);
    if override_type_3_7 {
        param_20 = 1u;
        let _e177 = frag_tex_coord1_1;
        param_21 = _e177;
        param_22 = 1i;
        let _e178 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
        let _e179 = frag_color1_;
        color1_ = (_e178 * _e179);
        param_23 = 2u;
        let _e181 = frag_tex_coord2_1;
        param_24 = _e181;
        param_25 = 2i;
        let _e182 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
        let _e183 = frag_color2_;
        color2_ = (_e182 * _e183);
        let _e185 = color0_;
        let _e187 = color1_;
        let _e190 = color2_;
        let _e192 = ((_e185.xyz + _e187.xyz) + _e190.xyz);
        let _e194 = color0_[3u];
        let _e196 = color1_[3u];
        let _e199 = color2_[3u];
        base = vec4<f32>(_e192.x, _e192.y, _e192.z, ((_e194 * _e196) * _e199));
    } else {
        if override_type_3_8 {
            param_26 = 1u;
            let _e205 = frag_tex_coord1_1;
            param_27 = _e205;
            param_28 = 1i;
            let _e206 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
            let _e207 = frag_color1_;
            color1_1 = (_e206 * _e207);
            param_29 = 2u;
            let _e209 = frag_tex_coord2_1;
            param_30 = _e209;
            param_31 = 2i;
            let _e210 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
            let _e211 = frag_color2_;
            color2_1 = (_e210 * _e211);
            let _e214 = color0_[3u];
            let _e215 = color0_;
            color0_ = (_e215 * _e214);
            let _e218 = color1_1[3u];
            let _e219 = color1_1;
            color1_1 = (_e219 * _e218);
            let _e222 = color2_1[3u];
            let _e223 = color2_1;
            color2_1 = (_e223 * _e222);
            let _e225 = color0_;
            let _e227 = color1_1;
            let _e230 = color2_1;
            let _e232 = ((_e225.xyz + _e227.xyz) + _e230.xyz);
            let _e234 = color0_[3u];
            let _e236 = color1_1[3u];
            let _e239 = color2_1[3u];
            base = vec4<f32>(_e232.x, _e232.y, _e232.z, ((_e234 * _e236) * _e239));
        } else {
            if override_type_3_9 {
                param_32 = 1u;
                let _e245 = frag_tex_coord1_1;
                param_33 = _e245;
                param_34 = 1i;
                let _e246 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                let _e247 = frag_color1_;
                color1_2 = (_e246 * _e247);
                param_35 = 2u;
                let _e249 = frag_tex_coord2_1;
                param_36 = _e249;
                param_37 = 2i;
                let _e250 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                let _e251 = frag_color2_;
                color2_2 = (_e250 * _e251);
                let _e254 = color0_[3u];
                let _e256 = color0_;
                color0_ = (_e256 * (1f - _e254));
                let _e259 = color1_2[3u];
                let _e261 = color1_2;
                color1_2 = (_e261 * (1f - _e259));
                let _e264 = color2_2[3u];
                let _e266 = color2_2;
                color2_2 = (_e266 * (1f - _e264));
                let _e268 = color0_;
                let _e270 = color1_2;
                let _e273 = color2_2;
                let _e275 = ((_e268.xyz + _e270.xyz) + _e273.xyz);
                let _e277 = color0_[3u];
                let _e279 = color1_2[3u];
                let _e282 = color2_2[3u];
                base = vec4<f32>(_e275.x, _e275.y, _e275.z, ((_e277 * _e279) * _e282));
            } else {
                if override_type_3_10 {
                    param_38 = 1u;
                    let _e288 = frag_tex_coord1_1;
                    param_39 = _e288;
                    param_40 = 1i;
                    let _e289 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_38), (&param_39), (&param_40));
                    let _e290 = frag_color1_;
                    color1_3 = (_e289 * _e290);
                    param_41 = 2u;
                    let _e292 = frag_tex_coord2_1;
                    param_42 = _e292;
                    param_43 = 2i;
                    let _e293 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_41), (&param_42), (&param_43));
                    let _e294 = frag_color2_;
                    color2_3 = (_e293 * _e294);
                    let _e296 = color0_;
                    let _e297 = color1_3;
                    let _e299 = color1_3[3u];
                    let _e302 = color2_3;
                    let _e304 = color2_3[3u];
                    base = mix(mix(_e296, _e297, vec4(_e299)), _e302, vec4(_e304));
                } else {
                    if override_type_3_11 {
                        param_44 = 1u;
                        let _e307 = frag_tex_coord1_1;
                        param_45 = _e307;
                        param_46 = 1i;
                        let _e308 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_44), (&param_45), (&param_46));
                        let _e309 = frag_color1_;
                        color1_4 = (_e308 * _e309);
                        param_47 = 2u;
                        let _e311 = frag_tex_coord2_1;
                        param_48 = _e311;
                        param_49 = 2i;
                        let _e312 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_47), (&param_48), (&param_49));
                        let _e313 = frag_color2_;
                        color2_4 = (_e312 * _e313);
                        let _e315 = color2_4;
                        let _e316 = color1_4;
                        let _e317 = color0_;
                        let _e319 = color1_4[3u];
                        let _e323 = color2_4[3u];
                        base = mix(_e315, mix(_e316, _e317, vec4(_e319)), vec4(_e323));
                    } else {
                        if override_type_3_12 {
                            param_50 = 1u;
                            let _e326 = frag_tex_coord1_1;
                            param_51 = _e326;
                            param_52 = 1i;
                            let _e327 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_50), (&param_51), (&param_52));
                            let _e328 = frag_color1_;
                            color1_5 = (_e327 * _e328);
                            param_53 = 2u;
                            let _e330 = frag_tex_coord2_1;
                            param_54 = _e330;
                            param_55 = 2i;
                            let _e331 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_53), (&param_54), (&param_55));
                            let _e332 = frag_color2_;
                            color2_5 = (_e331 * _e332);
                            let _e334 = color2_5;
                            let _e336 = color2_5[3u];
                            let _e339 = color1_5;
                            let _e341 = color1_5[3u];
                            let _e345 = color0_;
                            base = (((_e334 + vec4(_e336)) * (_e339 + vec4(_e341))) * _e345);
                        } else {
                            param_56 = 1u;
                            let _e347 = frag_tex_coord1_1;
                            param_57 = _e347;
                            param_58 = 1i;
                            let _e348 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_56), (&param_57), (&param_58));
                            let _e349 = frag_color1_;
                            color1_6 = (_e348 * _e349);
                            param_59 = 2u;
                            let _e351 = frag_tex_coord2_1;
                            param_60 = _e351;
                            param_61 = 2i;
                            let _e352 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_59), (&param_60), (&param_61));
                            let _e353 = frag_color2_;
                            color2_6 = (_e352 * _e353);
                            let _e355 = color0_;
                            let _e357 = color1_6;
                            let _e360 = color2_6;
                            let _e362 = ((_e355.xyz * _e357.xyz) * _e360.xyz);
                            base[0u] = _e362.x;
                            base[1u] = _e362.y;
                            base[2u] = _e362.z;
                            let _e370 = color0_[3u];
                            let _e372 = color1_6[3u];
                            let _e375 = color2_6[3u];
                            base[3u] = ((_e370 * _e372) * _e375);
                        }
                    }
                }
            }
        }
    }
    let _e378 = base;
    param_62 = _e378.xyz;
    let _e380 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_62));
    base[0u] = _e380.x;
    base[1u] = _e380.y;
    base[2u] = _e380.z;
    let _e387 = wired_advanced_fog_enabled_u0028_();
    if _e387 {
        let _e388 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e388;
        let _e389 = base;
        let _e392 = unnamed.advancedFogColorDensity;
        let _e394 = fogAmount;
        let _e396 = mix(_e389.xyz, _e392.xyz, vec3(_e394));
        base[0u] = _e396.x;
        base[1u] = _e396.y;
        base[2u] = _e396.z;
    }
    if override_type_3_13 {
        let _e404 = base[3u];
        if (_e404 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e406 = base;
            let _e408 = base;
            if (dot(_e406.xyz, _e408.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e412 = base;
    out_color = _e412;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    main_1();
    let _e17 = out_color;
    return _e17;
}
