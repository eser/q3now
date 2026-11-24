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
    _pad_to_packed_indices: array<vec4<f32>, 26>,
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
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
override override_type_3_8: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_15: bool = (discard_mode == 1i);
override override_type_3_16: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e92 = (*value);
    let _e93 = (*value);
    let _e95 = all((_e92 == _e93));
    phi_79_ = _e95;
    if _e95 {
        let _e96 = (*value);
        phi_79_ = all((abs(_e96) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_79_;
    return _e101;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e92 = (*value_1);
    let _e93 = (*value_1);
    let _e95 = all((_e92 == _e93));
    phi_64_ = _e95;
    if _e95 {
        let _e96 = (*value_1);
        phi_64_ = all((abs(_e96) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_64_;
    return _e101;
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
    var phi_103_: bool;
    var phi_112_: bool;
    var phi_122_: bool;
    var phi_129_: bool;
    var phi_158_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e103 = temporalOutcome_1;
    let _e104 = (_e103 != 1u);
    phi_103_ = _e104;
    if !(_e104) {
        let _e106 = temporalCurrentClip_1;
        param = _e106;
        let _e107 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e107);
    }
    let _e110 = phi_103_;
    phi_112_ = _e110;
    if !(_e110) {
        let _e112 = temporalPreviousClip_1;
        param_1 = _e112;
        let _e113 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e113);
    }
    let _e116 = phi_112_;
    phi_122_ = _e116;
    if !(_e116) {
        let _e119 = temporalCurrentClip_1[3u];
        phi_122_ = (_e119 <= 0.000001f);
    }
    let _e122 = phi_122_;
    phi_129_ = _e122;
    if !(_e122) {
        let _e125 = temporalPreviousClip_1[3u];
        phi_129_ = (_e125 <= 0.000001f);
    }
    let _e128 = phi_129_;
    if _e128 {
        return;
    }
    let _e129 = temporalCurrentClip_1;
    let _e132 = temporalCurrentClip_1[3u];
    currentNdc = (_e129.xy / vec2(_e132));
    let _e135 = temporalPreviousClip_1;
    let _e138 = temporalPreviousClip_1[3u];
    previousNdc = (_e135.xy / vec2(_e138));
    let _e141 = currentNdc;
    param_2 = _e141;
    let _e142 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e143 = !(_e142);
    phi_158_ = _e143;
    if !(_e143) {
        let _e145 = previousNdc;
        param_3 = _e145;
        let _e146 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e146);
    }
    let _e149 = phi_158_;
    if _e149 {
        return;
    }
    let _e150 = currentNdc;
    currentUv = ((_e150 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e153 = previousNdc;
    previousUv = ((_e153 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e156 = currentUv;
    let _e157 = previousUv;
    velocity = (_e156 - _e157);
    let _e159 = velocity;
    param_4 = _e159;
    let _e160 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e160) {
        return;
    }
    let _e162 = velocity;
    out_temporal_velocity = _e162;
    let _e163 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e163, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e94 + 0.5f));
    let _e99 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e101 = fogType;
    let _e104 = fogType;
    return (((_e99 > 0.5f) && (_e101 >= 1i)) && (_e104 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e94 = wired_advanced_fog_enabled_u0028_();
    if !(_e94) {
        return 0f;
    }
    let _e97 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e97, 0.000001f));
    let _e102 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e102 + 0.5f));
    let _e105 = fogType_1;
    if (_e105 == 1i) {
        let _e109 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e109 <= 0f) {
            return 0f;
        }
        let _e111 = viewDepth;
        let _e114 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e111 / _e114), 0f, 1f);
    }
    let _e119 = unnamed.advancedFogColorDensity[3u];
    let _e121 = viewDepth;
    opticalDepth = (max(_e119, 0f) * _e121);
    let _e123 = fogType_1;
    if (_e123 == 2i) {
        let _e125 = opticalDepth;
        return clamp((1f - exp(-(_e125))), 0f, 1f);
    }
    let _e130 = opticalDepth;
    let _e131 = opticalDepth;
    return clamp((1f - exp(-((_e130 * _e131)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e94 = (*rgb);
    let _e97 = unnamed.worldLightParams[0u];
    boosted = (_e94 * _e97);
    let _e100 = boosted[0u];
    let _e102 = boosted[1u];
    let _e104 = boosted[2u];
    peak = max(_e100, max(_e102, _e104));
    let _e107 = peak;
    if (_e107 > 1f) {
        let _e109 = peak;
        let _e110 = boosted;
        boosted = (_e110 / vec3(_e109));
    }
    let _e113 = boosted;
    return _e113;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e95 = (*c);
    (*c) = max(_e95, vec3<f32>(0f, 0f, 0f));
    let _e97 = (*c);
    cutoff = (_e97 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e99 = (*c);
    lo = (_e99 / vec3(12.92f));
    let _e102 = (*c);
    hi = pow(((_e102 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e107 = hi;
    let _e108 = lo;
    let _e109 = cutoff;
    return mix(_e107, _e108, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e109));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e107 = (*role);
    let _e109 = (*role);
    let _e114 = unnamed.packed_indices[(_e107 / 4u)][(_e109 % 4u)];
    let _e119 = (*uv);
    let _e120 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    c_1 = _e120;
    let _e121 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e121))) == 0i) {
        let _e126 = c_1;
        param_5 = _e126.xyz;
        let _e128 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = (*slot);
    if (lightmap_slot == (_e135 + 1i)) {
        let _e138 = c_1;
        param_6 = _e138.xyz;
        let _e140 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e140.x;
        c_1[1u] = _e140.y;
        c_1[2u] = _e140.z;
    }
    let _e147 = c_1;
    return _e147;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_9: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var sampledColor1_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var sampledColor2_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
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
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_19: f32;
    var param_20: vec4<f32>;

    let _e135 = unnamed.packed_indices[0i][3u];
    let _e141 = unnamed.packed_indices[0i][3u];
    let _e146 = fog_tex_coord_1;
    let _e147 = textureSample(wired_bindless_images[(_e135 & 4095u)], wired_bindless_samplers[((_e141 >> bitcast<u32>(12i)) & 255u)], _e146);
    fog = _e147;
    let _e148 = frag_color0In_1;
    param_7 = _e148.xyz;
    let _e150 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e152 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e150.x, _e150.y, _e150.z, _e152);
    let _e157 = frag_color1In_1;
    param_8 = _e157.xyz;
    let _e159 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e161 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e159.x, _e159.y, _e159.z, _e161);
    let _e166 = frag_color2In_1;
    param_9 = _e166.xyz;
    let _e168 = sRGBToLinear_u0028_vf3_u003b((&param_9));
    let _e170 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e168.x, _e168.y, _e168.z, _e170);
    param_10 = 0u;
    let _e175 = frag_tex_coord0_1;
    param_11 = _e175;
    param_12 = 0i;
    let _e176 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor0_ = _e176;
    param_13 = 1u;
    let _e177 = frag_tex_coord1_1;
    param_14 = _e177;
    param_15 = 1i;
    let _e178 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    sampledColor1_ = _e178;
    param_16 = 2u;
    let _e179 = frag_tex_coord2_1;
    param_17 = _e179;
    param_18 = 2i;
    let _e180 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor2_ = _e180;
    let _e181 = sampledColor0_;
    let _e182 = frag_color0_;
    color0_ = (_e181 * _e182);
    if override_type_3_2 {
        let _e184 = sampledColor1_;
        let _e185 = frag_color1_;
        color1_ = (_e184 * _e185);
        let _e187 = sampledColor2_;
        let _e188 = frag_color2_;
        color2_ = (_e187 * _e188);
        let _e190 = color0_;
        let _e192 = color1_;
        let _e195 = color2_;
        let _e197 = ((_e190.xyz + _e192.xyz) + _e195.xyz);
        let _e199 = color0_[3u];
        let _e201 = color1_[3u];
        let _e204 = color2_[3u];
        base = vec4<f32>(_e197.x, _e197.y, _e197.z, ((_e199 * _e201) * _e204));
    } else {
        if override_type_3_3 {
            let _e210 = sampledColor1_;
            let _e211 = frag_color1_;
            color1_1 = (_e210 * _e211);
            let _e213 = sampledColor2_;
            let _e214 = frag_color2_;
            color2_1 = (_e213 * _e214);
            let _e217 = color0_[3u];
            let _e218 = color0_;
            color0_ = (_e218 * _e217);
            let _e221 = color1_1[3u];
            let _e222 = color1_1;
            color1_1 = (_e222 * _e221);
            let _e225 = color2_1[3u];
            let _e226 = color2_1;
            color2_1 = (_e226 * _e225);
            let _e228 = color0_;
            let _e230 = color1_1;
            let _e233 = color2_1;
            let _e235 = ((_e228.xyz + _e230.xyz) + _e233.xyz);
            let _e237 = color0_[3u];
            let _e239 = color1_1[3u];
            let _e242 = color2_1[3u];
            base = vec4<f32>(_e235.x, _e235.y, _e235.z, ((_e237 * _e239) * _e242));
        } else {
            if override_type_3_4 {
                let _e248 = sampledColor1_;
                let _e249 = frag_color1_;
                color1_2 = (_e248 * _e249);
                let _e251 = sampledColor2_;
                let _e252 = frag_color2_;
                color2_2 = (_e251 * _e252);
                let _e255 = color0_[3u];
                let _e257 = color0_;
                color0_ = (_e257 * (1f - _e255));
                let _e260 = color1_2[3u];
                let _e262 = color1_2;
                color1_2 = (_e262 * (1f - _e260));
                let _e265 = color2_2[3u];
                let _e267 = color2_2;
                color2_2 = (_e267 * (1f - _e265));
                let _e269 = color0_;
                let _e271 = color1_2;
                let _e274 = color2_2;
                let _e276 = ((_e269.xyz + _e271.xyz) + _e274.xyz);
                let _e278 = color0_[3u];
                let _e280 = color1_2[3u];
                let _e283 = color2_2[3u];
                base = vec4<f32>(_e276.x, _e276.y, _e276.z, ((_e278 * _e280) * _e283));
            } else {
                if override_type_3_5 {
                    let _e289 = sampledColor1_;
                    let _e290 = frag_color1_;
                    color1_3 = (_e289 * _e290);
                    let _e292 = sampledColor2_;
                    let _e293 = frag_color2_;
                    color2_3 = (_e292 * _e293);
                    let _e295 = color0_;
                    let _e296 = color1_3;
                    let _e298 = color1_3[3u];
                    let _e301 = color2_3;
                    let _e303 = color2_3[3u];
                    base = mix(mix(_e295, _e296, vec4(_e298)), _e301, vec4(_e303));
                } else {
                    if override_type_3_6 {
                        let _e306 = sampledColor1_;
                        let _e307 = frag_color1_;
                        color1_4 = (_e306 * _e307);
                        let _e309 = sampledColor2_;
                        let _e310 = frag_color2_;
                        color2_4 = (_e309 * _e310);
                        let _e312 = color2_4;
                        let _e313 = color1_4;
                        let _e314 = color0_;
                        let _e316 = color1_4[3u];
                        let _e320 = color2_4[3u];
                        base = mix(_e312, mix(_e313, _e314, vec4(_e316)), vec4(_e320));
                    } else {
                        if override_type_3_7 {
                            let _e323 = sampledColor1_;
                            let _e324 = frag_color1_;
                            color1_5 = (_e323 * _e324);
                            let _e326 = sampledColor2_;
                            let _e327 = frag_color2_;
                            color2_5 = (_e326 * _e327);
                            let _e329 = color2_5;
                            let _e331 = color2_5[3u];
                            let _e334 = color1_5;
                            let _e336 = color1_5[3u];
                            let _e340 = color0_;
                            base = (((_e329 + vec4(_e331)) * (_e334 + vec4(_e336))) * _e340);
                        } else {
                            let _e342 = sampledColor1_;
                            let _e343 = frag_color1_;
                            color1_6 = (_e342 * _e343);
                            let _e345 = sampledColor2_;
                            let _e346 = frag_color2_;
                            color2_6 = (_e345 * _e346);
                            let _e348 = color0_;
                            let _e350 = color1_6;
                            let _e353 = color2_6;
                            let _e355 = ((_e348.xyz * _e350.xyz) * _e353.xyz);
                            base[0u] = _e355.x;
                            base[1u] = _e355.y;
                            base[2u] = _e355.z;
                            let _e363 = color0_[3u];
                            let _e365 = color1_6[3u];
                            let _e368 = color2_6[3u];
                            base[3u] = ((_e363 * _e365) * _e368);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
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
        if override_type_3_9 {
            let _e436 = fogAmount;
            let _e438 = base;
            let _e440 = (_e438.xyz * (1f - _e436));
            base[0u] = _e440.x;
            base[1u] = _e440.y;
            base[2u] = _e440.z;
        } else {
            if override_type_3_10 {
                let _e447 = fogAmount;
                let _e449 = base;
                base = (_e449 * (1f - _e447));
            } else {
                if override_type_3_11 {
                    let _e451 = fogAmount;
                    let _e454 = base[3u];
                    base[3u] = (_e454 * (1f - _e451));
                } else {
                    let _e457 = base;
                    let _e460 = unnamed.advancedFogColorDensity;
                    let _e462 = fogAmount;
                    let _e464 = mix(_e457.xyz, _e460.xyz, vec3(_e462));
                    base[0u] = _e464.x;
                    base[1u] = _e464.y;
                    base[2u] = _e464.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e471 = base;
            let _e474 = fog[3u];
            let _e476 = (_e471.xyz * (1f - _e474));
            base[0u] = _e476.x;
            base[1u] = _e476.y;
            base[2u] = _e476.z;
        } else {
            if override_type_3_13 {
                let _e483 = base;
                let _e485 = fog[3u];
                base = (_e483 * (1f - _e485));
            } else {
                if override_type_3_14 {
                    let _e489 = base[3u];
                    let _e491 = fog[3u];
                    base[3u] = (_e489 * (1f - _e491));
                } else {
                    let _e495 = base;
                    let _e496 = fog;
                    let _e498 = unnamed.fogColor;
                    let _e501 = fog[3u];
                    base = mix(_e495, (_e496 * _e498), vec4(_e501));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e505 = base[3u];
        if (_e505 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e507 = base;
            let _e509 = base;
            if (dot(_e507.xyz, _e509.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e513 = base;
    out_color = _e513;
    param_19 = 1f;
    let _e514 = base;
    param_20 = _e514;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_19), (&param_20));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e25 = out_temporal_velocity;
    let _e26 = out_temporal_validity;
    let _e27 = out_color;
    return FragmentOutput(_e25, _e26, _e27);
}
