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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e81 = (*value);
    let _e82 = (*value);
    let _e84 = all((_e81 == _e82));
    phi_79_ = _e84;
    if _e84 {
        let _e85 = (*value);
        phi_79_ = all((abs(_e85) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e90 = phi_79_;
    return _e90;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e81 = (*value_1);
    let _e82 = (*value_1);
    let _e84 = all((_e81 == _e82));
    phi_64_ = _e84;
    if _e84 {
        let _e85 = (*value_1);
        phi_64_ = all((abs(_e85) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e90 = phi_64_;
    return _e90;
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
    let _e92 = temporalOutcome_1;
    let _e93 = (_e92 != 1u);
    phi_103_ = _e93;
    if !(_e93) {
        let _e95 = temporalCurrentClip_1;
        param = _e95;
        let _e96 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e96);
    }
    let _e99 = phi_103_;
    phi_112_ = _e99;
    if !(_e99) {
        let _e101 = temporalPreviousClip_1;
        param_1 = _e101;
        let _e102 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e102);
    }
    let _e105 = phi_112_;
    phi_122_ = _e105;
    if !(_e105) {
        let _e108 = temporalCurrentClip_1[3u];
        phi_122_ = (_e108 <= 0.000001f);
    }
    let _e111 = phi_122_;
    phi_129_ = _e111;
    if !(_e111) {
        let _e114 = temporalPreviousClip_1[3u];
        phi_129_ = (_e114 <= 0.000001f);
    }
    let _e117 = phi_129_;
    if _e117 {
        return;
    }
    let _e118 = temporalCurrentClip_1;
    let _e121 = temporalCurrentClip_1[3u];
    currentNdc = (_e118.xy / vec2(_e121));
    let _e124 = temporalPreviousClip_1;
    let _e127 = temporalPreviousClip_1[3u];
    previousNdc = (_e124.xy / vec2(_e127));
    let _e130 = currentNdc;
    param_2 = _e130;
    let _e131 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e132 = !(_e131);
    phi_158_ = _e132;
    if !(_e132) {
        let _e134 = previousNdc;
        param_3 = _e134;
        let _e135 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e135);
    }
    let _e138 = phi_158_;
    if _e138 {
        return;
    }
    let _e139 = currentNdc;
    currentUv = ((_e139 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e142 = previousNdc;
    previousUv = ((_e142 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e145 = currentUv;
    let _e146 = previousUv;
    velocity = (_e145 - _e146);
    let _e148 = velocity;
    param_4 = _e148;
    let _e149 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e149) {
        return;
    }
    let _e151 = velocity;
    out_temporal_velocity = _e151;
    let _e152 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e152, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e83 + 0.5f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e90 = fogType;
    let _e93 = fogType;
    return (((_e88 > 0.5f) && (_e90 >= 1i)) && (_e93 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e83 = wired_advanced_fog_enabled_u0028_();
    if !(_e83) {
        return 0f;
    }
    let _e86 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e86, 0.000001f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e91 + 0.5f));
    let _e94 = fogType_1;
    if (_e94 == 1i) {
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e98 <= 0f) {
            return 0f;
        }
        let _e100 = viewDepth;
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e100 / _e103), 0f, 1f);
    }
    let _e108 = unnamed.advancedFogColorDensity[3u];
    let _e110 = viewDepth;
    opticalDepth = (max(_e108, 0f) * _e110);
    let _e112 = fogType_1;
    if (_e112 == 2i) {
        let _e114 = opticalDepth;
        return clamp((1f - exp(-(_e114))), 0f, 1f);
    }
    let _e119 = opticalDepth;
    let _e120 = opticalDepth;
    return clamp((1f - exp(-((_e119 * _e120)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e83 = (*rgb);
    let _e86 = unnamed.worldLightParams[0u];
    boosted = (_e83 * _e86);
    let _e89 = boosted[0u];
    let _e91 = boosted[1u];
    let _e93 = boosted[2u];
    peak = max(_e89, max(_e91, _e93));
    let _e96 = peak;
    if (_e96 > 1f) {
        let _e98 = peak;
        let _e99 = boosted;
        boosted = (_e99 / vec3(_e98));
    }
    let _e102 = boosted;
    return _e102;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e84 = (*c);
    (*c) = max(_e84, vec3<f32>(0f, 0f, 0f));
    let _e86 = (*c);
    cutoff = (_e86 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e88 = (*c);
    lo = (_e88 / vec3(12.92f));
    let _e91 = (*c);
    hi = pow(((_e91 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e96 = hi;
    let _e97 = lo;
    let _e98 = cutoff;
    return mix(_e96, _e97, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e98));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e108 = (*uv);
    let _e109 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    c_1 = _e109;
    let _e110 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e110))) == 0i) {
        let _e115 = c_1;
        param_5 = _e115.xyz;
        let _e117 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = (*slot);
    if (lightmap_slot == (_e124 + 1i)) {
        let _e127 = c_1;
        param_6 = _e127.xyz;
        let _e129 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e129.x;
        c_1[1u] = _e129.y;
        c_1[2u] = _e129.z;
    }
    let _e136 = c_1;
    return _e136;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var sampledColor1_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var sampledColor2_: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_17: f32;
    var param_18: vec4<f32>;

    let _e112 = unnamed.packed_indices[0i][3u];
    let _e118 = unnamed.packed_indices[0i][3u];
    let _e123 = fog_tex_coord_1;
    let _e124 = textureSample(wired_bindless_images[(_e112 & 4095u)], wired_bindless_samplers[((_e118 >> bitcast<u32>(12i)) & 255u)], _e123);
    fog = _e124;
    let _e125 = frag_color0In_1;
    param_7 = _e125.xyz;
    let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e129 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e127.x, _e127.y, _e127.z, _e129);
    param_8 = 0u;
    let _e134 = frag_tex_coord0_1;
    param_9 = _e134;
    param_10 = 0i;
    let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e135;
    param_11 = 1u;
    let _e136 = frag_tex_coord1_1;
    param_12 = _e136;
    param_13 = 1i;
    let _e137 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor1_ = _e137;
    param_14 = 2u;
    let _e138 = frag_tex_coord2_1;
    param_15 = _e138;
    param_16 = 2i;
    let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    sampledColor2_ = _e139;
    let _e140 = sampledColor0_;
    let _e141 = frag_color0_;
    color0_ = (_e140 * _e141);
    if override_type_3_ {
        let _e143 = sampledColor1_;
        color1_ = _e143;
        let _e144 = sampledColor2_;
        color2_ = _e144;
        let _e145 = color0_;
        let _e147 = color1_;
        let _e150 = color2_;
        let _e152 = ((_e145.xyz + _e147.xyz) + _e150.xyz);
        let _e154 = color0_[3u];
        let _e156 = color1_[3u];
        let _e159 = color2_[3u];
        base = vec4<f32>(_e152.x, _e152.y, _e152.z, ((_e154 * _e156) * _e159));
    } else {
        if override_type_3_1 {
            let _e165 = sampledColor1_;
            let _e166 = frag_color0_;
            color1_1 = (_e165 * _e166);
            let _e168 = sampledColor2_;
            let _e169 = frag_color0_;
            color2_1 = (_e168 * _e169);
            let _e171 = color0_;
            let _e173 = color1_1;
            let _e176 = color2_1;
            let _e178 = ((_e171.xyz + _e173.xyz) + _e176.xyz);
            let _e180 = color0_[3u];
            let _e182 = color1_1[3u];
            let _e185 = color2_1[3u];
            base = vec4<f32>(_e178.x, _e178.y, _e178.z, ((_e180 * _e182) * _e185));
        } else {
            let _e191 = sampledColor1_;
            color1_2 = _e191;
            let _e192 = sampledColor2_;
            color2_2 = _e192;
            let _e193 = color0_;
            let _e195 = color1_2;
            let _e198 = color2_2;
            let _e200 = ((_e193.xyz * _e195.xyz) * _e198.xyz);
            base[0u] = _e200.x;
            base[1u] = _e200.y;
            base[2u] = _e200.z;
            let _e208 = color0_[3u];
            let _e210 = color1_2[3u];
            let _e213 = color2_2[3u];
            base[3u] = ((_e208 * _e210) * _e213);
        }
    }
    if override_type_3_2 {
        let _e218 = unnamed.worldLightParams[1u];
        wetness = clamp(_e218, 0f, 1f);
        let _e222 = unnamed.worldLightParams[2u];
        frost = clamp(_e222, 0f, 1f);
        let _e224 = base;
        luminance = dot(_e224.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e227 = wetness;
        let _e229 = base;
        let _e231 = (_e229.xyz * mix(1f, 0.82f, _e227));
        base[0u] = _e231.x;
        base[1u] = _e231.y;
        base[2u] = _e231.z;
        let _e238 = base;
        let _e240 = luminance;
        let _e242 = luminance;
        let _e244 = luminance;
        let _e246 = frost;
        let _e249 = mix(_e238.xyz, vec3<f32>((_e240 * 0.88f), (_e242 * 0.94f), _e244), vec3((_e246 * 0.55f)));
        base[0u] = _e249.x;
        base[1u] = _e249.y;
        base[2u] = _e249.z;
    }
    let _e258 = unnamed.materialFactors[0u];
    if (_e258 >= 0f) {
        let _e261 = unnamed.materialBaseColor;
        let _e262 = base;
        base = (_e262 * _e261);
    }
    let _e264 = color0_;
    let _e267 = unnamed.emissionRadiance;
    let _e270 = base;
    let _e272 = (_e270.xyz + (_e264.xyz * _e267.xyz));
    base[0u] = _e272.x;
    base[1u] = _e272.y;
    base[2u] = _e272.z;
    let _e279 = wired_advanced_fog_enabled_u0028_();
    if _e279 {
        let _e280 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e280;
        if override_type_3_3 {
            let _e281 = fogAmount;
            let _e283 = base;
            let _e285 = (_e283.xyz * (1f - _e281));
            base[0u] = _e285.x;
            base[1u] = _e285.y;
            base[2u] = _e285.z;
        } else {
            if override_type_3_4 {
                let _e292 = fogAmount;
                let _e294 = base;
                base = (_e294 * (1f - _e292));
            } else {
                if override_type_3_5 {
                    let _e296 = fogAmount;
                    let _e299 = base[3u];
                    base[3u] = (_e299 * (1f - _e296));
                } else {
                    let _e302 = base;
                    let _e305 = unnamed.advancedFogColorDensity;
                    let _e307 = fogAmount;
                    let _e309 = mix(_e302.xyz, _e305.xyz, vec3(_e307));
                    base[0u] = _e309.x;
                    base[1u] = _e309.y;
                    base[2u] = _e309.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e316 = base;
            let _e319 = fog[3u];
            let _e321 = (_e316.xyz * (1f - _e319));
            base[0u] = _e321.x;
            base[1u] = _e321.y;
            base[2u] = _e321.z;
        } else {
            if override_type_3_7 {
                let _e328 = base;
                let _e330 = fog[3u];
                base = (_e328 * (1f - _e330));
            } else {
                if override_type_3_8 {
                    let _e334 = base[3u];
                    let _e336 = fog[3u];
                    base[3u] = (_e334 * (1f - _e336));
                } else {
                    let _e340 = base;
                    let _e341 = fog;
                    let _e343 = unnamed.fogColor;
                    let _e346 = fog[3u];
                    base = mix(_e340, (_e341 * _e343), vec4(_e346));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e350 = base[3u];
        if (_e350 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e352 = base;
            let _e354 = base;
            if (dot(_e352.xyz, _e354.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e358 = base;
    out_color = _e358;
    param_17 = 1f;
    let _e359 = base;
    param_18 = _e359;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_17), (&param_18));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
