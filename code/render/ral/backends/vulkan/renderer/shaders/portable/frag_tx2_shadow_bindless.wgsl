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

    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e79 + 0.5f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e86 = fogType;
    let _e89 = fogType;
    return (((_e84 > 0.5f) && (_e86 >= 1i)) && (_e89 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e79 = wired_advanced_fog_enabled_u0028_();
    if !(_e79) {
        return 0f;
    }
    let _e82 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e82, 0.000001f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e87 + 0.5f));
    let _e90 = fogType_1;
    if (_e90 == 1i) {
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e94 <= 0f) {
            return 0f;
        }
        let _e96 = viewDepth;
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e96 / _e99), 0f, 1f);
    }
    let _e104 = unnamed.advancedFogColorDensity[3u];
    let _e106 = viewDepth;
    opticalDepth = (max(_e104, 0f) * _e106);
    let _e108 = fogType_1;
    if (_e108 == 2i) {
        let _e110 = opticalDepth;
        return clamp((1f - exp(-(_e110))), 0f, 1f);
    }
    let _e115 = opticalDepth;
    let _e116 = opticalDepth;
    return clamp((1f - exp(-((_e115 * _e116)))), 0f, 1f);
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

    let _e86 = (*c);
    let _e89 = unnamed.cascadeMVP[_e86];
    let _e90 = (*worldPos);
    sc4_ = (_e89 * vec4<f32>(_e90.x, _e90.y, _e90.z, 1f));
    let _e96 = sc4_;
    let _e99 = sc4_[3u];
    sc = (_e96.xyz / vec3(_e99));
    let _e102 = sc;
    let _e106 = ((_e102.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e106.x;
    sc[1u] = _e106.y;
    let _e112 = sc[0u];
    let _e113 = (_e112 < 0f);
    phi_331_ = _e113;
    if !(_e113) {
        let _e116 = sc[0u];
        phi_331_ = (_e116 > 1f);
    }
    let _e119 = phi_331_;
    phi_338_ = _e119;
    if !(_e119) {
        let _e122 = sc[1u];
        phi_338_ = (_e122 < 0f);
    }
    let _e125 = phi_338_;
    phi_345_ = _e125;
    if !(_e125) {
        let _e128 = sc[1u];
        phi_345_ = (_e128 > 1f);
    }
    let _e131 = phi_345_;
    phi_352_ = _e131;
    if !(_e131) {
        let _e134 = sc[2u];
        phi_352_ = (_e134 > 1f);
    }
    let _e137 = phi_352_;
    if _e137 {
        return 1f;
    }
    let _e138 = (*c);
    layer = f32(_e138);
    let _e140 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e140).xy));
    let _e147 = sc[2u];
    currentDepth = (_e147 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e149 = currentDepth;
        let _e150 = sc;
        let _e151 = _e150.xy;
        let _e152 = layer;
        let _e155 = vec3<f32>(_e151.x, _e151.y, _e152);
        let _e161 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e155.x, _e155.y), i32(_e155.z));
        shadow = step(_e149, _e161.x);
    } else {
        if override_type_3_1 {
            let _e164 = currentDepth;
            let _e165 = sc;
            let _e166 = _e165.xy;
            let _e167 = layer;
            let _e170 = vec3<f32>(_e166.x, _e166.y, _e167);
            let _e176 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e170.x, _e170.y), i32(_e170.z));
            let _e179 = shadow;
            shadow = (_e179 + step(_e164, _e176.x));
            let _e181 = currentDepth;
            let _e182 = sc;
            let _e185 = texelSize[0u];
            let _e187 = (_e182.xy + vec2<f32>(_e185, 0f));
            let _e188 = layer;
            let _e191 = vec3<f32>(_e187.x, _e187.y, _e188);
            let _e197 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e191.x, _e191.y), i32(_e191.z));
            let _e200 = shadow;
            shadow = (_e200 + step(_e181, _e197.x));
            let _e202 = currentDepth;
            let _e203 = sc;
            let _e206 = texelSize[0u];
            let _e208 = (_e203.xy - vec2<f32>(_e206, 0f));
            let _e209 = layer;
            let _e212 = vec3<f32>(_e208.x, _e208.y, _e209);
            let _e218 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e212.x, _e212.y), i32(_e212.z));
            let _e221 = shadow;
            shadow = (_e221 + step(_e202, _e218.x));
            let _e223 = currentDepth;
            let _e224 = sc;
            let _e227 = texelSize[1u];
            let _e229 = (_e224.xy + vec2<f32>(0f, _e227));
            let _e230 = layer;
            let _e233 = vec3<f32>(_e229.x, _e229.y, _e230);
            let _e239 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e233.x, _e233.y), i32(_e233.z));
            let _e242 = shadow;
            shadow = (_e242 + step(_e223, _e239.x));
            let _e244 = currentDepth;
            let _e245 = sc;
            let _e248 = texelSize[1u];
            let _e250 = (_e245.xy - vec2<f32>(0f, _e248));
            let _e251 = layer;
            let _e254 = vec3<f32>(_e250.x, _e250.y, _e251);
            let _e260 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e254.x, _e254.y), i32(_e254.z));
            let _e263 = shadow;
            shadow = (_e263 + step(_e244, _e260.x));
            let _e265 = shadow;
            shadow = (_e265 / 5f);
        } else {
            x = -1i;
            loop {
                let _e267 = x;
                if (_e267 <= 1i) {
                    y = -1i;
                    loop {
                        let _e269 = y;
                        if (_e269 <= 1i) {
                            let _e271 = currentDepth;
                            let _e272 = sc;
                            let _e274 = x;
                            let _e276 = y;
                            let _e279 = texelSize;
                            let _e281 = (_e272.xy + (vec2<f32>(f32(_e274), f32(_e276)) * _e279));
                            let _e282 = layer;
                            let _e285 = vec3<f32>(_e281.x, _e281.y, _e282);
                            let _e291 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e285.x, _e285.y), i32(_e285.z));
                            let _e294 = shadow;
                            shadow = (_e294 + step(_e271, _e291.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e296 = y;
                            y = (_e296 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e298 = x;
                    x = (_e298 + 1i);
                }
            }
            let _e300 = shadow;
            shadow = (_e300 / 9f);
        }
    }
    let _e302 = shadow;
    return _e302;
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

    let _e93 = unnamed.cascadeSplits;
    let _e94 = (*viewDepth_1);
    cmp = step(_e93, vec4(_e94));
    let _e98 = cmp[0u];
    let _e100 = cmp[1u];
    let _e103 = cmp[2u];
    let _e106 = cmp[3u];
    cascade = min(i32((((_e98 + _e100) + _e103) + _e106)), 3i);
    let _e110 = cascade;
    (*outCascade) = _e110;
    let _e111 = cascade;
    if (_e111 == 0i) {
        local = 0f;
    } else {
        let _e113 = cascade;
        let _e118 = unnamed.cascadeSplits[max((_e113 - 1i), 0i)];
        local = _e118;
    }
    let _e119 = local;
    prevSplit = _e119;
    let _e120 = cascade;
    let _e123 = unnamed.cascadeSplits[_e120];
    farSplit = _e123;
    let _e124 = farSplit;
    let _e125 = prevSplit;
    blendRange = max((0.1f * (_e124 - _e125)), 1f);
    let _e129 = farSplit;
    let _e130 = (*viewDepth_1);
    let _e132 = blendRange;
    blendT = clamp(((_e129 - _e130) / _e132), 0f, 1f);
    let _e135 = cascade;
    param = _e135;
    let _e136 = (*worldPos_1);
    param_1 = _e136;
    let _e137 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e137;
    let _e138 = cascade;
    param_2 = min((_e138 + 1i), 3i);
    let _e141 = (*worldPos_1);
    param_3 = _e141;
    let _e142 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e142;
    let _e143 = s1_;
    let _e144 = s0_;
    let _e145 = blendT;
    return mix(_e143, _e144, _e145);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e87 = unnamed.packed_indices[1i][3u];
    let _e93 = unnamed.packed_indices[1i][3u];
    let _e98 = (*lm_uv);
    let _e99 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    sun_mask = _e99.x;
    let _e101 = sun_mask;
    if (_e101 > 0.001f) {
        let _e103 = shadowData_1;
        param_4 = _e103.xyz;
        let _e106 = shadowData_1[3u];
        param_5 = _e106;
        let _e107 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e108 = param_6;
        ignoredCascade = _e108;
        shadow_1 = _e107;
        let _e109 = shadow_1;
        let _e110 = sun_mask;
        let _e112 = (*rgb);
        (*rgb) = (_e112 * mix(1f, _e109, _e110));
    }
    let _e114 = (*rgb);
    return _e114;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e83 = (*rgb_1);
        param_7 = _e83;
        let _e84 = frag_tex_coord0_1;
        param_8 = _e84;
        let _e85 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e85;
    }
    if override_type_3_3 {
        let _e86 = (*rgb_1);
        param_9 = _e86;
        let _e87 = frag_tex_coord1_1;
        param_10 = _e87;
        let _e88 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e88;
    }
    if override_type_3_4 {
        let _e89 = (*rgb_1);
        param_11 = _e89;
        let _e90 = frag_tex_coord2_1;
        param_12 = _e90;
        let _e91 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e91;
    }
    let _e92 = (*rgb_1);
    return _e92;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e79 = (*rgb_2);
    let _e82 = unnamed.worldLightParams[0u];
    boosted = (_e79 * _e82);
    let _e85 = boosted[0u];
    let _e87 = boosted[1u];
    let _e89 = boosted[2u];
    peak = max(_e85, max(_e87, _e89));
    let _e92 = peak;
    if (_e92 > 1f) {
        let _e94 = peak;
        let _e95 = boosted;
        boosted = (_e95 / vec3(_e94));
    }
    let _e98 = boosted;
    return _e98;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e80 = (*c_1);
    (*c_1) = max(_e80, vec3<f32>(0f, 0f, 0f));
    let _e82 = (*c_1);
    cutoff = (_e82 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e84 = (*c_1);
    lo = (_e84 / vec3(12.92f));
    let _e87 = (*c_1);
    hi = pow(((_e87 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e92 = hi;
    let _e93 = lo;
    let _e94 = cutoff;
    return mix(_e92, _e93, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e94));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

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
        let _e123 = c_2;
        param_14 = _e123.xyz;
        let _e125 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e125.x;
        c_2[1u] = _e125.y;
        c_2[2u] = _e125.z;
    }
    let _e132 = c_2;
    return _e132;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_15: vec3<f32>;
    var color0_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color1_: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color2_: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var color2_1: vec4<f32>;
    var param_28: u32;
    var param_29: vec2<f32>;
    var param_30: i32;
    var color1_2: vec4<f32>;
    var param_31: u32;
    var param_32: vec2<f32>;
    var param_33: i32;
    var color2_2: vec4<f32>;
    var param_34: u32;
    var param_35: vec2<f32>;
    var param_36: i32;
    var param_37: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

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
    let _e123 = frag_color0_;
    color0_ = (_e122 * _e123);
    if override_type_3_5 {
        param_19 = 1u;
        let _e125 = frag_tex_coord1_1;
        param_20 = _e125;
        param_21 = 1i;
        let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
        color1_ = _e126;
        param_22 = 2u;
        let _e127 = frag_tex_coord2_1;
        param_23 = _e127;
        param_24 = 2i;
        let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
        color2_ = _e128;
        let _e129 = color0_;
        let _e131 = color1_;
        let _e134 = color2_;
        let _e136 = ((_e129.xyz + _e131.xyz) + _e134.xyz);
        let _e138 = color0_[3u];
        let _e140 = color1_[3u];
        let _e143 = color2_[3u];
        base = vec4<f32>(_e136.x, _e136.y, _e136.z, ((_e138 * _e140) * _e143));
    } else {
        if override_type_3_6 {
            param_25 = 1u;
            let _e149 = frag_tex_coord1_1;
            param_26 = _e149;
            param_27 = 1i;
            let _e150 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
            let _e151 = frag_color0_;
            color1_1 = (_e150 * _e151);
            param_28 = 2u;
            let _e153 = frag_tex_coord2_1;
            param_29 = _e153;
            param_30 = 2i;
            let _e154 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
            let _e155 = frag_color0_;
            color2_1 = (_e154 * _e155);
            let _e157 = color0_;
            let _e159 = color1_1;
            let _e162 = color2_1;
            let _e164 = ((_e157.xyz + _e159.xyz) + _e162.xyz);
            let _e166 = color0_[3u];
            let _e168 = color1_1[3u];
            let _e171 = color2_1[3u];
            base = vec4<f32>(_e164.x, _e164.y, _e164.z, ((_e166 * _e168) * _e171));
        } else {
            param_31 = 1u;
            let _e177 = frag_tex_coord1_1;
            param_32 = _e177;
            param_33 = 1i;
            let _e178 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
            color1_2 = _e178;
            param_34 = 2u;
            let _e179 = frag_tex_coord2_1;
            param_35 = _e179;
            param_36 = 2i;
            let _e180 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
            color2_2 = _e180;
            let _e181 = color0_;
            let _e183 = color1_2;
            let _e186 = color2_2;
            let _e188 = ((_e181.xyz * _e183.xyz) * _e186.xyz);
            base[0u] = _e188.x;
            base[1u] = _e188.y;
            base[2u] = _e188.z;
            let _e196 = color0_[3u];
            let _e198 = color1_2[3u];
            let _e201 = color2_2[3u];
            base[3u] = ((_e196 * _e198) * _e201);
        }
    }
    let _e204 = base;
    param_37 = _e204.xyz;
    let _e206 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_37));
    base[0u] = _e206.x;
    base[1u] = _e206.y;
    base[2u] = _e206.z;
    if override_type_3_7 {
        let _e215 = unnamed.worldLightParams[1u];
        wetness = clamp(_e215, 0f, 1f);
        let _e219 = unnamed.worldLightParams[2u];
        frost = clamp(_e219, 0f, 1f);
        let _e221 = base;
        luminance = dot(_e221.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e224 = wetness;
        let _e226 = base;
        let _e228 = (_e226.xyz * mix(1f, 0.82f, _e224));
        base[0u] = _e228.x;
        base[1u] = _e228.y;
        base[2u] = _e228.z;
        let _e235 = base;
        let _e237 = luminance;
        let _e239 = luminance;
        let _e241 = luminance;
        let _e243 = frost;
        let _e246 = mix(_e235.xyz, vec3<f32>((_e237 * 0.88f), (_e239 * 0.94f), _e241), vec3((_e243 * 0.55f)));
        base[0u] = _e246.x;
        base[1u] = _e246.y;
        base[2u] = _e246.z;
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
