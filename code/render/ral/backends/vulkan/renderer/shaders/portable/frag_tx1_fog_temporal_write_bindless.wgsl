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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e77 = (*value);
    let _e78 = (*value);
    let _e80 = all((_e77 == _e78));
    phi_78_ = _e80;
    if _e80 {
        let _e81 = (*value);
        phi_78_ = all((abs(_e81) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e86 = phi_78_;
    return _e86;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e77 = (*value_1);
    let _e78 = (*value_1);
    let _e80 = all((_e77 == _e78));
    phi_63_ = _e80;
    if _e80 {
        let _e81 = (*value_1);
        phi_63_ = all((abs(_e81) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e86 = phi_63_;
    return _e86;
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
    let _e87 = temporalOutcome_1;
    let _e88 = (_e87 != 1u);
    phi_101_ = _e88;
    if !(_e88) {
        let _e90 = temporalCurrentClip_1;
        param = _e90;
        let _e91 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e91);
    }
    let _e94 = phi_101_;
    phi_110_ = _e94;
    if !(_e94) {
        let _e96 = temporalPreviousClip_1;
        param_1 = _e96;
        let _e97 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e97);
    }
    let _e100 = phi_110_;
    phi_120_ = _e100;
    if !(_e100) {
        let _e103 = temporalCurrentClip_1[3u];
        phi_120_ = (_e103 <= 0.000001f);
    }
    let _e106 = phi_120_;
    phi_127_ = _e106;
    if !(_e106) {
        let _e109 = temporalPreviousClip_1[3u];
        phi_127_ = (_e109 <= 0.000001f);
    }
    let _e112 = phi_127_;
    if _e112 {
        return;
    }
    let _e113 = temporalCurrentClip_1;
    let _e116 = temporalCurrentClip_1[3u];
    currentNdc = (_e113.xy / vec2(_e116));
    let _e119 = temporalPreviousClip_1;
    let _e122 = temporalPreviousClip_1[3u];
    previousNdc = (_e119.xy / vec2(_e122));
    let _e125 = currentNdc;
    param_2 = _e125;
    let _e126 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e127 = !(_e126);
    phi_156_ = _e127;
    if !(_e127) {
        let _e129 = previousNdc;
        param_3 = _e129;
        let _e130 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e130);
    }
    let _e133 = phi_156_;
    if _e133 {
        return;
    }
    let _e134 = currentNdc;
    currentUv = ((_e134 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e137 = previousNdc;
    previousUv = ((_e137 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e140 = currentUv;
    let _e141 = previousUv;
    velocity = (_e140 - _e141);
    let _e143 = velocity;
    param_4 = _e143;
    let _e144 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e144) {
        return;
    }
    let _e146 = velocity;
    out_temporal_velocity = _e146;
    let _e147 = (*coverageConfidence);
    out_temporal_validity = clamp(_e147, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e79 + 0.5f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e86 = fogType;
    let _e89 = fogType;
    return (((_e84 > 0.5f) && (_e86 >= 1i)) && (_e89 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e79 = wired_advanced_fog_enabled_u0028_();
    if !(_e79) {
        return 0f;
    }
    let _e82 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e82, 0.000001f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e87 + 0.5f));
    let _e90 = fogType_1;
    if (_e90 == 1i) {
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e94 <= 0f) {
            return 0f;
        }
        let _e96 = viewDepth;
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e96 / _e99), 0f, 1f);
    }
    let _e104 = unnamed.advancedFogColorDensity[3u];
    let _e106 = viewDepth;
    opticalDepth = (max(_e104, 0f) * _e106);
    let _e108 = fogType_1;
    if (_e108 == 2i) {
        let _e110 = opticalDepth;
        return clamp((1f - exp(-(_e110))), 0f, 1f);
    }
    let _e115 = opticalDepth;
    let _e116 = opticalDepth;
    return clamp((1f - exp(-((_e115 * _e116)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e79 = (*rgb);
    let _e82 = unnamed.worldLightParams[0u];
    boosted = (_e79 * _e82);
    let _e85 = boosted[0u];
    let _e87 = boosted[1u];
    let _e89 = boosted[2u];
    peak = max(_e85, max(_e87, _e89));
    let _e92 = peak;
    if (_e92 > 1f) {
        let _e94 = peak;
        let _e95 = boosted;
        boosted = (_e95 / vec3(_e94));
    }
    let _e98 = boosted;
    return _e98;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e80 = (*c);
    (*c) = max(_e80, vec3<f32>(0f, 0f, 0f));
    let _e82 = (*c);
    cutoff = (_e82 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e84 = (*c);
    lo = (_e84 / vec3(12.92f));
    let _e87 = (*c);
    hi = pow(((_e87 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e92 = hi;
    let _e93 = lo;
    let _e94 = cutoff;
    return mix(_e92, _e93, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e94));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e104 = (*uv);
    let _e105 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    c_1 = _e105;
    let _e106 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e106))) == 0i) {
        let _e111 = c_1;
        param_5 = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e123 = c_1;
        param_6 = _e123.xyz;
        let _e125 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = c_1;
    return _e132;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e104 = unnamed.packed_indices[0i][3u];
    let _e110 = unnamed.packed_indices[0i][3u];
    let _e115 = fog_tex_coord_1;
    let _e116 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    fog = _e116;
    let _e117 = frag_color0In_1;
    param_7 = _e117.xyz;
    let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e121 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e119.x, _e119.y, _e119.z, _e121);
    param_8 = 0u;
    let _e126 = frag_tex_coord0_1;
    param_9 = _e126;
    param_10 = 0i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    let _e128 = frag_color0_;
    color0_ = (_e127 * _e128);
    if override_type_3_ {
        param_11 = 1u;
        let _e130 = frag_tex_coord1_1;
        param_12 = _e130;
        param_13 = 1i;
        let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
        color1_ = _e131;
        let _e132 = color0_;
        let _e134 = color1_;
        let _e136 = (_e132.xyz + _e134.xyz);
        let _e138 = color0_[3u];
        let _e140 = color1_[3u];
        base = vec4<f32>(_e136.x, _e136.y, _e136.z, (_e138 * _e140));
    } else {
        if override_type_3_1 {
            param_14 = 1u;
            let _e146 = frag_tex_coord1_1;
            param_15 = _e146;
            param_16 = 1i;
            let _e147 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e148 = frag_color0_;
            color1_1 = (_e147 * _e148);
            let _e150 = color0_;
            let _e152 = color1_1;
            let _e154 = (_e150.xyz + _e152.xyz);
            let _e156 = color0_[3u];
            let _e158 = color1_1[3u];
            base = vec4<f32>(_e154.x, _e154.y, _e154.z, (_e156 * _e158));
        } else {
            param_17 = 1u;
            let _e164 = frag_tex_coord1_1;
            param_18 = _e164;
            param_19 = 1i;
            let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e165;
            let _e166 = color0_;
            let _e168 = color1_2;
            let _e170 = (_e166.xyz * _e168.xyz);
            base[0u] = _e170.x;
            base[1u] = _e170.y;
            base[2u] = _e170.z;
            let _e178 = color0_[3u];
            let _e180 = color1_2[3u];
            base[3u] = (_e178 * _e180);
        }
    }
    if override_type_3_2 {
        let _e185 = unnamed.worldLightParams[1u];
        wetness = clamp(_e185, 0f, 1f);
        let _e189 = unnamed.worldLightParams[2u];
        frost = clamp(_e189, 0f, 1f);
        let _e191 = base;
        luminance = dot(_e191.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e194 = wetness;
        let _e196 = base;
        let _e198 = (_e196.xyz * mix(1f, 0.82f, _e194));
        base[0u] = _e198.x;
        base[1u] = _e198.y;
        base[2u] = _e198.z;
        let _e205 = base;
        let _e207 = luminance;
        let _e209 = luminance;
        let _e211 = luminance;
        let _e213 = frost;
        let _e216 = mix(_e205.xyz, vec3<f32>((_e207 * 0.88f), (_e209 * 0.94f), _e211), vec3((_e213 * 0.55f)));
        base[0u] = _e216.x;
        base[1u] = _e216.y;
        base[2u] = _e216.z;
    }
    let _e223 = color0_;
    let _e226 = unnamed.emissionRadiance;
    let _e229 = base;
    let _e231 = (_e229.xyz + (_e223.xyz * _e226.xyz));
    base[0u] = _e231.x;
    base[1u] = _e231.y;
    base[2u] = _e231.z;
    let _e238 = wired_advanced_fog_enabled_u0028_();
    if _e238 {
        let _e239 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e239;
        if override_type_3_3 {
            let _e240 = fogAmount;
            let _e242 = base;
            let _e244 = (_e242.xyz * (1f - _e240));
            base[0u] = _e244.x;
            base[1u] = _e244.y;
            base[2u] = _e244.z;
        } else {
            if override_type_3_4 {
                let _e251 = fogAmount;
                let _e253 = base;
                base = (_e253 * (1f - _e251));
            } else {
                if override_type_3_5 {
                    let _e255 = fogAmount;
                    let _e258 = base[3u];
                    base[3u] = (_e258 * (1f - _e255));
                } else {
                    let _e261 = base;
                    let _e264 = unnamed.advancedFogColorDensity;
                    let _e266 = fogAmount;
                    let _e268 = mix(_e261.xyz, _e264.xyz, vec3(_e266));
                    base[0u] = _e268.x;
                    base[1u] = _e268.y;
                    base[2u] = _e268.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e275 = base;
            let _e278 = fog[3u];
            let _e280 = (_e275.xyz * (1f - _e278));
            base[0u] = _e280.x;
            base[1u] = _e280.y;
            base[2u] = _e280.z;
        } else {
            if override_type_3_7 {
                let _e287 = base;
                let _e289 = fog[3u];
                base = (_e287 * (1f - _e289));
            } else {
                if override_type_3_8 {
                    let _e293 = base[3u];
                    let _e295 = fog[3u];
                    base[3u] = (_e293 * (1f - _e295));
                } else {
                    let _e299 = base;
                    let _e300 = fog;
                    let _e302 = unnamed.fogColor;
                    let _e305 = fog[3u];
                    base = mix(_e299, (_e300 * _e302), vec4(_e305));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e309 = base[3u];
        if (_e309 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e311 = base;
            let _e313 = base;
            if (dot(_e311.xyz, _e313.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e317 = base;
    out_color = _e317;
    param_20 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_20));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
