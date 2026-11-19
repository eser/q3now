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
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e62 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e62 + 0.5f));
    let _e67 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e69 = fogType;
    let _e72 = fogType;
    return (((_e67 > 0.5f) && (_e69 >= 1i)) && (_e72 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e62 = wired_advanced_fog_enabled_u0028_();
    if !(_e62) {
        return 0f;
    }
    let _e65 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e65, 0.000001f));
    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e70 + 0.5f));
    let _e73 = fogType_1;
    if (_e73 == 1i) {
        let _e77 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e77 <= 0f) {
            return 0f;
        }
        let _e79 = viewDepth;
        let _e82 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e79 / _e82), 0f, 1f);
    }
    let _e87 = unnamed.advancedFogColorDensity[3u];
    let _e89 = viewDepth;
    opticalDepth = (max(_e87, 0f) * _e89);
    let _e91 = fogType_1;
    if (_e91 == 2i) {
        let _e93 = opticalDepth;
        return clamp((1f - exp(-(_e93))), 0f, 1f);
    }
    let _e98 = opticalDepth;
    let _e99 = opticalDepth;
    return clamp((1f - exp(-((_e98 * _e99)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e62 = (*rgb);
    let _e65 = unnamed.worldLightParams[0u];
    boosted = (_e62 * _e65);
    let _e68 = boosted[0u];
    let _e70 = boosted[1u];
    let _e72 = boosted[2u];
    peak = max(_e68, max(_e70, _e72));
    let _e75 = peak;
    if (_e75 > 1f) {
        let _e77 = peak;
        let _e78 = boosted;
        boosted = (_e78 / vec3(_e77));
    }
    let _e81 = boosted;
    return _e81;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e63 = (*c);
    (*c) = max(_e63, vec3<f32>(0f, 0f, 0f));
    let _e65 = (*c);
    cutoff = (_e65 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e67 = (*c);
    lo = (_e67 / vec3(12.92f));
    let _e70 = (*c);
    hi = pow(((_e70 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e75 = hi;
    let _e76 = lo;
    let _e77 = cutoff;
    return mix(_e75, _e76, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e77));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e87 = (*uv);
    let _e88 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    c_1 = _e88;
    let _e89 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e89))) == 0i) {
        let _e94 = c_1;
        param = _e94.xyz;
        let _e96 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e96.x;
        c_1[1u] = _e96.y;
        c_1[2u] = _e96.z;
    }
    let _e103 = (*slot);
    if (lightmap_slot == (_e103 + 1i)) {
        let _e106 = c_1;
        param_1 = _e106.xyz;
        let _e108 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = c_1;
    return _e115;
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
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e78 = frag_color0In_1;
    param_2 = _e78.xyz;
    let _e80 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e82 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e80.x, _e80.y, _e80.z, _e82);
    param_3 = 0u;
    let _e87 = frag_tex_coord0_1;
    param_4 = _e87;
    param_5 = 0i;
    let _e88 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e88;
    param_6 = 1u;
    let _e89 = frag_tex_coord1_1;
    param_7 = _e89;
    param_8 = 1i;
    let _e90 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e90;
    let _e91 = sampledColor0_;
    let _e92 = frag_color0_;
    color0_ = (_e91 * _e92);
    if override_type_3_ {
        let _e94 = sampledColor1_;
        color1_ = _e94;
        let _e95 = color0_;
        let _e97 = color1_;
        let _e99 = (_e95.xyz + _e97.xyz);
        let _e101 = color0_[3u];
        let _e103 = color1_[3u];
        base = vec4<f32>(_e99.x, _e99.y, _e99.z, (_e101 * _e103));
    } else {
        if override_type_3_1 {
            let _e109 = sampledColor1_;
            let _e110 = frag_color0_;
            color1_1 = (_e109 * _e110);
            let _e112 = color0_;
            let _e114 = color1_1;
            let _e116 = (_e112.xyz + _e114.xyz);
            let _e118 = color0_[3u];
            let _e120 = color1_1[3u];
            base = vec4<f32>(_e116.x, _e116.y, _e116.z, (_e118 * _e120));
        } else {
            let _e126 = sampledColor1_;
            color1_2 = _e126;
            let _e127 = color0_;
            let _e129 = color1_2;
            let _e131 = (_e127.xyz * _e129.xyz);
            base[0u] = _e131.x;
            base[1u] = _e131.y;
            base[2u] = _e131.z;
            let _e139 = color0_[3u];
            let _e141 = color1_2[3u];
            base[3u] = (_e139 * _e141);
        }
    }
    if override_type_3_2 {
        let _e146 = unnamed.worldLightParams[1u];
        wetness = clamp(_e146, 0f, 1f);
        let _e150 = unnamed.worldLightParams[2u];
        frost = clamp(_e150, 0f, 1f);
        let _e152 = base;
        luminance = dot(_e152.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e155 = wetness;
        let _e157 = base;
        let _e159 = (_e157.xyz * mix(1f, 0.82f, _e155));
        base[0u] = _e159.x;
        base[1u] = _e159.y;
        base[2u] = _e159.z;
        let _e166 = base;
        let _e168 = luminance;
        let _e170 = luminance;
        let _e172 = luminance;
        let _e174 = frost;
        let _e177 = mix(_e166.xyz, vec3<f32>((_e168 * 0.88f), (_e170 * 0.94f), _e172), vec3((_e174 * 0.55f)));
        base[0u] = _e177.x;
        base[1u] = _e177.y;
        base[2u] = _e177.z;
    }
    let _e186 = unnamed.materialFactors[0u];
    if (_e186 >= 0f) {
        let _e189 = unnamed.materialBaseColor;
        let _e190 = base;
        base = (_e190 * _e189);
    }
    let _e192 = color0_;
    let _e195 = unnamed.emissionRadiance;
    let _e198 = base;
    let _e200 = (_e198.xyz + (_e192.xyz * _e195.xyz));
    base[0u] = _e200.x;
    base[1u] = _e200.y;
    base[2u] = _e200.z;
    let _e207 = wired_advanced_fog_enabled_u0028_();
    if _e207 {
        let _e208 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e208;
        let _e209 = base;
        let _e212 = unnamed.advancedFogColorDensity;
        let _e214 = fogAmount;
        let _e216 = mix(_e209.xyz, _e212.xyz, vec3(_e214));
        base[0u] = _e216.x;
        base[1u] = _e216.y;
        base[2u] = _e216.z;
    }
    if override_type_3_3 {
        let _e224 = base[3u];
        if (_e224 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e226 = base;
            let _e228 = base;
            if (dot(_e226.xyz, _e228.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e232 = base;
    out_color = _e232;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
