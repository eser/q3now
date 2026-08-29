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
override override_type_4_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_4_1: bool = (alpha_test_func == 2i);
override override_type_4_2: bool = (alpha_test_func == 3i);
@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
override override_type_4_3: bool = (alpha_test_func == 1i);
override override_type_4_4: bool = (alpha_test_func == 2i);
override override_type_4_5: bool = (alpha_test_func == 3i);
override override_type_4_6: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_7: bool = (acff == 1i);
override override_type_4_8: bool = (acff == 2i);
override override_type_4_9: bool = (acff == 3i);
override override_type_4_10: bool = (acff == 1i);
override override_type_4_11: bool = (acff == 2i);
override override_type_4_12: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_13: bool = (discard_mode == 1i);
override override_type_4_14: bool = (discard_mode == 2i);
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
        let _e82 = (*alpha);
        let _e85 = span;
        return clamp((abs((_e82 - alpha_test_value)) / _e85), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e88 = (*alpha);
            return clamp(((alpha_test_value - _e88) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
                let _e93 = (*alpha);
                return clamp(((_e93 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e80 + 0.5f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e87 = fogType;
    let _e90 = fogType;
    return (((_e85 > 0.5f) && (_e87 >= 1i)) && (_e90 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e80 = wired_advanced_fog_enabled_u0028_();
    if !(_e80) {
        return 0f;
    }
    let _e83 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e83, 0.000001f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e88 + 0.5f));
    let _e91 = fogType_1;
    if (_e91 == 1i) {
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e95 <= 0f) {
            return 0f;
        }
        let _e97 = viewDepth;
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e97 / _e100), 0f, 1f);
    }
    let _e105 = unnamed.advancedFogColorDensity[3u];
    let _e107 = viewDepth;
    opticalDepth = (max(_e105, 0f) * _e107);
    let _e109 = fogType_1;
    if (_e109 == 2i) {
        let _e111 = opticalDepth;
        return clamp((1f - exp(-(_e111))), 0f, 1f);
    }
    let _e116 = opticalDepth;
    let _e117 = opticalDepth;
    return clamp((1f - exp(-((_e116 * _e117)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e81 = (*c);
    (*c) = max(_e81, vec3<f32>(0f, 0f, 0f));
    let _e83 = (*c);
    cutoff = (_e83 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e85 = (*c);
    lo = (_e85 / vec3(12.92f));
    let _e88 = (*c);
    hi = pow(((_e88 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e93 = hi;
    let _e94 = lo;
    let _e95 = cutoff;
    return mix(_e93, _e94, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e95));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

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
        param = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e125 = unnamed.worldLightParams[0u];
        let _e126 = c_1;
        let _e128 = (_e126.xyz * _e125);
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = c_1;
    return _e135;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_4: f32;
    var param_5: f32;

    let _e93 = unnamed.packed_indices[0i][3u];
    let _e99 = unnamed.packed_indices[0i][3u];
    let _e104 = fog_tex_coord_1;
    let _e105 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    fog = _e105;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e110 = frag_tex_coord0_1;
    param_2 = _e110;
    param_3 = 0i;
    let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e112 = frag_color;
    color0_ = (_e111 * _e112);
    let _e114 = color0_;
    base = _e114;
    if override_type_4_3 {
        let _e116 = color0_[3u];
        if (_e116 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e119 = color0_[3u];
            if (_e119 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e122 = color0_[3u];
                if (_e122 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e124 = color0_;
    base = _e124;
    if override_type_4_6 {
        let _e127 = unnamed.worldLightParams[1u];
        wetness = clamp(_e127, 0f, 1f);
        let _e131 = unnamed.worldLightParams[2u];
        frost = clamp(_e131, 0f, 1f);
        let _e133 = base;
        luminance = dot(_e133.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e136 = wetness;
        let _e138 = base;
        let _e140 = (_e138.xyz * mix(1f, 0.82f, _e136));
        base[0u] = _e140.x;
        base[1u] = _e140.y;
        base[2u] = _e140.z;
        let _e147 = base;
        let _e149 = luminance;
        let _e151 = luminance;
        let _e153 = luminance;
        let _e155 = frost;
        let _e158 = mix(_e147.xyz, vec3<f32>((_e149 * 0.88f), (_e151 * 0.94f), _e153), vec3((_e155 * 0.55f)));
        base[0u] = _e158.x;
        base[1u] = _e158.y;
        base[2u] = _e158.z;
    }
    let _e165 = color0_;
    let _e168 = unnamed.emissionRadiance;
    let _e171 = base;
    let _e173 = (_e171.xyz + (_e165.xyz * _e168.xyz));
    base[0u] = _e173.x;
    base[1u] = _e173.y;
    base[2u] = _e173.z;
    let _e180 = wired_advanced_fog_enabled_u0028_();
    if _e180 {
        let _e181 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e181;
        if override_type_4_7 {
            let _e182 = fogAmount;
            let _e184 = base;
            let _e186 = (_e184.xyz * (1f - _e182));
            base[0u] = _e186.x;
            base[1u] = _e186.y;
            base[2u] = _e186.z;
        } else {
            if override_type_4_8 {
                let _e193 = fogAmount;
                let _e195 = base;
                base = (_e195 * (1f - _e193));
            } else {
                if override_type_4_9 {
                    let _e197 = fogAmount;
                    let _e200 = base[3u];
                    base[3u] = (_e200 * (1f - _e197));
                } else {
                    let _e203 = base;
                    let _e206 = unnamed.advancedFogColorDensity;
                    let _e208 = fogAmount;
                    let _e210 = mix(_e203.xyz, _e206.xyz, vec3(_e208));
                    base[0u] = _e210.x;
                    base[1u] = _e210.y;
                    base[2u] = _e210.z;
                }
            }
        }
    } else {
        if override_type_4_10 {
            let _e217 = base;
            let _e220 = fog[3u];
            let _e222 = (_e217.xyz * (1f - _e220));
            base[0u] = _e222.x;
            base[1u] = _e222.y;
            base[2u] = _e222.z;
        } else {
            if override_type_4_11 {
                let _e229 = base;
                let _e231 = fog[3u];
                base = (_e229 * (1f - _e231));
            } else {
                if override_type_4_12 {
                    let _e235 = base[3u];
                    let _e237 = fog[3u];
                    base[3u] = (_e235 * (1f - _e237));
                } else {
                    let _e241 = base;
                    let _e242 = fog;
                    let _e244 = unnamed.fogColor;
                    let _e247 = fog[3u];
                    base = mix(_e241, (_e242 * _e244), vec4(_e247));
                }
            }
        }
    }
    if override_type_4_13 {
        let _e251 = base[3u];
        if (_e251 == 0f) {
            discard;
        }
    } else {
        if override_type_4_14 {
            let _e253 = base;
            let _e255 = base;
            if (dot(_e253.xyz, _e255.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e259 = base;
    out_color = _e259;
    let _e261 = color0_[3u];
    param_4 = _e261;
    let _e262 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_4));
    param_5 = _e262;
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
