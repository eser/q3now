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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
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
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var sampledColor1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    param_2 = 0u;
    let _e78 = frag_tex_coord0_1;
    param_3 = _e78;
    param_4 = 0i;
    let _e79 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e79;
    param_5 = 1u;
    let _e80 = frag_tex_coord1_1;
    param_6 = _e80;
    param_7 = 1i;
    let _e81 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e81;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e86 = sampledColor0_;
    let _e87 = frag_color;
    color0_ = (_e86 * _e87);
    if override_type_3_ {
        let _e89 = sampledColor1_;
        color1_ = _e89;
        let _e90 = color0_;
        let _e92 = color1_;
        let _e94 = (_e90.xyz + _e92.xyz);
        let _e96 = color0_[3u];
        let _e98 = color1_[3u];
        base = vec4<f32>(_e94.x, _e94.y, _e94.z, (_e96 * _e98));
    } else {
        if override_type_3_1 {
            let _e104 = sampledColor1_;
            let _e105 = frag_color;
            color1_1 = (_e104 * _e105);
            let _e107 = color0_;
            let _e109 = color1_1;
            let _e111 = (_e107.xyz + _e109.xyz);
            let _e113 = color0_[3u];
            let _e115 = color1_1[3u];
            base = vec4<f32>(_e111.x, _e111.y, _e111.z, (_e113 * _e115));
        } else {
            let _e121 = sampledColor1_;
            let _e122 = frag_color;
            color1_2 = (_e121 * _e122);
            let _e124 = color0_;
            let _e126 = color1_2;
            let _e128 = (_e124.xyz * _e126.xyz);
            base[0u] = _e128.x;
            base[1u] = _e128.y;
            base[2u] = _e128.z;
            let _e136 = color0_[3u];
            let _e138 = color1_2[3u];
            base[3u] = (_e136 * _e138);
        }
    }
    if override_type_3_2 {
        let _e143 = unnamed.worldLightParams[1u];
        wetness = clamp(_e143, 0f, 1f);
        let _e147 = unnamed.worldLightParams[2u];
        frost = clamp(_e147, 0f, 1f);
        let _e149 = base;
        luminance = dot(_e149.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e152 = wetness;
        let _e154 = base;
        let _e156 = (_e154.xyz * mix(1f, 0.82f, _e152));
        base[0u] = _e156.x;
        base[1u] = _e156.y;
        base[2u] = _e156.z;
        let _e163 = base;
        let _e165 = luminance;
        let _e167 = luminance;
        let _e169 = luminance;
        let _e171 = frost;
        let _e174 = mix(_e163.xyz, vec3<f32>((_e165 * 0.88f), (_e167 * 0.94f), _e169), vec3((_e171 * 0.55f)));
        base[0u] = _e174.x;
        base[1u] = _e174.y;
        base[2u] = _e174.z;
    }
    let _e183 = unnamed.materialFactors[0u];
    if (_e183 >= 0f) {
        let _e186 = unnamed.materialBaseColor;
        let _e187 = base;
        base = (_e187 * _e186);
    }
    let _e189 = color0_;
    let _e192 = unnamed.emissionRadiance;
    let _e195 = base;
    let _e197 = (_e195.xyz + (_e189.xyz * _e192.xyz));
    base[0u] = _e197.x;
    base[1u] = _e197.y;
    base[2u] = _e197.z;
    let _e204 = wired_advanced_fog_enabled_u0028_();
    if _e204 {
        let _e205 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e205;
        let _e206 = base;
        let _e209 = unnamed.advancedFogColorDensity;
        let _e211 = fogAmount;
        let _e213 = mix(_e206.xyz, _e209.xyz, vec3(_e211));
        base[0u] = _e213.x;
        base[1u] = _e213.y;
        base[2u] = _e213.z;
    }
    if override_type_3_3 {
        let _e221 = base[3u];
        if (_e221 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e223 = base;
            let _e225 = base;
            if (dot(_e223.xyz, _e225.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e229 = base;
    out_color = _e229;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
