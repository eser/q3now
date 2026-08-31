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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e69 = (*value);
    let _e70 = (*value);
    let _e72 = all((_e69 == _e70));
    phi_78_ = _e72;
    if _e72 {
        let _e73 = (*value);
        phi_78_ = all((abs(_e73) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_78_;
    return _e78;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e69 = (*value_1);
    let _e70 = (*value_1);
    let _e72 = all((_e69 == _e70));
    phi_63_ = _e72;
    if _e72 {
        let _e73 = (*value_1);
        phi_63_ = all((abs(_e73) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_63_;
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
    var phi_101_: bool;
    var phi_110_: bool;
    var phi_120_: bool;
    var phi_127_: bool;
    var phi_156_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e79 = temporalOutcome_1;
    let _e80 = (_e79 != 1u);
    phi_101_ = _e80;
    if !(_e80) {
        let _e82 = temporalCurrentClip_1;
        param = _e82;
        let _e83 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e83);
    }
    let _e86 = phi_101_;
    phi_110_ = _e86;
    if !(_e86) {
        let _e88 = temporalPreviousClip_1;
        param_1 = _e88;
        let _e89 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e89);
    }
    let _e92 = phi_110_;
    phi_120_ = _e92;
    if !(_e92) {
        let _e95 = temporalCurrentClip_1[3u];
        phi_120_ = (_e95 <= 0.000001f);
    }
    let _e98 = phi_120_;
    phi_127_ = _e98;
    if !(_e98) {
        let _e101 = temporalPreviousClip_1[3u];
        phi_127_ = (_e101 <= 0.000001f);
    }
    let _e104 = phi_127_;
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
    phi_156_ = _e119;
    if !(_e119) {
        let _e121 = previousNdc;
        param_3 = _e121;
        let _e122 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e122);
    }
    let _e125 = phi_156_;
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

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e71 = (*rgb);
    let _e74 = unnamed.worldLightParams[0u];
    boosted = (_e71 * _e74);
    let _e77 = boosted[0u];
    let _e79 = boosted[1u];
    let _e81 = boosted[2u];
    peak = max(_e77, max(_e79, _e81));
    let _e84 = peak;
    if (_e84 > 1f) {
        let _e86 = peak;
        let _e87 = boosted;
        boosted = (_e87 / vec3(_e86));
    }
    let _e90 = boosted;
    return _e90;
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
    var param_6: vec3<f32>;

    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e96 = (*uv);
    let _e97 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e91 >> bitcast<u32>(12i)) & 255u)], _e96);
    c_1 = _e97;
    let _e98 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e98))) == 0i) {
        let _e103 = c_1;
        param_5 = _e103.xyz;
        let _e105 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = (*slot);
    if (lightmap_slot == (_e112 + 1i)) {
        let _e115 = c_1;
        param_6 = _e115.xyz;
        let _e117 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_2: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_19: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_7 = 0u;
    let _e95 = frag_tex_coord0_1;
    param_8 = _e95;
    param_9 = 0i;
    let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e97 = frag_color;
    color0_ = (_e96 * _e97);
    if override_type_3_ {
        param_10 = 1u;
        let _e99 = frag_tex_coord1_1;
        param_11 = _e99;
        param_12 = 1i;
        let _e100 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e100;
        let _e101 = color0_;
        let _e103 = color1_;
        let _e105 = (_e101.xyz + _e103.xyz);
        let _e107 = color0_[3u];
        let _e109 = color1_[3u];
        base = vec4<f32>(_e105.x, _e105.y, _e105.z, (_e107 * _e109));
    } else {
        if override_type_3_1 {
            param_13 = 1u;
            let _e115 = frag_tex_coord1_1;
            param_14 = _e115;
            param_15 = 1i;
            let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e117 = frag_color;
            color1_1 = (_e116 * _e117);
            let _e119 = color0_;
            let _e121 = color1_1;
            let _e123 = (_e119.xyz + _e121.xyz);
            let _e125 = color0_[3u];
            let _e127 = color1_1[3u];
            base = vec4<f32>(_e123.x, _e123.y, _e123.z, (_e125 * _e127));
        } else {
            param_16 = 1u;
            let _e133 = frag_tex_coord1_1;
            param_17 = _e133;
            param_18 = 1i;
            let _e134 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e135 = frag_color;
            color1_2 = (_e134 * _e135);
            let _e137 = color0_;
            let _e139 = color1_2;
            let _e141 = (_e137.xyz * _e139.xyz);
            base[0u] = _e141.x;
            base[1u] = _e141.y;
            base[2u] = _e141.z;
            let _e149 = color0_[3u];
            let _e151 = color1_2[3u];
            base[3u] = (_e149 * _e151);
        }
    }
    if override_type_3_2 {
        let _e156 = unnamed.worldLightParams[1u];
        wetness = clamp(_e156, 0f, 1f);
        let _e160 = unnamed.worldLightParams[2u];
        frost = clamp(_e160, 0f, 1f);
        let _e162 = base;
        luminance = dot(_e162.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e165 = wetness;
        let _e167 = base;
        let _e169 = (_e167.xyz * mix(1f, 0.82f, _e165));
        base[0u] = _e169.x;
        base[1u] = _e169.y;
        base[2u] = _e169.z;
        let _e176 = base;
        let _e178 = luminance;
        let _e180 = luminance;
        let _e182 = luminance;
        let _e184 = frost;
        let _e187 = mix(_e176.xyz, vec3<f32>((_e178 * 0.88f), (_e180 * 0.94f), _e182), vec3((_e184 * 0.55f)));
        base[0u] = _e187.x;
        base[1u] = _e187.y;
        base[2u] = _e187.z;
    }
    let _e194 = color0_;
    let _e197 = unnamed.emissionRadiance;
    let _e200 = base;
    let _e202 = (_e200.xyz + (_e194.xyz * _e197.xyz));
    base[0u] = _e202.x;
    base[1u] = _e202.y;
    base[2u] = _e202.z;
    let _e209 = wired_advanced_fog_enabled_u0028_();
    if _e209 {
        let _e210 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e210;
        let _e211 = base;
        let _e214 = unnamed.advancedFogColorDensity;
        let _e216 = fogAmount;
        let _e218 = mix(_e211.xyz, _e214.xyz, vec3(_e216));
        base[0u] = _e218.x;
        base[1u] = _e218.y;
        base[2u] = _e218.z;
    }
    if override_type_3_3 {
        let _e226 = base[3u];
        if (_e226 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e228 = base;
            let _e230 = base;
            if (dot(_e228.xyz, _e230.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e234 = base;
    out_color = _e234;
    param_19 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_19));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
