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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
    var sampledColor0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var sampledColor1_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var frag_color: vec4<f32>;
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

    let _e104 = unnamed.packed_indices[0i][3u];
    let _e110 = unnamed.packed_indices[0i][3u];
    let _e115 = fog_tex_coord_1;
    let _e116 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    fog = _e116;
    param_7 = 0u;
    let _e117 = frag_tex_coord0_1;
    param_8 = _e117;
    param_9 = 0i;
    let _e118 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e118;
    param_10 = 1u;
    let _e119 = frag_tex_coord1_1;
    param_11 = _e119;
    param_12 = 1i;
    let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor1_ = _e120;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e125 = sampledColor0_;
    let _e126 = frag_color;
    color0_ = (_e125 * _e126);
    if override_type_3_ {
        let _e128 = sampledColor1_;
        color1_ = _e128;
        let _e129 = color0_;
        let _e131 = color1_;
        let _e133 = (_e129.xyz + _e131.xyz);
        let _e135 = color0_[3u];
        let _e137 = color1_[3u];
        base = vec4<f32>(_e133.x, _e133.y, _e133.z, (_e135 * _e137));
    } else {
        if override_type_3_1 {
            let _e143 = sampledColor1_;
            let _e144 = frag_color;
            color1_1 = (_e143 * _e144);
            let _e146 = color0_;
            let _e148 = color1_1;
            let _e150 = (_e146.xyz + _e148.xyz);
            let _e152 = color0_[3u];
            let _e154 = color1_1[3u];
            base = vec4<f32>(_e150.x, _e150.y, _e150.z, (_e152 * _e154));
        } else {
            let _e160 = sampledColor1_;
            let _e161 = frag_color;
            color1_2 = (_e160 * _e161);
            let _e163 = color0_;
            let _e165 = color1_2;
            let _e167 = (_e163.xyz * _e165.xyz);
            base[0u] = _e167.x;
            base[1u] = _e167.y;
            base[2u] = _e167.z;
            let _e175 = color0_[3u];
            let _e177 = color1_2[3u];
            base[3u] = (_e175 * _e177);
        }
    }
    if override_type_3_2 {
        let _e182 = unnamed.worldLightParams[1u];
        wetness = clamp(_e182, 0f, 1f);
        let _e186 = unnamed.worldLightParams[2u];
        frost = clamp(_e186, 0f, 1f);
        let _e188 = base;
        luminance = dot(_e188.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e191 = wetness;
        let _e193 = base;
        let _e195 = (_e193.xyz * mix(1f, 0.82f, _e191));
        base[0u] = _e195.x;
        base[1u] = _e195.y;
        base[2u] = _e195.z;
        let _e202 = base;
        let _e204 = luminance;
        let _e206 = luminance;
        let _e208 = luminance;
        let _e210 = frost;
        let _e213 = mix(_e202.xyz, vec3<f32>((_e204 * 0.88f), (_e206 * 0.94f), _e208), vec3((_e210 * 0.55f)));
        base[0u] = _e213.x;
        base[1u] = _e213.y;
        base[2u] = _e213.z;
    }
    let _e222 = unnamed.materialFactors[0u];
    if (_e222 >= 0f) {
        let _e225 = unnamed.materialBaseColor;
        let _e226 = base;
        base = (_e226 * _e225);
    }
    let _e228 = color0_;
    let _e231 = unnamed.emissionRadiance;
    let _e234 = base;
    let _e236 = (_e234.xyz + (_e228.xyz * _e231.xyz));
    base[0u] = _e236.x;
    base[1u] = _e236.y;
    base[2u] = _e236.z;
    let _e243 = wired_advanced_fog_enabled_u0028_();
    if _e243 {
        let _e244 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e244;
        if override_type_3_3 {
            let _e245 = fogAmount;
            let _e247 = base;
            let _e249 = (_e247.xyz * (1f - _e245));
            base[0u] = _e249.x;
            base[1u] = _e249.y;
            base[2u] = _e249.z;
        } else {
            if override_type_3_4 {
                let _e256 = fogAmount;
                let _e258 = base;
                base = (_e258 * (1f - _e256));
            } else {
                if override_type_3_5 {
                    let _e260 = fogAmount;
                    let _e263 = base[3u];
                    base[3u] = (_e263 * (1f - _e260));
                } else {
                    let _e266 = base;
                    let _e269 = unnamed.advancedFogColorDensity;
                    let _e271 = fogAmount;
                    let _e273 = mix(_e266.xyz, _e269.xyz, vec3(_e271));
                    base[0u] = _e273.x;
                    base[1u] = _e273.y;
                    base[2u] = _e273.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e280 = base;
            let _e283 = fog[3u];
            let _e285 = (_e280.xyz * (1f - _e283));
            base[0u] = _e285.x;
            base[1u] = _e285.y;
            base[2u] = _e285.z;
        } else {
            if override_type_3_7 {
                let _e292 = base;
                let _e294 = fog[3u];
                base = (_e292 * (1f - _e294));
            } else {
                if override_type_3_8 {
                    let _e298 = base[3u];
                    let _e300 = fog[3u];
                    base[3u] = (_e298 * (1f - _e300));
                } else {
                    let _e304 = base;
                    let _e305 = fog;
                    let _e307 = unnamed.fogColor;
                    let _e310 = fog[3u];
                    base = mix(_e304, (_e305 * _e307), vec4(_e310));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e314 = base[3u];
        if (_e314 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e316 = base;
            let _e318 = base;
            if (dot(_e316.xyz, _e318.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e322 = base;
    out_color = _e322;
    param_13 = 1f;
    let _e323 = base;
    param_14 = _e323;
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
