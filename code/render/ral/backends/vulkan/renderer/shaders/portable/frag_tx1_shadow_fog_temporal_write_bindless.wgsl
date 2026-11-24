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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e97 = (*value);
    let _e98 = (*value);
    let _e100 = all((_e97 == _e98));
    phi_98_ = _e100;
    if _e100 {
        let _e101 = (*value);
        phi_98_ = all((abs(_e101) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e106 = phi_98_;
    return _e106;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e97 = (*value_1);
    let _e98 = (*value_1);
    let _e100 = all((_e97 == _e98));
    phi_83_ = _e100;
    if _e100 {
        let _e101 = (*value_1);
        phi_83_ = all((abs(_e101) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e106 = phi_83_;
    return _e106;
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
    let _e108 = temporalOutcome_1;
    let _e109 = (_e108 != 1u);
    phi_122_ = _e109;
    if !(_e109) {
        let _e111 = temporalCurrentClip_1;
        param = _e111;
        let _e112 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e112);
    }
    let _e115 = phi_122_;
    phi_131_ = _e115;
    if !(_e115) {
        let _e117 = temporalPreviousClip_1;
        param_1 = _e117;
        let _e118 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e118);
    }
    let _e121 = phi_131_;
    phi_141_ = _e121;
    if !(_e121) {
        let _e124 = temporalCurrentClip_1[3u];
        phi_141_ = (_e124 <= 0.000001f);
    }
    let _e127 = phi_141_;
    phi_148_ = _e127;
    if !(_e127) {
        let _e130 = temporalPreviousClip_1[3u];
        phi_148_ = (_e130 <= 0.000001f);
    }
    let _e133 = phi_148_;
    if _e133 {
        return;
    }
    let _e134 = temporalCurrentClip_1;
    let _e137 = temporalCurrentClip_1[3u];
    currentNdc = (_e134.xy / vec2(_e137));
    let _e140 = temporalPreviousClip_1;
    let _e143 = temporalPreviousClip_1[3u];
    previousNdc = (_e140.xy / vec2(_e143));
    let _e146 = currentNdc;
    param_2 = _e146;
    let _e147 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e148 = !(_e147);
    phi_177_ = _e148;
    if !(_e148) {
        let _e150 = previousNdc;
        param_3 = _e150;
        let _e151 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e151);
    }
    let _e154 = phi_177_;
    if _e154 {
        return;
    }
    let _e155 = currentNdc;
    currentUv = ((_e155 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e158 = previousNdc;
    previousUv = ((_e158 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e161 = currentUv;
    let _e162 = previousUv;
    velocity = (_e161 - _e162);
    let _e164 = velocity;
    param_4 = _e164;
    let _e165 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e165) {
        return;
    }
    let _e167 = velocity;
    out_temporal_velocity = _e167;
    let _e168 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e168, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e99 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e99 + 0.5f));
    let _e104 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e106 = fogType;
    let _e109 = fogType;
    return (((_e104 > 0.5f) && (_e106 >= 1i)) && (_e109 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e99 = wired_advanced_fog_enabled_u0028_();
    if !(_e99) {
        return 0f;
    }
    let _e102 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e102, 0.000001f));
    let _e107 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e107 + 0.5f));
    let _e110 = fogType_1;
    if (_e110 == 1i) {
        let _e114 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e114 <= 0f) {
            return 0f;
        }
        let _e116 = viewDepth;
        let _e119 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e116 / _e119), 0f, 1f);
    }
    let _e124 = unnamed.advancedFogColorDensity[3u];
    let _e126 = viewDepth;
    opticalDepth = (max(_e124, 0f) * _e126);
    let _e128 = fogType_1;
    if (_e128 == 2i) {
        let _e130 = opticalDepth;
        return clamp((1f - exp(-(_e130))), 0f, 1f);
    }
    let _e135 = opticalDepth;
    let _e136 = opticalDepth;
    return clamp((1f - exp(-((_e135 * _e136)))), 0f, 1f);
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

    let _e106 = (*c);
    let _e109 = unnamed.cascadeMVP[_e106];
    let _e110 = (*worldPos);
    sc4_ = (_e109 * vec4<f32>(_e110.x, _e110.y, _e110.z, 1f));
    let _e116 = sc4_;
    let _e119 = sc4_[3u];
    sc = (_e116.xyz / vec3(_e119));
    let _e122 = sc;
    let _e126 = ((_e122.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e126.x;
    sc[1u] = _e126.y;
    let _e132 = sc[0u];
    let _e133 = (_e132 < 0f);
    phi_473_ = _e133;
    if !(_e133) {
        let _e136 = sc[0u];
        phi_473_ = (_e136 > 1f);
    }
    let _e139 = phi_473_;
    phi_480_ = _e139;
    if !(_e139) {
        let _e142 = sc[1u];
        phi_480_ = (_e142 < 0f);
    }
    let _e145 = phi_480_;
    phi_487_ = _e145;
    if !(_e145) {
        let _e148 = sc[1u];
        phi_487_ = (_e148 > 1f);
    }
    let _e151 = phi_487_;
    phi_494_ = _e151;
    if !(_e151) {
        let _e154 = sc[2u];
        phi_494_ = (_e154 > 1f);
    }
    let _e157 = phi_494_;
    if _e157 {
        return 1f;
    }
    let _e158 = (*c);
    layer = f32(_e158);
    let _e160 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e160).xy));
    let _e167 = sc[2u];
    currentDepth = (_e167 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e169 = currentDepth;
        let _e170 = sc;
        let _e171 = _e170.xy;
        let _e172 = layer;
        let _e175 = vec3<f32>(_e171.x, _e171.y, _e172);
        let _e181 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e175.x, _e175.y), i32(_e175.z));
        shadow = step(_e169, _e181.x);
    } else {
        if override_type_3_1 {
            let _e184 = currentDepth;
            let _e185 = sc;
            let _e186 = _e185.xy;
            let _e187 = layer;
            let _e190 = vec3<f32>(_e186.x, _e186.y, _e187);
            let _e196 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e190.x, _e190.y), i32(_e190.z));
            let _e199 = shadow;
            shadow = (_e199 + step(_e184, _e196.x));
            let _e201 = currentDepth;
            let _e202 = sc;
            let _e205 = texelSize[0u];
            let _e207 = (_e202.xy + vec2<f32>(_e205, 0f));
            let _e208 = layer;
            let _e211 = vec3<f32>(_e207.x, _e207.y, _e208);
            let _e217 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e211.x, _e211.y), i32(_e211.z));
            let _e220 = shadow;
            shadow = (_e220 + step(_e201, _e217.x));
            let _e222 = currentDepth;
            let _e223 = sc;
            let _e226 = texelSize[0u];
            let _e228 = (_e223.xy - vec2<f32>(_e226, 0f));
            let _e229 = layer;
            let _e232 = vec3<f32>(_e228.x, _e228.y, _e229);
            let _e238 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e232.x, _e232.y), i32(_e232.z));
            let _e241 = shadow;
            shadow = (_e241 + step(_e222, _e238.x));
            let _e243 = currentDepth;
            let _e244 = sc;
            let _e247 = texelSize[1u];
            let _e249 = (_e244.xy + vec2<f32>(0f, _e247));
            let _e250 = layer;
            let _e253 = vec3<f32>(_e249.x, _e249.y, _e250);
            let _e259 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e253.x, _e253.y), i32(_e253.z));
            let _e262 = shadow;
            shadow = (_e262 + step(_e243, _e259.x));
            let _e264 = currentDepth;
            let _e265 = sc;
            let _e268 = texelSize[1u];
            let _e270 = (_e265.xy - vec2<f32>(0f, _e268));
            let _e271 = layer;
            let _e274 = vec3<f32>(_e270.x, _e270.y, _e271);
            let _e280 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e274.x, _e274.y), i32(_e274.z));
            let _e283 = shadow;
            shadow = (_e283 + step(_e264, _e280.x));
            let _e285 = shadow;
            shadow = (_e285 / 5f);
        } else {
            x = -1i;
            loop {
                let _e287 = x;
                if (_e287 <= 1i) {
                    y = -1i;
                    loop {
                        let _e289 = y;
                        if (_e289 <= 1i) {
                            let _e291 = currentDepth;
                            let _e292 = sc;
                            let _e294 = x;
                            let _e296 = y;
                            let _e299 = texelSize;
                            let _e301 = (_e292.xy + (vec2<f32>(f32(_e294), f32(_e296)) * _e299));
                            let _e302 = layer;
                            let _e305 = vec3<f32>(_e301.x, _e301.y, _e302);
                            let _e311 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e305.x, _e305.y), i32(_e305.z));
                            let _e314 = shadow;
                            shadow = (_e314 + step(_e291, _e311.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e316 = y;
                            y = (_e316 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e318 = x;
                    x = (_e318 + 1i);
                }
            }
            let _e320 = shadow;
            shadow = (_e320 / 9f);
        }
    }
    let _e322 = shadow;
    return _e322;
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

    let _e113 = unnamed.cascadeSplits;
    let _e114 = (*viewDepth_1);
    cmp = step(_e113, vec4(_e114));
    let _e118 = cmp[0u];
    let _e120 = cmp[1u];
    let _e123 = cmp[2u];
    let _e126 = cmp[3u];
    cascade = min(i32((((_e118 + _e120) + _e123) + _e126)), 3i);
    let _e130 = cascade;
    (*outCascade) = _e130;
    let _e131 = cascade;
    if (_e131 == 0i) {
        local = 0f;
    } else {
        let _e133 = cascade;
        let _e138 = unnamed.cascadeSplits[max((_e133 - 1i), 0i)];
        local = _e138;
    }
    let _e139 = local;
    prevSplit = _e139;
    let _e140 = cascade;
    let _e143 = unnamed.cascadeSplits[_e140];
    farSplit = _e143;
    let _e144 = farSplit;
    let _e145 = prevSplit;
    blendRange = max((0.1f * (_e144 - _e145)), 1f);
    let _e149 = farSplit;
    let _e150 = (*viewDepth_1);
    let _e152 = blendRange;
    blendT = clamp(((_e149 - _e150) / _e152), 0f, 1f);
    let _e155 = cascade;
    param_5 = _e155;
    let _e156 = (*worldPos_1);
    param_6 = _e156;
    let _e157 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e157;
    let _e158 = cascade;
    param_7 = min((_e158 + 1i), 3i);
    let _e161 = (*worldPos_1);
    param_8 = _e161;
    let _e162 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e162;
    let _e163 = s1_;
    let _e164 = s0_;
    let _e165 = blendT;
    return mix(_e163, _e164, _e165);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e107 = unnamed.packed_indices[1i][3u];
    let _e113 = unnamed.packed_indices[1i][3u];
    let _e118 = (*lm_uv);
    let _e119 = textureSample(wired_bindless_images[(_e107 & 4095u)], wired_bindless_samplers[((_e113 >> bitcast<u32>(12i)) & 255u)], _e118);
    sun_mask = _e119.x;
    let _e121 = sun_mask;
    if (_e121 > 0.001f) {
        let _e123 = shadowData_1;
        param_9 = _e123.xyz;
        let _e126 = shadowData_1[3u];
        param_10 = _e126;
        let _e127 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e128 = param_11;
        ignoredCascade = _e128;
        shadow_1 = _e127;
        let _e129 = shadow_1;
        let _e130 = sun_mask;
        let _e132 = (*rgb);
        (*rgb) = (_e132 * mix(1f, _e129, _e130));
    }
    let _e134 = (*rgb);
    return _e134;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;

    if override_type_3_2 {
        let _e101 = (*rgb_1);
        param_12 = _e101;
        let _e102 = frag_tex_coord0_1;
        param_13 = _e102;
        let _e103 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e103;
    }
    if override_type_3_3 {
        let _e104 = (*rgb_1);
        param_14 = _e104;
        let _e105 = frag_tex_coord1_1;
        param_15 = _e105;
        let _e106 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e106;
    }
    let _e107 = (*rgb_1);
    return _e107;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e99 = (*rgb_2);
    let _e102 = unnamed.worldLightParams[0u];
    boosted = (_e99 * _e102);
    let _e105 = boosted[0u];
    let _e107 = boosted[1u];
    let _e109 = boosted[2u];
    peak = max(_e105, max(_e107, _e109));
    let _e112 = peak;
    if (_e112 > 1f) {
        let _e114 = peak;
        let _e115 = boosted;
        boosted = (_e115 / vec3(_e114));
    }
    let _e118 = boosted;
    return _e118;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e100 = (*c_1);
    (*c_1) = max(_e100, vec3<f32>(0f, 0f, 0f));
    let _e102 = (*c_1);
    cutoff = (_e102 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e104 = (*c_1);
    lo = (_e104 / vec3(12.92f));
    let _e107 = (*c_1);
    hi = pow(((_e107 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e112 = hi;
    let _e113 = lo;
    let _e114 = cutoff;
    return mix(_e112, _e113, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e114));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;

    let _e102 = (*role);
    let _e104 = (*role);
    let _e109 = unnamed.packed_indices[(_e102 / 4u)][(_e104 % 4u)];
    let _e112 = (*role);
    let _e114 = (*role);
    let _e119 = unnamed.packed_indices[(_e112 / 4u)][(_e114 % 4u)];
    let _e124 = (*uv);
    let _e125 = textureSample(wired_bindless_images[(_e109 & 4095u)], wired_bindless_samplers[((_e119 >> bitcast<u32>(12i)) & 255u)], _e124);
    c_2 = _e125;
    let _e126 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e126))) == 0i) {
        let _e131 = c_2;
        param_16 = _e131.xyz;
        let _e133 = sRGBToLinear_u0028_vf3_u003b((&param_16));
        c_2[0u] = _e133.x;
        c_2[1u] = _e133.y;
        c_2[2u] = _e133.z;
    }
    let _e140 = (*slot);
    if (lightmap_slot == (_e140 + 1i)) {
        let _e143 = c_2;
        param_17 = _e143.xyz;
        let _e145 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_17));
        c_2[0u] = _e145.x;
        c_2[1u] = _e145.y;
        c_2[2u] = _e145.z;
    }
    let _e152 = c_2;
    return _e152;
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
    var param_27: vec4<f32>;

    let _e122 = unnamed.packed_indices[0i][3u];
    let _e128 = unnamed.packed_indices[0i][3u];
    let _e133 = fog_tex_coord_1;
    let _e134 = textureSample(wired_bindless_images[(_e122 & 4095u)], wired_bindless_samplers[((_e128 >> bitcast<u32>(12i)) & 255u)], _e133);
    fog = _e134;
    let _e135 = frag_color0In_1;
    param_18 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_18));
    let _e139 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    param_19 = 0u;
    let _e144 = frag_tex_coord0_1;
    param_20 = _e144;
    param_21 = 0i;
    let _e145 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
    sampledColor0_ = _e145;
    param_22 = 1u;
    let _e146 = frag_tex_coord1_1;
    param_23 = _e146;
    param_24 = 1i;
    let _e147 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
    sampledColor1_ = _e147;
    let _e148 = sampledColor0_;
    let _e149 = frag_color0_;
    color0_ = (_e148 * _e149);
    if override_type_3_4 {
        let _e151 = sampledColor1_;
        color1_ = _e151;
        let _e152 = color0_;
        let _e154 = color1_;
        let _e156 = (_e152.xyz + _e154.xyz);
        let _e158 = color0_[3u];
        let _e160 = color1_[3u];
        base = vec4<f32>(_e156.x, _e156.y, _e156.z, (_e158 * _e160));
    } else {
        if override_type_3_5 {
            let _e166 = sampledColor1_;
            let _e167 = frag_color0_;
            color1_1 = (_e166 * _e167);
            let _e169 = color0_;
            let _e171 = color1_1;
            let _e173 = (_e169.xyz + _e171.xyz);
            let _e175 = color0_[3u];
            let _e177 = color1_1[3u];
            base = vec4<f32>(_e173.x, _e173.y, _e173.z, (_e175 * _e177));
        } else {
            let _e183 = sampledColor1_;
            color1_2 = _e183;
            let _e184 = color0_;
            let _e186 = color1_2;
            let _e188 = (_e184.xyz * _e186.xyz);
            base[0u] = _e188.x;
            base[1u] = _e188.y;
            base[2u] = _e188.z;
            let _e196 = color0_[3u];
            let _e198 = color1_2[3u];
            base[3u] = (_e196 * _e198);
        }
    }
    let _e201 = base;
    param_25 = _e201.xyz;
    let _e203 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e203.x;
    base[1u] = _e203.y;
    base[2u] = _e203.z;
    if override_type_3_6 {
        let _e212 = unnamed.worldLightParams[1u];
        wetness = clamp(_e212, 0f, 1f);
        let _e216 = unnamed.worldLightParams[2u];
        frost = clamp(_e216, 0f, 1f);
        let _e218 = base;
        luminance = dot(_e218.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e221 = wetness;
        let _e223 = base;
        let _e225 = (_e223.xyz * mix(1f, 0.82f, _e221));
        base[0u] = _e225.x;
        base[1u] = _e225.y;
        base[2u] = _e225.z;
        let _e232 = base;
        let _e234 = luminance;
        let _e236 = luminance;
        let _e238 = luminance;
        let _e240 = frost;
        let _e243 = mix(_e232.xyz, vec3<f32>((_e234 * 0.88f), (_e236 * 0.94f), _e238), vec3((_e240 * 0.55f)));
        base[0u] = _e243.x;
        base[1u] = _e243.y;
        base[2u] = _e243.z;
    }
    let _e252 = unnamed.materialFactors[0u];
    if (_e252 >= 0f) {
        let _e255 = unnamed.materialBaseColor;
        let _e256 = base;
        base = (_e256 * _e255);
    }
    let _e258 = color0_;
    let _e261 = unnamed.emissionRadiance;
    let _e264 = base;
    let _e266 = (_e264.xyz + (_e258.xyz * _e261.xyz));
    base[0u] = _e266.x;
    base[1u] = _e266.y;
    base[2u] = _e266.z;
    let _e273 = wired_advanced_fog_enabled_u0028_();
    if _e273 {
        let _e274 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e274;
        if override_type_3_7 {
            let _e275 = fogAmount;
            let _e277 = base;
            let _e279 = (_e277.xyz * (1f - _e275));
            base[0u] = _e279.x;
            base[1u] = _e279.y;
            base[2u] = _e279.z;
        } else {
            if override_type_3_8 {
                let _e286 = fogAmount;
                let _e288 = base;
                base = (_e288 * (1f - _e286));
            } else {
                if override_type_3_9 {
                    let _e290 = fogAmount;
                    let _e293 = base[3u];
                    base[3u] = (_e293 * (1f - _e290));
                } else {
                    let _e296 = base;
                    let _e299 = unnamed.advancedFogColorDensity;
                    let _e301 = fogAmount;
                    let _e303 = mix(_e296.xyz, _e299.xyz, vec3(_e301));
                    base[0u] = _e303.x;
                    base[1u] = _e303.y;
                    base[2u] = _e303.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e310 = base;
            let _e313 = fog[3u];
            let _e315 = (_e310.xyz * (1f - _e313));
            base[0u] = _e315.x;
            base[1u] = _e315.y;
            base[2u] = _e315.z;
        } else {
            if override_type_3_11 {
                let _e322 = base;
                let _e324 = fog[3u];
                base = (_e322 * (1f - _e324));
            } else {
                if override_type_3_12 {
                    let _e328 = base[3u];
                    let _e330 = fog[3u];
                    base[3u] = (_e328 * (1f - _e330));
                } else {
                    let _e334 = base;
                    let _e335 = fog;
                    let _e337 = unnamed.fogColor;
                    let _e340 = fog[3u];
                    base = mix(_e334, (_e335 * _e337), vec4(_e340));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e344 = base[3u];
        if (_e344 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e346 = base;
            let _e348 = base;
            if (dot(_e346.xyz, _e348.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e352 = base;
    out_color = _e352;
    param_26 = 1f;
    let _e353 = base;
    param_27 = _e353;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_26), (&param_27));
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
