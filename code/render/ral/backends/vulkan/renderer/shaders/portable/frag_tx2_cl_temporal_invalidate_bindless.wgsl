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
override override_type_6_: bool = (tex_mode == 1i);
override override_type_6_1: bool = (tex_mode == 2i);
override override_type_6_2: bool = (override_type_6_ || override_type_6_1);
override override_type_6_3: bool = (tex_mode == 3i);
override override_type_6_4: bool = (tex_mode == 4i);
override override_type_6_5: bool = (tex_mode == 5i);
override override_type_6_6: bool = (tex_mode == 6i);
override override_type_6_7: bool = (tex_mode == 7i);
override override_type_6_8: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_9: bool = (discard_mode == 1i);
override override_type_6_10: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
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

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e82 = (*sceneColor)[0u];
    let _e84 = (*sceneColor)[1u];
    let _e86 = (*sceneColor)[2u];
    let _e91 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e82, max(_e84, _e86)), 0f, 1f), 0f, 0f, clamp(_e91, 0f, 1f));
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
    var param: vec3<f32>;
    var param_1: vec3<f32>;

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
        param = _e114.xyz;
        let _e116 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = (*slot);
    if (lightmap_slot == (_e123 + 1i)) {
        let _e126 = c_1;
        param_1 = _e126.xyz;
        let _e128 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = c_1;
    return _e135;
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
    var param_15: vec4<f32>;

    let _e119 = frag_color0In_1;
    param_2 = _e119.xyz;
    let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e123 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e121.x, _e121.y, _e121.z, _e123);
    let _e128 = frag_color1In_1;
    param_3 = _e128.xyz;
    let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e132 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e130.x, _e130.y, _e130.z, _e132);
    let _e137 = frag_color2In_1;
    param_4 = _e137.xyz;
    let _e139 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e141 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e139.x, _e139.y, _e139.z, _e141);
    param_5 = 0u;
    let _e146 = frag_tex_coord0_1;
    param_6 = _e146;
    param_7 = 0i;
    let _e147 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor0_ = _e147;
    param_8 = 1u;
    let _e148 = frag_tex_coord1_1;
    param_9 = _e148;
    param_10 = 1i;
    let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor1_ = _e149;
    param_11 = 2u;
    let _e150 = frag_tex_coord2_1;
    param_12 = _e150;
    param_13 = 2i;
    let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor2_ = _e151;
    let _e152 = sampledColor0_;
    let _e153 = frag_color0_;
    color0_ = (_e152 * _e153);
    if override_type_6_2 {
        let _e155 = sampledColor1_;
        let _e156 = frag_color1_;
        color1_ = (_e155 * _e156);
        let _e158 = sampledColor2_;
        let _e159 = frag_color2_;
        color2_ = (_e158 * _e159);
        let _e161 = color0_;
        let _e163 = color1_;
        let _e166 = color2_;
        let _e168 = ((_e161.xyz + _e163.xyz) + _e166.xyz);
        let _e170 = color0_[3u];
        let _e172 = color1_[3u];
        let _e175 = color2_[3u];
        base = vec4<f32>(_e168.x, _e168.y, _e168.z, ((_e170 * _e172) * _e175));
    } else {
        if override_type_6_3 {
            let _e181 = sampledColor1_;
            let _e182 = frag_color1_;
            color1_1 = (_e181 * _e182);
            let _e184 = sampledColor2_;
            let _e185 = frag_color2_;
            color2_1 = (_e184 * _e185);
            let _e188 = color0_[3u];
            let _e189 = color0_;
            color0_ = (_e189 * _e188);
            let _e192 = color1_1[3u];
            let _e193 = color1_1;
            color1_1 = (_e193 * _e192);
            let _e196 = color2_1[3u];
            let _e197 = color2_1;
            color2_1 = (_e197 * _e196);
            let _e199 = color0_;
            let _e201 = color1_1;
            let _e204 = color2_1;
            let _e206 = ((_e199.xyz + _e201.xyz) + _e204.xyz);
            let _e208 = color0_[3u];
            let _e210 = color1_1[3u];
            let _e213 = color2_1[3u];
            base = vec4<f32>(_e206.x, _e206.y, _e206.z, ((_e208 * _e210) * _e213));
        } else {
            if override_type_6_4 {
                let _e219 = sampledColor1_;
                let _e220 = frag_color1_;
                color1_2 = (_e219 * _e220);
                let _e222 = sampledColor2_;
                let _e223 = frag_color2_;
                color2_2 = (_e222 * _e223);
                let _e226 = color0_[3u];
                let _e228 = color0_;
                color0_ = (_e228 * (1f - _e226));
                let _e231 = color1_2[3u];
                let _e233 = color1_2;
                color1_2 = (_e233 * (1f - _e231));
                let _e236 = color2_2[3u];
                let _e238 = color2_2;
                color2_2 = (_e238 * (1f - _e236));
                let _e240 = color0_;
                let _e242 = color1_2;
                let _e245 = color2_2;
                let _e247 = ((_e240.xyz + _e242.xyz) + _e245.xyz);
                let _e249 = color0_[3u];
                let _e251 = color1_2[3u];
                let _e254 = color2_2[3u];
                base = vec4<f32>(_e247.x, _e247.y, _e247.z, ((_e249 * _e251) * _e254));
            } else {
                if override_type_6_5 {
                    let _e260 = sampledColor1_;
                    let _e261 = frag_color1_;
                    color1_3 = (_e260 * _e261);
                    let _e263 = sampledColor2_;
                    let _e264 = frag_color2_;
                    color2_3 = (_e263 * _e264);
                    let _e266 = color0_;
                    let _e267 = color1_3;
                    let _e269 = color1_3[3u];
                    let _e272 = color2_3;
                    let _e274 = color2_3[3u];
                    base = mix(mix(_e266, _e267, vec4(_e269)), _e272, vec4(_e274));
                } else {
                    if override_type_6_6 {
                        let _e277 = sampledColor1_;
                        let _e278 = frag_color1_;
                        color1_4 = (_e277 * _e278);
                        let _e280 = sampledColor2_;
                        let _e281 = frag_color2_;
                        color2_4 = (_e280 * _e281);
                        let _e283 = color2_4;
                        let _e284 = color1_4;
                        let _e285 = color0_;
                        let _e287 = color1_4[3u];
                        let _e291 = color2_4[3u];
                        base = mix(_e283, mix(_e284, _e285, vec4(_e287)), vec4(_e291));
                    } else {
                        if override_type_6_7 {
                            let _e294 = sampledColor1_;
                            let _e295 = frag_color1_;
                            color1_5 = (_e294 * _e295);
                            let _e297 = sampledColor2_;
                            let _e298 = frag_color2_;
                            color2_5 = (_e297 * _e298);
                            let _e300 = color2_5;
                            let _e302 = color2_5[3u];
                            let _e305 = color1_5;
                            let _e307 = color1_5[3u];
                            let _e311 = color0_;
                            base = (((_e300 + vec4(_e302)) * (_e305 + vec4(_e307))) * _e311);
                        } else {
                            let _e313 = sampledColor1_;
                            let _e314 = frag_color1_;
                            color1_6 = (_e313 * _e314);
                            let _e316 = sampledColor2_;
                            let _e317 = frag_color2_;
                            color2_6 = (_e316 * _e317);
                            let _e319 = color0_;
                            let _e321 = color1_6;
                            let _e324 = color2_6;
                            let _e326 = ((_e319.xyz * _e321.xyz) * _e324.xyz);
                            base[0u] = _e326.x;
                            base[1u] = _e326.y;
                            base[2u] = _e326.z;
                            let _e334 = color0_[3u];
                            let _e336 = color1_6[3u];
                            let _e339 = color2_6[3u];
                            base[3u] = ((_e334 * _e336) * _e339);
                        }
                    }
                }
            }
        }
    }
    if override_type_6_8 {
        let _e344 = unnamed.worldLightParams[1u];
        wetness = clamp(_e344, 0f, 1f);
        let _e348 = unnamed.worldLightParams[2u];
        frost = clamp(_e348, 0f, 1f);
        let _e350 = base;
        luminance = dot(_e350.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e353 = wetness;
        let _e355 = base;
        let _e357 = (_e355.xyz * mix(1f, 0.82f, _e353));
        base[0u] = _e357.x;
        base[1u] = _e357.y;
        base[2u] = _e357.z;
        let _e364 = base;
        let _e366 = luminance;
        let _e368 = luminance;
        let _e370 = luminance;
        let _e372 = frost;
        let _e375 = mix(_e364.xyz, vec3<f32>((_e366 * 0.88f), (_e368 * 0.94f), _e370), vec3((_e372 * 0.55f)));
        base[0u] = _e375.x;
        base[1u] = _e375.y;
        base[2u] = _e375.z;
    }
    let _e384 = unnamed.materialFactors[0u];
    if (_e384 >= 0f) {
        let _e387 = unnamed.materialBaseColor;
        let _e388 = base;
        base = (_e388 * _e387);
    }
    let _e390 = color0_;
    let _e393 = unnamed.emissionRadiance;
    let _e396 = base;
    let _e398 = (_e396.xyz + (_e390.xyz * _e393.xyz));
    base[0u] = _e398.x;
    base[1u] = _e398.y;
    base[2u] = _e398.z;
    let _e405 = wired_advanced_fog_enabled_u0028_();
    if _e405 {
        let _e406 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e406;
        let _e407 = base;
        let _e410 = unnamed.advancedFogColorDensity;
        let _e412 = fogAmount;
        let _e414 = mix(_e407.xyz, _e410.xyz, vec3(_e412));
        base[0u] = _e414.x;
        base[1u] = _e414.y;
        base[2u] = _e414.z;
    }
    if override_type_6_9 {
        let _e422 = base[3u];
        if (_e422 == 0f) {
            discard;
        }
    } else {
        if override_type_6_10 {
            let _e424 = base;
            let _e426 = base;
            if (dot(_e424.xyz, _e426.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e430 = base;
    out_color = _e430;
    param_14 = 1f;
    let _e431 = base;
    param_15 = _e431;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_14), (&param_15));
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
