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

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e72 + 0.5f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e79 = fogType;
    let _e82 = fogType;
    return (((_e77 > 0.5f) && (_e79 >= 1i)) && (_e82 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e72 = wired_advanced_fog_enabled_u0028_();
    if !(_e72) {
        return 0f;
    }
    let _e75 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e75, 0.000001f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e80 + 0.5f));
    let _e83 = fogType_1;
    if (_e83 == 1i) {
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e87 <= 0f) {
            return 0f;
        }
        let _e89 = viewDepth;
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e89 / _e92), 0f, 1f);
    }
    let _e97 = unnamed.advancedFogColorDensity[3u];
    let _e99 = viewDepth;
    opticalDepth = (max(_e97, 0f) * _e99);
    let _e101 = fogType_1;
    if (_e101 == 2i) {
        let _e103 = opticalDepth;
        return clamp((1f - exp(-(_e103))), 0f, 1f);
    }
    let _e108 = opticalDepth;
    let _e109 = opticalDepth;
    return clamp((1f - exp(-((_e108 * _e109)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e72 = (*rgb);
    let _e75 = unnamed.worldLightParams[0u];
    boosted = (_e72 * _e75);
    let _e78 = boosted[0u];
    let _e80 = boosted[1u];
    let _e82 = boosted[2u];
    peak = max(_e78, max(_e80, _e82));
    let _e85 = peak;
    if (_e85 > 1f) {
        let _e87 = peak;
        let _e88 = boosted;
        boosted = (_e88 / vec3(_e87));
    }
    let _e91 = boosted;
    return _e91;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e73 = (*c);
    (*c) = max(_e73, vec3<f32>(0f, 0f, 0f));
    let _e75 = (*c);
    cutoff = (_e75 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e77 = (*c);
    lo = (_e77 / vec3(12.92f));
    let _e80 = (*c);
    hi = pow(((_e80 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e85 = hi;
    let _e86 = lo;
    let _e87 = cutoff;
    return mix(_e85, _e86, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e87));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e116 = c_1;
        param_1 = _e116.xyz;
        let _e118 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = c_1;
    return _e125;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var sampledColor1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var sampledColor2_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
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

    let _e99 = unnamed.packed_indices[0i][3u];
    let _e105 = unnamed.packed_indices[0i][3u];
    let _e110 = fog_tex_coord_1;
    let _e111 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    fog = _e111;
    let _e112 = frag_color0In_1;
    param_2 = _e112.xyz;
    let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e116 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e114.x, _e114.y, _e114.z, _e116);
    param_3 = 0u;
    let _e121 = frag_tex_coord0_1;
    param_4 = _e121;
    param_5 = 0i;
    let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e122;
    param_6 = 1u;
    let _e123 = frag_tex_coord1_1;
    param_7 = _e123;
    param_8 = 1i;
    let _e124 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e124;
    param_9 = 2u;
    let _e125 = frag_tex_coord2_1;
    param_10 = _e125;
    param_11 = 2i;
    let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor2_ = _e126;
    let _e127 = sampledColor0_;
    let _e128 = frag_color0_;
    color0_ = (_e127 * _e128);
    if override_type_3_ {
        let _e130 = sampledColor1_;
        color1_ = _e130;
        let _e131 = sampledColor2_;
        color2_ = _e131;
        let _e132 = color0_;
        let _e134 = color1_;
        let _e137 = color2_;
        let _e139 = ((_e132.xyz + _e134.xyz) + _e137.xyz);
        let _e141 = color0_[3u];
        let _e143 = color1_[3u];
        let _e146 = color2_[3u];
        base = vec4<f32>(_e139.x, _e139.y, _e139.z, ((_e141 * _e143) * _e146));
    } else {
        if override_type_3_1 {
            let _e152 = sampledColor1_;
            let _e153 = frag_color0_;
            color1_1 = (_e152 * _e153);
            let _e155 = sampledColor2_;
            let _e156 = frag_color0_;
            color2_1 = (_e155 * _e156);
            let _e158 = color0_;
            let _e160 = color1_1;
            let _e163 = color2_1;
            let _e165 = ((_e158.xyz + _e160.xyz) + _e163.xyz);
            let _e167 = color0_[3u];
            let _e169 = color1_1[3u];
            let _e172 = color2_1[3u];
            base = vec4<f32>(_e165.x, _e165.y, _e165.z, ((_e167 * _e169) * _e172));
        } else {
            let _e178 = sampledColor1_;
            color1_2 = _e178;
            let _e179 = sampledColor2_;
            color2_2 = _e179;
            let _e180 = color0_;
            let _e182 = color1_2;
            let _e185 = color2_2;
            let _e187 = ((_e180.xyz * _e182.xyz) * _e185.xyz);
            base[0u] = _e187.x;
            base[1u] = _e187.y;
            base[2u] = _e187.z;
            let _e195 = color0_[3u];
            let _e197 = color1_2[3u];
            let _e200 = color2_2[3u];
            base[3u] = ((_e195 * _e197) * _e200);
        }
    }
    if override_type_3_2 {
        let _e205 = unnamed.worldLightParams[1u];
        wetness = clamp(_e205, 0f, 1f);
        let _e209 = unnamed.worldLightParams[2u];
        frost = clamp(_e209, 0f, 1f);
        let _e211 = base;
        luminance = dot(_e211.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e214 = wetness;
        let _e216 = base;
        let _e218 = (_e216.xyz * mix(1f, 0.82f, _e214));
        base[0u] = _e218.x;
        base[1u] = _e218.y;
        base[2u] = _e218.z;
        let _e225 = base;
        let _e227 = luminance;
        let _e229 = luminance;
        let _e231 = luminance;
        let _e233 = frost;
        let _e236 = mix(_e225.xyz, vec3<f32>((_e227 * 0.88f), (_e229 * 0.94f), _e231), vec3((_e233 * 0.55f)));
        base[0u] = _e236.x;
        base[1u] = _e236.y;
        base[2u] = _e236.z;
    }
    let _e245 = unnamed.materialFactors[0u];
    if (_e245 >= 0f) {
        let _e248 = unnamed.materialBaseColor;
        let _e249 = base;
        base = (_e249 * _e248);
    }
    let _e251 = color0_;
    let _e254 = unnamed.emissionRadiance;
    let _e257 = base;
    let _e259 = (_e257.xyz + (_e251.xyz * _e254.xyz));
    base[0u] = _e259.x;
    base[1u] = _e259.y;
    base[2u] = _e259.z;
    let _e266 = wired_advanced_fog_enabled_u0028_();
    if _e266 {
        let _e267 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e267;
        if override_type_3_3 {
            let _e268 = fogAmount;
            let _e270 = base;
            let _e272 = (_e270.xyz * (1f - _e268));
            base[0u] = _e272.x;
            base[1u] = _e272.y;
            base[2u] = _e272.z;
        } else {
            if override_type_3_4 {
                let _e279 = fogAmount;
                let _e281 = base;
                base = (_e281 * (1f - _e279));
            } else {
                if override_type_3_5 {
                    let _e283 = fogAmount;
                    let _e286 = base[3u];
                    base[3u] = (_e286 * (1f - _e283));
                } else {
                    let _e289 = base;
                    let _e292 = unnamed.advancedFogColorDensity;
                    let _e294 = fogAmount;
                    let _e296 = mix(_e289.xyz, _e292.xyz, vec3(_e294));
                    base[0u] = _e296.x;
                    base[1u] = _e296.y;
                    base[2u] = _e296.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e303 = base;
            let _e306 = fog[3u];
            let _e308 = (_e303.xyz * (1f - _e306));
            base[0u] = _e308.x;
            base[1u] = _e308.y;
            base[2u] = _e308.z;
        } else {
            if override_type_3_7 {
                let _e315 = base;
                let _e317 = fog[3u];
                base = (_e315 * (1f - _e317));
            } else {
                if override_type_3_8 {
                    let _e321 = base[3u];
                    let _e323 = fog[3u];
                    base[3u] = (_e321 * (1f - _e323));
                } else {
                    let _e327 = base;
                    let _e328 = fog;
                    let _e330 = unnamed.fogColor;
                    let _e333 = fog[3u];
                    base = mix(_e327, (_e328 * _e330), vec4(_e333));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e337 = base[3u];
        if (_e337 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e339 = base;
            let _e341 = base;
            if (dot(_e339.xyz, _e341.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e345 = base;
    out_color = _e345;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e13 = out_color;
    return _e13;
}
