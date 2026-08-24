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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e61 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e61 + 0.5f));
    let _e66 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e68 = fogType;
    let _e71 = fogType;
    return (((_e66 > 0.5f) && (_e68 >= 1i)) && (_e71 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e61 = wired_advanced_fog_enabled_u0028_();
    if !(_e61) {
        return 0f;
    }
    let _e64 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e64, 0.000001f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e69 + 0.5f));
    let _e72 = fogType_1;
    if (_e72 == 1i) {
        let _e76 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e76 <= 0f) {
            return 0f;
        }
        let _e78 = viewDepth;
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e78 / _e81), 0f, 1f);
    }
    let _e86 = unnamed.advancedFogColorDensity[3u];
    let _e88 = viewDepth;
    opticalDepth = (max(_e86, 0f) * _e88);
    let _e90 = fogType_1;
    if (_e90 == 2i) {
        let _e92 = opticalDepth;
        return clamp((1f - exp(-(_e92))), 0f, 1f);
    }
    let _e97 = opticalDepth;
    let _e98 = opticalDepth;
    return clamp((1f - exp(-((_e97 * _e98)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c);
    (*c) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_1 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_1;
        param = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_1;
        let _e109 = (_e107.xyz * _e106);
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_2: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_3: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_4: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_5: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_6: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var fogAmount: f32;

    let _e96 = frag_color0In_1;
    param_1 = _e96.xyz;
    let _e98 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e100 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e98.x, _e98.y, _e98.z, _e100);
    let _e105 = frag_color1In_1;
    param_2 = _e105.xyz;
    let _e107 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e109 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e107.x, _e107.y, _e107.z, _e109);
    param_3 = 0u;
    let _e114 = frag_tex_coord0_1;
    param_4 = _e114;
    param_5 = 0i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e116 = frag_color0_;
    color0_ = (_e115 * _e116);
    if override_type_3_2 {
        param_6 = 1u;
        let _e118 = frag_tex_coord1_1;
        param_7 = _e118;
        param_8 = 1i;
        let _e119 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e120 = frag_color1_;
        color1_ = (_e119 * _e120);
        let _e122 = color0_;
        let _e124 = color1_;
        let _e126 = (_e122.xyz + _e124.xyz);
        let _e128 = color0_[3u];
        let _e130 = color1_[3u];
        base = vec4<f32>(_e126.x, _e126.y, _e126.z, (_e128 * _e130));
    } else {
        if override_type_3_3 {
            param_9 = 1u;
            let _e136 = frag_tex_coord1_1;
            param_10 = _e136;
            param_11 = 1i;
            let _e137 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e138 = frag_color1_;
            color1_1 = (_e137 * _e138);
            let _e141 = color0_[3u];
            let _e142 = color0_;
            color0_ = (_e142 * _e141);
            let _e145 = color1_1[3u];
            let _e146 = color1_1;
            color1_1 = (_e146 * _e145);
            let _e148 = color0_;
            let _e150 = color1_1;
            let _e152 = (_e148.xyz + _e150.xyz);
            let _e154 = color0_[3u];
            let _e156 = color1_1[3u];
            base = vec4<f32>(_e152.x, _e152.y, _e152.z, (_e154 * _e156));
        } else {
            if override_type_3_4 {
                param_12 = 1u;
                let _e162 = frag_tex_coord1_1;
                param_13 = _e162;
                param_14 = 1i;
                let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e164 = frag_color1_;
                color1_2 = (_e163 * _e164);
                let _e167 = color0_[3u];
                let _e169 = color0_;
                color0_ = (_e169 * (1f - _e167));
                let _e172 = color1_2[3u];
                let _e174 = color1_2;
                color1_2 = (_e174 * (1f - _e172));
                let _e176 = color0_;
                let _e178 = color1_2;
                let _e180 = (_e176.xyz + _e178.xyz);
                let _e182 = color0_[3u];
                let _e184 = color1_2[3u];
                base = vec4<f32>(_e180.x, _e180.y, _e180.z, (_e182 * _e184));
            } else {
                if override_type_3_5 {
                    param_15 = 1u;
                    let _e190 = frag_tex_coord1_1;
                    param_16 = _e190;
                    param_17 = 1i;
                    let _e191 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e192 = frag_color1_;
                    color1_3 = (_e191 * _e192);
                    let _e194 = color0_;
                    let _e195 = color1_3;
                    let _e197 = color1_3[3u];
                    base = mix(_e194, _e195, vec4(_e197));
                } else {
                    if override_type_3_6 {
                        param_18 = 1u;
                        let _e200 = frag_tex_coord1_1;
                        param_19 = _e200;
                        param_20 = 1i;
                        let _e201 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e202 = frag_color1_;
                        color1_4 = (_e201 * _e202);
                        let _e204 = color1_4;
                        let _e205 = color0_;
                        let _e207 = color1_4[3u];
                        base = mix(_e204, _e205, vec4(_e207));
                    } else {
                        if override_type_3_7 {
                            param_21 = 1u;
                            let _e210 = frag_tex_coord1_1;
                            param_22 = _e210;
                            param_23 = 1i;
                            let _e211 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e212 = frag_color1_;
                            color1_5 = (_e211 * _e212);
                            let _e214 = color1_5;
                            let _e216 = color1_5[3u];
                            let _e219 = color0_;
                            base = ((_e214 + vec4(_e216)) * _e219);
                        } else {
                            param_24 = 1u;
                            let _e221 = frag_tex_coord1_1;
                            param_25 = _e221;
                            param_26 = 1i;
                            let _e222 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e223 = frag_color1_;
                            color1_6 = (_e222 * _e223);
                            let _e225 = color0_;
                            let _e227 = color1_6;
                            let _e229 = (_e225.xyz * _e227.xyz);
                            base[0u] = _e229.x;
                            base[1u] = _e229.y;
                            base[2u] = _e229.z;
                            let _e237 = color0_[3u];
                            let _e239 = color1_6[3u];
                            base[3u] = (_e237 * _e239);
                        }
                    }
                }
            }
        }
    }
    let _e242 = wired_advanced_fog_enabled_u0028_();
    if _e242 {
        let _e243 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e243;
        let _e244 = base;
        let _e247 = unnamed.advancedFogColorDensity;
        let _e249 = fogAmount;
        let _e251 = mix(_e244.xyz, _e247.xyz, vec3(_e249));
        base[0u] = _e251.x;
        base[1u] = _e251.y;
        base[2u] = _e251.z;
    }
    if override_type_3_8 {
        let _e259 = base[3u];
        if (_e259 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e261 = base;
            let _e263 = base;
            if (dot(_e261.xyz, _e263.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e267 = base;
    out_color = _e267;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e11 = out_color;
    return _e11;
}
