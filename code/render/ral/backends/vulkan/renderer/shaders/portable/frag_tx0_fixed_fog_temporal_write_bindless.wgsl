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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e85 = (*value);
    let _e86 = (*value);
    let _e88 = all((_e85 == _e86));
    phi_83_ = _e88;
    if _e88 {
        let _e89 = (*value);
        phi_83_ = all((abs(_e89) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e94 = phi_83_;
    return _e94;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_68_: bool;

    let _e85 = (*value_1);
    let _e86 = (*value_1);
    let _e88 = all((_e85 == _e86));
    phi_68_ = _e88;
    if _e88 {
        let _e89 = (*value_1);
        phi_68_ = all((abs(_e89) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e94 = phi_68_;
    return _e94;
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
    let _e96 = temporalOutcome_1;
    let _e97 = (_e96 != 1u);
    phi_107_ = _e97;
    if !(_e97) {
        let _e99 = temporalCurrentClip_1;
        param = _e99;
        let _e100 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_107_ = !(_e100);
    }
    let _e103 = phi_107_;
    phi_116_ = _e103;
    if !(_e103) {
        let _e105 = temporalPreviousClip_1;
        param_1 = _e105;
        let _e106 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_116_ = !(_e106);
    }
    let _e109 = phi_116_;
    phi_126_ = _e109;
    if !(_e109) {
        let _e112 = temporalCurrentClip_1[3u];
        phi_126_ = (_e112 <= 0.000001f);
    }
    let _e115 = phi_126_;
    phi_133_ = _e115;
    if !(_e115) {
        let _e118 = temporalPreviousClip_1[3u];
        phi_133_ = (_e118 <= 0.000001f);
    }
    let _e121 = phi_133_;
    if _e121 {
        return;
    }
    let _e122 = temporalCurrentClip_1;
    let _e125 = temporalCurrentClip_1[3u];
    currentNdc = (_e122.xy / vec2(_e125));
    let _e128 = temporalPreviousClip_1;
    let _e131 = temporalPreviousClip_1[3u];
    previousNdc = (_e128.xy / vec2(_e131));
    let _e134 = currentNdc;
    param_2 = _e134;
    let _e135 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e136 = !(_e135);
    phi_162_ = _e136;
    if !(_e136) {
        let _e138 = previousNdc;
        param_3 = _e138;
        let _e139 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_162_ = !(_e139);
    }
    let _e142 = phi_162_;
    if _e142 {
        return;
    }
    let _e143 = currentNdc;
    currentUv = ((_e143 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e146 = previousNdc;
    previousUv = ((_e146 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e149 = currentUv;
    let _e150 = previousUv;
    velocity = (_e149 - _e150);
    let _e152 = velocity;
    param_4 = _e152;
    let _e153 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e153) {
        return;
    }
    let _e155 = velocity;
    out_temporal_velocity = _e155;
    let _e156 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e156, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e89 = (*alpha);
        let _e92 = span;
        return clamp((abs((_e89 - alpha_test_value)) / _e92), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e95 = (*alpha);
            return clamp(((alpha_test_value - _e95) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e100 = (*alpha);
                return clamp(((_e100 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e87 + 0.5f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e94 = fogType;
    let _e97 = fogType;
    return (((_e92 > 0.5f) && (_e94 >= 1i)) && (_e97 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e87 = wired_advanced_fog_enabled_u0028_();
    if !(_e87) {
        return 0f;
    }
    let _e90 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e90, 0.000001f));
    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e95 + 0.5f));
    let _e98 = fogType_1;
    if (_e98 == 1i) {
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e102 <= 0f) {
            return 0f;
        }
        let _e104 = viewDepth;
        let _e107 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e104 / _e107), 0f, 1f);
    }
    let _e112 = unnamed.advancedFogColorDensity[3u];
    let _e114 = viewDepth;
    opticalDepth = (max(_e112, 0f) * _e114);
    let _e116 = fogType_1;
    if (_e116 == 2i) {
        let _e118 = opticalDepth;
        return clamp((1f - exp(-(_e118))), 0f, 1f);
    }
    let _e123 = opticalDepth;
    let _e124 = opticalDepth;
    return clamp((1f - exp(-((_e123 * _e124)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e87 = (*rgb);
    let _e90 = unnamed.worldLightParams[0u];
    boosted = (_e87 * _e90);
    let _e93 = boosted[0u];
    let _e95 = boosted[1u];
    let _e97 = boosted[2u];
    peak = max(_e93, max(_e95, _e97));
    let _e100 = peak;
    if (_e100 > 1f) {
        let _e102 = peak;
        let _e103 = boosted;
        boosted = (_e103 / vec3(_e102));
    }
    let _e106 = boosted;
    return _e106;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e88 = (*c);
    (*c) = max(_e88, vec3<f32>(0f, 0f, 0f));
    let _e90 = (*c);
    cutoff = (_e90 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e92 = (*c);
    lo = (_e92 / vec3(12.92f));
    let _e95 = (*c);
    hi = pow(((_e95 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e100 = hi;
    let _e101 = lo;
    let _e102 = cutoff;
    return mix(_e100, _e101, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e102));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e100 = (*role);
    let _e102 = (*role);
    let _e107 = unnamed.packed_indices[(_e100 / 4u)][(_e102 % 4u)];
    let _e112 = (*uv);
    let _e113 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    c_1 = _e113;
    let _e114 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e114))) == 0i) {
        let _e119 = c_1;
        param_5 = _e119.xyz;
        let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = (*slot);
    if (lightmap_slot == (_e128 + 1i)) {
        let _e131 = c_1;
        param_6 = _e131.xyz;
        let _e133 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e133.x;
        c_1[1u] = _e133.y;
        c_1[2u] = _e133.z;
    }
    let _e140 = c_1;
    return _e140;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_10: f32;
    var param_11: f32;
    var param_12: vec4<f32>;

    let _e102 = unnamed.packed_indices[0i][3u];
    let _e108 = unnamed.packed_indices[0i][3u];
    let _e113 = fog_tex_coord_1;
    let _e114 = textureSample(wired_bindless_images[(_e102 & 4095u)], wired_bindless_samplers[((_e108 >> bitcast<u32>(12i)) & 255u)], _e113);
    fog = _e114;
    param_7 = 0u;
    let _e115 = frag_tex_coord0_1;
    param_8 = _e115;
    param_9 = 0i;
    let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e116;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e121 = sampledColor0_;
    let _e122 = frag_color;
    color0_ = (_e121 * _e122);
    let _e124 = color0_;
    base = _e124;
    if override_type_3_3 {
        let _e126 = color0_[3u];
        if (_e126 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e129 = color0_[3u];
            if (_e129 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e132 = color0_[3u];
                if (_e132 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e134 = color0_;
    base = _e134;
    if override_type_3_6 {
        let _e137 = unnamed.worldLightParams[1u];
        wetness = clamp(_e137, 0f, 1f);
        let _e141 = unnamed.worldLightParams[2u];
        frost = clamp(_e141, 0f, 1f);
        let _e143 = base;
        luminance = dot(_e143.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e146 = wetness;
        let _e148 = base;
        let _e150 = (_e148.xyz * mix(1f, 0.82f, _e146));
        base[0u] = _e150.x;
        base[1u] = _e150.y;
        base[2u] = _e150.z;
        let _e157 = base;
        let _e159 = luminance;
        let _e161 = luminance;
        let _e163 = luminance;
        let _e165 = frost;
        let _e168 = mix(_e157.xyz, vec3<f32>((_e159 * 0.88f), (_e161 * 0.94f), _e163), vec3((_e165 * 0.55f)));
        base[0u] = _e168.x;
        base[1u] = _e168.y;
        base[2u] = _e168.z;
    }
    let _e177 = unnamed.materialFactors[0u];
    if (_e177 >= 0f) {
        let _e180 = unnamed.materialBaseColor;
        let _e181 = base;
        base = (_e181 * _e180);
    }
    let _e183 = color0_;
    let _e186 = unnamed.emissionRadiance;
    let _e189 = base;
    let _e191 = (_e189.xyz + (_e183.xyz * _e186.xyz));
    base[0u] = _e191.x;
    base[1u] = _e191.y;
    base[2u] = _e191.z;
    let _e198 = wired_advanced_fog_enabled_u0028_();
    if _e198 {
        let _e199 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e199;
        if override_type_3_7 {
            let _e200 = fogAmount;
            let _e202 = base;
            let _e204 = (_e202.xyz * (1f - _e200));
            base[0u] = _e204.x;
            base[1u] = _e204.y;
            base[2u] = _e204.z;
        } else {
            if override_type_3_8 {
                let _e211 = fogAmount;
                let _e213 = base;
                base = (_e213 * (1f - _e211));
            } else {
                if override_type_3_9 {
                    let _e215 = fogAmount;
                    let _e218 = base[3u];
                    base[3u] = (_e218 * (1f - _e215));
                } else {
                    let _e221 = base;
                    let _e224 = unnamed.advancedFogColorDensity;
                    let _e226 = fogAmount;
                    let _e228 = mix(_e221.xyz, _e224.xyz, vec3(_e226));
                    base[0u] = _e228.x;
                    base[1u] = _e228.y;
                    base[2u] = _e228.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e235 = base;
            let _e238 = fog[3u];
            let _e240 = (_e235.xyz * (1f - _e238));
            base[0u] = _e240.x;
            base[1u] = _e240.y;
            base[2u] = _e240.z;
        } else {
            if override_type_3_11 {
                let _e247 = base;
                let _e249 = fog[3u];
                base = (_e247 * (1f - _e249));
            } else {
                if override_type_3_12 {
                    let _e253 = base[3u];
                    let _e255 = fog[3u];
                    base[3u] = (_e253 * (1f - _e255));
                } else {
                    let _e259 = base;
                    let _e260 = fog;
                    let _e262 = unnamed.fogColor;
                    let _e265 = fog[3u];
                    base = mix(_e259, (_e260 * _e262), vec4(_e265));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e269 = base[3u];
        if (_e269 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e271 = base;
            let _e273 = base;
            if (dot(_e271.xyz, _e273.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e277 = base;
    out_color = _e277;
    let _e279 = color0_[3u];
    param_10 = _e279;
    let _e280 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_10));
    param_11 = _e280;
    let _e281 = base;
    param_12 = _e281;
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
