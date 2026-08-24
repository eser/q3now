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
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
@id(10) override acff: i32 = 0i;
override override_type_3_2: bool = (acff == 1i);
override override_type_3_3: bool = (acff == 2i);
override override_type_3_4: bool = (acff == 3i);
override override_type_3_5: bool = (acff == 1i);
override override_type_3_6: bool = (acff == 2i);
override override_type_3_7: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_8: bool = (discard_mode == 1i);
override override_type_3_9: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e68 = (*value);
    let _e69 = (*value);
    let _e71 = all((_e68 == _e69));
    phi_75_ = _e71;
    if _e71 {
        let _e72 = (*value);
        phi_75_ = all((abs(_e72) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e77 = phi_75_;
    return _e77;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e68 = (*value_1);
    let _e69 = (*value_1);
    let _e71 = all((_e68 == _e69));
    phi_60_ = _e71;
    if _e71 {
        let _e72 = (*value_1);
        phi_60_ = all((abs(_e72) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e77 = phi_60_;
    return _e77;
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
    var phi_98_: bool;
    var phi_107_: bool;
    var phi_117_: bool;
    var phi_124_: bool;
    var phi_153_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e78 = temporalOutcome_1;
    let _e79 = (_e78 != 1u);
    phi_98_ = _e79;
    if !(_e79) {
        let _e81 = temporalCurrentClip_1;
        param = _e81;
        let _e82 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e82);
    }
    let _e85 = phi_98_;
    phi_107_ = _e85;
    if !(_e85) {
        let _e87 = temporalPreviousClip_1;
        param_1 = _e87;
        let _e88 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e88);
    }
    let _e91 = phi_107_;
    phi_117_ = _e91;
    if !(_e91) {
        let _e94 = temporalCurrentClip_1[3u];
        phi_117_ = (_e94 <= 0.000001f);
    }
    let _e97 = phi_117_;
    phi_124_ = _e97;
    if !(_e97) {
        let _e100 = temporalPreviousClip_1[3u];
        phi_124_ = (_e100 <= 0.000001f);
    }
    let _e103 = phi_124_;
    if _e103 {
        return;
    }
    let _e104 = temporalCurrentClip_1;
    let _e107 = temporalCurrentClip_1[3u];
    currentNdc = (_e104.xy / vec2(_e107));
    let _e110 = temporalPreviousClip_1;
    let _e113 = temporalPreviousClip_1[3u];
    previousNdc = (_e110.xy / vec2(_e113));
    let _e116 = currentNdc;
    param_2 = _e116;
    let _e117 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e118 = !(_e117);
    phi_153_ = _e118;
    if !(_e118) {
        let _e120 = previousNdc;
        param_3 = _e120;
        let _e121 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e121);
    }
    let _e124 = phi_153_;
    if _e124 {
        return;
    }
    let _e125 = currentNdc;
    currentUv = ((_e125 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e128 = previousNdc;
    previousUv = ((_e128 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e131 = currentUv;
    let _e132 = previousUv;
    velocity = (_e131 - _e132);
    let _e134 = velocity;
    param_4 = _e134;
    let _e135 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e135) {
        return;
    }
    let _e137 = velocity;
    out_temporal_velocity = _e137;
    let _e138 = (*coverageConfidence);
    out_temporal_validity = clamp(_e138, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e70 + 0.5f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e77 = fogType;
    let _e80 = fogType;
    return (((_e75 > 0.5f) && (_e77 >= 1i)) && (_e80 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e70 = wired_advanced_fog_enabled_u0028_();
    if !(_e70) {
        return 0f;
    }
    let _e73 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e73, 0.000001f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e78 + 0.5f));
    let _e81 = fogType_1;
    if (_e81 == 1i) {
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e85 <= 0f) {
            return 0f;
        }
        let _e87 = viewDepth;
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e87 / _e90), 0f, 1f);
    }
    let _e95 = unnamed.advancedFogColorDensity[3u];
    let _e97 = viewDepth;
    opticalDepth = (max(_e95, 0f) * _e97);
    let _e99 = fogType_1;
    if (_e99 == 2i) {
        let _e101 = opticalDepth;
        return clamp((1f - exp(-(_e101))), 0f, 1f);
    }
    let _e106 = opticalDepth;
    let _e107 = opticalDepth;
    return clamp((1f - exp(-((_e106 * _e107)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e71 = (*c);
    (*c) = max(_e71, vec3<f32>(0f, 0f, 0f));
    let _e73 = (*c);
    cutoff = (_e73 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e75 = (*c);
    lo = (_e75 / vec3(12.92f));
    let _e78 = (*c);
    hi = pow(((_e78 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e83 = hi;
    let _e84 = lo;
    let _e85 = cutoff;
    return mix(_e83, _e84, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e85));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e94 = (*uv);
    let _e95 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    c_1 = _e95;
    let _e96 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e96))) == 0i) {
        let _e101 = c_1;
        param_5 = _e101.xyz;
        let _e103 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e103.x;
        c_1[1u] = _e103.y;
        c_1[2u] = _e103.z;
    }
    let _e110 = (*slot);
    if (lightmap_slot == (_e110 + 1i)) {
        let _e115 = unnamed.worldLightParams[0u];
        let _e116 = c_1;
        let _e118 = (_e116.xyz * _e115);
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = c_1;
    return _e125;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var color1_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_2: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var fogAmount: f32;
    var param_18: f32;

    let _e91 = unnamed.packed_indices[0i][3u];
    let _e97 = unnamed.packed_indices[0i][3u];
    let _e102 = fog_tex_coord_1;
    let _e103 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    fog = _e103;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e108 = frag_tex_coord0_1;
    param_7 = _e108;
    param_8 = 0i;
    let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e110 = frag_color;
    color0_ = (_e109 * _e110);
    if override_type_3_ {
        param_9 = 1u;
        let _e112 = frag_tex_coord1_1;
        param_10 = _e112;
        param_11 = 1i;
        let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e113;
        let _e114 = color0_;
        let _e116 = color1_;
        let _e118 = (_e114.xyz + _e116.xyz);
        let _e120 = color0_[3u];
        let _e122 = color1_[3u];
        base = vec4<f32>(_e118.x, _e118.y, _e118.z, (_e120 * _e122));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e128 = frag_tex_coord1_1;
            param_13 = _e128;
            param_14 = 1i;
            let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            let _e130 = frag_color;
            color1_1 = (_e129 * _e130);
            let _e132 = color0_;
            let _e134 = color1_1;
            let _e136 = (_e132.xyz + _e134.xyz);
            let _e138 = color0_[3u];
            let _e140 = color1_1[3u];
            base = vec4<f32>(_e136.x, _e136.y, _e136.z, (_e138 * _e140));
        } else {
            param_15 = 1u;
            let _e146 = frag_tex_coord1_1;
            param_16 = _e146;
            param_17 = 1i;
            let _e147 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            let _e148 = frag_color;
            color1_2 = (_e147 * _e148);
            let _e150 = color0_;
            let _e152 = color1_2;
            let _e154 = (_e150.xyz * _e152.xyz);
            base[0u] = _e154.x;
            base[1u] = _e154.y;
            base[2u] = _e154.z;
            let _e162 = color0_[3u];
            let _e164 = color1_2[3u];
            base[3u] = (_e162 * _e164);
        }
    }
    let _e167 = wired_advanced_fog_enabled_u0028_();
    if _e167 {
        let _e168 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e168;
        if override_type_3_2 {
            let _e169 = fogAmount;
            let _e171 = base;
            let _e173 = (_e171.xyz * (1f - _e169));
            base[0u] = _e173.x;
            base[1u] = _e173.y;
            base[2u] = _e173.z;
        } else {
            if override_type_3_3 {
                let _e180 = fogAmount;
                let _e182 = base;
                base = (_e182 * (1f - _e180));
            } else {
                if override_type_3_4 {
                    let _e184 = fogAmount;
                    let _e187 = base[3u];
                    base[3u] = (_e187 * (1f - _e184));
                } else {
                    let _e190 = base;
                    let _e193 = unnamed.advancedFogColorDensity;
                    let _e195 = fogAmount;
                    let _e197 = mix(_e190.xyz, _e193.xyz, vec3(_e195));
                    base[0u] = _e197.x;
                    base[1u] = _e197.y;
                    base[2u] = _e197.z;
                }
            }
        }
    } else {
        if override_type_3_5 {
            let _e204 = base;
            let _e207 = fog[3u];
            let _e209 = (_e204.xyz * (1f - _e207));
            base[0u] = _e209.x;
            base[1u] = _e209.y;
            base[2u] = _e209.z;
        } else {
            if override_type_3_6 {
                let _e216 = base;
                let _e218 = fog[3u];
                base = (_e216 * (1f - _e218));
            } else {
                if override_type_3_7 {
                    let _e222 = base[3u];
                    let _e224 = fog[3u];
                    base[3u] = (_e222 * (1f - _e224));
                } else {
                    let _e228 = base;
                    let _e229 = fog;
                    let _e231 = unnamed.fogColor;
                    let _e234 = fog[3u];
                    base = mix(_e228, (_e229 * _e231), vec4(_e234));
                }
            }
        }
    }
    if override_type_3_8 {
        let _e238 = base[3u];
        if (_e238 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e240 = base;
            let _e242 = base;
            if (dot(_e240.xyz, _e242.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e246 = base;
    out_color = _e246;
    param_18 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_18));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
