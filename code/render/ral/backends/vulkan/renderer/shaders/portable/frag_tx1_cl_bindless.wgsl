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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e73 = (*rgb);
    let _e76 = unnamed.worldLightParams[0u];
    boosted = (_e73 * _e76);
    let _e79 = boosted[0u];
    let _e81 = boosted[1u];
    let _e83 = boosted[2u];
    peak = max(_e79, max(_e81, _e83));
    let _e86 = peak;
    if (_e86 > 1f) {
        let _e88 = peak;
        let _e89 = boosted;
        boosted = (_e89 / vec3(_e88));
    }
    let _e92 = boosted;
    return _e92;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_1 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_1;
        param = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e117 = c_1;
        param_1 = _e117.xyz;
        let _e119 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
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

    let _e95 = frag_color0In_1;
    param_2 = _e95.xyz;
    let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e99 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e97.x, _e97.y, _e97.z, _e99);
    let _e104 = frag_color1In_1;
    param_3 = _e104.xyz;
    let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e108 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e106.x, _e106.y, _e106.z, _e108);
    param_4 = 0u;
    let _e113 = frag_tex_coord0_1;
    param_5 = _e113;
    param_6 = 0i;
    let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    sampledColor0_ = _e114;
    param_7 = 1u;
    let _e115 = frag_tex_coord1_1;
    param_8 = _e115;
    param_9 = 1i;
    let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor1_ = _e116;
    let _e117 = sampledColor0_;
    let _e118 = frag_color0_;
    color0_ = (_e117 * _e118);
    if override_type_3_2 {
        let _e120 = sampledColor1_;
        let _e121 = frag_color1_;
        color1_ = (_e120 * _e121);
        let _e123 = color0_;
        let _e125 = color1_;
        let _e127 = (_e123.xyz + _e125.xyz);
        let _e129 = color0_[3u];
        let _e131 = color1_[3u];
        base = vec4<f32>(_e127.x, _e127.y, _e127.z, (_e129 * _e131));
    } else {
        if override_type_3_3 {
            let _e137 = sampledColor1_;
            let _e138 = frag_color1_;
            color1_1 = (_e137 * _e138);
            let _e141 = color0_[3u];
            let _e142 = color0_;
            color0_ = (_e142 * _e141);
            let _e145 = color1_1[3u];
            let _e146 = color1_1;
            color1_1 = (_e146 * _e145);
            let _e148 = color0_;
            let _e150 = color1_1;
            let _e152 = (_e148.xyz + _e150.xyz);
            let _e154 = color0_[3u];
            let _e156 = color1_1[3u];
            base = vec4<f32>(_e152.x, _e152.y, _e152.z, (_e154 * _e156));
        } else {
            if override_type_3_4 {
                let _e162 = sampledColor1_;
                let _e163 = frag_color1_;
                color1_2 = (_e162 * _e163);
                let _e166 = color0_[3u];
                let _e168 = color0_;
                color0_ = (_e168 * (1f - _e166));
                let _e171 = color1_2[3u];
                let _e173 = color1_2;
                color1_2 = (_e173 * (1f - _e171));
                let _e175 = color0_;
                let _e177 = color1_2;
                let _e179 = (_e175.xyz + _e177.xyz);
                let _e181 = color0_[3u];
                let _e183 = color1_2[3u];
                base = vec4<f32>(_e179.x, _e179.y, _e179.z, (_e181 * _e183));
            } else {
                if override_type_3_5 {
                    let _e189 = sampledColor1_;
                    let _e190 = frag_color1_;
                    color1_3 = (_e189 * _e190);
                    let _e192 = color0_;
                    let _e193 = color1_3;
                    let _e195 = color1_3[3u];
                    base = mix(_e192, _e193, vec4(_e195));
                } else {
                    if override_type_3_6 {
                        let _e198 = sampledColor1_;
                        let _e199 = frag_color1_;
                        color1_4 = (_e198 * _e199);
                        let _e201 = color1_4;
                        let _e202 = color0_;
                        let _e204 = color1_4[3u];
                        base = mix(_e201, _e202, vec4(_e204));
                    } else {
                        if override_type_3_7 {
                            let _e207 = sampledColor1_;
                            let _e208 = frag_color1_;
                            color1_5 = (_e207 * _e208);
                            let _e210 = color1_5;
                            let _e212 = color1_5[3u];
                            let _e215 = color0_;
                            base = ((_e210 + vec4(_e212)) * _e215);
                        } else {
                            let _e217 = sampledColor1_;
                            let _e218 = frag_color1_;
                            color1_6 = (_e217 * _e218);
                            let _e220 = color0_;
                            let _e222 = color1_6;
                            let _e224 = (_e220.xyz * _e222.xyz);
                            base[0u] = _e224.x;
                            base[1u] = _e224.y;
                            base[2u] = _e224.z;
                            let _e232 = color0_[3u];
                            let _e234 = color1_6[3u];
                            base[3u] = (_e232 * _e234);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e239 = unnamed.worldLightParams[1u];
        wetness = clamp(_e239, 0f, 1f);
        let _e243 = unnamed.worldLightParams[2u];
        frost = clamp(_e243, 0f, 1f);
        let _e245 = base;
        luminance = dot(_e245.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e248 = wetness;
        let _e250 = base;
        let _e252 = (_e250.xyz * mix(1f, 0.82f, _e248));
        base[0u] = _e252.x;
        base[1u] = _e252.y;
        base[2u] = _e252.z;
        let _e259 = base;
        let _e261 = luminance;
        let _e263 = luminance;
        let _e265 = luminance;
        let _e267 = frost;
        let _e270 = mix(_e259.xyz, vec3<f32>((_e261 * 0.88f), (_e263 * 0.94f), _e265), vec3((_e267 * 0.55f)));
        base[0u] = _e270.x;
        base[1u] = _e270.y;
        base[2u] = _e270.z;
    }
    let _e279 = unnamed.materialFactors[0u];
    if (_e279 >= 0f) {
        let _e282 = unnamed.materialBaseColor;
        let _e283 = base;
        base = (_e283 * _e282);
    }
    let _e285 = color0_;
    let _e288 = unnamed.emissionRadiance;
    let _e291 = base;
    let _e293 = (_e291.xyz + (_e285.xyz * _e288.xyz));
    base[0u] = _e293.x;
    base[1u] = _e293.y;
    base[2u] = _e293.z;
    let _e300 = wired_advanced_fog_enabled_u0028_();
    if _e300 {
        let _e301 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e301;
        let _e302 = base;
        let _e305 = unnamed.advancedFogColorDensity;
        let _e307 = fogAmount;
        let _e309 = mix(_e302.xyz, _e305.xyz, vec3(_e307));
        base[0u] = _e309.x;
        base[1u] = _e309.y;
        base[2u] = _e309.z;
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
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
