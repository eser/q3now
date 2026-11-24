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
override override_type_6_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_6_5: bool = (tex_mode == 1i);
override override_type_6_6: bool = (tex_mode == 2i);
override override_type_6_7: bool = (override_type_6_5 || override_type_6_6);
override override_type_6_8: bool = (tex_mode == 3i);
override override_type_6_9: bool = (tex_mode == 4i);
override override_type_6_10: bool = (tex_mode == 5i);
override override_type_6_11: bool = (tex_mode == 6i);
override override_type_6_12: bool = (tex_mode == 7i);
override override_type_6_13: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_14: bool = (discard_mode == 1i);
override override_type_6_15: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e99 = (*sceneColor)[0u];
    let _e101 = (*sceneColor)[1u];
    let _e103 = (*sceneColor)[2u];
    let _e108 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e99, max(_e101, _e103)), 0f, 1f), 0f, 0f, clamp(_e108, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e99 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e99 + 0.5f));
    let _e104 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e106 = fogType;
    let _e109 = fogType;
    return (((_e104 > 0.5f) && (_e106 >= 1i)) && (_e109 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e99 = wired_advanced_fog_enabled_u0028_();
    if !(_e99) {
        return 0f;
    }
    let _e102 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e102, 0.000001f));
    let _e107 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e107 + 0.5f));
    let _e110 = fogType_1;
    if (_e110 == 1i) {
        let _e114 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e114 <= 0f) {
            return 0f;
        }
        let _e116 = viewDepth;
        let _e119 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e116 / _e119), 0f, 1f);
    }
    let _e124 = unnamed.advancedFogColorDensity[3u];
    let _e126 = viewDepth;
    opticalDepth = (max(_e124, 0f) * _e126);
    let _e128 = fogType_1;
    if (_e128 == 2i) {
        let _e130 = opticalDepth;
        return clamp((1f - exp(-(_e130))), 0f, 1f);
    }
    let _e135 = opticalDepth;
    let _e136 = opticalDepth;
    return clamp((1f - exp(-((_e135 * _e136)))), 0f, 1f);
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

    let _e106 = (*c);
    let _e109 = unnamed.cascadeMVP[_e106];
    let _e110 = (*worldPos);
    sc4_ = (_e109 * vec4<f32>(_e110.x, _e110.y, _e110.z, 1f));
    let _e116 = sc4_;
    let _e119 = sc4_[3u];
    sc = (_e116.xyz / vec3(_e119));
    let _e122 = sc;
    let _e126 = ((_e122.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e126.x;
    sc[1u] = _e126.y;
    let _e132 = sc[0u];
    let _e133 = (_e132 < 0f);
    phi_355_ = _e133;
    if !(_e133) {
        let _e136 = sc[0u];
        phi_355_ = (_e136 > 1f);
    }
    let _e139 = phi_355_;
    phi_362_ = _e139;
    if !(_e139) {
        let _e142 = sc[1u];
        phi_362_ = (_e142 < 0f);
    }
    let _e145 = phi_362_;
    phi_369_ = _e145;
    if !(_e145) {
        let _e148 = sc[1u];
        phi_369_ = (_e148 > 1f);
    }
    let _e151 = phi_369_;
    phi_376_ = _e151;
    if !(_e151) {
        let _e154 = sc[2u];
        phi_376_ = (_e154 > 1f);
    }
    let _e157 = phi_376_;
    if _e157 {
        return 1f;
    }
    let _e158 = (*c);
    layer = f32(_e158);
    let _e160 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e160).xy));
    let _e167 = sc[2u];
    currentDepth = (_e167 - shadow_bias);
    shadow = 0f;
    if override_type_6_ {
        let _e169 = currentDepth;
        let _e170 = sc;
        let _e171 = _e170.xy;
        let _e172 = layer;
        let _e175 = vec3<f32>(_e171.x, _e171.y, _e172);
        let _e181 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e175.x, _e175.y), i32(_e175.z));
        shadow = step(_e169, _e181.x);
    } else {
        if override_type_6_1 {
            let _e184 = currentDepth;
            let _e185 = sc;
            let _e186 = _e185.xy;
            let _e187 = layer;
            let _e190 = vec3<f32>(_e186.x, _e186.y, _e187);
            let _e196 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e190.x, _e190.y), i32(_e190.z));
            let _e199 = shadow;
            shadow = (_e199 + step(_e184, _e196.x));
            let _e201 = currentDepth;
            let _e202 = sc;
            let _e205 = texelSize[0u];
            let _e207 = (_e202.xy + vec2<f32>(_e205, 0f));
            let _e208 = layer;
            let _e211 = vec3<f32>(_e207.x, _e207.y, _e208);
            let _e217 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e211.x, _e211.y), i32(_e211.z));
            let _e220 = shadow;
            shadow = (_e220 + step(_e201, _e217.x));
            let _e222 = currentDepth;
            let _e223 = sc;
            let _e226 = texelSize[0u];
            let _e228 = (_e223.xy - vec2<f32>(_e226, 0f));
            let _e229 = layer;
            let _e232 = vec3<f32>(_e228.x, _e228.y, _e229);
            let _e238 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e232.x, _e232.y), i32(_e232.z));
            let _e241 = shadow;
            shadow = (_e241 + step(_e222, _e238.x));
            let _e243 = currentDepth;
            let _e244 = sc;
            let _e247 = texelSize[1u];
            let _e249 = (_e244.xy + vec2<f32>(0f, _e247));
            let _e250 = layer;
            let _e253 = vec3<f32>(_e249.x, _e249.y, _e250);
            let _e259 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e253.x, _e253.y), i32(_e253.z));
            let _e262 = shadow;
            shadow = (_e262 + step(_e243, _e259.x));
            let _e264 = currentDepth;
            let _e265 = sc;
            let _e268 = texelSize[1u];
            let _e270 = (_e265.xy - vec2<f32>(0f, _e268));
            let _e271 = layer;
            let _e274 = vec3<f32>(_e270.x, _e270.y, _e271);
            let _e280 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e274.x, _e274.y), i32(_e274.z));
            let _e283 = shadow;
            shadow = (_e283 + step(_e264, _e280.x));
            let _e285 = shadow;
            shadow = (_e285 / 5f);
        } else {
            x = -1i;
            loop {
                let _e287 = x;
                if (_e287 <= 1i) {
                    y = -1i;
                    loop {
                        let _e289 = y;
                        if (_e289 <= 1i) {
                            let _e291 = currentDepth;
                            let _e292 = sc;
                            let _e294 = x;
                            let _e296 = y;
                            let _e299 = texelSize;
                            let _e301 = (_e292.xy + (vec2<f32>(f32(_e294), f32(_e296)) * _e299));
                            let _e302 = layer;
                            let _e305 = vec3<f32>(_e301.x, _e301.y, _e302);
                            let _e311 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e305.x, _e305.y), i32(_e305.z));
                            let _e314 = shadow;
                            shadow = (_e314 + step(_e291, _e311.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e316 = y;
                            y = (_e316 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e318 = x;
                    x = (_e318 + 1i);
                }
            }
            let _e320 = shadow;
            shadow = (_e320 / 9f);
        }
    }
    let _e322 = shadow;
    return _e322;
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

    let _e113 = unnamed.cascadeSplits;
    let _e114 = (*viewDepth_1);
    cmp = step(_e113, vec4(_e114));
    let _e118 = cmp[0u];
    let _e120 = cmp[1u];
    let _e123 = cmp[2u];
    let _e126 = cmp[3u];
    cascade = min(i32((((_e118 + _e120) + _e123) + _e126)), 3i);
    let _e130 = cascade;
    (*outCascade) = _e130;
    let _e131 = cascade;
    if (_e131 == 0i) {
        local = 0f;
    } else {
        let _e133 = cascade;
        let _e138 = unnamed.cascadeSplits[max((_e133 - 1i), 0i)];
        local = _e138;
    }
    let _e139 = local;
    prevSplit = _e139;
    let _e140 = cascade;
    let _e143 = unnamed.cascadeSplits[_e140];
    farSplit = _e143;
    let _e144 = farSplit;
    let _e145 = prevSplit;
    blendRange = max((0.1f * (_e144 - _e145)), 1f);
    let _e149 = farSplit;
    let _e150 = (*viewDepth_1);
    let _e152 = blendRange;
    blendT = clamp(((_e149 - _e150) / _e152), 0f, 1f);
    let _e155 = cascade;
    param = _e155;
    let _e156 = (*worldPos_1);
    param_1 = _e156;
    let _e157 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e157;
    let _e158 = cascade;
    param_2 = min((_e158 + 1i), 3i);
    let _e161 = (*worldPos_1);
    param_3 = _e161;
    let _e162 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e162;
    let _e163 = s1_;
    let _e164 = s0_;
    let _e165 = blendT;
    return mix(_e163, _e164, _e165);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e107 = unnamed.packed_indices[1i][3u];
    let _e113 = unnamed.packed_indices[1i][3u];
    let _e118 = (*lm_uv);
    let _e119 = textureSample(wired_bindless_images[(_e107 & 4095u)], wired_bindless_samplers[((_e113 >> bitcast<u32>(12i)) & 255u)], _e118);
    sun_mask = _e119.x;
    let _e121 = sun_mask;
    if (_e121 > 0.001f) {
        let _e123 = shadowData_1;
        param_4 = _e123.xyz;
        let _e126 = shadowData_1[3u];
        param_5 = _e126;
        let _e127 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e128 = param_6;
        ignoredCascade = _e128;
        shadow_1 = _e127;
        let _e129 = shadow_1;
        let _e130 = sun_mask;
        let _e132 = (*rgb);
        (*rgb) = (_e132 * mix(1f, _e129, _e130));
    }
    let _e134 = (*rgb);
    return _e134;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_6_2 {
        let _e103 = (*rgb_1);
        param_7 = _e103;
        let _e104 = frag_tex_coord0_1;
        param_8 = _e104;
        let _e105 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e105;
    }
    if override_type_6_3 {
        let _e106 = (*rgb_1);
        param_9 = _e106;
        let _e107 = frag_tex_coord1_1;
        param_10 = _e107;
        let _e108 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e108;
    }
    if override_type_6_4 {
        let _e109 = (*rgb_1);
        param_11 = _e109;
        let _e110 = frag_tex_coord2_1;
        param_12 = _e110;
        let _e111 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e111;
    }
    let _e112 = (*rgb_1);
    return _e112;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e99 = (*rgb_2);
    let _e102 = unnamed.worldLightParams[0u];
    boosted = (_e99 * _e102);
    let _e105 = boosted[0u];
    let _e107 = boosted[1u];
    let _e109 = boosted[2u];
    peak = max(_e105, max(_e107, _e109));
    let _e112 = peak;
    if (_e112 > 1f) {
        let _e114 = peak;
        let _e115 = boosted;
        boosted = (_e115 / vec3(_e114));
    }
    let _e118 = boosted;
    return _e118;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e100 = (*c_1);
    (*c_1) = max(_e100, vec3<f32>(0f, 0f, 0f));
    let _e102 = (*c_1);
    cutoff = (_e102 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e104 = (*c_1);
    lo = (_e104 / vec3(12.92f));
    let _e107 = (*c_1);
    hi = pow(((_e107 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e112 = hi;
    let _e113 = lo;
    let _e114 = cutoff;
    return mix(_e112, _e113, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e114));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e102 = (*role);
    let _e104 = (*role);
    let _e109 = unnamed.packed_indices[(_e102 / 4u)][(_e104 % 4u)];
    let _e112 = (*role);
    let _e114 = (*role);
    let _e119 = unnamed.packed_indices[(_e112 / 4u)][(_e114 % 4u)];
    let _e124 = (*uv);
    let _e125 = textureSample(wired_bindless_images[(_e109 & 4095u)], wired_bindless_samplers[((_e119 >> bitcast<u32>(12i)) & 255u)], _e124);
    c_2 = _e125;
    let _e126 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e126))) == 0i) {
        let _e131 = c_2;
        param_13 = _e131.xyz;
        let _e133 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e133.x;
        c_2[1u] = _e133.y;
        c_2[2u] = _e133.z;
    }
    let _e140 = (*slot);
    if (lightmap_slot == (_e140 + 1i)) {
        let _e143 = c_2;
        param_14 = _e143.xyz;
        let _e145 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e145.x;
        c_2[1u] = _e145.y;
        c_2[2u] = _e145.z;
    }
    let _e152 = c_2;
    return _e152;
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
    var param_29: vec4<f32>;

    let _e137 = frag_color0In_1;
    param_15 = _e137.xyz;
    let _e139 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e141 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e139.x, _e139.y, _e139.z, _e141);
    let _e146 = frag_color1In_1;
    param_16 = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e150 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e148.x, _e148.y, _e148.z, _e150);
    let _e155 = frag_color2In_1;
    param_17 = _e155.xyz;
    let _e157 = sRGBToLinear_u0028_vf3_u003b((&param_17));
    let _e159 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e157.x, _e157.y, _e157.z, _e159);
    param_18 = 0u;
    let _e164 = frag_tex_coord0_1;
    param_19 = _e164;
    param_20 = 0i;
    let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e165;
    param_21 = 1u;
    let _e166 = frag_tex_coord1_1;
    param_22 = _e166;
    param_23 = 1i;
    let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e167;
    param_24 = 2u;
    let _e168 = frag_tex_coord2_1;
    param_25 = _e168;
    param_26 = 2i;
    let _e169 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
    sampledColor2_ = _e169;
    let _e170 = sampledColor0_;
    let _e171 = frag_color0_;
    color0_ = (_e170 * _e171);
    if override_type_6_7 {
        let _e173 = sampledColor1_;
        let _e174 = frag_color1_;
        color1_ = (_e173 * _e174);
        let _e176 = sampledColor2_;
        let _e177 = frag_color2_;
        color2_ = (_e176 * _e177);
        let _e179 = color0_;
        let _e181 = color1_;
        let _e184 = color2_;
        let _e186 = ((_e179.xyz + _e181.xyz) + _e184.xyz);
        let _e188 = color0_[3u];
        let _e190 = color1_[3u];
        let _e193 = color2_[3u];
        base = vec4<f32>(_e186.x, _e186.y, _e186.z, ((_e188 * _e190) * _e193));
    } else {
        if override_type_6_8 {
            let _e199 = sampledColor1_;
            let _e200 = frag_color1_;
            color1_1 = (_e199 * _e200);
            let _e202 = sampledColor2_;
            let _e203 = frag_color2_;
            color2_1 = (_e202 * _e203);
            let _e206 = color0_[3u];
            let _e207 = color0_;
            color0_ = (_e207 * _e206);
            let _e210 = color1_1[3u];
            let _e211 = color1_1;
            color1_1 = (_e211 * _e210);
            let _e214 = color2_1[3u];
            let _e215 = color2_1;
            color2_1 = (_e215 * _e214);
            let _e217 = color0_;
            let _e219 = color1_1;
            let _e222 = color2_1;
            let _e224 = ((_e217.xyz + _e219.xyz) + _e222.xyz);
            let _e226 = color0_[3u];
            let _e228 = color1_1[3u];
            let _e231 = color2_1[3u];
            base = vec4<f32>(_e224.x, _e224.y, _e224.z, ((_e226 * _e228) * _e231));
        } else {
            if override_type_6_9 {
                let _e237 = sampledColor1_;
                let _e238 = frag_color1_;
                color1_2 = (_e237 * _e238);
                let _e240 = sampledColor2_;
                let _e241 = frag_color2_;
                color2_2 = (_e240 * _e241);
                let _e244 = color0_[3u];
                let _e246 = color0_;
                color0_ = (_e246 * (1f - _e244));
                let _e249 = color1_2[3u];
                let _e251 = color1_2;
                color1_2 = (_e251 * (1f - _e249));
                let _e254 = color2_2[3u];
                let _e256 = color2_2;
                color2_2 = (_e256 * (1f - _e254));
                let _e258 = color0_;
                let _e260 = color1_2;
                let _e263 = color2_2;
                let _e265 = ((_e258.xyz + _e260.xyz) + _e263.xyz);
                let _e267 = color0_[3u];
                let _e269 = color1_2[3u];
                let _e272 = color2_2[3u];
                base = vec4<f32>(_e265.x, _e265.y, _e265.z, ((_e267 * _e269) * _e272));
            } else {
                if override_type_6_10 {
                    let _e278 = sampledColor1_;
                    let _e279 = frag_color1_;
                    color1_3 = (_e278 * _e279);
                    let _e281 = sampledColor2_;
                    let _e282 = frag_color2_;
                    color2_3 = (_e281 * _e282);
                    let _e284 = color0_;
                    let _e285 = color1_3;
                    let _e287 = color1_3[3u];
                    let _e290 = color2_3;
                    let _e292 = color2_3[3u];
                    base = mix(mix(_e284, _e285, vec4(_e287)), _e290, vec4(_e292));
                } else {
                    if override_type_6_11 {
                        let _e295 = sampledColor1_;
                        let _e296 = frag_color1_;
                        color1_4 = (_e295 * _e296);
                        let _e298 = sampledColor2_;
                        let _e299 = frag_color2_;
                        color2_4 = (_e298 * _e299);
                        let _e301 = color2_4;
                        let _e302 = color1_4;
                        let _e303 = color0_;
                        let _e305 = color1_4[3u];
                        let _e309 = color2_4[3u];
                        base = mix(_e301, mix(_e302, _e303, vec4(_e305)), vec4(_e309));
                    } else {
                        if override_type_6_12 {
                            let _e312 = sampledColor1_;
                            let _e313 = frag_color1_;
                            color1_5 = (_e312 * _e313);
                            let _e315 = sampledColor2_;
                            let _e316 = frag_color2_;
                            color2_5 = (_e315 * _e316);
                            let _e318 = color2_5;
                            let _e320 = color2_5[3u];
                            let _e323 = color1_5;
                            let _e325 = color1_5[3u];
                            let _e329 = color0_;
                            base = (((_e318 + vec4(_e320)) * (_e323 + vec4(_e325))) * _e329);
                        } else {
                            let _e331 = sampledColor1_;
                            let _e332 = frag_color1_;
                            color1_6 = (_e331 * _e332);
                            let _e334 = sampledColor2_;
                            let _e335 = frag_color2_;
                            color2_6 = (_e334 * _e335);
                            let _e337 = color0_;
                            let _e339 = color1_6;
                            let _e342 = color2_6;
                            let _e344 = ((_e337.xyz * _e339.xyz) * _e342.xyz);
                            base[0u] = _e344.x;
                            base[1u] = _e344.y;
                            base[2u] = _e344.z;
                            let _e352 = color0_[3u];
                            let _e354 = color1_6[3u];
                            let _e357 = color2_6[3u];
                            base[3u] = ((_e352 * _e354) * _e357);
                        }
                    }
                }
            }
        }
    }
    let _e360 = base;
    param_27 = _e360.xyz;
    let _e362 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_27));
    base[0u] = _e362.x;
    base[1u] = _e362.y;
    base[2u] = _e362.z;
    if override_type_6_13 {
        let _e371 = unnamed.worldLightParams[1u];
        wetness = clamp(_e371, 0f, 1f);
        let _e375 = unnamed.worldLightParams[2u];
        frost = clamp(_e375, 0f, 1f);
        let _e377 = base;
        luminance = dot(_e377.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e380 = wetness;
        let _e382 = base;
        let _e384 = (_e382.xyz * mix(1f, 0.82f, _e380));
        base[0u] = _e384.x;
        base[1u] = _e384.y;
        base[2u] = _e384.z;
        let _e391 = base;
        let _e393 = luminance;
        let _e395 = luminance;
        let _e397 = luminance;
        let _e399 = frost;
        let _e402 = mix(_e391.xyz, vec3<f32>((_e393 * 0.88f), (_e395 * 0.94f), _e397), vec3((_e399 * 0.55f)));
        base[0u] = _e402.x;
        base[1u] = _e402.y;
        base[2u] = _e402.z;
    }
    let _e411 = unnamed.materialFactors[0u];
    if (_e411 >= 0f) {
        let _e414 = unnamed.materialBaseColor;
        let _e415 = base;
        base = (_e415 * _e414);
    }
    let _e417 = color0_;
    let _e420 = unnamed.emissionRadiance;
    let _e423 = base;
    let _e425 = (_e423.xyz + (_e417.xyz * _e420.xyz));
    base[0u] = _e425.x;
    base[1u] = _e425.y;
    base[2u] = _e425.z;
    let _e432 = wired_advanced_fog_enabled_u0028_();
    if _e432 {
        let _e433 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e433;
        let _e434 = base;
        let _e437 = unnamed.advancedFogColorDensity;
        let _e439 = fogAmount;
        let _e441 = mix(_e434.xyz, _e437.xyz, vec3(_e439));
        base[0u] = _e441.x;
        base[1u] = _e441.y;
        base[2u] = _e441.z;
    }
    if override_type_6_14 {
        let _e449 = base[3u];
        if (_e449 == 0f) {
            discard;
        }
    } else {
        if override_type_6_15 {
            let _e451 = base;
            let _e453 = base;
            if (dot(_e451.xyz, _e453.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e457 = base;
    out_color = _e457;
    param_28 = 1f;
    let _e458 = base;
    param_29 = _e458;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_28), (&param_29));
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
