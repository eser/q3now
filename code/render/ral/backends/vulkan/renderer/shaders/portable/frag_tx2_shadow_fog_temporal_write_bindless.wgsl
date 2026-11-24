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
override override_type_3_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_5: bool = (tex_mode == 1i);
override override_type_3_6: bool = (tex_mode == 2i);
override override_type_3_7: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_8: bool = (acff == 1i);
override override_type_3_9: bool = (acff == 2i);
override override_type_3_10: bool = (acff == 3i);
override override_type_3_11: bool = (acff == 1i);
override override_type_3_12: bool = (acff == 2i);
override override_type_3_13: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_14: bool = (discard_mode == 1i);
override override_type_3_15: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e99 = (*value);
    let _e100 = (*value);
    let _e102 = all((_e99 == _e100));
    phi_98_ = _e102;
    if _e102 {
        let _e103 = (*value);
        phi_98_ = all((abs(_e103) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e108 = phi_98_;
    return _e108;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e99 = (*value_1);
    let _e100 = (*value_1);
    let _e102 = all((_e99 == _e100));
    phi_83_ = _e102;
    if _e102 {
        let _e103 = (*value_1);
        phi_83_ = all((abs(_e103) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e108 = phi_83_;
    return _e108;
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
    let _e110 = temporalOutcome_1;
    let _e111 = (_e110 != 1u);
    phi_122_ = _e111;
    if !(_e111) {
        let _e113 = temporalCurrentClip_1;
        param = _e113;
        let _e114 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e114);
    }
    let _e117 = phi_122_;
    phi_131_ = _e117;
    if !(_e117) {
        let _e119 = temporalPreviousClip_1;
        param_1 = _e119;
        let _e120 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e120);
    }
    let _e123 = phi_131_;
    phi_141_ = _e123;
    if !(_e123) {
        let _e126 = temporalCurrentClip_1[3u];
        phi_141_ = (_e126 <= 0.000001f);
    }
    let _e129 = phi_141_;
    phi_148_ = _e129;
    if !(_e129) {
        let _e132 = temporalPreviousClip_1[3u];
        phi_148_ = (_e132 <= 0.000001f);
    }
    let _e135 = phi_148_;
    if _e135 {
        return;
    }
    let _e136 = temporalCurrentClip_1;
    let _e139 = temporalCurrentClip_1[3u];
    currentNdc = (_e136.xy / vec2(_e139));
    let _e142 = temporalPreviousClip_1;
    let _e145 = temporalPreviousClip_1[3u];
    previousNdc = (_e142.xy / vec2(_e145));
    let _e148 = currentNdc;
    param_2 = _e148;
    let _e149 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e150 = !(_e149);
    phi_177_ = _e150;
    if !(_e150) {
        let _e152 = previousNdc;
        param_3 = _e152;
        let _e153 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e153);
    }
    let _e156 = phi_177_;
    if _e156 {
        return;
    }
    let _e157 = currentNdc;
    currentUv = ((_e157 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e160 = previousNdc;
    previousUv = ((_e160 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e163 = currentUv;
    let _e164 = previousUv;
    velocity = (_e163 - _e164);
    let _e166 = velocity;
    param_4 = _e166;
    let _e167 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e167) {
        return;
    }
    let _e169 = velocity;
    out_temporal_velocity = _e169;
    let _e170 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e170, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e101 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e101 + 0.5f));
    let _e106 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e108 = fogType;
    let _e111 = fogType;
    return (((_e106 > 0.5f) && (_e108 >= 1i)) && (_e111 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e101 = wired_advanced_fog_enabled_u0028_();
    if !(_e101) {
        return 0f;
    }
    let _e104 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e104, 0.000001f));
    let _e109 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e109 + 0.5f));
    let _e112 = fogType_1;
    if (_e112 == 1i) {
        let _e116 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e116 <= 0f) {
            return 0f;
        }
        let _e118 = viewDepth;
        let _e121 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e118 / _e121), 0f, 1f);
    }
    let _e126 = unnamed.advancedFogColorDensity[3u];
    let _e128 = viewDepth;
    opticalDepth = (max(_e126, 0f) * _e128);
    let _e130 = fogType_1;
    if (_e130 == 2i) {
        let _e132 = opticalDepth;
        return clamp((1f - exp(-(_e132))), 0f, 1f);
    }
    let _e137 = opticalDepth;
    let _e138 = opticalDepth;
    return clamp((1f - exp(-((_e137 * _e138)))), 0f, 1f);
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

    let _e108 = (*c);
    let _e111 = unnamed.cascadeMVP[_e108];
    let _e112 = (*worldPos);
    sc4_ = (_e111 * vec4<f32>(_e112.x, _e112.y, _e112.z, 1f));
    let _e118 = sc4_;
    let _e121 = sc4_[3u];
    sc = (_e118.xyz / vec3(_e121));
    let _e124 = sc;
    let _e128 = ((_e124.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e128.x;
    sc[1u] = _e128.y;
    let _e134 = sc[0u];
    let _e135 = (_e134 < 0f);
    phi_473_ = _e135;
    if !(_e135) {
        let _e138 = sc[0u];
        phi_473_ = (_e138 > 1f);
    }
    let _e141 = phi_473_;
    phi_480_ = _e141;
    if !(_e141) {
        let _e144 = sc[1u];
        phi_480_ = (_e144 < 0f);
    }
    let _e147 = phi_480_;
    phi_487_ = _e147;
    if !(_e147) {
        let _e150 = sc[1u];
        phi_487_ = (_e150 > 1f);
    }
    let _e153 = phi_487_;
    phi_494_ = _e153;
    if !(_e153) {
        let _e156 = sc[2u];
        phi_494_ = (_e156 > 1f);
    }
    let _e159 = phi_494_;
    if _e159 {
        return 1f;
    }
    let _e160 = (*c);
    layer = f32(_e160);
    let _e162 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e162).xy));
    let _e169 = sc[2u];
    currentDepth = (_e169 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e171 = currentDepth;
        let _e172 = sc;
        let _e173 = _e172.xy;
        let _e174 = layer;
        let _e177 = vec3<f32>(_e173.x, _e173.y, _e174);
        let _e183 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e177.x, _e177.y), i32(_e177.z));
        shadow = step(_e171, _e183.x);
    } else {
        if override_type_3_1 {
            let _e186 = currentDepth;
            let _e187 = sc;
            let _e188 = _e187.xy;
            let _e189 = layer;
            let _e192 = vec3<f32>(_e188.x, _e188.y, _e189);
            let _e198 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e192.x, _e192.y), i32(_e192.z));
            let _e201 = shadow;
            shadow = (_e201 + step(_e186, _e198.x));
            let _e203 = currentDepth;
            let _e204 = sc;
            let _e207 = texelSize[0u];
            let _e209 = (_e204.xy + vec2<f32>(_e207, 0f));
            let _e210 = layer;
            let _e213 = vec3<f32>(_e209.x, _e209.y, _e210);
            let _e219 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e213.x, _e213.y), i32(_e213.z));
            let _e222 = shadow;
            shadow = (_e222 + step(_e203, _e219.x));
            let _e224 = currentDepth;
            let _e225 = sc;
            let _e228 = texelSize[0u];
            let _e230 = (_e225.xy - vec2<f32>(_e228, 0f));
            let _e231 = layer;
            let _e234 = vec3<f32>(_e230.x, _e230.y, _e231);
            let _e240 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e234.x, _e234.y), i32(_e234.z));
            let _e243 = shadow;
            shadow = (_e243 + step(_e224, _e240.x));
            let _e245 = currentDepth;
            let _e246 = sc;
            let _e249 = texelSize[1u];
            let _e251 = (_e246.xy + vec2<f32>(0f, _e249));
            let _e252 = layer;
            let _e255 = vec3<f32>(_e251.x, _e251.y, _e252);
            let _e261 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e255.x, _e255.y), i32(_e255.z));
            let _e264 = shadow;
            shadow = (_e264 + step(_e245, _e261.x));
            let _e266 = currentDepth;
            let _e267 = sc;
            let _e270 = texelSize[1u];
            let _e272 = (_e267.xy - vec2<f32>(0f, _e270));
            let _e273 = layer;
            let _e276 = vec3<f32>(_e272.x, _e272.y, _e273);
            let _e282 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e276.x, _e276.y), i32(_e276.z));
            let _e285 = shadow;
            shadow = (_e285 + step(_e266, _e282.x));
            let _e287 = shadow;
            shadow = (_e287 / 5f);
        } else {
            x = -1i;
            loop {
                let _e289 = x;
                if (_e289 <= 1i) {
                    y = -1i;
                    loop {
                        let _e291 = y;
                        if (_e291 <= 1i) {
                            let _e293 = currentDepth;
                            let _e294 = sc;
                            let _e296 = x;
                            let _e298 = y;
                            let _e301 = texelSize;
                            let _e303 = (_e294.xy + (vec2<f32>(f32(_e296), f32(_e298)) * _e301));
                            let _e304 = layer;
                            let _e307 = vec3<f32>(_e303.x, _e303.y, _e304);
                            let _e313 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e307.x, _e307.y), i32(_e307.z));
                            let _e316 = shadow;
                            shadow = (_e316 + step(_e293, _e313.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e318 = y;
                            y = (_e318 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e320 = x;
                    x = (_e320 + 1i);
                }
            }
            let _e322 = shadow;
            shadow = (_e322 / 9f);
        }
    }
    let _e324 = shadow;
    return _e324;
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

    let _e115 = unnamed.cascadeSplits;
    let _e116 = (*viewDepth_1);
    cmp = step(_e115, vec4(_e116));
    let _e120 = cmp[0u];
    let _e122 = cmp[1u];
    let _e125 = cmp[2u];
    let _e128 = cmp[3u];
    cascade = min(i32((((_e120 + _e122) + _e125) + _e128)), 3i);
    let _e132 = cascade;
    (*outCascade) = _e132;
    let _e133 = cascade;
    if (_e133 == 0i) {
        local = 0f;
    } else {
        let _e135 = cascade;
        let _e140 = unnamed.cascadeSplits[max((_e135 - 1i), 0i)];
        local = _e140;
    }
    let _e141 = local;
    prevSplit = _e141;
    let _e142 = cascade;
    let _e145 = unnamed.cascadeSplits[_e142];
    farSplit = _e145;
    let _e146 = farSplit;
    let _e147 = prevSplit;
    blendRange = max((0.1f * (_e146 - _e147)), 1f);
    let _e151 = farSplit;
    let _e152 = (*viewDepth_1);
    let _e154 = blendRange;
    blendT = clamp(((_e151 - _e152) / _e154), 0f, 1f);
    let _e157 = cascade;
    param_5 = _e157;
    let _e158 = (*worldPos_1);
    param_6 = _e158;
    let _e159 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e159;
    let _e160 = cascade;
    param_7 = min((_e160 + 1i), 3i);
    let _e163 = (*worldPos_1);
    param_8 = _e163;
    let _e164 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e164;
    let _e165 = s1_;
    let _e166 = s0_;
    let _e167 = blendT;
    return mix(_e165, _e166, _e167);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e109 = unnamed.packed_indices[1i][3u];
    let _e115 = unnamed.packed_indices[1i][3u];
    let _e120 = (*lm_uv);
    let _e121 = textureSample(wired_bindless_images[(_e109 & 4095u)], wired_bindless_samplers[((_e115 >> bitcast<u32>(12i)) & 255u)], _e120);
    sun_mask = _e121.x;
    let _e123 = sun_mask;
    if (_e123 > 0.001f) {
        let _e125 = shadowData_1;
        param_9 = _e125.xyz;
        let _e128 = shadowData_1[3u];
        param_10 = _e128;
        let _e129 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e130 = param_11;
        ignoredCascade = _e130;
        shadow_1 = _e129;
        let _e131 = shadow_1;
        let _e132 = sun_mask;
        let _e134 = (*rgb);
        (*rgb) = (_e134 * mix(1f, _e131, _e132));
    }
    let _e136 = (*rgb);
    return _e136;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;
    var param_16: vec3<f32>;
    var param_17: vec2<f32>;

    if override_type_3_2 {
        let _e105 = (*rgb_1);
        param_12 = _e105;
        let _e106 = frag_tex_coord0_1;
        param_13 = _e106;
        let _e107 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e107;
    }
    if override_type_3_3 {
        let _e108 = (*rgb_1);
        param_14 = _e108;
        let _e109 = frag_tex_coord1_1;
        param_15 = _e109;
        let _e110 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e110;
    }
    if override_type_3_4 {
        let _e111 = (*rgb_1);
        param_16 = _e111;
        let _e112 = frag_tex_coord2_1;
        param_17 = _e112;
        let _e113 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_16), (&param_17));
        return _e113;
    }
    let _e114 = (*rgb_1);
    return _e114;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e101 = (*rgb_2);
    let _e104 = unnamed.worldLightParams[0u];
    boosted = (_e101 * _e104);
    let _e107 = boosted[0u];
    let _e109 = boosted[1u];
    let _e111 = boosted[2u];
    peak = max(_e107, max(_e109, _e111));
    let _e114 = peak;
    if (_e114 > 1f) {
        let _e116 = peak;
        let _e117 = boosted;
        boosted = (_e117 / vec3(_e116));
    }
    let _e120 = boosted;
    return _e120;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e102 = (*c_1);
    (*c_1) = max(_e102, vec3<f32>(0f, 0f, 0f));
    let _e104 = (*c_1);
    cutoff = (_e104 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e106 = (*c_1);
    lo = (_e106 / vec3(12.92f));
    let _e109 = (*c_1);
    hi = pow(((_e109 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e114 = hi;
    let _e115 = lo;
    let _e116 = cutoff;
    return mix(_e114, _e115, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e116));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_18: vec3<f32>;
    var param_19: vec3<f32>;

    let _e104 = (*role);
    let _e106 = (*role);
    let _e111 = unnamed.packed_indices[(_e104 / 4u)][(_e106 % 4u)];
    let _e114 = (*role);
    let _e116 = (*role);
    let _e121 = unnamed.packed_indices[(_e114 / 4u)][(_e116 % 4u)];
    let _e126 = (*uv);
    let _e127 = textureSample(wired_bindless_images[(_e111 & 4095u)], wired_bindless_samplers[((_e121 >> bitcast<u32>(12i)) & 255u)], _e126);
    c_2 = _e127;
    let _e128 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e128))) == 0i) {
        let _e133 = c_2;
        param_18 = _e133.xyz;
        let _e135 = sRGBToLinear_u0028_vf3_u003b((&param_18));
        c_2[0u] = _e135.x;
        c_2[1u] = _e135.y;
        c_2[2u] = _e135.z;
    }
    let _e142 = (*slot);
    if (lightmap_slot == (_e142 + 1i)) {
        let _e145 = c_2;
        param_19 = _e145.xyz;
        let _e147 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_19));
        c_2[0u] = _e147.x;
        c_2[1u] = _e147.y;
        c_2[2u] = _e147.z;
    }
    let _e154 = c_2;
    return _e154;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_20: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var sampledColor1_: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var sampledColor2_: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var param_30: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_31: f32;
    var param_32: vec4<f32>;

    let _e131 = unnamed.packed_indices[0i][3u];
    let _e137 = unnamed.packed_indices[0i][3u];
    let _e142 = fog_tex_coord_1;
    let _e143 = textureSample(wired_bindless_images[(_e131 & 4095u)], wired_bindless_samplers[((_e137 >> bitcast<u32>(12i)) & 255u)], _e142);
    fog = _e143;
    let _e144 = frag_color0In_1;
    param_20 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_20));
    let _e148 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e146.x, _e146.y, _e146.z, _e148);
    param_21 = 0u;
    let _e153 = frag_tex_coord0_1;
    param_22 = _e153;
    param_23 = 0i;
    let _e154 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
    sampledColor0_ = _e154;
    param_24 = 1u;
    let _e155 = frag_tex_coord1_1;
    param_25 = _e155;
    param_26 = 1i;
    let _e156 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
    sampledColor1_ = _e156;
    param_27 = 2u;
    let _e157 = frag_tex_coord2_1;
    param_28 = _e157;
    param_29 = 2i;
    let _e158 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
    sampledColor2_ = _e158;
    let _e159 = sampledColor0_;
    let _e160 = frag_color0_;
    color0_ = (_e159 * _e160);
    if override_type_3_5 {
        let _e162 = sampledColor1_;
        color1_ = _e162;
        let _e163 = sampledColor2_;
        color2_ = _e163;
        let _e164 = color0_;
        let _e166 = color1_;
        let _e169 = color2_;
        let _e171 = ((_e164.xyz + _e166.xyz) + _e169.xyz);
        let _e173 = color0_[3u];
        let _e175 = color1_[3u];
        let _e178 = color2_[3u];
        base = vec4<f32>(_e171.x, _e171.y, _e171.z, ((_e173 * _e175) * _e178));
    } else {
        if override_type_3_6 {
            let _e184 = sampledColor1_;
            let _e185 = frag_color0_;
            color1_1 = (_e184 * _e185);
            let _e187 = sampledColor2_;
            let _e188 = frag_color0_;
            color2_1 = (_e187 * _e188);
            let _e190 = color0_;
            let _e192 = color1_1;
            let _e195 = color2_1;
            let _e197 = ((_e190.xyz + _e192.xyz) + _e195.xyz);
            let _e199 = color0_[3u];
            let _e201 = color1_1[3u];
            let _e204 = color2_1[3u];
            base = vec4<f32>(_e197.x, _e197.y, _e197.z, ((_e199 * _e201) * _e204));
        } else {
            let _e210 = sampledColor1_;
            color1_2 = _e210;
            let _e211 = sampledColor2_;
            color2_2 = _e211;
            let _e212 = color0_;
            let _e214 = color1_2;
            let _e217 = color2_2;
            let _e219 = ((_e212.xyz * _e214.xyz) * _e217.xyz);
            base[0u] = _e219.x;
            base[1u] = _e219.y;
            base[2u] = _e219.z;
            let _e227 = color0_[3u];
            let _e229 = color1_2[3u];
            let _e232 = color2_2[3u];
            base[3u] = ((_e227 * _e229) * _e232);
        }
    }
    let _e235 = base;
    param_30 = _e235.xyz;
    let _e237 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_30));
    base[0u] = _e237.x;
    base[1u] = _e237.y;
    base[2u] = _e237.z;
    if override_type_3_7 {
        let _e246 = unnamed.worldLightParams[1u];
        wetness = clamp(_e246, 0f, 1f);
        let _e250 = unnamed.worldLightParams[2u];
        frost = clamp(_e250, 0f, 1f);
        let _e252 = base;
        luminance = dot(_e252.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e255 = wetness;
        let _e257 = base;
        let _e259 = (_e257.xyz * mix(1f, 0.82f, _e255));
        base[0u] = _e259.x;
        base[1u] = _e259.y;
        base[2u] = _e259.z;
        let _e266 = base;
        let _e268 = luminance;
        let _e270 = luminance;
        let _e272 = luminance;
        let _e274 = frost;
        let _e277 = mix(_e266.xyz, vec3<f32>((_e268 * 0.88f), (_e270 * 0.94f), _e272), vec3((_e274 * 0.55f)));
        base[0u] = _e277.x;
        base[1u] = _e277.y;
        base[2u] = _e277.z;
    }
    let _e286 = unnamed.materialFactors[0u];
    if (_e286 >= 0f) {
        let _e289 = unnamed.materialBaseColor;
        let _e290 = base;
        base = (_e290 * _e289);
    }
    let _e292 = color0_;
    let _e295 = unnamed.emissionRadiance;
    let _e298 = base;
    let _e300 = (_e298.xyz + (_e292.xyz * _e295.xyz));
    base[0u] = _e300.x;
    base[1u] = _e300.y;
    base[2u] = _e300.z;
    let _e307 = wired_advanced_fog_enabled_u0028_();
    if _e307 {
        let _e308 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e308;
        if override_type_3_8 {
            let _e309 = fogAmount;
            let _e311 = base;
            let _e313 = (_e311.xyz * (1f - _e309));
            base[0u] = _e313.x;
            base[1u] = _e313.y;
            base[2u] = _e313.z;
        } else {
            if override_type_3_9 {
                let _e320 = fogAmount;
                let _e322 = base;
                base = (_e322 * (1f - _e320));
            } else {
                if override_type_3_10 {
                    let _e324 = fogAmount;
                    let _e327 = base[3u];
                    base[3u] = (_e327 * (1f - _e324));
                } else {
                    let _e330 = base;
                    let _e333 = unnamed.advancedFogColorDensity;
                    let _e335 = fogAmount;
                    let _e337 = mix(_e330.xyz, _e333.xyz, vec3(_e335));
                    base[0u] = _e337.x;
                    base[1u] = _e337.y;
                    base[2u] = _e337.z;
                }
            }
        }
    } else {
        if override_type_3_11 {
            let _e344 = base;
            let _e347 = fog[3u];
            let _e349 = (_e344.xyz * (1f - _e347));
            base[0u] = _e349.x;
            base[1u] = _e349.y;
            base[2u] = _e349.z;
        } else {
            if override_type_3_12 {
                let _e356 = base;
                let _e358 = fog[3u];
                base = (_e356 * (1f - _e358));
            } else {
                if override_type_3_13 {
                    let _e362 = base[3u];
                    let _e364 = fog[3u];
                    base[3u] = (_e362 * (1f - _e364));
                } else {
                    let _e368 = base;
                    let _e369 = fog;
                    let _e371 = unnamed.fogColor;
                    let _e374 = fog[3u];
                    base = mix(_e368, (_e369 * _e371), vec4(_e374));
                }
            }
        }
    }
    if override_type_3_14 {
        let _e378 = base[3u];
        if (_e378 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e380 = base;
            let _e382 = base;
            if (dot(_e380.xyz, _e382.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e386 = base;
    out_color = _e386;
    param_31 = 1f;
    let _e387 = base;
    param_32 = _e387;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_31), (&param_32));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
