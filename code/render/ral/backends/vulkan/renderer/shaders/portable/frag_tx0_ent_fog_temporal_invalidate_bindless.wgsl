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
override override_type_4_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_4_1: bool = (alpha_test_func == 2i);
override override_type_4_2: bool = (alpha_test_func == 3i);
@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
override override_type_4_3: bool = (alpha_test_func == 1i);
override override_type_4_4: bool = (alpha_test_func == 2i);
override override_type_4_5: bool = (alpha_test_func == 3i);
@id(10) override acff: i32 = 0i;
override override_type_4_6: bool = (acff == 1i);
override override_type_4_7: bool = (acff == 2i);
override override_type_4_8: bool = (acff == 3i);
override override_type_4_9: bool = (acff == 1i);
override override_type_4_10: bool = (acff == 2i);
override override_type_4_11: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_12: bool = (discard_mode == 1i);
override override_type_4_13: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_4_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e70 = (*alpha);
        let _e73 = span;
        return clamp((abs((_e70 - alpha_test_value)) / _e73), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e76 = (*alpha);
            return clamp(((alpha_test_value - _e76) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
                let _e81 = (*alpha);
                return clamp(((_e81 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e68 + 0.5f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e75 = fogType;
    let _e78 = fogType;
    return (((_e73 > 0.5f) && (_e75 >= 1i)) && (_e78 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e68 = wired_advanced_fog_enabled_u0028_();
    if !(_e68) {
        return 0f;
    }
    let _e71 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e71, 0.000001f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e76 + 0.5f));
    let _e79 = fogType_1;
    if (_e79 == 1i) {
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e83 <= 0f) {
            return 0f;
        }
        let _e85 = viewDepth;
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e85 / _e88), 0f, 1f);
    }
    let _e93 = unnamed.advancedFogColorDensity[3u];
    let _e95 = viewDepth;
    opticalDepth = (max(_e93, 0f) * _e95);
    let _e97 = fogType_1;
    if (_e97 == 2i) {
        let _e99 = opticalDepth;
        return clamp((1f - exp(-(_e99))), 0f, 1f);
    }
    let _e104 = opticalDepth;
    let _e105 = opticalDepth;
    return clamp((1f - exp(-((_e104 * _e105)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c);
    (*c) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e92 = (*uv);
    let _e93 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    c_1 = _e93;
    let _e94 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e94))) == 0i) {
        let _e99 = c_1;
        param = _e99.xyz;
        let _e101 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e101.x;
        c_1[1u] = _e101.y;
        c_1[2u] = _e101.z;
    }
    let _e108 = (*slot);
    if (lightmap_slot == (_e108 + 1i)) {
        let _e113 = unnamed.worldLightParams[0u];
        let _e114 = c_1;
        let _e116 = (_e114.xyz * _e113);
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = c_1;
    return _e123;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var param_4: f32;
    var param_5: f32;

    let _e77 = unnamed.packed_indices[0i][3u];
    let _e83 = unnamed.packed_indices[0i][3u];
    let _e88 = fog_tex_coord_1;
    let _e89 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    fog = _e89;
    param_1 = 0u;
    let _e90 = frag_tex_coord0_1;
    param_2 = _e90;
    param_3 = 0i;
    let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e93 = unnamed.ent_color0_;
    color0_ = (_e91 * _e93);
    let _e95 = color0_;
    base = _e95;
    if override_type_4_3 {
        let _e97 = color0_[3u];
        if (_e97 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e100 = color0_[3u];
            if (_e100 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e103 = color0_[3u];
                if (_e103 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e105 = color0_;
    base = _e105;
    let _e106 = wired_advanced_fog_enabled_u0028_();
    if _e106 {
        let _e107 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e107;
        if override_type_4_6 {
            let _e108 = fogAmount;
            let _e110 = base;
            let _e112 = (_e110.xyz * (1f - _e108));
            base[0u] = _e112.x;
            base[1u] = _e112.y;
            base[2u] = _e112.z;
        } else {
            if override_type_4_7 {
                let _e119 = fogAmount;
                let _e121 = base;
                base = (_e121 * (1f - _e119));
            } else {
                if override_type_4_8 {
                    let _e123 = fogAmount;
                    let _e126 = base[3u];
                    base[3u] = (_e126 * (1f - _e123));
                } else {
                    let _e129 = base;
                    let _e132 = unnamed.advancedFogColorDensity;
                    let _e134 = fogAmount;
                    let _e136 = mix(_e129.xyz, _e132.xyz, vec3(_e134));
                    base[0u] = _e136.x;
                    base[1u] = _e136.y;
                    base[2u] = _e136.z;
                }
            }
        }
    } else {
        if override_type_4_9 {
            let _e143 = base;
            let _e146 = fog[3u];
            let _e148 = (_e143.xyz * (1f - _e146));
            base[0u] = _e148.x;
            base[1u] = _e148.y;
            base[2u] = _e148.z;
        } else {
            if override_type_4_10 {
                let _e155 = base;
                let _e157 = fog[3u];
                base = (_e155 * (1f - _e157));
            } else {
                if override_type_4_11 {
                    let _e161 = base[3u];
                    let _e163 = fog[3u];
                    base[3u] = (_e161 * (1f - _e163));
                } else {
                    let _e167 = base;
                    let _e168 = fog;
                    let _e170 = unnamed.fogColor;
                    let _e173 = fog[3u];
                    base = mix(_e167, (_e168 * _e170), vec4(_e173));
                }
            }
        }
    }
    if override_type_4_12 {
        let _e177 = base[3u];
        if (_e177 == 0f) {
            discard;
        }
    } else {
        if override_type_4_13 {
            let _e179 = base;
            let _e181 = base;
            if (dot(_e179.xyz, _e181.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e185 = base;
    out_color = _e185;
    let _e187 = color0_[3u];
    param_4 = _e187;
    let _e188 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_4));
    param_5 = _e188;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_5));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
