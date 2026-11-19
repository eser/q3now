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
    @location(2) member_1: f32,
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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e83 = (*value);
    let _e84 = (*value);
    let _e86 = all((_e83 == _e84));
    phi_78_ = _e86;
    if _e86 {
        let _e87 = (*value);
        phi_78_ = all((abs(_e87) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e92 = phi_78_;
    return _e92;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e83 = (*value_1);
    let _e84 = (*value_1);
    let _e86 = all((_e83 == _e84));
    phi_63_ = _e86;
    if _e86 {
        let _e87 = (*value_1);
        phi_63_ = all((abs(_e87) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e92 = phi_63_;
    return _e92;
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
    var phi_101_: bool;
    var phi_110_: bool;
    var phi_120_: bool;
    var phi_127_: bool;
    var phi_156_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e93 = temporalOutcome_1;
    let _e94 = (_e93 != 1u);
    phi_101_ = _e94;
    if !(_e94) {
        let _e96 = temporalCurrentClip_1;
        param = _e96;
        let _e97 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e97);
    }
    let _e100 = phi_101_;
    phi_110_ = _e100;
    if !(_e100) {
        let _e102 = temporalPreviousClip_1;
        param_1 = _e102;
        let _e103 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e103);
    }
    let _e106 = phi_110_;
    phi_120_ = _e106;
    if !(_e106) {
        let _e109 = temporalCurrentClip_1[3u];
        phi_120_ = (_e109 <= 0.000001f);
    }
    let _e112 = phi_120_;
    phi_127_ = _e112;
    if !(_e112) {
        let _e115 = temporalPreviousClip_1[3u];
        phi_127_ = (_e115 <= 0.000001f);
    }
    let _e118 = phi_127_;
    if _e118 {
        return;
    }
    let _e119 = temporalCurrentClip_1;
    let _e122 = temporalCurrentClip_1[3u];
    currentNdc = (_e119.xy / vec2(_e122));
    let _e125 = temporalPreviousClip_1;
    let _e128 = temporalPreviousClip_1[3u];
    previousNdc = (_e125.xy / vec2(_e128));
    let _e131 = currentNdc;
    param_2 = _e131;
    let _e132 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e133 = !(_e132);
    phi_156_ = _e133;
    if !(_e133) {
        let _e135 = previousNdc;
        param_3 = _e135;
        let _e136 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e136);
    }
    let _e139 = phi_156_;
    if _e139 {
        return;
    }
    let _e140 = currentNdc;
    currentUv = ((_e140 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e143 = previousNdc;
    previousUv = ((_e143 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e146 = currentUv;
    let _e147 = previousUv;
    velocity = (_e146 - _e147);
    let _e149 = velocity;
    param_4 = _e149;
    let _e150 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e150) {
        return;
    }
    let _e152 = velocity;
    out_temporal_velocity = _e152;
    let _e153 = (*coverageConfidence);
    out_temporal_validity = clamp(_e153, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e85 + 0.5f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e92 = fogType;
    let _e95 = fogType;
    return (((_e90 > 0.5f) && (_e92 >= 1i)) && (_e95 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e85 = wired_advanced_fog_enabled_u0028_();
    if !(_e85) {
        return 0f;
    }
    let _e88 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e88, 0.000001f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e93 + 0.5f));
    let _e96 = fogType_1;
    if (_e96 == 1i) {
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e100 <= 0f) {
            return 0f;
        }
        let _e102 = viewDepth;
        let _e105 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e102 / _e105), 0f, 1f);
    }
    let _e110 = unnamed.advancedFogColorDensity[3u];
    let _e112 = viewDepth;
    opticalDepth = (max(_e110, 0f) * _e112);
    let _e114 = fogType_1;
    if (_e114 == 2i) {
        let _e116 = opticalDepth;
        return clamp((1f - exp(-(_e116))), 0f, 1f);
    }
    let _e121 = opticalDepth;
    let _e122 = opticalDepth;
    return clamp((1f - exp(-((_e121 * _e122)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e85 = (*rgb);
    let _e88 = unnamed.worldLightParams[0u];
    boosted = (_e85 * _e88);
    let _e91 = boosted[0u];
    let _e93 = boosted[1u];
    let _e95 = boosted[2u];
    peak = max(_e91, max(_e93, _e95));
    let _e98 = peak;
    if (_e98 > 1f) {
        let _e100 = peak;
        let _e101 = boosted;
        boosted = (_e101 / vec3(_e100));
    }
    let _e104 = boosted;
    return _e104;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e86 = (*c);
    (*c) = max(_e86, vec3<f32>(0f, 0f, 0f));
    let _e88 = (*c);
    cutoff = (_e88 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e90 = (*c);
    lo = (_e90 / vec3(12.92f));
    let _e93 = (*c);
    hi = pow(((_e93 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e98 = hi;
    let _e99 = lo;
    let _e100 = cutoff;
    return mix(_e98, _e99, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e100));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e98 = (*role);
    let _e100 = (*role);
    let _e105 = unnamed.packed_indices[(_e98 / 4u)][(_e100 % 4u)];
    let _e110 = (*uv);
    let _e111 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    c_1 = _e111;
    let _e112 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e112))) == 0i) {
        let _e117 = c_1;
        param_5 = _e117.xyz;
        let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = (*slot);
    if (lightmap_slot == (_e126 + 1i)) {
        let _e129 = c_1;
        param_6 = _e129.xyz;
        let _e131 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e131.x;
        c_1[1u] = _e131.y;
        c_1[2u] = _e131.z;
    }
    let _e138 = c_1;
    return _e138;
}

fn main_1() {
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

    let _e121 = frag_color0In_1;
    param_7 = _e121.xyz;
    let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e125 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e123.x, _e123.y, _e123.z, _e125);
    let _e130 = frag_color1In_1;
    param_8 = _e130.xyz;
    let _e132 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e134 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e132.x, _e132.y, _e132.z, _e134);
    let _e139 = frag_color2In_1;
    param_9 = _e139.xyz;
    let _e141 = sRGBToLinear_u0028_vf3_u003b((&param_9));
    let _e143 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e141.x, _e141.y, _e141.z, _e143);
    param_10 = 0u;
    let _e148 = frag_tex_coord0_1;
    param_11 = _e148;
    param_12 = 0i;
    let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor0_ = _e149;
    param_13 = 1u;
    let _e150 = frag_tex_coord1_1;
    param_14 = _e150;
    param_15 = 1i;
    let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    sampledColor1_ = _e151;
    param_16 = 2u;
    let _e152 = frag_tex_coord2_1;
    param_17 = _e152;
    param_18 = 2i;
    let _e153 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor2_ = _e153;
    let _e154 = sampledColor0_;
    let _e155 = frag_color0_;
    color0_ = (_e154 * _e155);
    if override_type_3_2 {
        let _e157 = sampledColor1_;
        let _e158 = frag_color1_;
        color1_ = (_e157 * _e158);
        let _e160 = sampledColor2_;
        let _e161 = frag_color2_;
        color2_ = (_e160 * _e161);
        let _e163 = color0_;
        let _e165 = color1_;
        let _e168 = color2_;
        let _e170 = ((_e163.xyz + _e165.xyz) + _e168.xyz);
        let _e172 = color0_[3u];
        let _e174 = color1_[3u];
        let _e177 = color2_[3u];
        base = vec4<f32>(_e170.x, _e170.y, _e170.z, ((_e172 * _e174) * _e177));
    } else {
        if override_type_3_3 {
            let _e183 = sampledColor1_;
            let _e184 = frag_color1_;
            color1_1 = (_e183 * _e184);
            let _e186 = sampledColor2_;
            let _e187 = frag_color2_;
            color2_1 = (_e186 * _e187);
            let _e190 = color0_[3u];
            let _e191 = color0_;
            color0_ = (_e191 * _e190);
            let _e194 = color1_1[3u];
            let _e195 = color1_1;
            color1_1 = (_e195 * _e194);
            let _e198 = color2_1[3u];
            let _e199 = color2_1;
            color2_1 = (_e199 * _e198);
            let _e201 = color0_;
            let _e203 = color1_1;
            let _e206 = color2_1;
            let _e208 = ((_e201.xyz + _e203.xyz) + _e206.xyz);
            let _e210 = color0_[3u];
            let _e212 = color1_1[3u];
            let _e215 = color2_1[3u];
            base = vec4<f32>(_e208.x, _e208.y, _e208.z, ((_e210 * _e212) * _e215));
        } else {
            if override_type_3_4 {
                let _e221 = sampledColor1_;
                let _e222 = frag_color1_;
                color1_2 = (_e221 * _e222);
                let _e224 = sampledColor2_;
                let _e225 = frag_color2_;
                color2_2 = (_e224 * _e225);
                let _e228 = color0_[3u];
                let _e230 = color0_;
                color0_ = (_e230 * (1f - _e228));
                let _e233 = color1_2[3u];
                let _e235 = color1_2;
                color1_2 = (_e235 * (1f - _e233));
                let _e238 = color2_2[3u];
                let _e240 = color2_2;
                color2_2 = (_e240 * (1f - _e238));
                let _e242 = color0_;
                let _e244 = color1_2;
                let _e247 = color2_2;
                let _e249 = ((_e242.xyz + _e244.xyz) + _e247.xyz);
                let _e251 = color0_[3u];
                let _e253 = color1_2[3u];
                let _e256 = color2_2[3u];
                base = vec4<f32>(_e249.x, _e249.y, _e249.z, ((_e251 * _e253) * _e256));
            } else {
                if override_type_3_5 {
                    let _e262 = sampledColor1_;
                    let _e263 = frag_color1_;
                    color1_3 = (_e262 * _e263);
                    let _e265 = sampledColor2_;
                    let _e266 = frag_color2_;
                    color2_3 = (_e265 * _e266);
                    let _e268 = color0_;
                    let _e269 = color1_3;
                    let _e271 = color1_3[3u];
                    let _e274 = color2_3;
                    let _e276 = color2_3[3u];
                    base = mix(mix(_e268, _e269, vec4(_e271)), _e274, vec4(_e276));
                } else {
                    if override_type_3_6 {
                        let _e279 = sampledColor1_;
                        let _e280 = frag_color1_;
                        color1_4 = (_e279 * _e280);
                        let _e282 = sampledColor2_;
                        let _e283 = frag_color2_;
                        color2_4 = (_e282 * _e283);
                        let _e285 = color2_4;
                        let _e286 = color1_4;
                        let _e287 = color0_;
                        let _e289 = color1_4[3u];
                        let _e293 = color2_4[3u];
                        base = mix(_e285, mix(_e286, _e287, vec4(_e289)), vec4(_e293));
                    } else {
                        if override_type_3_7 {
                            let _e296 = sampledColor1_;
                            let _e297 = frag_color1_;
                            color1_5 = (_e296 * _e297);
                            let _e299 = sampledColor2_;
                            let _e300 = frag_color2_;
                            color2_5 = (_e299 * _e300);
                            let _e302 = color2_5;
                            let _e304 = color2_5[3u];
                            let _e307 = color1_5;
                            let _e309 = color1_5[3u];
                            let _e313 = color0_;
                            base = (((_e302 + vec4(_e304)) * (_e307 + vec4(_e309))) * _e313);
                        } else {
                            let _e315 = sampledColor1_;
                            let _e316 = frag_color1_;
                            color1_6 = (_e315 * _e316);
                            let _e318 = sampledColor2_;
                            let _e319 = frag_color2_;
                            color2_6 = (_e318 * _e319);
                            let _e321 = color0_;
                            let _e323 = color1_6;
                            let _e326 = color2_6;
                            let _e328 = ((_e321.xyz * _e323.xyz) * _e326.xyz);
                            base[0u] = _e328.x;
                            base[1u] = _e328.y;
                            base[2u] = _e328.z;
                            let _e336 = color0_[3u];
                            let _e338 = color1_6[3u];
                            let _e341 = color2_6[3u];
                            base[3u] = ((_e336 * _e338) * _e341);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e346 = unnamed.worldLightParams[1u];
        wetness = clamp(_e346, 0f, 1f);
        let _e350 = unnamed.worldLightParams[2u];
        frost = clamp(_e350, 0f, 1f);
        let _e352 = base;
        luminance = dot(_e352.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e355 = wetness;
        let _e357 = base;
        let _e359 = (_e357.xyz * mix(1f, 0.82f, _e355));
        base[0u] = _e359.x;
        base[1u] = _e359.y;
        base[2u] = _e359.z;
        let _e366 = base;
        let _e368 = luminance;
        let _e370 = luminance;
        let _e372 = luminance;
        let _e374 = frost;
        let _e377 = mix(_e366.xyz, vec3<f32>((_e368 * 0.88f), (_e370 * 0.94f), _e372), vec3((_e374 * 0.55f)));
        base[0u] = _e377.x;
        base[1u] = _e377.y;
        base[2u] = _e377.z;
    }
    let _e386 = unnamed.materialFactors[0u];
    if (_e386 >= 0f) {
        let _e389 = unnamed.materialBaseColor;
        let _e390 = base;
        base = (_e390 * _e389);
    }
    let _e392 = color0_;
    let _e395 = unnamed.emissionRadiance;
    let _e398 = base;
    let _e400 = (_e398.xyz + (_e392.xyz * _e395.xyz));
    base[0u] = _e400.x;
    base[1u] = _e400.y;
    base[2u] = _e400.z;
    let _e407 = wired_advanced_fog_enabled_u0028_();
    if _e407 {
        let _e408 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e408;
        let _e409 = base;
        let _e412 = unnamed.advancedFogColorDensity;
        let _e414 = fogAmount;
        let _e416 = mix(_e409.xyz, _e412.xyz, vec3(_e414));
        base[0u] = _e416.x;
        base[1u] = _e416.y;
        base[2u] = _e416.z;
    }
    if override_type_3_9 {
        let _e424 = base[3u];
        if (_e424 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e426 = base;
            let _e428 = base;
            if (dot(_e426.xyz, _e428.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e432 = base;
    out_color = _e432;
    param_19 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_19));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
