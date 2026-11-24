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

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_3: bool = (acff == 1i);
override override_type_3_4: bool = (acff == 2i);
override override_type_3_5: bool = (acff == 3i);
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e79 = (*value);
    let _e80 = (*value);
    let _e82 = all((_e79 == _e80));
    phi_79_ = _e82;
    if _e82 {
        let _e83 = (*value);
        phi_79_ = all((abs(_e83) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e88 = phi_79_;
    return _e88;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e79 = (*value_1);
    let _e80 = (*value_1);
    let _e82 = all((_e79 == _e80));
    phi_64_ = _e82;
    if _e82 {
        let _e83 = (*value_1);
        phi_64_ = all((abs(_e83) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e88 = phi_64_;
    return _e88;
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
    var phi_103_: bool;
    var phi_112_: bool;
    var phi_122_: bool;
    var phi_129_: bool;
    var phi_158_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e90 = temporalOutcome_1;
    let _e91 = (_e90 != 1u);
    phi_103_ = _e91;
    if !(_e91) {
        let _e93 = temporalCurrentClip_1;
        param = _e93;
        let _e94 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e94);
    }
    let _e97 = phi_103_;
    phi_112_ = _e97;
    if !(_e97) {
        let _e99 = temporalPreviousClip_1;
        param_1 = _e99;
        let _e100 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e100);
    }
    let _e103 = phi_112_;
    phi_122_ = _e103;
    if !(_e103) {
        let _e106 = temporalCurrentClip_1[3u];
        phi_122_ = (_e106 <= 0.000001f);
    }
    let _e109 = phi_122_;
    phi_129_ = _e109;
    if !(_e109) {
        let _e112 = temporalPreviousClip_1[3u];
        phi_129_ = (_e112 <= 0.000001f);
    }
    let _e115 = phi_129_;
    if _e115 {
        return;
    }
    let _e116 = temporalCurrentClip_1;
    let _e119 = temporalCurrentClip_1[3u];
    currentNdc = (_e116.xy / vec2(_e119));
    let _e122 = temporalPreviousClip_1;
    let _e125 = temporalPreviousClip_1[3u];
    previousNdc = (_e122.xy / vec2(_e125));
    let _e128 = currentNdc;
    param_2 = _e128;
    let _e129 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e130 = !(_e129);
    phi_158_ = _e130;
    if !(_e130) {
        let _e132 = previousNdc;
        param_3 = _e132;
        let _e133 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e133);
    }
    let _e136 = phi_158_;
    if _e136 {
        return;
    }
    let _e137 = currentNdc;
    currentUv = ((_e137 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e140 = previousNdc;
    previousUv = ((_e140 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e143 = currentUv;
    let _e144 = previousUv;
    velocity = (_e143 - _e144);
    let _e146 = velocity;
    param_4 = _e146;
    let _e147 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e147) {
        return;
    }
    let _e149 = velocity;
    out_temporal_velocity = _e149;
    let _e150 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e150, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e81 + 0.5f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e88 = fogType;
    let _e91 = fogType;
    return (((_e86 > 0.5f) && (_e88 >= 1i)) && (_e91 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e81 = wired_advanced_fog_enabled_u0028_();
    if !(_e81) {
        return 0f;
    }
    let _e84 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e84, 0.000001f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e89 + 0.5f));
    let _e92 = fogType_1;
    if (_e92 == 1i) {
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e96 <= 0f) {
            return 0f;
        }
        let _e98 = viewDepth;
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e98 / _e101), 0f, 1f);
    }
    let _e106 = unnamed.advancedFogColorDensity[3u];
    let _e108 = viewDepth;
    opticalDepth = (max(_e106, 0f) * _e108);
    let _e110 = fogType_1;
    if (_e110 == 2i) {
        let _e112 = opticalDepth;
        return clamp((1f - exp(-(_e112))), 0f, 1f);
    }
    let _e117 = opticalDepth;
    let _e118 = opticalDepth;
    return clamp((1f - exp(-((_e117 * _e118)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e81 = (*rgb);
    let _e84 = unnamed.worldLightParams[0u];
    boosted = (_e81 * _e84);
    let _e87 = boosted[0u];
    let _e89 = boosted[1u];
    let _e91 = boosted[2u];
    peak = max(_e87, max(_e89, _e91));
    let _e94 = peak;
    if (_e94 > 1f) {
        let _e96 = peak;
        let _e97 = boosted;
        boosted = (_e97 / vec3(_e96));
    }
    let _e100 = boosted;
    return _e100;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e82 = (*c);
    (*c) = max(_e82, vec3<f32>(0f, 0f, 0f));
    let _e84 = (*c);
    cutoff = (_e84 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e86 = (*c);
    lo = (_e86 / vec3(12.92f));
    let _e89 = (*c);
    hi = pow(((_e89 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e94 = hi;
    let _e95 = lo;
    let _e96 = cutoff;
    return mix(_e94, _e95, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e96));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e94 = (*role);
    let _e96 = (*role);
    let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
    let _e106 = (*uv);
    let _e107 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e101 >> bitcast<u32>(12i)) & 255u)], _e106);
    c_1 = _e107;
    let _e108 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e108))) == 0i) {
        let _e113 = c_1;
        param_5 = _e113.xyz;
        let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = (*slot);
    if (lightmap_slot == (_e122 + 1i)) {
        let _e125 = c_1;
        param_6 = _e125.xyz;
        let _e127 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e127.x;
        c_1[1u] = _e127.y;
        c_1[2u] = _e127.z;
    }
    let _e134 = c_1;
    return _e134;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var sampledColor1_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_13: f32;
    var param_14: vec4<f32>;

    let _e101 = unnamed.packed_indices[0i][3u];
    let _e107 = unnamed.packed_indices[0i][3u];
    let _e112 = fog_tex_coord_1;
    let _e113 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e107 >> bitcast<u32>(12i)) & 255u)], _e112);
    fog = _e113;
    param_7 = 0u;
    let _e114 = frag_tex_coord0_1;
    param_8 = _e114;
    param_9 = 0i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e115;
    param_10 = 1u;
    let _e116 = frag_tex_coord1_1;
    param_11 = _e116;
    param_12 = 1i;
    let _e117 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor1_ = _e117;
    let _e118 = sampledColor0_;
    color0_ = _e118;
    if override_type_3_ {
        let _e119 = sampledColor1_;
        color1_ = _e119;
        let _e120 = color0_;
        let _e122 = color1_;
        let _e124 = (_e120.xyz + _e122.xyz);
        let _e126 = color0_[3u];
        let _e128 = color1_[3u];
        base = vec4<f32>(_e124.x, _e124.y, _e124.z, (_e126 * _e128));
    } else {
        if override_type_3_1 {
            let _e134 = sampledColor1_;
            color1_1 = _e134;
            let _e135 = color0_;
            let _e137 = color1_1;
            let _e139 = (_e135.xyz + _e137.xyz);
            let _e141 = color0_[3u];
            let _e143 = color1_1[3u];
            base = vec4<f32>(_e139.x, _e139.y, _e139.z, (_e141 * _e143));
        } else {
            let _e149 = sampledColor1_;
            color1_2 = _e149;
            let _e150 = color0_;
            let _e152 = color1_2;
            let _e154 = (_e150.xyz * _e152.xyz);
            base[0u] = _e154.x;
            base[1u] = _e154.y;
            base[2u] = _e154.z;
            let _e162 = color0_[3u];
            let _e164 = color1_2[3u];
            base[3u] = (_e162 * _e164);
        }
    }
    if override_type_3_2 {
        let _e169 = unnamed.worldLightParams[1u];
        wetness = clamp(_e169, 0f, 1f);
        let _e173 = unnamed.worldLightParams[2u];
        frost = clamp(_e173, 0f, 1f);
        let _e175 = base;
        luminance = dot(_e175.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e178 = wetness;
        let _e180 = base;
        let _e182 = (_e180.xyz * mix(1f, 0.82f, _e178));
        base[0u] = _e182.x;
        base[1u] = _e182.y;
        base[2u] = _e182.z;
        let _e189 = base;
        let _e191 = luminance;
        let _e193 = luminance;
        let _e195 = luminance;
        let _e197 = frost;
        let _e200 = mix(_e189.xyz, vec3<f32>((_e191 * 0.88f), (_e193 * 0.94f), _e195), vec3((_e197 * 0.55f)));
        base[0u] = _e200.x;
        base[1u] = _e200.y;
        base[2u] = _e200.z;
    }
    let _e209 = unnamed.materialFactors[0u];
    if (_e209 >= 0f) {
        let _e212 = unnamed.materialBaseColor;
        let _e213 = base;
        base = (_e213 * _e212);
    }
    let _e215 = color0_;
    let _e218 = unnamed.emissionRadiance;
    let _e221 = base;
    let _e223 = (_e221.xyz + (_e215.xyz * _e218.xyz));
    base[0u] = _e223.x;
    base[1u] = _e223.y;
    base[2u] = _e223.z;
    let _e230 = wired_advanced_fog_enabled_u0028_();
    if _e230 {
        let _e231 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e231;
        if override_type_3_3 {
            let _e232 = fogAmount;
            let _e234 = base;
            let _e236 = (_e234.xyz * (1f - _e232));
            base[0u] = _e236.x;
            base[1u] = _e236.y;
            base[2u] = _e236.z;
        } else {
            if override_type_3_4 {
                let _e243 = fogAmount;
                let _e245 = base;
                base = (_e245 * (1f - _e243));
            } else {
                if override_type_3_5 {
                    let _e247 = fogAmount;
                    let _e250 = base[3u];
                    base[3u] = (_e250 * (1f - _e247));
                } else {
                    let _e253 = base;
                    let _e256 = unnamed.advancedFogColorDensity;
                    let _e258 = fogAmount;
                    let _e260 = mix(_e253.xyz, _e256.xyz, vec3(_e258));
                    base[0u] = _e260.x;
                    base[1u] = _e260.y;
                    base[2u] = _e260.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e267 = base;
            let _e270 = fog[3u];
            let _e272 = (_e267.xyz * (1f - _e270));
            base[0u] = _e272.x;
            base[1u] = _e272.y;
            base[2u] = _e272.z;
        } else {
            if override_type_3_7 {
                let _e279 = base;
                let _e281 = fog[3u];
                base = (_e279 * (1f - _e281));
            } else {
                if override_type_3_8 {
                    let _e285 = base[3u];
                    let _e287 = fog[3u];
                    base[3u] = (_e285 * (1f - _e287));
                } else {
                    let _e291 = base;
                    let _e292 = fog;
                    let _e294 = unnamed.fogColor;
                    let _e297 = fog[3u];
                    base = mix(_e291, (_e292 * _e294), vec4(_e297));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e301 = base[3u];
        if (_e301 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e303 = base;
            let _e305 = base;
            if (dot(_e303.xyz, _e305.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e309 = base;
    out_color = _e309;
    param_13 = 1f;
    let _e310 = base;
    param_14 = _e310;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_13), (&param_14));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
