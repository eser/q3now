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
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
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

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e80 = (*rgb);
    let _e83 = unnamed.worldLightParams[0u];
    boosted = (_e80 * _e83);
    let _e86 = boosted[0u];
    let _e88 = boosted[1u];
    let _e90 = boosted[2u];
    peak = max(_e86, max(_e88, _e90));
    let _e93 = peak;
    if (_e93 > 1f) {
        let _e95 = peak;
        let _e96 = boosted;
        boosted = (_e96 / vec3(_e95));
    }
    let _e99 = boosted;
    return _e99;
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
    var param_1: vec3<f32>;

    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e105 = (*uv);
    let _e106 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    c_1 = _e106;
    let _e107 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e107))) == 0i) {
        let _e112 = c_1;
        param = _e112.xyz;
        let _e114 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = (*slot);
    if (lightmap_slot == (_e121 + 1i)) {
        let _e124 = c_1;
        param_1 = _e124.xyz;
        let _e126 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e126.x;
        c_1[1u] = _e126.y;
        c_1[2u] = _e126.z;
    }
    let _e133 = c_1;
    return _e133;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_5: f32;
    var param_6: f32;

    let _e93 = unnamed.packed_indices[0i][3u];
    let _e99 = unnamed.packed_indices[0i][3u];
    let _e104 = fog_tex_coord_1;
    let _e105 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    fog = _e105;
    param_2 = 0u;
    let _e106 = frag_tex_coord0_1;
    param_3 = _e106;
    param_4 = 0i;
    let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e107;
    let _e108 = sampledColor0_;
    let _e110 = unnamed.ent_color0_;
    color0_ = (_e108 * _e110);
    let _e112 = color0_;
    base = _e112;
    if override_type_4_3 {
        let _e114 = color0_[3u];
        if (_e114 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e117 = color0_[3u];
            if (_e117 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e120 = color0_[3u];
                if (_e120 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e122 = color0_;
    base = _e122;
    if override_type_4_6 {
        let _e125 = unnamed.worldLightParams[1u];
        wetness = clamp(_e125, 0f, 1f);
        let _e129 = unnamed.worldLightParams[2u];
        frost = clamp(_e129, 0f, 1f);
        let _e131 = base;
        luminance = dot(_e131.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e134 = wetness;
        let _e136 = base;
        let _e138 = (_e136.xyz * mix(1f, 0.82f, _e134));
        base[0u] = _e138.x;
        base[1u] = _e138.y;
        base[2u] = _e138.z;
        let _e145 = base;
        let _e147 = luminance;
        let _e149 = luminance;
        let _e151 = luminance;
        let _e153 = frost;
        let _e156 = mix(_e145.xyz, vec3<f32>((_e147 * 0.88f), (_e149 * 0.94f), _e151), vec3((_e153 * 0.55f)));
        base[0u] = _e156.x;
        base[1u] = _e156.y;
        base[2u] = _e156.z;
    }
    let _e165 = unnamed.materialFactors[0u];
    if (_e165 >= 0f) {
        let _e168 = unnamed.materialBaseColor;
        let _e169 = base;
        base = (_e169 * _e168);
    }
    let _e171 = color0_;
    let _e174 = unnamed.emissionRadiance;
    let _e177 = base;
    let _e179 = (_e177.xyz + (_e171.xyz * _e174.xyz));
    base[0u] = _e179.x;
    base[1u] = _e179.y;
    base[2u] = _e179.z;
    let _e186 = wired_advanced_fog_enabled_u0028_();
    if _e186 {
        let _e187 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e187;
        if override_type_4_7 {
            let _e188 = fogAmount;
            let _e190 = base;
            let _e192 = (_e190.xyz * (1f - _e188));
            base[0u] = _e192.x;
            base[1u] = _e192.y;
            base[2u] = _e192.z;
        } else {
            if override_type_4_8 {
                let _e199 = fogAmount;
                let _e201 = base;
                base = (_e201 * (1f - _e199));
            } else {
                if override_type_4_9 {
                    let _e203 = fogAmount;
                    let _e206 = base[3u];
                    base[3u] = (_e206 * (1f - _e203));
                } else {
                    let _e209 = base;
                    let _e212 = unnamed.advancedFogColorDensity;
                    let _e214 = fogAmount;
                    let _e216 = mix(_e209.xyz, _e212.xyz, vec3(_e214));
                    base[0u] = _e216.x;
                    base[1u] = _e216.y;
                    base[2u] = _e216.z;
                }
            }
        }
    } else {
        if override_type_4_10 {
            let _e223 = base;
            let _e226 = fog[3u];
            let _e228 = (_e223.xyz * (1f - _e226));
            base[0u] = _e228.x;
            base[1u] = _e228.y;
            base[2u] = _e228.z;
        } else {
            if override_type_4_11 {
                let _e235 = base;
                let _e237 = fog[3u];
                base = (_e235 * (1f - _e237));
            } else {
                if override_type_4_12 {
                    let _e241 = base[3u];
                    let _e243 = fog[3u];
                    base[3u] = (_e241 * (1f - _e243));
                } else {
                    let _e247 = base;
                    let _e248 = fog;
                    let _e250 = unnamed.fogColor;
                    let _e253 = fog[3u];
                    base = mix(_e247, (_e248 * _e250), vec4(_e253));
                }
            }
        }
    }
    if override_type_4_13 {
        let _e257 = base[3u];
        if (_e257 == 0f) {
            discard;
        }
    } else {
        if override_type_4_14 {
            let _e259 = base;
            let _e261 = base;
            if (dot(_e259.xyz, _e261.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e265 = base;
    out_color = _e265;
    let _e267 = color0_[3u];
    param_5 = _e267;
    let _e268 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_5));
    param_6 = _e268;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_6));
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
