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
override override_type_4_3: bool = (lightmap_slot == 2i);
@id(6) override tex_mode: i32 = 0i;
override override_type_4_4: bool = (tex_mode == 1i);
override override_type_4_5: bool = (tex_mode == 2i);
override override_type_4_6: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_7: bool = (discard_mode == 1i);
override override_type_4_8: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord1_1: vec2<f32>;
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
    var phi_340_: bool;
    var phi_347_: bool;
    var phi_354_: bool;
    var phi_361_: bool;

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
    phi_340_ = _e118;
    if !(_e118) {
        let _e121 = sc[0u];
        phi_340_ = (_e121 > 1f);
    }
    let _e124 = phi_340_;
    phi_347_ = _e124;
    if !(_e124) {
        let _e127 = sc[1u];
        phi_347_ = (_e127 < 0f);
    }
    let _e130 = phi_347_;
    phi_354_ = _e130;
    if !(_e130) {
        let _e133 = sc[1u];
        phi_354_ = (_e133 > 1f);
    }
    let _e136 = phi_354_;
    phi_361_ = _e136;
    if !(_e136) {
        let _e139 = sc[2u];
        phi_361_ = (_e139 > 1f);
    }
    let _e142 = phi_361_;
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
    if override_type_4_ {
        let _e154 = currentDepth;
        let _e155 = sc;
        let _e156 = _e155.xy;
        let _e157 = layer;
        let _e160 = vec3<f32>(_e156.x, _e156.y, _e157);
        let _e166 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e160.x, _e160.y), i32(_e160.z));
        shadow = step(_e154, _e166.x);
    } else {
        if override_type_4_1 {
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
    var param: i32;
    var param_1: vec3<f32>;
    var s1_: f32;
    var param_2: i32;
    var param_3: vec3<f32>;

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
    param = _e140;
    let _e141 = (*worldPos_1);
    param_1 = _e141;
    let _e142 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e142;
    let _e143 = cascade;
    param_2 = min((_e143 + 1i), 3i);
    let _e146 = (*worldPos_1);
    param_3 = _e146;
    let _e147 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
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
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e92 = unnamed.packed_indices[1i][3u];
    let _e98 = unnamed.packed_indices[1i][3u];
    let _e103 = (*lm_uv);
    let _e104 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    sun_mask = _e104.x;
    let _e106 = sun_mask;
    if (_e106 > 0.001f) {
        let _e108 = shadowData_1;
        param_4 = _e108.xyz;
        let _e111 = shadowData_1[3u];
        param_5 = _e111;
        let _e112 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e113 = param_6;
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
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_4_2 {
        let _e86 = (*rgb_1);
        param_7 = _e86;
        let _e87 = frag_tex_coord0_1;
        param_8 = _e87;
        let _e88 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e88;
    }
    if override_type_4_3 {
        let _e89 = (*rgb_1);
        param_9 = _e89;
        let _e90 = frag_tex_coord1_1;
        param_10 = _e90;
        let _e91 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e91;
    }
    let _e92 = (*rgb_1);
    return _e92;
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
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

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
        param_11 = _e116.xyz;
        let _e118 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e118.x;
        c_2[1u] = _e118.y;
        c_2[2u] = _e118.z;
    }
    let _e125 = (*slot);
    if (lightmap_slot == (_e125 + 1i)) {
        let _e128 = c_2;
        param_12 = _e128.xyz;
        let _e130 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e130.x;
        c_2[1u] = _e130.y;
        c_2[2u] = _e130.z;
    }
    let _e137 = c_2;
    return _e137;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var sampledColor1_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var param_19: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_20: f32;

    param_13 = 0u;
    let _e100 = frag_tex_coord0_1;
    param_14 = _e100;
    param_15 = 0i;
    let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    sampledColor0_ = _e101;
    param_16 = 1u;
    let _e102 = frag_tex_coord1_1;
    param_17 = _e102;
    param_18 = 1i;
    let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor1_ = _e103;
    let _e104 = sampledColor0_;
    color0_ = _e104;
    if override_type_4_4 {
        let _e105 = sampledColor1_;
        color1_ = _e105;
        let _e106 = color0_;
        let _e108 = color1_;
        let _e110 = (_e106.xyz + _e108.xyz);
        let _e112 = color0_[3u];
        let _e114 = color1_[3u];
        base = vec4<f32>(_e110.x, _e110.y, _e110.z, (_e112 * _e114));
    } else {
        if override_type_4_5 {
            let _e120 = sampledColor1_;
            color1_1 = _e120;
            let _e121 = color0_;
            let _e123 = color1_1;
            let _e125 = (_e121.xyz + _e123.xyz);
            let _e127 = color0_[3u];
            let _e129 = color1_1[3u];
            base = vec4<f32>(_e125.x, _e125.y, _e125.z, (_e127 * _e129));
        } else {
            let _e135 = sampledColor1_;
            color1_2 = _e135;
            let _e136 = color0_;
            let _e138 = color1_2;
            let _e140 = (_e136.xyz * _e138.xyz);
            base[0u] = _e140.x;
            base[1u] = _e140.y;
            base[2u] = _e140.z;
            let _e148 = color0_[3u];
            let _e150 = color1_2[3u];
            base[3u] = (_e148 * _e150);
        }
    }
    let _e153 = base;
    param_19 = _e153.xyz;
    let _e155 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_19));
    base[0u] = _e155.x;
    base[1u] = _e155.y;
    base[2u] = _e155.z;
    if override_type_4_6 {
        let _e164 = unnamed.worldLightParams[1u];
        wetness = clamp(_e164, 0f, 1f);
        let _e168 = unnamed.worldLightParams[2u];
        frost = clamp(_e168, 0f, 1f);
        let _e170 = base;
        luminance = dot(_e170.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e173 = wetness;
        let _e175 = base;
        let _e177 = (_e175.xyz * mix(1f, 0.82f, _e173));
        base[0u] = _e177.x;
        base[1u] = _e177.y;
        base[2u] = _e177.z;
        let _e184 = base;
        let _e186 = luminance;
        let _e188 = luminance;
        let _e190 = luminance;
        let _e192 = frost;
        let _e195 = mix(_e184.xyz, vec3<f32>((_e186 * 0.88f), (_e188 * 0.94f), _e190), vec3((_e192 * 0.55f)));
        base[0u] = _e195.x;
        base[1u] = _e195.y;
        base[2u] = _e195.z;
    }
    let _e204 = unnamed.materialFactors[0u];
    if (_e204 >= 0f) {
        let _e207 = unnamed.materialBaseColor;
        let _e208 = base;
        base = (_e208 * _e207);
    }
    let _e210 = color0_;
    let _e213 = unnamed.emissionRadiance;
    let _e216 = base;
    let _e218 = (_e216.xyz + (_e210.xyz * _e213.xyz));
    base[0u] = _e218.x;
    base[1u] = _e218.y;
    base[2u] = _e218.z;
    let _e225 = wired_advanced_fog_enabled_u0028_();
    if _e225 {
        let _e226 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e226;
        let _e227 = base;
        let _e230 = unnamed.advancedFogColorDensity;
        let _e232 = fogAmount;
        let _e234 = mix(_e227.xyz, _e230.xyz, vec3(_e232));
        base[0u] = _e234.x;
        base[1u] = _e234.y;
        base[2u] = _e234.z;
    }
    if override_type_4_7 {
        let _e242 = base[3u];
        if (_e242 == 0f) {
            discard;
        }
    } else {
        if override_type_4_8 {
            let _e244 = base;
            let _e246 = base;
            if (dot(_e244.xyz, _e246.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e250 = base;
    out_color = _e250;
    param_20 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_20));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
