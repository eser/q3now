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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
override override_type_3_8: bool = (lightmap_slot != 0i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e82 = (*value);
    let _e83 = (*value);
    let _e85 = all((_e82 == _e83));
    phi_79_ = _e85;
    if _e85 {
        let _e86 = (*value);
        phi_79_ = all((abs(_e86) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e91 = phi_79_;
    return _e91;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e82 = (*value_1);
    let _e83 = (*value_1);
    let _e85 = all((_e82 == _e83));
    phi_64_ = _e85;
    if _e85 {
        let _e86 = (*value_1);
        phi_64_ = all((abs(_e86) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e91 = phi_64_;
    return _e91;
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
    let _e93 = temporalOutcome_1;
    let _e94 = (_e93 != 1u);
    phi_103_ = _e94;
    if !(_e94) {
        let _e96 = temporalCurrentClip_1;
        param = _e96;
        let _e97 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e97);
    }
    let _e100 = phi_103_;
    phi_112_ = _e100;
    if !(_e100) {
        let _e102 = temporalPreviousClip_1;
        param_1 = _e102;
        let _e103 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e103);
    }
    let _e106 = phi_112_;
    phi_122_ = _e106;
    if !(_e106) {
        let _e109 = temporalCurrentClip_1[3u];
        phi_122_ = (_e109 <= 0.000001f);
    }
    let _e112 = phi_122_;
    phi_129_ = _e112;
    if !(_e112) {
        let _e115 = temporalPreviousClip_1[3u];
        phi_129_ = (_e115 <= 0.000001f);
    }
    let _e118 = phi_129_;
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
    phi_158_ = _e133;
    if !(_e133) {
        let _e135 = previousNdc;
        param_3 = _e135;
        let _e136 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e136);
    }
    let _e139 = phi_158_;
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
    out_temporal_validity = vec4<f32>(clamp(_e153, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e84 + 0.5f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e91 = fogType;
    let _e94 = fogType;
    return (((_e89 > 0.5f) && (_e91 >= 1i)) && (_e94 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e84 = wired_advanced_fog_enabled_u0028_();
    if !(_e84) {
        return 0f;
    }
    let _e87 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e87, 0.000001f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e92 + 0.5f));
    let _e95 = fogType_1;
    if (_e95 == 1i) {
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e99 <= 0f) {
            return 0f;
        }
        let _e101 = viewDepth;
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e101 / _e104), 0f, 1f);
    }
    let _e109 = unnamed.advancedFogColorDensity[3u];
    let _e111 = viewDepth;
    opticalDepth = (max(_e109, 0f) * _e111);
    let _e113 = fogType_1;
    if (_e113 == 2i) {
        let _e115 = opticalDepth;
        return clamp((1f - exp(-(_e115))), 0f, 1f);
    }
    let _e120 = opticalDepth;
    let _e121 = opticalDepth;
    return clamp((1f - exp(-((_e120 * _e121)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e84 = (*rgb);
    let _e87 = unnamed.worldLightParams[0u];
    boosted = (_e84 * _e87);
    let _e90 = boosted[0u];
    let _e92 = boosted[1u];
    let _e94 = boosted[2u];
    peak = max(_e90, max(_e92, _e94));
    let _e97 = peak;
    if (_e97 > 1f) {
        let _e99 = peak;
        let _e100 = boosted;
        boosted = (_e100 / vec3(_e99));
    }
    let _e103 = boosted;
    return _e103;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e85 = (*c);
    (*c) = max(_e85, vec3<f32>(0f, 0f, 0f));
    let _e87 = (*c);
    cutoff = (_e87 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e89 = (*c);
    lo = (_e89 / vec3(12.92f));
    let _e92 = (*c);
    hi = pow(((_e92 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e97 = hi;
    let _e98 = lo;
    let _e99 = cutoff;
    return mix(_e97, _e98, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e99));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e109 = (*uv);
    let _e110 = textureSample(wired_bindless_images[(_e94 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    c_1 = _e110;
    let _e111 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e111))) == 0i) {
        let _e116 = c_1;
        param_5 = _e116.xyz;
        let _e118 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = (*slot);
    if (lightmap_slot == (_e125 + 1i)) {
        let _e128 = c_1;
        param_6 = _e128.xyz;
        let _e130 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e130.x;
        c_1[1u] = _e130.y;
        c_1[2u] = _e130.z;
    }
    let _e137 = c_1;
    return _e137;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var sampledColor1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_15: f32;
    var param_16: vec4<f32>;

    let _e108 = frag_color0In_1;
    param_7 = _e108.xyz;
    let _e110 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e112 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e110.x, _e110.y, _e110.z, _e112);
    let _e117 = frag_color1In_1;
    param_8 = _e117.xyz;
    let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e121 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e119.x, _e119.y, _e119.z, _e121);
    param_9 = 0u;
    let _e126 = frag_tex_coord0_1;
    param_10 = _e126;
    param_11 = 0i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor0_ = _e127;
    param_12 = 1u;
    let _e128 = frag_tex_coord1_1;
    param_13 = _e128;
    param_14 = 1i;
    let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    sampledColor1_ = _e129;
    let _e130 = sampledColor0_;
    let _e131 = frag_color0_;
    color0_ = (_e130 * _e131);
    if override_type_3_2 {
        let _e133 = sampledColor1_;
        let _e134 = frag_color1_;
        color1_ = (_e133 * _e134);
        let _e136 = color0_;
        let _e138 = color1_;
        let _e140 = (_e136.xyz + _e138.xyz);
        let _e142 = color0_[3u];
        let _e144 = color1_[3u];
        base = vec4<f32>(_e140.x, _e140.y, _e140.z, (_e142 * _e144));
    } else {
        if override_type_3_3 {
            let _e150 = sampledColor1_;
            let _e151 = frag_color1_;
            color1_1 = (_e150 * _e151);
            let _e154 = color0_[3u];
            let _e155 = color0_;
            color0_ = (_e155 * _e154);
            let _e158 = color1_1[3u];
            let _e159 = color1_1;
            color1_1 = (_e159 * _e158);
            let _e161 = color0_;
            let _e163 = color1_1;
            let _e165 = (_e161.xyz + _e163.xyz);
            let _e167 = color0_[3u];
            let _e169 = color1_1[3u];
            base = vec4<f32>(_e165.x, _e165.y, _e165.z, (_e167 * _e169));
        } else {
            if override_type_3_4 {
                let _e175 = sampledColor1_;
                let _e176 = frag_color1_;
                color1_2 = (_e175 * _e176);
                let _e179 = color0_[3u];
                let _e181 = color0_;
                color0_ = (_e181 * (1f - _e179));
                let _e184 = color1_2[3u];
                let _e186 = color1_2;
                color1_2 = (_e186 * (1f - _e184));
                let _e188 = color0_;
                let _e190 = color1_2;
                let _e192 = (_e188.xyz + _e190.xyz);
                let _e194 = color0_[3u];
                let _e196 = color1_2[3u];
                base = vec4<f32>(_e192.x, _e192.y, _e192.z, (_e194 * _e196));
            } else {
                if override_type_3_5 {
                    let _e202 = sampledColor1_;
                    let _e203 = frag_color1_;
                    color1_3 = (_e202 * _e203);
                    let _e205 = color0_;
                    let _e206 = color1_3;
                    let _e208 = color1_3[3u];
                    base = mix(_e205, _e206, vec4(_e208));
                } else {
                    if override_type_3_6 {
                        let _e211 = sampledColor1_;
                        let _e212 = frag_color1_;
                        color1_4 = (_e211 * _e212);
                        let _e214 = color1_4;
                        let _e215 = color0_;
                        let _e217 = color1_4[3u];
                        base = mix(_e214, _e215, vec4(_e217));
                    } else {
                        if override_type_3_7 {
                            let _e220 = sampledColor1_;
                            let _e221 = frag_color1_;
                            color1_5 = (_e220 * _e221);
                            let _e223 = color1_5;
                            let _e225 = color1_5[3u];
                            let _e228 = color0_;
                            base = ((_e223 + vec4(_e225)) * _e228);
                        } else {
                            let _e230 = sampledColor1_;
                            let _e231 = frag_color1_;
                            color1_6 = (_e230 * _e231);
                            let _e233 = color0_;
                            let _e235 = color1_6;
                            let _e237 = (_e233.xyz * _e235.xyz);
                            base[0u] = _e237.x;
                            base[1u] = _e237.y;
                            base[2u] = _e237.z;
                            let _e245 = color0_[3u];
                            let _e247 = color1_6[3u];
                            base[3u] = (_e245 * _e247);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e252 = unnamed.worldLightParams[1u];
        wetness = clamp(_e252, 0f, 1f);
        let _e256 = unnamed.worldLightParams[2u];
        frost = clamp(_e256, 0f, 1f);
        let _e258 = base;
        luminance = dot(_e258.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e261 = wetness;
        let _e263 = base;
        let _e265 = (_e263.xyz * mix(1f, 0.82f, _e261));
        base[0u] = _e265.x;
        base[1u] = _e265.y;
        base[2u] = _e265.z;
        let _e272 = base;
        let _e274 = luminance;
        let _e276 = luminance;
        let _e278 = luminance;
        let _e280 = frost;
        let _e283 = mix(_e272.xyz, vec3<f32>((_e274 * 0.88f), (_e276 * 0.94f), _e278), vec3((_e280 * 0.55f)));
        base[0u] = _e283.x;
        base[1u] = _e283.y;
        base[2u] = _e283.z;
    }
    let _e292 = unnamed.materialFactors[0u];
    if (_e292 >= 0f) {
        let _e295 = unnamed.materialBaseColor;
        let _e296 = base;
        base = (_e296 * _e295);
    }
    let _e298 = color0_;
    let _e301 = unnamed.emissionRadiance;
    let _e304 = base;
    let _e306 = (_e304.xyz + (_e298.xyz * _e301.xyz));
    base[0u] = _e306.x;
    base[1u] = _e306.y;
    base[2u] = _e306.z;
    let _e313 = wired_advanced_fog_enabled_u0028_();
    if _e313 {
        let _e314 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e314;
        let _e315 = base;
        let _e318 = unnamed.advancedFogColorDensity;
        let _e320 = fogAmount;
        let _e322 = mix(_e315.xyz, _e318.xyz, vec3(_e320));
        base[0u] = _e322.x;
        base[1u] = _e322.y;
        base[2u] = _e322.z;
    }
    if override_type_3_9 {
        let _e330 = base[3u];
        if (_e330 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e332 = base;
            let _e334 = base;
            if (dot(_e332.xyz, _e334.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e338 = base;
    out_color = _e338;
    param_15 = 1f;
    let _e339 = base;
    param_16 = _e339;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_15), (&param_16));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
