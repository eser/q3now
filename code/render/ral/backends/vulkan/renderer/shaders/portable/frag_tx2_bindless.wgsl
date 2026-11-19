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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e63 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e63 + 0.5f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e70 = fogType;
    let _e73 = fogType;
    return (((_e68 > 0.5f) && (_e70 >= 1i)) && (_e73 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e63 = wired_advanced_fog_enabled_u0028_();
    if !(_e63) {
        return 0f;
    }
    let _e66 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e66, 0.000001f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e71 + 0.5f));
    let _e74 = fogType_1;
    if (_e74 == 1i) {
        let _e78 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e78 <= 0f) {
            return 0f;
        }
        let _e80 = viewDepth;
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e80 / _e83), 0f, 1f);
    }
    let _e88 = unnamed.advancedFogColorDensity[3u];
    let _e90 = viewDepth;
    opticalDepth = (max(_e88, 0f) * _e90);
    let _e92 = fogType_1;
    if (_e92 == 2i) {
        let _e94 = opticalDepth;
        return clamp((1f - exp(-(_e94))), 0f, 1f);
    }
    let _e99 = opticalDepth;
    let _e100 = opticalDepth;
    return clamp((1f - exp(-((_e99 * _e100)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e63 = (*rgb);
    let _e66 = unnamed.worldLightParams[0u];
    boosted = (_e63 * _e66);
    let _e69 = boosted[0u];
    let _e71 = boosted[1u];
    let _e73 = boosted[2u];
    peak = max(_e69, max(_e71, _e73));
    let _e76 = peak;
    if (_e76 > 1f) {
        let _e78 = peak;
        let _e79 = boosted;
        boosted = (_e79 / vec3(_e78));
    }
    let _e82 = boosted;
    return _e82;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c);
    (*c) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e88 = (*uv);
    let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    c_1 = _e89;
    let _e90 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e90))) == 0i) {
        let _e95 = c_1;
        param = _e95.xyz;
        let _e97 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e97.x;
        c_1[1u] = _e97.y;
        c_1[2u] = _e97.z;
    }
    let _e104 = (*slot);
    if (lightmap_slot == (_e104 + 1i)) {
        let _e107 = c_1;
        param_1 = _e107.xyz;
        let _e109 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
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

    let _e86 = frag_color0In_1;
    param_2 = _e86.xyz;
    let _e88 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e90 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e88.x, _e88.y, _e88.z, _e90);
    param_3 = 0u;
    let _e95 = frag_tex_coord0_1;
    param_4 = _e95;
    param_5 = 0i;
    let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e96;
    param_6 = 1u;
    let _e97 = frag_tex_coord1_1;
    param_7 = _e97;
    param_8 = 1i;
    let _e98 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e98;
    param_9 = 2u;
    let _e99 = frag_tex_coord2_1;
    param_10 = _e99;
    param_11 = 2i;
    let _e100 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor2_ = _e100;
    let _e101 = sampledColor0_;
    let _e102 = frag_color0_;
    color0_ = (_e101 * _e102);
    if override_type_3_ {
        let _e104 = sampledColor1_;
        color1_ = _e104;
        let _e105 = sampledColor2_;
        color2_ = _e105;
        let _e106 = color0_;
        let _e108 = color1_;
        let _e111 = color2_;
        let _e113 = ((_e106.xyz + _e108.xyz) + _e111.xyz);
        let _e115 = color0_[3u];
        let _e117 = color1_[3u];
        let _e120 = color2_[3u];
        base = vec4<f32>(_e113.x, _e113.y, _e113.z, ((_e115 * _e117) * _e120));
    } else {
        if override_type_3_1 {
            let _e126 = sampledColor1_;
            let _e127 = frag_color0_;
            color1_1 = (_e126 * _e127);
            let _e129 = sampledColor2_;
            let _e130 = frag_color0_;
            color2_1 = (_e129 * _e130);
            let _e132 = color0_;
            let _e134 = color1_1;
            let _e137 = color2_1;
            let _e139 = ((_e132.xyz + _e134.xyz) + _e137.xyz);
            let _e141 = color0_[3u];
            let _e143 = color1_1[3u];
            let _e146 = color2_1[3u];
            base = vec4<f32>(_e139.x, _e139.y, _e139.z, ((_e141 * _e143) * _e146));
        } else {
            let _e152 = sampledColor1_;
            color1_2 = _e152;
            let _e153 = sampledColor2_;
            color2_2 = _e153;
            let _e154 = color0_;
            let _e156 = color1_2;
            let _e159 = color2_2;
            let _e161 = ((_e154.xyz * _e156.xyz) * _e159.xyz);
            base[0u] = _e161.x;
            base[1u] = _e161.y;
            base[2u] = _e161.z;
            let _e169 = color0_[3u];
            let _e171 = color1_2[3u];
            let _e174 = color2_2[3u];
            base[3u] = ((_e169 * _e171) * _e174);
        }
    }
    if override_type_3_2 {
        let _e179 = unnamed.worldLightParams[1u];
        wetness = clamp(_e179, 0f, 1f);
        let _e183 = unnamed.worldLightParams[2u];
        frost = clamp(_e183, 0f, 1f);
        let _e185 = base;
        luminance = dot(_e185.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e188 = wetness;
        let _e190 = base;
        let _e192 = (_e190.xyz * mix(1f, 0.82f, _e188));
        base[0u] = _e192.x;
        base[1u] = _e192.y;
        base[2u] = _e192.z;
        let _e199 = base;
        let _e201 = luminance;
        let _e203 = luminance;
        let _e205 = luminance;
        let _e207 = frost;
        let _e210 = mix(_e199.xyz, vec3<f32>((_e201 * 0.88f), (_e203 * 0.94f), _e205), vec3((_e207 * 0.55f)));
        base[0u] = _e210.x;
        base[1u] = _e210.y;
        base[2u] = _e210.z;
    }
    let _e219 = unnamed.materialFactors[0u];
    if (_e219 >= 0f) {
        let _e222 = unnamed.materialBaseColor;
        let _e223 = base;
        base = (_e223 * _e222);
    }
    let _e225 = color0_;
    let _e228 = unnamed.emissionRadiance;
    let _e231 = base;
    let _e233 = (_e231.xyz + (_e225.xyz * _e228.xyz));
    base[0u] = _e233.x;
    base[1u] = _e233.y;
    base[2u] = _e233.z;
    let _e240 = wired_advanced_fog_enabled_u0028_();
    if _e240 {
        let _e241 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e241;
        let _e242 = base;
        let _e245 = unnamed.advancedFogColorDensity;
        let _e247 = fogAmount;
        let _e249 = mix(_e242.xyz, _e245.xyz, vec3(_e247));
        base[0u] = _e249.x;
        base[1u] = _e249.y;
        base[2u] = _e249.z;
    }
    if override_type_3_3 {
        let _e257 = base[3u];
        if (_e257 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e259 = base;
            let _e261 = base;
            if (dot(_e259.xyz, _e261.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e265 = base;
    out_color = _e265;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
