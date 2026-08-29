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
        let _e80 = (*alpha);
        let _e83 = span;
        return clamp((abs((_e80 - alpha_test_value)) / _e83), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e86 = (*alpha);
            return clamp(((alpha_test_value - _e86) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
                let _e91 = (*alpha);
                return clamp(((_e91 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
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
    var param: vec3<f32>;

    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e102 = (*uv);
    let _e103 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    c_1 = _e103;
    let _e104 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e104))) == 0i) {
        let _e109 = c_1;
        param = _e109.xyz;
        let _e111 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = (*slot);
    if (lightmap_slot == (_e118 + 1i)) {
        let _e123 = unnamed.worldLightParams[0u];
        let _e124 = c_1;
        let _e126 = (_e124.xyz * _e123);
        c_1[0u] = _e126.x;
        c_1[1u] = _e126.y;
        c_1[2u] = _e126.z;
    }
    let _e133 = c_1;
    return _e133;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e90 = unnamed.packed_indices[0i][3u];
    let _e96 = unnamed.packed_indices[0i][3u];
    let _e101 = fog_tex_coord_1;
    let _e102 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    fog = _e102;
    param_1 = 0u;
    let _e103 = frag_tex_coord0_1;
    param_2 = _e103;
    param_3 = 0i;
    let _e104 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e106 = unnamed.ent_color0_;
    color0_ = (_e104 * _e106);
    let _e108 = color0_;
    base = _e108;
    if override_type_4_3 {
        let _e110 = color0_[3u];
        if (_e110 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e113 = color0_[3u];
            if (_e113 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e116 = color0_[3u];
                if (_e116 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e118 = color0_;
    base = _e118;
    if override_type_4_6 {
        let _e121 = unnamed.worldLightParams[1u];
        wetness = clamp(_e121, 0f, 1f);
        let _e125 = unnamed.worldLightParams[2u];
        frost = clamp(_e125, 0f, 1f);
        let _e127 = base;
        luminance = dot(_e127.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e130 = wetness;
        let _e132 = base;
        let _e134 = (_e132.xyz * mix(1f, 0.82f, _e130));
        base[0u] = _e134.x;
        base[1u] = _e134.y;
        base[2u] = _e134.z;
        let _e141 = base;
        let _e143 = luminance;
        let _e145 = luminance;
        let _e147 = luminance;
        let _e149 = frost;
        let _e152 = mix(_e141.xyz, vec3<f32>((_e143 * 0.88f), (_e145 * 0.94f), _e147), vec3((_e149 * 0.55f)));
        base[0u] = _e152.x;
        base[1u] = _e152.y;
        base[2u] = _e152.z;
    }
    let _e159 = color0_;
    let _e162 = unnamed.emissionRadiance;
    let _e165 = base;
    let _e167 = (_e165.xyz + (_e159.xyz * _e162.xyz));
    base[0u] = _e167.x;
    base[1u] = _e167.y;
    base[2u] = _e167.z;
    let _e174 = wired_advanced_fog_enabled_u0028_();
    if _e174 {
        let _e175 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e175;
        if override_type_4_7 {
            let _e176 = fogAmount;
            let _e178 = base;
            let _e180 = (_e178.xyz * (1f - _e176));
            base[0u] = _e180.x;
            base[1u] = _e180.y;
            base[2u] = _e180.z;
        } else {
            if override_type_4_8 {
                let _e187 = fogAmount;
                let _e189 = base;
                base = (_e189 * (1f - _e187));
            } else {
                if override_type_4_9 {
                    let _e191 = fogAmount;
                    let _e194 = base[3u];
                    base[3u] = (_e194 * (1f - _e191));
                } else {
                    let _e197 = base;
                    let _e200 = unnamed.advancedFogColorDensity;
                    let _e202 = fogAmount;
                    let _e204 = mix(_e197.xyz, _e200.xyz, vec3(_e202));
                    base[0u] = _e204.x;
                    base[1u] = _e204.y;
                    base[2u] = _e204.z;
                }
            }
        }
    } else {
        if override_type_4_10 {
            let _e211 = base;
            let _e214 = fog[3u];
            let _e216 = (_e211.xyz * (1f - _e214));
            base[0u] = _e216.x;
            base[1u] = _e216.y;
            base[2u] = _e216.z;
        } else {
            if override_type_4_11 {
                let _e223 = base;
                let _e225 = fog[3u];
                base = (_e223 * (1f - _e225));
            } else {
                if override_type_4_12 {
                    let _e229 = base[3u];
                    let _e231 = fog[3u];
                    base[3u] = (_e229 * (1f - _e231));
                } else {
                    let _e235 = base;
                    let _e236 = fog;
                    let _e238 = unnamed.fogColor;
                    let _e241 = fog[3u];
                    base = mix(_e235, (_e236 * _e238), vec4(_e241));
                }
            }
        }
    }
    if override_type_4_13 {
        let _e245 = base[3u];
        if (_e245 == 0f) {
            discard;
        }
    } else {
        if override_type_4_14 {
            let _e247 = base;
            let _e249 = base;
            if (dot(_e247.xyz, _e249.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e253 = base;
    out_color = _e253;
    let _e255 = color0_[3u];
    param_4 = _e255;
    let _e256 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_4));
    param_5 = _e256;
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
