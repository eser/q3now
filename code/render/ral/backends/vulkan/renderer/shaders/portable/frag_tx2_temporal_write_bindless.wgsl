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
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e69 = (*value);
    let _e70 = (*value);
    let _e72 = all((_e69 == _e70));
    phi_75_ = _e72;
    if _e72 {
        let _e73 = (*value);
        phi_75_ = all((abs(_e73) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_75_;
    return _e78;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e69 = (*value_1);
    let _e70 = (*value_1);
    let _e72 = all((_e69 == _e70));
    phi_60_ = _e72;
    if _e72 {
        let _e73 = (*value_1);
        phi_60_ = all((abs(_e73) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_60_;
    return _e78;
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
    let _e79 = temporalOutcome_1;
    let _e80 = (_e79 != 1u);
    phi_98_ = _e80;
    if !(_e80) {
        let _e82 = temporalCurrentClip_1;
        param = _e82;
        let _e83 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e83);
    }
    let _e86 = phi_98_;
    phi_107_ = _e86;
    if !(_e86) {
        let _e88 = temporalPreviousClip_1;
        param_1 = _e88;
        let _e89 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e89);
    }
    let _e92 = phi_107_;
    phi_117_ = _e92;
    if !(_e92) {
        let _e95 = temporalCurrentClip_1[3u];
        phi_117_ = (_e95 <= 0.000001f);
    }
    let _e98 = phi_117_;
    phi_124_ = _e98;
    if !(_e98) {
        let _e101 = temporalPreviousClip_1[3u];
        phi_124_ = (_e101 <= 0.000001f);
    }
    let _e104 = phi_124_;
    if _e104 {
        return;
    }
    let _e105 = temporalCurrentClip_1;
    let _e108 = temporalCurrentClip_1[3u];
    currentNdc = (_e105.xy / vec2(_e108));
    let _e111 = temporalPreviousClip_1;
    let _e114 = temporalPreviousClip_1[3u];
    previousNdc = (_e111.xy / vec2(_e114));
    let _e117 = currentNdc;
    param_2 = _e117;
    let _e118 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e119 = !(_e118);
    phi_153_ = _e119;
    if !(_e119) {
        let _e121 = previousNdc;
        param_3 = _e121;
        let _e122 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e122);
    }
    let _e125 = phi_153_;
    if _e125 {
        return;
    }
    let _e126 = currentNdc;
    currentUv = ((_e126 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e129 = previousNdc;
    previousUv = ((_e129 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e132 = currentUv;
    let _e133 = previousUv;
    velocity = (_e132 - _e133);
    let _e135 = velocity;
    param_4 = _e135;
    let _e136 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e136) {
        return;
    }
    let _e138 = velocity;
    out_temporal_velocity = _e138;
    let _e139 = (*coverageConfidence);
    out_temporal_validity = clamp(_e139, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e71 + 0.5f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e78 = fogType;
    let _e81 = fogType;
    return (((_e76 > 0.5f) && (_e78 >= 1i)) && (_e81 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e71 = wired_advanced_fog_enabled_u0028_();
    if !(_e71) {
        return 0f;
    }
    let _e74 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e74, 0.000001f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e79 + 0.5f));
    let _e82 = fogType_1;
    if (_e82 == 1i) {
        let _e86 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e86 <= 0f) {
            return 0f;
        }
        let _e88 = viewDepth;
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e88 / _e91), 0f, 1f);
    }
    let _e96 = unnamed.advancedFogColorDensity[3u];
    let _e98 = viewDepth;
    opticalDepth = (max(_e96, 0f) * _e98);
    let _e100 = fogType_1;
    if (_e100 == 2i) {
        let _e102 = opticalDepth;
        return clamp((1f - exp(-(_e102))), 0f, 1f);
    }
    let _e107 = opticalDepth;
    let _e108 = opticalDepth;
    return clamp((1f - exp(-((_e107 * _e108)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e72 = (*c);
    (*c) = max(_e72, vec3<f32>(0f, 0f, 0f));
    let _e74 = (*c);
    cutoff = (_e74 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e76 = (*c);
    lo = (_e76 / vec3(12.92f));
    let _e79 = (*c);
    hi = pow(((_e79 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e84 = hi;
    let _e85 = lo;
    let _e86 = cutoff;
    return mix(_e84, _e85, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e86));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e95 = (*uv);
    let _e96 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    c_1 = _e96;
    let _e97 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e97))) == 0i) {
        let _e102 = c_1;
        param_5 = _e102.xyz;
        let _e104 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = (*slot);
    if (lightmap_slot == (_e111 + 1i)) {
        let _e116 = unnamed.worldLightParams[0u];
        let _e117 = c_1;
        let _e119 = (_e117.xyz * _e116);
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var color2_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color2_1: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color2_2: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_28: f32;

    let _e104 = frag_color0In_1;
    param_6 = _e104.xyz;
    let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e108 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e106.x, _e106.y, _e106.z, _e108);
    param_7 = 0u;
    let _e113 = frag_tex_coord0_1;
    param_8 = _e113;
    param_9 = 0i;
    let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e115 = frag_color0_;
    color0_ = (_e114 * _e115);
    if override_type_3_ {
        param_10 = 1u;
        let _e117 = frag_tex_coord1_1;
        param_11 = _e117;
        param_12 = 1i;
        let _e118 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e118;
        param_13 = 2u;
        let _e119 = frag_tex_coord2_1;
        param_14 = _e119;
        param_15 = 2i;
        let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
        color2_ = _e120;
        let _e121 = color0_;
        let _e123 = color1_;
        let _e126 = color2_;
        let _e128 = ((_e121.xyz + _e123.xyz) + _e126.xyz);
        let _e130 = color0_[3u];
        let _e132 = color1_[3u];
        let _e135 = color2_[3u];
        base = vec4<f32>(_e128.x, _e128.y, _e128.z, ((_e130 * _e132) * _e135));
    } else {
        if override_type_3_1 {
            param_16 = 1u;
            let _e141 = frag_tex_coord1_1;
            param_17 = _e141;
            param_18 = 1i;
            let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e143 = frag_color0_;
            color1_1 = (_e142 * _e143);
            param_19 = 2u;
            let _e145 = frag_tex_coord2_1;
            param_20 = _e145;
            param_21 = 2i;
            let _e146 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            let _e147 = frag_color0_;
            color2_1 = (_e146 * _e147);
            let _e149 = color0_;
            let _e151 = color1_1;
            let _e154 = color2_1;
            let _e156 = ((_e149.xyz + _e151.xyz) + _e154.xyz);
            let _e158 = color0_[3u];
            let _e160 = color1_1[3u];
            let _e163 = color2_1[3u];
            base = vec4<f32>(_e156.x, _e156.y, _e156.z, ((_e158 * _e160) * _e163));
        } else {
            param_22 = 1u;
            let _e169 = frag_tex_coord1_1;
            param_23 = _e169;
            param_24 = 1i;
            let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            color1_2 = _e170;
            param_25 = 2u;
            let _e171 = frag_tex_coord2_1;
            param_26 = _e171;
            param_27 = 2i;
            let _e172 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
            color2_2 = _e172;
            let _e173 = color0_;
            let _e175 = color1_2;
            let _e178 = color2_2;
            let _e180 = ((_e173.xyz * _e175.xyz) * _e178.xyz);
            base[0u] = _e180.x;
            base[1u] = _e180.y;
            base[2u] = _e180.z;
            let _e188 = color0_[3u];
            let _e190 = color1_2[3u];
            let _e193 = color2_2[3u];
            base[3u] = ((_e188 * _e190) * _e193);
        }
    }
    if override_type_3_2 {
        let _e198 = unnamed.worldLightParams[1u];
        wetness = clamp(_e198, 0f, 1f);
        let _e202 = unnamed.worldLightParams[2u];
        frost = clamp(_e202, 0f, 1f);
        let _e204 = base;
        luminance = dot(_e204.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e207 = wetness;
        let _e209 = base;
        let _e211 = (_e209.xyz * mix(1f, 0.82f, _e207));
        base[0u] = _e211.x;
        base[1u] = _e211.y;
        base[2u] = _e211.z;
        let _e218 = base;
        let _e220 = luminance;
        let _e222 = luminance;
        let _e224 = luminance;
        let _e226 = frost;
        let _e229 = mix(_e218.xyz, vec3<f32>((_e220 * 0.88f), (_e222 * 0.94f), _e224), vec3((_e226 * 0.55f)));
        base[0u] = _e229.x;
        base[1u] = _e229.y;
        base[2u] = _e229.z;
    }
    let _e236 = color0_;
    let _e239 = unnamed.emissionRadiance;
    let _e242 = base;
    let _e244 = (_e242.xyz + (_e236.xyz * _e239.xyz));
    base[0u] = _e244.x;
    base[1u] = _e244.y;
    base[2u] = _e244.z;
    let _e251 = wired_advanced_fog_enabled_u0028_();
    if _e251 {
        let _e252 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e252;
        let _e253 = base;
        let _e256 = unnamed.advancedFogColorDensity;
        let _e258 = fogAmount;
        let _e260 = mix(_e253.xyz, _e256.xyz, vec3(_e258));
        base[0u] = _e260.x;
        base[1u] = _e260.y;
        base[2u] = _e260.z;
    }
    if override_type_3_3 {
        let _e268 = base[3u];
        if (_e268 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e270 = base;
            let _e272 = base;
            if (dot(_e270.xyz, _e272.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e276 = base;
    out_color = _e276;
    param_28 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_28));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
