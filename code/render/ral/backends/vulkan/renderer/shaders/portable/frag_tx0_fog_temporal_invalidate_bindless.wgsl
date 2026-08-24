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
var<private> frag_color0In_1: vec4<f32>;
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
        let _e71 = (*alpha);
        let _e74 = span;
        return clamp((abs((_e71 - alpha_test_value)) / _e74), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e77 = (*alpha);
            return clamp(((alpha_test_value - _e77) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
                let _e82 = (*alpha);
                return clamp(((_e82 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e69 + 0.5f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e76 = fogType;
    let _e79 = fogType;
    return (((_e74 > 0.5f) && (_e76 >= 1i)) && (_e79 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e69 = wired_advanced_fog_enabled_u0028_();
    if !(_e69) {
        return 0f;
    }
    let _e72 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e72, 0.000001f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e77 + 0.5f));
    let _e80 = fogType_1;
    if (_e80 == 1i) {
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e84 <= 0f) {
            return 0f;
        }
        let _e86 = viewDepth;
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e86 / _e89), 0f, 1f);
    }
    let _e94 = unnamed.advancedFogColorDensity[3u];
    let _e96 = viewDepth;
    opticalDepth = (max(_e94, 0f) * _e96);
    let _e98 = fogType_1;
    if (_e98 == 2i) {
        let _e100 = opticalDepth;
        return clamp((1f - exp(-(_e100))), 0f, 1f);
    }
    let _e105 = opticalDepth;
    let _e106 = opticalDepth;
    return clamp((1f - exp(-((_e105 * _e106)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c);
    (*c) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_1 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_1;
        param = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e114 = unnamed.worldLightParams[0u];
        let _e115 = c_1;
        let _e117 = (_e115.xyz * _e114);
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var param_5: f32;
    var param_6: f32;

    let _e80 = unnamed.packed_indices[0i][3u];
    let _e86 = unnamed.packed_indices[0i][3u];
    let _e91 = fog_tex_coord_1;
    let _e92 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e86 >> bitcast<u32>(12i)) & 255u)], _e91);
    fog = _e92;
    let _e93 = frag_color0In_1;
    param_1 = _e93.xyz;
    let _e95 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e97 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e95.x, _e95.y, _e95.z, _e97);
    param_2 = 0u;
    let _e102 = frag_tex_coord0_1;
    param_3 = _e102;
    param_4 = 0i;
    let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e104 = frag_color0_;
    color0_ = (_e103 * _e104);
    let _e106 = color0_;
    base = _e106;
    if override_type_4_3 {
        let _e108 = color0_[3u];
        if (_e108 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e111 = color0_[3u];
            if (_e111 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e114 = color0_[3u];
                if (_e114 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e116 = color0_;
    base = _e116;
    let _e117 = wired_advanced_fog_enabled_u0028_();
    if _e117 {
        let _e118 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e118;
        if override_type_4_6 {
            let _e119 = fogAmount;
            let _e121 = base;
            let _e123 = (_e121.xyz * (1f - _e119));
            base[0u] = _e123.x;
            base[1u] = _e123.y;
            base[2u] = _e123.z;
        } else {
            if override_type_4_7 {
                let _e130 = fogAmount;
                let _e132 = base;
                base = (_e132 * (1f - _e130));
            } else {
                if override_type_4_8 {
                    let _e134 = fogAmount;
                    let _e137 = base[3u];
                    base[3u] = (_e137 * (1f - _e134));
                } else {
                    let _e140 = base;
                    let _e143 = unnamed.advancedFogColorDensity;
                    let _e145 = fogAmount;
                    let _e147 = mix(_e140.xyz, _e143.xyz, vec3(_e145));
                    base[0u] = _e147.x;
                    base[1u] = _e147.y;
                    base[2u] = _e147.z;
                }
            }
        }
    } else {
        if override_type_4_9 {
            let _e154 = base;
            let _e157 = fog[3u];
            let _e159 = (_e154.xyz * (1f - _e157));
            base[0u] = _e159.x;
            base[1u] = _e159.y;
            base[2u] = _e159.z;
        } else {
            if override_type_4_10 {
                let _e166 = base;
                let _e168 = fog[3u];
                base = (_e166 * (1f - _e168));
            } else {
                if override_type_4_11 {
                    let _e172 = base[3u];
                    let _e174 = fog[3u];
                    base[3u] = (_e172 * (1f - _e174));
                } else {
                    let _e178 = base;
                    let _e179 = fog;
                    let _e181 = unnamed.fogColor;
                    let _e184 = fog[3u];
                    base = mix(_e178, (_e179 * _e181), vec4(_e184));
                }
            }
        }
    }
    if override_type_4_12 {
        let _e188 = base[3u];
        if (_e188 == 0f) {
            discard;
        }
    } else {
        if override_type_4_13 {
            let _e190 = base;
            let _e192 = base;
            if (dot(_e190.xyz, _e192.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e196 = base;
    out_color = _e196;
    let _e198 = color0_[3u];
    param_5 = _e198;
    let _e199 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_5));
    param_6 = _e199;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_6));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
