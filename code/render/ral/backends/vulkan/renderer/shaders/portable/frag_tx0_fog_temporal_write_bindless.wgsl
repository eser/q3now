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
    var phi_82_: bool;

    let _e83 = (*value);
    let _e84 = (*value);
    let _e86 = all((_e83 == _e84));
    phi_82_ = _e86;
    if _e86 {
        let _e87 = (*value);
        phi_82_ = all((abs(_e87) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e92 = phi_82_;
    return _e92;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_67_: bool;

    let _e83 = (*value_1);
    let _e84 = (*value_1);
    let _e86 = all((_e83 == _e84));
    phi_67_ = _e86;
    if _e86 {
        let _e87 = (*value_1);
        phi_67_ = all((abs(_e87) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e92 = phi_67_;
    return _e92;
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
    var phi_105_: bool;
    var phi_114_: bool;
    var phi_124_: bool;
    var phi_131_: bool;
    var phi_160_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e93 = temporalOutcome_1;
    let _e94 = (_e93 != 1u);
    phi_105_ = _e94;
    if !(_e94) {
        let _e96 = temporalCurrentClip_1;
        param = _e96;
        let _e97 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_105_ = !(_e97);
    }
    let _e100 = phi_105_;
    phi_114_ = _e100;
    if !(_e100) {
        let _e102 = temporalPreviousClip_1;
        param_1 = _e102;
        let _e103 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_114_ = !(_e103);
    }
    let _e106 = phi_114_;
    phi_124_ = _e106;
    if !(_e106) {
        let _e109 = temporalCurrentClip_1[3u];
        phi_124_ = (_e109 <= 0.000001f);
    }
    let _e112 = phi_124_;
    phi_131_ = _e112;
    if !(_e112) {
        let _e115 = temporalPreviousClip_1[3u];
        phi_131_ = (_e115 <= 0.000001f);
    }
    let _e118 = phi_131_;
    if _e118 {
        return;
    }
    let _e119 = temporalCurrentClip_1;
    let _e122 = temporalCurrentClip_1[3u];
    currentNdc = (_e119.xy / vec2(_e122));
    let _e125 = temporalPreviousClip_1;
    let _e128 = temporalPreviousClip_1[3u];
    previousNdc = (_e125.xy / vec2(_e128));
    let _e131 = currentNdc;
    param_2 = _e131;
    let _e132 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e133 = !(_e132);
    phi_160_ = _e133;
    if !(_e133) {
        let _e135 = previousNdc;
        param_3 = _e135;
        let _e136 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_160_ = !(_e136);
    }
    let _e139 = phi_160_;
    if _e139 {
        return;
    }
    let _e140 = currentNdc;
    currentUv = ((_e140 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e143 = previousNdc;
    previousUv = ((_e143 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e146 = currentUv;
    let _e147 = previousUv;
    velocity = (_e146 - _e147);
    let _e149 = velocity;
    param_4 = _e149;
    let _e150 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e150) {
        return;
    }
    let _e152 = velocity;
    out_temporal_velocity = _e152;
    let _e153 = (*coverageConfidence);
    out_temporal_validity = clamp(_e153, 0f, 1f);
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
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_11: f32;
    var param_12: f32;

    let _e100 = unnamed.packed_indices[0i][3u];
    let _e106 = unnamed.packed_indices[0i][3u];
    let _e111 = fog_tex_coord_1;
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    fog = _e112;
    let _e113 = frag_color0In_1;
    param_7 = _e113.xyz;
    let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e117 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e115.x, _e115.y, _e115.z, _e117);
    param_8 = 0u;
    let _e122 = frag_tex_coord0_1;
    param_9 = _e122;
    param_10 = 0i;
    let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e123;
    let _e124 = sampledColor0_;
    let _e125 = frag_color0_;
    color0_ = (_e124 * _e125);
    let _e127 = color0_;
    base = _e127;
    if override_type_3_3 {
        let _e129 = color0_[3u];
        if (_e129 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e132 = color0_[3u];
            if (_e132 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e135 = color0_[3u];
                if (_e135 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e137 = color0_;
    base = _e137;
    if override_type_3_6 {
        let _e140 = unnamed.worldLightParams[1u];
        wetness = clamp(_e140, 0f, 1f);
        let _e144 = unnamed.worldLightParams[2u];
        frost = clamp(_e144, 0f, 1f);
        let _e146 = base;
        luminance = dot(_e146.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e149 = wetness;
        let _e151 = base;
        let _e153 = (_e151.xyz * mix(1f, 0.82f, _e149));
        base[0u] = _e153.x;
        base[1u] = _e153.y;
        base[2u] = _e153.z;
        let _e160 = base;
        let _e162 = luminance;
        let _e164 = luminance;
        let _e166 = luminance;
        let _e168 = frost;
        let _e171 = mix(_e160.xyz, vec3<f32>((_e162 * 0.88f), (_e164 * 0.94f), _e166), vec3((_e168 * 0.55f)));
        base[0u] = _e171.x;
        base[1u] = _e171.y;
        base[2u] = _e171.z;
    }
    let _e180 = unnamed.materialFactors[0u];
    if (_e180 >= 0f) {
        let _e183 = unnamed.materialBaseColor;
        let _e184 = base;
        base = (_e184 * _e183);
    }
    let _e186 = color0_;
    let _e189 = unnamed.emissionRadiance;
    let _e192 = base;
    let _e194 = (_e192.xyz + (_e186.xyz * _e189.xyz));
    base[0u] = _e194.x;
    base[1u] = _e194.y;
    base[2u] = _e194.z;
    let _e201 = wired_advanced_fog_enabled_u0028_();
    if _e201 {
        let _e202 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e202;
        if override_type_3_7 {
            let _e203 = fogAmount;
            let _e205 = base;
            let _e207 = (_e205.xyz * (1f - _e203));
            base[0u] = _e207.x;
            base[1u] = _e207.y;
            base[2u] = _e207.z;
        } else {
            if override_type_3_8 {
                let _e214 = fogAmount;
                let _e216 = base;
                base = (_e216 * (1f - _e214));
            } else {
                if override_type_3_9 {
                    let _e218 = fogAmount;
                    let _e221 = base[3u];
                    base[3u] = (_e221 * (1f - _e218));
                } else {
                    let _e224 = base;
                    let _e227 = unnamed.advancedFogColorDensity;
                    let _e229 = fogAmount;
                    let _e231 = mix(_e224.xyz, _e227.xyz, vec3(_e229));
                    base[0u] = _e231.x;
                    base[1u] = _e231.y;
                    base[2u] = _e231.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e238 = base;
            let _e241 = fog[3u];
            let _e243 = (_e238.xyz * (1f - _e241));
            base[0u] = _e243.x;
            base[1u] = _e243.y;
            base[2u] = _e243.z;
        } else {
            if override_type_3_11 {
                let _e250 = base;
                let _e252 = fog[3u];
                base = (_e250 * (1f - _e252));
            } else {
                if override_type_3_12 {
                    let _e256 = base[3u];
                    let _e258 = fog[3u];
                    base[3u] = (_e256 * (1f - _e258));
                } else {
                    let _e262 = base;
                    let _e263 = fog;
                    let _e265 = unnamed.fogColor;
                    let _e268 = fog[3u];
                    base = mix(_e262, (_e263 * _e265), vec4(_e268));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e272 = base[3u];
        if (_e272 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e274 = base;
            let _e276 = base;
            if (dot(_e274.xyz, _e276.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e280 = base;
    out_color = _e280;
    let _e282 = color0_[3u];
    param_11 = _e282;
    let _e283 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_11));
    param_12 = _e283;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_12));
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
