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
override override_type_4_7: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_8: bool = (acff == 1i);
override override_type_4_9: bool = (acff == 2i);
override override_type_4_10: bool = (acff == 3i);
override override_type_4_11: bool = (acff == 1i);
override override_type_4_12: bool = (acff == 2i);
override override_type_4_13: bool = (acff == 3i);
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

    let _e96 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e96 + 0.5f));
    let _e101 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e103 = fogType;
    let _e106 = fogType;
    return (((_e101 > 0.5f) && (_e103 >= 1i)) && (_e106 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e96 = wired_advanced_fog_enabled_u0028_();
    if !(_e96) {
        return 0f;
    }
    let _e99 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e99, 0.000001f));
    let _e104 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e104 + 0.5f));
    let _e107 = fogType_1;
    if (_e107 == 1i) {
        let _e111 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e111 <= 0f) {
            return 0f;
        }
        let _e113 = viewDepth;
        let _e116 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e113 / _e116), 0f, 1f);
    }
    let _e121 = unnamed.advancedFogColorDensity[3u];
    let _e123 = viewDepth;
    opticalDepth = (max(_e121, 0f) * _e123);
    let _e125 = fogType_1;
    if (_e125 == 2i) {
        let _e127 = opticalDepth;
        return clamp((1f - exp(-(_e127))), 0f, 1f);
    }
    let _e132 = opticalDepth;
    let _e133 = opticalDepth;
    return clamp((1f - exp(-((_e132 * _e133)))), 0f, 1f);
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

    let _e103 = (*c);
    let _e106 = unnamed.cascadeMVP[_e103];
    let _e107 = (*worldPos);
    sc4_ = (_e106 * vec4<f32>(_e107.x, _e107.y, _e107.z, 1f));
    let _e113 = sc4_;
    let _e116 = sc4_[3u];
    sc = (_e113.xyz / vec3(_e116));
    let _e119 = sc;
    let _e123 = ((_e119.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e123.x;
    sc[1u] = _e123.y;
    let _e129 = sc[0u];
    let _e130 = (_e129 < 0f);
    phi_340_ = _e130;
    if !(_e130) {
        let _e133 = sc[0u];
        phi_340_ = (_e133 > 1f);
    }
    let _e136 = phi_340_;
    phi_347_ = _e136;
    if !(_e136) {
        let _e139 = sc[1u];
        phi_347_ = (_e139 < 0f);
    }
    let _e142 = phi_347_;
    phi_354_ = _e142;
    if !(_e142) {
        let _e145 = sc[1u];
        phi_354_ = (_e145 > 1f);
    }
    let _e148 = phi_354_;
    phi_361_ = _e148;
    if !(_e148) {
        let _e151 = sc[2u];
        phi_361_ = (_e151 > 1f);
    }
    let _e154 = phi_361_;
    if _e154 {
        return 1f;
    }
    let _e155 = (*c);
    layer = f32(_e155);
    let _e157 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e157).xy));
    let _e164 = sc[2u];
    currentDepth = (_e164 - shadow_bias);
    shadow = 0f;
    if override_type_4_ {
        let _e166 = currentDepth;
        let _e167 = sc;
        let _e168 = _e167.xy;
        let _e169 = layer;
        let _e172 = vec3<f32>(_e168.x, _e168.y, _e169);
        let _e178 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e172.x, _e172.y), i32(_e172.z));
        shadow = step(_e166, _e178.x);
    } else {
        if override_type_4_1 {
            let _e181 = currentDepth;
            let _e182 = sc;
            let _e183 = _e182.xy;
            let _e184 = layer;
            let _e187 = vec3<f32>(_e183.x, _e183.y, _e184);
            let _e193 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e187.x, _e187.y), i32(_e187.z));
            let _e196 = shadow;
            shadow = (_e196 + step(_e181, _e193.x));
            let _e198 = currentDepth;
            let _e199 = sc;
            let _e202 = texelSize[0u];
            let _e204 = (_e199.xy + vec2<f32>(_e202, 0f));
            let _e205 = layer;
            let _e208 = vec3<f32>(_e204.x, _e204.y, _e205);
            let _e214 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e208.x, _e208.y), i32(_e208.z));
            let _e217 = shadow;
            shadow = (_e217 + step(_e198, _e214.x));
            let _e219 = currentDepth;
            let _e220 = sc;
            let _e223 = texelSize[0u];
            let _e225 = (_e220.xy - vec2<f32>(_e223, 0f));
            let _e226 = layer;
            let _e229 = vec3<f32>(_e225.x, _e225.y, _e226);
            let _e235 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e229.x, _e229.y), i32(_e229.z));
            let _e238 = shadow;
            shadow = (_e238 + step(_e219, _e235.x));
            let _e240 = currentDepth;
            let _e241 = sc;
            let _e244 = texelSize[1u];
            let _e246 = (_e241.xy + vec2<f32>(0f, _e244));
            let _e247 = layer;
            let _e250 = vec3<f32>(_e246.x, _e246.y, _e247);
            let _e256 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e250.x, _e250.y), i32(_e250.z));
            let _e259 = shadow;
            shadow = (_e259 + step(_e240, _e256.x));
            let _e261 = currentDepth;
            let _e262 = sc;
            let _e265 = texelSize[1u];
            let _e267 = (_e262.xy - vec2<f32>(0f, _e265));
            let _e268 = layer;
            let _e271 = vec3<f32>(_e267.x, _e267.y, _e268);
            let _e277 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e271.x, _e271.y), i32(_e271.z));
            let _e280 = shadow;
            shadow = (_e280 + step(_e261, _e277.x));
            let _e282 = shadow;
            shadow = (_e282 / 5f);
        } else {
            x = -1i;
            loop {
                let _e284 = x;
                if (_e284 <= 1i) {
                    y = -1i;
                    loop {
                        let _e286 = y;
                        if (_e286 <= 1i) {
                            let _e288 = currentDepth;
                            let _e289 = sc;
                            let _e291 = x;
                            let _e293 = y;
                            let _e296 = texelSize;
                            let _e298 = (_e289.xy + (vec2<f32>(f32(_e291), f32(_e293)) * _e296));
                            let _e299 = layer;
                            let _e302 = vec3<f32>(_e298.x, _e298.y, _e299);
                            let _e308 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e302.x, _e302.y), i32(_e302.z));
                            let _e311 = shadow;
                            shadow = (_e311 + step(_e288, _e308.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e313 = y;
                            y = (_e313 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e315 = x;
                    x = (_e315 + 1i);
                }
            }
            let _e317 = shadow;
            shadow = (_e317 / 9f);
        }
    }
    let _e319 = shadow;
    return _e319;
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

    let _e110 = unnamed.cascadeSplits;
    let _e111 = (*viewDepth_1);
    cmp = step(_e110, vec4(_e111));
    let _e115 = cmp[0u];
    let _e117 = cmp[1u];
    let _e120 = cmp[2u];
    let _e123 = cmp[3u];
    cascade = min(i32((((_e115 + _e117) + _e120) + _e123)), 3i);
    let _e127 = cascade;
    (*outCascade) = _e127;
    let _e128 = cascade;
    if (_e128 == 0i) {
        local = 0f;
    } else {
        let _e130 = cascade;
        let _e135 = unnamed.cascadeSplits[max((_e130 - 1i), 0i)];
        local = _e135;
    }
    let _e136 = local;
    prevSplit = _e136;
    let _e137 = cascade;
    let _e140 = unnamed.cascadeSplits[_e137];
    farSplit = _e140;
    let _e141 = farSplit;
    let _e142 = prevSplit;
    blendRange = max((0.1f * (_e141 - _e142)), 1f);
    let _e146 = farSplit;
    let _e147 = (*viewDepth_1);
    let _e149 = blendRange;
    blendT = clamp(((_e146 - _e147) / _e149), 0f, 1f);
    let _e152 = cascade;
    param = _e152;
    let _e153 = (*worldPos_1);
    param_1 = _e153;
    let _e154 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e154;
    let _e155 = cascade;
    param_2 = min((_e155 + 1i), 3i);
    let _e158 = (*worldPos_1);
    param_3 = _e158;
    let _e159 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e159;
    let _e160 = s1_;
    let _e161 = s0_;
    let _e162 = blendT;
    return mix(_e160, _e161, _e162);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e104 = unnamed.packed_indices[1i][3u];
    let _e110 = unnamed.packed_indices[1i][3u];
    let _e115 = (*lm_uv);
    let _e116 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    sun_mask = _e116.x;
    let _e118 = sun_mask;
    if (_e118 > 0.001f) {
        let _e120 = shadowData_1;
        param_4 = _e120.xyz;
        let _e123 = shadowData_1[3u];
        param_5 = _e123;
        let _e124 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e125 = param_6;
        ignoredCascade = _e125;
        shadow_1 = _e124;
        let _e126 = shadow_1;
        let _e127 = sun_mask;
        let _e129 = (*rgb);
        (*rgb) = (_e129 * mix(1f, _e126, _e127));
    }
    let _e131 = (*rgb);
    return _e131;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_4_2 {
        let _e100 = (*rgb_1);
        param_7 = _e100;
        let _e101 = frag_tex_coord0_1;
        param_8 = _e101;
        let _e102 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e102;
    }
    if override_type_4_3 {
        let _e103 = (*rgb_1);
        param_9 = _e103;
        let _e104 = frag_tex_coord1_1;
        param_10 = _e104;
        let _e105 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e105;
    }
    if override_type_4_4 {
        let _e106 = (*rgb_1);
        param_11 = _e106;
        let _e107 = frag_tex_coord2_1;
        param_12 = _e107;
        let _e108 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e108;
    }
    let _e109 = (*rgb_1);
    return _e109;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e96 = (*rgb_2);
    let _e99 = unnamed.worldLightParams[0u];
    boosted = (_e96 * _e99);
    let _e102 = boosted[0u];
    let _e104 = boosted[1u];
    let _e106 = boosted[2u];
    peak = max(_e102, max(_e104, _e106));
    let _e109 = peak;
    if (_e109 > 1f) {
        let _e111 = peak;
        let _e112 = boosted;
        boosted = (_e112 / vec3(_e111));
    }
    let _e115 = boosted;
    return _e115;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e97 = (*c_1);
    (*c_1) = max(_e97, vec3<f32>(0f, 0f, 0f));
    let _e99 = (*c_1);
    cutoff = (_e99 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e101 = (*c_1);
    lo = (_e101 / vec3(12.92f));
    let _e104 = (*c_1);
    hi = pow(((_e104 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e109 = hi;
    let _e110 = lo;
    let _e111 = cutoff;
    return mix(_e109, _e110, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e111));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e109 = (*role);
    let _e111 = (*role);
    let _e116 = unnamed.packed_indices[(_e109 / 4u)][(_e111 % 4u)];
    let _e121 = (*uv);
    let _e122 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e116 >> bitcast<u32>(12i)) & 255u)], _e121);
    c_2 = _e122;
    let _e123 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e123))) == 0i) {
        let _e128 = c_2;
        param_13 = _e128.xyz;
        let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e130.x;
        c_2[1u] = _e130.y;
        c_2[2u] = _e130.z;
    }
    let _e137 = (*slot);
    if (lightmap_slot == (_e137 + 1i)) {
        let _e140 = c_2;
        param_14 = _e140.xyz;
        let _e142 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e142.x;
        c_2[1u] = _e142.y;
        c_2[2u] = _e142.z;
    }
    let _e149 = c_2;
    return _e149;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_15: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var sampledColor1_: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var sampledColor2_: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var param_25: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_26: f32;

    let _e125 = unnamed.packed_indices[0i][3u];
    let _e131 = unnamed.packed_indices[0i][3u];
    let _e136 = fog_tex_coord_1;
    let _e137 = textureSample(wired_bindless_images[(_e125 & 4095u)], wired_bindless_samplers[((_e131 >> bitcast<u32>(12i)) & 255u)], _e136);
    fog = _e137;
    let _e138 = frag_color0In_1;
    param_15 = _e138.xyz;
    let _e140 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e142 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e140.x, _e140.y, _e140.z, _e142);
    param_16 = 0u;
    let _e147 = frag_tex_coord0_1;
    param_17 = _e147;
    param_18 = 0i;
    let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor0_ = _e148;
    param_19 = 1u;
    let _e149 = frag_tex_coord1_1;
    param_20 = _e149;
    param_21 = 1i;
    let _e150 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor1_ = _e150;
    param_22 = 2u;
    let _e151 = frag_tex_coord2_1;
    param_23 = _e151;
    param_24 = 2i;
    let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor2_ = _e152;
    let _e153 = sampledColor0_;
    let _e154 = frag_color0_;
    color0_ = (_e153 * _e154);
    if override_type_4_5 {
        let _e156 = sampledColor1_;
        color1_ = _e156;
        let _e157 = sampledColor2_;
        color2_ = _e157;
        let _e158 = color0_;
        let _e160 = color1_;
        let _e163 = color2_;
        let _e165 = ((_e158.xyz + _e160.xyz) + _e163.xyz);
        let _e167 = color0_[3u];
        let _e169 = color1_[3u];
        let _e172 = color2_[3u];
        base = vec4<f32>(_e165.x, _e165.y, _e165.z, ((_e167 * _e169) * _e172));
    } else {
        if override_type_4_6 {
            let _e178 = sampledColor1_;
            let _e179 = frag_color0_;
            color1_1 = (_e178 * _e179);
            let _e181 = sampledColor2_;
            let _e182 = frag_color0_;
            color2_1 = (_e181 * _e182);
            let _e184 = color0_;
            let _e186 = color1_1;
            let _e189 = color2_1;
            let _e191 = ((_e184.xyz + _e186.xyz) + _e189.xyz);
            let _e193 = color0_[3u];
            let _e195 = color1_1[3u];
            let _e198 = color2_1[3u];
            base = vec4<f32>(_e191.x, _e191.y, _e191.z, ((_e193 * _e195) * _e198));
        } else {
            let _e204 = sampledColor1_;
            color1_2 = _e204;
            let _e205 = sampledColor2_;
            color2_2 = _e205;
            let _e206 = color0_;
            let _e208 = color1_2;
            let _e211 = color2_2;
            let _e213 = ((_e206.xyz * _e208.xyz) * _e211.xyz);
            base[0u] = _e213.x;
            base[1u] = _e213.y;
            base[2u] = _e213.z;
            let _e221 = color0_[3u];
            let _e223 = color1_2[3u];
            let _e226 = color2_2[3u];
            base[3u] = ((_e221 * _e223) * _e226);
        }
    }
    let _e229 = base;
    param_25 = _e229.xyz;
    let _e231 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e231.x;
    base[1u] = _e231.y;
    base[2u] = _e231.z;
    if override_type_4_7 {
        let _e240 = unnamed.worldLightParams[1u];
        wetness = clamp(_e240, 0f, 1f);
        let _e244 = unnamed.worldLightParams[2u];
        frost = clamp(_e244, 0f, 1f);
        let _e246 = base;
        luminance = dot(_e246.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e249 = wetness;
        let _e251 = base;
        let _e253 = (_e251.xyz * mix(1f, 0.82f, _e249));
        base[0u] = _e253.x;
        base[1u] = _e253.y;
        base[2u] = _e253.z;
        let _e260 = base;
        let _e262 = luminance;
        let _e264 = luminance;
        let _e266 = luminance;
        let _e268 = frost;
        let _e271 = mix(_e260.xyz, vec3<f32>((_e262 * 0.88f), (_e264 * 0.94f), _e266), vec3((_e268 * 0.55f)));
        base[0u] = _e271.x;
        base[1u] = _e271.y;
        base[2u] = _e271.z;
    }
    let _e280 = unnamed.materialFactors[0u];
    if (_e280 >= 0f) {
        let _e283 = unnamed.materialBaseColor;
        let _e284 = base;
        base = (_e284 * _e283);
    }
    let _e286 = color0_;
    let _e289 = unnamed.emissionRadiance;
    let _e292 = base;
    let _e294 = (_e292.xyz + (_e286.xyz * _e289.xyz));
    base[0u] = _e294.x;
    base[1u] = _e294.y;
    base[2u] = _e294.z;
    let _e301 = wired_advanced_fog_enabled_u0028_();
    if _e301 {
        let _e302 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e302;
        if override_type_4_8 {
            let _e303 = fogAmount;
            let _e305 = base;
            let _e307 = (_e305.xyz * (1f - _e303));
            base[0u] = _e307.x;
            base[1u] = _e307.y;
            base[2u] = _e307.z;
        } else {
            if override_type_4_9 {
                let _e314 = fogAmount;
                let _e316 = base;
                base = (_e316 * (1f - _e314));
            } else {
                if override_type_4_10 {
                    let _e318 = fogAmount;
                    let _e321 = base[3u];
                    base[3u] = (_e321 * (1f - _e318));
                } else {
                    let _e324 = base;
                    let _e327 = unnamed.advancedFogColorDensity;
                    let _e329 = fogAmount;
                    let _e331 = mix(_e324.xyz, _e327.xyz, vec3(_e329));
                    base[0u] = _e331.x;
                    base[1u] = _e331.y;
                    base[2u] = _e331.z;
                }
            }
        }
    } else {
        if override_type_4_11 {
            let _e338 = base;
            let _e341 = fog[3u];
            let _e343 = (_e338.xyz * (1f - _e341));
            base[0u] = _e343.x;
            base[1u] = _e343.y;
            base[2u] = _e343.z;
        } else {
            if override_type_4_12 {
                let _e350 = base;
                let _e352 = fog[3u];
                base = (_e350 * (1f - _e352));
            } else {
                if override_type_4_13 {
                    let _e356 = base[3u];
                    let _e358 = fog[3u];
                    base[3u] = (_e356 * (1f - _e358));
                } else {
                    let _e362 = base;
                    let _e363 = fog;
                    let _e365 = unnamed.fogColor;
                    let _e368 = fog[3u];
                    base = mix(_e362, (_e363 * _e365), vec4(_e368));
                }
            }
        }
    }
    if override_type_4_14 {
        let _e372 = base[3u];
        if (_e372 == 0f) {
            discard;
        }
    } else {
        if override_type_4_15 {
            let _e374 = base;
            let _e376 = base;
            if (dot(_e374.xyz, _e376.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e380 = base;
    out_color = _e380;
    param_26 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_26));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
