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
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
override override_type_3_3: bool = (lightmap_slot == 2i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
override override_type_3_6: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
override override_type_3_10: bool = (acff == 1i);
override override_type_3_11: bool = (acff == 2i);
override override_type_3_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_13: bool = (discard_mode == 1i);
override override_type_3_14: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e96 = (*value);
    let _e97 = (*value);
    let _e99 = all((_e96 == _e97));
    phi_97_ = _e99;
    if _e99 {
        let _e100 = (*value);
        phi_97_ = all((abs(_e100) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e105 = phi_97_;
    return _e105;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e96 = (*value_1);
    let _e97 = (*value_1);
    let _e99 = all((_e96 == _e97));
    phi_82_ = _e99;
    if _e99 {
        let _e100 = (*value_1);
        phi_82_ = all((abs(_e100) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e105 = phi_82_;
    return _e105;
}

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
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
    var phi_120_: bool;
    var phi_129_: bool;
    var phi_139_: bool;
    var phi_146_: bool;
    var phi_175_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e106 = temporalOutcome_1;
    let _e107 = (_e106 != 1u);
    phi_120_ = _e107;
    if !(_e107) {
        let _e109 = temporalCurrentClip_1;
        param = _e109;
        let _e110 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e110);
    }
    let _e113 = phi_120_;
    phi_129_ = _e113;
    if !(_e113) {
        let _e115 = temporalPreviousClip_1;
        param_1 = _e115;
        let _e116 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e116);
    }
    let _e119 = phi_129_;
    phi_139_ = _e119;
    if !(_e119) {
        let _e122 = temporalCurrentClip_1[3u];
        phi_139_ = (_e122 <= 0.000001f);
    }
    let _e125 = phi_139_;
    phi_146_ = _e125;
    if !(_e125) {
        let _e128 = temporalPreviousClip_1[3u];
        phi_146_ = (_e128 <= 0.000001f);
    }
    let _e131 = phi_146_;
    if _e131 {
        return;
    }
    let _e132 = temporalCurrentClip_1;
    let _e135 = temporalCurrentClip_1[3u];
    currentNdc = (_e132.xy / vec2(_e135));
    let _e138 = temporalPreviousClip_1;
    let _e141 = temporalPreviousClip_1[3u];
    previousNdc = (_e138.xy / vec2(_e141));
    let _e144 = currentNdc;
    param_2 = _e144;
    let _e145 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e146 = !(_e145);
    phi_175_ = _e146;
    if !(_e146) {
        let _e148 = previousNdc;
        param_3 = _e148;
        let _e149 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e149);
    }
    let _e152 = phi_175_;
    if _e152 {
        return;
    }
    let _e153 = currentNdc;
    currentUv = ((_e153 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e156 = previousNdc;
    previousUv = ((_e156 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e159 = currentUv;
    let _e160 = previousUv;
    velocity = (_e159 - _e160);
    let _e162 = velocity;
    param_4 = _e162;
    let _e163 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e163) {
        return;
    }
    let _e165 = velocity;
    out_temporal_velocity = _e165;
    let _e166 = (*coverageConfidence);
    out_temporal_validity = clamp(_e166, 0f, 1f);
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
    var phi_470_: bool;
    var phi_477_: bool;
    var phi_484_: bool;
    var phi_491_: bool;

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
    phi_470_ = _e132;
    if !(_e132) {
        let _e135 = sc[0u];
        phi_470_ = (_e135 > 1f);
    }
    let _e138 = phi_470_;
    phi_477_ = _e138;
    if !(_e138) {
        let _e141 = sc[1u];
        phi_477_ = (_e141 < 0f);
    }
    let _e144 = phi_477_;
    phi_484_ = _e144;
    if !(_e144) {
        let _e147 = sc[1u];
        phi_484_ = (_e147 > 1f);
    }
    let _e150 = phi_484_;
    phi_491_ = _e150;
    if !(_e150) {
        let _e153 = sc[2u];
        phi_491_ = (_e153 > 1f);
    }
    let _e156 = phi_491_;
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
    if override_type_3_ {
        let _e168 = currentDepth;
        let _e169 = sc;
        let _e170 = _e169.xy;
        let _e171 = layer;
        let _e174 = vec3<f32>(_e170.x, _e170.y, _e171);
        let _e180 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e174.x, _e174.y), i32(_e174.z));
        shadow = step(_e168, _e180.x);
    } else {
        if override_type_3_1 {
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
    var param_5: i32;
    var param_6: vec3<f32>;
    var s1_: f32;
    var param_7: i32;
    var param_8: vec3<f32>;

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
    param_5 = _e154;
    let _e155 = (*worldPos_1);
    param_6 = _e155;
    let _e156 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e156;
    let _e157 = cascade;
    param_7 = min((_e157 + 1i), 3i);
    let _e160 = (*worldPos_1);
    param_8 = _e160;
    let _e161 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
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
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e106 = unnamed.packed_indices[1i][3u];
    let _e112 = unnamed.packed_indices[1i][3u];
    let _e117 = (*lm_uv);
    let _e118 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    sun_mask = _e118.x;
    let _e120 = sun_mask;
    if (_e120 > 0.001f) {
        let _e122 = shadowData_1;
        param_9 = _e122.xyz;
        let _e125 = shadowData_1[3u];
        param_10 = _e125;
        let _e126 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e127 = param_11;
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
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e100 = (*rgb_1);
        param_12 = _e100;
        let _e101 = frag_tex_coord0_1;
        param_13 = _e101;
        let _e102 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e102;
    }
    if override_type_3_3 {
        let _e103 = (*rgb_1);
        param_14 = _e103;
        let _e104 = frag_tex_coord1_1;
        param_15 = _e104;
        let _e105 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e105;
    }
    let _e106 = (*rgb_1);
    return _e106;
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
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

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
        param_16 = _e130.xyz;
        let _e132 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e132.x;
        c_2[1u] = _e132.y;
        c_2[2u] = _e132.z;
    }
    let _e139 = (*slot);
    if (lightmap_slot == (_e139 + 1i)) {
        let _e142 = c_2;
        param_17 = _e142.xyz;
        let _e144 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e144.x;
        c_2[1u] = _e144.y;
        c_2[2u] = _e144.z;
    }
    let _e151 = c_2;
    return _e151;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_18: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var sampledColor1_: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var param_25: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_26: f32;

    let _e120 = unnamed.packed_indices[0i][3u];
    let _e126 = unnamed.packed_indices[0i][3u];
    let _e131 = fog_tex_coord_1;
    let _e132 = textureSample(wired_bindless_images[(_e120 & 4095u)], wired_bindless_samplers[((_e126 >> bitcast<u32>(12i)) & 255u)], _e131);
    fog = _e132;
    let _e133 = frag_color0In_1;
    param_18 = _e133.xyz;
    let _e135 = sRGBToLinear_u0028_vf3_u003b((&param_18));
    let _e137 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e135.x, _e135.y, _e135.z, _e137);
    param_19 = 0u;
    let _e142 = frag_tex_coord0_1;
    param_20 = _e142;
    param_21 = 0i;
    let _e143 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor0_ = _e143;
    param_22 = 1u;
    let _e144 = frag_tex_coord1_1;
    param_23 = _e144;
    param_24 = 1i;
    let _e145 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor1_ = _e145;
    let _e146 = sampledColor0_;
    let _e147 = frag_color0_;
    color0_ = (_e146 * _e147);
    if override_type_3_4 {
        let _e149 = sampledColor1_;
        color1_ = _e149;
        let _e150 = color0_;
        let _e152 = color1_;
        let _e154 = (_e150.xyz + _e152.xyz);
        let _e156 = color0_[3u];
        let _e158 = color1_[3u];
        base = vec4<f32>(_e154.x, _e154.y, _e154.z, (_e156 * _e158));
    } else {
        if override_type_3_5 {
            let _e164 = sampledColor1_;
            let _e165 = frag_color0_;
            color1_1 = (_e164 * _e165);
            let _e167 = color0_;
            let _e169 = color1_1;
            let _e171 = (_e167.xyz + _e169.xyz);
            let _e173 = color0_[3u];
            let _e175 = color1_1[3u];
            base = vec4<f32>(_e171.x, _e171.y, _e171.z, (_e173 * _e175));
        } else {
            let _e181 = sampledColor1_;
            color1_2 = _e181;
            let _e182 = color0_;
            let _e184 = color1_2;
            let _e186 = (_e182.xyz * _e184.xyz);
            base[0u] = _e186.x;
            base[1u] = _e186.y;
            base[2u] = _e186.z;
            let _e194 = color0_[3u];
            let _e196 = color1_2[3u];
            base[3u] = (_e194 * _e196);
        }
    }
    let _e199 = base;
    param_25 = _e199.xyz;
    let _e201 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e201.x;
    base[1u] = _e201.y;
    base[2u] = _e201.z;
    if override_type_3_6 {
        let _e210 = unnamed.worldLightParams[1u];
        wetness = clamp(_e210, 0f, 1f);
        let _e214 = unnamed.worldLightParams[2u];
        frost = clamp(_e214, 0f, 1f);
        let _e216 = base;
        luminance = dot(_e216.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e219 = wetness;
        let _e221 = base;
        let _e223 = (_e221.xyz * mix(1f, 0.82f, _e219));
        base[0u] = _e223.x;
        base[1u] = _e223.y;
        base[2u] = _e223.z;
        let _e230 = base;
        let _e232 = luminance;
        let _e234 = luminance;
        let _e236 = luminance;
        let _e238 = frost;
        let _e241 = mix(_e230.xyz, vec3<f32>((_e232 * 0.88f), (_e234 * 0.94f), _e236), vec3((_e238 * 0.55f)));
        base[0u] = _e241.x;
        base[1u] = _e241.y;
        base[2u] = _e241.z;
    }
    let _e250 = unnamed.materialFactors[0u];
    if (_e250 >= 0f) {
        let _e253 = unnamed.materialBaseColor;
        let _e254 = base;
        base = (_e254 * _e253);
    }
    let _e256 = color0_;
    let _e259 = unnamed.emissionRadiance;
    let _e262 = base;
    let _e264 = (_e262.xyz + (_e256.xyz * _e259.xyz));
    base[0u] = _e264.x;
    base[1u] = _e264.y;
    base[2u] = _e264.z;
    let _e271 = wired_advanced_fog_enabled_u0028_();
    if _e271 {
        let _e272 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e272;
        if override_type_3_7 {
            let _e273 = fogAmount;
            let _e275 = base;
            let _e277 = (_e275.xyz * (1f - _e273));
            base[0u] = _e277.x;
            base[1u] = _e277.y;
            base[2u] = _e277.z;
        } else {
            if override_type_3_8 {
                let _e284 = fogAmount;
                let _e286 = base;
                base = (_e286 * (1f - _e284));
            } else {
                if override_type_3_9 {
                    let _e288 = fogAmount;
                    let _e291 = base[3u];
                    base[3u] = (_e291 * (1f - _e288));
                } else {
                    let _e294 = base;
                    let _e297 = unnamed.advancedFogColorDensity;
                    let _e299 = fogAmount;
                    let _e301 = mix(_e294.xyz, _e297.xyz, vec3(_e299));
                    base[0u] = _e301.x;
                    base[1u] = _e301.y;
                    base[2u] = _e301.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e308 = base;
            let _e311 = fog[3u];
            let _e313 = (_e308.xyz * (1f - _e311));
            base[0u] = _e313.x;
            base[1u] = _e313.y;
            base[2u] = _e313.z;
        } else {
            if override_type_3_11 {
                let _e320 = base;
                let _e322 = fog[3u];
                base = (_e320 * (1f - _e322));
            } else {
                if override_type_3_12 {
                    let _e326 = base[3u];
                    let _e328 = fog[3u];
                    base[3u] = (_e326 * (1f - _e328));
                } else {
                    let _e332 = base;
                    let _e333 = fog;
                    let _e335 = unnamed.fogColor;
                    let _e338 = fog[3u];
                    base = mix(_e332, (_e333 * _e335), vec4(_e338));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e342 = base[3u];
        if (_e342 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e344 = base;
            let _e346 = base;
            if (dot(_e344.xyz, _e346.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e350 = base;
    out_color = _e350;
    param_26 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_26));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
