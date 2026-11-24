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
    @location(2) member_1: vec4<f32>,
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
var<private> out_temporal_validity: vec4<f32>;
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
    var phi_83_: bool;

    let _e83 = (*value);
    let _e84 = (*value);
    let _e86 = all((_e83 == _e84));
    phi_83_ = _e86;
    if _e86 {
        let _e87 = (*value);
        phi_83_ = all((abs(_e87) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e92 = phi_83_;
    return _e92;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_68_: bool;

    let _e83 = (*value_1);
    let _e84 = (*value_1);
    let _e86 = all((_e83 == _e84));
    phi_68_ = _e86;
    if _e86 {
        let _e87 = (*value_1);
        phi_68_ = all((abs(_e87) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e92 = phi_68_;
    return _e92;
}

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
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
    var phi_107_: bool;
    var phi_116_: bool;
    var phi_126_: bool;
    var phi_133_: bool;
    var phi_162_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e94 = temporalOutcome_1;
    let _e95 = (_e94 != 1u);
    phi_107_ = _e95;
    if !(_e95) {
        let _e97 = temporalCurrentClip_1;
        param = _e97;
        let _e98 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_107_ = !(_e98);
    }
    let _e101 = phi_107_;
    phi_116_ = _e101;
    if !(_e101) {
        let _e103 = temporalPreviousClip_1;
        param_1 = _e103;
        let _e104 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_116_ = !(_e104);
    }
    let _e107 = phi_116_;
    phi_126_ = _e107;
    if !(_e107) {
        let _e110 = temporalCurrentClip_1[3u];
        phi_126_ = (_e110 <= 0.000001f);
    }
    let _e113 = phi_126_;
    phi_133_ = _e113;
    if !(_e113) {
        let _e116 = temporalPreviousClip_1[3u];
        phi_133_ = (_e116 <= 0.000001f);
    }
    let _e119 = phi_133_;
    if _e119 {
        return;
    }
    let _e120 = temporalCurrentClip_1;
    let _e123 = temporalCurrentClip_1[3u];
    currentNdc = (_e120.xy / vec2(_e123));
    let _e126 = temporalPreviousClip_1;
    let _e129 = temporalPreviousClip_1[3u];
    previousNdc = (_e126.xy / vec2(_e129));
    let _e132 = currentNdc;
    param_2 = _e132;
    let _e133 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e134 = !(_e133);
    phi_162_ = _e134;
    if !(_e134) {
        let _e136 = previousNdc;
        param_3 = _e136;
        let _e137 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_162_ = !(_e137);
    }
    let _e140 = phi_162_;
    if _e140 {
        return;
    }
    let _e141 = currentNdc;
    currentUv = ((_e141 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e144 = previousNdc;
    previousUv = ((_e144 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e147 = currentUv;
    let _e148 = previousUv;
    velocity = (_e147 - _e148);
    let _e150 = velocity;
    param_4 = _e150;
    let _e151 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e151) {
        return;
    }
    let _e153 = velocity;
    out_temporal_velocity = _e153;
    let _e154 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e154, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e87 = (*alpha);
        let _e90 = span;
        return clamp((abs((_e87 - alpha_test_value)) / _e90), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e93 = (*alpha);
            return clamp(((alpha_test_value - _e93) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e98 = (*alpha);
                return clamp(((_e98 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e85 + 0.5f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e92 = fogType;
    let _e95 = fogType;
    return (((_e90 > 0.5f) && (_e92 >= 1i)) && (_e95 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e85 = wired_advanced_fog_enabled_u0028_();
    if !(_e85) {
        return 0f;
    }
    let _e88 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e88, 0.000001f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e93 + 0.5f));
    let _e96 = fogType_1;
    if (_e96 == 1i) {
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e100 <= 0f) {
            return 0f;
        }
        let _e102 = viewDepth;
        let _e105 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e102 / _e105), 0f, 1f);
    }
    let _e110 = unnamed.advancedFogColorDensity[3u];
    let _e112 = viewDepth;
    opticalDepth = (max(_e110, 0f) * _e112);
    let _e114 = fogType_1;
    if (_e114 == 2i) {
        let _e116 = opticalDepth;
        return clamp((1f - exp(-(_e116))), 0f, 1f);
    }
    let _e121 = opticalDepth;
    let _e122 = opticalDepth;
    return clamp((1f - exp(-((_e121 * _e122)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e85 = (*rgb);
    let _e88 = unnamed.worldLightParams[0u];
    boosted = (_e85 * _e88);
    let _e91 = boosted[0u];
    let _e93 = boosted[1u];
    let _e95 = boosted[2u];
    peak = max(_e91, max(_e93, _e95));
    let _e98 = peak;
    if (_e98 > 1f) {
        let _e100 = peak;
        let _e101 = boosted;
        boosted = (_e101 / vec3(_e100));
    }
    let _e104 = boosted;
    return _e104;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e86 = (*c);
    (*c) = max(_e86, vec3<f32>(0f, 0f, 0f));
    let _e88 = (*c);
    cutoff = (_e88 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e90 = (*c);
    lo = (_e90 / vec3(12.92f));
    let _e93 = (*c);
    hi = pow(((_e93 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e98 = hi;
    let _e99 = lo;
    let _e100 = cutoff;
    return mix(_e98, _e99, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e100));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e98 = (*role);
    let _e100 = (*role);
    let _e105 = unnamed.packed_indices[(_e98 / 4u)][(_e100 % 4u)];
    let _e110 = (*uv);
    let _e111 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    c_1 = _e111;
    let _e112 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e112))) == 0i) {
        let _e117 = c_1;
        param_5 = _e117.xyz;
        let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = (*slot);
    if (lightmap_slot == (_e126 + 1i)) {
        let _e129 = c_1;
        param_6 = _e129.xyz;
        let _e131 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e131.x;
        c_1[1u] = _e131.y;
        c_1[2u] = _e131.z;
    }
    let _e138 = c_1;
    return _e138;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_10: f32;
    var param_11: f32;
    var param_12: vec4<f32>;

    let _e99 = unnamed.packed_indices[0i][3u];
    let _e105 = unnamed.packed_indices[0i][3u];
    let _e110 = fog_tex_coord_1;
    let _e111 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    fog = _e111;
    param_7 = 0u;
    let _e112 = frag_tex_coord0_1;
    param_8 = _e112;
    param_9 = 0i;
    let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e113;
    let _e114 = sampledColor0_;
    let _e116 = unnamed.ent_color0_;
    color0_ = (_e114 * _e116);
    let _e118 = color0_;
    base = _e118;
    if override_type_3_3 {
        let _e120 = color0_[3u];
        if (_e120 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e123 = color0_[3u];
            if (_e123 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e126 = color0_[3u];
                if (_e126 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e128 = color0_;
    base = _e128;
    if override_type_3_6 {
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
        if override_type_3_7 {
            let _e194 = fogAmount;
            let _e196 = base;
            let _e198 = (_e196.xyz * (1f - _e194));
            base[0u] = _e198.x;
            base[1u] = _e198.y;
            base[2u] = _e198.z;
        } else {
            if override_type_3_8 {
                let _e205 = fogAmount;
                let _e207 = base;
                base = (_e207 * (1f - _e205));
            } else {
                if override_type_3_9 {
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
        if override_type_3_10 {
            let _e229 = base;
            let _e232 = fog[3u];
            let _e234 = (_e229.xyz * (1f - _e232));
            base[0u] = _e234.x;
            base[1u] = _e234.y;
            base[2u] = _e234.z;
        } else {
            if override_type_3_11 {
                let _e241 = base;
                let _e243 = fog[3u];
                base = (_e241 * (1f - _e243));
            } else {
                if override_type_3_12 {
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
    if override_type_3_13 {
        let _e263 = base[3u];
        if (_e263 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
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
    param_10 = _e273;
    let _e274 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_10));
    param_11 = _e274;
    let _e275 = base;
    param_12 = _e275;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_11), (&param_12));
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
