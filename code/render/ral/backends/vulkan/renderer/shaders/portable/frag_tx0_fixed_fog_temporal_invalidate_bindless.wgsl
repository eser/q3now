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
        let _e84 = (*alpha);
        let _e87 = span;
        return clamp((abs((_e84 - alpha_test_value)) / _e87), 0f, 1f);
    } else {
        if override_type_4_1 {
            let _e90 = (*alpha);
            return clamp(((alpha_test_value - _e90) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_4_2 {
                let _e95 = (*alpha);
                return clamp(((_e95 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
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
    var param: vec3<f32>;
    var param_1: vec3<f32>;

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
        param = _e114.xyz;
        let _e116 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = (*slot);
    if (lightmap_slot == (_e123 + 1i)) {
        let _e126 = c_1;
        param_1 = _e126.xyz;
        let _e128 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = c_1;
    return _e135;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_5: f32;
    var param_6: f32;

    let _e96 = unnamed.packed_indices[0i][3u];
    let _e102 = unnamed.packed_indices[0i][3u];
    let _e107 = fog_tex_coord_1;
    let _e108 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e102 >> bitcast<u32>(12i)) & 255u)], _e107);
    fog = _e108;
    param_2 = 0u;
    let _e109 = frag_tex_coord0_1;
    param_3 = _e109;
    param_4 = 0i;
    let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e110;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e115 = sampledColor0_;
    let _e116 = frag_color;
    color0_ = (_e115 * _e116);
    let _e118 = color0_;
    base = _e118;
    if override_type_4_3 {
        let _e120 = color0_[3u];
        if (_e120 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e123 = color0_[3u];
            if (_e123 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_4_5 {
                let _e126 = color0_[3u];
                if (_e126 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e128 = color0_;
    base = _e128;
    if override_type_4_6 {
        let _e131 = unnamed.worldLightParams[1u];
        wetness = clamp(_e131, 0f, 1f);
        let _e135 = unnamed.worldLightParams[2u];
        frost = clamp(_e135, 0f, 1f);
        let _e137 = base;
        luminance = dot(_e137.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e140 = wetness;
        let _e142 = base;
        let _e144 = (_e142.xyz * mix(1f, 0.82f, _e140));
        base[0u] = _e144.x;
        base[1u] = _e144.y;
        base[2u] = _e144.z;
        let _e151 = base;
        let _e153 = luminance;
        let _e155 = luminance;
        let _e157 = luminance;
        let _e159 = frost;
        let _e162 = mix(_e151.xyz, vec3<f32>((_e153 * 0.88f), (_e155 * 0.94f), _e157), vec3((_e159 * 0.55f)));
        base[0u] = _e162.x;
        base[1u] = _e162.y;
        base[2u] = _e162.z;
    }
    let _e171 = unnamed.materialFactors[0u];
    if (_e171 >= 0f) {
        let _e174 = unnamed.materialBaseColor;
        let _e175 = base;
        base = (_e175 * _e174);
    }
    let _e177 = color0_;
    let _e180 = unnamed.emissionRadiance;
    let _e183 = base;
    let _e185 = (_e183.xyz + (_e177.xyz * _e180.xyz));
    base[0u] = _e185.x;
    base[1u] = _e185.y;
    base[2u] = _e185.z;
    let _e192 = wired_advanced_fog_enabled_u0028_();
    if _e192 {
        let _e193 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e193;
        if override_type_4_7 {
            let _e194 = fogAmount;
            let _e196 = base;
            let _e198 = (_e196.xyz * (1f - _e194));
            base[0u] = _e198.x;
            base[1u] = _e198.y;
            base[2u] = _e198.z;
        } else {
            if override_type_4_8 {
                let _e205 = fogAmount;
                let _e207 = base;
                base = (_e207 * (1f - _e205));
            } else {
                if override_type_4_9 {
                    let _e209 = fogAmount;
                    let _e212 = base[3u];
                    base[3u] = (_e212 * (1f - _e209));
                } else {
                    let _e215 = base;
                    let _e218 = unnamed.advancedFogColorDensity;
                    let _e220 = fogAmount;
                    let _e222 = mix(_e215.xyz, _e218.xyz, vec3(_e220));
                    base[0u] = _e222.x;
                    base[1u] = _e222.y;
                    base[2u] = _e222.z;
                }
            }
        }
    } else {
        if override_type_4_10 {
            let _e229 = base;
            let _e232 = fog[3u];
            let _e234 = (_e229.xyz * (1f - _e232));
            base[0u] = _e234.x;
            base[1u] = _e234.y;
            base[2u] = _e234.z;
        } else {
            if override_type_4_11 {
                let _e241 = base;
                let _e243 = fog[3u];
                base = (_e241 * (1f - _e243));
            } else {
                if override_type_4_12 {
                    let _e247 = base[3u];
                    let _e249 = fog[3u];
                    base[3u] = (_e247 * (1f - _e249));
                } else {
                    let _e253 = base;
                    let _e254 = fog;
                    let _e256 = unnamed.fogColor;
                    let _e259 = fog[3u];
                    base = mix(_e253, (_e254 * _e256), vec4(_e259));
                }
            }
        }
    }
    if override_type_4_13 {
        let _e263 = base[3u];
        if (_e263 == 0f) {
            discard;
        }
    } else {
        if override_type_4_14 {
            let _e265 = base;
            let _e267 = base;
            if (dot(_e265.xyz, _e267.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e271 = base;
    out_color = _e271;
    let _e273 = color0_[3u];
    param_5 = _e273;
    let _e274 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_5));
    param_6 = _e274;
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
