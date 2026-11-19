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

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e75 + 0.5f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e82 = fogType;
    let _e85 = fogType;
    return (((_e80 > 0.5f) && (_e82 >= 1i)) && (_e85 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e75 = wired_advanced_fog_enabled_u0028_();
    if !(_e75) {
        return 0f;
    }
    let _e78 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e78, 0.000001f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e83 + 0.5f));
    let _e86 = fogType_1;
    if (_e86 == 1i) {
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e90 <= 0f) {
            return 0f;
        }
        let _e92 = viewDepth;
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e92 / _e95), 0f, 1f);
    }
    let _e100 = unnamed.advancedFogColorDensity[3u];
    let _e102 = viewDepth;
    opticalDepth = (max(_e100, 0f) * _e102);
    let _e104 = fogType_1;
    if (_e104 == 2i) {
        let _e106 = opticalDepth;
        return clamp((1f - exp(-(_e106))), 0f, 1f);
    }
    let _e111 = opticalDepth;
    let _e112 = opticalDepth;
    return clamp((1f - exp(-((_e111 * _e112)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e75 = (*rgb);
    let _e78 = unnamed.worldLightParams[0u];
    boosted = (_e75 * _e78);
    let _e81 = boosted[0u];
    let _e83 = boosted[1u];
    let _e85 = boosted[2u];
    peak = max(_e81, max(_e83, _e85));
    let _e88 = peak;
    if (_e88 > 1f) {
        let _e90 = peak;
        let _e91 = boosted;
        boosted = (_e91 / vec3(_e90));
    }
    let _e94 = boosted;
    return _e94;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e76 = (*c);
    (*c) = max(_e76, vec3<f32>(0f, 0f, 0f));
    let _e78 = (*c);
    cutoff = (_e78 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e80 = (*c);
    lo = (_e80 / vec3(12.92f));
    let _e83 = (*c);
    hi = pow(((_e83 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e88 = hi;
    let _e89 = lo;
    let _e90 = cutoff;
    return mix(_e88, _e89, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e90));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e100 = (*uv);
    let _e101 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    c_1 = _e101;
    let _e102 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e102))) == 0i) {
        let _e107 = c_1;
        param = _e107.xyz;
        let _e109 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = (*slot);
    if (lightmap_slot == (_e116 + 1i)) {
        let _e119 = c_1;
        param_1 = _e119.xyz;
        let _e121 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = c_1;
    return _e128;
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

    let _e110 = frag_color0In_1;
    param_2 = _e110.xyz;
    let _e112 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e114 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e112.x, _e112.y, _e112.z, _e114);
    let _e119 = frag_color1In_1;
    param_3 = _e119.xyz;
    let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e123 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e121.x, _e121.y, _e121.z, _e123);
    let _e128 = frag_color2In_1;
    param_4 = _e128.xyz;
    let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e132 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e130.x, _e130.y, _e130.z, _e132);
    param_5 = 0u;
    let _e137 = frag_tex_coord0_1;
    param_6 = _e137;
    param_7 = 0i;
    let _e138 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor0_ = _e138;
    param_8 = 1u;
    let _e139 = frag_tex_coord1_1;
    param_9 = _e139;
    param_10 = 1i;
    let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor1_ = _e140;
    param_11 = 2u;
    let _e141 = frag_tex_coord2_1;
    param_12 = _e141;
    param_13 = 2i;
    let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor2_ = _e142;
    let _e143 = sampledColor0_;
    let _e144 = frag_color0_;
    color0_ = (_e143 * _e144);
    if override_type_3_2 {
        let _e146 = sampledColor1_;
        let _e147 = frag_color1_;
        color1_ = (_e146 * _e147);
        let _e149 = sampledColor2_;
        let _e150 = frag_color2_;
        color2_ = (_e149 * _e150);
        let _e152 = color0_;
        let _e154 = color1_;
        let _e157 = color2_;
        let _e159 = ((_e152.xyz + _e154.xyz) + _e157.xyz);
        let _e161 = color0_[3u];
        let _e163 = color1_[3u];
        let _e166 = color2_[3u];
        base = vec4<f32>(_e159.x, _e159.y, _e159.z, ((_e161 * _e163) * _e166));
    } else {
        if override_type_3_3 {
            let _e172 = sampledColor1_;
            let _e173 = frag_color1_;
            color1_1 = (_e172 * _e173);
            let _e175 = sampledColor2_;
            let _e176 = frag_color2_;
            color2_1 = (_e175 * _e176);
            let _e179 = color0_[3u];
            let _e180 = color0_;
            color0_ = (_e180 * _e179);
            let _e183 = color1_1[3u];
            let _e184 = color1_1;
            color1_1 = (_e184 * _e183);
            let _e187 = color2_1[3u];
            let _e188 = color2_1;
            color2_1 = (_e188 * _e187);
            let _e190 = color0_;
            let _e192 = color1_1;
            let _e195 = color2_1;
            let _e197 = ((_e190.xyz + _e192.xyz) + _e195.xyz);
            let _e199 = color0_[3u];
            let _e201 = color1_1[3u];
            let _e204 = color2_1[3u];
            base = vec4<f32>(_e197.x, _e197.y, _e197.z, ((_e199 * _e201) * _e204));
        } else {
            if override_type_3_4 {
                let _e210 = sampledColor1_;
                let _e211 = frag_color1_;
                color1_2 = (_e210 * _e211);
                let _e213 = sampledColor2_;
                let _e214 = frag_color2_;
                color2_2 = (_e213 * _e214);
                let _e217 = color0_[3u];
                let _e219 = color0_;
                color0_ = (_e219 * (1f - _e217));
                let _e222 = color1_2[3u];
                let _e224 = color1_2;
                color1_2 = (_e224 * (1f - _e222));
                let _e227 = color2_2[3u];
                let _e229 = color2_2;
                color2_2 = (_e229 * (1f - _e227));
                let _e231 = color0_;
                let _e233 = color1_2;
                let _e236 = color2_2;
                let _e238 = ((_e231.xyz + _e233.xyz) + _e236.xyz);
                let _e240 = color0_[3u];
                let _e242 = color1_2[3u];
                let _e245 = color2_2[3u];
                base = vec4<f32>(_e238.x, _e238.y, _e238.z, ((_e240 * _e242) * _e245));
            } else {
                if override_type_3_5 {
                    let _e251 = sampledColor1_;
                    let _e252 = frag_color1_;
                    color1_3 = (_e251 * _e252);
                    let _e254 = sampledColor2_;
                    let _e255 = frag_color2_;
                    color2_3 = (_e254 * _e255);
                    let _e257 = color0_;
                    let _e258 = color1_3;
                    let _e260 = color1_3[3u];
                    let _e263 = color2_3;
                    let _e265 = color2_3[3u];
                    base = mix(mix(_e257, _e258, vec4(_e260)), _e263, vec4(_e265));
                } else {
                    if override_type_3_6 {
                        let _e268 = sampledColor1_;
                        let _e269 = frag_color1_;
                        color1_4 = (_e268 * _e269);
                        let _e271 = sampledColor2_;
                        let _e272 = frag_color2_;
                        color2_4 = (_e271 * _e272);
                        let _e274 = color2_4;
                        let _e275 = color1_4;
                        let _e276 = color0_;
                        let _e278 = color1_4[3u];
                        let _e282 = color2_4[3u];
                        base = mix(_e274, mix(_e275, _e276, vec4(_e278)), vec4(_e282));
                    } else {
                        if override_type_3_7 {
                            let _e285 = sampledColor1_;
                            let _e286 = frag_color1_;
                            color1_5 = (_e285 * _e286);
                            let _e288 = sampledColor2_;
                            let _e289 = frag_color2_;
                            color2_5 = (_e288 * _e289);
                            let _e291 = color2_5;
                            let _e293 = color2_5[3u];
                            let _e296 = color1_5;
                            let _e298 = color1_5[3u];
                            let _e302 = color0_;
                            base = (((_e291 + vec4(_e293)) * (_e296 + vec4(_e298))) * _e302);
                        } else {
                            let _e304 = sampledColor1_;
                            let _e305 = frag_color1_;
                            color1_6 = (_e304 * _e305);
                            let _e307 = sampledColor2_;
                            let _e308 = frag_color2_;
                            color2_6 = (_e307 * _e308);
                            let _e310 = color0_;
                            let _e312 = color1_6;
                            let _e315 = color2_6;
                            let _e317 = ((_e310.xyz * _e312.xyz) * _e315.xyz);
                            base[0u] = _e317.x;
                            base[1u] = _e317.y;
                            base[2u] = _e317.z;
                            let _e325 = color0_[3u];
                            let _e327 = color1_6[3u];
                            let _e330 = color2_6[3u];
                            base[3u] = ((_e325 * _e327) * _e330);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e335 = unnamed.worldLightParams[1u];
        wetness = clamp(_e335, 0f, 1f);
        let _e339 = unnamed.worldLightParams[2u];
        frost = clamp(_e339, 0f, 1f);
        let _e341 = base;
        luminance = dot(_e341.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e344 = wetness;
        let _e346 = base;
        let _e348 = (_e346.xyz * mix(1f, 0.82f, _e344));
        base[0u] = _e348.x;
        base[1u] = _e348.y;
        base[2u] = _e348.z;
        let _e355 = base;
        let _e357 = luminance;
        let _e359 = luminance;
        let _e361 = luminance;
        let _e363 = frost;
        let _e366 = mix(_e355.xyz, vec3<f32>((_e357 * 0.88f), (_e359 * 0.94f), _e361), vec3((_e363 * 0.55f)));
        base[0u] = _e366.x;
        base[1u] = _e366.y;
        base[2u] = _e366.z;
    }
    let _e375 = unnamed.materialFactors[0u];
    if (_e375 >= 0f) {
        let _e378 = unnamed.materialBaseColor;
        let _e379 = base;
        base = (_e379 * _e378);
    }
    let _e381 = color0_;
    let _e384 = unnamed.emissionRadiance;
    let _e387 = base;
    let _e389 = (_e387.xyz + (_e381.xyz * _e384.xyz));
    base[0u] = _e389.x;
    base[1u] = _e389.y;
    base[2u] = _e389.z;
    let _e396 = wired_advanced_fog_enabled_u0028_();
    if _e396 {
        let _e397 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e397;
        let _e398 = base;
        let _e401 = unnamed.advancedFogColorDensity;
        let _e403 = fogAmount;
        let _e405 = mix(_e398.xyz, _e401.xyz, vec3(_e403));
        base[0u] = _e405.x;
        base[1u] = _e405.y;
        base[2u] = _e405.z;
    }
    if override_type_3_9 {
        let _e413 = base[3u];
        if (_e413 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e415 = base;
            let _e417 = base;
            if (dot(_e415.xyz, _e417.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e421 = base;
    out_color = _e421;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e15 = out_color;
    return _e15;
}
