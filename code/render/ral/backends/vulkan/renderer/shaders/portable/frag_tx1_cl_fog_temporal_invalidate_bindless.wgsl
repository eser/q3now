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

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
@id(10) override acff: i32 = 0i;
override override_type_4_8: bool = (acff == 1i);
override override_type_4_9: bool = (acff == 2i);
override override_type_4_10: bool = (acff == 3i);
override override_type_4_11: bool = (acff == 1i);
override override_type_4_12: bool = (acff == 2i);
override override_type_4_13: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_14: bool = (discard_mode == 1i);
override override_type_4_15: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e75 + 0.5f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e82 = fogType;
    let _e85 = fogType;
    return (((_e80 > 0.5f) && (_e82 >= 1i)) && (_e85 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e75 = wired_advanced_fog_enabled_u0028_();
    if !(_e75) {
        return 0f;
    }
    let _e78 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e78, 0.000001f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e83 + 0.5f));
    let _e86 = fogType_1;
    if (_e86 == 1i) {
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e90 <= 0f) {
            return 0f;
        }
        let _e92 = viewDepth;
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e92 / _e95), 0f, 1f);
    }
    let _e100 = unnamed.advancedFogColorDensity[3u];
    let _e102 = viewDepth;
    opticalDepth = (max(_e100, 0f) * _e102);
    let _e104 = fogType_1;
    if (_e104 == 2i) {
        let _e106 = opticalDepth;
        return clamp((1f - exp(-(_e106))), 0f, 1f);
    }
    let _e111 = opticalDepth;
    let _e112 = opticalDepth;
    return clamp((1f - exp(-((_e111 * _e112)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e76 = (*c);
    (*c) = max(_e76, vec3<f32>(0f, 0f, 0f));
    let _e78 = (*c);
    cutoff = (_e78 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e80 = (*c);
    lo = (_e80 / vec3(12.92f));
    let _e83 = (*c);
    hi = pow(((_e83 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e88 = hi;
    let _e89 = lo;
    let _e90 = cutoff;
    return mix(_e88, _e89, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e90));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e99 = (*uv);
    let _e100 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    c_1 = _e100;
    let _e101 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e101))) == 0i) {
        let _e106 = c_1;
        param = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e120 = unnamed.worldLightParams[0u];
        let _e121 = c_1;
        let _e123 = (_e121.xyz * _e120);
        c_1[0u] = _e123.x;
        c_1[1u] = _e123.y;
        c_1[2u] = _e123.z;
    }
    let _e130 = c_1;
    return _e130;
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
    var param_27: f32;

    let _e115 = unnamed.packed_indices[0i][3u];
    let _e121 = unnamed.packed_indices[0i][3u];
    let _e126 = fog_tex_coord_1;
    let _e127 = textureSample(wired_bindless_images[(_e115 & 4095u)], wired_bindless_samplers[((_e121 >> bitcast<u32>(12i)) & 255u)], _e126);
    fog = _e127;
    let _e128 = frag_color0In_1;
    param_1 = _e128.xyz;
    let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e132 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e130.x, _e130.y, _e130.z, _e132);
    let _e137 = frag_color1In_1;
    param_2 = _e137.xyz;
    let _e139 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e141 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e139.x, _e139.y, _e139.z, _e141);
    param_3 = 0u;
    let _e146 = frag_tex_coord0_1;
    param_4 = _e146;
    param_5 = 0i;
    let _e147 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e148 = frag_color0_;
    color0_ = (_e147 * _e148);
    if override_type_4_2 {
        param_6 = 1u;
        let _e150 = frag_tex_coord1_1;
        param_7 = _e150;
        param_8 = 1i;
        let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e152 = frag_color1_;
        color1_ = (_e151 * _e152);
        let _e154 = color0_;
        let _e156 = color1_;
        let _e158 = (_e154.xyz + _e156.xyz);
        let _e160 = color0_[3u];
        let _e162 = color1_[3u];
        base = vec4<f32>(_e158.x, _e158.y, _e158.z, (_e160 * _e162));
    } else {
        if override_type_4_3 {
            param_9 = 1u;
            let _e168 = frag_tex_coord1_1;
            param_10 = _e168;
            param_11 = 1i;
            let _e169 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e170 = frag_color1_;
            color1_1 = (_e169 * _e170);
            let _e173 = color0_[3u];
            let _e174 = color0_;
            color0_ = (_e174 * _e173);
            let _e177 = color1_1[3u];
            let _e178 = color1_1;
            color1_1 = (_e178 * _e177);
            let _e180 = color0_;
            let _e182 = color1_1;
            let _e184 = (_e180.xyz + _e182.xyz);
            let _e186 = color0_[3u];
            let _e188 = color1_1[3u];
            base = vec4<f32>(_e184.x, _e184.y, _e184.z, (_e186 * _e188));
        } else {
            if override_type_4_4 {
                param_12 = 1u;
                let _e194 = frag_tex_coord1_1;
                param_13 = _e194;
                param_14 = 1i;
                let _e195 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e196 = frag_color1_;
                color1_2 = (_e195 * _e196);
                let _e199 = color0_[3u];
                let _e201 = color0_;
                color0_ = (_e201 * (1f - _e199));
                let _e204 = color1_2[3u];
                let _e206 = color1_2;
                color1_2 = (_e206 * (1f - _e204));
                let _e208 = color0_;
                let _e210 = color1_2;
                let _e212 = (_e208.xyz + _e210.xyz);
                let _e214 = color0_[3u];
                let _e216 = color1_2[3u];
                base = vec4<f32>(_e212.x, _e212.y, _e212.z, (_e214 * _e216));
            } else {
                if override_type_4_5 {
                    param_15 = 1u;
                    let _e222 = frag_tex_coord1_1;
                    param_16 = _e222;
                    param_17 = 1i;
                    let _e223 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e224 = frag_color1_;
                    color1_3 = (_e223 * _e224);
                    let _e226 = color0_;
                    let _e227 = color1_3;
                    let _e229 = color1_3[3u];
                    base = mix(_e226, _e227, vec4(_e229));
                } else {
                    if override_type_4_6 {
                        param_18 = 1u;
                        let _e232 = frag_tex_coord1_1;
                        param_19 = _e232;
                        param_20 = 1i;
                        let _e233 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e234 = frag_color1_;
                        color1_4 = (_e233 * _e234);
                        let _e236 = color1_4;
                        let _e237 = color0_;
                        let _e239 = color1_4[3u];
                        base = mix(_e236, _e237, vec4(_e239));
                    } else {
                        if override_type_4_7 {
                            param_21 = 1u;
                            let _e242 = frag_tex_coord1_1;
                            param_22 = _e242;
                            param_23 = 1i;
                            let _e243 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e244 = frag_color1_;
                            color1_5 = (_e243 * _e244);
                            let _e246 = color1_5;
                            let _e248 = color1_5[3u];
                            let _e251 = color0_;
                            base = ((_e246 + vec4(_e248)) * _e251);
                        } else {
                            param_24 = 1u;
                            let _e253 = frag_tex_coord1_1;
                            param_25 = _e253;
                            param_26 = 1i;
                            let _e254 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e255 = frag_color1_;
                            color1_6 = (_e254 * _e255);
                            let _e257 = color0_;
                            let _e259 = color1_6;
                            let _e261 = (_e257.xyz * _e259.xyz);
                            base[0u] = _e261.x;
                            base[1u] = _e261.y;
                            base[2u] = _e261.z;
                            let _e269 = color0_[3u];
                            let _e271 = color1_6[3u];
                            base[3u] = (_e269 * _e271);
                        }
                    }
                }
            }
        }
    }
    let _e274 = wired_advanced_fog_enabled_u0028_();
    if _e274 {
        let _e275 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e275;
        if override_type_4_8 {
            let _e276 = fogAmount;
            let _e278 = base;
            let _e280 = (_e278.xyz * (1f - _e276));
            base[0u] = _e280.x;
            base[1u] = _e280.y;
            base[2u] = _e280.z;
        } else {
            if override_type_4_9 {
                let _e287 = fogAmount;
                let _e289 = base;
                base = (_e289 * (1f - _e287));
            } else {
                if override_type_4_10 {
                    let _e291 = fogAmount;
                    let _e294 = base[3u];
                    base[3u] = (_e294 * (1f - _e291));
                } else {
                    let _e297 = base;
                    let _e300 = unnamed.advancedFogColorDensity;
                    let _e302 = fogAmount;
                    let _e304 = mix(_e297.xyz, _e300.xyz, vec3(_e302));
                    base[0u] = _e304.x;
                    base[1u] = _e304.y;
                    base[2u] = _e304.z;
                }
            }
        }
    } else {
        if override_type_4_11 {
            let _e311 = base;
            let _e314 = fog[3u];
            let _e316 = (_e311.xyz * (1f - _e314));
            base[0u] = _e316.x;
            base[1u] = _e316.y;
            base[2u] = _e316.z;
        } else {
            if override_type_4_12 {
                let _e323 = base;
                let _e325 = fog[3u];
                base = (_e323 * (1f - _e325));
            } else {
                if override_type_4_13 {
                    let _e329 = base[3u];
                    let _e331 = fog[3u];
                    base[3u] = (_e329 * (1f - _e331));
                } else {
                    let _e335 = base;
                    let _e336 = fog;
                    let _e338 = unnamed.fogColor;
                    let _e341 = fog[3u];
                    base = mix(_e335, (_e336 * _e338), vec4(_e341));
                }
            }
        }
    }
    if override_type_4_14 {
        let _e345 = base[3u];
        if (_e345 == 0f) {
            discard;
        }
    } else {
        if override_type_4_15 {
            let _e347 = base;
            let _e349 = base;
            if (dot(_e347.xyz, _e349.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e353 = base;
    out_color = _e353;
    param_27 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_27));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
