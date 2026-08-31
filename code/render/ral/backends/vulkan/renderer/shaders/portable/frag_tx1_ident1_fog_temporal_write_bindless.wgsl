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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e76 = (*value);
    let _e77 = (*value);
    let _e79 = all((_e76 == _e77));
    phi_78_ = _e79;
    if _e79 {
        let _e80 = (*value);
        phi_78_ = all((abs(_e80) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e85 = phi_78_;
    return _e85;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e76 = (*value_1);
    let _e77 = (*value_1);
    let _e79 = all((_e76 == _e77));
    phi_63_ = _e79;
    if _e79 {
        let _e80 = (*value_1);
        phi_63_ = all((abs(_e80) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e85 = phi_63_;
    return _e85;
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
    let _e86 = temporalOutcome_1;
    let _e87 = (_e86 != 1u);
    phi_101_ = _e87;
    if !(_e87) {
        let _e89 = temporalCurrentClip_1;
        param = _e89;
        let _e90 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e90);
    }
    let _e93 = phi_101_;
    phi_110_ = _e93;
    if !(_e93) {
        let _e95 = temporalPreviousClip_1;
        param_1 = _e95;
        let _e96 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e96);
    }
    let _e99 = phi_110_;
    phi_120_ = _e99;
    if !(_e99) {
        let _e102 = temporalCurrentClip_1[3u];
        phi_120_ = (_e102 <= 0.000001f);
    }
    let _e105 = phi_120_;
    phi_127_ = _e105;
    if !(_e105) {
        let _e108 = temporalPreviousClip_1[3u];
        phi_127_ = (_e108 <= 0.000001f);
    }
    let _e111 = phi_127_;
    if _e111 {
        return;
    }
    let _e112 = temporalCurrentClip_1;
    let _e115 = temporalCurrentClip_1[3u];
    currentNdc = (_e112.xy / vec2(_e115));
    let _e118 = temporalPreviousClip_1;
    let _e121 = temporalPreviousClip_1[3u];
    previousNdc = (_e118.xy / vec2(_e121));
    let _e124 = currentNdc;
    param_2 = _e124;
    let _e125 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e126 = !(_e125);
    phi_156_ = _e126;
    if !(_e126) {
        let _e128 = previousNdc;
        param_3 = _e128;
        let _e129 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e129);
    }
    let _e132 = phi_156_;
    if _e132 {
        return;
    }
    let _e133 = currentNdc;
    currentUv = ((_e133 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e136 = previousNdc;
    previousUv = ((_e136 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e139 = currentUv;
    let _e140 = previousUv;
    velocity = (_e139 - _e140);
    let _e142 = velocity;
    param_4 = _e142;
    let _e143 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e143) {
        return;
    }
    let _e145 = velocity;
    out_temporal_velocity = _e145;
    let _e146 = (*coverageConfidence);
    out_temporal_validity = clamp(_e146, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e78 + 0.5f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e85 = fogType;
    let _e88 = fogType;
    return (((_e83 > 0.5f) && (_e85 >= 1i)) && (_e88 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e78 = wired_advanced_fog_enabled_u0028_();
    if !(_e78) {
        return 0f;
    }
    let _e81 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e81, 0.000001f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e86 + 0.5f));
    let _e89 = fogType_1;
    if (_e89 == 1i) {
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e93 <= 0f) {
            return 0f;
        }
        let _e95 = viewDepth;
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e95 / _e98), 0f, 1f);
    }
    let _e103 = unnamed.advancedFogColorDensity[3u];
    let _e105 = viewDepth;
    opticalDepth = (max(_e103, 0f) * _e105);
    let _e107 = fogType_1;
    if (_e107 == 2i) {
        let _e109 = opticalDepth;
        return clamp((1f - exp(-(_e109))), 0f, 1f);
    }
    let _e114 = opticalDepth;
    let _e115 = opticalDepth;
    return clamp((1f - exp(-((_e114 * _e115)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e78 = (*rgb);
    let _e81 = unnamed.worldLightParams[0u];
    boosted = (_e78 * _e81);
    let _e84 = boosted[0u];
    let _e86 = boosted[1u];
    let _e88 = boosted[2u];
    peak = max(_e84, max(_e86, _e88));
    let _e91 = peak;
    if (_e91 > 1f) {
        let _e93 = peak;
        let _e94 = boosted;
        boosted = (_e94 / vec3(_e93));
    }
    let _e97 = boosted;
    return _e97;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e79 = (*c);
    (*c) = max(_e79, vec3<f32>(0f, 0f, 0f));
    let _e81 = (*c);
    cutoff = (_e81 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e83 = (*c);
    lo = (_e83 / vec3(12.92f));
    let _e86 = (*c);
    hi = pow(((_e86 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e91 = hi;
    let _e92 = lo;
    let _e93 = cutoff;
    return mix(_e91, _e92, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e93));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e91 = (*role);
    let _e93 = (*role);
    let _e98 = unnamed.packed_indices[(_e91 / 4u)][(_e93 % 4u)];
    let _e103 = (*uv);
    let _e104 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    c_1 = _e104;
    let _e105 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e105))) == 0i) {
        let _e110 = c_1;
        param_5 = _e110.xyz;
        let _e112 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = (*slot);
    if (lightmap_slot == (_e119 + 1i)) {
        let _e122 = c_1;
        param_6 = _e122.xyz;
        let _e124 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = c_1;
    return _e131;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e101 = unnamed.packed_indices[0i][3u];
    let _e107 = unnamed.packed_indices[0i][3u];
    let _e112 = fog_tex_coord_1;
    let _e113 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    fog = _e113;
    param_7 = 0u;
    let _e114 = frag_tex_coord0_1;
    param_8 = _e114;
    param_9 = 0i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    color0_ = _e115;
    if override_type_3_ {
        param_10 = 1u;
        let _e116 = frag_tex_coord1_1;
        param_11 = _e116;
        param_12 = 1i;
        let _e117 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e117;
        let _e118 = color0_;
        let _e120 = color1_;
        let _e122 = (_e118.xyz + _e120.xyz);
        let _e124 = color0_[3u];
        let _e126 = color1_[3u];
        base = vec4<f32>(_e122.x, _e122.y, _e122.z, (_e124 * _e126));
    } else {
        if override_type_3_1 {
            param_13 = 1u;
            let _e132 = frag_tex_coord1_1;
            param_14 = _e132;
            param_15 = 1i;
            let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            color1_1 = _e133;
            let _e134 = color0_;
            let _e136 = color1_1;
            let _e138 = (_e134.xyz + _e136.xyz);
            let _e140 = color0_[3u];
            let _e142 = color1_1[3u];
            base = vec4<f32>(_e138.x, _e138.y, _e138.z, (_e140 * _e142));
        } else {
            param_16 = 1u;
            let _e148 = frag_tex_coord1_1;
            param_17 = _e148;
            param_18 = 1i;
            let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            color1_2 = _e149;
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
    if override_type_3_2 {
        let _e169 = unnamed.worldLightParams[1u];
        wetness = clamp(_e169, 0f, 1f);
        let _e173 = unnamed.worldLightParams[2u];
        frost = clamp(_e173, 0f, 1f);
        let _e175 = base;
        luminance = dot(_e175.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e178 = wetness;
        let _e180 = base;
        let _e182 = (_e180.xyz * mix(1f, 0.82f, _e178));
        base[0u] = _e182.x;
        base[1u] = _e182.y;
        base[2u] = _e182.z;
        let _e189 = base;
        let _e191 = luminance;
        let _e193 = luminance;
        let _e195 = luminance;
        let _e197 = frost;
        let _e200 = mix(_e189.xyz, vec3<f32>((_e191 * 0.88f), (_e193 * 0.94f), _e195), vec3((_e197 * 0.55f)));
        base[0u] = _e200.x;
        base[1u] = _e200.y;
        base[2u] = _e200.z;
    }
    let _e207 = color0_;
    let _e210 = unnamed.emissionRadiance;
    let _e213 = base;
    let _e215 = (_e213.xyz + (_e207.xyz * _e210.xyz));
    base[0u] = _e215.x;
    base[1u] = _e215.y;
    base[2u] = _e215.z;
    let _e222 = wired_advanced_fog_enabled_u0028_();
    if _e222 {
        let _e223 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e223;
        if override_type_3_3 {
            let _e224 = fogAmount;
            let _e226 = base;
            let _e228 = (_e226.xyz * (1f - _e224));
            base[0u] = _e228.x;
            base[1u] = _e228.y;
            base[2u] = _e228.z;
        } else {
            if override_type_3_4 {
                let _e235 = fogAmount;
                let _e237 = base;
                base = (_e237 * (1f - _e235));
            } else {
                if override_type_3_5 {
                    let _e239 = fogAmount;
                    let _e242 = base[3u];
                    base[3u] = (_e242 * (1f - _e239));
                } else {
                    let _e245 = base;
                    let _e248 = unnamed.advancedFogColorDensity;
                    let _e250 = fogAmount;
                    let _e252 = mix(_e245.xyz, _e248.xyz, vec3(_e250));
                    base[0u] = _e252.x;
                    base[1u] = _e252.y;
                    base[2u] = _e252.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e259 = base;
            let _e262 = fog[3u];
            let _e264 = (_e259.xyz * (1f - _e262));
            base[0u] = _e264.x;
            base[1u] = _e264.y;
            base[2u] = _e264.z;
        } else {
            if override_type_3_7 {
                let _e271 = base;
                let _e273 = fog[3u];
                base = (_e271 * (1f - _e273));
            } else {
                if override_type_3_8 {
                    let _e277 = base[3u];
                    let _e279 = fog[3u];
                    base[3u] = (_e277 * (1f - _e279));
                } else {
                    let _e283 = base;
                    let _e284 = fog;
                    let _e286 = unnamed.fogColor;
                    let _e289 = fog[3u];
                    base = mix(_e283, (_e284 * _e286), vec4(_e289));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e293 = base[3u];
        if (_e293 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e295 = base;
            let _e297 = base;
            if (dot(_e295.xyz, _e297.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e301 = base;
    out_color = _e301;
    param_19 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_19));
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
