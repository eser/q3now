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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e49 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e49 + 0.5f));
    let _e54 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e56 = fogType;
    let _e59 = fogType;
    return (((_e54 > 0.5f) && (_e56 >= 1i)) && (_e59 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e49 = wired_advanced_fog_enabled_u0028_();
    if !(_e49) {
        return 0f;
    }
    let _e52 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e52, 0.000001f));
    let _e57 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e57 + 0.5f));
    let _e60 = fogType_1;
    if (_e60 == 1i) {
        let _e64 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e64 <= 0f) {
            return 0f;
        }
        let _e66 = viewDepth;
        let _e69 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e66 / _e69), 0f, 1f);
    }
    let _e74 = unnamed.advancedFogColorDensity[3u];
    let _e76 = viewDepth;
    opticalDepth = (max(_e74, 0f) * _e76);
    let _e78 = fogType_1;
    if (_e78 == 2i) {
        let _e80 = opticalDepth;
        return clamp((1f - exp(-(_e80))), 0f, 1f);
    }
    let _e85 = opticalDepth;
    let _e86 = opticalDepth;
    return clamp((1f - exp(-((_e85 * _e86)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e50 = (*c);
    (*c) = max(_e50, vec3<f32>(0f, 0f, 0f));
    let _e52 = (*c);
    cutoff = (_e52 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e54 = (*c);
    lo = (_e54 / vec3(12.92f));
    let _e57 = (*c);
    hi = pow(((_e57 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e62 = hi;
    let _e63 = lo;
    let _e64 = cutoff;
    return mix(_e62, _e63, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e64));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e51 = (*role);
    let _e53 = (*role);
    let _e58 = unnamed.packed_indices[(_e51 / 4u)][(_e53 % 4u)];
    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e73 = (*uv);
    let _e74 = textureSample(wired_bindless_images[(_e58 & 4095u)], wired_bindless_samplers[((_e68 >> bitcast<u32>(12i)) & 255u)], _e73);
    c_1 = _e74;
    let _e75 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e75))) == 0i) {
        let _e80 = c_1;
        param = _e80.xyz;
        let _e82 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e82.x;
        c_1[1u] = _e82.y;
        c_1[2u] = _e82.z;
    }
    let _e89 = (*slot);
    if (lightmap_slot == (_e89 + 1i)) {
        let _e94 = unnamed.worldLightParams[0u];
        let _e95 = c_1;
        let _e97 = (_e95.xyz * _e94);
        c_1[0u] = _e97.x;
        c_1[1u] = _e97.y;
        c_1[2u] = _e97.z;
    }
    let _e104 = c_1;
    return _e104;
}

fn main_1() {
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

    param_1 = 0u;
    let _e64 = frag_tex_coord0_1;
    param_2 = _e64;
    param_3 = 0i;
    let _e65 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e65;
    if override_type_3_ {
        param_4 = 1u;
        let _e66 = frag_tex_coord1_1;
        param_5 = _e66;
        param_6 = 1i;
        let _e67 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e67;
        let _e68 = color0_;
        let _e70 = color1_;
        let _e72 = (_e68.xyz + _e70.xyz);
        let _e74 = color0_[3u];
        let _e76 = color1_[3u];
        base = vec4<f32>(_e72.x, _e72.y, _e72.z, (_e74 * _e76));
    } else {
        if override_type_3_1 {
            param_7 = 1u;
            let _e82 = frag_tex_coord1_1;
            param_8 = _e82;
            param_9 = 1i;
            let _e83 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            color1_1 = _e83;
            let _e84 = color0_;
            let _e86 = color1_1;
            let _e88 = (_e84.xyz + _e86.xyz);
            let _e90 = color0_[3u];
            let _e92 = color1_1[3u];
            base = vec4<f32>(_e88.x, _e88.y, _e88.z, (_e90 * _e92));
        } else {
            param_10 = 1u;
            let _e98 = frag_tex_coord1_1;
            param_11 = _e98;
            param_12 = 1i;
            let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            color1_2 = _e99;
            let _e100 = color0_;
            let _e102 = color1_2;
            let _e104 = (_e100.xyz * _e102.xyz);
            base[0u] = _e104.x;
            base[1u] = _e104.y;
            base[2u] = _e104.z;
            let _e112 = color0_[3u];
            let _e114 = color1_2[3u];
            base[3u] = (_e112 * _e114);
        }
    }
    let _e117 = wired_advanced_fog_enabled_u0028_();
    if _e117 {
        let _e118 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e118;
        let _e119 = base;
        let _e122 = unnamed.advancedFogColorDensity;
        let _e124 = fogAmount;
        let _e126 = mix(_e119.xyz, _e122.xyz, vec3(_e124));
        base[0u] = _e126.x;
        base[1u] = _e126.y;
        base[2u] = _e126.z;
    }
    if override_type_3_2 {
        let _e134 = base[3u];
        if (_e134 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e136 = base;
            let _e138 = base;
            if (dot(_e136.xyz, _e138.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e142 = base;
    out_color = _e142;
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
