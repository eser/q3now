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
@id(10) override acff: i32 = 0i;
override override_type_4_7: bool = (acff == 1i);
override override_type_4_8: bool = (acff == 2i);
override override_type_4_9: bool = (acff == 3i);
override override_type_4_10: bool = (acff == 1i);
override override_type_4_11: bool = (acff == 2i);
override override_type_4_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_13: bool = (discard_mode == 1i);
override override_type_4_14: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e94 + 0.5f));
    let _e99 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e101 = fogType;
    let _e104 = fogType;
    return (((_e99 > 0.5f) && (_e101 >= 1i)) && (_e104 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e94 = wired_advanced_fog_enabled_u0028_();
    if !(_e94) {
        return 0f;
    }
    let _e97 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e97, 0.000001f));
    let _e102 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e102 + 0.5f));
    let _e105 = fogType_1;
    if (_e105 == 1i) {
        let _e109 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e109 <= 0f) {
            return 0f;
        }
        let _e111 = viewDepth;
        let _e114 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e111 / _e114), 0f, 1f);
    }
    let _e119 = unnamed.advancedFogColorDensity[3u];
    let _e121 = viewDepth;
    opticalDepth = (max(_e119, 0f) * _e121);
    let _e123 = fogType_1;
    if (_e123 == 2i) {
        let _e125 = opticalDepth;
        return clamp((1f - exp(-(_e125))), 0f, 1f);
    }
    let _e130 = opticalDepth;
    let _e131 = opticalDepth;
    return clamp((1f - exp(-((_e130 * _e131)))), 0f, 1f);
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

    let _e101 = (*c);
    let _e104 = unnamed.cascadeMVP[_e101];
    let _e105 = (*worldPos);
    sc4_ = (_e104 * vec4<f32>(_e105.x, _e105.y, _e105.z, 1f));
    let _e111 = sc4_;
    let _e114 = sc4_[3u];
    sc = (_e111.xyz / vec3(_e114));
    let _e117 = sc;
    let _e121 = ((_e117.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e121.x;
    sc[1u] = _e121.y;
    let _e127 = sc[0u];
    let _e128 = (_e127 < 0f);
    phi_340_ = _e128;
    if !(_e128) {
        let _e131 = sc[0u];
        phi_340_ = (_e131 > 1f);
    }
    let _e134 = phi_340_;
    phi_347_ = _e134;
    if !(_e134) {
        let _e137 = sc[1u];
        phi_347_ = (_e137 < 0f);
    }
    let _e140 = phi_347_;
    phi_354_ = _e140;
    if !(_e140) {
        let _e143 = sc[1u];
        phi_354_ = (_e143 > 1f);
    }
    let _e146 = phi_354_;
    phi_361_ = _e146;
    if !(_e146) {
        let _e149 = sc[2u];
        phi_361_ = (_e149 > 1f);
    }
    let _e152 = phi_361_;
    if _e152 {
        return 1f;
    }
    let _e153 = (*c);
    layer = f32(_e153);
    let _e155 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e155).xy));
    let _e162 = sc[2u];
    currentDepth = (_e162 - shadow_bias);
    shadow = 0f;
    if override_type_4_ {
        let _e164 = currentDepth;
        let _e165 = sc;
        let _e166 = _e165.xy;
        let _e167 = layer;
        let _e170 = vec3<f32>(_e166.x, _e166.y, _e167);
        let _e176 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e170.x, _e170.y), i32(_e170.z));
        shadow = step(_e164, _e176.x);
    } else {
        if override_type_4_1 {
            let _e179 = currentDepth;
            let _e180 = sc;
            let _e181 = _e180.xy;
            let _e182 = layer;
            let _e185 = vec3<f32>(_e181.x, _e181.y, _e182);
            let _e191 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e185.x, _e185.y), i32(_e185.z));
            let _e194 = shadow;
            shadow = (_e194 + step(_e179, _e191.x));
            let _e196 = currentDepth;
            let _e197 = sc;
            let _e200 = texelSize[0u];
            let _e202 = (_e197.xy + vec2<f32>(_e200, 0f));
            let _e203 = layer;
            let _e206 = vec3<f32>(_e202.x, _e202.y, _e203);
            let _e212 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e206.x, _e206.y), i32(_e206.z));
            let _e215 = shadow;
            shadow = (_e215 + step(_e196, _e212.x));
            let _e217 = currentDepth;
            let _e218 = sc;
            let _e221 = texelSize[0u];
            let _e223 = (_e218.xy - vec2<f32>(_e221, 0f));
            let _e224 = layer;
            let _e227 = vec3<f32>(_e223.x, _e223.y, _e224);
            let _e233 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e227.x, _e227.y), i32(_e227.z));
            let _e236 = shadow;
            shadow = (_e236 + step(_e217, _e233.x));
            let _e238 = currentDepth;
            let _e239 = sc;
            let _e242 = texelSize[1u];
            let _e244 = (_e239.xy + vec2<f32>(0f, _e242));
            let _e245 = layer;
            let _e248 = vec3<f32>(_e244.x, _e244.y, _e245);
            let _e254 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e248.x, _e248.y), i32(_e248.z));
            let _e257 = shadow;
            shadow = (_e257 + step(_e238, _e254.x));
            let _e259 = currentDepth;
            let _e260 = sc;
            let _e263 = texelSize[1u];
            let _e265 = (_e260.xy - vec2<f32>(0f, _e263));
            let _e266 = layer;
            let _e269 = vec3<f32>(_e265.x, _e265.y, _e266);
            let _e275 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e269.x, _e269.y), i32(_e269.z));
            let _e278 = shadow;
            shadow = (_e278 + step(_e259, _e275.x));
            let _e280 = shadow;
            shadow = (_e280 / 5f);
        } else {
            x = -1i;
            loop {
                let _e282 = x;
                if (_e282 <= 1i) {
                    y = -1i;
                    loop {
                        let _e284 = y;
                        if (_e284 <= 1i) {
                            let _e286 = currentDepth;
                            let _e287 = sc;
                            let _e289 = x;
                            let _e291 = y;
                            let _e294 = texelSize;
                            let _e296 = (_e287.xy + (vec2<f32>(f32(_e289), f32(_e291)) * _e294));
                            let _e297 = layer;
                            let _e300 = vec3<f32>(_e296.x, _e296.y, _e297);
                            let _e306 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e300.x, _e300.y), i32(_e300.z));
                            let _e309 = shadow;
                            shadow = (_e309 + step(_e286, _e306.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e311 = y;
                            y = (_e311 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e313 = x;
                    x = (_e313 + 1i);
                }
            }
            let _e315 = shadow;
            shadow = (_e315 / 9f);
        }
    }
    let _e317 = shadow;
    return _e317;
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

    let _e108 = unnamed.cascadeSplits;
    let _e109 = (*viewDepth_1);
    cmp = step(_e108, vec4(_e109));
    let _e113 = cmp[0u];
    let _e115 = cmp[1u];
    let _e118 = cmp[2u];
    let _e121 = cmp[3u];
    cascade = min(i32((((_e113 + _e115) + _e118) + _e121)), 3i);
    let _e125 = cascade;
    (*outCascade) = _e125;
    let _e126 = cascade;
    if (_e126 == 0i) {
        local = 0f;
    } else {
        let _e128 = cascade;
        let _e133 = unnamed.cascadeSplits[max((_e128 - 1i), 0i)];
        local = _e133;
    }
    let _e134 = local;
    prevSplit = _e134;
    let _e135 = cascade;
    let _e138 = unnamed.cascadeSplits[_e135];
    farSplit = _e138;
    let _e139 = farSplit;
    let _e140 = prevSplit;
    blendRange = max((0.1f * (_e139 - _e140)), 1f);
    let _e144 = farSplit;
    let _e145 = (*viewDepth_1);
    let _e147 = blendRange;
    blendT = clamp(((_e144 - _e145) / _e147), 0f, 1f);
    let _e150 = cascade;
    param = _e150;
    let _e151 = (*worldPos_1);
    param_1 = _e151;
    let _e152 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e152;
    let _e153 = cascade;
    param_2 = min((_e153 + 1i), 3i);
    let _e156 = (*worldPos_1);
    param_3 = _e156;
    let _e157 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e157;
    let _e158 = s1_;
    let _e159 = s0_;
    let _e160 = blendT;
    return mix(_e158, _e159, _e160);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e102 = unnamed.packed_indices[1i][3u];
    let _e108 = unnamed.packed_indices[1i][3u];
    let _e113 = (*lm_uv);
    let _e114 = textureSample(wired_bindless_images[(_e102 & 4095u)], wired_bindless_samplers[((_e108 >> bitcast<u32>(12i)) & 255u)], _e113);
    sun_mask = _e114.x;
    let _e116 = sun_mask;
    if (_e116 > 0.001f) {
        let _e118 = shadowData_1;
        param_4 = _e118.xyz;
        let _e121 = shadowData_1[3u];
        param_5 = _e121;
        let _e122 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e123 = param_6;
        ignoredCascade = _e123;
        shadow_1 = _e122;
        let _e124 = shadow_1;
        let _e125 = sun_mask;
        let _e127 = (*rgb);
        (*rgb) = (_e127 * mix(1f, _e124, _e125));
    }
    let _e129 = (*rgb);
    return _e129;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_4_2 {
        let _e96 = (*rgb_1);
        param_7 = _e96;
        let _e97 = frag_tex_coord0_1;
        param_8 = _e97;
        let _e98 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e98;
    }
    if override_type_4_3 {
        let _e99 = (*rgb_1);
        param_9 = _e99;
        let _e100 = frag_tex_coord1_1;
        param_10 = _e100;
        let _e101 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e101;
    }
    let _e102 = (*rgb_1);
    return _e102;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e94 = (*rgb_2);
    let _e97 = unnamed.worldLightParams[0u];
    boosted = (_e94 * _e97);
    let _e100 = boosted[0u];
    let _e102 = boosted[1u];
    let _e104 = boosted[2u];
    peak = max(_e100, max(_e102, _e104));
    let _e107 = peak;
    if (_e107 > 1f) {
        let _e109 = peak;
        let _e110 = boosted;
        boosted = (_e110 / vec3(_e109));
    }
    let _e113 = boosted;
    return _e113;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e95 = (*c_1);
    (*c_1) = max(_e95, vec3<f32>(0f, 0f, 0f));
    let _e97 = (*c_1);
    cutoff = (_e97 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e99 = (*c_1);
    lo = (_e99 / vec3(12.92f));
    let _e102 = (*c_1);
    hi = pow(((_e102 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e107 = hi;
    let _e108 = lo;
    let _e109 = cutoff;
    return mix(_e107, _e108, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e109));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e107 = (*role);
    let _e109 = (*role);
    let _e114 = unnamed.packed_indices[(_e107 / 4u)][(_e109 % 4u)];
    let _e119 = (*uv);
    let _e120 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    c_2 = _e120;
    let _e121 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e121))) == 0i) {
        let _e126 = c_2;
        param_11 = _e126.xyz;
        let _e128 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e128.x;
        c_2[1u] = _e128.y;
        c_2[2u] = _e128.z;
    }
    let _e135 = (*slot);
    if (lightmap_slot == (_e135 + 1i)) {
        let _e138 = c_2;
        param_12 = _e138.xyz;
        let _e140 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e140.x;
        c_2[1u] = _e140.y;
        c_2[2u] = _e140.z;
    }
    let _e147 = c_2;
    return _e147;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e116 = unnamed.packed_indices[0i][3u];
    let _e122 = unnamed.packed_indices[0i][3u];
    let _e127 = fog_tex_coord_1;
    let _e128 = textureSample(wired_bindless_images[(_e116 & 4095u)], wired_bindless_samplers[((_e122 >> bitcast<u32>(12i)) & 255u)], _e127);
    fog = _e128;
    let _e129 = frag_color0In_1;
    param_13 = _e129.xyz;
    let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e133 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e131.x, _e131.y, _e131.z, _e133);
    param_14 = 0u;
    let _e138 = frag_tex_coord0_1;
    param_15 = _e138;
    param_16 = 0i;
    let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    sampledColor0_ = _e139;
    param_17 = 1u;
    let _e140 = frag_tex_coord1_1;
    param_18 = _e140;
    param_19 = 1i;
    let _e141 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    sampledColor1_ = _e141;
    let _e142 = sampledColor0_;
    let _e143 = frag_color0_;
    color0_ = (_e142 * _e143);
    if override_type_4_4 {
        let _e145 = sampledColor1_;
        color1_ = _e145;
        let _e146 = color0_;
        let _e148 = color1_;
        let _e150 = (_e146.xyz + _e148.xyz);
        let _e152 = color0_[3u];
        let _e154 = color1_[3u];
        base = vec4<f32>(_e150.x, _e150.y, _e150.z, (_e152 * _e154));
    } else {
        if override_type_4_5 {
            let _e160 = sampledColor1_;
            let _e161 = frag_color0_;
            color1_1 = (_e160 * _e161);
            let _e163 = color0_;
            let _e165 = color1_1;
            let _e167 = (_e163.xyz + _e165.xyz);
            let _e169 = color0_[3u];
            let _e171 = color1_1[3u];
            base = vec4<f32>(_e167.x, _e167.y, _e167.z, (_e169 * _e171));
        } else {
            let _e177 = sampledColor1_;
            color1_2 = _e177;
            let _e178 = color0_;
            let _e180 = color1_2;
            let _e182 = (_e178.xyz * _e180.xyz);
            base[0u] = _e182.x;
            base[1u] = _e182.y;
            base[2u] = _e182.z;
            let _e190 = color0_[3u];
            let _e192 = color1_2[3u];
            base[3u] = (_e190 * _e192);
        }
    }
    let _e195 = base;
    param_20 = _e195.xyz;
    let _e197 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_20));
    base[0u] = _e197.x;
    base[1u] = _e197.y;
    base[2u] = _e197.z;
    if override_type_4_6 {
        let _e206 = unnamed.worldLightParams[1u];
        wetness = clamp(_e206, 0f, 1f);
        let _e210 = unnamed.worldLightParams[2u];
        frost = clamp(_e210, 0f, 1f);
        let _e212 = base;
        luminance = dot(_e212.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e215 = wetness;
        let _e217 = base;
        let _e219 = (_e217.xyz * mix(1f, 0.82f, _e215));
        base[0u] = _e219.x;
        base[1u] = _e219.y;
        base[2u] = _e219.z;
        let _e226 = base;
        let _e228 = luminance;
        let _e230 = luminance;
        let _e232 = luminance;
        let _e234 = frost;
        let _e237 = mix(_e226.xyz, vec3<f32>((_e228 * 0.88f), (_e230 * 0.94f), _e232), vec3((_e234 * 0.55f)));
        base[0u] = _e237.x;
        base[1u] = _e237.y;
        base[2u] = _e237.z;
    }
    let _e246 = unnamed.materialFactors[0u];
    if (_e246 >= 0f) {
        let _e249 = unnamed.materialBaseColor;
        let _e250 = base;
        base = (_e250 * _e249);
    }
    let _e252 = color0_;
    let _e255 = unnamed.emissionRadiance;
    let _e258 = base;
    let _e260 = (_e258.xyz + (_e252.xyz * _e255.xyz));
    base[0u] = _e260.x;
    base[1u] = _e260.y;
    base[2u] = _e260.z;
    let _e267 = wired_advanced_fog_enabled_u0028_();
    if _e267 {
        let _e268 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e268;
        if override_type_4_7 {
            let _e269 = fogAmount;
            let _e271 = base;
            let _e273 = (_e271.xyz * (1f - _e269));
            base[0u] = _e273.x;
            base[1u] = _e273.y;
            base[2u] = _e273.z;
        } else {
            if override_type_4_8 {
                let _e280 = fogAmount;
                let _e282 = base;
                base = (_e282 * (1f - _e280));
            } else {
                if override_type_4_9 {
                    let _e284 = fogAmount;
                    let _e287 = base[3u];
                    base[3u] = (_e287 * (1f - _e284));
                } else {
                    let _e290 = base;
                    let _e293 = unnamed.advancedFogColorDensity;
                    let _e295 = fogAmount;
                    let _e297 = mix(_e290.xyz, _e293.xyz, vec3(_e295));
                    base[0u] = _e297.x;
                    base[1u] = _e297.y;
                    base[2u] = _e297.z;
                }
            }
        }
    } else {
        if override_type_4_10 {
            let _e304 = base;
            let _e307 = fog[3u];
            let _e309 = (_e304.xyz * (1f - _e307));
            base[0u] = _e309.x;
            base[1u] = _e309.y;
            base[2u] = _e309.z;
        } else {
            if override_type_4_11 {
                let _e316 = base;
                let _e318 = fog[3u];
                base = (_e316 * (1f - _e318));
            } else {
                if override_type_4_12 {
                    let _e322 = base[3u];
                    let _e324 = fog[3u];
                    base[3u] = (_e322 * (1f - _e324));
                } else {
                    let _e328 = base;
                    let _e329 = fog;
                    let _e331 = unnamed.fogColor;
                    let _e334 = fog[3u];
                    base = mix(_e328, (_e329 * _e331), vec4(_e334));
                }
            }
        }
    }
    if override_type_4_13 {
        let _e338 = base[3u];
        if (_e338 == 0f) {
            discard;
        }
    } else {
        if override_type_4_14 {
            let _e340 = base;
            let _e342 = base;
            if (dot(_e340.xyz, _e342.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e346 = base;
    out_color = _e346;
    param_21 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_21));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
