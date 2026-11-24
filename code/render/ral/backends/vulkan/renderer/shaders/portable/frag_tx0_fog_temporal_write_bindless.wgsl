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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_83_: bool;

    let _e84 = (*value);
    let _e85 = (*value);
    let _e87 = all((_e84 == _e85));
    phi_83_ = _e87;
    if _e87 {
        let _e88 = (*value);
        phi_83_ = all((abs(_e88) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e93 = phi_83_;
    return _e93;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_68_: bool;

    let _e84 = (*value_1);
    let _e85 = (*value_1);
    let _e87 = all((_e84 == _e85));
    phi_68_ = _e87;
    if _e87 {
        let _e88 = (*value_1);
        phi_68_ = all((abs(_e88) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e93 = phi_68_;
    return _e93;
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
    let _e95 = temporalOutcome_1;
    let _e96 = (_e95 != 1u);
    phi_107_ = _e96;
    if !(_e96) {
        let _e98 = temporalCurrentClip_1;
        param = _e98;
        let _e99 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_107_ = !(_e99);
    }
    let _e102 = phi_107_;
    phi_116_ = _e102;
    if !(_e102) {
        let _e104 = temporalPreviousClip_1;
        param_1 = _e104;
        let _e105 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_116_ = !(_e105);
    }
    let _e108 = phi_116_;
    phi_126_ = _e108;
    if !(_e108) {
        let _e111 = temporalCurrentClip_1[3u];
        phi_126_ = (_e111 <= 0.000001f);
    }
    let _e114 = phi_126_;
    phi_133_ = _e114;
    if !(_e114) {
        let _e117 = temporalPreviousClip_1[3u];
        phi_133_ = (_e117 <= 0.000001f);
    }
    let _e120 = phi_133_;
    if _e120 {
        return;
    }
    let _e121 = temporalCurrentClip_1;
    let _e124 = temporalCurrentClip_1[3u];
    currentNdc = (_e121.xy / vec2(_e124));
    let _e127 = temporalPreviousClip_1;
    let _e130 = temporalPreviousClip_1[3u];
    previousNdc = (_e127.xy / vec2(_e130));
    let _e133 = currentNdc;
    param_2 = _e133;
    let _e134 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e135 = !(_e134);
    phi_162_ = _e135;
    if !(_e135) {
        let _e137 = previousNdc;
        param_3 = _e137;
        let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_162_ = !(_e138);
    }
    let _e141 = phi_162_;
    if _e141 {
        return;
    }
    let _e142 = currentNdc;
    currentUv = ((_e142 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e145 = previousNdc;
    previousUv = ((_e145 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e148 = currentUv;
    let _e149 = previousUv;
    velocity = (_e148 - _e149);
    let _e151 = velocity;
    param_4 = _e151;
    let _e152 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e152) {
        return;
    }
    let _e154 = velocity;
    out_temporal_velocity = _e154;
    let _e155 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e155, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wiredTemporalAtestConfidence_u0028_f1_u003b(alpha: ptr<function, f32>) -> f32 {
    var span: f32;

    if override_type_3_ {
        span = max(max(alpha_test_value, (1f - alpha_test_value)), 0.000001f);
        let _e88 = (*alpha);
        let _e91 = span;
        return clamp((abs((_e88 - alpha_test_value)) / _e91), 0f, 1f);
    } else {
        if override_type_3_1 {
            let _e94 = (*alpha);
            return clamp(((alpha_test_value - _e94) / max(alpha_test_value, 0.000001f)), 0f, 1f);
        } else {
            if override_type_3_2 {
                let _e99 = (*alpha);
                return clamp(((_e99 - alpha_test_value) / max((1f - alpha_test_value), 0.000001f)), 0f, 1f);
            }
        }
    }
    return 1f;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e86 + 0.5f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e93 = fogType;
    let _e96 = fogType;
    return (((_e91 > 0.5f) && (_e93 >= 1i)) && (_e96 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e86 = wired_advanced_fog_enabled_u0028_();
    if !(_e86) {
        return 0f;
    }
    let _e89 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e89, 0.000001f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e94 + 0.5f));
    let _e97 = fogType_1;
    if (_e97 == 1i) {
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e101 <= 0f) {
            return 0f;
        }
        let _e103 = viewDepth;
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e103 / _e106), 0f, 1f);
    }
    let _e111 = unnamed.advancedFogColorDensity[3u];
    let _e113 = viewDepth;
    opticalDepth = (max(_e111, 0f) * _e113);
    let _e115 = fogType_1;
    if (_e115 == 2i) {
        let _e117 = opticalDepth;
        return clamp((1f - exp(-(_e117))), 0f, 1f);
    }
    let _e122 = opticalDepth;
    let _e123 = opticalDepth;
    return clamp((1f - exp(-((_e122 * _e123)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e86 = (*rgb);
    let _e89 = unnamed.worldLightParams[0u];
    boosted = (_e86 * _e89);
    let _e92 = boosted[0u];
    let _e94 = boosted[1u];
    let _e96 = boosted[2u];
    peak = max(_e92, max(_e94, _e96));
    let _e99 = peak;
    if (_e99 > 1f) {
        let _e101 = peak;
        let _e102 = boosted;
        boosted = (_e102 / vec3(_e101));
    }
    let _e105 = boosted;
    return _e105;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e87 = (*c);
    (*c) = max(_e87, vec3<f32>(0f, 0f, 0f));
    let _e89 = (*c);
    cutoff = (_e89 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e91 = (*c);
    lo = (_e91 / vec3(12.92f));
    let _e94 = (*c);
    hi = pow(((_e94 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e99 = hi;
    let _e100 = lo;
    let _e101 = cutoff;
    return mix(_e99, _e100, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e101));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e111 = (*uv);
    let _e112 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    c_1 = _e112;
    let _e113 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e113))) == 0i) {
        let _e118 = c_1;
        param_5 = _e118.xyz;
        let _e120 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = (*slot);
    if (lightmap_slot == (_e127 + 1i)) {
        let _e130 = c_1;
        param_6 = _e130.xyz;
        let _e132 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e132.x;
        c_1[1u] = _e132.y;
        c_1[2u] = _e132.z;
    }
    let _e139 = c_1;
    return _e139;
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
    var param_13: vec4<f32>;

    let _e102 = unnamed.packed_indices[0i][3u];
    let _e108 = unnamed.packed_indices[0i][3u];
    let _e113 = fog_tex_coord_1;
    let _e114 = textureSample(wired_bindless_images[(_e102 & 4095u)], wired_bindless_samplers[((_e108 >> bitcast<u32>(12i)) & 255u)], _e113);
    fog = _e114;
    let _e115 = frag_color0In_1;
    param_7 = _e115.xyz;
    let _e117 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e119 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e117.x, _e117.y, _e117.z, _e119);
    param_8 = 0u;
    let _e124 = frag_tex_coord0_1;
    param_9 = _e124;
    param_10 = 0i;
    let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e125;
    let _e126 = sampledColor0_;
    let _e127 = frag_color0_;
    color0_ = (_e126 * _e127);
    let _e129 = color0_;
    base = _e129;
    if override_type_3_3 {
        let _e131 = color0_[3u];
        if (_e131 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e134 = color0_[3u];
            if (_e134 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_5 {
                let _e137 = color0_[3u];
                if (_e137 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e139 = color0_;
    base = _e139;
    if override_type_3_6 {
        let _e142 = unnamed.worldLightParams[1u];
        wetness = clamp(_e142, 0f, 1f);
        let _e146 = unnamed.worldLightParams[2u];
        frost = clamp(_e146, 0f, 1f);
        let _e148 = base;
        luminance = dot(_e148.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e151 = wetness;
        let _e153 = base;
        let _e155 = (_e153.xyz * mix(1f, 0.82f, _e151));
        base[0u] = _e155.x;
        base[1u] = _e155.y;
        base[2u] = _e155.z;
        let _e162 = base;
        let _e164 = luminance;
        let _e166 = luminance;
        let _e168 = luminance;
        let _e170 = frost;
        let _e173 = mix(_e162.xyz, vec3<f32>((_e164 * 0.88f), (_e166 * 0.94f), _e168), vec3((_e170 * 0.55f)));
        base[0u] = _e173.x;
        base[1u] = _e173.y;
        base[2u] = _e173.z;
    }
    let _e182 = unnamed.materialFactors[0u];
    if (_e182 >= 0f) {
        let _e185 = unnamed.materialBaseColor;
        let _e186 = base;
        base = (_e186 * _e185);
    }
    let _e188 = color0_;
    let _e191 = unnamed.emissionRadiance;
    let _e194 = base;
    let _e196 = (_e194.xyz + (_e188.xyz * _e191.xyz));
    base[0u] = _e196.x;
    base[1u] = _e196.y;
    base[2u] = _e196.z;
    let _e203 = wired_advanced_fog_enabled_u0028_();
    if _e203 {
        let _e204 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e204;
        if override_type_3_7 {
            let _e205 = fogAmount;
            let _e207 = base;
            let _e209 = (_e207.xyz * (1f - _e205));
            base[0u] = _e209.x;
            base[1u] = _e209.y;
            base[2u] = _e209.z;
        } else {
            if override_type_3_8 {
                let _e216 = fogAmount;
                let _e218 = base;
                base = (_e218 * (1f - _e216));
            } else {
                if override_type_3_9 {
                    let _e220 = fogAmount;
                    let _e223 = base[3u];
                    base[3u] = (_e223 * (1f - _e220));
                } else {
                    let _e226 = base;
                    let _e229 = unnamed.advancedFogColorDensity;
                    let _e231 = fogAmount;
                    let _e233 = mix(_e226.xyz, _e229.xyz, vec3(_e231));
                    base[0u] = _e233.x;
                    base[1u] = _e233.y;
                    base[2u] = _e233.z;
                }
            }
        }
    } else {
        if override_type_3_10 {
            let _e240 = base;
            let _e243 = fog[3u];
            let _e245 = (_e240.xyz * (1f - _e243));
            base[0u] = _e245.x;
            base[1u] = _e245.y;
            base[2u] = _e245.z;
        } else {
            if override_type_3_11 {
                let _e252 = base;
                let _e254 = fog[3u];
                base = (_e252 * (1f - _e254));
            } else {
                if override_type_3_12 {
                    let _e258 = base[3u];
                    let _e260 = fog[3u];
                    base[3u] = (_e258 * (1f - _e260));
                } else {
                    let _e264 = base;
                    let _e265 = fog;
                    let _e267 = unnamed.fogColor;
                    let _e270 = fog[3u];
                    base = mix(_e264, (_e265 * _e267), vec4(_e270));
                }
            }
        }
    }
    if override_type_3_13 {
        let _e274 = base[3u];
        if (_e274 == 0f) {
            discard;
        }
    } else {
        if override_type_3_14 {
            let _e276 = base;
            let _e278 = base;
            if (dot(_e276.xyz, _e278.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e282 = base;
    out_color = _e282;
    let _e284 = color0_[3u];
    param_11 = _e284;
    let _e285 = wiredTemporalAtestConfidence_u0028_f1_u003b((&param_11));
    param_12 = _e285;
    let _e286 = base;
    param_13 = _e286;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_12), (&param_13));
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
