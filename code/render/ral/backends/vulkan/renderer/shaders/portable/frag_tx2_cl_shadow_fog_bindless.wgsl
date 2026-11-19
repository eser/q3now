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

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
override override_type_3_3: bool = (lightmap_slot == 2i);
override override_type_3_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_5: bool = (tex_mode == 1i);
override override_type_3_6: bool = (tex_mode == 2i);
override override_type_3_7: bool = (override_type_3_5 || override_type_3_6);
override override_type_3_8: bool = (tex_mode == 3i);
override override_type_3_9: bool = (tex_mode == 4i);
override override_type_3_10: bool = (tex_mode == 5i);
override override_type_3_11: bool = (tex_mode == 6i);
override override_type_3_12: bool = (tex_mode == 7i);
override override_type_3_13: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_14: bool = (acff == 1i);
override override_type_3_15: bool = (acff == 2i);
override override_type_3_16: bool = (acff == 3i);
override override_type_3_17: bool = (acff == 1i);
override override_type_3_18: bool = (acff == 2i);
override override_type_3_19: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_20: bool = (discard_mode == 1i);
override override_type_3_21: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

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

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e100 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e100 + 0.5f));
    let _e105 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e107 = fogType;
    let _e110 = fogType;
    return (((_e105 > 0.5f) && (_e107 >= 1i)) && (_e110 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e100 = wired_advanced_fog_enabled_u0028_();
    if !(_e100) {
        return 0f;
    }
    let _e103 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e103, 0.000001f));
    let _e108 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e108 + 0.5f));
    let _e111 = fogType_1;
    if (_e111 == 1i) {
        let _e115 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e115 <= 0f) {
            return 0f;
        }
        let _e117 = viewDepth;
        let _e120 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e117 / _e120), 0f, 1f);
    }
    let _e125 = unnamed.advancedFogColorDensity[3u];
    let _e127 = viewDepth;
    opticalDepth = (max(_e125, 0f) * _e127);
    let _e129 = fogType_1;
    if (_e129 == 2i) {
        let _e131 = opticalDepth;
        return clamp((1f - exp(-(_e131))), 0f, 1f);
    }
    let _e136 = opticalDepth;
    let _e137 = opticalDepth;
    return clamp((1f - exp(-((_e136 * _e137)))), 0f, 1f);
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
    var phi_331_: bool;
    var phi_338_: bool;
    var phi_345_: bool;
    var phi_352_: bool;

    let _e107 = (*c);
    let _e110 = unnamed.cascadeMVP[_e107];
    let _e111 = (*worldPos);
    sc4_ = (_e110 * vec4<f32>(_e111.x, _e111.y, _e111.z, 1f));
    let _e117 = sc4_;
    let _e120 = sc4_[3u];
    sc = (_e117.xyz / vec3(_e120));
    let _e123 = sc;
    let _e127 = ((_e123.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e127.x;
    sc[1u] = _e127.y;
    let _e133 = sc[0u];
    let _e134 = (_e133 < 0f);
    phi_331_ = _e134;
    if !(_e134) {
        let _e137 = sc[0u];
        phi_331_ = (_e137 > 1f);
    }
    let _e140 = phi_331_;
    phi_338_ = _e140;
    if !(_e140) {
        let _e143 = sc[1u];
        phi_338_ = (_e143 < 0f);
    }
    let _e146 = phi_338_;
    phi_345_ = _e146;
    if !(_e146) {
        let _e149 = sc[1u];
        phi_345_ = (_e149 > 1f);
    }
    let _e152 = phi_345_;
    phi_352_ = _e152;
    if !(_e152) {
        let _e155 = sc[2u];
        phi_352_ = (_e155 > 1f);
    }
    let _e158 = phi_352_;
    if _e158 {
        return 1f;
    }
    let _e159 = (*c);
    layer = f32(_e159);
    let _e161 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e161).xy));
    let _e168 = sc[2u];
    currentDepth = (_e168 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e170 = currentDepth;
        let _e171 = sc;
        let _e172 = _e171.xy;
        let _e173 = layer;
        let _e176 = vec3<f32>(_e172.x, _e172.y, _e173);
        let _e182 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e176.x, _e176.y), i32(_e176.z));
        shadow = step(_e170, _e182.x);
    } else {
        if override_type_3_1 {
            let _e185 = currentDepth;
            let _e186 = sc;
            let _e187 = _e186.xy;
            let _e188 = layer;
            let _e191 = vec3<f32>(_e187.x, _e187.y, _e188);
            let _e197 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e191.x, _e191.y), i32(_e191.z));
            let _e200 = shadow;
            shadow = (_e200 + step(_e185, _e197.x));
            let _e202 = currentDepth;
            let _e203 = sc;
            let _e206 = texelSize[0u];
            let _e208 = (_e203.xy + vec2<f32>(_e206, 0f));
            let _e209 = layer;
            let _e212 = vec3<f32>(_e208.x, _e208.y, _e209);
            let _e218 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e212.x, _e212.y), i32(_e212.z));
            let _e221 = shadow;
            shadow = (_e221 + step(_e202, _e218.x));
            let _e223 = currentDepth;
            let _e224 = sc;
            let _e227 = texelSize[0u];
            let _e229 = (_e224.xy - vec2<f32>(_e227, 0f));
            let _e230 = layer;
            let _e233 = vec3<f32>(_e229.x, _e229.y, _e230);
            let _e239 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e233.x, _e233.y), i32(_e233.z));
            let _e242 = shadow;
            shadow = (_e242 + step(_e223, _e239.x));
            let _e244 = currentDepth;
            let _e245 = sc;
            let _e248 = texelSize[1u];
            let _e250 = (_e245.xy + vec2<f32>(0f, _e248));
            let _e251 = layer;
            let _e254 = vec3<f32>(_e250.x, _e250.y, _e251);
            let _e260 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e254.x, _e254.y), i32(_e254.z));
            let _e263 = shadow;
            shadow = (_e263 + step(_e244, _e260.x));
            let _e265 = currentDepth;
            let _e266 = sc;
            let _e269 = texelSize[1u];
            let _e271 = (_e266.xy - vec2<f32>(0f, _e269));
            let _e272 = layer;
            let _e275 = vec3<f32>(_e271.x, _e271.y, _e272);
            let _e281 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e275.x, _e275.y), i32(_e275.z));
            let _e284 = shadow;
            shadow = (_e284 + step(_e265, _e281.x));
            let _e286 = shadow;
            shadow = (_e286 / 5f);
        } else {
            x = -1i;
            loop {
                let _e288 = x;
                if (_e288 <= 1i) {
                    y = -1i;
                    loop {
                        let _e290 = y;
                        if (_e290 <= 1i) {
                            let _e292 = currentDepth;
                            let _e293 = sc;
                            let _e295 = x;
                            let _e297 = y;
                            let _e300 = texelSize;
                            let _e302 = (_e293.xy + (vec2<f32>(f32(_e295), f32(_e297)) * _e300));
                            let _e303 = layer;
                            let _e306 = vec3<f32>(_e302.x, _e302.y, _e303);
                            let _e312 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e306.x, _e306.y), i32(_e306.z));
                            let _e315 = shadow;
                            shadow = (_e315 + step(_e292, _e312.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e317 = y;
                            y = (_e317 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e319 = x;
                    x = (_e319 + 1i);
                }
            }
            let _e321 = shadow;
            shadow = (_e321 / 9f);
        }
    }
    let _e323 = shadow;
    return _e323;
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

    let _e114 = unnamed.cascadeSplits;
    let _e115 = (*viewDepth_1);
    cmp = step(_e114, vec4(_e115));
    let _e119 = cmp[0u];
    let _e121 = cmp[1u];
    let _e124 = cmp[2u];
    let _e127 = cmp[3u];
    cascade = min(i32((((_e119 + _e121) + _e124) + _e127)), 3i);
    let _e131 = cascade;
    (*outCascade) = _e131;
    let _e132 = cascade;
    if (_e132 == 0i) {
        local = 0f;
    } else {
        let _e134 = cascade;
        let _e139 = unnamed.cascadeSplits[max((_e134 - 1i), 0i)];
        local = _e139;
    }
    let _e140 = local;
    prevSplit = _e140;
    let _e141 = cascade;
    let _e144 = unnamed.cascadeSplits[_e141];
    farSplit = _e144;
    let _e145 = farSplit;
    let _e146 = prevSplit;
    blendRange = max((0.1f * (_e145 - _e146)), 1f);
    let _e150 = farSplit;
    let _e151 = (*viewDepth_1);
    let _e153 = blendRange;
    blendT = clamp(((_e150 - _e151) / _e153), 0f, 1f);
    let _e156 = cascade;
    param = _e156;
    let _e157 = (*worldPos_1);
    param_1 = _e157;
    let _e158 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e158;
    let _e159 = cascade;
    param_2 = min((_e159 + 1i), 3i);
    let _e162 = (*worldPos_1);
    param_3 = _e162;
    let _e163 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e163;
    let _e164 = s1_;
    let _e165 = s0_;
    let _e166 = blendT;
    return mix(_e164, _e165, _e166);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e108 = unnamed.packed_indices[1i][3u];
    let _e114 = unnamed.packed_indices[1i][3u];
    let _e119 = (*lm_uv);
    let _e120 = textureSample(wired_bindless_images[(_e108 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    sun_mask = _e120.x;
    let _e122 = sun_mask;
    if (_e122 > 0.001f) {
        let _e124 = shadowData_1;
        param_4 = _e124.xyz;
        let _e127 = shadowData_1[3u];
        param_5 = _e127;
        let _e128 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e129 = param_6;
        ignoredCascade = _e129;
        shadow_1 = _e128;
        let _e130 = shadow_1;
        let _e131 = sun_mask;
        let _e133 = (*rgb);
        (*rgb) = (_e133 * mix(1f, _e130, _e131));
    }
    let _e135 = (*rgb);
    return _e135;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e104 = (*rgb_1);
        param_7 = _e104;
        let _e105 = frag_tex_coord0_1;
        param_8 = _e105;
        let _e106 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e106;
    }
    if override_type_3_3 {
        let _e107 = (*rgb_1);
        param_9 = _e107;
        let _e108 = frag_tex_coord1_1;
        param_10 = _e108;
        let _e109 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e109;
    }
    if override_type_3_4 {
        let _e110 = (*rgb_1);
        param_11 = _e110;
        let _e111 = frag_tex_coord2_1;
        param_12 = _e111;
        let _e112 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e112;
    }
    let _e113 = (*rgb_1);
    return _e113;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e100 = (*rgb_2);
    let _e103 = unnamed.worldLightParams[0u];
    boosted = (_e100 * _e103);
    let _e106 = boosted[0u];
    let _e108 = boosted[1u];
    let _e110 = boosted[2u];
    peak = max(_e106, max(_e108, _e110));
    let _e113 = peak;
    if (_e113 > 1f) {
        let _e115 = peak;
        let _e116 = boosted;
        boosted = (_e116 / vec3(_e115));
    }
    let _e119 = boosted;
    return _e119;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e101 = (*c_1);
    (*c_1) = max(_e101, vec3<f32>(0f, 0f, 0f));
    let _e103 = (*c_1);
    cutoff = (_e103 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e105 = (*c_1);
    lo = (_e105 / vec3(12.92f));
    let _e108 = (*c_1);
    hi = pow(((_e108 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e113 = hi;
    let _e114 = lo;
    let _e115 = cutoff;
    return mix(_e113, _e114, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e115));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;

    let _e103 = (*role);
    let _e105 = (*role);
    let _e110 = unnamed.packed_indices[(_e103 / 4u)][(_e105 % 4u)];
    let _e113 = (*role);
    let _e115 = (*role);
    let _e120 = unnamed.packed_indices[(_e113 / 4u)][(_e115 % 4u)];
    let _e125 = (*uv);
    let _e126 = textureSample(wired_bindless_images[(_e110 & 4095u)], wired_bindless_samplers[((_e120 >> bitcast<u32>(12i)) & 255u)], _e125);
    c_2 = _e126;
    let _e127 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e127))) == 0i) {
        let _e132 = c_2;
        param_13 = _e132.xyz;
        let _e134 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e134.x;
        c_2[1u] = _e134.y;
        c_2[2u] = _e134.z;
    }
    let _e141 = (*slot);
    if (lightmap_slot == (_e141 + 1i)) {
        let _e144 = c_2;
        param_14 = _e144.xyz;
        let _e146 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e146.x;
        c_2[1u] = _e146.y;
        c_2[2u] = _e146.z;
    }
    let _e153 = c_2;
    return _e153;
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

    let _e140 = unnamed.packed_indices[0i][3u];
    let _e146 = unnamed.packed_indices[0i][3u];
    let _e151 = fog_tex_coord_1;
    let _e152 = textureSample(wired_bindless_images[(_e140 & 4095u)], wired_bindless_samplers[((_e146 >> bitcast<u32>(12i)) & 255u)], _e151);
    fog = _e152;
    let _e153 = frag_color0In_1;
    param_15 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e157 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    let _e162 = frag_color1In_1;
    param_16 = _e162.xyz;
    let _e164 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e166 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e164.x, _e164.y, _e164.z, _e166);
    let _e171 = frag_color2In_1;
    param_17 = _e171.xyz;
    let _e173 = sRGBToLinear_u0028_vf3_u003b((&param_17));
    let _e175 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e173.x, _e173.y, _e173.z, _e175);
    param_18 = 0u;
    let _e180 = frag_tex_coord0_1;
    param_19 = _e180;
    param_20 = 0i;
    let _e181 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e181;
    param_21 = 1u;
    let _e182 = frag_tex_coord1_1;
    param_22 = _e182;
    param_23 = 1i;
    let _e183 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e183;
    param_24 = 2u;
    let _e184 = frag_tex_coord2_1;
    param_25 = _e184;
    param_26 = 2i;
    let _e185 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
    sampledColor2_ = _e185;
    let _e186 = sampledColor0_;
    let _e187 = frag_color0_;
    color0_ = (_e186 * _e187);
    if override_type_3_7 {
        let _e189 = sampledColor1_;
        let _e190 = frag_color1_;
        color1_ = (_e189 * _e190);
        let _e192 = sampledColor2_;
        let _e193 = frag_color2_;
        color2_ = (_e192 * _e193);
        let _e195 = color0_;
        let _e197 = color1_;
        let _e200 = color2_;
        let _e202 = ((_e195.xyz + _e197.xyz) + _e200.xyz);
        let _e204 = color0_[3u];
        let _e206 = color1_[3u];
        let _e209 = color2_[3u];
        base = vec4<f32>(_e202.x, _e202.y, _e202.z, ((_e204 * _e206) * _e209));
    } else {
        if override_type_3_8 {
            let _e215 = sampledColor1_;
            let _e216 = frag_color1_;
            color1_1 = (_e215 * _e216);
            let _e218 = sampledColor2_;
            let _e219 = frag_color2_;
            color2_1 = (_e218 * _e219);
            let _e222 = color0_[3u];
            let _e223 = color0_;
            color0_ = (_e223 * _e222);
            let _e226 = color1_1[3u];
            let _e227 = color1_1;
            color1_1 = (_e227 * _e226);
            let _e230 = color2_1[3u];
            let _e231 = color2_1;
            color2_1 = (_e231 * _e230);
            let _e233 = color0_;
            let _e235 = color1_1;
            let _e238 = color2_1;
            let _e240 = ((_e233.xyz + _e235.xyz) + _e238.xyz);
            let _e242 = color0_[3u];
            let _e244 = color1_1[3u];
            let _e247 = color2_1[3u];
            base = vec4<f32>(_e240.x, _e240.y, _e240.z, ((_e242 * _e244) * _e247));
        } else {
            if override_type_3_9 {
                let _e253 = sampledColor1_;
                let _e254 = frag_color1_;
                color1_2 = (_e253 * _e254);
                let _e256 = sampledColor2_;
                let _e257 = frag_color2_;
                color2_2 = (_e256 * _e257);
                let _e260 = color0_[3u];
                let _e262 = color0_;
                color0_ = (_e262 * (1f - _e260));
                let _e265 = color1_2[3u];
                let _e267 = color1_2;
                color1_2 = (_e267 * (1f - _e265));
                let _e270 = color2_2[3u];
                let _e272 = color2_2;
                color2_2 = (_e272 * (1f - _e270));
                let _e274 = color0_;
                let _e276 = color1_2;
                let _e279 = color2_2;
                let _e281 = ((_e274.xyz + _e276.xyz) + _e279.xyz);
                let _e283 = color0_[3u];
                let _e285 = color1_2[3u];
                let _e288 = color2_2[3u];
                base = vec4<f32>(_e281.x, _e281.y, _e281.z, ((_e283 * _e285) * _e288));
            } else {
                if override_type_3_10 {
                    let _e294 = sampledColor1_;
                    let _e295 = frag_color1_;
                    color1_3 = (_e294 * _e295);
                    let _e297 = sampledColor2_;
                    let _e298 = frag_color2_;
                    color2_3 = (_e297 * _e298);
                    let _e300 = color0_;
                    let _e301 = color1_3;
                    let _e303 = color1_3[3u];
                    let _e306 = color2_3;
                    let _e308 = color2_3[3u];
                    base = mix(mix(_e300, _e301, vec4(_e303)), _e306, vec4(_e308));
                } else {
                    if override_type_3_11 {
                        let _e311 = sampledColor1_;
                        let _e312 = frag_color1_;
                        color1_4 = (_e311 * _e312);
                        let _e314 = sampledColor2_;
                        let _e315 = frag_color2_;
                        color2_4 = (_e314 * _e315);
                        let _e317 = color2_4;
                        let _e318 = color1_4;
                        let _e319 = color0_;
                        let _e321 = color1_4[3u];
                        let _e325 = color2_4[3u];
                        base = mix(_e317, mix(_e318, _e319, vec4(_e321)), vec4(_e325));
                    } else {
                        if override_type_3_12 {
                            let _e328 = sampledColor1_;
                            let _e329 = frag_color1_;
                            color1_5 = (_e328 * _e329);
                            let _e331 = sampledColor2_;
                            let _e332 = frag_color2_;
                            color2_5 = (_e331 * _e332);
                            let _e334 = color2_5;
                            let _e336 = color2_5[3u];
                            let _e339 = color1_5;
                            let _e341 = color1_5[3u];
                            let _e345 = color0_;
                            base = (((_e334 + vec4(_e336)) * (_e339 + vec4(_e341))) * _e345);
                        } else {
                            let _e347 = sampledColor1_;
                            let _e348 = frag_color1_;
                            color1_6 = (_e347 * _e348);
                            let _e350 = sampledColor2_;
                            let _e351 = frag_color2_;
                            color2_6 = (_e350 * _e351);
                            let _e353 = color0_;
                            let _e355 = color1_6;
                            let _e358 = color2_6;
                            let _e360 = ((_e353.xyz * _e355.xyz) * _e358.xyz);
                            base[0u] = _e360.x;
                            base[1u] = _e360.y;
                            base[2u] = _e360.z;
                            let _e368 = color0_[3u];
                            let _e370 = color1_6[3u];
                            let _e373 = color2_6[3u];
                            base[3u] = ((_e368 * _e370) * _e373);
                        }
                    }
                }
            }
        }
    }
    let _e376 = base;
    param_27 = _e376.xyz;
    let _e378 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_27));
    base[0u] = _e378.x;
    base[1u] = _e378.y;
    base[2u] = _e378.z;
    if override_type_3_13 {
        let _e387 = unnamed.worldLightParams[1u];
        wetness = clamp(_e387, 0f, 1f);
        let _e391 = unnamed.worldLightParams[2u];
        frost = clamp(_e391, 0f, 1f);
        let _e393 = base;
        luminance = dot(_e393.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e396 = wetness;
        let _e398 = base;
        let _e400 = (_e398.xyz * mix(1f, 0.82f, _e396));
        base[0u] = _e400.x;
        base[1u] = _e400.y;
        base[2u] = _e400.z;
        let _e407 = base;
        let _e409 = luminance;
        let _e411 = luminance;
        let _e413 = luminance;
        let _e415 = frost;
        let _e418 = mix(_e407.xyz, vec3<f32>((_e409 * 0.88f), (_e411 * 0.94f), _e413), vec3((_e415 * 0.55f)));
        base[0u] = _e418.x;
        base[1u] = _e418.y;
        base[2u] = _e418.z;
    }
    let _e427 = unnamed.materialFactors[0u];
    if (_e427 >= 0f) {
        let _e430 = unnamed.materialBaseColor;
        let _e431 = base;
        base = (_e431 * _e430);
    }
    let _e433 = color0_;
    let _e436 = unnamed.emissionRadiance;
    let _e439 = base;
    let _e441 = (_e439.xyz + (_e433.xyz * _e436.xyz));
    base[0u] = _e441.x;
    base[1u] = _e441.y;
    base[2u] = _e441.z;
    let _e448 = wired_advanced_fog_enabled_u0028_();
    if _e448 {
        let _e449 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e449;
        if override_type_3_14 {
            let _e450 = fogAmount;
            let _e452 = base;
            let _e454 = (_e452.xyz * (1f - _e450));
            base[0u] = _e454.x;
            base[1u] = _e454.y;
            base[2u] = _e454.z;
        } else {
            if override_type_3_15 {
                let _e461 = fogAmount;
                let _e463 = base;
                base = (_e463 * (1f - _e461));
            } else {
                if override_type_3_16 {
                    let _e465 = fogAmount;
                    let _e468 = base[3u];
                    base[3u] = (_e468 * (1f - _e465));
                } else {
                    let _e471 = base;
                    let _e474 = unnamed.advancedFogColorDensity;
                    let _e476 = fogAmount;
                    let _e478 = mix(_e471.xyz, _e474.xyz, vec3(_e476));
                    base[0u] = _e478.x;
                    base[1u] = _e478.y;
                    base[2u] = _e478.z;
                }
            }
        }
    } else {
        if override_type_3_17 {
            let _e485 = base;
            let _e488 = fog[3u];
            let _e490 = (_e485.xyz * (1f - _e488));
            base[0u] = _e490.x;
            base[1u] = _e490.y;
            base[2u] = _e490.z;
        } else {
            if override_type_3_18 {
                let _e497 = base;
                let _e499 = fog[3u];
                base = (_e497 * (1f - _e499));
            } else {
                if override_type_3_19 {
                    let _e503 = base[3u];
                    let _e505 = fog[3u];
                    base[3u] = (_e503 * (1f - _e505));
                } else {
                    let _e509 = base;
                    let _e510 = fog;
                    let _e512 = unnamed.fogColor;
                    let _e515 = fog[3u];
                    base = mix(_e509, (_e510 * _e512), vec4(_e515));
                }
            }
        }
    }
    if override_type_3_20 {
        let _e519 = base[3u];
        if (_e519 == 0f) {
            discard;
        }
    } else {
        if override_type_3_21 {
            let _e521 = base;
            let _e523 = base;
            if (dot(_e521.xyz, _e523.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e527 = base;
    out_color = _e527;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    main_1();
    let _e19 = out_color;
    return _e19;
}
