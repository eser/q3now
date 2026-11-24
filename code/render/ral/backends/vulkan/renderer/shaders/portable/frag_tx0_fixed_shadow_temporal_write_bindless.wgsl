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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_4: bool = (discard_mode == 1i);
override override_type_3_5: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_98_: bool;

    let _e84 = (*value);
    let _e85 = (*value);
    let _e87 = all((_e84 == _e85));
    phi_98_ = _e87;
    if _e87 {
        let _e88 = (*value);
        phi_98_ = all((abs(_e88) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e93 = phi_98_;
    return _e93;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_83_: bool;

    let _e84 = (*value_1);
    let _e85 = (*value_1);
    let _e87 = all((_e84 == _e85));
    phi_83_ = _e87;
    if _e87 {
        let _e88 = (*value_1);
        phi_83_ = all((abs(_e88) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e93 = phi_83_;
    return _e93;
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
    let _e95 = temporalOutcome_1;
    let _e96 = (_e95 != 1u);
    phi_122_ = _e96;
    if !(_e96) {
        let _e98 = temporalCurrentClip_1;
        param = _e98;
        let _e99 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_122_ = !(_e99);
    }
    let _e102 = phi_122_;
    phi_131_ = _e102;
    if !(_e102) {
        let _e104 = temporalPreviousClip_1;
        param_1 = _e104;
        let _e105 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_131_ = !(_e105);
    }
    let _e108 = phi_131_;
    phi_141_ = _e108;
    if !(_e108) {
        let _e111 = temporalCurrentClip_1[3u];
        phi_141_ = (_e111 <= 0.000001f);
    }
    let _e114 = phi_141_;
    phi_148_ = _e114;
    if !(_e114) {
        let _e117 = temporalPreviousClip_1[3u];
        phi_148_ = (_e117 <= 0.000001f);
    }
    let _e120 = phi_148_;
    if _e120 {
        return;
    }
    let _e121 = temporalCurrentClip_1;
    let _e124 = temporalCurrentClip_1[3u];
    currentNdc = (_e121.xy / vec2(_e124));
    let _e127 = temporalPreviousClip_1;
    let _e130 = temporalPreviousClip_1[3u];
    previousNdc = (_e127.xy / vec2(_e130));
    let _e133 = currentNdc;
    param_2 = _e133;
    let _e134 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e135 = !(_e134);
    phi_177_ = _e135;
    if !(_e135) {
        let _e137 = previousNdc;
        param_3 = _e137;
        let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_177_ = !(_e138);
    }
    let _e141 = phi_177_;
    if _e141 {
        return;
    }
    let _e142 = currentNdc;
    currentUv = ((_e142 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e145 = previousNdc;
    previousUv = ((_e145 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e148 = currentUv;
    let _e149 = previousUv;
    velocity = (_e148 - _e149);
    let _e151 = velocity;
    param_4 = _e151;
    let _e152 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e152) {
        return;
    }
    let _e154 = velocity;
    out_temporal_velocity = _e154;
    let _e155 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e155, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e86 + 0.5f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e93 = fogType;
    let _e96 = fogType;
    return (((_e91 > 0.5f) && (_e93 >= 1i)) && (_e96 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e86 = wired_advanced_fog_enabled_u0028_();
    if !(_e86) {
        return 0f;
    }
    let _e89 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e89, 0.000001f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e94 + 0.5f));
    let _e97 = fogType_1;
    if (_e97 == 1i) {
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e101 <= 0f) {
            return 0f;
        }
        let _e103 = viewDepth;
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e103 / _e106), 0f, 1f);
    }
    let _e111 = unnamed.advancedFogColorDensity[3u];
    let _e113 = viewDepth;
    opticalDepth = (max(_e111, 0f) * _e113);
    let _e115 = fogType_1;
    if (_e115 == 2i) {
        let _e117 = opticalDepth;
        return clamp((1f - exp(-(_e117))), 0f, 1f);
    }
    let _e122 = opticalDepth;
    let _e123 = opticalDepth;
    return clamp((1f - exp(-((_e122 * _e123)))), 0f, 1f);
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

    let _e93 = (*c);
    let _e96 = unnamed.cascadeMVP[_e93];
    let _e97 = (*worldPos);
    sc4_ = (_e96 * vec4<f32>(_e97.x, _e97.y, _e97.z, 1f));
    let _e103 = sc4_;
    let _e106 = sc4_[3u];
    sc = (_e103.xyz / vec3(_e106));
    let _e109 = sc;
    let _e113 = ((_e109.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e113.x;
    sc[1u] = _e113.y;
    let _e119 = sc[0u];
    let _e120 = (_e119 < 0f);
    phi_473_ = _e120;
    if !(_e120) {
        let _e123 = sc[0u];
        phi_473_ = (_e123 > 1f);
    }
    let _e126 = phi_473_;
    phi_480_ = _e126;
    if !(_e126) {
        let _e129 = sc[1u];
        phi_480_ = (_e129 < 0f);
    }
    let _e132 = phi_480_;
    phi_487_ = _e132;
    if !(_e132) {
        let _e135 = sc[1u];
        phi_487_ = (_e135 > 1f);
    }
    let _e138 = phi_487_;
    phi_494_ = _e138;
    if !(_e138) {
        let _e141 = sc[2u];
        phi_494_ = (_e141 > 1f);
    }
    let _e144 = phi_494_;
    if _e144 {
        return 1f;
    }
    let _e145 = (*c);
    layer = f32(_e145);
    let _e147 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e147).xy));
    let _e154 = sc[2u];
    currentDepth = (_e154 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e156 = currentDepth;
        let _e157 = sc;
        let _e158 = _e157.xy;
        let _e159 = layer;
        let _e162 = vec3<f32>(_e158.x, _e158.y, _e159);
        let _e168 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e162.x, _e162.y), i32(_e162.z));
        shadow = step(_e156, _e168.x);
    } else {
        if override_type_3_1 {
            let _e171 = currentDepth;
            let _e172 = sc;
            let _e173 = _e172.xy;
            let _e174 = layer;
            let _e177 = vec3<f32>(_e173.x, _e173.y, _e174);
            let _e183 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e177.x, _e177.y), i32(_e177.z));
            let _e186 = shadow;
            shadow = (_e186 + step(_e171, _e183.x));
            let _e188 = currentDepth;
            let _e189 = sc;
            let _e192 = texelSize[0u];
            let _e194 = (_e189.xy + vec2<f32>(_e192, 0f));
            let _e195 = layer;
            let _e198 = vec3<f32>(_e194.x, _e194.y, _e195);
            let _e204 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e198.x, _e198.y), i32(_e198.z));
            let _e207 = shadow;
            shadow = (_e207 + step(_e188, _e204.x));
            let _e209 = currentDepth;
            let _e210 = sc;
            let _e213 = texelSize[0u];
            let _e215 = (_e210.xy - vec2<f32>(_e213, 0f));
            let _e216 = layer;
            let _e219 = vec3<f32>(_e215.x, _e215.y, _e216);
            let _e225 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e219.x, _e219.y), i32(_e219.z));
            let _e228 = shadow;
            shadow = (_e228 + step(_e209, _e225.x));
            let _e230 = currentDepth;
            let _e231 = sc;
            let _e234 = texelSize[1u];
            let _e236 = (_e231.xy + vec2<f32>(0f, _e234));
            let _e237 = layer;
            let _e240 = vec3<f32>(_e236.x, _e236.y, _e237);
            let _e246 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e240.x, _e240.y), i32(_e240.z));
            let _e249 = shadow;
            shadow = (_e249 + step(_e230, _e246.x));
            let _e251 = currentDepth;
            let _e252 = sc;
            let _e255 = texelSize[1u];
            let _e257 = (_e252.xy - vec2<f32>(0f, _e255));
            let _e258 = layer;
            let _e261 = vec3<f32>(_e257.x, _e257.y, _e258);
            let _e267 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e261.x, _e261.y), i32(_e261.z));
            let _e270 = shadow;
            shadow = (_e270 + step(_e251, _e267.x));
            let _e272 = shadow;
            shadow = (_e272 / 5f);
        } else {
            x = -1i;
            loop {
                let _e274 = x;
                if (_e274 <= 1i) {
                    y = -1i;
                    loop {
                        let _e276 = y;
                        if (_e276 <= 1i) {
                            let _e278 = currentDepth;
                            let _e279 = sc;
                            let _e281 = x;
                            let _e283 = y;
                            let _e286 = texelSize;
                            let _e288 = (_e279.xy + (vec2<f32>(f32(_e281), f32(_e283)) * _e286));
                            let _e289 = layer;
                            let _e292 = vec3<f32>(_e288.x, _e288.y, _e289);
                            let _e298 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e292.x, _e292.y), i32(_e292.z));
                            let _e301 = shadow;
                            shadow = (_e301 + step(_e278, _e298.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e303 = y;
                            y = (_e303 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e305 = x;
                    x = (_e305 + 1i);
                }
            }
            let _e307 = shadow;
            shadow = (_e307 / 9f);
        }
    }
    let _e309 = shadow;
    return _e309;
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

    let _e100 = unnamed.cascadeSplits;
    let _e101 = (*viewDepth_1);
    cmp = step(_e100, vec4(_e101));
    let _e105 = cmp[0u];
    let _e107 = cmp[1u];
    let _e110 = cmp[2u];
    let _e113 = cmp[3u];
    cascade = min(i32((((_e105 + _e107) + _e110) + _e113)), 3i);
    let _e117 = cascade;
    (*outCascade) = _e117;
    let _e118 = cascade;
    if (_e118 == 0i) {
        local = 0f;
    } else {
        let _e120 = cascade;
        let _e125 = unnamed.cascadeSplits[max((_e120 - 1i), 0i)];
        local = _e125;
    }
    let _e126 = local;
    prevSplit = _e126;
    let _e127 = cascade;
    let _e130 = unnamed.cascadeSplits[_e127];
    farSplit = _e130;
    let _e131 = farSplit;
    let _e132 = prevSplit;
    blendRange = max((0.1f * (_e131 - _e132)), 1f);
    let _e136 = farSplit;
    let _e137 = (*viewDepth_1);
    let _e139 = blendRange;
    blendT = clamp(((_e136 - _e137) / _e139), 0f, 1f);
    let _e142 = cascade;
    param_5 = _e142;
    let _e143 = (*worldPos_1);
    param_6 = _e143;
    let _e144 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e144;
    let _e145 = cascade;
    param_7 = min((_e145 + 1i), 3i);
    let _e148 = (*worldPos_1);
    param_8 = _e148;
    let _e149 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e149;
    let _e150 = s1_;
    let _e151 = s0_;
    let _e152 = blendT;
    return mix(_e150, _e151, _e152);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e94 = unnamed.packed_indices[1i][3u];
    let _e100 = unnamed.packed_indices[1i][3u];
    let _e105 = (*lm_uv);
    let _e106 = textureSample(wired_bindless_images[(_e94 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    sun_mask = _e106.x;
    let _e108 = sun_mask;
    if (_e108 > 0.001f) {
        let _e110 = shadowData_1;
        param_9 = _e110.xyz;
        let _e113 = shadowData_1[3u];
        param_10 = _e113;
        let _e114 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e115 = param_11;
        ignoredCascade = _e115;
        shadow_1 = _e114;
        let _e116 = shadow_1;
        let _e117 = sun_mask;
        let _e119 = (*rgb);
        (*rgb) = (_e119 * mix(1f, _e116, _e117));
    }
    let _e121 = (*rgb);
    return _e121;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;

    if override_type_3_2 {
        let _e86 = (*rgb_1);
        param_12 = _e86;
        let _e87 = frag_tex_coord0_1;
        param_13 = _e87;
        let _e88 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e88;
    }
    let _e89 = (*rgb_1);
    return _e89;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e86 = (*rgb_2);
    let _e89 = unnamed.worldLightParams[0u];
    boosted = (_e86 * _e89);
    let _e92 = boosted[0u];
    let _e94 = boosted[1u];
    let _e96 = boosted[2u];
    peak = max(_e92, max(_e94, _e96));
    let _e99 = peak;
    if (_e99 > 1f) {
        let _e101 = peak;
        let _e102 = boosted;
        boosted = (_e102 / vec3(_e101));
    }
    let _e105 = boosted;
    return _e105;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e87 = (*c_1);
    (*c_1) = max(_e87, vec3<f32>(0f, 0f, 0f));
    let _e89 = (*c_1);
    cutoff = (_e89 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e91 = (*c_1);
    lo = (_e91 / vec3(12.92f));
    let _e94 = (*c_1);
    hi = pow(((_e94 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e99 = hi;
    let _e100 = lo;
    let _e101 = cutoff;
    return mix(_e99, _e100, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e101));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_14: vec3<f32>;
    var param_15: vec3<f32>;

    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e111 = (*uv);
    let _e112 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    c_2 = _e112;
    let _e113 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e113))) == 0i) {
        let _e118 = c_2;
        param_14 = _e118.xyz;
        let _e120 = sRGBToLinear_u0028_vf3_u003b((&param_14));
        c_2[0u] = _e120.x;
        c_2[1u] = _e120.y;
        c_2[2u] = _e120.z;
    }
    let _e127 = (*slot);
    if (lightmap_slot == (_e127 + 1i)) {
        let _e130 = c_2;
        param_15 = _e130.xyz;
        let _e132 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_15));
        c_2[0u] = _e132.x;
        c_2[1u] = _e132.y;
        c_2[2u] = _e132.z;
    }
    let _e139 = c_2;
    return _e139;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_19: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_20: f32;
    var param_21: vec4<f32>;

    param_16 = 0u;
    let _e97 = frag_tex_coord0_1;
    param_17 = _e97;
    param_18 = 0i;
    let _e98 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor0_ = _e98;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e103 = sampledColor0_;
    let _e104 = frag_color;
    color0_ = (_e103 * _e104);
    let _e106 = color0_;
    base = _e106;
    let _e107 = color0_;
    base = _e107;
    let _e108 = base;
    param_19 = _e108.xyz;
    let _e110 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_19));
    base[0u] = _e110.x;
    base[1u] = _e110.y;
    base[2u] = _e110.z;
    if override_type_3_3 {
        let _e119 = unnamed.worldLightParams[1u];
        wetness = clamp(_e119, 0f, 1f);
        let _e123 = unnamed.worldLightParams[2u];
        frost = clamp(_e123, 0f, 1f);
        let _e125 = base;
        luminance = dot(_e125.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e128 = wetness;
        let _e130 = base;
        let _e132 = (_e130.xyz * mix(1f, 0.82f, _e128));
        base[0u] = _e132.x;
        base[1u] = _e132.y;
        base[2u] = _e132.z;
        let _e139 = base;
        let _e141 = luminance;
        let _e143 = luminance;
        let _e145 = luminance;
        let _e147 = frost;
        let _e150 = mix(_e139.xyz, vec3<f32>((_e141 * 0.88f), (_e143 * 0.94f), _e145), vec3((_e147 * 0.55f)));
        base[0u] = _e150.x;
        base[1u] = _e150.y;
        base[2u] = _e150.z;
    }
    let _e159 = unnamed.materialFactors[0u];
    if (_e159 >= 0f) {
        let _e162 = unnamed.materialBaseColor;
        let _e163 = base;
        base = (_e163 * _e162);
    }
    let _e165 = color0_;
    let _e168 = unnamed.emissionRadiance;
    let _e171 = base;
    let _e173 = (_e171.xyz + (_e165.xyz * _e168.xyz));
    base[0u] = _e173.x;
    base[1u] = _e173.y;
    base[2u] = _e173.z;
    let _e180 = wired_advanced_fog_enabled_u0028_();
    if _e180 {
        let _e181 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e181;
        let _e182 = base;
        let _e185 = unnamed.advancedFogColorDensity;
        let _e187 = fogAmount;
        let _e189 = mix(_e182.xyz, _e185.xyz, vec3(_e187));
        base[0u] = _e189.x;
        base[1u] = _e189.y;
        base[2u] = _e189.z;
    }
    if override_type_3_4 {
        let _e197 = base[3u];
        if (_e197 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e199 = base;
            let _e201 = base;
            if (dot(_e199.xyz, _e201.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e205 = base;
    out_color = _e205;
    param_20 = 1f;
    let _e206 = base;
    param_21 = _e206;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_20), (&param_21));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
