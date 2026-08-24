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
@id(10) override acff: i32 = 0i;
override override_type_3_8: bool = (acff == 1i);
override override_type_3_9: bool = (acff == 2i);
override override_type_3_10: bool = (acff == 3i);
override override_type_3_11: bool = (acff == 1i);
override override_type_3_12: bool = (acff == 2i);
override override_type_3_13: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_14: bool = (discard_mode == 1i);
override override_type_3_15: bool = (discard_mode == 2i);
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
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e69 + 0.5f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e76 = fogType;
    let _e79 = fogType;
    return (((_e74 > 0.5f) && (_e76 >= 1i)) && (_e79 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e69 = wired_advanced_fog_enabled_u0028_();
    if !(_e69) {
        return 0f;
    }
    let _e72 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e72, 0.000001f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e77 + 0.5f));
    let _e80 = fogType_1;
    if (_e80 == 1i) {
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e84 <= 0f) {
            return 0f;
        }
        let _e86 = viewDepth;
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e86 / _e89), 0f, 1f);
    }
    let _e94 = unnamed.advancedFogColorDensity[3u];
    let _e96 = viewDepth;
    opticalDepth = (max(_e94, 0f) * _e96);
    let _e98 = fogType_1;
    if (_e98 == 2i) {
        let _e100 = opticalDepth;
        return clamp((1f - exp(-(_e100))), 0f, 1f);
    }
    let _e105 = opticalDepth;
    let _e106 = opticalDepth;
    return clamp((1f - exp(-((_e105 * _e106)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c);
    (*c) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_1 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_1;
        param = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e114 = unnamed.worldLightParams[0u];
        let _e115 = c_1;
        let _e117 = (_e115.xyz * _e114);
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e108 = unnamed.packed_indices[0i][3u];
    let _e114 = unnamed.packed_indices[0i][3u];
    let _e119 = fog_tex_coord_1;
    let _e120 = textureSample(wired_bindless_images[(_e108 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    fog = _e120;
    let _e121 = frag_color0In_1;
    param_1 = _e121.xyz;
    let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e125 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e123.x, _e123.y, _e123.z, _e125);
    let _e130 = frag_color1In_1;
    param_2 = _e130.xyz;
    let _e132 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e134 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e132.x, _e132.y, _e132.z, _e134);
    param_3 = 0u;
    let _e139 = frag_tex_coord0_1;
    param_4 = _e139;
    param_5 = 0i;
    let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e141 = frag_color0_;
    color0_ = (_e140 * _e141);
    if override_type_3_2 {
        param_6 = 1u;
        let _e143 = frag_tex_coord1_1;
        param_7 = _e143;
        param_8 = 1i;
        let _e144 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e145 = frag_color1_;
        color1_ = (_e144 * _e145);
        let _e147 = color0_;
        let _e149 = color1_;
        let _e151 = (_e147.xyz + _e149.xyz);
        let _e153 = color0_[3u];
        let _e155 = color1_[3u];
        base = vec4<f32>(_e151.x, _e151.y, _e151.z, (_e153 * _e155));
    } else {
        if override_type_3_3 {
            param_9 = 1u;
            let _e161 = frag_tex_coord1_1;
            param_10 = _e161;
            param_11 = 1i;
            let _e162 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e163 = frag_color1_;
            color1_1 = (_e162 * _e163);
            let _e166 = color0_[3u];
            let _e167 = color0_;
            color0_ = (_e167 * _e166);
            let _e170 = color1_1[3u];
            let _e171 = color1_1;
            color1_1 = (_e171 * _e170);
            let _e173 = color0_;
            let _e175 = color1_1;
            let _e177 = (_e173.xyz + _e175.xyz);
            let _e179 = color0_[3u];
            let _e181 = color1_1[3u];
            base = vec4<f32>(_e177.x, _e177.y, _e177.z, (_e179 * _e181));
        } else {
            if override_type_3_4 {
                param_12 = 1u;
                let _e187 = frag_tex_coord1_1;
                param_13 = _e187;
                param_14 = 1i;
                let _e188 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e189 = frag_color1_;
                color1_2 = (_e188 * _e189);
                let _e192 = color0_[3u];
                let _e194 = color0_;
                color0_ = (_e194 * (1f - _e192));
                let _e197 = color1_2[3u];
                let _e199 = color1_2;
                color1_2 = (_e199 * (1f - _e197));
                let _e201 = color0_;
                let _e203 = color1_2;
                let _e205 = (_e201.xyz + _e203.xyz);
                let _e207 = color0_[3u];
                let _e209 = color1_2[3u];
                base = vec4<f32>(_e205.x, _e205.y, _e205.z, (_e207 * _e209));
            } else {
                if override_type_3_5 {
                    param_15 = 1u;
                    let _e215 = frag_tex_coord1_1;
                    param_16 = _e215;
                    param_17 = 1i;
                    let _e216 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e217 = frag_color1_;
                    color1_3 = (_e216 * _e217);
                    let _e219 = color0_;
                    let _e220 = color1_3;
                    let _e222 = color1_3[3u];
                    base = mix(_e219, _e220, vec4(_e222));
                } else {
                    if override_type_3_6 {
                        param_18 = 1u;
                        let _e225 = frag_tex_coord1_1;
                        param_19 = _e225;
                        param_20 = 1i;
                        let _e226 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e227 = frag_color1_;
                        color1_4 = (_e226 * _e227);
                        let _e229 = color1_4;
                        let _e230 = color0_;
                        let _e232 = color1_4[3u];
                        base = mix(_e229, _e230, vec4(_e232));
                    } else {
                        if override_type_3_7 {
                            param_21 = 1u;
                            let _e235 = frag_tex_coord1_1;
                            param_22 = _e235;
                            param_23 = 1i;
                            let _e236 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e237 = frag_color1_;
                            color1_5 = (_e236 * _e237);
                            let _e239 = color1_5;
                            let _e241 = color1_5[3u];
                            let _e244 = color0_;
                            base = ((_e239 + vec4(_e241)) * _e244);
                        } else {
                            param_24 = 1u;
                            let _e246 = frag_tex_coord1_1;
                            param_25 = _e246;
                            param_26 = 1i;
                            let _e247 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e248 = frag_color1_;
                            color1_6 = (_e247 * _e248);
                            let _e250 = color0_;
                            let _e252 = color1_6;
                            let _e254 = (_e250.xyz * _e252.xyz);
                            base[0u] = _e254.x;
                            base[1u] = _e254.y;
                            base[2u] = _e254.z;
                            let _e262 = color0_[3u];
                            let _e264 = color1_6[3u];
                            base[3u] = (_e262 * _e264);
                        }
                    }
                }
            }
        }
    }
    let _e267 = wired_advanced_fog_enabled_u0028_();
    if _e267 {
        let _e268 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e268;
        if override_type_3_8 {
            let _e269 = fogAmount;
            let _e271 = base;
            let _e273 = (_e271.xyz * (1f - _e269));
            base[0u] = _e273.x;
            base[1u] = _e273.y;
            base[2u] = _e273.z;
        } else {
            if override_type_3_9 {
                let _e280 = fogAmount;
                let _e282 = base;
                base = (_e282 * (1f - _e280));
            } else {
                if override_type_3_10 {
                    let _e284 = fogAmount;
                    let _e287 = base[3u];
                    base[3u] = (_e287 * (1f - _e284));
                } else {
                    let _e290 = base;
                    let _e293 = unnamed.advancedFogColorDensity;
                    let _e295 = fogAmount;
                    let _e297 = mix(_e290.xyz, _e293.xyz, vec3(_e295));
                    base[0u] = _e297.x;
                    base[1u] = _e297.y;
                    base[2u] = _e297.z;
                }
            }
        }
    } else {
        if override_type_3_11 {
            let _e304 = base;
            let _e307 = fog[3u];
            let _e309 = (_e304.xyz * (1f - _e307));
            base[0u] = _e309.x;
            base[1u] = _e309.y;
            base[2u] = _e309.z;
        } else {
            if override_type_3_12 {
                let _e316 = base;
                let _e318 = fog[3u];
                base = (_e316 * (1f - _e318));
            } else {
                if override_type_3_13 {
                    let _e322 = base[3u];
                    let _e324 = fog[3u];
                    base[3u] = (_e322 * (1f - _e324));
                } else {
                    let _e328 = base;
                    let _e329 = fog;
                    let _e331 = unnamed.fogColor;
                    let _e334 = fog[3u];
                    base = mix(_e328, (_e329 * _e331), vec4(_e334));
                }
            }
        }
    }
    if override_type_3_14 {
        let _e338 = base[3u];
        if (_e338 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e340 = base;
            let _e342 = base;
            if (dot(_e340.xyz, _e342.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e346 = base;
    out_color = _e346;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e13 = out_color;
    return _e13;
}
