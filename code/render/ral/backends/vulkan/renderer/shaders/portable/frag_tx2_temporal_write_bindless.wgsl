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
    var phi_78_: bool;

    let _e71 = (*value);
    let _e72 = (*value);
    let _e74 = all((_e71 == _e72));
    phi_78_ = _e74;
    if _e74 {
        let _e75 = (*value);
        phi_78_ = all((abs(_e75) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e80 = phi_78_;
    return _e80;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e71 = (*value_1);
    let _e72 = (*value_1);
    let _e74 = all((_e71 == _e72));
    phi_63_ = _e74;
    if _e74 {
        let _e75 = (*value_1);
        phi_63_ = all((abs(_e75) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e80 = phi_63_;
    return _e80;
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
    let _e81 = temporalOutcome_1;
    let _e82 = (_e81 != 1u);
    phi_101_ = _e82;
    if !(_e82) {
        let _e84 = temporalCurrentClip_1;
        param = _e84;
        let _e85 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e85);
    }
    let _e88 = phi_101_;
    phi_110_ = _e88;
    if !(_e88) {
        let _e90 = temporalPreviousClip_1;
        param_1 = _e90;
        let _e91 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e91);
    }
    let _e94 = phi_110_;
    phi_120_ = _e94;
    if !(_e94) {
        let _e97 = temporalCurrentClip_1[3u];
        phi_120_ = (_e97 <= 0.000001f);
    }
    let _e100 = phi_120_;
    phi_127_ = _e100;
    if !(_e100) {
        let _e103 = temporalPreviousClip_1[3u];
        phi_127_ = (_e103 <= 0.000001f);
    }
    let _e106 = phi_127_;
    if _e106 {
        return;
    }
    let _e107 = temporalCurrentClip_1;
    let _e110 = temporalCurrentClip_1[3u];
    currentNdc = (_e107.xy / vec2(_e110));
    let _e113 = temporalPreviousClip_1;
    let _e116 = temporalPreviousClip_1[3u];
    previousNdc = (_e113.xy / vec2(_e116));
    let _e119 = currentNdc;
    param_2 = _e119;
    let _e120 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e121 = !(_e120);
    phi_156_ = _e121;
    if !(_e121) {
        let _e123 = previousNdc;
        param_3 = _e123;
        let _e124 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e124);
    }
    let _e127 = phi_156_;
    if _e127 {
        return;
    }
    let _e128 = currentNdc;
    currentUv = ((_e128 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e131 = previousNdc;
    previousUv = ((_e131 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e134 = currentUv;
    let _e135 = previousUv;
    velocity = (_e134 - _e135);
    let _e137 = velocity;
    param_4 = _e137;
    let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e138) {
        return;
    }
    let _e140 = velocity;
    out_temporal_velocity = _e140;
    let _e141 = (*coverageConfidence);
    out_temporal_validity = clamp(_e141, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e73 = (*rgb);
    let _e76 = unnamed.worldLightParams[0u];
    boosted = (_e73 * _e76);
    let _e79 = boosted[0u];
    let _e81 = boosted[1u];
    let _e83 = boosted[2u];
    peak = max(_e79, max(_e81, _e83));
    let _e86 = peak;
    if (_e86 > 1f) {
        let _e88 = peak;
        let _e89 = boosted;
        boosted = (_e89 / vec3(_e88));
    }
    let _e92 = boosted;
    return _e92;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_1 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_1;
        param_5 = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e117 = c_1;
        param_6 = _e117.xyz;
        let _e119 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var sampledColor1_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var sampledColor2_: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_17: f32;

    let _e97 = frag_color0In_1;
    param_7 = _e97.xyz;
    let _e99 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e101 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e99.x, _e99.y, _e99.z, _e101);
    param_8 = 0u;
    let _e106 = frag_tex_coord0_1;
    param_9 = _e106;
    param_10 = 0i;
    let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e107;
    param_11 = 1u;
    let _e108 = frag_tex_coord1_1;
    param_12 = _e108;
    param_13 = 1i;
    let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor1_ = _e109;
    param_14 = 2u;
    let _e110 = frag_tex_coord2_1;
    param_15 = _e110;
    param_16 = 2i;
    let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    sampledColor2_ = _e111;
    let _e112 = sampledColor0_;
    let _e113 = frag_color0_;
    color0_ = (_e112 * _e113);
    if override_type_3_ {
        let _e115 = sampledColor1_;
        color1_ = _e115;
        let _e116 = sampledColor2_;
        color2_ = _e116;
        let _e117 = color0_;
        let _e119 = color1_;
        let _e122 = color2_;
        let _e124 = ((_e117.xyz + _e119.xyz) + _e122.xyz);
        let _e126 = color0_[3u];
        let _e128 = color1_[3u];
        let _e131 = color2_[3u];
        base = vec4<f32>(_e124.x, _e124.y, _e124.z, ((_e126 * _e128) * _e131));
    } else {
        if override_type_3_1 {
            let _e137 = sampledColor1_;
            let _e138 = frag_color0_;
            color1_1 = (_e137 * _e138);
            let _e140 = sampledColor2_;
            let _e141 = frag_color0_;
            color2_1 = (_e140 * _e141);
            let _e143 = color0_;
            let _e145 = color1_1;
            let _e148 = color2_1;
            let _e150 = ((_e143.xyz + _e145.xyz) + _e148.xyz);
            let _e152 = color0_[3u];
            let _e154 = color1_1[3u];
            let _e157 = color2_1[3u];
            base = vec4<f32>(_e150.x, _e150.y, _e150.z, ((_e152 * _e154) * _e157));
        } else {
            let _e163 = sampledColor1_;
            color1_2 = _e163;
            let _e164 = sampledColor2_;
            color2_2 = _e164;
            let _e165 = color0_;
            let _e167 = color1_2;
            let _e170 = color2_2;
            let _e172 = ((_e165.xyz * _e167.xyz) * _e170.xyz);
            base[0u] = _e172.x;
            base[1u] = _e172.y;
            base[2u] = _e172.z;
            let _e180 = color0_[3u];
            let _e182 = color1_2[3u];
            let _e185 = color2_2[3u];
            base[3u] = ((_e180 * _e182) * _e185);
        }
    }
    if override_type_3_2 {
        let _e190 = unnamed.worldLightParams[1u];
        wetness = clamp(_e190, 0f, 1f);
        let _e194 = unnamed.worldLightParams[2u];
        frost = clamp(_e194, 0f, 1f);
        let _e196 = base;
        luminance = dot(_e196.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e199 = wetness;
        let _e201 = base;
        let _e203 = (_e201.xyz * mix(1f, 0.82f, _e199));
        base[0u] = _e203.x;
        base[1u] = _e203.y;
        base[2u] = _e203.z;
        let _e210 = base;
        let _e212 = luminance;
        let _e214 = luminance;
        let _e216 = luminance;
        let _e218 = frost;
        let _e221 = mix(_e210.xyz, vec3<f32>((_e212 * 0.88f), (_e214 * 0.94f), _e216), vec3((_e218 * 0.55f)));
        base[0u] = _e221.x;
        base[1u] = _e221.y;
        base[2u] = _e221.z;
    }
    let _e230 = unnamed.materialFactors[0u];
    if (_e230 >= 0f) {
        let _e233 = unnamed.materialBaseColor;
        let _e234 = base;
        base = (_e234 * _e233);
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
    param_17 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_17));
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
