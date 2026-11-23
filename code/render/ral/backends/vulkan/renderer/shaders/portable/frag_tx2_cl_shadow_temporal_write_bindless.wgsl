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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_14: bool = (discard_mode == 1i);
override override_type_3_15: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e100 = (*value);
    let _e101 = (*value);
    let _e103 = all((_e100 == _e101));
    phi_97_ = _e103;
    if _e103 {
        let _e104 = (*value);
        phi_97_ = all((abs(_e104) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e109 = phi_97_;
    return _e109;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_82_: bool;

    let _e100 = (*value_1);
    let _e101 = (*value_1);
    let _e103 = all((_e100 == _e101));
    phi_82_ = _e103;
    if _e103 {
        let _e104 = (*value_1);
        phi_82_ = all((abs(_e104) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e109 = phi_82_;
    return _e109;
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
    let _e110 = temporalOutcome_1;
    let _e111 = (_e110 != 1u);
    phi_120_ = _e111;
    if !(_e111) {
        let _e113 = temporalCurrentClip_1;
        param = _e113;
        let _e114 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_120_ = !(_e114);
    }
    let _e117 = phi_120_;
    phi_129_ = _e117;
    if !(_e117) {
        let _e119 = temporalPreviousClip_1;
        param_1 = _e119;
        let _e120 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_129_ = !(_e120);
    }
    let _e123 = phi_129_;
    phi_139_ = _e123;
    if !(_e123) {
        let _e126 = temporalCurrentClip_1[3u];
        phi_139_ = (_e126 <= 0.000001f);
    }
    let _e129 = phi_139_;
    phi_146_ = _e129;
    if !(_e129) {
        let _e132 = temporalPreviousClip_1[3u];
        phi_146_ = (_e132 <= 0.000001f);
    }
    let _e135 = phi_146_;
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
    phi_175_ = _e150;
    if !(_e150) {
        let _e152 = previousNdc;
        param_3 = _e152;
        let _e153 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_175_ = !(_e153);
    }
    let _e156 = phi_175_;
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
    out_temporal_validity = clamp(_e170, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e102 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e102 + 0.5f));
    let _e107 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e109 = fogType;
    let _e112 = fogType;
    return (((_e107 > 0.5f) && (_e109 >= 1i)) && (_e112 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e102 = wired_advanced_fog_enabled_u0028_();
    if !(_e102) {
        return 0f;
    }
    let _e105 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e105, 0.000001f));
    let _e110 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e110 + 0.5f));
    let _e113 = fogType_1;
    if (_e113 == 1i) {
        let _e117 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e117 <= 0f) {
            return 0f;
        }
        let _e119 = viewDepth;
        let _e122 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e119 / _e122), 0f, 1f);
    }
    let _e127 = unnamed.advancedFogColorDensity[3u];
    let _e129 = viewDepth;
    opticalDepth = (max(_e127, 0f) * _e129);
    let _e131 = fogType_1;
    if (_e131 == 2i) {
        let _e133 = opticalDepth;
        return clamp((1f - exp(-(_e133))), 0f, 1f);
    }
    let _e138 = opticalDepth;
    let _e139 = opticalDepth;
    return clamp((1f - exp(-((_e138 * _e139)))), 0f, 1f);
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

    let _e109 = (*c);
    let _e112 = unnamed.cascadeMVP[_e109];
    let _e113 = (*worldPos);
    sc4_ = (_e112 * vec4<f32>(_e113.x, _e113.y, _e113.z, 1f));
    let _e119 = sc4_;
    let _e122 = sc4_[3u];
    sc = (_e119.xyz / vec3(_e122));
    let _e125 = sc;
    let _e129 = ((_e125.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e129.x;
    sc[1u] = _e129.y;
    let _e135 = sc[0u];
    let _e136 = (_e135 < 0f);
    phi_470_ = _e136;
    if !(_e136) {
        let _e139 = sc[0u];
        phi_470_ = (_e139 > 1f);
    }
    let _e142 = phi_470_;
    phi_477_ = _e142;
    if !(_e142) {
        let _e145 = sc[1u];
        phi_477_ = (_e145 < 0f);
    }
    let _e148 = phi_477_;
    phi_484_ = _e148;
    if !(_e148) {
        let _e151 = sc[1u];
        phi_484_ = (_e151 > 1f);
    }
    let _e154 = phi_484_;
    phi_491_ = _e154;
    if !(_e154) {
        let _e157 = sc[2u];
        phi_491_ = (_e157 > 1f);
    }
    let _e160 = phi_491_;
    if _e160 {
        return 1f;
    }
    let _e161 = (*c);
    layer = f32(_e161);
    let _e163 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e163).xy));
    let _e170 = sc[2u];
    currentDepth = (_e170 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e172 = currentDepth;
        let _e173 = sc;
        let _e174 = _e173.xy;
        let _e175 = layer;
        let _e178 = vec3<f32>(_e174.x, _e174.y, _e175);
        let _e184 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e178.x, _e178.y), i32(_e178.z));
        shadow = step(_e172, _e184.x);
    } else {
        if override_type_3_1 {
            let _e187 = currentDepth;
            let _e188 = sc;
            let _e189 = _e188.xy;
            let _e190 = layer;
            let _e193 = vec3<f32>(_e189.x, _e189.y, _e190);
            let _e199 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e193.x, _e193.y), i32(_e193.z));
            let _e202 = shadow;
            shadow = (_e202 + step(_e187, _e199.x));
            let _e204 = currentDepth;
            let _e205 = sc;
            let _e208 = texelSize[0u];
            let _e210 = (_e205.xy + vec2<f32>(_e208, 0f));
            let _e211 = layer;
            let _e214 = vec3<f32>(_e210.x, _e210.y, _e211);
            let _e220 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e214.x, _e214.y), i32(_e214.z));
            let _e223 = shadow;
            shadow = (_e223 + step(_e204, _e220.x));
            let _e225 = currentDepth;
            let _e226 = sc;
            let _e229 = texelSize[0u];
            let _e231 = (_e226.xy - vec2<f32>(_e229, 0f));
            let _e232 = layer;
            let _e235 = vec3<f32>(_e231.x, _e231.y, _e232);
            let _e241 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e235.x, _e235.y), i32(_e235.z));
            let _e244 = shadow;
            shadow = (_e244 + step(_e225, _e241.x));
            let _e246 = currentDepth;
            let _e247 = sc;
            let _e250 = texelSize[1u];
            let _e252 = (_e247.xy + vec2<f32>(0f, _e250));
            let _e253 = layer;
            let _e256 = vec3<f32>(_e252.x, _e252.y, _e253);
            let _e262 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e256.x, _e256.y), i32(_e256.z));
            let _e265 = shadow;
            shadow = (_e265 + step(_e246, _e262.x));
            let _e267 = currentDepth;
            let _e268 = sc;
            let _e271 = texelSize[1u];
            let _e273 = (_e268.xy - vec2<f32>(0f, _e271));
            let _e274 = layer;
            let _e277 = vec3<f32>(_e273.x, _e273.y, _e274);
            let _e283 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e277.x, _e277.y), i32(_e277.z));
            let _e286 = shadow;
            shadow = (_e286 + step(_e267, _e283.x));
            let _e288 = shadow;
            shadow = (_e288 / 5f);
        } else {
            x = -1i;
            loop {
                let _e290 = x;
                if (_e290 <= 1i) {
                    y = -1i;
                    loop {
                        let _e292 = y;
                        if (_e292 <= 1i) {
                            let _e294 = currentDepth;
                            let _e295 = sc;
                            let _e297 = x;
                            let _e299 = y;
                            let _e302 = texelSize;
                            let _e304 = (_e295.xy + (vec2<f32>(f32(_e297), f32(_e299)) * _e302));
                            let _e305 = layer;
                            let _e308 = vec3<f32>(_e304.x, _e304.y, _e305);
                            let _e314 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e308.x, _e308.y), i32(_e308.z));
                            let _e317 = shadow;
                            shadow = (_e317 + step(_e294, _e314.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e319 = y;
                            y = (_e319 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e321 = x;
                    x = (_e321 + 1i);
                }
            }
            let _e323 = shadow;
            shadow = (_e323 / 9f);
        }
    }
    let _e325 = shadow;
    return _e325;
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

    let _e116 = unnamed.cascadeSplits;
    let _e117 = (*viewDepth_1);
    cmp = step(_e116, vec4(_e117));
    let _e121 = cmp[0u];
    let _e123 = cmp[1u];
    let _e126 = cmp[2u];
    let _e129 = cmp[3u];
    cascade = min(i32((((_e121 + _e123) + _e126) + _e129)), 3i);
    let _e133 = cascade;
    (*outCascade) = _e133;
    let _e134 = cascade;
    if (_e134 == 0i) {
        local = 0f;
    } else {
        let _e136 = cascade;
        let _e141 = unnamed.cascadeSplits[max((_e136 - 1i), 0i)];
        local = _e141;
    }
    let _e142 = local;
    prevSplit = _e142;
    let _e143 = cascade;
    let _e146 = unnamed.cascadeSplits[_e143];
    farSplit = _e146;
    let _e147 = farSplit;
    let _e148 = prevSplit;
    blendRange = max((0.1f * (_e147 - _e148)), 1f);
    let _e152 = farSplit;
    let _e153 = (*viewDepth_1);
    let _e155 = blendRange;
    blendT = clamp(((_e152 - _e153) / _e155), 0f, 1f);
    let _e158 = cascade;
    param_5 = _e158;
    let _e159 = (*worldPos_1);
    param_6 = _e159;
    let _e160 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_5), (&param_6));
    s0_ = _e160;
    let _e161 = cascade;
    param_7 = min((_e161 + 1i), 3i);
    let _e164 = (*worldPos_1);
    param_8 = _e164;
    let _e165 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_7), (&param_8));
    s1_ = _e165;
    let _e166 = s1_;
    let _e167 = s0_;
    let _e168 = blendT;
    return mix(_e166, _e167, _e168);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_9: vec3<f32>;
    var param_10: f32;
    var param_11: i32;

    let _e110 = unnamed.packed_indices[1i][3u];
    let _e116 = unnamed.packed_indices[1i][3u];
    let _e121 = (*lm_uv);
    let _e122 = textureSample(wired_bindless_images[(_e110 & 4095u)], wired_bindless_samplers[((_e116 >> bitcast<u32>(12i)) & 255u)], _e121);
    sun_mask = _e122.x;
    let _e124 = sun_mask;
    if (_e124 > 0.001f) {
        let _e126 = shadowData_1;
        param_9 = _e126.xyz;
        let _e129 = shadowData_1[3u];
        param_10 = _e129;
        let _e130 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        let _e131 = param_11;
        ignoredCascade = _e131;
        shadow_1 = _e130;
        let _e132 = shadow_1;
        let _e133 = sun_mask;
        let _e135 = (*rgb);
        (*rgb) = (_e135 * mix(1f, _e132, _e133));
    }
    let _e137 = (*rgb);
    return _e137;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_12: vec3<f32>;
    var param_13: vec2<f32>;
    var param_14: vec3<f32>;
    var param_15: vec2<f32>;
    var param_16: vec3<f32>;
    var param_17: vec2<f32>;

    if override_type_3_2 {
        let _e106 = (*rgb_1);
        param_12 = _e106;
        let _e107 = frag_tex_coord0_1;
        param_13 = _e107;
        let _e108 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_12), (&param_13));
        return _e108;
    }
    if override_type_3_3 {
        let _e109 = (*rgb_1);
        param_14 = _e109;
        let _e110 = frag_tex_coord1_1;
        param_15 = _e110;
        let _e111 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_14), (&param_15));
        return _e111;
    }
    if override_type_3_4 {
        let _e112 = (*rgb_1);
        param_16 = _e112;
        let _e113 = frag_tex_coord2_1;
        param_17 = _e113;
        let _e114 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_16), (&param_17));
        return _e114;
    }
    let _e115 = (*rgb_1);
    return _e115;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e102 = (*rgb_2);
    let _e105 = unnamed.worldLightParams[0u];
    boosted = (_e102 * _e105);
    let _e108 = boosted[0u];
    let _e110 = boosted[1u];
    let _e112 = boosted[2u];
    peak = max(_e108, max(_e110, _e112));
    let _e115 = peak;
    if (_e115 > 1f) {
        let _e117 = peak;
        let _e118 = boosted;
        boosted = (_e118 / vec3(_e117));
    }
    let _e121 = boosted;
    return _e121;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e103 = (*c_1);
    (*c_1) = max(_e103, vec3<f32>(0f, 0f, 0f));
    let _e105 = (*c_1);
    cutoff = (_e105 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e107 = (*c_1);
    lo = (_e107 / vec3(12.92f));
    let _e110 = (*c_1);
    hi = pow(((_e110 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e115 = hi;
    let _e116 = lo;
    let _e117 = cutoff;
    return mix(_e115, _e116, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e117));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_18: vec3<f32>;
    var param_19: vec3<f32>;

    let _e105 = (*role);
    let _e107 = (*role);
    let _e112 = unnamed.packed_indices[(_e105 / 4u)][(_e107 % 4u)];
    let _e115 = (*role);
    let _e117 = (*role);
    let _e122 = unnamed.packed_indices[(_e115 / 4u)][(_e117 % 4u)];
    let _e127 = (*uv);
    let _e128 = textureSample(wired_bindless_images[(_e112 & 4095u)], wired_bindless_samplers[((_e122 >> bitcast<u32>(12i)) & 255u)], _e127);
    c_2 = _e128;
    let _e129 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e129))) == 0i) {
        let _e134 = c_2;
        param_18 = _e134.xyz;
        let _e136 = sRGBToLinear_u0028_vf3_u003b((&param_18));
        c_2[0u] = _e136.x;
        c_2[1u] = _e136.y;
        c_2[2u] = _e136.z;
    }
    let _e143 = (*slot);
    if (lightmap_slot == (_e143 + 1i)) {
        let _e146 = c_2;
        param_19 = _e146.xyz;
        let _e148 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_19));
        c_2[0u] = _e148.x;
        c_2[1u] = _e148.y;
        c_2[2u] = _e148.z;
    }
    let _e155 = c_2;
    return _e155;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_20: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_21: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_22: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var sampledColor1_: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var sampledColor2_: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
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
    var param_32: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_33: f32;

    let _e139 = frag_color0In_1;
    param_20 = _e139.xyz;
    let _e141 = sRGBToLinear_u0028_vf3_u003b((&param_20));
    let _e143 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e141.x, _e141.y, _e141.z, _e143);
    let _e148 = frag_color1In_1;
    param_21 = _e148.xyz;
    let _e150 = sRGBToLinear_u0028_vf3_u003b((&param_21));
    let _e152 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e150.x, _e150.y, _e150.z, _e152);
    let _e157 = frag_color2In_1;
    param_22 = _e157.xyz;
    let _e159 = sRGBToLinear_u0028_vf3_u003b((&param_22));
    let _e161 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e159.x, _e159.y, _e159.z, _e161);
    param_23 = 0u;
    let _e166 = frag_tex_coord0_1;
    param_24 = _e166;
    param_25 = 0i;
    let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
    sampledColor0_ = _e167;
    param_26 = 1u;
    let _e168 = frag_tex_coord1_1;
    param_27 = _e168;
    param_28 = 1i;
    let _e169 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
    sampledColor1_ = _e169;
    param_29 = 2u;
    let _e170 = frag_tex_coord2_1;
    param_30 = _e170;
    param_31 = 2i;
    let _e171 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
    sampledColor2_ = _e171;
    let _e172 = sampledColor0_;
    let _e173 = frag_color0_;
    color0_ = (_e172 * _e173);
    if override_type_3_7 {
        let _e175 = sampledColor1_;
        let _e176 = frag_color1_;
        color1_ = (_e175 * _e176);
        let _e178 = sampledColor2_;
        let _e179 = frag_color2_;
        color2_ = (_e178 * _e179);
        let _e181 = color0_;
        let _e183 = color1_;
        let _e186 = color2_;
        let _e188 = ((_e181.xyz + _e183.xyz) + _e186.xyz);
        let _e190 = color0_[3u];
        let _e192 = color1_[3u];
        let _e195 = color2_[3u];
        base = vec4<f32>(_e188.x, _e188.y, _e188.z, ((_e190 * _e192) * _e195));
    } else {
        if override_type_3_8 {
            let _e201 = sampledColor1_;
            let _e202 = frag_color1_;
            color1_1 = (_e201 * _e202);
            let _e204 = sampledColor2_;
            let _e205 = frag_color2_;
            color2_1 = (_e204 * _e205);
            let _e208 = color0_[3u];
            let _e209 = color0_;
            color0_ = (_e209 * _e208);
            let _e212 = color1_1[3u];
            let _e213 = color1_1;
            color1_1 = (_e213 * _e212);
            let _e216 = color2_1[3u];
            let _e217 = color2_1;
            color2_1 = (_e217 * _e216);
            let _e219 = color0_;
            let _e221 = color1_1;
            let _e224 = color2_1;
            let _e226 = ((_e219.xyz + _e221.xyz) + _e224.xyz);
            let _e228 = color0_[3u];
            let _e230 = color1_1[3u];
            let _e233 = color2_1[3u];
            base = vec4<f32>(_e226.x, _e226.y, _e226.z, ((_e228 * _e230) * _e233));
        } else {
            if override_type_3_9 {
                let _e239 = sampledColor1_;
                let _e240 = frag_color1_;
                color1_2 = (_e239 * _e240);
                let _e242 = sampledColor2_;
                let _e243 = frag_color2_;
                color2_2 = (_e242 * _e243);
                let _e246 = color0_[3u];
                let _e248 = color0_;
                color0_ = (_e248 * (1f - _e246));
                let _e251 = color1_2[3u];
                let _e253 = color1_2;
                color1_2 = (_e253 * (1f - _e251));
                let _e256 = color2_2[3u];
                let _e258 = color2_2;
                color2_2 = (_e258 * (1f - _e256));
                let _e260 = color0_;
                let _e262 = color1_2;
                let _e265 = color2_2;
                let _e267 = ((_e260.xyz + _e262.xyz) + _e265.xyz);
                let _e269 = color0_[3u];
                let _e271 = color1_2[3u];
                let _e274 = color2_2[3u];
                base = vec4<f32>(_e267.x, _e267.y, _e267.z, ((_e269 * _e271) * _e274));
            } else {
                if override_type_3_10 {
                    let _e280 = sampledColor1_;
                    let _e281 = frag_color1_;
                    color1_3 = (_e280 * _e281);
                    let _e283 = sampledColor2_;
                    let _e284 = frag_color2_;
                    color2_3 = (_e283 * _e284);
                    let _e286 = color0_;
                    let _e287 = color1_3;
                    let _e289 = color1_3[3u];
                    let _e292 = color2_3;
                    let _e294 = color2_3[3u];
                    base = mix(mix(_e286, _e287, vec4(_e289)), _e292, vec4(_e294));
                } else {
                    if override_type_3_11 {
                        let _e297 = sampledColor1_;
                        let _e298 = frag_color1_;
                        color1_4 = (_e297 * _e298);
                        let _e300 = sampledColor2_;
                        let _e301 = frag_color2_;
                        color2_4 = (_e300 * _e301);
                        let _e303 = color2_4;
                        let _e304 = color1_4;
                        let _e305 = color0_;
                        let _e307 = color1_4[3u];
                        let _e311 = color2_4[3u];
                        base = mix(_e303, mix(_e304, _e305, vec4(_e307)), vec4(_e311));
                    } else {
                        if override_type_3_12 {
                            let _e314 = sampledColor1_;
                            let _e315 = frag_color1_;
                            color1_5 = (_e314 * _e315);
                            let _e317 = sampledColor2_;
                            let _e318 = frag_color2_;
                            color2_5 = (_e317 * _e318);
                            let _e320 = color2_5;
                            let _e322 = color2_5[3u];
                            let _e325 = color1_5;
                            let _e327 = color1_5[3u];
                            let _e331 = color0_;
                            base = (((_e320 + vec4(_e322)) * (_e325 + vec4(_e327))) * _e331);
                        } else {
                            let _e333 = sampledColor1_;
                            let _e334 = frag_color1_;
                            color1_6 = (_e333 * _e334);
                            let _e336 = sampledColor2_;
                            let _e337 = frag_color2_;
                            color2_6 = (_e336 * _e337);
                            let _e339 = color0_;
                            let _e341 = color1_6;
                            let _e344 = color2_6;
                            let _e346 = ((_e339.xyz * _e341.xyz) * _e344.xyz);
                            base[0u] = _e346.x;
                            base[1u] = _e346.y;
                            base[2u] = _e346.z;
                            let _e354 = color0_[3u];
                            let _e356 = color1_6[3u];
                            let _e359 = color2_6[3u];
                            base[3u] = ((_e354 * _e356) * _e359);
                        }
                    }
                }
            }
        }
    }
    let _e362 = base;
    param_32 = _e362.xyz;
    let _e364 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_32));
    base[0u] = _e364.x;
    base[1u] = _e364.y;
    base[2u] = _e364.z;
    if override_type_3_13 {
        let _e373 = unnamed.worldLightParams[1u];
        wetness = clamp(_e373, 0f, 1f);
        let _e377 = unnamed.worldLightParams[2u];
        frost = clamp(_e377, 0f, 1f);
        let _e379 = base;
        luminance = dot(_e379.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e382 = wetness;
        let _e384 = base;
        let _e386 = (_e384.xyz * mix(1f, 0.82f, _e382));
        base[0u] = _e386.x;
        base[1u] = _e386.y;
        base[2u] = _e386.z;
        let _e393 = base;
        let _e395 = luminance;
        let _e397 = luminance;
        let _e399 = luminance;
        let _e401 = frost;
        let _e404 = mix(_e393.xyz, vec3<f32>((_e395 * 0.88f), (_e397 * 0.94f), _e399), vec3((_e401 * 0.55f)));
        base[0u] = _e404.x;
        base[1u] = _e404.y;
        base[2u] = _e404.z;
    }
    let _e413 = unnamed.materialFactors[0u];
    if (_e413 >= 0f) {
        let _e416 = unnamed.materialBaseColor;
        let _e417 = base;
        base = (_e417 * _e416);
    }
    let _e419 = color0_;
    let _e422 = unnamed.emissionRadiance;
    let _e425 = base;
    let _e427 = (_e425.xyz + (_e419.xyz * _e422.xyz));
    base[0u] = _e427.x;
    base[1u] = _e427.y;
    base[2u] = _e427.z;
    let _e434 = wired_advanced_fog_enabled_u0028_();
    if _e434 {
        let _e435 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e435;
        let _e436 = base;
        let _e439 = unnamed.advancedFogColorDensity;
        let _e441 = fogAmount;
        let _e443 = mix(_e436.xyz, _e439.xyz, vec3(_e441));
        base[0u] = _e443.x;
        base[1u] = _e443.y;
        base[2u] = _e443.z;
    }
    if override_type_3_14 {
        let _e451 = base[3u];
        if (_e451 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e453 = base;
            let _e455 = base;
            if (dot(_e453.xyz, _e455.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e459 = base;
    out_color = _e459;
    param_33 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_33));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    main_1();
    let _e25 = out_temporal_velocity;
    let _e26 = out_temporal_validity;
    let _e27 = out_color;
    return FragmentOutput(_e25, _e26, _e27);
}
