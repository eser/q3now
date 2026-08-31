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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_4: bool = (acff == 1i);
override override_type_3_5: bool = (acff == 2i);
override override_type_3_6: bool = (acff == 3i);
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_10: bool = (discard_mode == 1i);
override override_type_3_11: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e82 + 0.5f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e89 = fogType;
    let _e92 = fogType;
    return (((_e87 > 0.5f) && (_e89 >= 1i)) && (_e92 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e82 = wired_advanced_fog_enabled_u0028_();
    if !(_e82) {
        return 0f;
    }
    let _e85 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e85, 0.000001f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e90 + 0.5f));
    let _e93 = fogType_1;
    if (_e93 == 1i) {
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e97 <= 0f) {
            return 0f;
        }
        let _e99 = viewDepth;
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e99 / _e102), 0f, 1f);
    }
    let _e107 = unnamed.advancedFogColorDensity[3u];
    let _e109 = viewDepth;
    opticalDepth = (max(_e107, 0f) * _e109);
    let _e111 = fogType_1;
    if (_e111 == 2i) {
        let _e113 = opticalDepth;
        return clamp((1f - exp(-(_e113))), 0f, 1f);
    }
    let _e118 = opticalDepth;
    let _e119 = opticalDepth;
    return clamp((1f - exp(-((_e118 * _e119)))), 0f, 1f);
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

    let _e89 = (*c);
    let _e92 = unnamed.cascadeMVP[_e89];
    let _e93 = (*worldPos);
    sc4_ = (_e92 * vec4<f32>(_e93.x, _e93.y, _e93.z, 1f));
    let _e99 = sc4_;
    let _e102 = sc4_[3u];
    sc = (_e99.xyz / vec3(_e102));
    let _e105 = sc;
    let _e109 = ((_e105.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e109.x;
    sc[1u] = _e109.y;
    let _e115 = sc[0u];
    let _e116 = (_e115 < 0f);
    phi_331_ = _e116;
    if !(_e116) {
        let _e119 = sc[0u];
        phi_331_ = (_e119 > 1f);
    }
    let _e122 = phi_331_;
    phi_338_ = _e122;
    if !(_e122) {
        let _e125 = sc[1u];
        phi_338_ = (_e125 < 0f);
    }
    let _e128 = phi_338_;
    phi_345_ = _e128;
    if !(_e128) {
        let _e131 = sc[1u];
        phi_345_ = (_e131 > 1f);
    }
    let _e134 = phi_345_;
    phi_352_ = _e134;
    if !(_e134) {
        let _e137 = sc[2u];
        phi_352_ = (_e137 > 1f);
    }
    let _e140 = phi_352_;
    if _e140 {
        return 1f;
    }
    let _e141 = (*c);
    layer = f32(_e141);
    let _e143 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e143).xy));
    let _e150 = sc[2u];
    currentDepth = (_e150 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e152 = currentDepth;
        let _e153 = sc;
        let _e154 = _e153.xy;
        let _e155 = layer;
        let _e158 = vec3<f32>(_e154.x, _e154.y, _e155);
        let _e164 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e158.x, _e158.y), i32(_e158.z));
        shadow = step(_e152, _e164.x);
    } else {
        if override_type_3_1 {
            let _e167 = currentDepth;
            let _e168 = sc;
            let _e169 = _e168.xy;
            let _e170 = layer;
            let _e173 = vec3<f32>(_e169.x, _e169.y, _e170);
            let _e179 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e173.x, _e173.y), i32(_e173.z));
            let _e182 = shadow;
            shadow = (_e182 + step(_e167, _e179.x));
            let _e184 = currentDepth;
            let _e185 = sc;
            let _e188 = texelSize[0u];
            let _e190 = (_e185.xy + vec2<f32>(_e188, 0f));
            let _e191 = layer;
            let _e194 = vec3<f32>(_e190.x, _e190.y, _e191);
            let _e200 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e194.x, _e194.y), i32(_e194.z));
            let _e203 = shadow;
            shadow = (_e203 + step(_e184, _e200.x));
            let _e205 = currentDepth;
            let _e206 = sc;
            let _e209 = texelSize[0u];
            let _e211 = (_e206.xy - vec2<f32>(_e209, 0f));
            let _e212 = layer;
            let _e215 = vec3<f32>(_e211.x, _e211.y, _e212);
            let _e221 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e215.x, _e215.y), i32(_e215.z));
            let _e224 = shadow;
            shadow = (_e224 + step(_e205, _e221.x));
            let _e226 = currentDepth;
            let _e227 = sc;
            let _e230 = texelSize[1u];
            let _e232 = (_e227.xy + vec2<f32>(0f, _e230));
            let _e233 = layer;
            let _e236 = vec3<f32>(_e232.x, _e232.y, _e233);
            let _e242 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e236.x, _e236.y), i32(_e236.z));
            let _e245 = shadow;
            shadow = (_e245 + step(_e226, _e242.x));
            let _e247 = currentDepth;
            let _e248 = sc;
            let _e251 = texelSize[1u];
            let _e253 = (_e248.xy - vec2<f32>(0f, _e251));
            let _e254 = layer;
            let _e257 = vec3<f32>(_e253.x, _e253.y, _e254);
            let _e263 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e257.x, _e257.y), i32(_e257.z));
            let _e266 = shadow;
            shadow = (_e266 + step(_e247, _e263.x));
            let _e268 = shadow;
            shadow = (_e268 / 5f);
        } else {
            x = -1i;
            loop {
                let _e270 = x;
                if (_e270 <= 1i) {
                    y = -1i;
                    loop {
                        let _e272 = y;
                        if (_e272 <= 1i) {
                            let _e274 = currentDepth;
                            let _e275 = sc;
                            let _e277 = x;
                            let _e279 = y;
                            let _e282 = texelSize;
                            let _e284 = (_e275.xy + (vec2<f32>(f32(_e277), f32(_e279)) * _e282));
                            let _e285 = layer;
                            let _e288 = vec3<f32>(_e284.x, _e284.y, _e285);
                            let _e294 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e288.x, _e288.y), i32(_e288.z));
                            let _e297 = shadow;
                            shadow = (_e297 + step(_e274, _e294.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e299 = y;
                            y = (_e299 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e301 = x;
                    x = (_e301 + 1i);
                }
            }
            let _e303 = shadow;
            shadow = (_e303 / 9f);
        }
    }
    let _e305 = shadow;
    return _e305;
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

    let _e96 = unnamed.cascadeSplits;
    let _e97 = (*viewDepth_1);
    cmp = step(_e96, vec4(_e97));
    let _e101 = cmp[0u];
    let _e103 = cmp[1u];
    let _e106 = cmp[2u];
    let _e109 = cmp[3u];
    cascade = min(i32((((_e101 + _e103) + _e106) + _e109)), 3i);
    let _e113 = cascade;
    (*outCascade) = _e113;
    let _e114 = cascade;
    if (_e114 == 0i) {
        local = 0f;
    } else {
        let _e116 = cascade;
        let _e121 = unnamed.cascadeSplits[max((_e116 - 1i), 0i)];
        local = _e121;
    }
    let _e122 = local;
    prevSplit = _e122;
    let _e123 = cascade;
    let _e126 = unnamed.cascadeSplits[_e123];
    farSplit = _e126;
    let _e127 = farSplit;
    let _e128 = prevSplit;
    blendRange = max((0.1f * (_e127 - _e128)), 1f);
    let _e132 = farSplit;
    let _e133 = (*viewDepth_1);
    let _e135 = blendRange;
    blendT = clamp(((_e132 - _e133) / _e135), 0f, 1f);
    let _e138 = cascade;
    param = _e138;
    let _e139 = (*worldPos_1);
    param_1 = _e139;
    let _e140 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e140;
    let _e141 = cascade;
    param_2 = min((_e141 + 1i), 3i);
    let _e144 = (*worldPos_1);
    param_3 = _e144;
    let _e145 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e145;
    let _e146 = s1_;
    let _e147 = s0_;
    let _e148 = blendT;
    return mix(_e146, _e147, _e148);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e90 = unnamed.packed_indices[1i][3u];
    let _e96 = unnamed.packed_indices[1i][3u];
    let _e101 = (*lm_uv);
    let _e102 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    sun_mask = _e102.x;
    let _e104 = sun_mask;
    if (_e104 > 0.001f) {
        let _e106 = shadowData_1;
        param_4 = _e106.xyz;
        let _e109 = shadowData_1[3u];
        param_5 = _e109;
        let _e110 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e111 = param_6;
        ignoredCascade = _e111;
        shadow_1 = _e110;
        let _e112 = shadow_1;
        let _e113 = sun_mask;
        let _e115 = (*rgb);
        (*rgb) = (_e115 * mix(1f, _e112, _e113));
    }
    let _e117 = (*rgb);
    return _e117;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e82 = (*rgb_1);
        param_7 = _e82;
        let _e83 = frag_tex_coord0_1;
        param_8 = _e83;
        let _e84 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e84;
    }
    let _e85 = (*rgb_1);
    return _e85;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e82 = (*rgb_2);
    let _e85 = unnamed.worldLightParams[0u];
    boosted = (_e82 * _e85);
    let _e88 = boosted[0u];
    let _e90 = boosted[1u];
    let _e92 = boosted[2u];
    peak = max(_e88, max(_e90, _e92));
    let _e95 = peak;
    if (_e95 > 1f) {
        let _e97 = peak;
        let _e98 = boosted;
        boosted = (_e98 / vec3(_e97));
    }
    let _e101 = boosted;
    return _e101;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e83 = (*c_1);
    (*c_1) = max(_e83, vec3<f32>(0f, 0f, 0f));
    let _e85 = (*c_1);
    cutoff = (_e85 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e87 = (*c_1);
    lo = (_e87 / vec3(12.92f));
    let _e90 = (*c_1);
    hi = pow(((_e90 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e95 = hi;
    let _e96 = lo;
    let _e97 = cutoff;
    return mix(_e95, _e96, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e97));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e95 = (*role);
    let _e97 = (*role);
    let _e102 = unnamed.packed_indices[(_e95 / 4u)][(_e97 % 4u)];
    let _e107 = (*uv);
    let _e108 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e102 >> bitcast<u32>(12i)) & 255u)], _e107);
    c_2 = _e108;
    let _e109 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e109))) == 0i) {
        let _e114 = c_2;
        param_9 = _e114.xyz;
        let _e116 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e116.x;
        c_2[1u] = _e116.y;
        c_2[2u] = _e116.z;
    }
    let _e123 = (*slot);
    if (lightmap_slot == (_e123 + 1i)) {
        let _e126 = c_2;
        param_10 = _e126.xyz;
        let _e128 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e128.x;
        c_2[1u] = _e128.y;
        c_2[2u] = _e128.z;
    }
    let _e135 = c_2;
    return _e135;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var param_14: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e94 = unnamed.packed_indices[0i][3u];
    let _e100 = unnamed.packed_indices[0i][3u];
    let _e105 = fog_tex_coord_1;
    let _e106 = textureSample(wired_bindless_images[(_e94 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    fog = _e106;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_11 = 0u;
    let _e111 = frag_tex_coord0_1;
    param_12 = _e111;
    param_13 = 0i;
    let _e112 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    let _e113 = frag_color;
    color0_ = (_e112 * _e113);
    let _e115 = color0_;
    base = _e115;
    let _e116 = color0_;
    base = _e116;
    let _e117 = base;
    param_14 = _e117.xyz;
    let _e119 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e119.x;
    base[1u] = _e119.y;
    base[2u] = _e119.z;
    if override_type_3_3 {
        let _e128 = unnamed.worldLightParams[1u];
        wetness = clamp(_e128, 0f, 1f);
        let _e132 = unnamed.worldLightParams[2u];
        frost = clamp(_e132, 0f, 1f);
        let _e134 = base;
        luminance = dot(_e134.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e137 = wetness;
        let _e139 = base;
        let _e141 = (_e139.xyz * mix(1f, 0.82f, _e137));
        base[0u] = _e141.x;
        base[1u] = _e141.y;
        base[2u] = _e141.z;
        let _e148 = base;
        let _e150 = luminance;
        let _e152 = luminance;
        let _e154 = luminance;
        let _e156 = frost;
        let _e159 = mix(_e148.xyz, vec3<f32>((_e150 * 0.88f), (_e152 * 0.94f), _e154), vec3((_e156 * 0.55f)));
        base[0u] = _e159.x;
        base[1u] = _e159.y;
        base[2u] = _e159.z;
    }
    let _e166 = color0_;
    let _e169 = unnamed.emissionRadiance;
    let _e172 = base;
    let _e174 = (_e172.xyz + (_e166.xyz * _e169.xyz));
    base[0u] = _e174.x;
    base[1u] = _e174.y;
    base[2u] = _e174.z;
    let _e181 = wired_advanced_fog_enabled_u0028_();
    if _e181 {
        let _e182 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e182;
        if override_type_3_4 {
            let _e183 = fogAmount;
            let _e185 = base;
            let _e187 = (_e185.xyz * (1f - _e183));
            base[0u] = _e187.x;
            base[1u] = _e187.y;
            base[2u] = _e187.z;
        } else {
            if override_type_3_5 {
                let _e194 = fogAmount;
                let _e196 = base;
                base = (_e196 * (1f - _e194));
            } else {
                if override_type_3_6 {
                    let _e198 = fogAmount;
                    let _e201 = base[3u];
                    base[3u] = (_e201 * (1f - _e198));
                } else {
                    let _e204 = base;
                    let _e207 = unnamed.advancedFogColorDensity;
                    let _e209 = fogAmount;
                    let _e211 = mix(_e204.xyz, _e207.xyz, vec3(_e209));
                    base[0u] = _e211.x;
                    base[1u] = _e211.y;
                    base[2u] = _e211.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e218 = base;
            let _e221 = fog[3u];
            let _e223 = (_e218.xyz * (1f - _e221));
            base[0u] = _e223.x;
            base[1u] = _e223.y;
            base[2u] = _e223.z;
        } else {
            if override_type_3_8 {
                let _e230 = base;
                let _e232 = fog[3u];
                base = (_e230 * (1f - _e232));
            } else {
                if override_type_3_9 {
                    let _e236 = base[3u];
                    let _e238 = fog[3u];
                    base[3u] = (_e236 * (1f - _e238));
                } else {
                    let _e242 = base;
                    let _e243 = fog;
                    let _e245 = unnamed.fogColor;
                    let _e248 = fog[3u];
                    base = mix(_e242, (_e243 * _e245), vec4(_e248));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e252 = base[3u];
        if (_e252 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e254 = base;
            let _e256 = base;
            if (dot(_e254.xyz, _e256.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e260 = base;
    out_color = _e260;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e9 = out_color;
    return _e9;
}
