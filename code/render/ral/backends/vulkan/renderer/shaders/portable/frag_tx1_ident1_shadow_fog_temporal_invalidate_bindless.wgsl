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

    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e93 + 0.5f));
    let _e98 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e100 = fogType;
    let _e103 = fogType;
    return (((_e98 > 0.5f) && (_e100 >= 1i)) && (_e103 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e93 = wired_advanced_fog_enabled_u0028_();
    if !(_e93) {
        return 0f;
    }
    let _e96 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e96, 0.000001f));
    let _e101 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e101 + 0.5f));
    let _e104 = fogType_1;
    if (_e104 == 1i) {
        let _e108 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e108 <= 0f) {
            return 0f;
        }
        let _e110 = viewDepth;
        let _e113 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e110 / _e113), 0f, 1f);
    }
    let _e118 = unnamed.advancedFogColorDensity[3u];
    let _e120 = viewDepth;
    opticalDepth = (max(_e118, 0f) * _e120);
    let _e122 = fogType_1;
    if (_e122 == 2i) {
        let _e124 = opticalDepth;
        return clamp((1f - exp(-(_e124))), 0f, 1f);
    }
    let _e129 = opticalDepth;
    let _e130 = opticalDepth;
    return clamp((1f - exp(-((_e129 * _e130)))), 0f, 1f);
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

    let _e100 = (*c);
    let _e103 = unnamed.cascadeMVP[_e100];
    let _e104 = (*worldPos);
    sc4_ = (_e103 * vec4<f32>(_e104.x, _e104.y, _e104.z, 1f));
    let _e110 = sc4_;
    let _e113 = sc4_[3u];
    sc = (_e110.xyz / vec3(_e113));
    let _e116 = sc;
    let _e120 = ((_e116.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e120.x;
    sc[1u] = _e120.y;
    let _e126 = sc[0u];
    let _e127 = (_e126 < 0f);
    phi_340_ = _e127;
    if !(_e127) {
        let _e130 = sc[0u];
        phi_340_ = (_e130 > 1f);
    }
    let _e133 = phi_340_;
    phi_347_ = _e133;
    if !(_e133) {
        let _e136 = sc[1u];
        phi_347_ = (_e136 < 0f);
    }
    let _e139 = phi_347_;
    phi_354_ = _e139;
    if !(_e139) {
        let _e142 = sc[1u];
        phi_354_ = (_e142 > 1f);
    }
    let _e145 = phi_354_;
    phi_361_ = _e145;
    if !(_e145) {
        let _e148 = sc[2u];
        phi_361_ = (_e148 > 1f);
    }
    let _e151 = phi_361_;
    if _e151 {
        return 1f;
    }
    let _e152 = (*c);
    layer = f32(_e152);
    let _e154 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e154).xy));
    let _e161 = sc[2u];
    currentDepth = (_e161 - shadow_bias);
    shadow = 0f;
    if override_type_4_ {
        let _e163 = currentDepth;
        let _e164 = sc;
        let _e165 = _e164.xy;
        let _e166 = layer;
        let _e169 = vec3<f32>(_e165.x, _e165.y, _e166);
        let _e175 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e169.x, _e169.y), i32(_e169.z));
        shadow = step(_e163, _e175.x);
    } else {
        if override_type_4_1 {
            let _e178 = currentDepth;
            let _e179 = sc;
            let _e180 = _e179.xy;
            let _e181 = layer;
            let _e184 = vec3<f32>(_e180.x, _e180.y, _e181);
            let _e190 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e184.x, _e184.y), i32(_e184.z));
            let _e193 = shadow;
            shadow = (_e193 + step(_e178, _e190.x));
            let _e195 = currentDepth;
            let _e196 = sc;
            let _e199 = texelSize[0u];
            let _e201 = (_e196.xy + vec2<f32>(_e199, 0f));
            let _e202 = layer;
            let _e205 = vec3<f32>(_e201.x, _e201.y, _e202);
            let _e211 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e205.x, _e205.y), i32(_e205.z));
            let _e214 = shadow;
            shadow = (_e214 + step(_e195, _e211.x));
            let _e216 = currentDepth;
            let _e217 = sc;
            let _e220 = texelSize[0u];
            let _e222 = (_e217.xy - vec2<f32>(_e220, 0f));
            let _e223 = layer;
            let _e226 = vec3<f32>(_e222.x, _e222.y, _e223);
            let _e232 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e226.x, _e226.y), i32(_e226.z));
            let _e235 = shadow;
            shadow = (_e235 + step(_e216, _e232.x));
            let _e237 = currentDepth;
            let _e238 = sc;
            let _e241 = texelSize[1u];
            let _e243 = (_e238.xy + vec2<f32>(0f, _e241));
            let _e244 = layer;
            let _e247 = vec3<f32>(_e243.x, _e243.y, _e244);
            let _e253 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e247.x, _e247.y), i32(_e247.z));
            let _e256 = shadow;
            shadow = (_e256 + step(_e237, _e253.x));
            let _e258 = currentDepth;
            let _e259 = sc;
            let _e262 = texelSize[1u];
            let _e264 = (_e259.xy - vec2<f32>(0f, _e262));
            let _e265 = layer;
            let _e268 = vec3<f32>(_e264.x, _e264.y, _e265);
            let _e274 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e268.x, _e268.y), i32(_e268.z));
            let _e277 = shadow;
            shadow = (_e277 + step(_e258, _e274.x));
            let _e279 = shadow;
            shadow = (_e279 / 5f);
        } else {
            x = -1i;
            loop {
                let _e281 = x;
                if (_e281 <= 1i) {
                    y = -1i;
                    loop {
                        let _e283 = y;
                        if (_e283 <= 1i) {
                            let _e285 = currentDepth;
                            let _e286 = sc;
                            let _e288 = x;
                            let _e290 = y;
                            let _e293 = texelSize;
                            let _e295 = (_e286.xy + (vec2<f32>(f32(_e288), f32(_e290)) * _e293));
                            let _e296 = layer;
                            let _e299 = vec3<f32>(_e295.x, _e295.y, _e296);
                            let _e305 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e299.x, _e299.y), i32(_e299.z));
                            let _e308 = shadow;
                            shadow = (_e308 + step(_e285, _e305.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e310 = y;
                            y = (_e310 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e312 = x;
                    x = (_e312 + 1i);
                }
            }
            let _e314 = shadow;
            shadow = (_e314 / 9f);
        }
    }
    let _e316 = shadow;
    return _e316;
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

    let _e107 = unnamed.cascadeSplits;
    let _e108 = (*viewDepth_1);
    cmp = step(_e107, vec4(_e108));
    let _e112 = cmp[0u];
    let _e114 = cmp[1u];
    let _e117 = cmp[2u];
    let _e120 = cmp[3u];
    cascade = min(i32((((_e112 + _e114) + _e117) + _e120)), 3i);
    let _e124 = cascade;
    (*outCascade) = _e124;
    let _e125 = cascade;
    if (_e125 == 0i) {
        local = 0f;
    } else {
        let _e127 = cascade;
        let _e132 = unnamed.cascadeSplits[max((_e127 - 1i), 0i)];
        local = _e132;
    }
    let _e133 = local;
    prevSplit = _e133;
    let _e134 = cascade;
    let _e137 = unnamed.cascadeSplits[_e134];
    farSplit = _e137;
    let _e138 = farSplit;
    let _e139 = prevSplit;
    blendRange = max((0.1f * (_e138 - _e139)), 1f);
    let _e143 = farSplit;
    let _e144 = (*viewDepth_1);
    let _e146 = blendRange;
    blendT = clamp(((_e143 - _e144) / _e146), 0f, 1f);
    let _e149 = cascade;
    param = _e149;
    let _e150 = (*worldPos_1);
    param_1 = _e150;
    let _e151 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e151;
    let _e152 = cascade;
    param_2 = min((_e152 + 1i), 3i);
    let _e155 = (*worldPos_1);
    param_3 = _e155;
    let _e156 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e156;
    let _e157 = s1_;
    let _e158 = s0_;
    let _e159 = blendT;
    return mix(_e157, _e158, _e159);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e101 = unnamed.packed_indices[1i][3u];
    let _e107 = unnamed.packed_indices[1i][3u];
    let _e112 = (*lm_uv);
    let _e113 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    sun_mask = _e113.x;
    let _e115 = sun_mask;
    if (_e115 > 0.001f) {
        let _e117 = shadowData_1;
        param_4 = _e117.xyz;
        let _e120 = shadowData_1[3u];
        param_5 = _e120;
        let _e121 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e122 = param_6;
        ignoredCascade = _e122;
        shadow_1 = _e121;
        let _e123 = shadow_1;
        let _e124 = sun_mask;
        let _e126 = (*rgb);
        (*rgb) = (_e126 * mix(1f, _e123, _e124));
    }
    let _e128 = (*rgb);
    return _e128;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_4_2 {
        let _e95 = (*rgb_1);
        param_7 = _e95;
        let _e96 = frag_tex_coord0_1;
        param_8 = _e96;
        let _e97 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e97;
    }
    if override_type_4_3 {
        let _e98 = (*rgb_1);
        param_9 = _e98;
        let _e99 = frag_tex_coord1_1;
        param_10 = _e99;
        let _e100 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e100;
    }
    let _e101 = (*rgb_1);
    return _e101;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e93 = (*rgb_2);
    let _e96 = unnamed.worldLightParams[0u];
    boosted = (_e93 * _e96);
    let _e99 = boosted[0u];
    let _e101 = boosted[1u];
    let _e103 = boosted[2u];
    peak = max(_e99, max(_e101, _e103));
    let _e106 = peak;
    if (_e106 > 1f) {
        let _e108 = peak;
        let _e109 = boosted;
        boosted = (_e109 / vec3(_e108));
    }
    let _e112 = boosted;
    return _e112;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e94 = (*c_1);
    (*c_1) = max(_e94, vec3<f32>(0f, 0f, 0f));
    let _e96 = (*c_1);
    cutoff = (_e96 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e98 = (*c_1);
    lo = (_e98 / vec3(12.92f));
    let _e101 = (*c_1);
    hi = pow(((_e101 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e106 = hi;
    let _e107 = lo;
    let _e108 = cutoff;
    return mix(_e106, _e107, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e108));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e106 = (*role);
    let _e108 = (*role);
    let _e113 = unnamed.packed_indices[(_e106 / 4u)][(_e108 % 4u)];
    let _e118 = (*uv);
    let _e119 = textureSample(wired_bindless_images[(_e103 & 4095u)], wired_bindless_samplers[((_e113 >> bitcast<u32>(12i)) & 255u)], _e118);
    c_2 = _e119;
    let _e120 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e120))) == 0i) {
        let _e125 = c_2;
        param_11 = _e125.xyz;
        let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e127.x;
        c_2[1u] = _e127.y;
        c_2[2u] = _e127.z;
    }
    let _e134 = (*slot);
    if (lightmap_slot == (_e134 + 1i)) {
        let _e137 = c_2;
        param_12 = _e137.xyz;
        let _e139 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e139.x;
        c_2[1u] = _e139.y;
        c_2[2u] = _e139.z;
    }
    let _e146 = c_2;
    return _e146;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e113 = unnamed.packed_indices[0i][3u];
    let _e119 = unnamed.packed_indices[0i][3u];
    let _e124 = fog_tex_coord_1;
    let _e125 = textureSample(wired_bindless_images[(_e113 & 4095u)], wired_bindless_samplers[((_e119 >> bitcast<u32>(12i)) & 255u)], _e124);
    fog = _e125;
    param_13 = 0u;
    let _e126 = frag_tex_coord0_1;
    param_14 = _e126;
    param_15 = 0i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    sampledColor0_ = _e127;
    param_16 = 1u;
    let _e128 = frag_tex_coord1_1;
    param_17 = _e128;
    param_18 = 1i;
    let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor1_ = _e129;
    let _e130 = sampledColor0_;
    color0_ = _e130;
    if override_type_4_4 {
        let _e131 = sampledColor1_;
        color1_ = _e131;
        let _e132 = color0_;
        let _e134 = color1_;
        let _e136 = (_e132.xyz + _e134.xyz);
        let _e138 = color0_[3u];
        let _e140 = color1_[3u];
        base = vec4<f32>(_e136.x, _e136.y, _e136.z, (_e138 * _e140));
    } else {
        if override_type_4_5 {
            let _e146 = sampledColor1_;
            color1_1 = _e146;
            let _e147 = color0_;
            let _e149 = color1_1;
            let _e151 = (_e147.xyz + _e149.xyz);
            let _e153 = color0_[3u];
            let _e155 = color1_1[3u];
            base = vec4<f32>(_e151.x, _e151.y, _e151.z, (_e153 * _e155));
        } else {
            let _e161 = sampledColor1_;
            color1_2 = _e161;
            let _e162 = color0_;
            let _e164 = color1_2;
            let _e166 = (_e162.xyz * _e164.xyz);
            base[0u] = _e166.x;
            base[1u] = _e166.y;
            base[2u] = _e166.z;
            let _e174 = color0_[3u];
            let _e176 = color1_2[3u];
            base[3u] = (_e174 * _e176);
        }
    }
    let _e179 = base;
    param_19 = _e179.xyz;
    let _e181 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_19));
    base[0u] = _e181.x;
    base[1u] = _e181.y;
    base[2u] = _e181.z;
    if override_type_4_6 {
        let _e190 = unnamed.worldLightParams[1u];
        wetness = clamp(_e190, 0f, 1f);
        let _e194 = unnamed.worldLightParams[2u];
        frost = clamp(_e194, 0f, 1f);
        let _e196 = base;
        luminance = dot(_e196.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e199 = wetness;
        let _e201 = base;
        let _e203 = (_e201.xyz * mix(1f, 0.82f, _e199));
        base[0u] = _e203.x;
        base[1u] = _e203.y;
        base[2u] = _e203.z;
        let _e210 = base;
        let _e212 = luminance;
        let _e214 = luminance;
        let _e216 = luminance;
        let _e218 = frost;
        let _e221 = mix(_e210.xyz, vec3<f32>((_e212 * 0.88f), (_e214 * 0.94f), _e216), vec3((_e218 * 0.55f)));
        base[0u] = _e221.x;
        base[1u] = _e221.y;
        base[2u] = _e221.z;
    }
    let _e230 = unnamed.materialFactors[0u];
    if (_e230 >= 0f) {
        let _e233 = unnamed.materialBaseColor;
        let _e234 = base;
        base = (_e234 * _e233);
    }
    let _e236 = color0_;
    let _e239 = unnamed.emissionRadiance;
    let _e242 = base;
    let _e244 = (_e242.xyz + (_e236.xyz * _e239.xyz));
    base[0u] = _e244.x;
    base[1u] = _e244.y;
    base[2u] = _e244.z;
    let _e251 = wired_advanced_fog_enabled_u0028_();
    if _e251 {
        let _e252 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e252;
        if override_type_4_7 {
            let _e253 = fogAmount;
            let _e255 = base;
            let _e257 = (_e255.xyz * (1f - _e253));
            base[0u] = _e257.x;
            base[1u] = _e257.y;
            base[2u] = _e257.z;
        } else {
            if override_type_4_8 {
                let _e264 = fogAmount;
                let _e266 = base;
                base = (_e266 * (1f - _e264));
            } else {
                if override_type_4_9 {
                    let _e268 = fogAmount;
                    let _e271 = base[3u];
                    base[3u] = (_e271 * (1f - _e268));
                } else {
                    let _e274 = base;
                    let _e277 = unnamed.advancedFogColorDensity;
                    let _e279 = fogAmount;
                    let _e281 = mix(_e274.xyz, _e277.xyz, vec3(_e279));
                    base[0u] = _e281.x;
                    base[1u] = _e281.y;
                    base[2u] = _e281.z;
                }
            }
        }
    } else {
        if override_type_4_10 {
            let _e288 = base;
            let _e291 = fog[3u];
            let _e293 = (_e288.xyz * (1f - _e291));
            base[0u] = _e293.x;
            base[1u] = _e293.y;
            base[2u] = _e293.z;
        } else {
            if override_type_4_11 {
                let _e300 = base;
                let _e302 = fog[3u];
                base = (_e300 * (1f - _e302));
            } else {
                if override_type_4_12 {
                    let _e306 = base[3u];
                    let _e308 = fog[3u];
                    base[3u] = (_e306 * (1f - _e308));
                } else {
                    let _e312 = base;
                    let _e313 = fog;
                    let _e315 = unnamed.fogColor;
                    let _e318 = fog[3u];
                    base = mix(_e312, (_e313 * _e315), vec4(_e318));
                }
            }
        }
    }
    if override_type_4_13 {
        let _e322 = base[3u];
        if (_e322 == 0f) {
            discard;
        }
    } else {
        if override_type_4_14 {
            let _e324 = base;
            let _e326 = base;
            if (dot(_e324.xyz, _e326.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e330 = base;
    out_color = _e330;
    param_20 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_20));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
