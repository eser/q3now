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
@id(10) override acff: i32 = 0i;
override override_type_3_3: bool = (acff == 1i);
override override_type_3_4: bool = (acff == 2i);
override override_type_3_5: bool = (acff == 3i);
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e80 = (*value);
    let _e81 = (*value);
    let _e83 = all((_e80 == _e81));
    phi_78_ = _e83;
    if _e83 {
        let _e84 = (*value);
        phi_78_ = all((abs(_e84) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e89 = phi_78_;
    return _e89;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e80 = (*value_1);
    let _e81 = (*value_1);
    let _e83 = all((_e80 == _e81));
    phi_63_ = _e83;
    if _e83 {
        let _e84 = (*value_1);
        phi_63_ = all((abs(_e84) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e89 = phi_63_;
    return _e89;
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
    let _e90 = temporalOutcome_1;
    let _e91 = (_e90 != 1u);
    phi_101_ = _e91;
    if !(_e91) {
        let _e93 = temporalCurrentClip_1;
        param = _e93;
        let _e94 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e94);
    }
    let _e97 = phi_101_;
    phi_110_ = _e97;
    if !(_e97) {
        let _e99 = temporalPreviousClip_1;
        param_1 = _e99;
        let _e100 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e100);
    }
    let _e103 = phi_110_;
    phi_120_ = _e103;
    if !(_e103) {
        let _e106 = temporalCurrentClip_1[3u];
        phi_120_ = (_e106 <= 0.000001f);
    }
    let _e109 = phi_120_;
    phi_127_ = _e109;
    if !(_e109) {
        let _e112 = temporalPreviousClip_1[3u];
        phi_127_ = (_e112 <= 0.000001f);
    }
    let _e115 = phi_127_;
    if _e115 {
        return;
    }
    let _e116 = temporalCurrentClip_1;
    let _e119 = temporalCurrentClip_1[3u];
    currentNdc = (_e116.xy / vec2(_e119));
    let _e122 = temporalPreviousClip_1;
    let _e125 = temporalPreviousClip_1[3u];
    previousNdc = (_e122.xy / vec2(_e125));
    let _e128 = currentNdc;
    param_2 = _e128;
    let _e129 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e130 = !(_e129);
    phi_156_ = _e130;
    if !(_e130) {
        let _e132 = previousNdc;
        param_3 = _e132;
        let _e133 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e133);
    }
    let _e136 = phi_156_;
    if _e136 {
        return;
    }
    let _e137 = currentNdc;
    currentUv = ((_e137 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e140 = previousNdc;
    previousUv = ((_e140 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e143 = currentUv;
    let _e144 = previousUv;
    velocity = (_e143 - _e144);
    let _e146 = velocity;
    param_4 = _e146;
    let _e147 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e147) {
        return;
    }
    let _e149 = velocity;
    out_temporal_velocity = _e149;
    let _e150 = (*coverageConfidence);
    out_temporal_validity = clamp(_e150, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e82 + 0.5f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e89 = fogType;
    let _e92 = fogType;
    return (((_e87 > 0.5f) && (_e89 >= 1i)) && (_e92 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e82 = wired_advanced_fog_enabled_u0028_();
    if !(_e82) {
        return 0f;
    }
    let _e85 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e85, 0.000001f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e90 + 0.5f));
    let _e93 = fogType_1;
    if (_e93 == 1i) {
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e97 <= 0f) {
            return 0f;
        }
        let _e99 = viewDepth;
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e99 / _e102), 0f, 1f);
    }
    let _e107 = unnamed.advancedFogColorDensity[3u];
    let _e109 = viewDepth;
    opticalDepth = (max(_e107, 0f) * _e109);
    let _e111 = fogType_1;
    if (_e111 == 2i) {
        let _e113 = opticalDepth;
        return clamp((1f - exp(-(_e113))), 0f, 1f);
    }
    let _e118 = opticalDepth;
    let _e119 = opticalDepth;
    return clamp((1f - exp(-((_e118 * _e119)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e82 = (*rgb);
    let _e85 = unnamed.worldLightParams[0u];
    boosted = (_e82 * _e85);
    let _e88 = boosted[0u];
    let _e90 = boosted[1u];
    let _e92 = boosted[2u];
    peak = max(_e88, max(_e90, _e92));
    let _e95 = peak;
    if (_e95 > 1f) {
        let _e97 = peak;
        let _e98 = boosted;
        boosted = (_e98 / vec3(_e97));
    }
    let _e101 = boosted;
    return _e101;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e83 = (*c);
    (*c) = max(_e83, vec3<f32>(0f, 0f, 0f));
    let _e85 = (*c);
    cutoff = (_e85 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e87 = (*c);
    lo = (_e87 / vec3(12.92f));
    let _e90 = (*c);
    hi = pow(((_e90 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e95 = hi;
    let _e96 = lo;
    let _e97 = cutoff;
    return mix(_e95, _e96, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e97));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e95 = (*role);
    let _e97 = (*role);
    let _e102 = unnamed.packed_indices[(_e95 / 4u)][(_e97 % 4u)];
    let _e107 = (*uv);
    let _e108 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e102 >> bitcast<u32>(12i)) & 255u)], _e107);
    c_1 = _e108;
    let _e109 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e109))) == 0i) {
        let _e114 = c_1;
        param_5 = _e114.xyz;
        let _e116 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = (*slot);
    if (lightmap_slot == (_e123 + 1i)) {
        let _e126 = c_1;
        param_6 = _e126.xyz;
        let _e128 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = c_1;
    return _e135;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e110 = unnamed.packed_indices[0i][3u];
    let _e116 = unnamed.packed_indices[0i][3u];
    let _e121 = fog_tex_coord_1;
    let _e122 = textureSample(wired_bindless_images[(_e110 & 4095u)], wired_bindless_samplers[((_e116 >> bitcast<u32>(12i)) & 255u)], _e121);
    fog = _e122;
    let _e123 = frag_color0In_1;
    param_7 = _e123.xyz;
    let _e125 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e127 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e125.x, _e125.y, _e125.z, _e127);
    param_8 = 0u;
    let _e132 = frag_tex_coord0_1;
    param_9 = _e132;
    param_10 = 0i;
    let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e133;
    param_11 = 1u;
    let _e134 = frag_tex_coord1_1;
    param_12 = _e134;
    param_13 = 1i;
    let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor1_ = _e135;
    param_14 = 2u;
    let _e136 = frag_tex_coord2_1;
    param_15 = _e136;
    param_16 = 2i;
    let _e137 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    sampledColor2_ = _e137;
    let _e138 = sampledColor0_;
    let _e139 = frag_color0_;
    color0_ = (_e138 * _e139);
    if override_type_3_ {
        let _e141 = sampledColor1_;
        color1_ = _e141;
        let _e142 = sampledColor2_;
        color2_ = _e142;
        let _e143 = color0_;
        let _e145 = color1_;
        let _e148 = color2_;
        let _e150 = ((_e143.xyz + _e145.xyz) + _e148.xyz);
        let _e152 = color0_[3u];
        let _e154 = color1_[3u];
        let _e157 = color2_[3u];
        base = vec4<f32>(_e150.x, _e150.y, _e150.z, ((_e152 * _e154) * _e157));
    } else {
        if override_type_3_1 {
            let _e163 = sampledColor1_;
            let _e164 = frag_color0_;
            color1_1 = (_e163 * _e164);
            let _e166 = sampledColor2_;
            let _e167 = frag_color0_;
            color2_1 = (_e166 * _e167);
            let _e169 = color0_;
            let _e171 = color1_1;
            let _e174 = color2_1;
            let _e176 = ((_e169.xyz + _e171.xyz) + _e174.xyz);
            let _e178 = color0_[3u];
            let _e180 = color1_1[3u];
            let _e183 = color2_1[3u];
            base = vec4<f32>(_e176.x, _e176.y, _e176.z, ((_e178 * _e180) * _e183));
        } else {
            let _e189 = sampledColor1_;
            color1_2 = _e189;
            let _e190 = sampledColor2_;
            color2_2 = _e190;
            let _e191 = color0_;
            let _e193 = color1_2;
            let _e196 = color2_2;
            let _e198 = ((_e191.xyz * _e193.xyz) * _e196.xyz);
            base[0u] = _e198.x;
            base[1u] = _e198.y;
            base[2u] = _e198.z;
            let _e206 = color0_[3u];
            let _e208 = color1_2[3u];
            let _e211 = color2_2[3u];
            base[3u] = ((_e206 * _e208) * _e211);
        }
    }
    if override_type_3_2 {
        let _e216 = unnamed.worldLightParams[1u];
        wetness = clamp(_e216, 0f, 1f);
        let _e220 = unnamed.worldLightParams[2u];
        frost = clamp(_e220, 0f, 1f);
        let _e222 = base;
        luminance = dot(_e222.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e225 = wetness;
        let _e227 = base;
        let _e229 = (_e227.xyz * mix(1f, 0.82f, _e225));
        base[0u] = _e229.x;
        base[1u] = _e229.y;
        base[2u] = _e229.z;
        let _e236 = base;
        let _e238 = luminance;
        let _e240 = luminance;
        let _e242 = luminance;
        let _e244 = frost;
        let _e247 = mix(_e236.xyz, vec3<f32>((_e238 * 0.88f), (_e240 * 0.94f), _e242), vec3((_e244 * 0.55f)));
        base[0u] = _e247.x;
        base[1u] = _e247.y;
        base[2u] = _e247.z;
    }
    let _e256 = unnamed.materialFactors[0u];
    if (_e256 >= 0f) {
        let _e259 = unnamed.materialBaseColor;
        let _e260 = base;
        base = (_e260 * _e259);
    }
    let _e262 = color0_;
    let _e265 = unnamed.emissionRadiance;
    let _e268 = base;
    let _e270 = (_e268.xyz + (_e262.xyz * _e265.xyz));
    base[0u] = _e270.x;
    base[1u] = _e270.y;
    base[2u] = _e270.z;
    let _e277 = wired_advanced_fog_enabled_u0028_();
    if _e277 {
        let _e278 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e278;
        if override_type_3_3 {
            let _e279 = fogAmount;
            let _e281 = base;
            let _e283 = (_e281.xyz * (1f - _e279));
            base[0u] = _e283.x;
            base[1u] = _e283.y;
            base[2u] = _e283.z;
        } else {
            if override_type_3_4 {
                let _e290 = fogAmount;
                let _e292 = base;
                base = (_e292 * (1f - _e290));
            } else {
                if override_type_3_5 {
                    let _e294 = fogAmount;
                    let _e297 = base[3u];
                    base[3u] = (_e297 * (1f - _e294));
                } else {
                    let _e300 = base;
                    let _e303 = unnamed.advancedFogColorDensity;
                    let _e305 = fogAmount;
                    let _e307 = mix(_e300.xyz, _e303.xyz, vec3(_e305));
                    base[0u] = _e307.x;
                    base[1u] = _e307.y;
                    base[2u] = _e307.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e314 = base;
            let _e317 = fog[3u];
            let _e319 = (_e314.xyz * (1f - _e317));
            base[0u] = _e319.x;
            base[1u] = _e319.y;
            base[2u] = _e319.z;
        } else {
            if override_type_3_7 {
                let _e326 = base;
                let _e328 = fog[3u];
                base = (_e326 * (1f - _e328));
            } else {
                if override_type_3_8 {
                    let _e332 = base[3u];
                    let _e334 = fog[3u];
                    base[3u] = (_e332 * (1f - _e334));
                } else {
                    let _e338 = base;
                    let _e339 = fog;
                    let _e341 = unnamed.fogColor;
                    let _e344 = fog[3u];
                    base = mix(_e338, (_e339 * _e341), vec4(_e344));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e348 = base[3u];
        if (_e348 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e350 = base;
            let _e352 = base;
            if (dot(_e350.xyz, _e352.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e356 = base;
    out_color = _e356;
    param_17 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_17));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
