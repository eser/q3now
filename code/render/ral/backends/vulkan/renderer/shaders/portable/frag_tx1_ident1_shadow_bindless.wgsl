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
override override_type_3_6: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_7: bool = (discard_mode == 1i);
override override_type_3_8: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e76 + 0.5f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e83 = fogType;
    let _e86 = fogType;
    return (((_e81 > 0.5f) && (_e83 >= 1i)) && (_e86 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e76 = wired_advanced_fog_enabled_u0028_();
    if !(_e76) {
        return 0f;
    }
    let _e79 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e79, 0.000001f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e84 + 0.5f));
    let _e87 = fogType_1;
    if (_e87 == 1i) {
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e91 <= 0f) {
            return 0f;
        }
        let _e93 = viewDepth;
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e93 / _e96), 0f, 1f);
    }
    let _e101 = unnamed.advancedFogColorDensity[3u];
    let _e103 = viewDepth;
    opticalDepth = (max(_e101, 0f) * _e103);
    let _e105 = fogType_1;
    if (_e105 == 2i) {
        let _e107 = opticalDepth;
        return clamp((1f - exp(-(_e107))), 0f, 1f);
    }
    let _e112 = opticalDepth;
    let _e113 = opticalDepth;
    return clamp((1f - exp(-((_e112 * _e113)))), 0f, 1f);
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
    var phi_331_: bool;
    var phi_338_: bool;
    var phi_345_: bool;
    var phi_352_: bool;

    let _e83 = (*c);
    let _e86 = unnamed.cascadeMVP[_e83];
    let _e87 = (*worldPos);
    sc4_ = (_e86 * vec4<f32>(_e87.x, _e87.y, _e87.z, 1f));
    let _e93 = sc4_;
    let _e96 = sc4_[3u];
    sc = (_e93.xyz / vec3(_e96));
    let _e99 = sc;
    let _e103 = ((_e99.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e103.x;
    sc[1u] = _e103.y;
    let _e109 = sc[0u];
    let _e110 = (_e109 < 0f);
    phi_331_ = _e110;
    if !(_e110) {
        let _e113 = sc[0u];
        phi_331_ = (_e113 > 1f);
    }
    let _e116 = phi_331_;
    phi_338_ = _e116;
    if !(_e116) {
        let _e119 = sc[1u];
        phi_338_ = (_e119 < 0f);
    }
    let _e122 = phi_338_;
    phi_345_ = _e122;
    if !(_e122) {
        let _e125 = sc[1u];
        phi_345_ = (_e125 > 1f);
    }
    let _e128 = phi_345_;
    phi_352_ = _e128;
    if !(_e128) {
        let _e131 = sc[2u];
        phi_352_ = (_e131 > 1f);
    }
    let _e134 = phi_352_;
    if _e134 {
        return 1f;
    }
    let _e135 = (*c);
    layer = f32(_e135);
    let _e137 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e137).xy));
    let _e144 = sc[2u];
    currentDepth = (_e144 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e146 = currentDepth;
        let _e147 = sc;
        let _e148 = _e147.xy;
        let _e149 = layer;
        let _e152 = vec3<f32>(_e148.x, _e148.y, _e149);
        let _e158 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e152.x, _e152.y), i32(_e152.z));
        shadow = step(_e146, _e158.x);
    } else {
        if override_type_3_1 {
            let _e161 = currentDepth;
            let _e162 = sc;
            let _e163 = _e162.xy;
            let _e164 = layer;
            let _e167 = vec3<f32>(_e163.x, _e163.y, _e164);
            let _e173 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e167.x, _e167.y), i32(_e167.z));
            let _e176 = shadow;
            shadow = (_e176 + step(_e161, _e173.x));
            let _e178 = currentDepth;
            let _e179 = sc;
            let _e182 = texelSize[0u];
            let _e184 = (_e179.xy + vec2<f32>(_e182, 0f));
            let _e185 = layer;
            let _e188 = vec3<f32>(_e184.x, _e184.y, _e185);
            let _e194 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e188.x, _e188.y), i32(_e188.z));
            let _e197 = shadow;
            shadow = (_e197 + step(_e178, _e194.x));
            let _e199 = currentDepth;
            let _e200 = sc;
            let _e203 = texelSize[0u];
            let _e205 = (_e200.xy - vec2<f32>(_e203, 0f));
            let _e206 = layer;
            let _e209 = vec3<f32>(_e205.x, _e205.y, _e206);
            let _e215 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e209.x, _e209.y), i32(_e209.z));
            let _e218 = shadow;
            shadow = (_e218 + step(_e199, _e215.x));
            let _e220 = currentDepth;
            let _e221 = sc;
            let _e224 = texelSize[1u];
            let _e226 = (_e221.xy + vec2<f32>(0f, _e224));
            let _e227 = layer;
            let _e230 = vec3<f32>(_e226.x, _e226.y, _e227);
            let _e236 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e230.x, _e230.y), i32(_e230.z));
            let _e239 = shadow;
            shadow = (_e239 + step(_e220, _e236.x));
            let _e241 = currentDepth;
            let _e242 = sc;
            let _e245 = texelSize[1u];
            let _e247 = (_e242.xy - vec2<f32>(0f, _e245));
            let _e248 = layer;
            let _e251 = vec3<f32>(_e247.x, _e247.y, _e248);
            let _e257 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e251.x, _e251.y), i32(_e251.z));
            let _e260 = shadow;
            shadow = (_e260 + step(_e241, _e257.x));
            let _e262 = shadow;
            shadow = (_e262 / 5f);
        } else {
            x = -1i;
            loop {
                let _e264 = x;
                if (_e264 <= 1i) {
                    y = -1i;
                    loop {
                        let _e266 = y;
                        if (_e266 <= 1i) {
                            let _e268 = currentDepth;
                            let _e269 = sc;
                            let _e271 = x;
                            let _e273 = y;
                            let _e276 = texelSize;
                            let _e278 = (_e269.xy + (vec2<f32>(f32(_e271), f32(_e273)) * _e276));
                            let _e279 = layer;
                            let _e282 = vec3<f32>(_e278.x, _e278.y, _e279);
                            let _e288 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e282.x, _e282.y), i32(_e282.z));
                            let _e291 = shadow;
                            shadow = (_e291 + step(_e268, _e288.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e293 = y;
                            y = (_e293 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e295 = x;
                    x = (_e295 + 1i);
                }
            }
            let _e297 = shadow;
            shadow = (_e297 / 9f);
        }
    }
    let _e299 = shadow;
    return _e299;
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

    let _e90 = unnamed.cascadeSplits;
    let _e91 = (*viewDepth_1);
    cmp = step(_e90, vec4(_e91));
    let _e95 = cmp[0u];
    let _e97 = cmp[1u];
    let _e100 = cmp[2u];
    let _e103 = cmp[3u];
    cascade = min(i32((((_e95 + _e97) + _e100) + _e103)), 3i);
    let _e107 = cascade;
    (*outCascade) = _e107;
    let _e108 = cascade;
    if (_e108 == 0i) {
        local = 0f;
    } else {
        let _e110 = cascade;
        let _e115 = unnamed.cascadeSplits[max((_e110 - 1i), 0i)];
        local = _e115;
    }
    let _e116 = local;
    prevSplit = _e116;
    let _e117 = cascade;
    let _e120 = unnamed.cascadeSplits[_e117];
    farSplit = _e120;
    let _e121 = farSplit;
    let _e122 = prevSplit;
    blendRange = max((0.1f * (_e121 - _e122)), 1f);
    let _e126 = farSplit;
    let _e127 = (*viewDepth_1);
    let _e129 = blendRange;
    blendT = clamp(((_e126 - _e127) / _e129), 0f, 1f);
    let _e132 = cascade;
    param = _e132;
    let _e133 = (*worldPos_1);
    param_1 = _e133;
    let _e134 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e134;
    let _e135 = cascade;
    param_2 = min((_e135 + 1i), 3i);
    let _e138 = (*worldPos_1);
    param_3 = _e138;
    let _e139 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e139;
    let _e140 = s1_;
    let _e141 = s0_;
    let _e142 = blendT;
    return mix(_e140, _e141, _e142);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e84 = unnamed.packed_indices[1i][3u];
    let _e90 = unnamed.packed_indices[1i][3u];
    let _e95 = (*lm_uv);
    let _e96 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    sun_mask = _e96.x;
    let _e98 = sun_mask;
    if (_e98 > 0.001f) {
        let _e100 = shadowData_1;
        param_4 = _e100.xyz;
        let _e103 = shadowData_1[3u];
        param_5 = _e103;
        let _e104 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e105 = param_6;
        ignoredCascade = _e105;
        shadow_1 = _e104;
        let _e106 = shadow_1;
        let _e107 = sun_mask;
        let _e109 = (*rgb);
        (*rgb) = (_e109 * mix(1f, _e106, _e107));
    }
    let _e111 = (*rgb);
    return _e111;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e78 = (*rgb_1);
        param_7 = _e78;
        let _e79 = frag_tex_coord0_1;
        param_8 = _e79;
        let _e80 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e80;
    }
    if override_type_3_3 {
        let _e81 = (*rgb_1);
        param_9 = _e81;
        let _e82 = frag_tex_coord1_1;
        param_10 = _e82;
        let _e83 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e83;
    }
    let _e84 = (*rgb_1);
    return _e84;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e76 = (*rgb_2);
    let _e79 = unnamed.worldLightParams[0u];
    boosted = (_e76 * _e79);
    let _e82 = boosted[0u];
    let _e84 = boosted[1u];
    let _e86 = boosted[2u];
    peak = max(_e82, max(_e84, _e86));
    let _e89 = peak;
    if (_e89 > 1f) {
        let _e91 = peak;
        let _e92 = boosted;
        boosted = (_e92 / vec3(_e91));
    }
    let _e95 = boosted;
    return _e95;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e77 = (*c_1);
    (*c_1) = max(_e77, vec3<f32>(0f, 0f, 0f));
    let _e79 = (*c_1);
    cutoff = (_e79 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e81 = (*c_1);
    lo = (_e81 / vec3(12.92f));
    let _e84 = (*c_1);
    hi = pow(((_e84 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e89 = hi;
    let _e90 = lo;
    let _e91 = cutoff;
    return mix(_e89, _e90, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e91));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e101 = (*uv);
    let _e102 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    c_2 = _e102;
    let _e103 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e103))) == 0i) {
        let _e108 = c_2;
        param_11 = _e108.xyz;
        let _e110 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e110.x;
        c_2[1u] = _e110.y;
        c_2[2u] = _e110.z;
    }
    let _e117 = (*slot);
    if (lightmap_slot == (_e117 + 1i)) {
        let _e120 = c_2;
        param_12 = _e120.xyz;
        let _e122 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e122.x;
        c_2[1u] = _e122.y;
        c_2[2u] = _e122.z;
    }
    let _e129 = c_2;
    return _e129;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var param_25: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    param_13 = 0u;
    let _e95 = frag_tex_coord0_1;
    param_14 = _e95;
    param_15 = 0i;
    let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    color0_ = _e96;
    if override_type_3_4 {
        param_16 = 1u;
        let _e97 = frag_tex_coord1_1;
        param_17 = _e97;
        param_18 = 1i;
        let _e98 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
        color1_ = _e98;
        let _e99 = color0_;
        let _e101 = color1_;
        let _e103 = (_e99.xyz + _e101.xyz);
        let _e105 = color0_[3u];
        let _e107 = color1_[3u];
        base = vec4<f32>(_e103.x, _e103.y, _e103.z, (_e105 * _e107));
    } else {
        if override_type_3_5 {
            param_19 = 1u;
            let _e113 = frag_tex_coord1_1;
            param_20 = _e113;
            param_21 = 1i;
            let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            color1_1 = _e114;
            let _e115 = color0_;
            let _e117 = color1_1;
            let _e119 = (_e115.xyz + _e117.xyz);
            let _e121 = color0_[3u];
            let _e123 = color1_1[3u];
            base = vec4<f32>(_e119.x, _e119.y, _e119.z, (_e121 * _e123));
        } else {
            param_22 = 1u;
            let _e129 = frag_tex_coord1_1;
            param_23 = _e129;
            param_24 = 1i;
            let _e130 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            color1_2 = _e130;
            let _e131 = color0_;
            let _e133 = color1_2;
            let _e135 = (_e131.xyz * _e133.xyz);
            base[0u] = _e135.x;
            base[1u] = _e135.y;
            base[2u] = _e135.z;
            let _e143 = color0_[3u];
            let _e145 = color1_2[3u];
            base[3u] = (_e143 * _e145);
        }
    }
    let _e148 = base;
    param_25 = _e148.xyz;
    let _e150 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e150.x;
    base[1u] = _e150.y;
    base[2u] = _e150.z;
    if override_type_3_6 {
        let _e159 = unnamed.worldLightParams[1u];
        wetness = clamp(_e159, 0f, 1f);
        let _e163 = unnamed.worldLightParams[2u];
        frost = clamp(_e163, 0f, 1f);
        let _e165 = base;
        luminance = dot(_e165.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e168 = wetness;
        let _e170 = base;
        let _e172 = (_e170.xyz * mix(1f, 0.82f, _e168));
        base[0u] = _e172.x;
        base[1u] = _e172.y;
        base[2u] = _e172.z;
        let _e179 = base;
        let _e181 = luminance;
        let _e183 = luminance;
        let _e185 = luminance;
        let _e187 = frost;
        let _e190 = mix(_e179.xyz, vec3<f32>((_e181 * 0.88f), (_e183 * 0.94f), _e185), vec3((_e187 * 0.55f)));
        base[0u] = _e190.x;
        base[1u] = _e190.y;
        base[2u] = _e190.z;
    }
    let _e197 = color0_;
    let _e200 = unnamed.emissionRadiance;
    let _e203 = base;
    let _e205 = (_e203.xyz + (_e197.xyz * _e200.xyz));
    base[0u] = _e205.x;
    base[1u] = _e205.y;
    base[2u] = _e205.z;
    let _e212 = wired_advanced_fog_enabled_u0028_();
    if _e212 {
        let _e213 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e213;
        let _e214 = base;
        let _e217 = unnamed.advancedFogColorDensity;
        let _e219 = fogAmount;
        let _e221 = mix(_e214.xyz, _e217.xyz, vec3(_e219));
        base[0u] = _e221.x;
        base[1u] = _e221.y;
        base[2u] = _e221.z;
    }
    if override_type_3_7 {
        let _e229 = base[3u];
        if (_e229 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e231 = base;
            let _e233 = base;
            if (dot(_e231.xyz, _e233.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e237 = base;
    out_color = _e237;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
