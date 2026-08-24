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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
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
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var fogAmount: f32;

    let _e80 = frag_color0In_1;
    param_1 = _e80.xyz;
    let _e82 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e84 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e82.x, _e82.y, _e82.z, _e84);
    param_2 = 0u;
    let _e89 = frag_tex_coord0_1;
    param_3 = _e89;
    param_4 = 0i;
    let _e90 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e91 = frag_color0_;
    color0_ = (_e90 * _e91);
    if override_type_3_ {
        param_5 = 1u;
        let _e93 = frag_tex_coord1_1;
        param_6 = _e93;
        param_7 = 1i;
        let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e94;
        param_8 = 2u;
        let _e95 = frag_tex_coord2_1;
        param_9 = _e95;
        param_10 = 2i;
        let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e96;
        let _e97 = color0_;
        let _e99 = color1_;
        let _e102 = color2_;
        let _e104 = ((_e97.xyz + _e99.xyz) + _e102.xyz);
        let _e106 = color0_[3u];
        let _e108 = color1_[3u];
        let _e111 = color2_[3u];
        base = vec4<f32>(_e104.x, _e104.y, _e104.z, ((_e106 * _e108) * _e111));
    } else {
        if override_type_3_1 {
            param_11 = 1u;
            let _e117 = frag_tex_coord1_1;
            param_12 = _e117;
            param_13 = 1i;
            let _e118 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e119 = frag_color0_;
            color1_1 = (_e118 * _e119);
            param_14 = 2u;
            let _e121 = frag_tex_coord2_1;
            param_15 = _e121;
            param_16 = 2i;
            let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e123 = frag_color0_;
            color2_1 = (_e122 * _e123);
            let _e125 = color0_;
            let _e127 = color1_1;
            let _e130 = color2_1;
            let _e132 = ((_e125.xyz + _e127.xyz) + _e130.xyz);
            let _e134 = color0_[3u];
            let _e136 = color1_1[3u];
            let _e139 = color2_1[3u];
            base = vec4<f32>(_e132.x, _e132.y, _e132.z, ((_e134 * _e136) * _e139));
        } else {
            param_17 = 1u;
            let _e145 = frag_tex_coord1_1;
            param_18 = _e145;
            param_19 = 1i;
            let _e146 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e146;
            param_20 = 2u;
            let _e147 = frag_tex_coord2_1;
            param_21 = _e147;
            param_22 = 2i;
            let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e148;
            let _e149 = color0_;
            let _e151 = color1_2;
            let _e154 = color2_2;
            let _e156 = ((_e149.xyz * _e151.xyz) * _e154.xyz);
            base[0u] = _e156.x;
            base[1u] = _e156.y;
            base[2u] = _e156.z;
            let _e164 = color0_[3u];
            let _e166 = color1_2[3u];
            let _e169 = color2_2[3u];
            base[3u] = ((_e164 * _e166) * _e169);
        }
    }
    let _e172 = wired_advanced_fog_enabled_u0028_();
    if _e172 {
        let _e173 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e173;
        let _e174 = base;
        let _e177 = unnamed.advancedFogColorDensity;
        let _e179 = fogAmount;
        let _e181 = mix(_e174.xyz, _e177.xyz, vec3(_e179));
        base[0u] = _e181.x;
        base[1u] = _e181.y;
        base[2u] = _e181.z;
    }
    if override_type_3_2 {
        let _e189 = base[3u];
        if (_e189 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e191 = base;
            let _e193 = base;
            if (dot(_e191.xyz, _e193.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e197 = base;
    out_color = _e197;
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
