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
@id(7) override discard_mode: i32 = 0i;
override override_type_6_13: bool = (discard_mode == 1i);
override override_type_6_14: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e96 = (*sceneColor)[0u];
    let _e98 = (*sceneColor)[1u];
    let _e100 = (*sceneColor)[2u];
    let _e105 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e96, max(_e98, _e100)), 0f, 1f), 0f, 0f, clamp(_e105, 0f, 1f));
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
    var phi_355_: bool;
    var phi_362_: bool;
    var phi_369_: bool;
    var phi_376_: bool;

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
    phi_355_ = _e130;
    if !(_e130) {
        let _e133 = sc[0u];
        phi_355_ = (_e133 > 1f);
    }
    let _e136 = phi_355_;
    phi_362_ = _e136;
    if !(_e136) {
        let _e139 = sc[1u];
        phi_362_ = (_e139 < 0f);
    }
    let _e142 = phi_362_;
    phi_369_ = _e142;
    if !(_e142) {
        let _e145 = sc[1u];
        phi_369_ = (_e145 > 1f);
    }
    let _e148 = phi_369_;
    phi_376_ = _e148;
    if !(_e148) {
        let _e151 = sc[2u];
        phi_376_ = (_e151 > 1f);
    }
    let _e154 = phi_376_;
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
    if override_type_6_ {
        let _e166 = currentDepth;
        let _e167 = sc;
        let _e168 = _e167.xy;
        let _e169 = layer;
        let _e172 = vec3<f32>(_e168.x, _e168.y, _e169);
        let _e178 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e172.x, _e172.y), i32(_e172.z));
        shadow = step(_e166, _e178.x);
    } else {
        if override_type_6_1 {
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

    if override_type_6_2 {
        let _e98 = (*rgb_1);
        param_7 = _e98;
        let _e99 = frag_tex_coord0_1;
        param_8 = _e99;
        let _e100 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e100;
    }
    if override_type_6_3 {
        let _e101 = (*rgb_1);
        param_9 = _e101;
        let _e102 = frag_tex_coord1_1;
        param_10 = _e102;
        let _e103 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e103;
    }
    let _e104 = (*rgb_1);
    return _e104;
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
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;

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
        param_11 = _e128.xyz;
        let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e130.x;
        c_2[1u] = _e130.y;
        c_2[2u] = _e130.z;
    }
    let _e137 = (*slot);
    if (lightmap_slot == (_e137 + 1i)) {
        let _e140 = c_2;
        param_12 = _e140.xyz;
        let _e142 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_12));
        c_2[0u] = _e142.x;
        c_2[1u] = _e142.y;
        c_2[2u] = _e142.z;
    }
    let _e149 = c_2;
    return _e149;
}

