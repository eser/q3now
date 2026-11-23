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
override override_type_4_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_4_5: bool = (tex_mode == 1i);
override override_type_4_6: bool = (tex_mode == 2i);
override override_type_4_7: bool = (override_type_4_5 || override_type_4_6);
override override_type_4_8: bool = (tex_mode == 3i);
override override_type_4_9: bool = (tex_mode == 4i);
override override_type_4_10: bool = (tex_mode == 5i);
override override_type_4_11: bool = (tex_mode == 6i);
override override_type_4_12: bool = (tex_mode == 7i);
override override_type_4_13: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_14: bool = (discard_mode == 1i);
override override_type_4_15: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
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

    let _e98 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e98 + 0.5f));
    let _e103 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e105 = fogType;
    let _e108 = fogType;
    return (((_e103 > 0.5f) && (_e105 >= 1i)) && (_e108 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e98 = wired_advanced_fog_enabled_u0028_();
    if !(_e98) {
        return 0f;
    }
    let _e101 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e101, 0.000001f));
    let _e106 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e106 + 0.5f));
    let _e109 = fogType_1;
    if (_e109 == 1i) {
        let _e113 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e113 <= 0f) {
            return 0f;
        }
        let _e115 = viewDepth;
        let _e118 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e115 / _e118), 0f, 1f);
    }
    let _e123 = unnamed.advancedFogColorDensity[3u];
    let _e125 = viewDepth;
    opticalDepth = (max(_e123, 0f) * _e125);
    let _e127 = fogType_1;
    if (_e127 == 2i) {
        let _e129 = opticalDepth;
        return clamp((1f - exp(-(_e129))), 0f, 1f);
    }
    let _e134 = opticalDepth;
    let _e135 = opticalDepth;
    return clamp((1f - exp(-((_e134 * _e135)))), 0f, 1f);
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

    let _e105 = (*c);
    let _e108 = unnamed.cascadeMVP[_e105];
    let _e109 = (*worldPos);
    sc4_ = (_e108 * vec4<f32>(_e109.x, _e109.y, _e109.z, 1f));
    let _e115 = sc4_;
    let _e118 = sc4_[3u];
    sc = (_e115.xyz / vec3(_e118));
    let _e121 = sc;
    let _e125 = ((_e121.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e125.x;
    sc[1u] = _e125.y;
    let _e131 = sc[0u];
    let _e132 = (_e131 < 0f);
    phi_340_ = _e132;
    if !(_e132) {
        let _e135 = sc[0u];
        phi_340_ = (_e135 > 1f);
    }
    let _e138 = phi_340_;
    phi_347_ = _e138;
    if !(_e138) {
        let _e141 = sc[1u];
        phi_347_ = (_e141 < 0f);
    }
    let _e144 = phi_347_;
    phi_354_ = _e144;
    if !(_e144) {
        let _e147 = sc[1u];
        phi_354_ = (_e147 > 1f);
    }
    let _e150 = phi_354_;
    phi_361_ = _e150;
    if !(_e150) {
        let _e153 = sc[2u];
        phi_361_ = (_e153 > 1f);
    }
    let _e156 = phi_361_;
    if _e156 {
        return 1f;
    }
    let _e157 = (*c);
    layer = f32(_e157);
    let _e159 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e159).xy));
    let _e166 = sc[2u];
    currentDepth = (_e166 - shadow_bias);
    shadow = 0f;
    if override_type_4_ {
        let _e168 = currentDepth;
        let _e169 = sc;
        let _e170 = _e169.xy;
        let _e171 = layer;
        let _e174 = vec3<f32>(_e170.x, _e170.y, _e171);
        let _e180 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e174.x, _e174.y), i32(_e174.z));
        shadow = step(_e168, _e180.x);
    } else {
        if override_type_4_1 {
            let _e183 = currentDepth;
            let _e184 = sc;
            let _e185 = _e184.xy;
            let _e186 = layer;
            let _e189 = vec3<f32>(_e185.x, _e185.y, _e186);
            let _e195 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e189.x, _e189.y), i32(_e189.z));
            let _e198 = shadow;
            shadow = (_e198 + step(_e183, _e195.x));
            let _e200 = currentDepth;
            let _e201 = sc;
            let _e204 = texelSize[0u];
            let _e206 = (_e201.xy + vec2<f32>(_e204, 0f));
            let _e207 = layer;
            let _e210 = vec3<f32>(_e206.x, _e206.y, _e207);
            let _e216 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e210.x, _e210.y), i32(_e210.z));
            let _e219 = shadow;
            shadow = (_e219 + step(_e200, _e216.x));
            let _e221 = currentDepth;
            let _e222 = sc;
            let _e225 = texelSize[0u];
            let _e227 = (_e222.xy - vec2<f32>(_e225, 0f));
            let _e228 = layer;
            let _e231 = vec3<f32>(_e227.x, _e227.y, _e228);
            let _e237 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e231.x, _e231.y), i32(_e231.z));
            let _e240 = shadow;
            shadow = (_e240 + step(_e221, _e237.x));
            let _e242 = currentDepth;
            let _e243 = sc;
            let _e246 = texelSize[1u];
            let _e248 = (_e243.xy + vec2<f32>(0f, _e246));
            let _e249 = layer;
            let _e252 = vec3<f32>(_e248.x, _e248.y, _e249);
            let _e258 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e252.x, _e252.y), i32(_e252.z));
            let _e261 = shadow;
            shadow = (_e261 + step(_e242, _e258.x));
            let _e263 = currentDepth;
            let _e264 = sc;
            let _e267 = texelSize[1u];
            let _e269 = (_e264.xy - vec2<f32>(0f, _e267));
            let _e270 = layer;
            let _e273 = vec3<f32>(_e269.x, _e269.y, _e270);
            let _e279 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e273.x, _e273.y), i32(_e273.z));
            let _e282 = shadow;
            shadow = (_e282 + step(_e263, _e279.x));
            let _e284 = shadow;
            shadow = (_e284 / 5f);
        } else {
            x = -1i;
            loop {
                let _e286 = x;
                if (_e286 <= 1i) {
                    y = -1i;
                    loop {
                        let _e288 = y;
                        if (_e288 <= 1i) {
                            let _e290 = currentDepth;
                            let _e291 = sc;
                            let _e293 = x;
                            let _e295 = y;
                            let _e298 = texelSize;
                            let _e300 = (_e291.xy + (vec2<f32>(f32(_e293), f32(_e295)) * _e298));
                            let _e301 = layer;
                            let _e304 = vec3<f32>(_e300.x, _e300.y, _e301);
                            let _e310 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e304.x, _e304.y), i32(_e304.z));
                            let _e313 = shadow;
                            shadow = (_e313 + step(_e290, _e310.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e315 = y;
                            y = (_e315 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e317 = x;
                    x = (_e317 + 1i);
                }
            }
            let _e319 = shadow;
            shadow = (_e319 / 9f);
        }
    }
    let _e321 = shadow;
    return _e321;
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

    let _e112 = unnamed.cascadeSplits;
    let _e113 = (*viewDepth_1);
    cmp = step(_e112, vec4(_e113));
    let _e117 = cmp[0u];
    let _e119 = cmp[1u];
    let _e122 = cmp[2u];
    let _e125 = cmp[3u];
    cascade = min(i32((((_e117 + _e119) + _e122) + _e125)), 3i);
    let _e129 = cascade;
    (*outCascade) = _e129;
    let _e130 = cascade;
    if (_e130 == 0i) {
        local = 0f;
    } else {
        let _e132 = cascade;
        let _e137 = unnamed.cascadeSplits[max((_e132 - 1i), 0i)];
        local = _e137;
    }
    let _e138 = local;
    prevSplit = _e138;
    let _e139 = cascade;
    let _e142 = unnamed.cascadeSplits[_e139];
    farSplit = _e142;
    let _e143 = farSplit;
    let _e144 = prevSplit;
    blendRange = max((0.1f * (_e143 - _e144)), 1f);
    let _e148 = farSplit;
    let _e149 = (*viewDepth_1);
    let _e151 = blendRange;
    blendT = clamp(((_e148 - _e149) / _e151), 0f, 1f);
    let _e154 = cascade;
    param = _e154;
    let _e155 = (*worldPos_1);
    param_1 = _e155;
    let _e156 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e156;
    let _e157 = cascade;
    param_2 = min((_e157 + 1i), 3i);
    let _e160 = (*worldPos_1);
    param_3 = _e160;
    let _e161 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e161;
    let _e162 = s1_;
    let _e163 = s0_;
    let _e164 = blendT;
    return mix(_e162, _e163, _e164);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e106 = unnamed.packed_indices[1i][3u];
    let _e112 = unnamed.packed_indices[1i][3u];
    let _e117 = (*lm_uv);
    let _e118 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    sun_mask = _e118.x;
    let _e120 = sun_mask;
    if (_e120 > 0.001f) {
        let _e122 = shadowData_1;
        param_4 = _e122.xyz;
        let _e125 = shadowData_1[3u];
        param_5 = _e125;
        let _e126 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e127 = param_6;
        ignoredCascade = _e127;
        shadow_1 = _e126;
        let _e128 = shadow_1;
        let _e129 = sun_mask;
        let _e131 = (*rgb);
        (*rgb) = (_e131 * mix(1f, _e128, _e129));
    }
    let _e133 = (*rgb);
    return _e133;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_4_2 {
        let _e102 = (*rgb_1);
        param_7 = _e102;
        let _e103 = frag_tex_coord0_1;
        param_8 = _e103;
        let _e104 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e104;
    }
    if override_type_4_3 {
        let _e105 = (*rgb_1);
        param_9 = _e105;
        let _e106 = frag_tex_coord1_1;
        param_10 = _e106;
        let _e107 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e107;
    }
    if override_type_4_4 {
        let _e108 = (*rgb_1);
        param_11 = _e108;
        let _e109 = frag_tex_coord2_1;
        param_12 = _e109;
        let _e110 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e110;
    }
    let _e111 = (*rgb_1);
    return _e111;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e98 = (*rgb_2);
    let _e101 = unnamed.worldLightParams[0u];
    boosted = (_e98 * _e101);
    let _e104 = boosted[0u];
    let _e106 = boosted[1u];
    let _e108 = boosted[2u];
    peak = max(_e104, max(_e106, _e108));
    let _e111 = peak;
    if (_e111 > 1f) {
        let _e113 = peak;
        let _e114 = boosted;
        boosted = (_e114 / vec3(_e113));
    }
    let _e117 = boosted;
    return _e117;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e99 = (*c_1);
    (*c_1) = max(_e99, vec3<f32>(0f, 0f, 0f));
    let _e101 = (*c_1);
    cutoff = (_e101 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e103 = (*c_1);
    lo = (_e103 / vec3(12.92f));
    let _e106 = (*c_1);
    hi = pow(((_e106 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e111 = hi;
    let _e112 = lo;
    let _e113 = cutoff;
    return mix(_e111, _e112, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e113));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e101 = (*role);
    let _e103 = (*role);
    let _e108 = unnamed.packed_indices[(_e101 / 4u)][(_e103 % 4u)];
    let _e111 = (*role);
    let _e113 = (*role);
    let _e118 = unnamed.packed_indices[(_e111 / 4u)][(_e113 % 4u)];
    let _e123 = (*uv);
    let _e124 = textureSample(wired_bindless_images[(_e108 & 4095u)], wired_bindless_samplers[((_e118 >> bitcast<u32>(12i)) & 255u)], _e123);
    c_2 = _e124;
    let _e125 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e125))) == 0i) {
        let _e130 = c_2;
        param_13 = _e130.xyz;
        let _e132 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e132.x;
        c_2[1u] = _e132.y;
        c_2[2u] = _e132.z;
    }
    let _e139 = (*slot);
    if (lightmap_slot == (_e139 + 1i)) {
        let _e142 = c_2;
        param_14 = _e142.xyz;
        let _e144 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e144.x;
        c_2[1u] = _e144.y;
        c_2[2u] = _e144.z;
    }
    let _e151 = c_2;
    return _e151;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_15: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_16: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_17: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var sampledColor1_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var sampledColor2_: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color2_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color2_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color2_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var color2_6: vec4<f32>;
    var param_27: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_28: f32;

    let _e135 = frag_color0In_1;
    param_15 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e139 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    let _e144 = frag_color1In_1;
    param_16 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e148 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e146.x, _e146.y, _e146.z, _e148);
    let _e153 = frag_color2In_1;
    param_17 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_17));
    let _e157 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    param_18 = 0u;
    let _e162 = frag_tex_coord0_1;
    param_19 = _e162;
    param_20 = 0i;
    let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e163;
    param_21 = 1u;
    let _e164 = frag_tex_coord1_1;
    param_22 = _e164;
    param_23 = 1i;
    let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e165;
    param_24 = 2u;
    let _e166 = frag_tex_coord2_1;
    param_25 = _e166;
    param_26 = 2i;
    let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
    sampledColor2_ = _e167;
    let _e168 = sampledColor0_;
    let _e169 = frag_color0_;
    color0_ = (_e168 * _e169);
    if override_type_4_7 {
        let _e171 = sampledColor1_;
        let _e172 = frag_color1_;
        color1_ = (_e171 * _e172);
        let _e174 = sampledColor2_;
        let _e175 = frag_color2_;
        color2_ = (_e174 * _e175);
        let _e177 = color0_;
        let _e179 = color1_;
        let _e182 = color2_;
        let _e184 = ((_e177.xyz + _e179.xyz) + _e182.xyz);
        let _e186 = color0_[3u];
        let _e188 = color1_[3u];
        let _e191 = color2_[3u];
        base = vec4<f32>(_e184.x, _e184.y, _e184.z, ((_e186 * _e188) * _e191));
    } else {
        if override_type_4_8 {
            let _e197 = sampledColor1_;
            let _e198 = frag_color1_;
            color1_1 = (_e197 * _e198);
            let _e200 = sampledColor2_;
            let _e201 = frag_color2_;
            color2_1 = (_e200 * _e201);
            let _e204 = color0_[3u];
            let _e205 = color0_;
            color0_ = (_e205 * _e204);
            let _e208 = color1_1[3u];
            let _e209 = color1_1;
            color1_1 = (_e209 * _e208);
            let _e212 = color2_1[3u];
            let _e213 = color2_1;
            color2_1 = (_e213 * _e212);
            let _e215 = color0_;
            let _e217 = color1_1;
            let _e220 = color2_1;
            let _e222 = ((_e215.xyz + _e217.xyz) + _e220.xyz);
            let _e224 = color0_[3u];
            let _e226 = color1_1[3u];
            let _e229 = color2_1[3u];
            base = vec4<f32>(_e222.x, _e222.y, _e222.z, ((_e224 * _e226) * _e229));
        } else {
            if override_type_4_9 {
                let _e235 = sampledColor1_;
                let _e236 = frag_color1_;
                color1_2 = (_e235 * _e236);
                let _e238 = sampledColor2_;
                let _e239 = frag_color2_;
                color2_2 = (_e238 * _e239);
                let _e242 = color0_[3u];
                let _e244 = color0_;
                color0_ = (_e244 * (1f - _e242));
                let _e247 = color1_2[3u];
                let _e249 = color1_2;
                color1_2 = (_e249 * (1f - _e247));
                let _e252 = color2_2[3u];
                let _e254 = color2_2;
                color2_2 = (_e254 * (1f - _e252));
                let _e256 = color0_;
                let _e258 = color1_2;
                let _e261 = color2_2;
                let _e263 = ((_e256.xyz + _e258.xyz) + _e261.xyz);
                let _e265 = color0_[3u];
                let _e267 = color1_2[3u];
                let _e270 = color2_2[3u];
                base = vec4<f32>(_e263.x, _e263.y, _e263.z, ((_e265 * _e267) * _e270));
            } else {
                if override_type_4_10 {
                    let _e276 = sampledColor1_;
                    let _e277 = frag_color1_;
                    color1_3 = (_e276 * _e277);
                    let _e279 = sampledColor2_;
                    let _e280 = frag_color2_;
                    color2_3 = (_e279 * _e280);
                    let _e282 = color0_;
                    let _e283 = color1_3;
                    let _e285 = color1_3[3u];
                    let _e288 = color2_3;
                    let _e290 = color2_3[3u];
                    base = mix(mix(_e282, _e283, vec4(_e285)), _e288, vec4(_e290));
                } else {
                    if override_type_4_11 {
                        let _e293 = sampledColor1_;
                        let _e294 = frag_color1_;
                        color1_4 = (_e293 * _e294);
                        let _e296 = sampledColor2_;
                        let _e297 = frag_color2_;
                        color2_4 = (_e296 * _e297);
                        let _e299 = color2_4;
                        let _e300 = color1_4;
                        let _e301 = color0_;
                        let _e303 = color1_4[3u];
                        let _e307 = color2_4[3u];
                        base = mix(_e299, mix(_e300, _e301, vec4(_e303)), vec4(_e307));
                    } else {
                        if override_type_4_12 {
                            let _e310 = sampledColor1_;
                            let _e311 = frag_color1_;
                            color1_5 = (_e310 * _e311);
                            let _e313 = sampledColor2_;
                            let _e314 = frag_color2_;
                            color2_5 = (_e313 * _e314);
                            let _e316 = color2_5;
                            let _e318 = color2_5[3u];
                            let _e321 = color1_5;
                            let _e323 = color1_5[3u];
                            let _e327 = color0_;
                            base = (((_e316 + vec4(_e318)) * (_e321 + vec4(_e323))) * _e327);
                        } else {
                            let _e329 = sampledColor1_;
                            let _e330 = frag_color1_;
                            color1_6 = (_e329 * _e330);
                            let _e332 = sampledColor2_;
                            let _e333 = frag_color2_;
                            color2_6 = (_e332 * _e333);
                            let _e335 = color0_;
                            let _e337 = color1_6;
                            let _e340 = color2_6;
                            let _e342 = ((_e335.xyz * _e337.xyz) * _e340.xyz);
                            base[0u] = _e342.x;
                            base[1u] = _e342.y;
                            base[2u] = _e342.z;
                            let _e350 = color0_[3u];
                            let _e352 = color1_6[3u];
                            let _e355 = color2_6[3u];
                            base[3u] = ((_e350 * _e352) * _e355);
                        }
                    }
                }
            }
        }
    }
    let _e358 = base;
    param_27 = _e358.xyz;
    let _e360 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_27));
    base[0u] = _e360.x;
    base[1u] = _e360.y;
    base[2u] = _e360.z;
    if override_type_4_13 {
        let _e369 = unnamed.worldLightParams[1u];
        wetness = clamp(_e369, 0f, 1f);
        let _e373 = unnamed.worldLightParams[2u];
        frost = clamp(_e373, 0f, 1f);
        let _e375 = base;
        luminance = dot(_e375.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e378 = wetness;
        let _e380 = base;
        let _e382 = (_e380.xyz * mix(1f, 0.82f, _e378));
        base[0u] = _e382.x;
        base[1u] = _e382.y;
        base[2u] = _e382.z;
        let _e389 = base;
        let _e391 = luminance;
        let _e393 = luminance;
        let _e395 = luminance;
        let _e397 = frost;
        let _e400 = mix(_e389.xyz, vec3<f32>((_e391 * 0.88f), (_e393 * 0.94f), _e395), vec3((_e397 * 0.55f)));
        base[0u] = _e400.x;
        base[1u] = _e400.y;
        base[2u] = _e400.z;
    }
    let _e409 = unnamed.materialFactors[0u];
    if (_e409 >= 0f) {
        let _e412 = unnamed.materialBaseColor;
        let _e413 = base;
        base = (_e413 * _e412);
    }
    let _e415 = color0_;
    let _e418 = unnamed.emissionRadiance;
    let _e421 = base;
    let _e423 = (_e421.xyz + (_e415.xyz * _e418.xyz));
    base[0u] = _e423.x;
    base[1u] = _e423.y;
    base[2u] = _e423.z;
    let _e430 = wired_advanced_fog_enabled_u0028_();
    if _e430 {
        let _e431 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e431;
        let _e432 = base;
        let _e435 = unnamed.advancedFogColorDensity;
        let _e437 = fogAmount;
        let _e439 = mix(_e432.xyz, _e435.xyz, vec3(_e437));
        base[0u] = _e439.x;
        base[1u] = _e439.y;
        base[2u] = _e439.z;
    }
    if override_type_4_14 {
        let _e447 = base[3u];
        if (_e447 == 0f) {
            discard;
        }
    } else {
        if override_type_4_15 {
            let _e449 = base;
            let _e451 = base;
            if (dot(_e449.xyz, _e451.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e455 = base;
    out_color = _e455;
    param_28 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_28));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e25 = out_temporal_velocity;
    let _e26 = out_temporal_validity;
    let _e27 = out_color;
    return FragmentOutput(_e25, _e26, _e27);
}
