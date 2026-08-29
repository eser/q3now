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

@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
override override_type_3_3: bool = (alpha_test_func == 1i);
override override_type_3_4: bool = (alpha_test_func == 2i);
override override_type_3_5: bool = (alpha_test_func == 3i);
override override_type_3_6: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
override override_type_3_10: bool = (acff == 1i);
override override_type_3_11: bool = (acff == 2i);
override override_type_3_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_13: bool = (discard_mode == 1i);
override override_type_3_14: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e81 = (*value);
    let _e82 = (*value);
    let _e84 = all((_e81 == _e82));
    phi_79_ = _e84;
    if _e84 {
        let _e85 = (*value);
        phi_79_ = all((abs(_e85) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e90 = phi_79_;
    return _e90;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e81 = (*value_1);
    let _e82 = (*value_1);
    let _e84 = all((_e81 == _e82));
    phi_64_ = _e84;
    if _e84 {
        let _e85 = (*value_1);
        phi_64_ = all((abs(_e85) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e90 = phi_64_;
    return _e90;
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
    var phi_102_: bool;
    var phi_111_: bool;
    var phi_121_: bool;
    var phi_128_: bool;
    var phi_157_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e91 = temporalOutcome_1;
    let _e92 = (_e91 != 1u);
    phi_102_ = _e92;
    if !(_e92) {
        let _e94 = temporalCurrentClip_1;
        param = _e94;
        let _e95 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e95);
    }
    let _e98 = phi_102_;
    phi_111_ = _e98;
    if !(_e98) {
        let _e100 = temporalPreviousClip_1;
        param_1 = _e100;
        let _e101 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e101);
    }
    let _e104 = phi_111_;
    phi_121_ = _e104;
    if !(_e104) {
        let _e107 = temporalCurrentClip_1[3u];
        phi_121_ = (_e107 <= 0.000001f);
    }
    let _e110 = phi_121_;
    phi_128_ = _e110;
    if !(_e110) {
        let _e113 = temporalPreviousClip_1[3u];
        phi_128_ = (_e113 <= 0.000001f);
    }
    let _e116 = phi_128_;
    if _e116 {
        return;
    }
    let _e117 = temporalCurrentClip_1;
    let _e120 = temporalCurrentClip_1[3u];
    currentNdc = (_e117.xy / vec2(_e120));
    let _e123 = temporalPreviousClip_1;
    let _e126 = temporalPreviousClip_1[3u];
    previousNdc = (_e123.xy / vec2(_e126));
    let _e129 = currentNdc;
    param_2 = _e129;
    let _e130 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e131 = !(_e130);
    phi_157_ = _e131;
    if !(_e131) {
        let _e133 = previousNdc;
        param_3 = _e133;
        let _e134 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e134);
    }
    let _e137 = phi_157_;
    if _e137 {
        return;
    }
    let _e138 = currentNdc;
    currentUv = ((_e138 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e141 = previousNdc;
    previousUv = ((_e141 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e144 = currentUv;
    let _e145 = previousUv;
    velocity = (_e144 - _e145);
    let _e147 = velocity;
    param_4 = _e147;
    let _e148 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e148) {
        return;
    }
    let _e150 = velocity;
    out_temporal_velocity = _e150;
    let _e151 = (*coverageConfidence);
    out_temporal_validity = clamp(_e151, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e85 = (*alpha);
        let _e88 = span;
        return clamp((abs((_e85 - alpha_test_value)) / _e88), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e91 = (*alpha);
            return clamp(((alpha_test_value - _e91) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e96 = (*alpha);
                return clamp(((_e96 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e83 + 0.5f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e90 = fogType;
    let _e93 = fogType;
    return (((_e88 > 0.5f) && (_e90 >= 1i)) && (_e93 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e83 = wired_advanced_fog_enabled_u0028_();
    if !(_e83) {
        return 0f;
    }
    let _e86 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e86, 0.000001f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e91 + 0.5f));
    let _e94 = fogType_1;
    if (_e94 == 1i) {
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e98 <= 0f) {
            return 0f;
        }
        let _e100 = viewDepth;
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e100 / _e103), 0f, 1f);
    }
    let _e108 = unnamed.advancedFogColorDensity[3u];
    let _e110 = viewDepth;
    opticalDepth = (max(_e108, 0f) * _e110);
    let _e112 = fogType_1;
    if (_e112 == 2i) {
        let _e114 = opticalDepth;
        return clamp((1f - exp(-(_e114))), 0f, 1f);
    }
    let _e119 = opticalDepth;
    let _e120 = opticalDepth;
    return clamp((1f - exp(-((_e119 * _e120)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e84 = (*c);
    (*c) = max(_e84, vec3<f32>(0f, 0f, 0f));
    let _e86 = (*c);
    cutoff = (_e86 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e88 = (*c);
    lo = (_e88 / vec3(12.92f));
    let _e91 = (*c);
    hi = pow(((_e91 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e96 = hi;
    let _e97 = lo;
    let _e98 = cutoff;
    return mix(_e96, _e97, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e98));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

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
        let _e128 = unnamed.worldLightParams[0u];
        let _e129 = c_1;
        let _e131 = (_e129.xyz * _e128);
        c_1[0u] = _e131.x;
        c_1[1u] = _e131.y;
        c_1[2u] = _e131.z;
    }
    let _e138 = c_1;
    return _e138;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_10: f32;
    var param_11: f32;

    let _e97 = unnamed.packed_indices[0i][3u];
    let _e103 = unnamed.packed_indices[0i][3u];
    let _e108 = fog_tex_coord_1;
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    fog = _e109;
    let _e110 = frag_color0In_1;
    param_6 = _e110.xyz;
    let _e112 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e114 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e112.x, _e112.y, _e112.z, _e114);
    param_7 = 0u;
    let _e119 = frag_tex_coord0_1;
    param_8 = _e119;
    param_9 = 0i;
    let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e121 = frag_color0_;
    color0_ = (_e120 * _e121);
    let _e123 = color0_;
    base = _e123;
    if override_type_3_3 {
        let _e125 = color0_[3u];
        if (_e125 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e128 = color0_[3u];
            if (_e128 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e131 = color0_[3u];
                if (_e131 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e133 = color0_;
    base = _e133;
    if override_type_3_6 {
        let _e136 = unnamed.worldLightParams[1u];
        wetness = clamp(_e136, 0f, 1f);
        let _e140 = unnamed.worldLightParams[2u];
        frost = clamp(_e140, 0f, 1f);
        let _e142 = base;
        luminance = dot(_e142.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e145 = wetness;
        let _e147 = base;
        let _e149 = (_e147.xyz * mix(1f, 0.82f, _e145));
        base[0u] = _e149.x;
        base[1u] = _e149.y;
        base[2u] = _e149.z;
        let _e156 = base;
        let _e158 = luminance;
        let _e160 = luminance;
        let _e162 = luminance;
        let _e164 = frost;
        let _e167 = mix(_e156.xyz, vec3<f32>((_e158 * 0.88f), (_e160 * 0.94f), _e162), vec3((_e164 * 0.55f)));
        base[0u] = _e167.x;
        base[1u] = _e167.y;
        base[2u] = _e167.z;
    }
    let _e174 = color0_;
    let _e177 = unnamed.emissionRadiance;
    let _e180 = base;
    let _e182 = (_e180.xyz + (_e174.xyz * _e177.xyz));
    base[0u] = _e182.x;
    base[1u] = _e182.y;
    base[2u] = _e182.z;
    let _e189 = wired_advanced_fog_enabled_u0028_();
    if _e189 {
        let _e190 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e190;
        if override_type_3_7 {
            let _e191 = fogAmount;
            let _e193 = base;
            let _e195 = (_e193.xyz * (1f - _e191));
            base[0u] = _e195.x;
            base[1u] = _e195.y;
            base[2u] = _e195.z;
        } else {
            if override_type_3_8 {
                let _e202 = fogAmount;
                let _e204 = base;
                base = (_e204 * (1f - _e202));
            } else {
                if override_type_3_9 {
                    let _e206 = fogAmount;
                    let _e209 = base[3u];
                    base[3u] = (_e209 * (1f - _e206));
                } else {
                    let _e212 = base;
                    let _e215 = unnamed.advancedFogColorDensity;
                    let _e217 = fogAmount;
                    let _e219 = mix(_e212.xyz, _e215.xyz, vec3(_e217));
                    base[0u] = _e219.x;
                    base[1u] = _e219.y;
                    base[2u] = _e219.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e226 = base;
            let _e229 = fog[3u];
            let _e231 = (_e226.xyz * (1f - _e229));
            base[0u] = _e231.x;
            base[1u] = _e231.y;
            base[2u] = _e231.z;
        } else {
            if override_type_3_11 {
                let _e238 = base;
                let _e240 = fog[3u];
                base = (_e238 * (1f - _e240));
            } else {
                if override_type_3_12 {
                    let _e244 = base[3u];
                    let _e246 = fog[3u];
                    base[3u] = (_e244 * (1f - _e246));
                } else {
                    let _e250 = base;
                    let _e251 = fog;
                    let _e253 = unnamed.fogColor;
                    let _e256 = fog[3u];
                    base = mix(_e250, (_e251 * _e253), vec4(_e256));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e260 = base[3u];
        if (_e260 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e262 = base;
            let _e264 = base;
            if (dot(_e262.xyz, _e264.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e268 = base;
    out_color = _e268;
    let _e270 = color0_[3u];
    param_10 = _e270;
    let _e271 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_10));
    param_11 = _e271;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_11));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
