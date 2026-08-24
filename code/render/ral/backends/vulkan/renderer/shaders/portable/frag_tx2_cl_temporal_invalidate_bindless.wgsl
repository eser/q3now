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
@id(7) override discard_mode: i32 = 0i;
override override_type_4_8: bool = (discard_mode == 1i);
override override_type_4_9: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
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
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_3: vec3<f32>;
    var color0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var color1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color2_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color2_1: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color1_2: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color2_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color1_3: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var color2_3: vec4<f32>;
    var param_28: u32;
    var param_29: vec2<f32>;
    var param_30: i32;
    var color1_4: vec4<f32>;
    var param_31: u32;
    var param_32: vec2<f32>;
    var param_33: i32;
    var color2_4: vec4<f32>;
    var param_34: u32;
    var param_35: vec2<f32>;
    var param_36: i32;
    var color1_5: vec4<f32>;
    var param_37: u32;
    var param_38: vec2<f32>;
    var param_39: i32;
    var color2_5: vec4<f32>;
    var param_40: u32;
    var param_41: vec2<f32>;
    var param_42: i32;
    var color1_6: vec4<f32>;
    var param_43: u32;
    var param_44: vec2<f32>;
    var param_45: i32;
    var color2_6: vec4<f32>;
    var param_46: u32;
    var param_47: vec2<f32>;
    var param_48: i32;
    var fogAmount: f32;
    var param_49: f32;

    let _e135 = frag_color0In_1;
    param_1 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e139 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    let _e144 = frag_color1In_1;
    param_2 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e148 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e146.x, _e146.y, _e146.z, _e148);
    let _e153 = frag_color2In_1;
    param_3 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e157 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    param_4 = 0u;
    let _e162 = frag_tex_coord0_1;
    param_5 = _e162;
    param_6 = 0i;
    let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e164 = frag_color0_;
    color0_ = (_e163 * _e164);
    if override_type_4_2 {
        param_7 = 1u;
        let _e166 = frag_tex_coord1_1;
        param_8 = _e166;
        param_9 = 1i;
        let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e168 = frag_color1_;
        color1_ = (_e167 * _e168);
        param_10 = 2u;
        let _e170 = frag_tex_coord2_1;
        param_11 = _e170;
        param_12 = 2i;
        let _e171 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e172 = frag_color2_;
        color2_ = (_e171 * _e172);
        let _e174 = color0_;
        let _e176 = color1_;
        let _e179 = color2_;
        let _e181 = ((_e174.xyz + _e176.xyz) + _e179.xyz);
        let _e183 = color0_[3u];
        let _e185 = color1_[3u];
        let _e188 = color2_[3u];
        base = vec4<f32>(_e181.x, _e181.y, _e181.z, ((_e183 * _e185) * _e188));
    } else {
        if override_type_4_3 {
            param_13 = 1u;
            let _e194 = frag_tex_coord1_1;
            param_14 = _e194;
            param_15 = 1i;
            let _e195 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e196 = frag_color1_;
            color1_1 = (_e195 * _e196);
            param_16 = 2u;
            let _e198 = frag_tex_coord2_1;
            param_17 = _e198;
            param_18 = 2i;
            let _e199 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e200 = frag_color2_;
            color2_1 = (_e199 * _e200);
            let _e203 = color0_[3u];
            let _e204 = color0_;
            color0_ = (_e204 * _e203);
            let _e207 = color1_1[3u];
            let _e208 = color1_1;
            color1_1 = (_e208 * _e207);
            let _e211 = color2_1[3u];
            let _e212 = color2_1;
            color2_1 = (_e212 * _e211);
            let _e214 = color0_;
            let _e216 = color1_1;
            let _e219 = color2_1;
            let _e221 = ((_e214.xyz + _e216.xyz) + _e219.xyz);
            let _e223 = color0_[3u];
            let _e225 = color1_1[3u];
            let _e228 = color2_1[3u];
            base = vec4<f32>(_e221.x, _e221.y, _e221.z, ((_e223 * _e225) * _e228));
        } else {
            if override_type_4_4 {
                param_19 = 1u;
                let _e234 = frag_tex_coord1_1;
                param_20 = _e234;
                param_21 = 1i;
                let _e235 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e236 = frag_color1_;
                color1_2 = (_e235 * _e236);
                param_22 = 2u;
                let _e238 = frag_tex_coord2_1;
                param_23 = _e238;
                param_24 = 2i;
                let _e239 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e240 = frag_color2_;
                color2_2 = (_e239 * _e240);
                let _e243 = color0_[3u];
                let _e245 = color0_;
                color0_ = (_e245 * (1f - _e243));
                let _e248 = color1_2[3u];
                let _e250 = color1_2;
                color1_2 = (_e250 * (1f - _e248));
                let _e253 = color2_2[3u];
                let _e255 = color2_2;
                color2_2 = (_e255 * (1f - _e253));
                let _e257 = color0_;
                let _e259 = color1_2;
                let _e262 = color2_2;
                let _e264 = ((_e257.xyz + _e259.xyz) + _e262.xyz);
                let _e266 = color0_[3u];
                let _e268 = color1_2[3u];
                let _e271 = color2_2[3u];
                base = vec4<f32>(_e264.x, _e264.y, _e264.z, ((_e266 * _e268) * _e271));
            } else {
                if override_type_4_5 {
                    param_25 = 1u;
                    let _e277 = frag_tex_coord1_1;
                    param_26 = _e277;
                    param_27 = 1i;
                    let _e278 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e279 = frag_color1_;
                    color1_3 = (_e278 * _e279);
                    param_28 = 2u;
                    let _e281 = frag_tex_coord2_1;
                    param_29 = _e281;
                    param_30 = 2i;
                    let _e282 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e283 = frag_color2_;
                    color2_3 = (_e282 * _e283);
                    let _e285 = color0_;
                    let _e286 = color1_3;
                    let _e288 = color1_3[3u];
                    let _e291 = color2_3;
                    let _e293 = color2_3[3u];
                    base = mix(mix(_e285, _e286, vec4(_e288)), _e291, vec4(_e293));
                } else {
                    if override_type_4_6 {
                        param_31 = 1u;
                        let _e296 = frag_tex_coord1_1;
                        param_32 = _e296;
                        param_33 = 1i;
                        let _e297 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e298 = frag_color1_;
                        color1_4 = (_e297 * _e298);
                        param_34 = 2u;
                        let _e300 = frag_tex_coord2_1;
                        param_35 = _e300;
                        param_36 = 2i;
                        let _e301 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e302 = frag_color2_;
                        color2_4 = (_e301 * _e302);
                        let _e304 = color2_4;
                        let _e305 = color1_4;
                        let _e306 = color0_;
                        let _e308 = color1_4[3u];
                        let _e312 = color2_4[3u];
                        base = mix(_e304, mix(_e305, _e306, vec4(_e308)), vec4(_e312));
                    } else {
                        if override_type_4_7 {
                            param_37 = 1u;
                            let _e315 = frag_tex_coord1_1;
                            param_38 = _e315;
                            param_39 = 1i;
                            let _e316 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e317 = frag_color1_;
                            color1_5 = (_e316 * _e317);
                            param_40 = 2u;
                            let _e319 = frag_tex_coord2_1;
                            param_41 = _e319;
                            param_42 = 2i;
                            let _e320 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e321 = frag_color2_;
                            color2_5 = (_e320 * _e321);
                            let _e323 = color2_5;
                            let _e325 = color2_5[3u];
                            let _e328 = color1_5;
                            let _e330 = color1_5[3u];
                            let _e334 = color0_;
                            base = (((_e323 + vec4(_e325)) * (_e328 + vec4(_e330))) * _e334);
                        } else {
                            param_43 = 1u;
                            let _e336 = frag_tex_coord1_1;
                            param_44 = _e336;
                            param_45 = 1i;
                            let _e337 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e338 = frag_color1_;
                            color1_6 = (_e337 * _e338);
                            param_46 = 2u;
                            let _e340 = frag_tex_coord2_1;
                            param_47 = _e340;
                            param_48 = 2i;
                            let _e341 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e342 = frag_color2_;
                            color2_6 = (_e341 * _e342);
                            let _e344 = color0_;
                            let _e346 = color1_6;
                            let _e349 = color2_6;
                            let _e351 = ((_e344.xyz * _e346.xyz) * _e349.xyz);
                            base[0u] = _e351.x;
                            base[1u] = _e351.y;
                            base[2u] = _e351.z;
                            let _e359 = color0_[3u];
                            let _e361 = color1_6[3u];
                            let _e364 = color2_6[3u];
                            base[3u] = ((_e359 * _e361) * _e364);
                        }
                    }
                }
            }
        }
    }
    let _e367 = wired_advanced_fog_enabled_u0028_();
    if _e367 {
        let _e368 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e368;
        let _e369 = base;
        let _e372 = unnamed.advancedFogColorDensity;
        let _e374 = fogAmount;
        let _e376 = mix(_e369.xyz, _e372.xyz, vec3(_e374));
        base[0u] = _e376.x;
        base[1u] = _e376.y;
        base[2u] = _e376.z;
    }
    if override_type_4_8 {
        let _e384 = base[3u];
        if (_e384 == 0f) {
            discard;
        }
    } else {
        if override_type_4_9 {
            let _e386 = base;
            let _e388 = base;
            if (dot(_e386.xyz, _e388.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e392 = base;
    out_color = _e392;
    param_49 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_49));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
