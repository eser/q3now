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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e80 = (*value);
    let _e81 = (*value);
    let _e83 = all((_e80 == _e81));
    phi_79_ = _e83;
    if _e83 {
        let _e84 = (*value);
        phi_79_ = all((abs(_e84) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e89 = phi_79_;
    return _e89;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e80 = (*value_1);
    let _e81 = (*value_1);
    let _e83 = all((_e80 == _e81));
    phi_64_ = _e83;
    if _e83 {
        let _e84 = (*value_1);
        phi_64_ = all((abs(_e84) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e89 = phi_64_;
    return _e89;
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
    let _e91 = temporalOutcome_1;
    let _e92 = (_e91 != 1u);
    phi_103_ = _e92;
    if !(_e92) {
        let _e94 = temporalCurrentClip_1;
        param = _e94;
        let _e95 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e95);
    }
    let _e98 = phi_103_;
    phi_112_ = _e98;
    if !(_e98) {
        let _e100 = temporalPreviousClip_1;
        param_1 = _e100;
        let _e101 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e101);
    }
    let _e104 = phi_112_;
    phi_122_ = _e104;
    if !(_e104) {
        let _e107 = temporalCurrentClip_1[3u];
        phi_122_ = (_e107 <= 0.000001f);
    }
    let _e110 = phi_122_;
    phi_129_ = _e110;
    if !(_e110) {
        let _e113 = temporalPreviousClip_1[3u];
        phi_129_ = (_e113 <= 0.000001f);
    }
    let _e116 = phi_129_;
    if _e116 {
        return;
    }
    let _e117 = temporalCurrentClip_1;
    let _e120 = temporalCurrentClip_1[3u];
    currentNdc = (_e117.xy / vec2(_e120));
    let _e123 = temporalPreviousClip_1;
    let _e126 = temporalPreviousClip_1[3u];
    previousNdc = (_e123.xy / vec2(_e126));
    let _e129 = currentNdc;
    param_2 = _e129;
    let _e130 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e131 = !(_e130);
    phi_158_ = _e131;
    if !(_e131) {
        let _e133 = previousNdc;
        param_3 = _e133;
        let _e134 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e134);
    }
    let _e137 = phi_158_;
    if _e137 {
        return;
    }
    let _e138 = currentNdc;
    currentUv = ((_e138 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e141 = previousNdc;
    previousUv = ((_e141 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e144 = currentUv;
    let _e145 = previousUv;
    velocity = (_e144 - _e145);
    let _e147 = velocity;
    param_4 = _e147;
    let _e148 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e148) {
        return;
    }
    let _e150 = velocity;
    out_temporal_velocity = _e150;
    let _e151 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e151, 0f, 1f), 0f, 0f, 1f);
    return;
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
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

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
        param_5 = _e114.xyz;
        let _e116 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = (*slot);
    if (lightmap_slot == (_e123 + 1i)) {
        let _e126 = c_1;
        param_6 = _e126.xyz;
        let _e128 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = c_1;
    return _e135;
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
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_14: f32;
    var param_15: vec4<f32>;

    let _e104 = unnamed.packed_indices[0i][3u];
    let _e110 = unnamed.packed_indices[0i][3u];
    let _e115 = fog_tex_coord_1;
    let _e116 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    fog = _e116;
    let _e117 = frag_color0In_1;
    param_7 = _e117.xyz;
    let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e121 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e119.x, _e119.y, _e119.z, _e121);
    param_8 = 0u;
    let _e126 = frag_tex_coord0_1;
    param_9 = _e126;
    param_10 = 0i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e127;
    param_11 = 1u;
    let _e128 = frag_tex_coord1_1;
    param_12 = _e128;
    param_13 = 1i;
    let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor1_ = _e129;
    let _e130 = sampledColor0_;
    let _e131 = frag_color0_;
    color0_ = (_e130 * _e131);
    if override_type_3_ {
        let _e133 = sampledColor1_;
        color1_ = _e133;
        let _e134 = color0_;
        let _e136 = color1_;
        let _e138 = (_e134.xyz + _e136.xyz);
        let _e140 = color0_[3u];
        let _e142 = color1_[3u];
        base = vec4<f32>(_e138.x, _e138.y, _e138.z, (_e140 * _e142));
    } else {
        if override_type_3_1 {
            let _e148 = sampledColor1_;
            let _e149 = frag_color0_;
            color1_1 = (_e148 * _e149);
            let _e151 = color0_;
            let _e153 = color1_1;
            let _e155 = (_e151.xyz + _e153.xyz);
            let _e157 = color0_[3u];
            let _e159 = color1_1[3u];
            base = vec4<f32>(_e155.x, _e155.y, _e155.z, (_e157 * _e159));
        } else {
            let _e165 = sampledColor1_;
            color1_2 = _e165;
            let _e166 = color0_;
            let _e168 = color1_2;
            let _e170 = (_e166.xyz * _e168.xyz);
            base[0u] = _e170.x;
            base[1u] = _e170.y;
            base[2u] = _e170.z;
            let _e178 = color0_[3u];
            let _e180 = color1_2[3u];
            base[3u] = (_e178 * _e180);
        }
    }
    if override_type_3_2 {
        let _e185 = unnamed.worldLightParams[1u];
        wetness = clamp(_e185, 0f, 1f);
        let _e189 = unnamed.worldLightParams[2u];
        frost = clamp(_e189, 0f, 1f);
        let _e191 = base;
        luminance = dot(_e191.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e194 = wetness;
        let _e196 = base;
        let _e198 = (_e196.xyz * mix(1f, 0.82f, _e194));
        base[0u] = _e198.x;
        base[1u] = _e198.y;
        base[2u] = _e198.z;
        let _e205 = base;
        let _e207 = luminance;
        let _e209 = luminance;
        let _e211 = luminance;
        let _e213 = frost;
        let _e216 = mix(_e205.xyz, vec3<f32>((_e207 * 0.88f), (_e209 * 0.94f), _e211), vec3((_e213 * 0.55f)));
        base[0u] = _e216.x;
        base[1u] = _e216.y;
        base[2u] = _e216.z;
    }
    let _e225 = unnamed.materialFactors[0u];
    if (_e225 >= 0f) {
        let _e228 = unnamed.materialBaseColor;
        let _e229 = base;
        base = (_e229 * _e228);
    }
    let _e231 = color0_;
    let _e234 = unnamed.emissionRadiance;
    let _e237 = base;
    let _e239 = (_e237.xyz + (_e231.xyz * _e234.xyz));
    base[0u] = _e239.x;
    base[1u] = _e239.y;
    base[2u] = _e239.z;
    let _e246 = wired_advanced_fog_enabled_u0028_();
    if _e246 {
        let _e247 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e247;
        if override_type_3_3 {
            let _e248 = fogAmount;
            let _e250 = base;
            let _e252 = (_e250.xyz * (1f - _e248));
            base[0u] = _e252.x;
            base[1u] = _e252.y;
            base[2u] = _e252.z;
        } else {
            if override_type_3_4 {
                let _e259 = fogAmount;
                let _e261 = base;
                base = (_e261 * (1f - _e259));
            } else {
                if override_type_3_5 {
                    let _e263 = fogAmount;
                    let _e266 = base[3u];
                    base[3u] = (_e266 * (1f - _e263));
                } else {
                    let _e269 = base;
                    let _e272 = unnamed.advancedFogColorDensity;
                    let _e274 = fogAmount;
                    let _e276 = mix(_e269.xyz, _e272.xyz, vec3(_e274));
                    base[0u] = _e276.x;
                    base[1u] = _e276.y;
                    base[2u] = _e276.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e283 = base;
            let _e286 = fog[3u];
            let _e288 = (_e283.xyz * (1f - _e286));
            base[0u] = _e288.x;
            base[1u] = _e288.y;
            base[2u] = _e288.z;
        } else {
            if override_type_3_7 {
                let _e295 = base;
                let _e297 = fog[3u];
                base = (_e295 * (1f - _e297));
            } else {
                if override_type_3_8 {
                    let _e301 = base[3u];
                    let _e303 = fog[3u];
                    base[3u] = (_e301 * (1f - _e303));
                } else {
                    let _e307 = base;
                    let _e308 = fog;
                    let _e310 = unnamed.fogColor;
                    let _e313 = fog[3u];
                    base = mix(_e307, (_e308 * _e310), vec4(_e313));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e317 = base[3u];
        if (_e317 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e319 = base;
            let _e321 = base;
            if (dot(_e319.xyz, _e321.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e325 = base;
    out_color = _e325;
    param_14 = 1f;
    let _e326 = base;
    param_15 = _e326;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_14), (&param_15));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
