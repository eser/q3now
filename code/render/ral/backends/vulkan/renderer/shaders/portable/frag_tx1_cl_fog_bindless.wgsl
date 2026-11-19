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
@id(10) override acff: i32 = 0i;
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_15: bool = (discard_mode == 1i);
override override_type_3_16: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

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
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var sampledColor1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
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

    let _e107 = unnamed.packed_indices[0i][3u];
    let _e113 = unnamed.packed_indices[0i][3u];
    let _e118 = fog_tex_coord_1;
    let _e119 = textureSample(wired_bindless_images[(_e107 & 4095u)], wired_bindless_samplers[((_e113 >> bitcast<u32>(12i)) & 255u)], _e118);
    fog = _e119;
    let _e120 = frag_color0In_1;
    param_2 = _e120.xyz;
    let _e122 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e124 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e122.x, _e122.y, _e122.z, _e124);
    let _e129 = frag_color1In_1;
    param_3 = _e129.xyz;
    let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e133 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e131.x, _e131.y, _e131.z, _e133);
    param_4 = 0u;
    let _e138 = frag_tex_coord0_1;
    param_5 = _e138;
    param_6 = 0i;
    let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    sampledColor0_ = _e139;
    param_7 = 1u;
    let _e140 = frag_tex_coord1_1;
    param_8 = _e140;
    param_9 = 1i;
    let _e141 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor1_ = _e141;
    let _e142 = sampledColor0_;
    let _e143 = frag_color0_;
    color0_ = (_e142 * _e143);
    if override_type_3_2 {
        let _e145 = sampledColor1_;
        let _e146 = frag_color1_;
        color1_ = (_e145 * _e146);
        let _e148 = color0_;
        let _e150 = color1_;
        let _e152 = (_e148.xyz + _e150.xyz);
        let _e154 = color0_[3u];
        let _e156 = color1_[3u];
        base = vec4<f32>(_e152.x, _e152.y, _e152.z, (_e154 * _e156));
    } else {
        if override_type_3_3 {
            let _e162 = sampledColor1_;
            let _e163 = frag_color1_;
            color1_1 = (_e162 * _e163);
            let _e166 = color0_[3u];
            let _e167 = color0_;
            color0_ = (_e167 * _e166);
            let _e170 = color1_1[3u];
            let _e171 = color1_1;
            color1_1 = (_e171 * _e170);
            let _e173 = color0_;
            let _e175 = color1_1;
            let _e177 = (_e173.xyz + _e175.xyz);
            let _e179 = color0_[3u];
            let _e181 = color1_1[3u];
            base = vec4<f32>(_e177.x, _e177.y, _e177.z, (_e179 * _e181));
        } else {
            if override_type_3_4 {
                let _e187 = sampledColor1_;
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
                    let _e214 = sampledColor1_;
                    let _e215 = frag_color1_;
                    color1_3 = (_e214 * _e215);
                    let _e217 = color0_;
                    let _e218 = color1_3;
                    let _e220 = color1_3[3u];
                    base = mix(_e217, _e218, vec4(_e220));
                } else {
                    if override_type_3_6 {
                        let _e223 = sampledColor1_;
                        let _e224 = frag_color1_;
                        color1_4 = (_e223 * _e224);
                        let _e226 = color1_4;
                        let _e227 = color0_;
                        let _e229 = color1_4[3u];
                        base = mix(_e226, _e227, vec4(_e229));
                    } else {
                        if override_type_3_7 {
                            let _e232 = sampledColor1_;
                            let _e233 = frag_color1_;
                            color1_5 = (_e232 * _e233);
                            let _e235 = color1_5;
                            let _e237 = color1_5[3u];
                            let _e240 = color0_;
                            base = ((_e235 + vec4(_e237)) * _e240);
                        } else {
                            let _e242 = sampledColor1_;
                            let _e243 = frag_color1_;
                            color1_6 = (_e242 * _e243);
                            let _e245 = color0_;
                            let _e247 = color1_6;
                            let _e249 = (_e245.xyz * _e247.xyz);
                            base[0u] = _e249.x;
                            base[1u] = _e249.y;
                            base[2u] = _e249.z;
                            let _e257 = color0_[3u];
                            let _e259 = color1_6[3u];
                            base[3u] = (_e257 * _e259);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e264 = unnamed.worldLightParams[1u];
        wetness = clamp(_e264, 0f, 1f);
        let _e268 = unnamed.worldLightParams[2u];
        frost = clamp(_e268, 0f, 1f);
        let _e270 = base;
        luminance = dot(_e270.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e273 = wetness;
        let _e275 = base;
        let _e277 = (_e275.xyz * mix(1f, 0.82f, _e273));
        base[0u] = _e277.x;
        base[1u] = _e277.y;
        base[2u] = _e277.z;
        let _e284 = base;
        let _e286 = luminance;
        let _e288 = luminance;
        let _e290 = luminance;
        let _e292 = frost;
        let _e295 = mix(_e284.xyz, vec3<f32>((_e286 * 0.88f), (_e288 * 0.94f), _e290), vec3((_e292 * 0.55f)));
        base[0u] = _e295.x;
        base[1u] = _e295.y;
        base[2u] = _e295.z;
    }
    let _e304 = unnamed.materialFactors[0u];
    if (_e304 >= 0f) {
        let _e307 = unnamed.materialBaseColor;
        let _e308 = base;
        base = (_e308 * _e307);
    }
    let _e310 = color0_;
    let _e313 = unnamed.emissionRadiance;
    let _e316 = base;
    let _e318 = (_e316.xyz + (_e310.xyz * _e313.xyz));
    base[0u] = _e318.x;
    base[1u] = _e318.y;
    base[2u] = _e318.z;
    let _e325 = wired_advanced_fog_enabled_u0028_();
    if _e325 {
        let _e326 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e326;
        if override_type_3_9 {
            let _e327 = fogAmount;
            let _e329 = base;
            let _e331 = (_e329.xyz * (1f - _e327));
            base[0u] = _e331.x;
            base[1u] = _e331.y;
            base[2u] = _e331.z;
        } else {
            if override_type_3_10 {
                let _e338 = fogAmount;
                let _e340 = base;
                base = (_e340 * (1f - _e338));
            } else {
                if override_type_3_11 {
                    let _e342 = fogAmount;
                    let _e345 = base[3u];
                    base[3u] = (_e345 * (1f - _e342));
                } else {
                    let _e348 = base;
                    let _e351 = unnamed.advancedFogColorDensity;
                    let _e353 = fogAmount;
                    let _e355 = mix(_e348.xyz, _e351.xyz, vec3(_e353));
                    base[0u] = _e355.x;
                    base[1u] = _e355.y;
                    base[2u] = _e355.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e362 = base;
            let _e365 = fog[3u];
            let _e367 = (_e362.xyz * (1f - _e365));
            base[0u] = _e367.x;
            base[1u] = _e367.y;
            base[2u] = _e367.z;
        } else {
            if override_type_3_13 {
                let _e374 = base;
                let _e376 = fog[3u];
                base = (_e374 * (1f - _e376));
            } else {
                if override_type_3_14 {
                    let _e380 = base[3u];
                    let _e382 = fog[3u];
                    base[3u] = (_e380 * (1f - _e382));
                } else {
                    let _e386 = base;
                    let _e387 = fog;
                    let _e389 = unnamed.fogColor;
                    let _e392 = fog[3u];
                    base = mix(_e386, (_e387 * _e389), vec4(_e392));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e396 = base[3u];
        if (_e396 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e398 = base;
            let _e400 = base;
            if (dot(_e398.xyz, _e400.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e404 = base;
    out_color = _e404;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e13 = out_color;
    return _e13;
}
