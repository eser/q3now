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
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
override override_type_3_3: bool = (lightmap_slot == 2i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
override override_type_3_6: bool = (override_type_3_4 || override_type_3_5);
override override_type_3_7: bool = (tex_mode == 3i);
override override_type_3_8: bool = (tex_mode == 4i);
override override_type_3_9: bool = (tex_mode == 5i);
override override_type_3_10: bool = (tex_mode == 6i);
override override_type_3_11: bool = (tex_mode == 7i);
override override_type_3_12: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_13: bool = (discard_mode == 1i);
override override_type_3_14: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
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

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e98 = (*value);
    let _e99 = (*value);
    let _e101 = all((_e98 == _e99));
    phi_98_ = _e101;
    if _e101 {
        let _e102 = (*value);
        phi_98_ = all((abs(_e102) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e107 = phi_98_;
    return _e107;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e98 = (*value_1);
    let _e99 = (*value_1);
    let _e101 = all((_e98 == _e99));
    phi_83_ = _e101;
    if _e101 {
        let _e102 = (*value_1);
        phi_83_ = all((abs(_e102) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e107 = phi_83_;
    return _e107;
}

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_122_: bool;
    var phi_131_: bool;
    var phi_141_: bool;
    var phi_148_: bool;
    var phi_177_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e109 = temporalOutcome_1;
    let _e110 = (_e109 != 1u);
    phi_122_ = _e110;
    if !(_e110) {
        let _e112 = temporalCurrentClip_1;
        param = _e112;
        let _e113 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e113);
    }
    let _e116 = phi_122_;
    phi_131_ = _e116;
    if !(_e116) {
        let _e118 = temporalPreviousClip_1;
        param_1 = _e118;
        let _e119 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e119);
    }
    let _e122 = phi_131_;
    phi_141_ = _e122;
    if !(_e122) {
        let _e125 = temporalCurrentClip_1[3u];
        phi_141_ = (_e125 <= 0.000001f);
    }
    let _e128 = phi_141_;
    phi_148_ = _e128;
    if !(_e128) {
        let _e131 = temporalPreviousClip_1[3u];
        phi_148_ = (_e131 <= 0.000001f);
    }
    let _e134 = phi_148_;
    if _e134 {
        return;
    }
    let _e135 = temporalCurrentClip_1;
    let _e138 = temporalCurrentClip_1[3u];
    currentNdc = (_e135.xy / vec2(_e138));
    let _e141 = temporalPreviousClip_1;
    let _e144 = temporalPreviousClip_1[3u];
    previousNdc = (_e141.xy / vec2(_e144));
    let _e147 = currentNdc;
    param_2 = _e147;
    let _e148 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e149 = !(_e148);
    phi_177_ = _e149;
    if !(_e149) {
        let _e151 = previousNdc;
        param_3 = _e151;
        let _e152 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e152);
    }
    let _e155 = phi_177_;
    if _e155 {
        return;
    }
    let _e156 = currentNdc;
    currentUv = ((_e156 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e159 = previousNdc;
    previousUv = ((_e159 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e162 = currentUv;
    let _e163 = previousUv;
    velocity = (_e162 - _e163);
    let _e165 = velocity;
    param_4 = _e165;
    let _e166 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e166) {
        return;
    }
    let _e168 = velocity;
    out_temporal_velocity = _e168;
    let _e169 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e169, 0f, 1f), 0f, 0f, 1f);
    return;
}

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
    var phi_473_: bool;
    var phi_480_: bool;
    var phi_487_: bool;
    var phi_494_: bool;

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
    phi_473_ = _e134;
    if !(_e134) {
        let _e137 = sc[0u];
        phi_473_ = (_e137 > 1f);
    }
    let _e140 = phi_473_;
    phi_480_ = _e140;
    if !(_e140) {
        let _e143 = sc[1u];
        phi_480_ = (_e143 < 0f);
    }
    let _e146 = phi_480_;
    phi_487_ = _e146;
    if !(_e146) {
        let _e149 = sc[1u];
        phi_487_ = (_e149 > 1f);
    }
    let _e152 = phi_487_;
    phi_494_ = _e152;
    if !(_e152) {
        let _e155 = sc[2u];
        phi_494_ = (_e155 > 1f);
    }
    let _e158 = phi_494_;
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
    var param_5: i32;
    var param_6: vec3<f32>;
    var s1_: f32;
    var param_7: i32;
    var param_8: vec3<f32>;

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
    param_5 = _e156;
    let _e157 = (*worldPos_1);
    param_6 = _e157;
    let _e158 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e158;
    let _e159 = cascade;
    param_7 = min((_e159 + 1i), 3i);
    let _e162 = (*worldPos_1);
    param_8 = _e162;
    let _e163 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
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
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e108 = unnamed.packed_indices[1i][3u];
    let _e114 = unnamed.packed_indices[1i][3u];
    let _e119 = (*lm_uv);
    let _e120 = textureSample(wired_bindless_images[(_e108 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    sun_mask = _e120.x;
    let _e122 = sun_mask;
    if (_e122 > 0.001f) {
        let _e124 = shadowData_1;
        param_9 = _e124.xyz;
        let _e127 = shadowData_1[3u];
        param_10 = _e127;
        let _e128 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e129 = param_11;
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
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e102 = (*rgb_1);
        param_12 = _e102;
        let _e103 = frag_tex_coord0_1;
        param_13 = _e103;
        let _e104 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e104;
    }
    if override_type_3_3 {
        let _e105 = (*rgb_1);
        param_14 = _e105;
        let _e106 = frag_tex_coord1_1;
        param_15 = _e106;
        let _e107 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e107;
    }
    let _e108 = (*rgb_1);
    return _e108;
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
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

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
        param_16 = _e132.xyz;
        let _e134 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e134.x;
        c_2[1u] = _e134.y;
        c_2[2u] = _e134.z;
    }
    let _e141 = (*slot);
    if (lightmap_slot == (_e141 + 1i)) {
        let _e144 = c_2;
        param_17 = _e144.xyz;
        let _e146 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e146.x;
        c_2[1u] = _e146.y;
        c_2[2u] = _e146.z;
    }
    let _e153 = c_2;
    return _e153;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_18: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_19: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var sampledColor1_: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var param_26: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_27: f32;
    var param_28: vec4<f32>;

    let _e125 = frag_color0In_1;
    param_18 = _e125.xyz;
    let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_18));
    let _e129 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e127.x, _e127.y, _e127.z, _e129);
    let _e134 = frag_color1In_1;
    param_19 = _e134.xyz;
    let _e136 = sRGBToLinear_u0028_vf3_u003b((&param_19));
    let _e138 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e136.x, _e136.y, _e136.z, _e138);
    param_20 = 0u;
    let _e143 = frag_tex_coord0_1;
    param_21 = _e143;
    param_22 = 0i;
    let _e144 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
    sampledColor0_ = _e144;
    param_23 = 1u;
    let _e145 = frag_tex_coord1_1;
    param_24 = _e145;
    param_25 = 1i;
    let _e146 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
    sampledColor1_ = _e146;
    let _e147 = sampledColor0_;
    let _e148 = frag_color0_;
    color0_ = (_e147 * _e148);
    if override_type_3_6 {
        let _e150 = sampledColor1_;
        let _e151 = frag_color1_;
        color1_ = (_e150 * _e151);
        let _e153 = color0_;
        let _e155 = color1_;
        let _e157 = (_e153.xyz + _e155.xyz);
        let _e159 = color0_[3u];
        let _e161 = color1_[3u];
        base = vec4<f32>(_e157.x, _e157.y, _e157.z, (_e159 * _e161));
    } else {
        if override_type_3_7 {
            let _e167 = sampledColor1_;
            let _e168 = frag_color1_;
            color1_1 = (_e167 * _e168);
            let _e171 = color0_[3u];
            let _e172 = color0_;
            color0_ = (_e172 * _e171);
            let _e175 = color1_1[3u];
            let _e176 = color1_1;
            color1_1 = (_e176 * _e175);
            let _e178 = color0_;
            let _e180 = color1_1;
            let _e182 = (_e178.xyz + _e180.xyz);
            let _e184 = color0_[3u];
            let _e186 = color1_1[3u];
            base = vec4<f32>(_e182.x, _e182.y, _e182.z, (_e184 * _e186));
        } else {
            if override_type_3_8 {
                let _e192 = sampledColor1_;
                let _e193 = frag_color1_;
                color1_2 = (_e192 * _e193);
                let _e196 = color0_[3u];
                let _e198 = color0_;
                color0_ = (_e198 * (1f - _e196));
                let _e201 = color1_2[3u];
                let _e203 = color1_2;
                color1_2 = (_e203 * (1f - _e201));
                let _e205 = color0_;
                let _e207 = color1_2;
                let _e209 = (_e205.xyz + _e207.xyz);
                let _e211 = color0_[3u];
                let _e213 = color1_2[3u];
                base = vec4<f32>(_e209.x, _e209.y, _e209.z, (_e211 * _e213));
            } else {
                if override_type_3_9 {
                    let _e219 = sampledColor1_;
                    let _e220 = frag_color1_;
                    color1_3 = (_e219 * _e220);
                    let _e222 = color0_;
                    let _e223 = color1_3;
                    let _e225 = color1_3[3u];
                    base = mix(_e222, _e223, vec4(_e225));
                } else {
                    if override_type_3_10 {
                        let _e228 = sampledColor1_;
                        let _e229 = frag_color1_;
                        color1_4 = (_e228 * _e229);
                        let _e231 = color1_4;
                        let _e232 = color0_;
                        let _e234 = color1_4[3u];
                        base = mix(_e231, _e232, vec4(_e234));
                    } else {
                        if override_type_3_11 {
                            let _e237 = sampledColor1_;
                            let _e238 = frag_color1_;
                            color1_5 = (_e237 * _e238);
                            let _e240 = color1_5;
                            let _e242 = color1_5[3u];
                            let _e245 = color0_;
                            base = ((_e240 + vec4(_e242)) * _e245);
                        } else {
                            let _e247 = sampledColor1_;
                            let _e248 = frag_color1_;
                            color1_6 = (_e247 * _e248);
                            let _e250 = color0_;
                            let _e252 = color1_6;
                            let _e254 = (_e250.xyz * _e252.xyz);
                            base[0u] = _e254.x;
                            base[1u] = _e254.y;
                            base[2u] = _e254.z;
                            let _e262 = color0_[3u];
                            let _e264 = color1_6[3u];
                            base[3u] = (_e262 * _e264);
                        }
                    }
                }
            }
        }
    }
    let _e267 = base;
    param_26 = _e267.xyz;
    let _e269 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_26));
    base[0u] = _e269.x;
    base[1u] = _e269.y;
    base[2u] = _e269.z;
    if override_type_3_12 {
        let _e278 = unnamed.worldLightParams[1u];
        wetness = clamp(_e278, 0f, 1f);
        let _e282 = unnamed.worldLightParams[2u];
        frost = clamp(_e282, 0f, 1f);
        let _e284 = base;
        luminance = dot(_e284.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e287 = wetness;
        let _e289 = base;
        let _e291 = (_e289.xyz * mix(1f, 0.82f, _e287));
        base[0u] = _e291.x;
        base[1u] = _e291.y;
        base[2u] = _e291.z;
        let _e298 = base;
        let _e300 = luminance;
        let _e302 = luminance;
        let _e304 = luminance;
        let _e306 = frost;
        let _e309 = mix(_e298.xyz, vec3<f32>((_e300 * 0.88f), (_e302 * 0.94f), _e304), vec3((_e306 * 0.55f)));
        base[0u] = _e309.x;
        base[1u] = _e309.y;
        base[2u] = _e309.z;
    }
    let _e318 = unnamed.materialFactors[0u];
    if (_e318 >= 0f) {
        let _e321 = unnamed.materialBaseColor;
        let _e322 = base;
        base = (_e322 * _e321);
    }
    let _e324 = color0_;
    let _e327 = unnamed.emissionRadiance;
    let _e330 = base;
    let _e332 = (_e330.xyz + (_e324.xyz * _e327.xyz));
    base[0u] = _e332.x;
    base[1u] = _e332.y;
    base[2u] = _e332.z;
    let _e339 = wired_advanced_fog_enabled_u0028_();
    if _e339 {
        let _e340 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e340;
        let _e341 = base;
        let _e344 = unnamed.advancedFogColorDensity;
        let _e346 = fogAmount;
        let _e348 = mix(_e341.xyz, _e344.xyz, vec3(_e346));
        base[0u] = _e348.x;
        base[1u] = _e348.y;
        base[2u] = _e348.z;
    }
    if override_type_3_13 {
        let _e356 = base[3u];
        if (_e356 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e358 = base;
            let _e360 = base;
            if (dot(_e358.xyz, _e360.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e364 = base;
    out_color = _e364;
    param_27 = 1f;
    let _e365 = base;
    param_28 = _e365;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_27), (&param_28));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
