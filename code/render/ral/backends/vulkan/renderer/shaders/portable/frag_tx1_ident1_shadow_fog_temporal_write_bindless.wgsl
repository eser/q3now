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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e96 = (*value);
    let _e97 = (*value);
    let _e99 = all((_e96 == _e97));
    phi_98_ = _e99;
    if _e99 {
        let _e100 = (*value);
        phi_98_ = all((abs(_e100) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e105 = phi_98_;
    return _e105;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e96 = (*value_1);
    let _e97 = (*value_1);
    let _e99 = all((_e96 == _e97));
    phi_83_ = _e99;
    if _e99 {
        let _e100 = (*value_1);
        phi_83_ = all((abs(_e100) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e105 = phi_83_;
    return _e105;
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
    let _e107 = temporalOutcome_1;
    let _e108 = (_e107 != 1u);
    phi_122_ = _e108;
    if !(_e108) {
        let _e110 = temporalCurrentClip_1;
        param = _e110;
        let _e111 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e111);
    }
    let _e114 = phi_122_;
    phi_131_ = _e114;
    if !(_e114) {
        let _e116 = temporalPreviousClip_1;
        param_1 = _e116;
        let _e117 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e117);
    }
    let _e120 = phi_131_;
    phi_141_ = _e120;
    if !(_e120) {
        let _e123 = temporalCurrentClip_1[3u];
        phi_141_ = (_e123 <= 0.000001f);
    }
    let _e126 = phi_141_;
    phi_148_ = _e126;
    if !(_e126) {
        let _e129 = temporalPreviousClip_1[3u];
        phi_148_ = (_e129 <= 0.000001f);
    }
    let _e132 = phi_148_;
    if _e132 {
        return;
    }
    let _e133 = temporalCurrentClip_1;
    let _e136 = temporalCurrentClip_1[3u];
    currentNdc = (_e133.xy / vec2(_e136));
    let _e139 = temporalPreviousClip_1;
    let _e142 = temporalPreviousClip_1[3u];
    previousNdc = (_e139.xy / vec2(_e142));
    let _e145 = currentNdc;
    param_2 = _e145;
    let _e146 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e147 = !(_e146);
    phi_177_ = _e147;
    if !(_e147) {
        let _e149 = previousNdc;
        param_3 = _e149;
        let _e150 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e150);
    }
    let _e153 = phi_177_;
    if _e153 {
        return;
    }
    let _e154 = currentNdc;
    currentUv = ((_e154 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e157 = previousNdc;
    previousUv = ((_e157 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e160 = currentUv;
    let _e161 = previousUv;
    velocity = (_e160 - _e161);
    let _e163 = velocity;
    param_4 = _e163;
    let _e164 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e164) {
        return;
    }
    let _e166 = velocity;
    out_temporal_velocity = _e166;
    let _e167 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e167, 0f, 1f), 0f, 0f, 1f);
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
    var phi_473_: bool;
    var phi_480_: bool;
    var phi_487_: bool;
    var phi_494_: bool;

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
    phi_473_ = _e132;
    if !(_e132) {
        let _e135 = sc[0u];
        phi_473_ = (_e135 > 1f);
    }
    let _e138 = phi_473_;
    phi_480_ = _e138;
    if !(_e138) {
        let _e141 = sc[1u];
        phi_480_ = (_e141 < 0f);
    }
    let _e144 = phi_480_;
    phi_487_ = _e144;
    if !(_e144) {
        let _e147 = sc[1u];
        phi_487_ = (_e147 > 1f);
    }
    let _e150 = phi_487_;
    phi_494_ = _e150;
    if !(_e150) {
        let _e153 = sc[2u];
        phi_494_ = (_e153 > 1f);
    }
    let _e156 = phi_494_;
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
    var sampledColor0_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var sampledColor1_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var param_24: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_25: f32;
    var param_26: vec4<f32>;

    let _e119 = unnamed.packed_indices[0i][3u];
    let _e125 = unnamed.packed_indices[0i][3u];
    let _e130 = fog_tex_coord_1;
    let _e131 = textureSample(wired_bindless_images[(_e119 & 4095u)], wired_bindless_samplers[((_e125 >> bitcast<u32>(12i)) & 255u)], _e130);
    fog = _e131;
    param_18 = 0u;
    let _e132 = frag_tex_coord0_1;
    param_19 = _e132;
    param_20 = 0i;
    let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
    sampledColor0_ = _e133;
    param_21 = 1u;
    let _e134 = frag_tex_coord1_1;
    param_22 = _e134;
    param_23 = 1i;
    let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor1_ = _e135;
    let _e136 = sampledColor0_;
    color0_ = _e136;
    if override_type_3_4 {
        let _e137 = sampledColor1_;
        color1_ = _e137;
        let _e138 = color0_;
        let _e140 = color1_;
        let _e142 = (_e138.xyz + _e140.xyz);
        let _e144 = color0_[3u];
        let _e146 = color1_[3u];
        base = vec4<f32>(_e142.x, _e142.y, _e142.z, (_e144 * _e146));
    } else {
        if override_type_3_5 {
            let _e152 = sampledColor1_;
            color1_1 = _e152;
            let _e153 = color0_;
            let _e155 = color1_1;
            let _e157 = (_e153.xyz + _e155.xyz);
            let _e159 = color0_[3u];
            let _e161 = color1_1[3u];
            base = vec4<f32>(_e157.x, _e157.y, _e157.z, (_e159 * _e161));
        } else {
            let _e167 = sampledColor1_;
            color1_2 = _e167;
            let _e168 = color0_;
            let _e170 = color1_2;
            let _e172 = (_e168.xyz * _e170.xyz);
            base[0u] = _e172.x;
            base[1u] = _e172.y;
            base[2u] = _e172.z;
            let _e180 = color0_[3u];
            let _e182 = color1_2[3u];
            base[3u] = (_e180 * _e182);
        }
    }
    let _e185 = base;
    param_24 = _e185.xyz;
    let _e187 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_24));
    base[0u] = _e187.x;
    base[1u] = _e187.y;
    base[2u] = _e187.z;
    if override_type_3_6 {
        let _e196 = unnamed.worldLightParams[1u];
        wetness = clamp(_e196, 0f, 1f);
        let _e200 = unnamed.worldLightParams[2u];
        frost = clamp(_e200, 0f, 1f);
        let _e202 = base;
        luminance = dot(_e202.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e205 = wetness;
        let _e207 = base;
        let _e209 = (_e207.xyz * mix(1f, 0.82f, _e205));
        base[0u] = _e209.x;
        base[1u] = _e209.y;
        base[2u] = _e209.z;
        let _e216 = base;
        let _e218 = luminance;
        let _e220 = luminance;
        let _e222 = luminance;
        let _e224 = frost;
        let _e227 = mix(_e216.xyz, vec3<f32>((_e218 * 0.88f), (_e220 * 0.94f), _e222), vec3((_e224 * 0.55f)));
        base[0u] = _e227.x;
        base[1u] = _e227.y;
        base[2u] = _e227.z;
    }
    let _e236 = unnamed.materialFactors[0u];
    if (_e236 >= 0f) {
        let _e239 = unnamed.materialBaseColor;
        let _e240 = base;
        base = (_e240 * _e239);
    }
    let _e242 = color0_;
    let _e245 = unnamed.emissionRadiance;
    let _e248 = base;
    let _e250 = (_e248.xyz + (_e242.xyz * _e245.xyz));
    base[0u] = _e250.x;
    base[1u] = _e250.y;
    base[2u] = _e250.z;
    let _e257 = wired_advanced_fog_enabled_u0028_();
    if _e257 {
        let _e258 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e258;
        if override_type_3_7 {
            let _e259 = fogAmount;
            let _e261 = base;
            let _e263 = (_e261.xyz * (1f - _e259));
            base[0u] = _e263.x;
            base[1u] = _e263.y;
            base[2u] = _e263.z;
        } else {
            if override_type_3_8 {
                let _e270 = fogAmount;
                let _e272 = base;
                base = (_e272 * (1f - _e270));
            } else {
                if override_type_3_9 {
                    let _e274 = fogAmount;
                    let _e277 = base[3u];
                    base[3u] = (_e277 * (1f - _e274));
                } else {
                    let _e280 = base;
                    let _e283 = unnamed.advancedFogColorDensity;
                    let _e285 = fogAmount;
                    let _e287 = mix(_e280.xyz, _e283.xyz, vec3(_e285));
                    base[0u] = _e287.x;
                    base[1u] = _e287.y;
                    base[2u] = _e287.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e294 = base;
            let _e297 = fog[3u];
            let _e299 = (_e294.xyz * (1f - _e297));
            base[0u] = _e299.x;
            base[1u] = _e299.y;
            base[2u] = _e299.z;
        } else {
            if override_type_3_11 {
                let _e306 = base;
                let _e308 = fog[3u];
                base = (_e306 * (1f - _e308));
            } else {
                if override_type_3_12 {
                    let _e312 = base[3u];
                    let _e314 = fog[3u];
                    base[3u] = (_e312 * (1f - _e314));
                } else {
                    let _e318 = base;
                    let _e319 = fog;
                    let _e321 = unnamed.fogColor;
                    let _e324 = fog[3u];
                    base = mix(_e318, (_e319 * _e321), vec4(_e324));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e328 = base[3u];
        if (_e328 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e330 = base;
            let _e332 = base;
            if (dot(_e330.xyz, _e332.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e336 = base;
    out_color = _e336;
    param_25 = 1f;
    let _e337 = base;
    param_26 = _e337;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_25), (&param_26));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
