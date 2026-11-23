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
var<private> frag_color0In_1: vec4<f32>;
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
    var phi_340_: bool;
    var phi_347_: bool;
    var phi_354_: bool;
    var phi_361_: bool;

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
    phi_340_ = _e119;
    if !(_e119) {
        let _e122 = sc[0u];
        phi_340_ = (_e122 > 1f);
    }
    let _e125 = phi_340_;
    phi_347_ = _e125;
    if !(_e125) {
        let _e128 = sc[1u];
        phi_347_ = (_e128 < 0f);
    }
    let _e131 = phi_347_;
    phi_354_ = _e131;
    if !(_e131) {
        let _e134 = sc[1u];
        phi_354_ = (_e134 > 1f);
    }
    let _e137 = phi_354_;
    phi_361_ = _e137;
    if !(_e137) {
        let _e140 = sc[2u];
        phi_361_ = (_e140 > 1f);
    }
    let _e143 = phi_361_;
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
    if override_type_4_ {
        let _e155 = currentDepth;
        let _e156 = sc;
        let _e157 = _e156.xy;
        let _e158 = layer;
        let _e161 = vec3<f32>(_e157.x, _e157.y, _e158);
        let _e167 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e161.x, _e161.y), i32(_e161.z));
        shadow = step(_e155, _e167.x);
    } else {
        if override_type_4_1 {
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

    if override_type_4_2 {
        let _e87 = (*rgb_1);
        param_7 = _e87;
        let _e88 = frag_tex_coord0_1;
        param_8 = _e88;
        let _e89 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e89;
    }
    if override_type_4_3 {
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
    var frag_color0_: vec4<f32>;
    var param_13: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var sampledColor1_: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var param_20: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_21: f32;

    let _e103 = frag_color0In_1;
    param_13 = _e103.xyz;
    let _e105 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e107 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e105.x, _e105.y, _e105.z, _e107);
    param_14 = 0u;
    let _e112 = frag_tex_coord0_1;
    param_15 = _e112;
    param_16 = 0i;
    let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    sampledColor0_ = _e113;
    param_17 = 1u;
    let _e114 = frag_tex_coord1_1;
    param_18 = _e114;
    param_19 = 1i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    sampledColor1_ = _e115;
    let _e116 = sampledColor0_;
    let _e117 = frag_color0_;
    color0_ = (_e116 * _e117);
    if override_type_4_4 {
        let _e119 = sampledColor1_;
        color1_ = _e119;
        let _e120 = color0_;
        let _e122 = color1_;
        let _e124 = (_e120.xyz + _e122.xyz);
        let _e126 = color0_[3u];
        let _e128 = color1_[3u];
        base = vec4<f32>(_e124.x, _e124.y, _e124.z, (_e126 * _e128));
    } else {
        if override_type_4_5 {
            let _e134 = sampledColor1_;
            let _e135 = frag_color0_;
            color1_1 = (_e134 * _e135);
            let _e137 = color0_;
            let _e139 = color1_1;
            let _e141 = (_e137.xyz + _e139.xyz);
            let _e143 = color0_[3u];
            let _e145 = color1_1[3u];
            base = vec4<f32>(_e141.x, _e141.y, _e141.z, (_e143 * _e145));
        } else {
            let _e151 = sampledColor1_;
            color1_2 = _e151;
            let _e152 = color0_;
            let _e154 = color1_2;
            let _e156 = (_e152.xyz * _e154.xyz);
            base[0u] = _e156.x;
            base[1u] = _e156.y;
            base[2u] = _e156.z;
            let _e164 = color0_[3u];
            let _e166 = color1_2[3u];
            base[3u] = (_e164 * _e166);
        }
    }
    let _e169 = base;
    param_20 = _e169.xyz;
    let _e171 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_20));
    base[0u] = _e171.x;
    base[1u] = _e171.y;
    base[2u] = _e171.z;
    if override_type_4_6 {
        let _e180 = unnamed.worldLightParams[1u];
        wetness = clamp(_e180, 0f, 1f);
        let _e184 = unnamed.worldLightParams[2u];
        frost = clamp(_e184, 0f, 1f);
        let _e186 = base;
        luminance = dot(_e186.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e189 = wetness;
        let _e191 = base;
        let _e193 = (_e191.xyz * mix(1f, 0.82f, _e189));
        base[0u] = _e193.x;
        base[1u] = _e193.y;
        base[2u] = _e193.z;
        let _e200 = base;
        let _e202 = luminance;
        let _e204 = luminance;
        let _e206 = luminance;
        let _e208 = frost;
        let _e211 = mix(_e200.xyz, vec3<f32>((_e202 * 0.88f), (_e204 * 0.94f), _e206), vec3((_e208 * 0.55f)));
        base[0u] = _e211.x;
        base[1u] = _e211.y;
        base[2u] = _e211.z;
    }
    let _e220 = unnamed.materialFactors[0u];
    if (_e220 >= 0f) {
        let _e223 = unnamed.materialBaseColor;
        let _e224 = base;
        base = (_e224 * _e223);
    }
    let _e226 = color0_;
    let _e229 = unnamed.emissionRadiance;
    let _e232 = base;
    let _e234 = (_e232.xyz + (_e226.xyz * _e229.xyz));
    base[0u] = _e234.x;
    base[1u] = _e234.y;
    base[2u] = _e234.z;
    let _e241 = wired_advanced_fog_enabled_u0028_();
    if _e241 {
        let _e242 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e242;
        let _e243 = base;
        let _e246 = unnamed.advancedFogColorDensity;
        let _e248 = fogAmount;
        let _e250 = mix(_e243.xyz, _e246.xyz, vec3(_e248));
        base[0u] = _e250.x;
        base[1u] = _e250.y;
        base[2u] = _e250.z;
    }
    if override_type_4_7 {
        let _e258 = base[3u];
        if (_e258 == 0f) {
            discard;
        }
    } else {
        if override_type_4_8 {
            let _e260 = base;
            let _e262 = base;
            if (dot(_e260.xyz, _e262.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e266 = base;
    out_color = _e266;
    param_21 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_21));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
