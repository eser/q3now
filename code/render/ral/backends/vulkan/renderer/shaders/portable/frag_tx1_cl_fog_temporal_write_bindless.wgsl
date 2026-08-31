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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e87 = (*value);
    let _e88 = (*value);
    let _e90 = all((_e87 == _e88));
    phi_78_ = _e90;
    if _e90 {
        let _e91 = (*value);
        phi_78_ = all((abs(_e91) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e96 = phi_78_;
    return _e96;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e87 = (*value_1);
    let _e88 = (*value_1);
    let _e90 = all((_e87 == _e88));
    phi_63_ = _e90;
    if _e90 {
        let _e91 = (*value_1);
        phi_63_ = all((abs(_e91) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e96 = phi_63_;
    return _e96;
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
    let _e97 = temporalOutcome_1;
    let _e98 = (_e97 != 1u);
    phi_101_ = _e98;
    if !(_e98) {
        let _e100 = temporalCurrentClip_1;
        param = _e100;
        let _e101 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e101);
    }
    let _e104 = phi_101_;
    phi_110_ = _e104;
    if !(_e104) {
        let _e106 = temporalPreviousClip_1;
        param_1 = _e106;
        let _e107 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e107);
    }
    let _e110 = phi_110_;
    phi_120_ = _e110;
    if !(_e110) {
        let _e113 = temporalCurrentClip_1[3u];
        phi_120_ = (_e113 <= 0.000001f);
    }
    let _e116 = phi_120_;
    phi_127_ = _e116;
    if !(_e116) {
        let _e119 = temporalPreviousClip_1[3u];
        phi_127_ = (_e119 <= 0.000001f);
    }
    let _e122 = phi_127_;
    if _e122 {
        return;
    }
    let _e123 = temporalCurrentClip_1;
    let _e126 = temporalCurrentClip_1[3u];
    currentNdc = (_e123.xy / vec2(_e126));
    let _e129 = temporalPreviousClip_1;
    let _e132 = temporalPreviousClip_1[3u];
    previousNdc = (_e129.xy / vec2(_e132));
    let _e135 = currentNdc;
    param_2 = _e135;
    let _e136 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e137 = !(_e136);
    phi_156_ = _e137;
    if !(_e137) {
        let _e139 = previousNdc;
        param_3 = _e139;
        let _e140 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e140);
    }
    let _e143 = phi_156_;
    if _e143 {
        return;
    }
    let _e144 = currentNdc;
    currentUv = ((_e144 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e147 = previousNdc;
    previousUv = ((_e147 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e150 = currentUv;
    let _e151 = previousUv;
    velocity = (_e150 - _e151);
    let _e153 = velocity;
    param_4 = _e153;
    let _e154 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e154) {
        return;
    }
    let _e156 = velocity;
    out_temporal_velocity = _e156;
    let _e157 = (*coverageConfidence);
    out_temporal_validity = clamp(_e157, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e89 + 0.5f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e96 = fogType;
    let _e99 = fogType;
    return (((_e94 > 0.5f) && (_e96 >= 1i)) && (_e99 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e89 = wired_advanced_fog_enabled_u0028_();
    if !(_e89) {
        return 0f;
    }
    let _e92 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e92, 0.000001f));
    let _e97 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e97 + 0.5f));
    let _e100 = fogType_1;
    if (_e100 == 1i) {
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e104 <= 0f) {
            return 0f;
        }
        let _e106 = viewDepth;
        let _e109 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e106 / _e109), 0f, 1f);
    }
    let _e114 = unnamed.advancedFogColorDensity[3u];
    let _e116 = viewDepth;
    opticalDepth = (max(_e114, 0f) * _e116);
    let _e118 = fogType_1;
    if (_e118 == 2i) {
        let _e120 = opticalDepth;
        return clamp((1f - exp(-(_e120))), 0f, 1f);
    }
    let _e125 = opticalDepth;
    let _e126 = opticalDepth;
    return clamp((1f - exp(-((_e125 * _e126)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e89 = (*rgb);
    let _e92 = unnamed.worldLightParams[0u];
    boosted = (_e89 * _e92);
    let _e95 = boosted[0u];
    let _e97 = boosted[1u];
    let _e99 = boosted[2u];
    peak = max(_e95, max(_e97, _e99));
    let _e102 = peak;
    if (_e102 > 1f) {
        let _e104 = peak;
        let _e105 = boosted;
        boosted = (_e105 / vec3(_e104));
    }
    let _e108 = boosted;
    return _e108;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e90 = (*c);
    (*c) = max(_e90, vec3<f32>(0f, 0f, 0f));
    let _e92 = (*c);
    cutoff = (_e92 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e94 = (*c);
    lo = (_e94 / vec3(12.92f));
    let _e97 = (*c);
    hi = pow(((_e97 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e102 = hi;
    let _e103 = lo;
    let _e104 = cutoff;
    return mix(_e102, _e103, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e104));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e102 = (*role);
    let _e104 = (*role);
    let _e109 = unnamed.packed_indices[(_e102 / 4u)][(_e104 % 4u)];
    let _e114 = (*uv);
    let _e115 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e109 >> bitcast<u32>(12i)) & 255u)], _e114);
    c_1 = _e115;
    let _e116 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e116))) == 0i) {
        let _e121 = c_1;
        param_5 = _e121.xyz;
        let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e123.x;
        c_1[1u] = _e123.y;
        c_1[2u] = _e123.z;
    }
    let _e130 = (*slot);
    if (lightmap_slot == (_e130 + 1i)) {
        let _e133 = c_1;
        param_6 = _e133.xyz;
        let _e135 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e135.x;
        c_1[1u] = _e135.y;
        c_1[2u] = _e135.z;
    }
    let _e142 = c_1;
    return _e142;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var color0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_2: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_3: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_4: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color1_5: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color1_6: vec4<f32>;
    var param_30: u32;
    var param_31: vec2<f32>;
    var param_32: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_33: f32;

    let _e132 = unnamed.packed_indices[0i][3u];
    let _e138 = unnamed.packed_indices[0i][3u];
    let _e143 = fog_tex_coord_1;
    let _e144 = textureSample(wired_bindless_images[(_e132 & 4095u)], wired_bindless_samplers[((_e138 >> bitcast<u32>(12i)) & 255u)], _e143);
    fog = _e144;
    let _e145 = frag_color0In_1;
    param_7 = _e145.xyz;
    let _e147 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e149 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e147.x, _e147.y, _e147.z, _e149);
    let _e154 = frag_color1In_1;
    param_8 = _e154.xyz;
    let _e156 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e158 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e156.x, _e156.y, _e156.z, _e158);
    param_9 = 0u;
    let _e163 = frag_tex_coord0_1;
    param_10 = _e163;
    param_11 = 0i;
    let _e164 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    let _e165 = frag_color0_;
    color0_ = (_e164 * _e165);
    if override_type_3_2 {
        param_12 = 1u;
        let _e167 = frag_tex_coord1_1;
        param_13 = _e167;
        param_14 = 1i;
        let _e168 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
        let _e169 = frag_color1_;
        color1_ = (_e168 * _e169);
        let _e171 = color0_;
        let _e173 = color1_;
        let _e175 = (_e171.xyz + _e173.xyz);
        let _e177 = color0_[3u];
        let _e179 = color1_[3u];
        base = vec4<f32>(_e175.x, _e175.y, _e175.z, (_e177 * _e179));
    } else {
        if override_type_3_3 {
            param_15 = 1u;
            let _e185 = frag_tex_coord1_1;
            param_16 = _e185;
            param_17 = 1i;
            let _e186 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            let _e187 = frag_color1_;
            color1_1 = (_e186 * _e187);
            let _e190 = color0_[3u];
            let _e191 = color0_;
            color0_ = (_e191 * _e190);
            let _e194 = color1_1[3u];
            let _e195 = color1_1;
            color1_1 = (_e195 * _e194);
            let _e197 = color0_;
            let _e199 = color1_1;
            let _e201 = (_e197.xyz + _e199.xyz);
            let _e203 = color0_[3u];
            let _e205 = color1_1[3u];
            base = vec4<f32>(_e201.x, _e201.y, _e201.z, (_e203 * _e205));
        } else {
            if override_type_3_4 {
                param_18 = 1u;
                let _e211 = frag_tex_coord1_1;
                param_19 = _e211;
                param_20 = 1i;
                let _e212 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                let _e213 = frag_color1_;
                color1_2 = (_e212 * _e213);
                let _e216 = color0_[3u];
                let _e218 = color0_;
                color0_ = (_e218 * (1f - _e216));
                let _e221 = color1_2[3u];
                let _e223 = color1_2;
                color1_2 = (_e223 * (1f - _e221));
                let _e225 = color0_;
                let _e227 = color1_2;
                let _e229 = (_e225.xyz + _e227.xyz);
                let _e231 = color0_[3u];
                let _e233 = color1_2[3u];
                base = vec4<f32>(_e229.x, _e229.y, _e229.z, (_e231 * _e233));
            } else {
                if override_type_3_5 {
                    param_21 = 1u;
                    let _e239 = frag_tex_coord1_1;
                    param_22 = _e239;
                    param_23 = 1i;
                    let _e240 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                    let _e241 = frag_color1_;
                    color1_3 = (_e240 * _e241);
                    let _e243 = color0_;
                    let _e244 = color1_3;
                    let _e246 = color1_3[3u];
                    base = mix(_e243, _e244, vec4(_e246));
                } else {
                    if override_type_3_6 {
                        param_24 = 1u;
                        let _e249 = frag_tex_coord1_1;
                        param_25 = _e249;
                        param_26 = 1i;
                        let _e250 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                        let _e251 = frag_color1_;
                        color1_4 = (_e250 * _e251);
                        let _e253 = color1_4;
                        let _e254 = color0_;
                        let _e256 = color1_4[3u];
                        base = mix(_e253, _e254, vec4(_e256));
                    } else {
                        if override_type_3_7 {
                            param_27 = 1u;
                            let _e259 = frag_tex_coord1_1;
                            param_28 = _e259;
                            param_29 = 1i;
                            let _e260 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
                            let _e261 = frag_color1_;
                            color1_5 = (_e260 * _e261);
                            let _e263 = color1_5;
                            let _e265 = color1_5[3u];
                            let _e268 = color0_;
                            base = ((_e263 + vec4(_e265)) * _e268);
                        } else {
                            param_30 = 1u;
                            let _e270 = frag_tex_coord1_1;
                            param_31 = _e270;
                            param_32 = 1i;
                            let _e271 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
                            let _e272 = frag_color1_;
                            color1_6 = (_e271 * _e272);
                            let _e274 = color0_;
                            let _e276 = color1_6;
                            let _e278 = (_e274.xyz * _e276.xyz);
                            base[0u] = _e278.x;
                            base[1u] = _e278.y;
                            base[2u] = _e278.z;
                            let _e286 = color0_[3u];
                            let _e288 = color1_6[3u];
                            base[3u] = (_e286 * _e288);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e293 = unnamed.worldLightParams[1u];
        wetness = clamp(_e293, 0f, 1f);
        let _e297 = unnamed.worldLightParams[2u];
        frost = clamp(_e297, 0f, 1f);
        let _e299 = base;
        luminance = dot(_e299.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e302 = wetness;
        let _e304 = base;
        let _e306 = (_e304.xyz * mix(1f, 0.82f, _e302));
        base[0u] = _e306.x;
        base[1u] = _e306.y;
        base[2u] = _e306.z;
        let _e313 = base;
        let _e315 = luminance;
        let _e317 = luminance;
        let _e319 = luminance;
        let _e321 = frost;
        let _e324 = mix(_e313.xyz, vec3<f32>((_e315 * 0.88f), (_e317 * 0.94f), _e319), vec3((_e321 * 0.55f)));
        base[0u] = _e324.x;
        base[1u] = _e324.y;
        base[2u] = _e324.z;
    }
    let _e331 = color0_;
    let _e334 = unnamed.emissionRadiance;
    let _e337 = base;
    let _e339 = (_e337.xyz + (_e331.xyz * _e334.xyz));
    base[0u] = _e339.x;
    base[1u] = _e339.y;
    base[2u] = _e339.z;
    let _e346 = wired_advanced_fog_enabled_u0028_();
    if _e346 {
        let _e347 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e347;
        if override_type_3_9 {
            let _e348 = fogAmount;
            let _e350 = base;
            let _e352 = (_e350.xyz * (1f - _e348));
            base[0u] = _e352.x;
            base[1u] = _e352.y;
            base[2u] = _e352.z;
        } else {
            if override_type_3_10 {
                let _e359 = fogAmount;
                let _e361 = base;
                base = (_e361 * (1f - _e359));
            } else {
                if override_type_3_11 {
                    let _e363 = fogAmount;
                    let _e366 = base[3u];
                    base[3u] = (_e366 * (1f - _e363));
                } else {
                    let _e369 = base;
                    let _e372 = unnamed.advancedFogColorDensity;
                    let _e374 = fogAmount;
                    let _e376 = mix(_e369.xyz, _e372.xyz, vec3(_e374));
                    base[0u] = _e376.x;
                    base[1u] = _e376.y;
                    base[2u] = _e376.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e383 = base;
            let _e386 = fog[3u];
            let _e388 = (_e383.xyz * (1f - _e386));
            base[0u] = _e388.x;
            base[1u] = _e388.y;
            base[2u] = _e388.z;
        } else {
            if override_type_3_13 {
                let _e395 = base;
                let _e397 = fog[3u];
                base = (_e395 * (1f - _e397));
            } else {
                if override_type_3_14 {
                    let _e401 = base[3u];
                    let _e403 = fog[3u];
                    base[3u] = (_e401 * (1f - _e403));
                } else {
                    let _e407 = base;
                    let _e408 = fog;
                    let _e410 = unnamed.fogColor;
                    let _e413 = fog[3u];
                    base = mix(_e407, (_e408 * _e410), vec4(_e413));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e417 = base[3u];
        if (_e417 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e419 = base;
            let _e421 = base;
            if (dot(_e419.xyz, _e421.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e425 = base;
    out_color = _e425;
    param_33 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_33));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
