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
override override_type_3_7: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_8: bool = (discard_mode == 1i);
override override_type_3_9: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e81 + 0.5f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e88 = fogType;
    let _e91 = fogType;
    return (((_e86 > 0.5f) && (_e88 >= 1i)) && (_e91 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e81 = wired_advanced_fog_enabled_u0028_();
    if !(_e81) {
        return 0f;
    }
    let _e84 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e84, 0.000001f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e89 + 0.5f));
    let _e92 = fogType_1;
    if (_e92 == 1i) {
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e96 <= 0f) {
            return 0f;
        }
        let _e98 = viewDepth;
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e98 / _e101), 0f, 1f);
    }
    let _e106 = unnamed.advancedFogColorDensity[3u];
    let _e108 = viewDepth;
    opticalDepth = (max(_e106, 0f) * _e108);
    let _e110 = fogType_1;
    if (_e110 == 2i) {
        let _e112 = opticalDepth;
        return clamp((1f - exp(-(_e112))), 0f, 1f);
    }
    let _e117 = opticalDepth;
    let _e118 = opticalDepth;
    return clamp((1f - exp(-((_e117 * _e118)))), 0f, 1f);
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

    let _e88 = (*c);
    let _e91 = unnamed.cascadeMVP[_e88];
    let _e92 = (*worldPos);
    sc4_ = (_e91 * vec4<f32>(_e92.x, _e92.y, _e92.z, 1f));
    let _e98 = sc4_;
    let _e101 = sc4_[3u];
    sc = (_e98.xyz / vec3(_e101));
    let _e104 = sc;
    let _e108 = ((_e104.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e108.x;
    sc[1u] = _e108.y;
    let _e114 = sc[0u];
    let _e115 = (_e114 < 0f);
    phi_331_ = _e115;
    if !(_e115) {
        let _e118 = sc[0u];
        phi_331_ = (_e118 > 1f);
    }
    let _e121 = phi_331_;
    phi_338_ = _e121;
    if !(_e121) {
        let _e124 = sc[1u];
        phi_338_ = (_e124 < 0f);
    }
    let _e127 = phi_338_;
    phi_345_ = _e127;
    if !(_e127) {
        let _e130 = sc[1u];
        phi_345_ = (_e130 > 1f);
    }
    let _e133 = phi_345_;
    phi_352_ = _e133;
    if !(_e133) {
        let _e136 = sc[2u];
        phi_352_ = (_e136 > 1f);
    }
    let _e139 = phi_352_;
    if _e139 {
        return 1f;
    }
    let _e140 = (*c);
    layer = f32(_e140);
    let _e142 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e142).xy));
    let _e149 = sc[2u];
    currentDepth = (_e149 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e151 = currentDepth;
        let _e152 = sc;
        let _e153 = _e152.xy;
        let _e154 = layer;
        let _e157 = vec3<f32>(_e153.x, _e153.y, _e154);
        let _e163 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e157.x, _e157.y), i32(_e157.z));
        shadow = step(_e151, _e163.x);
    } else {
        if override_type_3_1 {
            let _e166 = currentDepth;
            let _e167 = sc;
            let _e168 = _e167.xy;
            let _e169 = layer;
            let _e172 = vec3<f32>(_e168.x, _e168.y, _e169);
            let _e178 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e172.x, _e172.y), i32(_e172.z));
            let _e181 = shadow;
            shadow = (_e181 + step(_e166, _e178.x));
            let _e183 = currentDepth;
            let _e184 = sc;
            let _e187 = texelSize[0u];
            let _e189 = (_e184.xy + vec2<f32>(_e187, 0f));
            let _e190 = layer;
            let _e193 = vec3<f32>(_e189.x, _e189.y, _e190);
            let _e199 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e193.x, _e193.y), i32(_e193.z));
            let _e202 = shadow;
            shadow = (_e202 + step(_e183, _e199.x));
            let _e204 = currentDepth;
            let _e205 = sc;
            let _e208 = texelSize[0u];
            let _e210 = (_e205.xy - vec2<f32>(_e208, 0f));
            let _e211 = layer;
            let _e214 = vec3<f32>(_e210.x, _e210.y, _e211);
            let _e220 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e214.x, _e214.y), i32(_e214.z));
            let _e223 = shadow;
            shadow = (_e223 + step(_e204, _e220.x));
            let _e225 = currentDepth;
            let _e226 = sc;
            let _e229 = texelSize[1u];
            let _e231 = (_e226.xy + vec2<f32>(0f, _e229));
            let _e232 = layer;
            let _e235 = vec3<f32>(_e231.x, _e231.y, _e232);
            let _e241 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e235.x, _e235.y), i32(_e235.z));
            let _e244 = shadow;
            shadow = (_e244 + step(_e225, _e241.x));
            let _e246 = currentDepth;
            let _e247 = sc;
            let _e250 = texelSize[1u];
            let _e252 = (_e247.xy - vec2<f32>(0f, _e250));
            let _e253 = layer;
            let _e256 = vec3<f32>(_e252.x, _e252.y, _e253);
            let _e262 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e256.x, _e256.y), i32(_e256.z));
            let _e265 = shadow;
            shadow = (_e265 + step(_e246, _e262.x));
            let _e267 = shadow;
            shadow = (_e267 / 5f);
        } else {
            x = -1i;
            loop {
                let _e269 = x;
                if (_e269 <= 1i) {
                    y = -1i;
                    loop {
                        let _e271 = y;
                        if (_e271 <= 1i) {
                            let _e273 = currentDepth;
                            let _e274 = sc;
                            let _e276 = x;
                            let _e278 = y;
                            let _e281 = texelSize;
                            let _e283 = (_e274.xy + (vec2<f32>(f32(_e276), f32(_e278)) * _e281));
                            let _e284 = layer;
                            let _e287 = vec3<f32>(_e283.x, _e283.y, _e284);
                            let _e293 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e287.x, _e287.y), i32(_e287.z));
                            let _e296 = shadow;
                            shadow = (_e296 + step(_e273, _e293.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e298 = y;
                            y = (_e298 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e300 = x;
                    x = (_e300 + 1i);
                }
            }
            let _e302 = shadow;
            shadow = (_e302 / 9f);
        }
    }
    let _e304 = shadow;
    return _e304;
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

    let _e95 = unnamed.cascadeSplits;
    let _e96 = (*viewDepth_1);
    cmp = step(_e95, vec4(_e96));
    let _e100 = cmp[0u];
    let _e102 = cmp[1u];
    let _e105 = cmp[2u];
    let _e108 = cmp[3u];
    cascade = min(i32((((_e100 + _e102) + _e105) + _e108)), 3i);
    let _e112 = cascade;
    (*outCascade) = _e112;
    let _e113 = cascade;
    if (_e113 == 0i) {
        local = 0f;
    } else {
        let _e115 = cascade;
        let _e120 = unnamed.cascadeSplits[max((_e115 - 1i), 0i)];
        local = _e120;
    }
    let _e121 = local;
    prevSplit = _e121;
    let _e122 = cascade;
    let _e125 = unnamed.cascadeSplits[_e122];
    farSplit = _e125;
    let _e126 = farSplit;
    let _e127 = prevSplit;
    blendRange = max((0.1f * (_e126 - _e127)), 1f);
    let _e131 = farSplit;
    let _e132 = (*viewDepth_1);
    let _e134 = blendRange;
    blendT = clamp(((_e131 - _e132) / _e134), 0f, 1f);
    let _e137 = cascade;
    param = _e137;
    let _e138 = (*worldPos_1);
    param_1 = _e138;
    let _e139 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e139;
    let _e140 = cascade;
    param_2 = min((_e140 + 1i), 3i);
    let _e143 = (*worldPos_1);
    param_3 = _e143;
    let _e144 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e144;
    let _e145 = s1_;
    let _e146 = s0_;
    let _e147 = blendT;
    return mix(_e145, _e146, _e147);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e89 = unnamed.packed_indices[1i][3u];
    let _e95 = unnamed.packed_indices[1i][3u];
    let _e100 = (*lm_uv);
    let _e101 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    sun_mask = _e101.x;
    let _e103 = sun_mask;
    if (_e103 > 0.001f) {
        let _e105 = shadowData_1;
        param_4 = _e105.xyz;
        let _e108 = shadowData_1[3u];
        param_5 = _e108;
        let _e109 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e110 = param_6;
        ignoredCascade = _e110;
        shadow_1 = _e109;
        let _e111 = shadow_1;
        let _e112 = sun_mask;
        let _e114 = (*rgb);
        (*rgb) = (_e114 * mix(1f, _e111, _e112));
    }
    let _e116 = (*rgb);
    return _e116;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e85 = (*rgb_1);
        param_7 = _e85;
        let _e86 = frag_tex_coord0_1;
        param_8 = _e86;
        let _e87 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e87;
    }
    if override_type_3_3 {
        let _e88 = (*rgb_1);
        param_9 = _e88;
        let _e89 = frag_tex_coord1_1;
        param_10 = _e89;
        let _e90 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e90;
    }
    if override_type_3_4 {
        let _e91 = (*rgb_1);
        param_11 = _e91;
        let _e92 = frag_tex_coord2_1;
        param_12 = _e92;
        let _e93 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e93;
    }
    let _e94 = (*rgb_1);
    return _e94;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e81 = (*rgb_2);
    let _e84 = unnamed.worldLightParams[0u];
    boosted = (_e81 * _e84);
    let _e87 = boosted[0u];
    let _e89 = boosted[1u];
    let _e91 = boosted[2u];
    peak = max(_e87, max(_e89, _e91));
    let _e94 = peak;
    if (_e94 > 1f) {
        let _e96 = peak;
        let _e97 = boosted;
        boosted = (_e97 / vec3(_e96));
    }
    let _e100 = boosted;
    return _e100;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e82 = (*c_1);
    (*c_1) = max(_e82, vec3<f32>(0f, 0f, 0f));
    let _e84 = (*c_1);
    cutoff = (_e84 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e86 = (*c_1);
    lo = (_e86 / vec3(12.92f));
    let _e89 = (*c_1);
    hi = pow(((_e89 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e94 = hi;
    let _e95 = lo;
    let _e96 = cutoff;
    return mix(_e94, _e95, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e96));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e94 = (*role);
    let _e96 = (*role);
    let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
    let _e106 = (*uv);
    let _e107 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e101 >> bitcast<u32>(12i)) & 255u)], _e106);
    c_2 = _e107;
    let _e108 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e108))) == 0i) {
        let _e113 = c_2;
        param_13 = _e113.xyz;
        let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e115.x;
        c_2[1u] = _e115.y;
        c_2[2u] = _e115.z;
    }
    let _e122 = (*slot);
    if (lightmap_slot == (_e122 + 1i)) {
        let _e125 = c_2;
        param_14 = _e125.xyz;
        let _e127 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e127.x;
        c_2[1u] = _e127.y;
        c_2[2u] = _e127.z;
    }
    let _e134 = c_2;
    return _e134;
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

    let _e105 = frag_color0In_1;
    param_15 = _e105.xyz;
    let _e107 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e109 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e107.x, _e107.y, _e107.z, _e109);
    param_16 = 0u;
    let _e114 = frag_tex_coord0_1;
    param_17 = _e114;
    param_18 = 0i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor0_ = _e115;
    param_19 = 1u;
    let _e116 = frag_tex_coord1_1;
    param_20 = _e116;
    param_21 = 1i;
    let _e117 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor1_ = _e117;
    param_22 = 2u;
    let _e118 = frag_tex_coord2_1;
    param_23 = _e118;
    param_24 = 2i;
    let _e119 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor2_ = _e119;
    let _e120 = sampledColor0_;
    let _e121 = frag_color0_;
    color0_ = (_e120 * _e121);
    if override_type_3_5 {
        let _e123 = sampledColor1_;
        color1_ = _e123;
        let _e124 = sampledColor2_;
        color2_ = _e124;
        let _e125 = color0_;
        let _e127 = color1_;
        let _e130 = color2_;
        let _e132 = ((_e125.xyz + _e127.xyz) + _e130.xyz);
        let _e134 = color0_[3u];
        let _e136 = color1_[3u];
        let _e139 = color2_[3u];
        base = vec4<f32>(_e132.x, _e132.y, _e132.z, ((_e134 * _e136) * _e139));
    } else {
        if override_type_3_6 {
            let _e145 = sampledColor1_;
            let _e146 = frag_color0_;
            color1_1 = (_e145 * _e146);
            let _e148 = sampledColor2_;
            let _e149 = frag_color0_;
            color2_1 = (_e148 * _e149);
            let _e151 = color0_;
            let _e153 = color1_1;
            let _e156 = color2_1;
            let _e158 = ((_e151.xyz + _e153.xyz) + _e156.xyz);
            let _e160 = color0_[3u];
            let _e162 = color1_1[3u];
            let _e165 = color2_1[3u];
            base = vec4<f32>(_e158.x, _e158.y, _e158.z, ((_e160 * _e162) * _e165));
        } else {
            let _e171 = sampledColor1_;
            color1_2 = _e171;
            let _e172 = sampledColor2_;
            color2_2 = _e172;
            let _e173 = color0_;
            let _e175 = color1_2;
            let _e178 = color2_2;
            let _e180 = ((_e173.xyz * _e175.xyz) * _e178.xyz);
            base[0u] = _e180.x;
            base[1u] = _e180.y;
            base[2u] = _e180.z;
            let _e188 = color0_[3u];
            let _e190 = color1_2[3u];
            let _e193 = color2_2[3u];
            base[3u] = ((_e188 * _e190) * _e193);
        }
    }
    let _e196 = base;
    param_25 = _e196.xyz;
    let _e198 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e198.x;
    base[1u] = _e198.y;
    base[2u] = _e198.z;
    if override_type_3_7 {
        let _e207 = unnamed.worldLightParams[1u];
        wetness = clamp(_e207, 0f, 1f);
        let _e211 = unnamed.worldLightParams[2u];
        frost = clamp(_e211, 0f, 1f);
        let _e213 = base;
        luminance = dot(_e213.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e216 = wetness;
        let _e218 = base;
        let _e220 = (_e218.xyz * mix(1f, 0.82f, _e216));
        base[0u] = _e220.x;
        base[1u] = _e220.y;
        base[2u] = _e220.z;
        let _e227 = base;
        let _e229 = luminance;
        let _e231 = luminance;
        let _e233 = luminance;
        let _e235 = frost;
        let _e238 = mix(_e227.xyz, vec3<f32>((_e229 * 0.88f), (_e231 * 0.94f), _e233), vec3((_e235 * 0.55f)));
        base[0u] = _e238.x;
        base[1u] = _e238.y;
        base[2u] = _e238.z;
    }
    let _e247 = unnamed.materialFactors[0u];
    if (_e247 >= 0f) {
        let _e250 = unnamed.materialBaseColor;
        let _e251 = base;
        base = (_e251 * _e250);
    }
    let _e253 = color0_;
    let _e256 = unnamed.emissionRadiance;
    let _e259 = base;
    let _e261 = (_e259.xyz + (_e253.xyz * _e256.xyz));
    base[0u] = _e261.x;
    base[1u] = _e261.y;
    base[2u] = _e261.z;
    let _e268 = wired_advanced_fog_enabled_u0028_();
    if _e268 {
        let _e269 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e269;
        let _e270 = base;
        let _e273 = unnamed.advancedFogColorDensity;
        let _e275 = fogAmount;
        let _e277 = mix(_e270.xyz, _e273.xyz, vec3(_e275));
        base[0u] = _e277.x;
        base[1u] = _e277.y;
        base[2u] = _e277.z;
    }
    if override_type_3_8 {
        let _e285 = base[3u];
        if (_e285 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e287 = base;
            let _e289 = base;
            if (dot(_e287.xyz, _e289.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e293 = base;
    out_color = _e293;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e13 = out_color;
    return _e13;
}
