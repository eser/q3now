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
@id(10) override acff: i32 = 0i;
override override_type_3_2: bool = (acff == 1i);
override override_type_3_3: bool = (acff == 2i);
override override_type_3_4: bool = (acff == 3i);
override override_type_3_5: bool = (acff == 1i);
override override_type_3_6: bool = (acff == 2i);
override override_type_3_7: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_8: bool = (discard_mode == 1i);
override override_type_3_9: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e58 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e58 + 0.5f));
    let _e63 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e65 = fogType;
    let _e68 = fogType;
    return (((_e63 > 0.5f) && (_e65 >= 1i)) && (_e68 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e58 = wired_advanced_fog_enabled_u0028_();
    if !(_e58) {
        return 0f;
    }
    let _e61 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e61, 0.000001f));
    let _e66 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e66 + 0.5f));
    let _e69 = fogType_1;
    if (_e69 == 1i) {
        let _e73 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e73 <= 0f) {
            return 0f;
        }
        let _e75 = viewDepth;
        let _e78 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e75 / _e78), 0f, 1f);
    }
    let _e83 = unnamed.advancedFogColorDensity[3u];
    let _e85 = viewDepth;
    opticalDepth = (max(_e83, 0f) * _e85);
    let _e87 = fogType_1;
    if (_e87 == 2i) {
        let _e89 = opticalDepth;
        return clamp((1f - exp(-(_e89))), 0f, 1f);
    }
    let _e94 = opticalDepth;
    let _e95 = opticalDepth;
    return clamp((1f - exp(-((_e94 * _e95)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e59 = (*c);
    (*c) = max(_e59, vec3<f32>(0f, 0f, 0f));
    let _e61 = (*c);
    cutoff = (_e61 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e63 = (*c);
    lo = (_e63 / vec3(12.92f));
    let _e66 = (*c);
    hi = pow(((_e66 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e71 = hi;
    let _e72 = lo;
    let _e73 = cutoff;
    return mix(_e71, _e72, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e73));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e60 = (*role);
    let _e62 = (*role);
    let _e67 = unnamed.packed_indices[(_e60 / 4u)][(_e62 % 4u)];
    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e82 = (*uv);
    let _e83 = textureSample(wired_bindless_images[(_e67 & 4095u)], wired_bindless_samplers[((_e77 >> bitcast<u32>(12i)) & 255u)], _e82);
    c_1 = _e83;
    let _e84 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e84))) == 0i) {
        let _e89 = c_1;
        param = _e89.xyz;
        let _e91 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e91.x;
        c_1[1u] = _e91.y;
        c_1[2u] = _e91.z;
    }
    let _e98 = (*slot);
    if (lightmap_slot == (_e98 + 1i)) {
        let _e103 = unnamed.worldLightParams[0u];
        let _e104 = c_1;
        let _e106 = (_e104.xyz * _e103);
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = c_1;
    return _e113;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e77 = unnamed.packed_indices[0i][3u];
    let _e83 = unnamed.packed_indices[0i][3u];
    let _e88 = fog_tex_coord_1;
    let _e89 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    fog = _e89;
    param_1 = 0u;
    let _e90 = frag_tex_coord0_1;
    param_2 = _e90;
    param_3 = 0i;
    let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e91;
    if override_type_3_ {
        param_4 = 1u;
        let _e92 = frag_tex_coord1_1;
        param_5 = _e92;
        param_6 = 1i;
        let _e93 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e93;
        let _e94 = color0_;
        let _e96 = color1_;
        let _e98 = (_e94.xyz + _e96.xyz);
        let _e100 = color0_[3u];
        let _e102 = color1_[3u];
        base = vec4<f32>(_e98.x, _e98.y, _e98.z, (_e100 * _e102));
    } else {
        if override_type_3_1 {
            param_7 = 1u;
            let _e108 = frag_tex_coord1_1;
            param_8 = _e108;
            param_9 = 1i;
            let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            color1_1 = _e109;
            let _e110 = color0_;
            let _e112 = color1_1;
            let _e114 = (_e110.xyz + _e112.xyz);
            let _e116 = color0_[3u];
            let _e118 = color1_1[3u];
            base = vec4<f32>(_e114.x, _e114.y, _e114.z, (_e116 * _e118));
        } else {
            param_10 = 1u;
            let _e124 = frag_tex_coord1_1;
            param_11 = _e124;
            param_12 = 1i;
            let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            color1_2 = _e125;
            let _e126 = color0_;
            let _e128 = color1_2;
            let _e130 = (_e126.xyz * _e128.xyz);
            base[0u] = _e130.x;
            base[1u] = _e130.y;
            base[2u] = _e130.z;
            let _e138 = color0_[3u];
            let _e140 = color1_2[3u];
            base[3u] = (_e138 * _e140);
        }
    }
    let _e143 = wired_advanced_fog_enabled_u0028_();
    if _e143 {
        let _e144 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e144;
        if override_type_3_2 {
            let _e145 = fogAmount;
            let _e147 = base;
            let _e149 = (_e147.xyz * (1f - _e145));
            base[0u] = _e149.x;
            base[1u] = _e149.y;
            base[2u] = _e149.z;
        } else {
            if override_type_3_3 {
                let _e156 = fogAmount;
                let _e158 = base;
                base = (_e158 * (1f - _e156));
            } else {
                if override_type_3_4 {
                    let _e160 = fogAmount;
                    let _e163 = base[3u];
                    base[3u] = (_e163 * (1f - _e160));
                } else {
                    let _e166 = base;
                    let _e169 = unnamed.advancedFogColorDensity;
                    let _e171 = fogAmount;
                    let _e173 = mix(_e166.xyz, _e169.xyz, vec3(_e171));
                    base[0u] = _e173.x;
                    base[1u] = _e173.y;
                    base[2u] = _e173.z;
                }
            }
        }
    } else {
        if override_type_3_5 {
            let _e180 = base;
            let _e183 = fog[3u];
            let _e185 = (_e180.xyz * (1f - _e183));
            base[0u] = _e185.x;
            base[1u] = _e185.y;
            base[2u] = _e185.z;
        } else {
            if override_type_3_6 {
                let _e192 = base;
                let _e194 = fog[3u];
                base = (_e192 * (1f - _e194));
            } else {
                if override_type_3_7 {
                    let _e198 = base[3u];
                    let _e200 = fog[3u];
                    base[3u] = (_e198 * (1f - _e200));
                } else {
                    let _e204 = base;
                    let _e205 = fog;
                    let _e207 = unnamed.fogColor;
                    let _e210 = fog[3u];
                    base = mix(_e204, (_e205 * _e207), vec4(_e210));
                }
            }
        }
    }
    if override_type_3_8 {
        let _e214 = base[3u];
        if (_e214 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e216 = base;
            let _e218 = base;
            if (dot(_e216.xyz, _e218.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e222 = base;
    out_color = _e222;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
