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
    var phi_331_: bool;
    var phi_338_: bool;
    var phi_345_: bool;
    var phi_352_: bool;

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
    phi_331_ = _e119;
    if !(_e119) {
        let _e122 = sc[0u];
        phi_331_ = (_e122 > 1f);
    }
    let _e125 = phi_331_;
    phi_338_ = _e125;
    if !(_e125) {
        let _e128 = sc[1u];
        phi_338_ = (_e128 < 0f);
    }
    let _e131 = phi_338_;
    phi_345_ = _e131;
    if !(_e131) {
        let _e134 = sc[1u];
        phi_345_ = (_e134 > 1f);
    }
    let _e137 = phi_345_;
    phi_352_ = _e137;
    if !(_e137) {
        let _e140 = sc[2u];
        phi_352_ = (_e140 > 1f);
    }
    let _e143 = phi_352_;
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

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e85 = (*rgb_2);
    let _e88 = unnamed.worldLightParams[0u];
    boosted = (_e85 * _e88);
    let _e91 = boosted[0u];
    let _e93 = boosted[1u];
    let _e95 = boosted[2u];
    peak = max(_e91, max(_e93, _e95));
    let _e98 = peak;
    if (_e98 > 1f) {
        let _e100 = peak;
        let _e101 = boosted;
        boosted = (_e101 / vec3(_e100));
    }
    let _e104 = boosted;
    return _e104;
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
    var param_12: vec3<f32>;

    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e98 = (*role);
    let _e100 = (*role);
    let _e105 = unnamed.packed_indices[(_e98 / 4u)][(_e100 % 4u)];
    let _e110 = (*uv);
    let _e111 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    c_2 = _e111;
    let _e112 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e112))) == 0i) {
        let _e117 = c_2;
        param_11 = _e117.xyz;
        let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e119.x;
        c_2[1u] = _e119.y;
        c_2[2u] = _e119.z;
    }
    let _e126 = (*slot);
    if (lightmap_slot == (_e126 + 1i)) {
        let _e129 = c_2;
        param_12 = _e129.xyz;
        let _e131 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e131.x;
        c_2[1u] = _e131.y;
        c_2[2u] = _e131.z;
    }
    let _e138 = c_2;
    return _e138;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e108 = unnamed.packed_indices[0i][3u];
    let _e114 = unnamed.packed_indices[0i][3u];
    let _e119 = fog_tex_coord_1;
    let _e120 = textureSample(wired_bindless_images[(_e108 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    fog = _e120;
    param_13 = 0u;
    let _e121 = frag_tex_coord0_1;
    param_14 = _e121;
    param_15 = 0i;
    let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    color0_ = _e122;
    if override_type_3_4 {
        param_16 = 1u;
        let _e123 = frag_tex_coord1_1;
        param_17 = _e123;
        param_18 = 1i;
        let _e124 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
        color1_ = _e124;
        let _e125 = color0_;
        let _e127 = color1_;
        let _e129 = (_e125.xyz + _e127.xyz);
        let _e131 = color0_[3u];
        let _e133 = color1_[3u];
        base = vec4<f32>(_e129.x, _e129.y, _e129.z, (_e131 * _e133));
    } else {
        if override_type_3_5 {
            param_19 = 1u;
            let _e139 = frag_tex_coord1_1;
            param_20 = _e139;
            param_21 = 1i;
            let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            color1_1 = _e140;
            let _e141 = color0_;
            let _e143 = color1_1;
            let _e145 = (_e141.xyz + _e143.xyz);
            let _e147 = color0_[3u];
            let _e149 = color1_1[3u];
            base = vec4<f32>(_e145.x, _e145.y, _e145.z, (_e147 * _e149));
        } else {
            param_22 = 1u;
            let _e155 = frag_tex_coord1_1;
            param_23 = _e155;
            param_24 = 1i;
            let _e156 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            color1_2 = _e156;
            let _e157 = color0_;
            let _e159 = color1_2;
            let _e161 = (_e157.xyz * _e159.xyz);
            base[0u] = _e161.x;
            base[1u] = _e161.y;
            base[2u] = _e161.z;
            let _e169 = color0_[3u];
            let _e171 = color1_2[3u];
            base[3u] = (_e169 * _e171);
        }
    }
    let _e174 = base;
    param_25 = _e174.xyz;
    let _e176 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e176.x;
    base[1u] = _e176.y;
    base[2u] = _e176.z;
    if override_type_3_6 {
        let _e185 = unnamed.worldLightParams[1u];
        wetness = clamp(_e185, 0f, 1f);
        let _e189 = unnamed.worldLightParams[2u];
        frost = clamp(_e189, 0f, 1f);
        let _e191 = base;
        luminance = dot(_e191.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e194 = wetness;
        let _e196 = base;
        let _e198 = (_e196.xyz * mix(1f, 0.82f, _e194));
        base[0u] = _e198.x;
        base[1u] = _e198.y;
        base[2u] = _e198.z;
        let _e205 = base;
        let _e207 = luminance;
        let _e209 = luminance;
        let _e211 = luminance;
        let _e213 = frost;
        let _e216 = mix(_e205.xyz, vec3<f32>((_e207 * 0.88f), (_e209 * 0.94f), _e211), vec3((_e213 * 0.55f)));
        base[0u] = _e216.x;
        base[1u] = _e216.y;
        base[2u] = _e216.z;
    }
    let _e223 = color0_;
    let _e226 = unnamed.emissionRadiance;
    let _e229 = base;
    let _e231 = (_e229.xyz + (_e223.xyz * _e226.xyz));
    base[0u] = _e231.x;
    base[1u] = _e231.y;
    base[2u] = _e231.z;
    let _e238 = wired_advanced_fog_enabled_u0028_();
    if _e238 {
        let _e239 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e239;
        if override_type_3_7 {
            let _e240 = fogAmount;
            let _e242 = base;
            let _e244 = (_e242.xyz * (1f - _e240));
            base[0u] = _e244.x;
            base[1u] = _e244.y;
            base[2u] = _e244.z;
        } else {
            if override_type_3_8 {
                let _e251 = fogAmount;
                let _e253 = base;
                base = (_e253 * (1f - _e251));
            } else {
                if override_type_3_9 {
                    let _e255 = fogAmount;
                    let _e258 = base[3u];
                    base[3u] = (_e258 * (1f - _e255));
                } else {
                    let _e261 = base;
                    let _e264 = unnamed.advancedFogColorDensity;
                    let _e266 = fogAmount;
                    let _e268 = mix(_e261.xyz, _e264.xyz, vec3(_e266));
                    base[0u] = _e268.x;
                    base[1u] = _e268.y;
                    base[2u] = _e268.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e275 = base;
            let _e278 = fog[3u];
            let _e280 = (_e275.xyz * (1f - _e278));
            base[0u] = _e280.x;
            base[1u] = _e280.y;
            base[2u] = _e280.z;
        } else {
            if override_type_3_11 {
                let _e287 = base;
                let _e289 = fog[3u];
                base = (_e287 * (1f - _e289));
            } else {
                if override_type_3_12 {
                    let _e293 = base[3u];
                    let _e295 = fog[3u];
                    base[3u] = (_e293 * (1f - _e295));
                } else {
                    let _e299 = base;
                    let _e300 = fog;
                    let _e302 = unnamed.fogColor;
                    let _e305 = fog[3u];
                    base = mix(_e299, (_e300 * _e302), vec4(_e305));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e309 = base[3u];
        if (_e309 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e311 = base;
            let _e313 = base;
            if (dot(_e311.xyz, _e313.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e317 = base;
    out_color = _e317;
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
