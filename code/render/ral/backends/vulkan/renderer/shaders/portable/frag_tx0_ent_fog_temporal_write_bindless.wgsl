enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    ent_color0_: vec4<f32>,
    ent_color1_: vec4<f32>,
    ent_color2_: vec4<f32>,
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
@id(10) override acff: i32 = 0i;
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_12: bool = (discard_mode == 1i);
override override_type_3_13: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e70 = (*value);
    let _e71 = (*value);
    let _e73 = all((_e70 == _e71));
    phi_79_ = _e73;
    if _e73 {
        let _e74 = (*value);
        phi_79_ = all((abs(_e74) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e79 = phi_79_;
    return _e79;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e70 = (*value_1);
    let _e71 = (*value_1);
    let _e73 = all((_e70 == _e71));
    phi_64_ = _e73;
    if _e73 {
        let _e74 = (*value_1);
        phi_64_ = all((abs(_e74) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e79 = phi_64_;
    return _e79;
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
    let _e80 = temporalOutcome_1;
    let _e81 = (_e80 != 1u);
    phi_102_ = _e81;
    if !(_e81) {
        let _e83 = temporalCurrentClip_1;
        param = _e83;
        let _e84 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_102_ = !(_e84);
    }
    let _e87 = phi_102_;
    phi_111_ = _e87;
    if !(_e87) {
        let _e89 = temporalPreviousClip_1;
        param_1 = _e89;
        let _e90 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_111_ = !(_e90);
    }
    let _e93 = phi_111_;
    phi_121_ = _e93;
    if !(_e93) {
        let _e96 = temporalCurrentClip_1[3u];
        phi_121_ = (_e96 <= 0.000001f);
    }
    let _e99 = phi_121_;
    phi_128_ = _e99;
    if !(_e99) {
        let _e102 = temporalPreviousClip_1[3u];
        phi_128_ = (_e102 <= 0.000001f);
    }
    let _e105 = phi_128_;
    if _e105 {
        return;
    }
    let _e106 = temporalCurrentClip_1;
    let _e109 = temporalCurrentClip_1[3u];
    currentNdc = (_e106.xy / vec2(_e109));
    let _e112 = temporalPreviousClip_1;
    let _e115 = temporalPreviousClip_1[3u];
    previousNdc = (_e112.xy / vec2(_e115));
    let _e118 = currentNdc;
    param_2 = _e118;
    let _e119 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e120 = !(_e119);
    phi_157_ = _e120;
    if !(_e120) {
        let _e122 = previousNdc;
        param_3 = _e122;
        let _e123 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_157_ = !(_e123);
    }
    let _e126 = phi_157_;
    if _e126 {
        return;
    }
    let _e127 = currentNdc;
    currentUv = ((_e127 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e130 = previousNdc;
    previousUv = ((_e130 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e133 = currentUv;
    let _e134 = previousUv;
    velocity = (_e133 - _e134);
    let _e136 = velocity;
    param_4 = _e136;
    let _e137 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e137) {
        return;
    }
    let _e139 = velocity;
    out_temporal_velocity = _e139;
    let _e140 = (*coverageConfidence);
    out_temporal_validity = clamp(_e140, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e74 = (*alpha);
        let _e77 = span;
        return clamp((abs((_e74 - alpha_test_value)) / _e77), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e80 = (*alpha);
            return clamp(((alpha_test_value - _e80) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e85 = (*alpha);
                return clamp(((_e85 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e72 + 0.5f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e79 = fogType;
    let _e82 = fogType;
    return (((_e77 > 0.5f) && (_e79 >= 1i)) && (_e82 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e72 = wired_advanced_fog_enabled_u0028_();
    if !(_e72) {
        return 0f;
    }
    let _e75 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e75, 0.000001f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e80 + 0.5f));
    let _e83 = fogType_1;
    if (_e83 == 1i) {
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e87 <= 0f) {
            return 0f;
        }
        let _e89 = viewDepth;
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e89 / _e92), 0f, 1f);
    }
    let _e97 = unnamed.advancedFogColorDensity[3u];
    let _e99 = viewDepth;
    opticalDepth = (max(_e97, 0f) * _e99);
    let _e101 = fogType_1;
    if (_e101 == 2i) {
        let _e103 = opticalDepth;
        return clamp((1f - exp(-(_e103))), 0f, 1f);
    }
    let _e108 = opticalDepth;
    let _e109 = opticalDepth;
    return clamp((1f - exp(-((_e108 * _e109)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e73 = (*c);
    (*c) = max(_e73, vec3<f32>(0f, 0f, 0f));
    let _e75 = (*c);
    cutoff = (_e75 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e77 = (*c);
    lo = (_e77 / vec3(12.92f));
    let _e80 = (*c);
    hi = pow(((_e80 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e85 = hi;
    let _e86 = lo;
    let _e87 = cutoff;
    return mix(_e85, _e86, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e87));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

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
        let _e117 = unnamed.worldLightParams[0u];
        let _e118 = c_1;
        let _e120 = (_e118.xyz * _e117);
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = c_1;
    return _e127;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var param_9: f32;
    var param_10: f32;

    let _e81 = unnamed.packed_indices[0i][3u];
    let _e87 = unnamed.packed_indices[0i][3u];
    let _e92 = fog_tex_coord_1;
    let _e93 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    fog = _e93;
    param_6 = 0u;
    let _e94 = frag_tex_coord0_1;
    param_7 = _e94;
    param_8 = 0i;
    let _e95 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e97 = unnamed.ent_color0_;
    color0_ = (_e95 * _e97);
    let _e99 = color0_;
    base = _e99;
    if override_type_3_3 {
        let _e101 = color0_[3u];
        if (_e101 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e104 = color0_[3u];
            if (_e104 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e107 = color0_[3u];
                if (_e107 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e109 = color0_;
    base = _e109;
    let _e110 = wired_advanced_fog_enabled_u0028_();
    if _e110 {
        let _e111 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e111;
        if override_type_3_6 {
            let _e112 = fogAmount;
            let _e114 = base;
            let _e116 = (_e114.xyz * (1f - _e112));
            base[0u] = _e116.x;
            base[1u] = _e116.y;
            base[2u] = _e116.z;
        } else {
            if override_type_3_7 {
                let _e123 = fogAmount;
                let _e125 = base;
                base = (_e125 * (1f - _e123));
            } else {
                if override_type_3_8 {
                    let _e127 = fogAmount;
                    let _e130 = base[3u];
                    base[3u] = (_e130 * (1f - _e127));
                } else {
                    let _e133 = base;
                    let _e136 = unnamed.advancedFogColorDensity;
                    let _e138 = fogAmount;
                    let _e140 = mix(_e133.xyz, _e136.xyz, vec3(_e138));
                    base[0u] = _e140.x;
                    base[1u] = _e140.y;
                    base[2u] = _e140.z;
                }
            }
        }
    } else {
        if override_type_3_9 {
            let _e147 = base;
            let _e150 = fog[3u];
            let _e152 = (_e147.xyz * (1f - _e150));
            base[0u] = _e152.x;
            base[1u] = _e152.y;
            base[2u] = _e152.z;
        } else {
            if override_type_3_10 {
                let _e159 = base;
                let _e161 = fog[3u];
                base = (_e159 * (1f - _e161));
            } else {
                if override_type_3_11 {
                    let _e165 = base[3u];
                    let _e167 = fog[3u];
                    base[3u] = (_e165 * (1f - _e167));
                } else {
                    let _e171 = base;
                    let _e172 = fog;
                    let _e174 = unnamed.fogColor;
                    let _e177 = fog[3u];
                    base = mix(_e171, (_e172 * _e174), vec4(_e177));
                }
            }
        }
    }
    if override_type_3_12 {
        let _e181 = base[3u];
        if (_e181 == 0f) {
            discard;
        }
    } else {
        if override_type_3_13 {
            let _e183 = base;
            let _e185 = base;
            if (dot(_e183.xyz, _e185.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e189 = base;
    out_color = _e189;
    let _e191 = color0_[3u];
    param_9 = _e191;
    let _e192 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_9));
    param_10 = _e192;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_10));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