fn main_1() {
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

    let _e121 = frag_color0In_1;
    param_13 = _e121.xyz;
    let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e125 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e123.x, _e123.y, _e123.z, _e125);
    let _e130 = frag_color1In_1;
    param_14 = _e130.xyz;
    let _e132 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e134 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e132.x, _e132.y, _e132.z, _e134);
    param_15 = 0u;
    let _e139 = frag_tex_coord0_1;
    param_16 = _e139;
    param_17 = 0i;
    let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
    sampledColor0_ = _e140;
    param_18 = 1u;
    let _e141 = frag_tex_coord1_1;
    param_19 = _e141;
    param_20 = 1i;
    let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor1_ = _e142;
    let _e143 = sampledColor0_;
    let _e144 = frag_color0_;
    color0_ = (_e143 * _e144);
    if override_type_6_6 {
        let _e146 = sampledColor1_;
        let _e147 = frag_color1_;
        color1_ = (_e146 * _e147);
        let _e149 = color0_;
        let _e151 = color1_;
        let _e153 = (_e149.xyz + _e151.xyz);
        let _e155 = color0_[3u];
        let _e157 = color1_[3u];
        base = vec4<f32>(_e153.x, _e153.y, _e153.z, (_e155 * _e157));
    } else {
        if override_type_6_7 {
            let _e163 = sampledColor1_;
            let _e164 = frag_color1_;
            color1_1 = (_e163 * _e164);
            let _e167 = color0_[3u];
            let _e168 = color0_;
            color0_ = (_e168 * _e167);
            let _e171 = color1_1[3u];
            let _e172 = color1_1;
            color1_1 = (_e172 * _e171);
            let _e174 = color0_;
            let _e176 = color1_1;
            let _e178 = (_e174.xyz + _e176.xyz);
            let _e180 = color0_[3u];
            let _e182 = color1_1[3u];
            base = vec4<f32>(_e178.x, _e178.y, _e178.z, (_e180 * _e182));
        } else {
            if override_type_6_8 {
                let _e188 = sampledColor1_;
                let _e189 = frag_color1_;
                color1_2 = (_e188 * _e189);
                let _e192 = color0_[3u];
                let _e194 = color0_;
                color0_ = (_e194 * (1f - _e192));
                let _e197 = color1_2[3u];
                let _e199 = color1_2;
                color1_2 = (_e199 * (1f - _e197));
                let _e201 = color0_;
                let _e203 = color1_2;
                let _e205 = (_e201.xyz + _e203.xyz);
                let _e207 = color0_[3u];
                let _e209 = color1_2[3u];
                base = vec4<f32>(_e205.x, _e205.y, _e205.z, (_e207 * _e209));
            } else {
                if override_type_6_9 {
                    let _e215 = sampledColor1_;
                    let _e216 = frag_color1_;
                    color1_3 = (_e215 * _e216);
                    let _e218 = color0_;
                    let _e219 = color1_3;
                    let _e221 = color1_3[3u];
                    base = mix(_e218, _e219, vec4(_e221));
                } else {
                    if override_type_6_10 {
                        let _e224 = sampledColor1_;
                        let _e225 = frag_color1_;
                        color1_4 = (_e224 * _e225);
                        let _e227 = color1_4;
                        let _e228 = color0_;
                        let _e230 = color1_4[3u];
                        base = mix(_e227, _e228, vec4(_e230));
                    } else {
                        if override_type_6_11 {
                            let _e233 = sampledColor1_;
                            let _e234 = frag_color1_;
                            color1_5 = (_e233 * _e234);
                            let _e236 = color1_5;
                            let _e238 = color1_5[3u];
                            let _e241 = color0_;
                            base = ((_e236 + vec4(_e238)) * _e241);
                        } else {
                            let _e243 = sampledColor1_;
                            let _e244 = frag_color1_;
                            color1_6 = (_e243 * _e244);
                            let _e246 = color0_;
                            let _e248 = color1_6;
                            let _e250 = (_e246.xyz * _e248.xyz);
                            base[0u] = _e250.x;
                            base[1u] = _e250.y;
                            base[2u] = _e250.z;
                            let _e258 = color0_[3u];
                            let _e260 = color1_6[3u];
                            base[3u] = (_e258 * _e260);
                        }
                    }
                }
            }
        }
    }
    let _e263 = base;
    param_21 = _e263.xyz;
    let _e265 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_21));
    base[0u] = _e265.x;
    base[1u] = _e265.y;
    base[2u] = _e265.z;
    if override_type_6_12 {
        let _e274 = unnamed.worldLightParams[1u];
        wetness = clamp(_e274, 0f, 1f);
        let _e278 = unnamed.worldLightParams[2u];
        frost = clamp(_e278, 0f, 1f);
        let _e280 = base;
        luminance = dot(_e280.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e283 = wetness;
        let _e285 = base;
        let _e287 = (_e285.xyz * mix(1f, 0.82f, _e283));
        base[0u] = _e287.x;
        base[1u] = _e287.y;
        base[2u] = _e287.z;
        let _e294 = base;
        let _e296 = luminance;
        let _e298 = luminance;
        let _e300 = luminance;
        let _e302 = frost;
        let _e305 = mix(_e294.xyz, vec3<f32>((_e296 * 0.88f), (_e298 * 0.94f), _e300), vec3((_e302 * 0.55f)));
        base[0u] = _e305.x;
        base[1u] = _e305.y;
        base[2u] = _e305.z;
    }
    let _e314 = unnamed.materialFactors[0u];
    if (_e314 >= 0f) {
        let _e317 = unnamed.materialBaseColor;
        let _e318 = base;
        base = (_e318 * _e317);
    }
    let _e320 = color0_;
    let _e323 = unnamed.emissionRadiance;
    let _e326 = base;
    let _e328 = (_e326.xyz + (_e320.xyz * _e323.xyz));
    base[0u] = _e328.x;
    base[1u] = _e328.y;
    base[2u] = _e328.z;
    let _e335 = wired_advanced_fog_enabled_u0028_();
    if _e335 {
        let _e336 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e336;
        let _e337 = base;
        let _e340 = unnamed.advancedFogColorDensity;
        let _e342 = fogAmount;
        let _e344 = mix(_e337.xyz, _e340.xyz, vec3(_e342));
        base[0u] = _e344.x;
        base[1u] = _e344.y;
        base[2u] = _e344.z;
    }
    if override_type_6_13 {
        let _e352 = base[3u];
        if (_e352 == 0f) {
            discard;
        }
    } else {
        if override_type_6_14 {
            let _e354 = base;
            let _e356 = base;
            if (dot(_e354.xyz, _e356.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e360 = base;
    out_color = _e360;
    param_22 = 1f;
    let _e361 = base;
    param_23 = _e361;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_22), (&param_23));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
