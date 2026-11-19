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

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
override override_type_4_8: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_9: bool = (discard_mode == 1i);
override override_type_4_10: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
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
    var param: vec3<f32>;
    var param_1: vec3<f32>;

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
        param = _e113.xyz;
        let _e115 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = (*slot);
    if (lightmap_slot == (_e122 + 1i)) {
        let _e125 = c_1;
        param_1 = _e125.xyz;
        let _e127 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e127.x;
        c_1[1u] = _e127.y;
        c_1[2u] = _e127.z;
    }
    let _e134 = c_1;
    return _e134;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_4: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var sampledColor1_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var sampledColor2_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color2_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color2_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color2_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var color2_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_14: f32;

    let _e117 = frag_color0In_1;
    param_2 = _e117.xyz;
    let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e121 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e119.x, _e119.y, _e119.z, _e121);
    let _e126 = frag_color1In_1;
    param_3 = _e126.xyz;
    let _e128 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e130 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e128.x, _e128.y, _e128.z, _e130);
    let _e135 = frag_color2In_1;
    param_4 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e139 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    param_5 = 0u;
    let _e144 = frag_tex_coord0_1;
    param_6 = _e144;
    param_7 = 0i;
    let _e145 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor0_ = _e145;
    param_8 = 1u;
    let _e146 = frag_tex_coord1_1;
    param_9 = _e146;
    param_10 = 1i;
    let _e147 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor1_ = _e147;
    param_11 = 2u;
    let _e148 = frag_tex_coord2_1;
    param_12 = _e148;
    param_13 = 2i;
    let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor2_ = _e149;
    let _e150 = sampledColor0_;
    let _e151 = frag_color0_;
    color0_ = (_e150 * _e151);
    if override_type_4_2 {
        let _e153 = sampledColor1_;
        let _e154 = frag_color1_;
        color1_ = (_e153 * _e154);
        let _e156 = sampledColor2_;
        let _e157 = frag_color2_;
        color2_ = (_e156 * _e157);
        let _e159 = color0_;
        let _e161 = color1_;
        let _e164 = color2_;
        let _e166 = ((_e159.xyz + _e161.xyz) + _e164.xyz);
        let _e168 = color0_[3u];
        let _e170 = color1_[3u];
        let _e173 = color2_[3u];
        base = vec4<f32>(_e166.x, _e166.y, _e166.z, ((_e168 * _e170) * _e173));
    } else {
        if override_type_4_3 {
            let _e179 = sampledColor1_;
            let _e180 = frag_color1_;
            color1_1 = (_e179 * _e180);
            let _e182 = sampledColor2_;
            let _e183 = frag_color2_;
            color2_1 = (_e182 * _e183);
            let _e186 = color0_[3u];
            let _e187 = color0_;
            color0_ = (_e187 * _e186);
            let _e190 = color1_1[3u];
            let _e191 = color1_1;
            color1_1 = (_e191 * _e190);
            let _e194 = color2_1[3u];
            let _e195 = color2_1;
            color2_1 = (_e195 * _e194);
            let _e197 = color0_;
            let _e199 = color1_1;
            let _e202 = color2_1;
            let _e204 = ((_e197.xyz + _e199.xyz) + _e202.xyz);
            let _e206 = color0_[3u];
            let _e208 = color1_1[3u];
            let _e211 = color2_1[3u];
            base = vec4<f32>(_e204.x, _e204.y, _e204.z, ((_e206 * _e208) * _e211));
        } else {
            if override_type_4_4 {
                let _e217 = sampledColor1_;
                let _e218 = frag_color1_;
                color1_2 = (_e217 * _e218);
                let _e220 = sampledColor2_;
                let _e221 = frag_color2_;
                color2_2 = (_e220 * _e221);
                let _e224 = color0_[3u];
                let _e226 = color0_;
                color0_ = (_e226 * (1f - _e224));
                let _e229 = color1_2[3u];
                let _e231 = color1_2;
                color1_2 = (_e231 * (1f - _e229));
                let _e234 = color2_2[3u];
                let _e236 = color2_2;
                color2_2 = (_e236 * (1f - _e234));
                let _e238 = color0_;
                let _e240 = color1_2;
                let _e243 = color2_2;
                let _e245 = ((_e238.xyz + _e240.xyz) + _e243.xyz);
                let _e247 = color0_[3u];
                let _e249 = color1_2[3u];
                let _e252 = color2_2[3u];
                base = vec4<f32>(_e245.x, _e245.y, _e245.z, ((_e247 * _e249) * _e252));
            } else {
                if override_type_4_5 {
                    let _e258 = sampledColor1_;
                    let _e259 = frag_color1_;
                    color1_3 = (_e258 * _e259);
                    let _e261 = sampledColor2_;
                    let _e262 = frag_color2_;
                    color2_3 = (_e261 * _e262);
                    let _e264 = color0_;
                    let _e265 = color1_3;
                    let _e267 = color1_3[3u];
                    let _e270 = color2_3;
                    let _e272 = color2_3[3u];
                    base = mix(mix(_e264, _e265, vec4(_e267)), _e270, vec4(_e272));
                } else {
                    if override_type_4_6 {
                        let _e275 = sampledColor1_;
                        let _e276 = frag_color1_;
                        color1_4 = (_e275 * _e276);
                        let _e278 = sampledColor2_;
                        let _e279 = frag_color2_;
                        color2_4 = (_e278 * _e279);
                        let _e281 = color2_4;
                        let _e282 = color1_4;
                        let _e283 = color0_;
                        let _e285 = color1_4[3u];
                        let _e289 = color2_4[3u];
                        base = mix(_e281, mix(_e282, _e283, vec4(_e285)), vec4(_e289));
                    } else {
                        if override_type_4_7 {
                            let _e292 = sampledColor1_;
                            let _e293 = frag_color1_;
                            color1_5 = (_e292 * _e293);
                            let _e295 = sampledColor2_;
                            let _e296 = frag_color2_;
                            color2_5 = (_e295 * _e296);
                            let _e298 = color2_5;
                            let _e300 = color2_5[3u];
                            let _e303 = color1_5;
                            let _e305 = color1_5[3u];
                            let _e309 = color0_;
                            base = (((_e298 + vec4(_e300)) * (_e303 + vec4(_e305))) * _e309);
                        } else {
                            let _e311 = sampledColor1_;
                            let _e312 = frag_color1_;
                            color1_6 = (_e311 * _e312);
                            let _e314 = sampledColor2_;
                            let _e315 = frag_color2_;
                            color2_6 = (_e314 * _e315);
                            let _e317 = color0_;
                            let _e319 = color1_6;
                            let _e322 = color2_6;
                            let _e324 = ((_e317.xyz * _e319.xyz) * _e322.xyz);
                            base[0u] = _e324.x;
                            base[1u] = _e324.y;
                            base[2u] = _e324.z;
                            let _e332 = color0_[3u];
                            let _e334 = color1_6[3u];
                            let _e337 = color2_6[3u];
                            base[3u] = ((_e332 * _e334) * _e337);
                        }
                    }
                }
            }
        }
    }
    if override_type_4_8 {
        let _e342 = unnamed.worldLightParams[1u];
        wetness = clamp(_e342, 0f, 1f);
        let _e346 = unnamed.worldLightParams[2u];
        frost = clamp(_e346, 0f, 1f);
        let _e348 = base;
        luminance = dot(_e348.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e351 = wetness;
        let _e353 = base;
        let _e355 = (_e353.xyz * mix(1f, 0.82f, _e351));
        base[0u] = _e355.x;
        base[1u] = _e355.y;
        base[2u] = _e355.z;
        let _e362 = base;
        let _e364 = luminance;
        let _e366 = luminance;
        let _e368 = luminance;
        let _e370 = frost;
        let _e373 = mix(_e362.xyz, vec3<f32>((_e364 * 0.88f), (_e366 * 0.94f), _e368), vec3((_e370 * 0.55f)));
        base[0u] = _e373.x;
        base[1u] = _e373.y;
        base[2u] = _e373.z;
    }
    let _e382 = unnamed.materialFactors[0u];
    if (_e382 >= 0f) {
        let _e385 = unnamed.materialBaseColor;
        let _e386 = base;
        base = (_e386 * _e385);
    }
    let _e388 = color0_;
    let _e391 = unnamed.emissionRadiance;
    let _e394 = base;
    let _e396 = (_e394.xyz + (_e388.xyz * _e391.xyz));
    base[0u] = _e396.x;
    base[1u] = _e396.y;
    base[2u] = _e396.z;
    let _e403 = wired_advanced_fog_enabled_u0028_();
    if _e403 {
        let _e404 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e404;
        let _e405 = base;
        let _e408 = unnamed.advancedFogColorDensity;
        let _e410 = fogAmount;
        let _e412 = mix(_e405.xyz, _e408.xyz, vec3(_e410));
        base[0u] = _e412.x;
        base[1u] = _e412.y;
        base[2u] = _e412.z;
    }
    if override_type_4_9 {
        let _e420 = base[3u];
        if (_e420 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e422 = base;
            let _e424 = base;
            if (dot(_e422.xyz, _e424.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e428 = base;
    out_color = _e428;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
