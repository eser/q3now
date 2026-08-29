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

    let _e60 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e60 + 0.5f));
    let _e65 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e67 = fogType;
    let _e70 = fogType;
    return (((_e65 > 0.5f) && (_e67 >= 1i)) && (_e70 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e60 = wired_advanced_fog_enabled_u0028_();
    if !(_e60) {
        return 0f;
    }
    let _e63 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e63, 0.000001f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e68 + 0.5f));
    let _e71 = fogType_1;
    if (_e71 == 1i) {
        let _e75 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e75 <= 0f) {
            return 0f;
        }
        let _e77 = viewDepth;
        let _e80 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e77 / _e80), 0f, 1f);
    }
    let _e85 = unnamed.advancedFogColorDensity[3u];
    let _e87 = viewDepth;
    opticalDepth = (max(_e85, 0f) * _e87);
    let _e89 = fogType_1;
    if (_e89 == 2i) {
        let _e91 = opticalDepth;
        return clamp((1f - exp(-(_e91))), 0f, 1f);
    }
    let _e96 = opticalDepth;
    let _e97 = opticalDepth;
    return clamp((1f - exp(-((_e96 * _e97)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e61 = (*c);
    (*c) = max(_e61, vec3<f32>(0f, 0f, 0f));
    let _e63 = (*c);
    cutoff = (_e63 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e65 = (*c);
    lo = (_e65 / vec3(12.92f));
    let _e68 = (*c);
    hi = pow(((_e68 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e73 = hi;
    let _e74 = lo;
    let _e75 = cutoff;
    return mix(_e73, _e74, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e75));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e84 = (*uv);
    let _e85 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    c_1 = _e85;
    let _e86 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e86))) == 0i) {
        let _e91 = c_1;
        param = _e91.xyz;
        let _e93 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e93.x;
        c_1[1u] = _e93.y;
        c_1[2u] = _e93.z;
    }
    let _e100 = (*slot);
    if (lightmap_slot == (_e100 + 1i)) {
        let _e105 = unnamed.worldLightParams[0u];
        let _e106 = c_1;
        let _e108 = (_e106.xyz * _e105);
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = c_1;
    return _e115;
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
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
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
        let _e95 = color0_;
        let _e97 = color1_;
        let _e99 = (_e95.xyz + _e97.xyz);
        let _e101 = color0_[3u];
        let _e103 = color1_[3u];
        base = vec4<f32>(_e99.x, _e99.y, _e99.z, (_e101 * _e103));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e109 = frag_tex_coord1_1;
            param_9 = _e109;
            param_10 = 1i;
            let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e111 = frag_color0_;
            color1_1 = (_e110 * _e111);
            let _e113 = color0_;
            let _e115 = color1_1;
            let _e117 = (_e113.xyz + _e115.xyz);
            let _e119 = color0_[3u];
            let _e121 = color1_1[3u];
            base = vec4<f32>(_e117.x, _e117.y, _e117.z, (_e119 * _e121));
        } else {
            param_11 = 1u;
            let _e127 = frag_tex_coord1_1;
            param_12 = _e127;
            param_13 = 1i;
            let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e128;
            let _e129 = color0_;
            let _e131 = color1_2;
            let _e133 = (_e129.xyz * _e131.xyz);
            base[0u] = _e133.x;
            base[1u] = _e133.y;
            base[2u] = _e133.z;
            let _e141 = color0_[3u];
            let _e143 = color1_2[3u];
            base[3u] = (_e141 * _e143);
        }
    }
    if override_type_3_2 {
        let _e148 = unnamed.worldLightParams[1u];
        wetness = clamp(_e148, 0f, 1f);
        let _e152 = unnamed.worldLightParams[2u];
        frost = clamp(_e152, 0f, 1f);
        let _e154 = base;
        luminance = dot(_e154.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e157 = wetness;
        let _e159 = base;
        let _e161 = (_e159.xyz * mix(1f, 0.82f, _e157));
        base[0u] = _e161.x;
        base[1u] = _e161.y;
        base[2u] = _e161.z;
        let _e168 = base;
        let _e170 = luminance;
        let _e172 = luminance;
        let _e174 = luminance;
        let _e176 = frost;
        let _e179 = mix(_e168.xyz, vec3<f32>((_e170 * 0.88f), (_e172 * 0.94f), _e174), vec3((_e176 * 0.55f)));
        base[0u] = _e179.x;
        base[1u] = _e179.y;
        base[2u] = _e179.z;
    }
    let _e186 = color0_;
    let _e189 = unnamed.emissionRadiance;
    let _e192 = base;
    let _e194 = (_e192.xyz + (_e186.xyz * _e189.xyz));
    base[0u] = _e194.x;
    base[1u] = _e194.y;
    base[2u] = _e194.z;
    let _e201 = wired_advanced_fog_enabled_u0028_();
    if _e201 {
        let _e202 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e202;
        let _e203 = base;
        let _e206 = unnamed.advancedFogColorDensity;
        let _e208 = fogAmount;
        let _e210 = mix(_e203.xyz, _e206.xyz, vec3(_e208));
        base[0u] = _e210.x;
        base[1u] = _e210.y;
        base[2u] = _e210.z;
    }
    if override_type_3_3 {
        let _e218 = base[3u];
        if (_e218 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e220 = base;
            let _e222 = base;
            if (dot(_e220.xyz, _e222.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e226 = base;
    out_color = _e226;
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
