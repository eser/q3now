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
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_4: bool = (discard_mode == 1i);
override override_type_3_5: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e82 = (*value);
    let _e83 = (*value);
    let _e85 = all((_e82 == _e83));
    phi_97_ = _e85;
    if _e85 {
        let _e86 = (*value);
        phi_97_ = all((abs(_e86) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e91 = phi_97_;
    return _e91;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e82 = (*value_1);
    let _e83 = (*value_1);
    let _e85 = all((_e82 == _e83));
    phi_82_ = _e85;
    if _e85 {
        let _e86 = (*value_1);
        phi_82_ = all((abs(_e86) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e91 = phi_82_;
    return _e91;
}

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_120_: bool;
    var phi_129_: bool;
    var phi_139_: bool;
    var phi_146_: bool;
    var phi_175_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e92 = temporalOutcome_1;
    let _e93 = (_e92 != 1u);
    phi_120_ = _e93;
    if !(_e93) {
        let _e95 = temporalCurrentClip_1;
        param = _e95;
        let _e96 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e96);
    }
    let _e99 = phi_120_;
    phi_129_ = _e99;
    if !(_e99) {
        let _e101 = temporalPreviousClip_1;
        param_1 = _e101;
        let _e102 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e102);
    }
    let _e105 = phi_129_;
    phi_139_ = _e105;
    if !(_e105) {
        let _e108 = temporalCurrentClip_1[3u];
        phi_139_ = (_e108 <= 0.000001f);
    }
    let _e111 = phi_139_;
    phi_146_ = _e111;
    if !(_e111) {
        let _e114 = temporalPreviousClip_1[3u];
        phi_146_ = (_e114 <= 0.000001f);
    }
    let _e117 = phi_146_;
    if _e117 {
        return;
    }
    let _e118 = temporalCurrentClip_1;
    let _e121 = temporalCurrentClip_1[3u];
    currentNdc = (_e118.xy / vec2(_e121));
    let _e124 = temporalPreviousClip_1;
    let _e127 = temporalPreviousClip_1[3u];
    previousNdc = (_e124.xy / vec2(_e127));
    let _e130 = currentNdc;
    param_2 = _e130;
    let _e131 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e132 = !(_e131);
    phi_175_ = _e132;
    if !(_e132) {
        let _e134 = previousNdc;
        param_3 = _e134;
        let _e135 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e135);
    }
    let _e138 = phi_175_;
    if _e138 {
        return;
    }
    let _e139 = currentNdc;
    currentUv = ((_e139 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e142 = previousNdc;
    previousUv = ((_e142 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e145 = currentUv;
    let _e146 = previousUv;
    velocity = (_e145 - _e146);
    let _e148 = velocity;
    param_4 = _e148;
    let _e149 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e149) {
        return;
    }
    let _e151 = velocity;
    out_temporal_velocity = _e151;
    let _e152 = (*coverageConfidence);
    out_temporal_validity = clamp(_e152, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e84 + 0.5f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e91 = fogType;
    let _e94 = fogType;
    return (((_e89 > 0.5f) && (_e91 >= 1i)) && (_e94 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e84 = wired_advanced_fog_enabled_u0028_();
    if !(_e84) {
        return 0f;
    }
    let _e87 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e87, 0.000001f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e92 + 0.5f));
    let _e95 = fogType_1;
    if (_e95 == 1i) {
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e99 <= 0f) {
            return 0f;
        }
        let _e101 = viewDepth;
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e101 / _e104), 0f, 1f);
    }
    let _e109 = unnamed.advancedFogColorDensity[3u];
    let _e111 = viewDepth;
    opticalDepth = (max(_e109, 0f) * _e111);
    let _e113 = fogType_1;
    if (_e113 == 2i) {
        let _e115 = opticalDepth;
        return clamp((1f - exp(-(_e115))), 0f, 1f);
    }
    let _e120 = opticalDepth;
    let _e121 = opticalDepth;
    return clamp((1f - exp(-((_e120 * _e121)))), 0f, 1f);
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
    var phi_470_: bool;
    var phi_477_: bool;
    var phi_484_: bool;
    var phi_491_: bool;

    let _e91 = (*c);
    let _e94 = unnamed.cascadeMVP[_e91];
    let _e95 = (*worldPos);
    sc4_ = (_e94 * vec4<f32>(_e95.x, _e95.y, _e95.z, 1f));
    let _e101 = sc4_;
    let _e104 = sc4_[3u];
    sc = (_e101.xyz / vec3(_e104));
    let _e107 = sc;
    let _e111 = ((_e107.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e111.x;
    sc[1u] = _e111.y;
    let _e117 = sc[0u];
    let _e118 = (_e117 < 0f);
    phi_470_ = _e118;
    if !(_e118) {
        let _e121 = sc[0u];
        phi_470_ = (_e121 > 1f);
    }
    let _e124 = phi_470_;
    phi_477_ = _e124;
    if !(_e124) {
        let _e127 = sc[1u];
        phi_477_ = (_e127 < 0f);
    }
    let _e130 = phi_477_;
    phi_484_ = _e130;
    if !(_e130) {
        let _e133 = sc[1u];
        phi_484_ = (_e133 > 1f);
    }
    let _e136 = phi_484_;
    phi_491_ = _e136;
    if !(_e136) {
        let _e139 = sc[2u];
        phi_491_ = (_e139 > 1f);
    }
    let _e142 = phi_491_;
    if _e142 {
        return 1f;
    }
    let _e143 = (*c);
    layer = f32(_e143);
    let _e145 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e145).xy));
    let _e152 = sc[2u];
    currentDepth = (_e152 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e154 = currentDepth;
        let _e155 = sc;
        let _e156 = _e155.xy;
        let _e157 = layer;
        let _e160 = vec3<f32>(_e156.x, _e156.y, _e157);
        let _e166 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e160.x, _e160.y), i32(_e160.z));
        shadow = step(_e154, _e166.x);
    } else {
        if override_type_3_1 {
            let _e169 = currentDepth;
            let _e170 = sc;
            let _e171 = _e170.xy;
            let _e172 = layer;
            let _e175 = vec3<f32>(_e171.x, _e171.y, _e172);
            let _e181 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e175.x, _e175.y), i32(_e175.z));
            let _e184 = shadow;
            shadow = (_e184 + step(_e169, _e181.x));
            let _e186 = currentDepth;
            let _e187 = sc;
            let _e190 = texelSize[0u];
            let _e192 = (_e187.xy + vec2<f32>(_e190, 0f));
            let _e193 = layer;
            let _e196 = vec3<f32>(_e192.x, _e192.y, _e193);
            let _e202 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e196.x, _e196.y), i32(_e196.z));
            let _e205 = shadow;
            shadow = (_e205 + step(_e186, _e202.x));
            let _e207 = currentDepth;
            let _e208 = sc;
            let _e211 = texelSize[0u];
            let _e213 = (_e208.xy - vec2<f32>(_e211, 0f));
            let _e214 = layer;
            let _e217 = vec3<f32>(_e213.x, _e213.y, _e214);
            let _e223 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e217.x, _e217.y), i32(_e217.z));
            let _e226 = shadow;
            shadow = (_e226 + step(_e207, _e223.x));
            let _e228 = currentDepth;
            let _e229 = sc;
            let _e232 = texelSize[1u];
            let _e234 = (_e229.xy + vec2<f32>(0f, _e232));
            let _e235 = layer;
            let _e238 = vec3<f32>(_e234.x, _e234.y, _e235);
            let _e244 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e238.x, _e238.y), i32(_e238.z));
            let _e247 = shadow;
            shadow = (_e247 + step(_e228, _e244.x));
            let _e249 = currentDepth;
            let _e250 = sc;
            let _e253 = texelSize[1u];
            let _e255 = (_e250.xy - vec2<f32>(0f, _e253));
            let _e256 = layer;
            let _e259 = vec3<f32>(_e255.x, _e255.y, _e256);
            let _e265 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e259.x, _e259.y), i32(_e259.z));
            let _e268 = shadow;
            shadow = (_e268 + step(_e249, _e265.x));
            let _e270 = shadow;
            shadow = (_e270 / 5f);
        } else {
            x = -1i;
            loop {
                let _e272 = x;
                if (_e272 <= 1i) {
                    y = -1i;
                    loop {
                        let _e274 = y;
                        if (_e274 <= 1i) {
                            let _e276 = currentDepth;
                            let _e277 = sc;
                            let _e279 = x;
                            let _e281 = y;
                            let _e284 = texelSize;
                            let _e286 = (_e277.xy + (vec2<f32>(f32(_e279), f32(_e281)) * _e284));
                            let _e287 = layer;
                            let _e290 = vec3<f32>(_e286.x, _e286.y, _e287);
                            let _e296 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e290.x, _e290.y), i32(_e290.z));
                            let _e299 = shadow;
                            shadow = (_e299 + step(_e276, _e296.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e301 = y;
                            y = (_e301 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e303 = x;
                    x = (_e303 + 1i);
                }
            }
            let _e305 = shadow;
            shadow = (_e305 / 9f);
        }
    }
    let _e307 = shadow;
    return _e307;
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
    var param_5: i32;
    var param_6: vec3<f32>;
    var s1_: f32;
    var param_7: i32;
    var param_8: vec3<f32>;

    let _e98 = unnamed.cascadeSplits;
    let _e99 = (*viewDepth_1);
    cmp = step(_e98, vec4(_e99));
    let _e103 = cmp[0u];
    let _e105 = cmp[1u];
    let _e108 = cmp[2u];
    let _e111 = cmp[3u];
    cascade = min(i32((((_e103 + _e105) + _e108) + _e111)), 3i);
    let _e115 = cascade;
    (*outCascade) = _e115;
    let _e116 = cascade;
    if (_e116 == 0i) {
        local = 0f;
    } else {
        let _e118 = cascade;
        let _e123 = unnamed.cascadeSplits[max((_e118 - 1i), 0i)];
        local = _e123;
    }
    let _e124 = local;
    prevSplit = _e124;
    let _e125 = cascade;
    let _e128 = unnamed.cascadeSplits[_e125];
    farSplit = _e128;
    let _e129 = farSplit;
    let _e130 = prevSplit;
    blendRange = max((0.1f * (_e129 - _e130)), 1f);
    let _e134 = farSplit;
    let _e135 = (*viewDepth_1);
    let _e137 = blendRange;
    blendT = clamp(((_e134 - _e135) / _e137), 0f, 1f);
    let _e140 = cascade;
    param_5 = _e140;
    let _e141 = (*worldPos_1);
    param_6 = _e141;
    let _e142 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e142;
    let _e143 = cascade;
    param_7 = min((_e143 + 1i), 3i);
    let _e146 = (*worldPos_1);
    param_8 = _e146;
    let _e147 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e147;
    let _e148 = s1_;
    let _e149 = s0_;
    let _e150 = blendT;
    return mix(_e148, _e149, _e150);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e92 = unnamed.packed_indices[1i][3u];
    let _e98 = unnamed.packed_indices[1i][3u];
    let _e103 = (*lm_uv);
    let _e104 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    sun_mask = _e104.x;
    let _e106 = sun_mask;
    if (_e106 > 0.001f) {
        let _e108 = shadowData_1;
        param_9 = _e108.xyz;
        let _e111 = shadowData_1[3u];
        param_10 = _e111;
        let _e112 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e113 = param_11;
        ignoredCascade = _e113;
        shadow_1 = _e112;
        let _e114 = shadow_1;
        let _e115 = sun_mask;
        let _e117 = (*rgb);
        (*rgb) = (_e117 * mix(1f, _e114, _e115));
    }
    let _e119 = (*rgb);
    return _e119;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;

    if override_type_3_2 {
        let _e84 = (*rgb_1);
        param_12 = _e84;
        let _e85 = frag_tex_coord0_1;
        param_13 = _e85;
        let _e86 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e86;
    }
    let _e87 = (*rgb_1);
    return _e87;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e84 = (*rgb_2);
    let _e87 = unnamed.worldLightParams[0u];
    boosted = (_e84 * _e87);
    let _e90 = boosted[0u];
    let _e92 = boosted[1u];
    let _e94 = boosted[2u];
    peak = max(_e90, max(_e92, _e94));
    let _e97 = peak;
    if (_e97 > 1f) {
        let _e99 = peak;
        let _e100 = boosted;
        boosted = (_e100 / vec3(_e99));
    }
    let _e103 = boosted;
    return _e103;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e85 = (*c_1);
    (*c_1) = max(_e85, vec3<f32>(0f, 0f, 0f));
    let _e87 = (*c_1);
    cutoff = (_e87 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e89 = (*c_1);
    lo = (_e89 / vec3(12.92f));
    let _e92 = (*c_1);
    hi = pow(((_e92 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e97 = hi;
    let _e98 = lo;
    let _e99 = cutoff;
    return mix(_e97, _e98, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e99));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_14: vec3<f32>;
    var param_15: vec3<f32>;

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
        param_14 = _e116.xyz;
        let _e118 = sRGBToLinear_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e118.x;
        c_2[1u] = _e118.y;
        c_2[2u] = _e118.z;
    }
    let _e125 = (*slot);
    if (lightmap_slot == (_e125 + 1i)) {
        let _e128 = c_2;
        param_15 = _e128.xyz;
        let _e130 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_15));
        c_2[0u] = _e130.x;
        c_2[1u] = _e130.y;
        c_2[2u] = _e130.z;
    }
    let _e137 = c_2;
    return _e137;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_16: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_20: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_21: f32;

    let _e95 = frag_color0In_1;
    param_16 = _e95.xyz;
    let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e99 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e97.x, _e97.y, _e97.z, _e99);
    param_17 = 0u;
    let _e104 = frag_tex_coord0_1;
    param_18 = _e104;
    param_19 = 0i;
    let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    sampledColor0_ = _e105;
    let _e106 = sampledColor0_;
    let _e107 = frag_color0_;
    color0_ = (_e106 * _e107);
    let _e109 = color0_;
    base = _e109;
    let _e110 = color0_;
    base = _e110;
    let _e111 = base;
    param_20 = _e111.xyz;
    let _e113 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_20));
    base[0u] = _e113.x;
    base[1u] = _e113.y;
    base[2u] = _e113.z;
    if override_type_3_3 {
        let _e122 = unnamed.worldLightParams[1u];
        wetness = clamp(_e122, 0f, 1f);
        let _e126 = unnamed.worldLightParams[2u];
        frost = clamp(_e126, 0f, 1f);
        let _e128 = base;
        luminance = dot(_e128.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e131 = wetness;
        let _e133 = base;
        let _e135 = (_e133.xyz * mix(1f, 0.82f, _e131));
        base[0u] = _e135.x;
        base[1u] = _e135.y;
        base[2u] = _e135.z;
        let _e142 = base;
        let _e144 = luminance;
        let _e146 = luminance;
        let _e148 = luminance;
        let _e150 = frost;
        let _e153 = mix(_e142.xyz, vec3<f32>((_e144 * 0.88f), (_e146 * 0.94f), _e148), vec3((_e150 * 0.55f)));
        base[0u] = _e153.x;
        base[1u] = _e153.y;
        base[2u] = _e153.z;
    }
    let _e162 = unnamed.materialFactors[0u];
    if (_e162 >= 0f) {
        let _e165 = unnamed.materialBaseColor;
        let _e166 = base;
        base = (_e166 * _e165);
    }
    let _e168 = color0_;
    let _e171 = unnamed.emissionRadiance;
    let _e174 = base;
    let _e176 = (_e174.xyz + (_e168.xyz * _e171.xyz));
    base[0u] = _e176.x;
    base[1u] = _e176.y;
    base[2u] = _e176.z;
    let _e183 = wired_advanced_fog_enabled_u0028_();
    if _e183 {
        let _e184 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e184;
        let _e185 = base;
        let _e188 = unnamed.advancedFogColorDensity;
        let _e190 = fogAmount;
        let _e192 = mix(_e185.xyz, _e188.xyz, vec3(_e190));
        base[0u] = _e192.x;
        base[1u] = _e192.y;
        base[2u] = _e192.z;
    }
    if override_type_3_4 {
        let _e200 = base[3u];
        if (_e200 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e202 = base;
            let _e204 = base;
            if (dot(_e202.xyz, _e204.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e208 = base;
    out_color = _e208;
    param_21 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_21));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
