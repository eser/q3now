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

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_4_: bool = (shadow_pcf <= 1i);
override override_type_4_1: bool = (shadow_pcf <= 5i);
override override_type_4_2: bool = (lightmap_slot == 1i);
override override_type_4_3: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_4: bool = (acff == 1i);
override override_type_4_5: bool = (acff == 2i);
override override_type_4_6: bool = (acff == 3i);
override override_type_4_7: bool = (acff == 1i);
override override_type_4_8: bool = (acff == 2i);
override override_type_4_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_10: bool = (discard_mode == 1i);
override override_type_4_11: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e88 + 0.5f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e95 = fogType;
    let _e98 = fogType;
    return (((_e93 > 0.5f) && (_e95 >= 1i)) && (_e98 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e88 = wired_advanced_fog_enabled_u0028_();
    if !(_e88) {
        return 0f;
    }
    let _e91 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e91, 0.000001f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e96 + 0.5f));
    let _e99 = fogType_1;
    if (_e99 == 1i) {
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e103 <= 0f) {
            return 0f;
        }
        let _e105 = viewDepth;
        let _e108 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e105 / _e108), 0f, 1f);
    }
    let _e113 = unnamed.advancedFogColorDensity[3u];
    let _e115 = viewDepth;
    opticalDepth = (max(_e113, 0f) * _e115);
    let _e117 = fogType_1;
    if (_e117 == 2i) {
        let _e119 = opticalDepth;
        return clamp((1f - exp(-(_e119))), 0f, 1f);
    }
    let _e124 = opticalDepth;
    let _e125 = opticalDepth;
    return clamp((1f - exp(-((_e124 * _e125)))), 0f, 1f);
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
    var phi_340_: bool;
    var phi_347_: bool;
    var phi_354_: bool;
    var phi_361_: bool;

    let _e95 = (*c);
    let _e98 = unnamed.cascadeMVP[_e95];
    let _e99 = (*worldPos);
    sc4_ = (_e98 * vec4<f32>(_e99.x, _e99.y, _e99.z, 1f));
    let _e105 = sc4_;
    let _e108 = sc4_[3u];
    sc = (_e105.xyz / vec3(_e108));
    let _e111 = sc;
    let _e115 = ((_e111.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e115.x;
    sc[1u] = _e115.y;
    let _e121 = sc[0u];
    let _e122 = (_e121 < 0f);
    phi_340_ = _e122;
    if !(_e122) {
        let _e125 = sc[0u];
        phi_340_ = (_e125 > 1f);
    }
    let _e128 = phi_340_;
    phi_347_ = _e128;
    if !(_e128) {
        let _e131 = sc[1u];
        phi_347_ = (_e131 < 0f);
    }
    let _e134 = phi_347_;
    phi_354_ = _e134;
    if !(_e134) {
        let _e137 = sc[1u];
        phi_354_ = (_e137 > 1f);
    }
    let _e140 = phi_354_;
    phi_361_ = _e140;
    if !(_e140) {
        let _e143 = sc[2u];
        phi_361_ = (_e143 > 1f);
    }
    let _e146 = phi_361_;
    if _e146 {
        return 1f;
    }
    let _e147 = (*c);
    layer = f32(_e147);
    let _e149 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e149).xy));
    let _e156 = sc[2u];
    currentDepth = (_e156 - shadow_bias);
    shadow = 0f;
    if override_type_4_ {
        let _e158 = currentDepth;
        let _e159 = sc;
        let _e160 = _e159.xy;
        let _e161 = layer;
        let _e164 = vec3<f32>(_e160.x, _e160.y, _e161);
        let _e170 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e164.x, _e164.y), i32(_e164.z));
        shadow = step(_e158, _e170.x);
    } else {
        if override_type_4_1 {
            let _e173 = currentDepth;
            let _e174 = sc;
            let _e175 = _e174.xy;
            let _e176 = layer;
            let _e179 = vec3<f32>(_e175.x, _e175.y, _e176);
            let _e185 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e179.x, _e179.y), i32(_e179.z));
            let _e188 = shadow;
            shadow = (_e188 + step(_e173, _e185.x));
            let _e190 = currentDepth;
            let _e191 = sc;
            let _e194 = texelSize[0u];
            let _e196 = (_e191.xy + vec2<f32>(_e194, 0f));
            let _e197 = layer;
            let _e200 = vec3<f32>(_e196.x, _e196.y, _e197);
            let _e206 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e200.x, _e200.y), i32(_e200.z));
            let _e209 = shadow;
            shadow = (_e209 + step(_e190, _e206.x));
            let _e211 = currentDepth;
            let _e212 = sc;
            let _e215 = texelSize[0u];
            let _e217 = (_e212.xy - vec2<f32>(_e215, 0f));
            let _e218 = layer;
            let _e221 = vec3<f32>(_e217.x, _e217.y, _e218);
            let _e227 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e221.x, _e221.y), i32(_e221.z));
            let _e230 = shadow;
            shadow = (_e230 + step(_e211, _e227.x));
            let _e232 = currentDepth;
            let _e233 = sc;
            let _e236 = texelSize[1u];
            let _e238 = (_e233.xy + vec2<f32>(0f, _e236));
            let _e239 = layer;
            let _e242 = vec3<f32>(_e238.x, _e238.y, _e239);
            let _e248 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e242.x, _e242.y), i32(_e242.z));
            let _e251 = shadow;
            shadow = (_e251 + step(_e232, _e248.x));
            let _e253 = currentDepth;
            let _e254 = sc;
            let _e257 = texelSize[1u];
            let _e259 = (_e254.xy - vec2<f32>(0f, _e257));
            let _e260 = layer;
            let _e263 = vec3<f32>(_e259.x, _e259.y, _e260);
            let _e269 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e263.x, _e263.y), i32(_e263.z));
            let _e272 = shadow;
            shadow = (_e272 + step(_e253, _e269.x));
            let _e274 = shadow;
            shadow = (_e274 / 5f);
        } else {
            x = -1i;
            loop {
                let _e276 = x;
                if (_e276 <= 1i) {
                    y = -1i;
                    loop {
                        let _e278 = y;
                        if (_e278 <= 1i) {
                            let _e280 = currentDepth;
                            let _e281 = sc;
                            let _e283 = x;
                            let _e285 = y;
                            let _e288 = texelSize;
                            let _e290 = (_e281.xy + (vec2<f32>(f32(_e283), f32(_e285)) * _e288));
                            let _e291 = layer;
                            let _e294 = vec3<f32>(_e290.x, _e290.y, _e291);
                            let _e300 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e294.x, _e294.y), i32(_e294.z));
                            let _e303 = shadow;
                            shadow = (_e303 + step(_e280, _e300.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e305 = y;
                            y = (_e305 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e307 = x;
                    x = (_e307 + 1i);
                }
            }
            let _e309 = shadow;
            shadow = (_e309 / 9f);
        }
    }
    let _e311 = shadow;
    return _e311;
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

    let _e102 = unnamed.cascadeSplits;
    let _e103 = (*viewDepth_1);
    cmp = step(_e102, vec4(_e103));
    let _e107 = cmp[0u];
    let _e109 = cmp[1u];
    let _e112 = cmp[2u];
    let _e115 = cmp[3u];
    cascade = min(i32((((_e107 + _e109) + _e112) + _e115)), 3i);
    let _e119 = cascade;
    (*outCascade) = _e119;
    let _e120 = cascade;
    if (_e120 == 0i) {
        local = 0f;
    } else {
        let _e122 = cascade;
        let _e127 = unnamed.cascadeSplits[max((_e122 - 1i), 0i)];
        local = _e127;
    }
    let _e128 = local;
    prevSplit = _e128;
    let _e129 = cascade;
    let _e132 = unnamed.cascadeSplits[_e129];
    farSplit = _e132;
    let _e133 = farSplit;
    let _e134 = prevSplit;
    blendRange = max((0.1f * (_e133 - _e134)), 1f);
    let _e138 = farSplit;
    let _e139 = (*viewDepth_1);
    let _e141 = blendRange;
    blendT = clamp(((_e138 - _e139) / _e141), 0f, 1f);
    let _e144 = cascade;
    param = _e144;
    let _e145 = (*worldPos_1);
    param_1 = _e145;
    let _e146 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e146;
    let _e147 = cascade;
    param_2 = min((_e147 + 1i), 3i);
    let _e150 = (*worldPos_1);
    param_3 = _e150;
    let _e151 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e151;
    let _e152 = s1_;
    let _e153 = s0_;
    let _e154 = blendT;
    return mix(_e152, _e153, _e154);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e96 = unnamed.packed_indices[1i][3u];
    let _e102 = unnamed.packed_indices[1i][3u];
    let _e107 = (*lm_uv);
    let _e108 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e102 >> bitcast<u32>(12i)) & 255u)], _e107);
    sun_mask = _e108.x;
    let _e110 = sun_mask;
    if (_e110 > 0.001f) {
        let _e112 = shadowData_1;
        param_4 = _e112.xyz;
        let _e115 = shadowData_1[3u];
        param_5 = _e115;
        let _e116 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e117 = param_6;
        ignoredCascade = _e117;
        shadow_1 = _e116;
        let _e118 = shadow_1;
        let _e119 = sun_mask;
        let _e121 = (*rgb);
        (*rgb) = (_e121 * mix(1f, _e118, _e119));
    }
    let _e123 = (*rgb);
    return _e123;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_4_2 {
        let _e88 = (*rgb_1);
        param_7 = _e88;
        let _e89 = frag_tex_coord0_1;
        param_8 = _e89;
        let _e90 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e90;
    }
    let _e91 = (*rgb_1);
    return _e91;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e88 = (*rgb_2);
    let _e91 = unnamed.worldLightParams[0u];
    boosted = (_e88 * _e91);
    let _e94 = boosted[0u];
    let _e96 = boosted[1u];
    let _e98 = boosted[2u];
    peak = max(_e94, max(_e96, _e98));
    let _e101 = peak;
    if (_e101 > 1f) {
        let _e103 = peak;
        let _e104 = boosted;
        boosted = (_e104 / vec3(_e103));
    }
    let _e107 = boosted;
    return _e107;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e89 = (*c_1);
    (*c_1) = max(_e89, vec3<f32>(0f, 0f, 0f));
    let _e91 = (*c_1);
    cutoff = (_e91 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e93 = (*c_1);
    lo = (_e93 / vec3(12.92f));
    let _e96 = (*c_1);
    hi = pow(((_e96 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e101 = hi;
    let _e102 = lo;
    let _e103 = cutoff;
    return mix(_e101, _e102, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e103));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

    let _e91 = (*role);
    let _e93 = (*role);
    let _e98 = unnamed.packed_indices[(_e91 / 4u)][(_e93 % 4u)];
    let _e101 = (*role);
    let _e103 = (*role);
    let _e108 = unnamed.packed_indices[(_e101 / 4u)][(_e103 % 4u)];
    let _e113 = (*uv);
    let _e114 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e108 >> bitcast<u32>(12i)) & 255u)], _e113);
    c_2 = _e114;
    let _e115 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e115))) == 0i) {
        let _e120 = c_2;
        param_9 = _e120.xyz;
        let _e122 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e122.x;
        c_2[1u] = _e122.y;
        c_2[2u] = _e122.z;
    }
    let _e129 = (*slot);
    if (lightmap_slot == (_e129 + 1i)) {
        let _e132 = c_2;
        param_10 = _e132.xyz;
        let _e134 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e134.x;
        c_2[1u] = _e134.y;
        c_2[2u] = _e134.z;
    }
    let _e141 = c_2;
    return _e141;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_14: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_15: f32;

    let _e101 = unnamed.packed_indices[0i][3u];
    let _e107 = unnamed.packed_indices[0i][3u];
    let _e112 = fog_tex_coord_1;
    let _e113 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    fog = _e113;
    param_11 = 0u;
    let _e114 = frag_tex_coord0_1;
    param_12 = _e114;
    param_13 = 0i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor0_ = _e115;
    let _e116 = sampledColor0_;
    color0_ = _e116;
    let _e117 = color0_;
    base = _e117;
    let _e118 = color0_;
    base = _e118;
    let _e119 = base;
    param_14 = _e119.xyz;
    let _e121 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e121.x;
    base[1u] = _e121.y;
    base[2u] = _e121.z;
    if override_type_4_3 {
        let _e130 = unnamed.worldLightParams[1u];
        wetness = clamp(_e130, 0f, 1f);
        let _e134 = unnamed.worldLightParams[2u];
        frost = clamp(_e134, 0f, 1f);
        let _e136 = base;
        luminance = dot(_e136.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e139 = wetness;
        let _e141 = base;
        let _e143 = (_e141.xyz * mix(1f, 0.82f, _e139));
        base[0u] = _e143.x;
        base[1u] = _e143.y;
        base[2u] = _e143.z;
        let _e150 = base;
        let _e152 = luminance;
        let _e154 = luminance;
        let _e156 = luminance;
        let _e158 = frost;
        let _e161 = mix(_e150.xyz, vec3<f32>((_e152 * 0.88f), (_e154 * 0.94f), _e156), vec3((_e158 * 0.55f)));
        base[0u] = _e161.x;
        base[1u] = _e161.y;
        base[2u] = _e161.z;
    }
    let _e170 = unnamed.materialFactors[0u];
    if (_e170 >= 0f) {
        let _e173 = unnamed.materialBaseColor;
        let _e174 = base;
        base = (_e174 * _e173);
    }
    let _e176 = color0_;
    let _e179 = unnamed.emissionRadiance;
    let _e182 = base;
    let _e184 = (_e182.xyz + (_e176.xyz * _e179.xyz));
    base[0u] = _e184.x;
    base[1u] = _e184.y;
    base[2u] = _e184.z;
    let _e191 = wired_advanced_fog_enabled_u0028_();
    if _e191 {
        let _e192 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e192;
        if override_type_4_4 {
            let _e193 = fogAmount;
            let _e195 = base;
            let _e197 = (_e195.xyz * (1f - _e193));
            base[0u] = _e197.x;
            base[1u] = _e197.y;
            base[2u] = _e197.z;
        } else {
            if override_type_4_5 {
                let _e204 = fogAmount;
                let _e206 = base;
                base = (_e206 * (1f - _e204));
            } else {
                if override_type_4_6 {
                    let _e208 = fogAmount;
                    let _e211 = base[3u];
                    base[3u] = (_e211 * (1f - _e208));
                } else {
                    let _e214 = base;
                    let _e217 = unnamed.advancedFogColorDensity;
                    let _e219 = fogAmount;
                    let _e221 = mix(_e214.xyz, _e217.xyz, vec3(_e219));
                    base[0u] = _e221.x;
                    base[1u] = _e221.y;
                    base[2u] = _e221.z;
                }
            }
        }
    } else {
        if override_type_4_7 {
            let _e228 = base;
            let _e231 = fog[3u];
            let _e233 = (_e228.xyz * (1f - _e231));
            base[0u] = _e233.x;
            base[1u] = _e233.y;
            base[2u] = _e233.z;
        } else {
            if override_type_4_8 {
                let _e240 = base;
                let _e242 = fog[3u];
                base = (_e240 * (1f - _e242));
            } else {
                if override_type_4_9 {
                    let _e246 = base[3u];
                    let _e248 = fog[3u];
                    base[3u] = (_e246 * (1f - _e248));
                } else {
                    let _e252 = base;
                    let _e253 = fog;
                    let _e255 = unnamed.fogColor;
                    let _e258 = fog[3u];
                    base = mix(_e252, (_e253 * _e255), vec4(_e258));
                }
            }
        }
    }
    if override_type_4_10 {
        let _e262 = base[3u];
        if (_e262 == 0f) {
            discard;
        }
    } else {
        if override_type_4_11 {
            let _e264 = base;
            let _e266 = base;
            if (dot(_e264.xyz, _e266.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e270 = base;
    out_color = _e270;
    param_15 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_15));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
