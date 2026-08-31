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
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e79 = (*value);
    let _e80 = (*value);
    let _e82 = all((_e79 == _e80));
    phi_78_ = _e82;
    if _e82 {
        let _e83 = (*value);
        phi_78_ = all((abs(_e83) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e88 = phi_78_;
    return _e88;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e79 = (*value_1);
    let _e80 = (*value_1);
    let _e82 = all((_e79 == _e80));
    phi_63_ = _e82;
    if _e82 {
        let _e83 = (*value_1);
        phi_63_ = all((abs(_e83) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e88 = phi_63_;
    return _e88;
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
    var phi_101_: bool;
    var phi_110_: bool;
    var phi_120_: bool;
    var phi_127_: bool;
    var phi_156_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e89 = temporalOutcome_1;
    let _e90 = (_e89 != 1u);
    phi_101_ = _e90;
    if !(_e90) {
        let _e92 = temporalCurrentClip_1;
        param = _e92;
        let _e93 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e93);
    }
    let _e96 = phi_101_;
    phi_110_ = _e96;
    if !(_e96) {
        let _e98 = temporalPreviousClip_1;
        param_1 = _e98;
        let _e99 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e99);
    }
    let _e102 = phi_110_;
    phi_120_ = _e102;
    if !(_e102) {
        let _e105 = temporalCurrentClip_1[3u];
        phi_120_ = (_e105 <= 0.000001f);
    }
    let _e108 = phi_120_;
    phi_127_ = _e108;
    if !(_e108) {
        let _e111 = temporalPreviousClip_1[3u];
        phi_127_ = (_e111 <= 0.000001f);
    }
    let _e114 = phi_127_;
    if _e114 {
        return;
    }
    let _e115 = temporalCurrentClip_1;
    let _e118 = temporalCurrentClip_1[3u];
    currentNdc = (_e115.xy / vec2(_e118));
    let _e121 = temporalPreviousClip_1;
    let _e124 = temporalPreviousClip_1[3u];
    previousNdc = (_e121.xy / vec2(_e124));
    let _e127 = currentNdc;
    param_2 = _e127;
    let _e128 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e129 = !(_e128);
    phi_156_ = _e129;
    if !(_e129) {
        let _e131 = previousNdc;
        param_3 = _e131;
        let _e132 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e132);
    }
    let _e135 = phi_156_;
    if _e135 {
        return;
    }
    let _e136 = currentNdc;
    currentUv = ((_e136 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e139 = previousNdc;
    previousUv = ((_e139 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e142 = currentUv;
    let _e143 = previousUv;
    velocity = (_e142 - _e143);
    let _e145 = velocity;
    param_4 = _e145;
    let _e146 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e146) {
        return;
    }
    let _e148 = velocity;
    out_temporal_velocity = _e148;
    let _e149 = (*coverageConfidence);
    out_temporal_validity = clamp(_e149, 0f, 1f);
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
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var color0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_2: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_3: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_4: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color1_5: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color1_6: vec4<f32>;
    var param_30: u32;
    var param_31: vec2<f32>;
    var param_32: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_33: f32;

    let _e120 = frag_color0In_1;
    param_7 = _e120.xyz;
    let _e122 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e124 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e122.x, _e122.y, _e122.z, _e124);
    let _e129 = frag_color1In_1;
    param_8 = _e129.xyz;
    let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e133 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e131.x, _e131.y, _e131.z, _e133);
    param_9 = 0u;
    let _e138 = frag_tex_coord0_1;
    param_10 = _e138;
    param_11 = 0i;
    let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    let _e140 = frag_color0_;
    color0_ = (_e139 * _e140);
    if override_type_3_2 {
        param_12 = 1u;
        let _e142 = frag_tex_coord1_1;
        param_13 = _e142;
        param_14 = 1i;
        let _e143 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
        let _e144 = frag_color1_;
        color1_ = (_e143 * _e144);
        let _e146 = color0_;
        let _e148 = color1_;
        let _e150 = (_e146.xyz + _e148.xyz);
        let _e152 = color0_[3u];
        let _e154 = color1_[3u];
        base = vec4<f32>(_e150.x, _e150.y, _e150.z, (_e152 * _e154));
    } else {
        if override_type_3_3 {
            param_15 = 1u;
            let _e160 = frag_tex_coord1_1;
            param_16 = _e160;
            param_17 = 1i;
            let _e161 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            let _e162 = frag_color1_;
            color1_1 = (_e161 * _e162);
            let _e165 = color0_[3u];
            let _e166 = color0_;
            color0_ = (_e166 * _e165);
            let _e169 = color1_1[3u];
            let _e170 = color1_1;
            color1_1 = (_e170 * _e169);
            let _e172 = color0_;
            let _e174 = color1_1;
            let _e176 = (_e172.xyz + _e174.xyz);
            let _e178 = color0_[3u];
            let _e180 = color1_1[3u];
            base = vec4<f32>(_e176.x, _e176.y, _e176.z, (_e178 * _e180));
        } else {
            if override_type_3_4 {
                param_18 = 1u;
                let _e186 = frag_tex_coord1_1;
                param_19 = _e186;
                param_20 = 1i;
                let _e187 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                let _e188 = frag_color1_;
                color1_2 = (_e187 * _e188);
                let _e191 = color0_[3u];
                let _e193 = color0_;
                color0_ = (_e193 * (1f - _e191));
                let _e196 = color1_2[3u];
                let _e198 = color1_2;
                color1_2 = (_e198 * (1f - _e196));
                let _e200 = color0_;
                let _e202 = color1_2;
                let _e204 = (_e200.xyz + _e202.xyz);
                let _e206 = color0_[3u];
                let _e208 = color1_2[3u];
                base = vec4<f32>(_e204.x, _e204.y, _e204.z, (_e206 * _e208));
            } else {
                if override_type_3_5 {
                    param_21 = 1u;
                    let _e214 = frag_tex_coord1_1;
                    param_22 = _e214;
                    param_23 = 1i;
                    let _e215 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                    let _e216 = frag_color1_;
                    color1_3 = (_e215 * _e216);
                    let _e218 = color0_;
                    let _e219 = color1_3;
                    let _e221 = color1_3[3u];
                    base = mix(_e218, _e219, vec4(_e221));
                } else {
                    if override_type_3_6 {
                        param_24 = 1u;
                        let _e224 = frag_tex_coord1_1;
                        param_25 = _e224;
                        param_26 = 1i;
                        let _e225 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                        let _e226 = frag_color1_;
                        color1_4 = (_e225 * _e226);
                        let _e228 = color1_4;
                        let _e229 = color0_;
                        let _e231 = color1_4[3u];
                        base = mix(_e228, _e229, vec4(_e231));
                    } else {
                        if override_type_3_7 {
                            param_27 = 1u;
                            let _e234 = frag_tex_coord1_1;
                            param_28 = _e234;
                            param_29 = 1i;
                            let _e235 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
                            let _e236 = frag_color1_;
                            color1_5 = (_e235 * _e236);
                            let _e238 = color1_5;
                            let _e240 = color1_5[3u];
                            let _e243 = color0_;
                            base = ((_e238 + vec4(_e240)) * _e243);
                        } else {
                            param_30 = 1u;
                            let _e245 = frag_tex_coord1_1;
                            param_31 = _e245;
                            param_32 = 1i;
                            let _e246 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
                            let _e247 = frag_color1_;
                            color1_6 = (_e246 * _e247);
                            let _e249 = color0_;
                            let _e251 = color1_6;
                            let _e253 = (_e249.xyz * _e251.xyz);
                            base[0u] = _e253.x;
                            base[1u] = _e253.y;
                            base[2u] = _e253.z;
                            let _e261 = color0_[3u];
                            let _e263 = color1_6[3u];
                            base[3u] = (_e261 * _e263);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e268 = unnamed.worldLightParams[1u];
        wetness = clamp(_e268, 0f, 1f);
        let _e272 = unnamed.worldLightParams[2u];
        frost = clamp(_e272, 0f, 1f);
        let _e274 = base;
        luminance = dot(_e274.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e277 = wetness;
        let _e279 = base;
        let _e281 = (_e279.xyz * mix(1f, 0.82f, _e277));
        base[0u] = _e281.x;
        base[1u] = _e281.y;
        base[2u] = _e281.z;
        let _e288 = base;
        let _e290 = luminance;
        let _e292 = luminance;
        let _e294 = luminance;
        let _e296 = frost;
        let _e299 = mix(_e288.xyz, vec3<f32>((_e290 * 0.88f), (_e292 * 0.94f), _e294), vec3((_e296 * 0.55f)));
        base[0u] = _e299.x;
        base[1u] = _e299.y;
        base[2u] = _e299.z;
    }
    let _e306 = color0_;
    let _e309 = unnamed.emissionRadiance;
    let _e312 = base;
    let _e314 = (_e312.xyz + (_e306.xyz * _e309.xyz));
    base[0u] = _e314.x;
    base[1u] = _e314.y;
    base[2u] = _e314.z;
    let _e321 = wired_advanced_fog_enabled_u0028_();
    if _e321 {
        let _e322 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e322;
        let _e323 = base;
        let _e326 = unnamed.advancedFogColorDensity;
        let _e328 = fogAmount;
        let _e330 = mix(_e323.xyz, _e326.xyz, vec3(_e328));
        base[0u] = _e330.x;
        base[1u] = _e330.y;
        base[2u] = _e330.z;
    }
    if override_type_3_9 {
        let _e338 = base[3u];
        if (_e338 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e340 = base;
            let _e342 = base;
            if (dot(_e340.xyz, _e342.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e346 = base;
    out_color = _e346;
    param_33 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_33));
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
