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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e59 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e59 + 0.5f));
    let _e64 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e66 = fogType;
    let _e69 = fogType;
    return (((_e64 > 0.5f) && (_e66 >= 1i)) && (_e69 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e59 = wired_advanced_fog_enabled_u0028_();
    if !(_e59) {
        return 0f;
    }
    let _e62 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e62, 0.000001f));
    let _e67 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e67 + 0.5f));
    let _e70 = fogType_1;
    if (_e70 == 1i) {
        let _e74 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e74 <= 0f) {
            return 0f;
        }
        let _e76 = viewDepth;
        let _e79 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e76 / _e79), 0f, 1f);
    }
    let _e84 = unnamed.advancedFogColorDensity[3u];
    let _e86 = viewDepth;
    opticalDepth = (max(_e84, 0f) * _e86);
    let _e88 = fogType_1;
    if (_e88 == 2i) {
        let _e90 = opticalDepth;
        return clamp((1f - exp(-(_e90))), 0f, 1f);
    }
    let _e95 = opticalDepth;
    let _e96 = opticalDepth;
    return clamp((1f - exp(-((_e95 * _e96)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c);
    (*c) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e83 = (*uv);
    let _e84 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    c_1 = _e84;
    let _e85 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e85))) == 0i) {
        let _e90 = c_1;
        param = _e90.xyz;
        let _e92 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e92.x;
        c_1[1u] = _e92.y;
        c_1[2u] = _e92.z;
    }
    let _e99 = (*slot);
    if (lightmap_slot == (_e99 + 1i)) {
        let _e104 = unnamed.worldLightParams[0u];
        let _e105 = c_1;
        let _e107 = (_e105.xyz * _e104);
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = c_1;
    return _e114;
}

fn main_1() {
    var fog: vec4<f32>;
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
    var fogAmount: f32;

    let _e80 = unnamed.packed_indices[0i][3u];
    let _e86 = unnamed.packed_indices[0i][3u];
    let _e91 = fog_tex_coord_1;
    let _e92 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e86 >> bitcast<u32>(12i)) & 255u)], _e91);
    fog = _e92;
    let _e93 = frag_color0In_1;
    param_1 = _e93.xyz;
    let _e95 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e97 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e95.x, _e95.y, _e95.z, _e97);
    param_2 = 0u;
    let _e102 = frag_tex_coord0_1;
    param_3 = _e102;
    param_4 = 0i;
    let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e104 = frag_color0_;
    color0_ = (_e103 * _e104);
    if override_type_3_ {
        param_5 = 1u;
        let _e106 = frag_tex_coord1_1;
        param_6 = _e106;
        param_7 = 1i;
        let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e107;
        let _e108 = color0_;
        let _e110 = color1_;
        let _e112 = (_e108.xyz + _e110.xyz);
        let _e114 = color0_[3u];
        let _e116 = color1_[3u];
        base = vec4<f32>(_e112.x, _e112.y, _e112.z, (_e114 * _e116));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e122 = frag_tex_coord1_1;
            param_9 = _e122;
            param_10 = 1i;
            let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e124 = frag_color0_;
            color1_1 = (_e123 * _e124);
            let _e126 = color0_;
            let _e128 = color1_1;
            let _e130 = (_e126.xyz + _e128.xyz);
            let _e132 = color0_[3u];
            let _e134 = color1_1[3u];
            base = vec4<f32>(_e130.x, _e130.y, _e130.z, (_e132 * _e134));
        } else {
            param_11 = 1u;
            let _e140 = frag_tex_coord1_1;
            param_12 = _e140;
            param_13 = 1i;
            let _e141 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e141;
            let _e142 = color0_;
            let _e144 = color1_2;
            let _e146 = (_e142.xyz * _e144.xyz);
            base[0u] = _e146.x;
            base[1u] = _e146.y;
            base[2u] = _e146.z;
            let _e154 = color0_[3u];
            let _e156 = color1_2[3u];
            base[3u] = (_e154 * _e156);
        }
    }
    let _e159 = wired_advanced_fog_enabled_u0028_();
    if _e159 {
        let _e160 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e160;
        if override_type_3_2 {
            let _e161 = fogAmount;
            let _e163 = base;
            let _e165 = (_e163.xyz * (1f - _e161));
            base[0u] = _e165.x;
            base[1u] = _e165.y;
            base[2u] = _e165.z;
        } else {
            if override_type_3_3 {
                let _e172 = fogAmount;
                let _e174 = base;
                base = (_e174 * (1f - _e172));
            } else {
                if override_type_3_4 {
                    let _e176 = fogAmount;
                    let _e179 = base[3u];
                    base[3u] = (_e179 * (1f - _e176));
                } else {
                    let _e182 = base;
                    let _e185 = unnamed.advancedFogColorDensity;
                    let _e187 = fogAmount;
                    let _e189 = mix(_e182.xyz, _e185.xyz, vec3(_e187));
                    base[0u] = _e189.x;
                    base[1u] = _e189.y;
                    base[2u] = _e189.z;
                }
            }
        }
    } else {
        if override_type_3_5 {
            let _e196 = base;
            let _e199 = fog[3u];
            let _e201 = (_e196.xyz * (1f - _e199));
            base[0u] = _e201.x;
            base[1u] = _e201.y;
            base[2u] = _e201.z;
        } else {
            if override_type_3_6 {
                let _e208 = base;
                let _e210 = fog[3u];
                base = (_e208 * (1f - _e210));
            } else {
                if override_type_3_7 {
                    let _e214 = base[3u];
                    let _e216 = fog[3u];
                    base[3u] = (_e214 * (1f - _e216));
                } else {
                    let _e220 = base;
                    let _e221 = fog;
                    let _e223 = unnamed.fogColor;
                    let _e226 = fog[3u];
                    base = mix(_e220, (_e221 * _e223), vec4(_e226));
                }
            }
        }
    }
    if override_type_3_8 {
        let _e230 = base[3u];
        if (_e230 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e232 = base;
            let _e234 = base;
            if (dot(_e232.xyz, _e234.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e238 = base;
    out_color = _e238;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
