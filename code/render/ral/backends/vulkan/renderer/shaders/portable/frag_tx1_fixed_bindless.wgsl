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
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_2: bool = (discard_mode == 1i);
override override_type_3_3: bool = (discard_mode == 2i);
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

    let _e51 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e51 + 0.5f));
    let _e56 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e58 = fogType;
    let _e61 = fogType;
    return (((_e56 > 0.5f) && (_e58 >= 1i)) && (_e61 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e51 = wired_advanced_fog_enabled_u0028_();
    if !(_e51) {
        return 0f;
    }
    let _e54 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e54, 0.000001f));
    let _e59 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e59 + 0.5f));
    let _e62 = fogType_1;
    if (_e62 == 1i) {
        let _e66 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e66 <= 0f) {
            return 0f;
        }
        let _e68 = viewDepth;
        let _e71 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e68 / _e71), 0f, 1f);
    }
    let _e76 = unnamed.advancedFogColorDensity[3u];
    let _e78 = viewDepth;
    opticalDepth = (max(_e76, 0f) * _e78);
    let _e80 = fogType_1;
    if (_e80 == 2i) {
        let _e82 = opticalDepth;
        return clamp((1f - exp(-(_e82))), 0f, 1f);
    }
    let _e87 = opticalDepth;
    let _e88 = opticalDepth;
    return clamp((1f - exp(-((_e87 * _e88)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e52 = (*c);
    (*c) = max(_e52, vec3<f32>(0f, 0f, 0f));
    let _e54 = (*c);
    cutoff = (_e54 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e56 = (*c);
    lo = (_e56 / vec3(12.92f));
    let _e59 = (*c);
    hi = pow(((_e59 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e64 = hi;
    let _e65 = lo;
    let _e66 = cutoff;
    return mix(_e64, _e65, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e66));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e53 = (*role);
    let _e55 = (*role);
    let _e60 = unnamed.packed_indices[(_e53 / 4u)][(_e55 % 4u)];
    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e75 = (*uv);
    let _e76 = textureSample(wired_bindless_images[(_e60 & 4095u)], wired_bindless_samplers[((_e70 >> bitcast<u32>(12i)) & 255u)], _e75);
    c_1 = _e76;
    let _e77 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e77))) == 0i) {
        let _e82 = c_1;
        param = _e82.xyz;
        let _e84 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e84.x;
        c_1[1u] = _e84.y;
        c_1[2u] = _e84.z;
    }
    let _e91 = (*slot);
    if (lightmap_slot == (_e91 + 1i)) {
        let _e96 = unnamed.worldLightParams[0u];
        let _e97 = c_1;
        let _e99 = (_e97.xyz * _e96);
        c_1[0u] = _e99.x;
        c_1[1u] = _e99.y;
        c_1[2u] = _e99.z;
    }
    let _e106 = c_1;
    return _e106;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var color1_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_2: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var fogAmount: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e71 = frag_tex_coord0_1;
    param_2 = _e71;
    param_3 = 0i;
    let _e72 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e73 = frag_color;
    color0_ = (_e72 * _e73);
    if override_type_3_ {
        param_4 = 1u;
        let _e75 = frag_tex_coord1_1;
        param_5 = _e75;
        param_6 = 1i;
        let _e76 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e76;
        let _e77 = color0_;
        let _e79 = color1_;
        let _e81 = (_e77.xyz + _e79.xyz);
        let _e83 = color0_[3u];
        let _e85 = color1_[3u];
        base = vec4<f32>(_e81.x, _e81.y, _e81.z, (_e83 * _e85));
    } else {
        if override_type_3_1 {
            param_7 = 1u;
            let _e91 = frag_tex_coord1_1;
            param_8 = _e91;
            param_9 = 1i;
            let _e92 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e93 = frag_color;
            color1_1 = (_e92 * _e93);
            let _e95 = color0_;
            let _e97 = color1_1;
            let _e99 = (_e95.xyz + _e97.xyz);
            let _e101 = color0_[3u];
            let _e103 = color1_1[3u];
            base = vec4<f32>(_e99.x, _e99.y, _e99.z, (_e101 * _e103));
        } else {
            param_10 = 1u;
            let _e109 = frag_tex_coord1_1;
            param_11 = _e109;
            param_12 = 1i;
            let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e111 = frag_color;
            color1_2 = (_e110 * _e111);
            let _e113 = color0_;
            let _e115 = color1_2;
            let _e117 = (_e113.xyz * _e115.xyz);
            base[0u] = _e117.x;
            base[1u] = _e117.y;
            base[2u] = _e117.z;
            let _e125 = color0_[3u];
            let _e127 = color1_2[3u];
            base[3u] = (_e125 * _e127);
        }
    }
    let _e130 = wired_advanced_fog_enabled_u0028_();
    if _e130 {
        let _e131 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e131;
        let _e132 = base;
        let _e135 = unnamed.advancedFogColorDensity;
        let _e137 = fogAmount;
        let _e139 = mix(_e132.xyz, _e135.xyz, vec3(_e137));
        base[0u] = _e139.x;
        base[1u] = _e139.y;
        base[2u] = _e139.z;
    }
    if override_type_3_2 {
        let _e147 = base[3u];
        if (_e147 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e149 = base;
            let _e151 = base;
            if (dot(_e149.xyz, _e151.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e155 = base;
    out_color = _e155;
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
