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
    var fog: vec4<f32>;
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

    let _e80 = unnamed.packed_indices[0i][3u];
    let _e86 = unnamed.packed_indices[0i][3u];
    let _e91 = fog_tex_coord_1;
    let _e92 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e86 >> bitcast<u32>(12i)) & 255u)], _e91);
    fog = _e92;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e97 = frag_tex_coord0_1;
    param_2 = _e97;
    param_3 = 0i;
    let _e98 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e99 = frag_color;
    color0_ = (_e98 * _e99);
    if override_type_3_ {
        param_4 = 1u;
        let _e101 = frag_tex_coord1_1;
        param_5 = _e101;
        param_6 = 1i;
        let _e102 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e102;
        let _e103 = color0_;
        let _e105 = color1_;
        let _e107 = (_e103.xyz + _e105.xyz);
        let _e109 = color0_[3u];
        let _e111 = color1_[3u];
        base = vec4<f32>(_e107.x, _e107.y, _e107.z, (_e109 * _e111));
    } else {
        if override_type_3_1 {
            param_7 = 1u;
            let _e117 = frag_tex_coord1_1;
            param_8 = _e117;
            param_9 = 1i;
            let _e118 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e119 = frag_color;
            color1_1 = (_e118 * _e119);
            let _e121 = color0_;
            let _e123 = color1_1;
            let _e125 = (_e121.xyz + _e123.xyz);
            let _e127 = color0_[3u];
            let _e129 = color1_1[3u];
            base = vec4<f32>(_e125.x, _e125.y, _e125.z, (_e127 * _e129));
        } else {
            param_10 = 1u;
            let _e135 = frag_tex_coord1_1;
            param_11 = _e135;
            param_12 = 1i;
            let _e136 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e137 = frag_color;
            color1_2 = (_e136 * _e137);
            let _e139 = color0_;
            let _e141 = color1_2;
            let _e143 = (_e139.xyz * _e141.xyz);
            base[0u] = _e143.x;
            base[1u] = _e143.y;
            base[2u] = _e143.z;
            let _e151 = color0_[3u];
            let _e153 = color1_2[3u];
            base[3u] = (_e151 * _e153);
        }
    }
    let _e156 = wired_advanced_fog_enabled_u0028_();
    if _e156 {
        let _e157 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e157;
        if override_type_3_2 {
            let _e158 = fogAmount;
            let _e160 = base;
            let _e162 = (_e160.xyz * (1f - _e158));
            base[0u] = _e162.x;
            base[1u] = _e162.y;
            base[2u] = _e162.z;
        } else {
            if override_type_3_3 {
                let _e169 = fogAmount;
                let _e171 = base;
                base = (_e171 * (1f - _e169));
            } else {
                if override_type_3_4 {
                    let _e173 = fogAmount;
                    let _e176 = base[3u];
                    base[3u] = (_e176 * (1f - _e173));
                } else {
                    let _e179 = base;
                    let _e182 = unnamed.advancedFogColorDensity;
                    let _e184 = fogAmount;
                    let _e186 = mix(_e179.xyz, _e182.xyz, vec3(_e184));
                    base[0u] = _e186.x;
                    base[1u] = _e186.y;
                    base[2u] = _e186.z;
                }
            }
        }
    } else {
        if override_type_3_5 {
            let _e193 = base;
            let _e196 = fog[3u];
            let _e198 = (_e193.xyz * (1f - _e196));
            base[0u] = _e198.x;
            base[1u] = _e198.y;
            base[2u] = _e198.z;
        } else {
            if override_type_3_6 {
                let _e205 = base;
                let _e207 = fog[3u];
                base = (_e205 * (1f - _e207));
            } else {
                if override_type_3_7 {
                    let _e211 = base[3u];
                    let _e213 = fog[3u];
                    base[3u] = (_e211 * (1f - _e213));
                } else {
                    let _e217 = base;
                    let _e218 = fog;
                    let _e220 = unnamed.fogColor;
                    let _e223 = fog[3u];
                    base = mix(_e217, (_e218 * _e220), vec4(_e223));
                }
            }
        }
    }
    if override_type_3_8 {
        let _e227 = base[3u];
        if (_e227 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e229 = base;
            let _e231 = base;
            if (dot(_e229.xyz, _e231.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e235 = base;
    out_color = _e235;
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
