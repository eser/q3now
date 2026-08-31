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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e68 = (*value);
    let _e69 = (*value);
    let _e71 = all((_e68 == _e69));
    phi_78_ = _e71;
    if _e71 {
        let _e72 = (*value);
        phi_78_ = all((abs(_e72) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e77 = phi_78_;
    return _e77;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e68 = (*value_1);
    let _e69 = (*value_1);
    let _e71 = all((_e68 == _e69));
    phi_63_ = _e71;
    if _e71 {
        let _e72 = (*value_1);
        phi_63_ = all((abs(_e72) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e77 = phi_63_;
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
    var phi_101_: bool;
    var phi_110_: bool;
    var phi_120_: bool;
    var phi_127_: bool;
    var phi_156_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e78 = temporalOutcome_1;
    let _e79 = (_e78 != 1u);
    phi_101_ = _e79;
    if !(_e79) {
        let _e81 = temporalCurrentClip_1;
        param = _e81;
        let _e82 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e82);
    }
    let _e85 = phi_101_;
    phi_110_ = _e85;
    if !(_e85) {
        let _e87 = temporalPreviousClip_1;
        param_1 = _e87;
        let _e88 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e88);
    }
    let _e91 = phi_110_;
    phi_120_ = _e91;
    if !(_e91) {
        let _e94 = temporalCurrentClip_1[3u];
        phi_120_ = (_e94 <= 0.000001f);
    }
    let _e97 = phi_120_;
    phi_127_ = _e97;
    if !(_e97) {
        let _e100 = temporalPreviousClip_1[3u];
        phi_127_ = (_e100 <= 0.000001f);
    }
    let _e103 = phi_127_;
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
    phi_156_ = _e118;
    if !(_e118) {
        let _e120 = previousNdc;
        param_3 = _e120;
        let _e121 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e121);
    }
    let _e124 = phi_156_;
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

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e70 = (*rgb);
    let _e73 = unnamed.worldLightParams[0u];
    boosted = (_e70 * _e73);
    let _e76 = boosted[0u];
    let _e78 = boosted[1u];
    let _e80 = boosted[2u];
    peak = max(_e76, max(_e78, _e80));
    let _e83 = peak;
    if (_e83 > 1f) {
        let _e85 = peak;
        let _e86 = boosted;
        boosted = (_e86 / vec3(_e85));
    }
    let _e89 = boosted;
    return _e89;
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
    var param_6: vec3<f32>;

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
        let _e114 = c_1;
        param_6 = _e114.xyz;
        let _e116 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = c_1;
    return _e123;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var color0_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_20: f32;

    let _e91 = frag_color0In_1;
    param_7 = _e91.xyz;
    let _e93 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e95 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e93.x, _e93.y, _e93.z, _e95);
    param_8 = 0u;
    let _e100 = frag_tex_coord0_1;
    param_9 = _e100;
    param_10 = 0i;
    let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    let _e102 = frag_color0_;
    color0_ = (_e101 * _e102);
    if override_type_3_ {
        param_11 = 1u;
        let _e104 = frag_tex_coord1_1;
        param_12 = _e104;
        param_13 = 1i;
        let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
        color1_ = _e105;
        let _e106 = color0_;
        let _e108 = color1_;
        let _e110 = (_e106.xyz + _e108.xyz);
        let _e112 = color0_[3u];
        let _e114 = color1_[3u];
        base = vec4<f32>(_e110.x, _e110.y, _e110.z, (_e112 * _e114));
    } else {
        if override_type_3_1 {
            param_14 = 1u;
            let _e120 = frag_tex_coord1_1;
            param_15 = _e120;
            param_16 = 1i;
            let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e122 = frag_color0_;
            color1_1 = (_e121 * _e122);
            let _e124 = color0_;
            let _e126 = color1_1;
            let _e128 = (_e124.xyz + _e126.xyz);
            let _e130 = color0_[3u];
            let _e132 = color1_1[3u];
            base = vec4<f32>(_e128.x, _e128.y, _e128.z, (_e130 * _e132));
        } else {
            param_17 = 1u;
            let _e138 = frag_tex_coord1_1;
            param_18 = _e138;
            param_19 = 1i;
            let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e139;
            let _e140 = color0_;
            let _e142 = color1_2;
            let _e144 = (_e140.xyz * _e142.xyz);
            base[0u] = _e144.x;
            base[1u] = _e144.y;
            base[2u] = _e144.z;
            let _e152 = color0_[3u];
            let _e154 = color1_2[3u];
            base[3u] = (_e152 * _e154);
        }
    }
    if override_type_3_2 {
        let _e159 = unnamed.worldLightParams[1u];
        wetness = clamp(_e159, 0f, 1f);
        let _e163 = unnamed.worldLightParams[2u];
        frost = clamp(_e163, 0f, 1f);
        let _e165 = base;
        luminance = dot(_e165.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e168 = wetness;
        let _e170 = base;
        let _e172 = (_e170.xyz * mix(1f, 0.82f, _e168));
        base[0u] = _e172.x;
        base[1u] = _e172.y;
        base[2u] = _e172.z;
        let _e179 = base;
        let _e181 = luminance;
        let _e183 = luminance;
        let _e185 = luminance;
        let _e187 = frost;
        let _e190 = mix(_e179.xyz, vec3<f32>((_e181 * 0.88f), (_e183 * 0.94f), _e185), vec3((_e187 * 0.55f)));
        base[0u] = _e190.x;
        base[1u] = _e190.y;
        base[2u] = _e190.z;
    }
    let _e197 = color0_;
    let _e200 = unnamed.emissionRadiance;
    let _e203 = base;
    let _e205 = (_e203.xyz + (_e197.xyz * _e200.xyz));
    base[0u] = _e205.x;
    base[1u] = _e205.y;
    base[2u] = _e205.z;
    let _e212 = wired_advanced_fog_enabled_u0028_();
    if _e212 {
        let _e213 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e213;
        let _e214 = base;
        let _e217 = unnamed.advancedFogColorDensity;
        let _e219 = fogAmount;
        let _e221 = mix(_e214.xyz, _e217.xyz, vec3(_e219));
        base[0u] = _e221.x;
        base[1u] = _e221.y;
        base[2u] = _e221.z;
    }
    if override_type_3_3 {
        let _e229 = base[3u];
        if (_e229 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e231 = base;
            let _e233 = base;
            if (dot(_e231.xyz, _e233.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e237 = base;
    out_color = _e237;
    param_20 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_20));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
