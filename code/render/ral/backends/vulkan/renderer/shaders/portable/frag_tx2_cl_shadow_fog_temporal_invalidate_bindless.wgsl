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
@id(10) override acff: i32 = 0i;
override override_type_4_14: bool = (acff == 1i);
override override_type_4_15: bool = (acff == 2i);
override override_type_4_16: bool = (acff == 3i);
override override_type_4_17: bool = (acff == 1i);
override override_type_4_18: bool = (acff == 2i);
override override_type_4_19: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_20: bool = (discard_mode == 1i);
override override_type_4_21: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e106 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e106 + 0.5f));
    let _e111 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e113 = fogType;
    let _e116 = fogType;
    return (((_e111 > 0.5f) && (_e113 >= 1i)) && (_e116 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e106 = wired_advanced_fog_enabled_u0028_();
    if !(_e106) {
        return 0f;
    }
    let _e109 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e109, 0.000001f));
    let _e114 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e114 + 0.5f));
    let _e117 = fogType_1;
    if (_e117 == 1i) {
        let _e121 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e121 <= 0f) {
            return 0f;
        }
        let _e123 = viewDepth;
        let _e126 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e123 / _e126), 0f, 1f);
    }
    let _e131 = unnamed.advancedFogColorDensity[3u];
    let _e133 = viewDepth;
    opticalDepth = (max(_e131, 0f) * _e133);
    let _e135 = fogType_1;
    if (_e135 == 2i) {
        let _e137 = opticalDepth;
        return clamp((1f - exp(-(_e137))), 0f, 1f);
    }
    let _e142 = opticalDepth;
    let _e143 = opticalDepth;
    return clamp((1f - exp(-((_e142 * _e143)))), 0f, 1f);
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

    let _e113 = (*c);
    let _e116 = unnamed.cascadeMVP[_e113];
    let _e117 = (*worldPos);
    sc4_ = (_e116 * vec4<f32>(_e117.x, _e117.y, _e117.z, 1f));
    let _e123 = sc4_;
    let _e126 = sc4_[3u];
    sc = (_e123.xyz / vec3(_e126));
    let _e129 = sc;
    let _e133 = ((_e129.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e133.x;
    sc[1u] = _e133.y;
    let _e139 = sc[0u];
    let _e140 = (_e139 < 0f);
    phi_340_ = _e140;
    if !(_e140) {
        let _e143 = sc[0u];
        phi_340_ = (_e143 > 1f);
    }
    let _e146 = phi_340_;
    phi_347_ = _e146;
    if !(_e146) {
        let _e149 = sc[1u];
        phi_347_ = (_e149 < 0f);
    }
    let _e152 = phi_347_;
    phi_354_ = _e152;
    if !(_e152) {
        let _e155 = sc[1u];
        phi_354_ = (_e155 > 1f);
    }
    let _e158 = phi_354_;
    phi_361_ = _e158;
    if !(_e158) {
        let _e161 = sc[2u];
        phi_361_ = (_e161 > 1f);
    }
    let _e164 = phi_361_;
    if _e164 {
        return 1f;
    }
    let _e165 = (*c);
    layer = f32(_e165);
    let _e167 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e167).xy));
    let _e174 = sc[2u];
    currentDepth = (_e174 - shadow_bias);
    shadow = 0f;
    if override_type_4_ {
        let _e176 = currentDepth;
        let _e177 = sc;
        let _e178 = _e177.xy;
        let _e179 = layer;
        let _e182 = vec3<f32>(_e178.x, _e178.y, _e179);
        let _e188 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e182.x, _e182.y), i32(_e182.z));
        shadow = step(_e176, _e188.x);
    } else {
        if override_type_4_1 {
            let _e191 = currentDepth;
            let _e192 = sc;
            let _e193 = _e192.xy;
            let _e194 = layer;
            let _e197 = vec3<f32>(_e193.x, _e193.y, _e194);
            let _e203 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e197.x, _e197.y), i32(_e197.z));
            let _e206 = shadow;
            shadow = (_e206 + step(_e191, _e203.x));
            let _e208 = currentDepth;
            let _e209 = sc;
            let _e212 = texelSize[0u];
            let _e214 = (_e209.xy + vec2<f32>(_e212, 0f));
            let _e215 = layer;
            let _e218 = vec3<f32>(_e214.x, _e214.y, _e215);
            let _e224 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e218.x, _e218.y), i32(_e218.z));
            let _e227 = shadow;
            shadow = (_e227 + step(_e208, _e224.x));
            let _e229 = currentDepth;
            let _e230 = sc;
            let _e233 = texelSize[0u];
            let _e235 = (_e230.xy - vec2<f32>(_e233, 0f));
            let _e236 = layer;
            let _e239 = vec3<f32>(_e235.x, _e235.y, _e236);
            let _e245 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e239.x, _e239.y), i32(_e239.z));
            let _e248 = shadow;
            shadow = (_e248 + step(_e229, _e245.x));
            let _e250 = currentDepth;
            let _e251 = sc;
            let _e254 = texelSize[1u];
            let _e256 = (_e251.xy + vec2<f32>(0f, _e254));
            let _e257 = layer;
            let _e260 = vec3<f32>(_e256.x, _e256.y, _e257);
            let _e266 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e260.x, _e260.y), i32(_e260.z));
            let _e269 = shadow;
            shadow = (_e269 + step(_e250, _e266.x));
            let _e271 = currentDepth;
            let _e272 = sc;
            let _e275 = texelSize[1u];
            let _e277 = (_e272.xy - vec2<f32>(0f, _e275));
            let _e278 = layer;
            let _e281 = vec3<f32>(_e277.x, _e277.y, _e278);
            let _e287 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e281.x, _e281.y), i32(_e281.z));
            let _e290 = shadow;
            shadow = (_e290 + step(_e271, _e287.x));
            let _e292 = shadow;
            shadow = (_e292 / 5f);
        } else {
            x = -1i;
            loop {
                let _e294 = x;
                if (_e294 <= 1i) {
                    y = -1i;
                    loop {
                        let _e296 = y;
                        if (_e296 <= 1i) {
                            let _e298 = currentDepth;
                            let _e299 = sc;
                            let _e301 = x;
                            let _e303 = y;
                            let _e306 = texelSize;
                            let _e308 = (_e299.xy + (vec2<f32>(f32(_e301), f32(_e303)) * _e306));
                            let _e309 = layer;
                            let _e312 = vec3<f32>(_e308.x, _e308.y, _e309);
                            let _e318 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e312.x, _e312.y), i32(_e312.z));
                            let _e321 = shadow;
                            shadow = (_e321 + step(_e298, _e318.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e323 = y;
                            y = (_e323 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e325 = x;
                    x = (_e325 + 1i);
                }
            }
            let _e327 = shadow;
            shadow = (_e327 / 9f);
        }
    }
    let _e329 = shadow;
    return _e329;
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

    let _e120 = unnamed.cascadeSplits;
    let _e121 = (*viewDepth_1);
    cmp = step(_e120, vec4(_e121));
    let _e125 = cmp[0u];
    let _e127 = cmp[1u];
    let _e130 = cmp[2u];
    let _e133 = cmp[3u];
    cascade = min(i32((((_e125 + _e127) + _e130) + _e133)), 3i);
    let _e137 = cascade;
    (*outCascade) = _e137;
    let _e138 = cascade;
    if (_e138 == 0i) {
        local = 0f;
    } else {
        let _e140 = cascade;
        let _e145 = unnamed.cascadeSplits[max((_e140 - 1i), 0i)];
        local = _e145;
    }
    let _e146 = local;
    prevSplit = _e146;
    let _e147 = cascade;
    let _e150 = unnamed.cascadeSplits[_e147];
    farSplit = _e150;
    let _e151 = farSplit;
    let _e152 = prevSplit;
    blendRange = max((0.1f * (_e151 - _e152)), 1f);
    let _e156 = farSplit;
    let _e157 = (*viewDepth_1);
    let _e159 = blendRange;
    blendT = clamp(((_e156 - _e157) / _e159), 0f, 1f);
    let _e162 = cascade;
    param = _e162;
    let _e163 = (*worldPos_1);
    param_1 = _e163;
    let _e164 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e164;
    let _e165 = cascade;
    param_2 = min((_e165 + 1i), 3i);
    let _e168 = (*worldPos_1);
    param_3 = _e168;
    let _e169 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e169;
    let _e170 = s1_;
    let _e171 = s0_;
    let _e172 = blendT;
    return mix(_e170, _e171, _e172);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e114 = unnamed.packed_indices[1i][3u];
    let _e120 = unnamed.packed_indices[1i][3u];
    let _e125 = (*lm_uv);
    let _e126 = textureSample(wired_bindless_images[(_e114 & 4095u)], wired_bindless_samplers[((_e120 >> bitcast<u32>(12i)) & 255u)], _e125);
    sun_mask = _e126.x;
    let _e128 = sun_mask;
    if (_e128 > 0.001f) {
        let _e130 = shadowData_1;
        param_4 = _e130.xyz;
        let _e133 = shadowData_1[3u];
        param_5 = _e133;
        let _e134 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e135 = param_6;
        ignoredCascade = _e135;
        shadow_1 = _e134;
        let _e136 = shadow_1;
        let _e137 = sun_mask;
        let _e139 = (*rgb);
        (*rgb) = (_e139 * mix(1f, _e136, _e137));
    }
    let _e141 = (*rgb);
    return _e141;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_4_2 {
        let _e110 = (*rgb_1);
        param_7 = _e110;
        let _e111 = frag_tex_coord0_1;
        param_8 = _e111;
        let _e112 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e112;
    }
    if override_type_4_3 {
        let _e113 = (*rgb_1);
        param_9 = _e113;
        let _e114 = frag_tex_coord1_1;
        param_10 = _e114;
        let _e115 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e115;
    }
    if override_type_4_4 {
        let _e116 = (*rgb_1);
        param_11 = _e116;
        let _e117 = frag_tex_coord2_1;
        param_12 = _e117;
        let _e118 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e118;
    }
    let _e119 = (*rgb_1);
    return _e119;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e106 = (*rgb_2);
    let _e109 = unnamed.worldLightParams[0u];
    boosted = (_e106 * _e109);
    let _e112 = boosted[0u];
    let _e114 = boosted[1u];
    let _e116 = boosted[2u];
    peak = max(_e112, max(_e114, _e116));
    let _e119 = peak;
    if (_e119 > 1f) {
        let _e121 = peak;
        let _e122 = boosted;
        boosted = (_e122 / vec3(_e121));
    }
    let _e125 = boosted;
    return _e125;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e107 = (*c_1);
    (*c_1) = max(_e107, vec3<f32>(0f, 0f, 0f));
    let _e109 = (*c_1);
    cutoff = (_e109 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e111 = (*c_1);
    lo = (_e111 / vec3(12.92f));
    let _e114 = (*c_1);
    hi = pow(((_e114 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e119 = hi;
    let _e120 = lo;
    let _e121 = cutoff;
    return mix(_e119, _e120, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e121));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e109 = (*role);
    let _e111 = (*role);
    let _e116 = unnamed.packed_indices[(_e109 / 4u)][(_e111 % 4u)];
    let _e119 = (*role);
    let _e121 = (*role);
    let _e126 = unnamed.packed_indices[(_e119 / 4u)][(_e121 % 4u)];
    let _e131 = (*uv);
    let _e132 = textureSample(wired_bindless_images[(_e116 & 4095u)], wired_bindless_samplers[((_e126 >> bitcast<u32>(12i)) & 255u)], _e131);
    c_2 = _e132;
    let _e133 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e133))) == 0i) {
        let _e138 = c_2;
        param_13 = _e138.xyz;
        let _e140 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e140.x;
        c_2[1u] = _e140.y;
        c_2[2u] = _e140.z;
    }
    let _e147 = (*slot);
    if (lightmap_slot == (_e147 + 1i)) {
        let _e150 = c_2;
        param_14 = _e150.xyz;
        let _e152 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e152.x;
        c_2[1u] = _e152.y;
        c_2[2u] = _e152.z;
    }
    let _e159 = c_2;
    return _e159;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e147 = unnamed.packed_indices[0i][3u];
    let _e153 = unnamed.packed_indices[0i][3u];
    let _e158 = fog_tex_coord_1;
    let _e159 = textureSample(wired_bindless_images[(_e147 & 4095u)], wired_bindless_samplers[((_e153 >> bitcast<u32>(12i)) & 255u)], _e158);
    fog = _e159;
    let _e160 = frag_color0In_1;
    param_15 = _e160.xyz;
    let _e162 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e164 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e162.x, _e162.y, _e162.z, _e164);
    let _e169 = frag_color1In_1;
    param_16 = _e169.xyz;
    let _e171 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e173 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e171.x, _e171.y, _e171.z, _e173);
    let _e178 = frag_color2In_1;
    param_17 = _e178.xyz;
    let _e180 = sRGBToLinear_u0028_vf3_u003b((&param_17));
    let _e182 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e180.x, _e180.y, _e180.z, _e182);
    param_18 = 0u;
    let _e187 = frag_tex_coord0_1;
    param_19 = _e187;
    param_20 = 0i;
    let _e188 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e188;
    param_21 = 1u;
    let _e189 = frag_tex_coord1_1;
    param_22 = _e189;
    param_23 = 1i;
    let _e190 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e190;
    param_24 = 2u;
    let _e191 = frag_tex_coord2_1;
    param_25 = _e191;
    param_26 = 2i;
    let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
    sampledColor2_ = _e192;
    let _e193 = sampledColor0_;
    let _e194 = frag_color0_;
    color0_ = (_e193 * _e194);
    if override_type_4_7 {
        let _e196 = sampledColor1_;
        let _e197 = frag_color1_;
        color1_ = (_e196 * _e197);
        let _e199 = sampledColor2_;
        let _e200 = frag_color2_;
        color2_ = (_e199 * _e200);
        let _e202 = color0_;
        let _e204 = color1_;
        let _e207 = color2_;
        let _e209 = ((_e202.xyz + _e204.xyz) + _e207.xyz);
        let _e211 = color0_[3u];
        let _e213 = color1_[3u];
        let _e216 = color2_[3u];
        base = vec4<f32>(_e209.x, _e209.y, _e209.z, ((_e211 * _e213) * _e216));
    } else {
        if override_type_4_8 {
            let _e222 = sampledColor1_;
            let _e223 = frag_color1_;
            color1_1 = (_e222 * _e223);
            let _e225 = sampledColor2_;
            let _e226 = frag_color2_;
            color2_1 = (_e225 * _e226);
            let _e229 = color0_[3u];
            let _e230 = color0_;
            color0_ = (_e230 * _e229);
            let _e233 = color1_1[3u];
            let _e234 = color1_1;
            color1_1 = (_e234 * _e233);
            let _e237 = color2_1[3u];
            let _e238 = color2_1;
            color2_1 = (_e238 * _e237);
            let _e240 = color0_;
            let _e242 = color1_1;
            let _e245 = color2_1;
            let _e247 = ((_e240.xyz + _e242.xyz) + _e245.xyz);
            let _e249 = color0_[3u];
            let _e251 = color1_1[3u];
            let _e254 = color2_1[3u];
            base = vec4<f32>(_e247.x, _e247.y, _e247.z, ((_e249 * _e251) * _e254));
        } else {
            if override_type_4_9 {
                let _e260 = sampledColor1_;
                let _e261 = frag_color1_;
                color1_2 = (_e260 * _e261);
                let _e263 = sampledColor2_;
                let _e264 = frag_color2_;
                color2_2 = (_e263 * _e264);
                let _e267 = color0_[3u];
                let _e269 = color0_;
                color0_ = (_e269 * (1f - _e267));
                let _e272 = color1_2[3u];
                let _e274 = color1_2;
                color1_2 = (_e274 * (1f - _e272));
                let _e277 = color2_2[3u];
                let _e279 = color2_2;
                color2_2 = (_e279 * (1f - _e277));
                let _e281 = color0_;
                let _e283 = color1_2;
                let _e286 = color2_2;
                let _e288 = ((_e281.xyz + _e283.xyz) + _e286.xyz);
                let _e290 = color0_[3u];
                let _e292 = color1_2[3u];
                let _e295 = color2_2[3u];
                base = vec4<f32>(_e288.x, _e288.y, _e288.z, ((_e290 * _e292) * _e295));
            } else {
                if override_type_4_10 {
                    let _e301 = sampledColor1_;
                    let _e302 = frag_color1_;
                    color1_3 = (_e301 * _e302);
                    let _e304 = sampledColor2_;
                    let _e305 = frag_color2_;
                    color2_3 = (_e304 * _e305);
                    let _e307 = color0_;
                    let _e308 = color1_3;
                    let _e310 = color1_3[3u];
                    let _e313 = color2_3;
                    let _e315 = color2_3[3u];
                    base = mix(mix(_e307, _e308, vec4(_e310)), _e313, vec4(_e315));
                } else {
                    if override_type_4_11 {
                        let _e318 = sampledColor1_;
                        let _e319 = frag_color1_;
                        color1_4 = (_e318 * _e319);
                        let _e321 = sampledColor2_;
                        let _e322 = frag_color2_;
                        color2_4 = (_e321 * _e322);
                        let _e324 = color2_4;
                        let _e325 = color1_4;
                        let _e326 = color0_;
                        let _e328 = color1_4[3u];
                        let _e332 = color2_4[3u];
                        base = mix(_e324, mix(_e325, _e326, vec4(_e328)), vec4(_e332));
                    } else {
                        if override_type_4_12 {
                            let _e335 = sampledColor1_;
                            let _e336 = frag_color1_;
                            color1_5 = (_e335 * _e336);
                            let _e338 = sampledColor2_;
                            let _e339 = frag_color2_;
                            color2_5 = (_e338 * _e339);
                            let _e341 = color2_5;
                            let _e343 = color2_5[3u];
                            let _e346 = color1_5;
                            let _e348 = color1_5[3u];
                            let _e352 = color0_;
                            base = (((_e341 + vec4(_e343)) * (_e346 + vec4(_e348))) * _e352);
                        } else {
                            let _e354 = sampledColor1_;
                            let _e355 = frag_color1_;
                            color1_6 = (_e354 * _e355);
                            let _e357 = sampledColor2_;
                            let _e358 = frag_color2_;
                            color2_6 = (_e357 * _e358);
                            let _e360 = color0_;
                            let _e362 = color1_6;
                            let _e365 = color2_6;
                            let _e367 = ((_e360.xyz * _e362.xyz) * _e365.xyz);
                            base[0u] = _e367.x;
                            base[1u] = _e367.y;
                            base[2u] = _e367.z;
                            let _e375 = color0_[3u];
                            let _e377 = color1_6[3u];
                            let _e380 = color2_6[3u];
                            base[3u] = ((_e375 * _e377) * _e380);
                        }
                    }
                }
            }
        }
    }
    let _e383 = base;
    param_27 = _e383.xyz;
    let _e385 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_27));
    base[0u] = _e385.x;
    base[1u] = _e385.y;
    base[2u] = _e385.z;
    if override_type_4_13 {
        let _e394 = unnamed.worldLightParams[1u];
        wetness = clamp(_e394, 0f, 1f);
        let _e398 = unnamed.worldLightParams[2u];
        frost = clamp(_e398, 0f, 1f);
        let _e400 = base;
        luminance = dot(_e400.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e403 = wetness;
        let _e405 = base;
        let _e407 = (_e405.xyz * mix(1f, 0.82f, _e403));
        base[0u] = _e407.x;
        base[1u] = _e407.y;
        base[2u] = _e407.z;
        let _e414 = base;
        let _e416 = luminance;
        let _e418 = luminance;
        let _e420 = luminance;
        let _e422 = frost;
        let _e425 = mix(_e414.xyz, vec3<f32>((_e416 * 0.88f), (_e418 * 0.94f), _e420), vec3((_e422 * 0.55f)));
        base[0u] = _e425.x;
        base[1u] = _e425.y;
        base[2u] = _e425.z;
    }
    let _e434 = unnamed.materialFactors[0u];
    if (_e434 >= 0f) {
        let _e437 = unnamed.materialBaseColor;
        let _e438 = base;
        base = (_e438 * _e437);
    }
    let _e440 = color0_;
    let _e443 = unnamed.emissionRadiance;
    let _e446 = base;
    let _e448 = (_e446.xyz + (_e440.xyz * _e443.xyz));
    base[0u] = _e448.x;
    base[1u] = _e448.y;
    base[2u] = _e448.z;
    let _e455 = wired_advanced_fog_enabled_u0028_();
    if _e455 {
        let _e456 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e456;
        if override_type_4_14 {
            let _e457 = fogAmount;
            let _e459 = base;
            let _e461 = (_e459.xyz * (1f - _e457));
            base[0u] = _e461.x;
            base[1u] = _e461.y;
            base[2u] = _e461.z;
        } else {
            if override_type_4_15 {
                let _e468 = fogAmount;
                let _e470 = base;
                base = (_e470 * (1f - _e468));
            } else {
                if override_type_4_16 {
                    let _e472 = fogAmount;
                    let _e475 = base[3u];
                    base[3u] = (_e475 * (1f - _e472));
                } else {
                    let _e478 = base;
                    let _e481 = unnamed.advancedFogColorDensity;
                    let _e483 = fogAmount;
                    let _e485 = mix(_e478.xyz, _e481.xyz, vec3(_e483));
                    base[0u] = _e485.x;
                    base[1u] = _e485.y;
                    base[2u] = _e485.z;
                }
            }
        }
    } else {
        if override_type_4_17 {
            let _e492 = base;
            let _e495 = fog[3u];
            let _e497 = (_e492.xyz * (1f - _e495));
            base[0u] = _e497.x;
            base[1u] = _e497.y;
            base[2u] = _e497.z;
        } else {
            if override_type_4_18 {
                let _e504 = base;
                let _e506 = fog[3u];
                base = (_e504 * (1f - _e506));
            } else {
                if override_type_4_19 {
                    let _e510 = base[3u];
                    let _e512 = fog[3u];
                    base[3u] = (_e510 * (1f - _e512));
                } else {
                    let _e516 = base;
                    let _e517 = fog;
                    let _e519 = unnamed.fogColor;
                    let _e522 = fog[3u];
                    base = mix(_e516, (_e517 * _e519), vec4(_e522));
                }
            }
        }
    }
    if override_type_4_20 {
        let _e526 = base[3u];
        if (_e526 == 0f) {
            discard;
        }
    } else {
        if override_type_4_21 {
            let _e528 = base;
            let _e530 = base;
            if (dot(_e528.xyz, _e530.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e534 = base;
    out_color = _e534;
    param_28 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_28));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e27 = out_temporal_velocity;
    let _e28 = out_temporal_validity;
    let _e29 = out_color;
    return FragmentOutput(_e27, _e28, _e29);
}
