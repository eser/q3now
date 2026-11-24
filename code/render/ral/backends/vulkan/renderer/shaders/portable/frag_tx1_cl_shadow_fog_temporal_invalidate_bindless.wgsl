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
    @location(2) member_1: vec4<f32>,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_6_: bool = (shadow_pcf <= 1i);
override override_type_6_1: bool = (shadow_pcf <= 5i);
override override_type_6_2: bool = (lightmap_slot == 1i);
override override_type_6_3: bool = (lightmap_slot == 2i);
@id(6) override tex_mode: i32 = 0i;
override override_type_6_4: bool = (tex_mode == 1i);
override override_type_6_5: bool = (tex_mode == 2i);
override override_type_6_6: bool = (override_type_6_4 || override_type_6_5);
override override_type_6_7: bool = (tex_mode == 3i);
override override_type_6_8: bool = (tex_mode == 4i);
override override_type_6_9: bool = (tex_mode == 5i);
override override_type_6_10: bool = (tex_mode == 6i);
override override_type_6_11: bool = (tex_mode == 7i);
override override_type_6_12: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_6_13: bool = (acff == 1i);
override override_type_6_14: bool = (acff == 2i);
override override_type_6_15: bool = (acff == 3i);
override override_type_6_16: bool = (acff == 1i);
override override_type_6_17: bool = (acff == 2i);
override override_type_6_18: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_19: bool = (discard_mode == 1i);
override override_type_6_20: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e104 = (*sceneColor)[0u];
    let _e106 = (*sceneColor)[1u];
    let _e108 = (*sceneColor)[2u];
    let _e113 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e104, max(_e106, _e108)), 0f, 1f), 0f, 0f, clamp(_e113, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e104 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e104 + 0.5f));
    let _e109 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e111 = fogType;
    let _e114 = fogType;
    return (((_e109 > 0.5f) && (_e111 >= 1i)) && (_e114 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e104 = wired_advanced_fog_enabled_u0028_();
    if !(_e104) {
        return 0f;
    }
    let _e107 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e107, 0.000001f));
    let _e112 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e112 + 0.5f));
    let _e115 = fogType_1;
    if (_e115 == 1i) {
        let _e119 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e119 <= 0f) {
            return 0f;
        }
        let _e121 = viewDepth;
        let _e124 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e121 / _e124), 0f, 1f);
    }
    let _e129 = unnamed.advancedFogColorDensity[3u];
    let _e131 = viewDepth;
    opticalDepth = (max(_e129, 0f) * _e131);
    let _e133 = fogType_1;
    if (_e133 == 2i) {
        let _e135 = opticalDepth;
        return clamp((1f - exp(-(_e135))), 0f, 1f);
    }
    let _e140 = opticalDepth;
    let _e141 = opticalDepth;
    return clamp((1f - exp(-((_e140 * _e141)))), 0f, 1f);
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
    var phi_355_: bool;
    var phi_362_: bool;
    var phi_369_: bool;
    var phi_376_: bool;

    let _e111 = (*c);
    let _e114 = unnamed.cascadeMVP[_e111];
    let _e115 = (*worldPos);
    sc4_ = (_e114 * vec4<f32>(_e115.x, _e115.y, _e115.z, 1f));
    let _e121 = sc4_;
    let _e124 = sc4_[3u];
    sc = (_e121.xyz / vec3(_e124));
    let _e127 = sc;
    let _e131 = ((_e127.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e131.x;
    sc[1u] = _e131.y;
    let _e137 = sc[0u];
    let _e138 = (_e137 < 0f);
    phi_355_ = _e138;
    if !(_e138) {
        let _e141 = sc[0u];
        phi_355_ = (_e141 > 1f);
    }
    let _e144 = phi_355_;
    phi_362_ = _e144;
    if !(_e144) {
        let _e147 = sc[1u];
        phi_362_ = (_e147 < 0f);
    }
    let _e150 = phi_362_;
    phi_369_ = _e150;
    if !(_e150) {
        let _e153 = sc[1u];
        phi_369_ = (_e153 > 1f);
    }
    let _e156 = phi_369_;
    phi_376_ = _e156;
    if !(_e156) {
        let _e159 = sc[2u];
        phi_376_ = (_e159 > 1f);
    }
    let _e162 = phi_376_;
    if _e162 {
        return 1f;
    }
    let _e163 = (*c);
    layer = f32(_e163);
    let _e165 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e165).xy));
    let _e172 = sc[2u];
    currentDepth = (_e172 - shadow_bias);
    shadow = 0f;
    if override_type_6_ {
        let _e174 = currentDepth;
        let _e175 = sc;
        let _e176 = _e175.xy;
        let _e177 = layer;
        let _e180 = vec3<f32>(_e176.x, _e176.y, _e177);
        let _e186 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e180.x, _e180.y), i32(_e180.z));
        shadow = step(_e174, _e186.x);
    } else {
        if override_type_6_1 {
            let _e189 = currentDepth;
            let _e190 = sc;
            let _e191 = _e190.xy;
            let _e192 = layer;
            let _e195 = vec3<f32>(_e191.x, _e191.y, _e192);
            let _e201 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e195.x, _e195.y), i32(_e195.z));
            let _e204 = shadow;
            shadow = (_e204 + step(_e189, _e201.x));
            let _e206 = currentDepth;
            let _e207 = sc;
            let _e210 = texelSize[0u];
            let _e212 = (_e207.xy + vec2<f32>(_e210, 0f));
            let _e213 = layer;
            let _e216 = vec3<f32>(_e212.x, _e212.y, _e213);
            let _e222 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e216.x, _e216.y), i32(_e216.z));
            let _e225 = shadow;
            shadow = (_e225 + step(_e206, _e222.x));
            let _e227 = currentDepth;
            let _e228 = sc;
            let _e231 = texelSize[0u];
            let _e233 = (_e228.xy - vec2<f32>(_e231, 0f));
            let _e234 = layer;
            let _e237 = vec3<f32>(_e233.x, _e233.y, _e234);
            let _e243 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e237.x, _e237.y), i32(_e237.z));
            let _e246 = shadow;
            shadow = (_e246 + step(_e227, _e243.x));
            let _e248 = currentDepth;
            let _e249 = sc;
            let _e252 = texelSize[1u];
            let _e254 = (_e249.xy + vec2<f32>(0f, _e252));
            let _e255 = layer;
            let _e258 = vec3<f32>(_e254.x, _e254.y, _e255);
            let _e264 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e258.x, _e258.y), i32(_e258.z));
            let _e267 = shadow;
            shadow = (_e267 + step(_e248, _e264.x));
            let _e269 = currentDepth;
            let _e270 = sc;
            let _e273 = texelSize[1u];
            let _e275 = (_e270.xy - vec2<f32>(0f, _e273));
            let _e276 = layer;
            let _e279 = vec3<f32>(_e275.x, _e275.y, _e276);
            let _e285 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e279.x, _e279.y), i32(_e279.z));
            let _e288 = shadow;
            shadow = (_e288 + step(_e269, _e285.x));
            let _e290 = shadow;
            shadow = (_e290 / 5f);
        } else {
            x = -1i;
            loop {
                let _e292 = x;
                if (_e292 <= 1i) {
                    y = -1i;
                    loop {
                        let _e294 = y;
                        if (_e294 <= 1i) {
                            let _e296 = currentDepth;
                            let _e297 = sc;
                            let _e299 = x;
                            let _e301 = y;
                            let _e304 = texelSize;
                            let _e306 = (_e297.xy + (vec2<f32>(f32(_e299), f32(_e301)) * _e304));
                            let _e307 = layer;
                            let _e310 = vec3<f32>(_e306.x, _e306.y, _e307);
                            let _e316 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e310.x, _e310.y), i32(_e310.z));
                            let _e319 = shadow;
                            shadow = (_e319 + step(_e296, _e316.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e321 = y;
                            y = (_e321 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e323 = x;
                    x = (_e323 + 1i);
                }
            }
            let _e325 = shadow;
            shadow = (_e325 / 9f);
        }
    }
    let _e327 = shadow;
    return _e327;
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

    let _e118 = unnamed.cascadeSplits;
    let _e119 = (*viewDepth_1);
    cmp = step(_e118, vec4(_e119));
    let _e123 = cmp[0u];
    let _e125 = cmp[1u];
    let _e128 = cmp[2u];
    let _e131 = cmp[3u];
    cascade = min(i32((((_e123 + _e125) + _e128) + _e131)), 3i);
    let _e135 = cascade;
    (*outCascade) = _e135;
    let _e136 = cascade;
    if (_e136 == 0i) {
        local = 0f;
    } else {
        let _e138 = cascade;
        let _e143 = unnamed.cascadeSplits[max((_e138 - 1i), 0i)];
        local = _e143;
    }
    let _e144 = local;
    prevSplit = _e144;
    let _e145 = cascade;
    let _e148 = unnamed.cascadeSplits[_e145];
    farSplit = _e148;
    let _e149 = farSplit;
    let _e150 = prevSplit;
    blendRange = max((0.1f * (_e149 - _e150)), 1f);
    let _e154 = farSplit;
    let _e155 = (*viewDepth_1);
    let _e157 = blendRange;
    blendT = clamp(((_e154 - _e155) / _e157), 0f, 1f);
    let _e160 = cascade;
    param = _e160;
    let _e161 = (*worldPos_1);
    param_1 = _e161;
    let _e162 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e162;
    let _e163 = cascade;
    param_2 = min((_e163 + 1i), 3i);
    let _e166 = (*worldPos_1);
    param_3 = _e166;
    let _e167 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e167;
    let _e168 = s1_;
    let _e169 = s0_;
    let _e170 = blendT;
    return mix(_e168, _e169, _e170);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e112 = unnamed.packed_indices[1i][3u];
    let _e118 = unnamed.packed_indices[1i][3u];
    let _e123 = (*lm_uv);
    let _e124 = textureSample(wired_bindless_images[(_e112 & 4095u)], wired_bindless_samplers[((_e118 >> bitcast<u32>(12i)) & 255u)], _e123);
    sun_mask = _e124.x;
    let _e126 = sun_mask;
    if (_e126 > 0.001f) {
        let _e128 = shadowData_1;
        param_4 = _e128.xyz;
        let _e131 = shadowData_1[3u];
        param_5 = _e131;
        let _e132 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e133 = param_6;
        ignoredCascade = _e133;
        shadow_1 = _e132;
        let _e134 = shadow_1;
        let _e135 = sun_mask;
        let _e137 = (*rgb);
        (*rgb) = (_e137 * mix(1f, _e134, _e135));
    }
    let _e139 = (*rgb);
    return _e139;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_6_2 {
        let _e106 = (*rgb_1);
        param_7 = _e106;
        let _e107 = frag_tex_coord0_1;
        param_8 = _e107;
        let _e108 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e108;
    }
    if override_type_6_3 {
        let _e109 = (*rgb_1);
        param_9 = _e109;
        let _e110 = frag_tex_coord1_1;
        param_10 = _e110;
        let _e111 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e111;
    }
    let _e112 = (*rgb_1);
    return _e112;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e104 = (*rgb_2);
    let _e107 = unnamed.worldLightParams[0u];
    boosted = (_e104 * _e107);
    let _e110 = boosted[0u];
    let _e112 = boosted[1u];
    let _e114 = boosted[2u];
    peak = max(_e110, max(_e112, _e114));
    let _e117 = peak;
    if (_e117 > 1f) {
        let _e119 = peak;
        let _e120 = boosted;
        boosted = (_e120 / vec3(_e119));
    }
    let _e123 = boosted;
    return _e123;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e105 = (*c_1);
    (*c_1) = max(_e105, vec3<f32>(0f, 0f, 0f));
    let _e107 = (*c_1);
    cutoff = (_e107 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e109 = (*c_1);
    lo = (_e109 / vec3(12.92f));
    let _e112 = (*c_1);
    hi = pow(((_e112 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e117 = hi;
    let _e118 = lo;
    let _e119 = cutoff;
    return mix(_e117, _e118, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e119));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

    let _e107 = (*role);
    let _e109 = (*role);
    let _e114 = unnamed.packed_indices[(_e107 / 4u)][(_e109 % 4u)];
    let _e117 = (*role);
    let _e119 = (*role);
    let _e124 = unnamed.packed_indices[(_e117 / 4u)][(_e119 % 4u)];
    let _e129 = (*uv);
    let _e130 = textureSample(wired_bindless_images[(_e114 & 4095u)], wired_bindless_samplers[((_e124 >> bitcast<u32>(12i)) & 255u)], _e129);
    c_2 = _e130;
    let _e131 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e131))) == 0i) {
        let _e136 = c_2;
        param_11 = _e136.xyz;
        let _e138 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e138.x;
        c_2[1u] = _e138.y;
        c_2[2u] = _e138.z;
    }
    let _e145 = (*slot);
    if (lightmap_slot == (_e145 + 1i)) {
        let _e148 = c_2;
        param_12 = _e148.xyz;
        let _e150 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e150.x;
        c_2[1u] = _e150.y;
        c_2[2u] = _e150.z;
    }
    let _e157 = c_2;
    return _e157;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_13: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_14: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var sampledColor1_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var param_21: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_22: f32;
    var param_23: vec4<f32>;

    let _e133 = unnamed.packed_indices[0i][3u];
    let _e139 = unnamed.packed_indices[0i][3u];
    let _e144 = fog_tex_coord_1;
    let _e145 = textureSample(wired_bindless_images[(_e133 & 4095u)], wired_bindless_samplers[((_e139 >> bitcast<u32>(12i)) & 255u)], _e144);
    fog = _e145;
    let _e146 = frag_color0In_1;
    param_13 = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e150 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e148.x, _e148.y, _e148.z, _e150);
    let _e155 = frag_color1In_1;
    param_14 = _e155.xyz;
    let _e157 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e159 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e157.x, _e157.y, _e157.z, _e159);
    param_15 = 0u;
    let _e164 = frag_tex_coord0_1;
    param_16 = _e164;
    param_17 = 0i;
    let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
    sampledColor0_ = _e165;
    param_18 = 1u;
    let _e166 = frag_tex_coord1_1;
    param_19 = _e166;
    param_20 = 1i;
    let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor1_ = _e167;
    let _e168 = sampledColor0_;
    let _e169 = frag_color0_;
    color0_ = (_e168 * _e169);
    if override_type_6_6 {
        let _e171 = sampledColor1_;
        let _e172 = frag_color1_;
        color1_ = (_e171 * _e172);
        let _e174 = color0_;
        let _e176 = color1_;
        let _e178 = (_e174.xyz + _e176.xyz);
        let _e180 = color0_[3u];
        let _e182 = color1_[3u];
        base = vec4<f32>(_e178.x, _e178.y, _e178.z, (_e180 * _e182));
    } else {
        if override_type_6_7 {
            let _e188 = sampledColor1_;
            let _e189 = frag_color1_;
            color1_1 = (_e188 * _e189);
            let _e192 = color0_[3u];
            let _e193 = color0_;
            color0_ = (_e193 * _e192);
            let _e196 = color1_1[3u];
            let _e197 = color1_1;
            color1_1 = (_e197 * _e196);
            let _e199 = color0_;
            let _e201 = color1_1;
            let _e203 = (_e199.xyz + _e201.xyz);
            let _e205 = color0_[3u];
            let _e207 = color1_1[3u];
            base = vec4<f32>(_e203.x, _e203.y, _e203.z, (_e205 * _e207));
        } else {
            if override_type_6_8 {
                let _e213 = sampledColor1_;
                let _e214 = frag_color1_;
                color1_2 = (_e213 * _e214);
                let _e217 = color0_[3u];
                let _e219 = color0_;
                color0_ = (_e219 * (1f - _e217));
                let _e222 = color1_2[3u];
                let _e224 = color1_2;
                color1_2 = (_e224 * (1f - _e222));
                let _e226 = color0_;
                let _e228 = color1_2;
                let _e230 = (_e226.xyz + _e228.xyz);
                let _e232 = color0_[3u];
                let _e234 = color1_2[3u];
                base = vec4<f32>(_e230.x, _e230.y, _e230.z, (_e232 * _e234));
            } else {
                if override_type_6_9 {
                    let _e240 = sampledColor1_;
                    let _e241 = frag_color1_;
                    color1_3 = (_e240 * _e241);
                    let _e243 = color0_;
                    let _e244 = color1_3;
                    let _e246 = color1_3[3u];
                    base = mix(_e243, _e244, vec4(_e246));
                } else {
                    if override_type_6_10 {
                        let _e249 = sampledColor1_;
                        let _e250 = frag_color1_;
                        color1_4 = (_e249 * _e250);
                        let _e252 = color1_4;
                        let _e253 = color0_;
                        let _e255 = color1_4[3u];
                        base = mix(_e252, _e253, vec4(_e255));
                    } else {
                        if override_type_6_11 {
                            let _e258 = sampledColor1_;
                            let _e259 = frag_color1_;
                            color1_5 = (_e258 * _e259);
                            let _e261 = color1_5;
                            let _e263 = color1_5[3u];
                            let _e266 = color0_;
                            base = ((_e261 + vec4(_e263)) * _e266);
                        } else {
                            let _e268 = sampledColor1_;
                            let _e269 = frag_color1_;
                            color1_6 = (_e268 * _e269);
                            let _e271 = color0_;
                            let _e273 = color1_6;
                            let _e275 = (_e271.xyz * _e273.xyz);
                            base[0u] = _e275.x;
                            base[1u] = _e275.y;
                            base[2u] = _e275.z;
                            let _e283 = color0_[3u];
                            let _e285 = color1_6[3u];
                            base[3u] = (_e283 * _e285);
                        }
                    }
                }
            }
        }
    }
    let _e288 = base;
    param_21 = _e288.xyz;
    let _e290 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_21));
    base[0u] = _e290.x;
    base[1u] = _e290.y;
    base[2u] = _e290.z;
    if override_type_6_12 {
        let _e299 = unnamed.worldLightParams[1u];
        wetness = clamp(_e299, 0f, 1f);
        let _e303 = unnamed.worldLightParams[2u];
        frost = clamp(_e303, 0f, 1f);
        let _e305 = base;
        luminance = dot(_e305.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e308 = wetness;
        let _e310 = base;
        let _e312 = (_e310.xyz * mix(1f, 0.82f, _e308));
        base[0u] = _e312.x;
        base[1u] = _e312.y;
        base[2u] = _e312.z;
        let _e319 = base;
        let _e321 = luminance;
        let _e323 = luminance;
        let _e325 = luminance;
        let _e327 = frost;
        let _e330 = mix(_e319.xyz, vec3<f32>((_e321 * 0.88f), (_e323 * 0.94f), _e325), vec3((_e327 * 0.55f)));
        base[0u] = _e330.x;
        base[1u] = _e330.y;
        base[2u] = _e330.z;
    }
    let _e339 = unnamed.materialFactors[0u];
    if (_e339 >= 0f) {
        let _e342 = unnamed.materialBaseColor;
        let _e343 = base;
        base = (_e343 * _e342);
    }
    let _e345 = color0_;
    let _e348 = unnamed.emissionRadiance;
    let _e351 = base;
    let _e353 = (_e351.xyz + (_e345.xyz * _e348.xyz));
    base[0u] = _e353.x;
    base[1u] = _e353.y;
    base[2u] = _e353.z;
    let _e360 = wired_advanced_fog_enabled_u0028_();
    if _e360 {
        let _e361 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e361;
        if override_type_6_13 {
            let _e362 = fogAmount;
            let _e364 = base;
            let _e366 = (_e364.xyz * (1f - _e362));
            base[0u] = _e366.x;
            base[1u] = _e366.y;
            base[2u] = _e366.z;
        } else {
            if override_type_6_14 {
                let _e373 = fogAmount;
                let _e375 = base;
                base = (_e375 * (1f - _e373));
            } else {
                if override_type_6_15 {
                    let _e377 = fogAmount;
                    let _e380 = base[3u];
                    base[3u] = (_e380 * (1f - _e377));
                } else {
                    let _e383 = base;
                    let _e386 = unnamed.advancedFogColorDensity;
                    let _e388 = fogAmount;
                    let _e390 = mix(_e383.xyz, _e386.xyz, vec3(_e388));
                    base[0u] = _e390.x;
                    base[1u] = _e390.y;
                    base[2u] = _e390.z;
                }
            }
        }
    } else {
        if override_type_6_16 {
            let _e397 = base;
            let _e400 = fog[3u];
            let _e402 = (_e397.xyz * (1f - _e400));
            base[0u] = _e402.x;
            base[1u] = _e402.y;
            base[2u] = _e402.z;
        } else {
            if override_type_6_17 {
                let _e409 = base;
                let _e411 = fog[3u];
                base = (_e409 * (1f - _e411));
            } else {
                if override_type_6_18 {
                    let _e415 = base[3u];
                    let _e417 = fog[3u];
                    base[3u] = (_e415 * (1f - _e417));
                } else {
                    let _e421 = base;
                    let _e422 = fog;
                    let _e424 = unnamed.fogColor;
                    let _e427 = fog[3u];
                    base = mix(_e421, (_e422 * _e424), vec4(_e427));
                }
            }
        }
    }
    if override_type_6_19 {
        let _e431 = base[3u];
        if (_e431 == 0f) {
            discard;
        }
    } else {
        if override_type_6_20 {
            let _e433 = base;
            let _e435 = base;
            if (dot(_e433.xyz, _e435.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e439 = base;
    out_color = _e439;
    param_22 = 1f;
    let _e440 = base;
    param_23 = _e440;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_22), (&param_23));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
