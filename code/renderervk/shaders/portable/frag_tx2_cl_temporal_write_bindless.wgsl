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
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
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

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e69 = (*value);
    let _e70 = (*value);
    let _e72 = all((_e69 == _e70));
    phi_66_ = _e72;
    if _e72 {
        let _e73 = (*value);
        phi_66_ = all((abs(_e73) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_66_;
    return _e78;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e69 = (*value_1);
    let _e70 = (*value_1);
    let _e72 = all((_e69 == _e70));
    phi_51_ = _e72;
    if _e72 {
        let _e73 = (*value_1);
        phi_51_ = all((abs(_e73) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_51_;
    return _e78;
}

fn wiredTemporalWriteAux_u0028_() {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_89_: bool;
    var phi_98_: bool;
    var phi_108_: bool;
    var phi_115_: bool;
    var phi_144_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e78 = temporalOutcome_1;
    let _e79 = (_e78 != 1u);
    phi_89_ = _e79;
    if !(_e79) {
        let _e81 = temporalCurrentClip_1;
        param = _e81;
        let _e82 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e82);
    }
    let _e85 = phi_89_;
    phi_98_ = _e85;
    if !(_e85) {
        let _e87 = temporalPreviousClip_1;
        param_1 = _e87;
        let _e88 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e88);
    }
    let _e91 = phi_98_;
    phi_108_ = _e91;
    if !(_e91) {
        let _e94 = temporalCurrentClip_1[3u];
        phi_108_ = (_e94 <= 0.000001f);
    }
    let _e97 = phi_108_;
    phi_115_ = _e97;
    if !(_e97) {
        let _e100 = temporalPreviousClip_1[3u];
        phi_115_ = (_e100 <= 0.000001f);
    }
    let _e103 = phi_115_;
    if _e103 {
        return;
    }
    let _e104 = temporalCurrentClip_1;
    let _e107 = temporalCurrentClip_1[3u];
    currentNdc = (_e104.xy / vec2(_e107));
    let _e110 = temporalPreviousClip_1;
    let _e113 = temporalPreviousClip_1[3u];
    previousNdc = (_e110.xy / vec2(_e113));
    let _e116 = currentNdc;
    param_2 = _e116;
    let _e117 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e118 = !(_e117);
    phi_144_ = _e118;
    if !(_e118) {
        let _e120 = previousNdc;
        param_3 = _e120;
        let _e121 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e121);
    }
    let _e124 = phi_144_;
    if _e124 {
        return;
    }
    let _e125 = currentNdc;
    currentUv = ((_e125 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e128 = previousNdc;
    previousUv = ((_e128 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e131 = currentUv;
    let _e132 = previousUv;
    velocity = (_e131 - _e132);
    let _e134 = velocity;
    param_4 = _e134;
    let _e135 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e135) {
        return;
    }
    let _e137 = velocity;
    out_temporal_velocity = _e137;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e72 = (*c);
    (*c) = max(_e72, vec3<f32>(0f, 0f, 0f));
    let _e74 = (*c);
    cutoff = (_e74 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e76 = (*c);
    lo = (_e76 / vec3(12.92f));
    let _e79 = (*c);
    hi = pow(((_e79 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e84 = hi;
    let _e85 = lo;
    let _e86 = cutoff;
    return mix(_e84, _e85, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e86));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e95 = (*uv);
    let _e96 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    c_1 = _e96;
    let _e97 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e97))) == 0i) {
        let _e102 = c_1;
        param_5 = _e102.xyz;
        let _e104 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = (*slot);
    if (lightmap_slot == (_e111 + 1i)) {
        let _e116 = unnamed.worldLightParams[0u];
        let _e117 = c_1;
        let _e119 = (_e117.xyz * _e116);
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_8: vec3<f32>;
    var color0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color2_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color2_1: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_2: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color2_2: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color1_3: vec4<f32>;
    var param_30: u32;
    var param_31: vec2<f32>;
    var param_32: i32;
    var color2_3: vec4<f32>;
    var param_33: u32;
    var param_34: vec2<f32>;
    var param_35: i32;
    var color1_4: vec4<f32>;
    var param_36: u32;
    var param_37: vec2<f32>;
    var param_38: i32;
    var color2_4: vec4<f32>;
    var param_39: u32;
    var param_40: vec2<f32>;
    var param_41: i32;
    var color1_5: vec4<f32>;
    var param_42: u32;
    var param_43: vec2<f32>;
    var param_44: i32;
    var color2_5: vec4<f32>;
    var param_45: u32;
    var param_46: vec2<f32>;
    var param_47: i32;
    var color1_6: vec4<f32>;
    var param_48: u32;
    var param_49: vec2<f32>;
    var param_50: i32;
    var color2_6: vec4<f32>;
    var param_51: u32;
    var param_52: vec2<f32>;
    var param_53: i32;

    let _e135 = frag_color0In_1;
    param_6 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e139 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    let _e144 = frag_color1In_1;
    param_7 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e148 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e146.x, _e146.y, _e146.z, _e148);
    let _e153 = frag_color2In_1;
    param_8 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e157 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    param_9 = 0u;
    let _e162 = frag_tex_coord0_1;
    param_10 = _e162;
    param_11 = 0i;
    let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    let _e164 = frag_color0_;
    color0_ = (_e163 * _e164);
    if override_type_3_2 {
        param_12 = 1u;
        let _e166 = frag_tex_coord1_1;
        param_13 = _e166;
        param_14 = 1i;
        let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
        let _e168 = frag_color1_;
        color1_ = (_e167 * _e168);
        param_15 = 2u;
        let _e170 = frag_tex_coord2_1;
        param_16 = _e170;
        param_17 = 2i;
        let _e171 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
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
        if override_type_3_3 {
            param_18 = 1u;
            let _e194 = frag_tex_coord1_1;
            param_19 = _e194;
            param_20 = 1i;
            let _e195 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            let _e196 = frag_color1_;
            color1_1 = (_e195 * _e196);
            param_21 = 2u;
            let _e198 = frag_tex_coord2_1;
            param_22 = _e198;
            param_23 = 2i;
            let _e199 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
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
            if override_type_3_4 {
                param_24 = 1u;
                let _e234 = frag_tex_coord1_1;
                param_25 = _e234;
                param_26 = 1i;
                let _e235 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                let _e236 = frag_color1_;
                color1_2 = (_e235 * _e236);
                param_27 = 2u;
                let _e238 = frag_tex_coord2_1;
                param_28 = _e238;
                param_29 = 2i;
                let _e239 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
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
                if override_type_3_5 {
                    param_30 = 1u;
                    let _e277 = frag_tex_coord1_1;
                    param_31 = _e277;
                    param_32 = 1i;
                    let _e278 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
                    let _e279 = frag_color1_;
                    color1_3 = (_e278 * _e279);
                    param_33 = 2u;
                    let _e281 = frag_tex_coord2_1;
                    param_34 = _e281;
                    param_35 = 2i;
                    let _e282 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_33), (&param_34), (&param_35));
                    let _e283 = frag_color2_;
                    color2_3 = (_e282 * _e283);
                    let _e285 = color0_;
                    let _e286 = color1_3;
                    let _e288 = color1_3[3u];
                    let _e291 = color2_3;
                    let _e293 = color2_3[3u];
                    base = mix(mix(_e285, _e286, vec4(_e288)), _e291, vec4(_e293));
                } else {
                    if override_type_3_6 {
                        param_36 = 1u;
                        let _e296 = frag_tex_coord1_1;
                        param_37 = _e296;
                        param_38 = 1i;
                        let _e297 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_36), (&param_37), (&param_38));
                        let _e298 = frag_color1_;
                        color1_4 = (_e297 * _e298);
                        param_39 = 2u;
                        let _e300 = frag_tex_coord2_1;
                        param_40 = _e300;
                        param_41 = 2i;
                        let _e301 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_39), (&param_40), (&param_41));
                        let _e302 = frag_color2_;
                        color2_4 = (_e301 * _e302);
                        let _e304 = color2_4;
                        let _e305 = color1_4;
                        let _e306 = color0_;
                        let _e308 = color1_4[3u];
                        let _e312 = color2_4[3u];
                        base = mix(_e304, mix(_e305, _e306, vec4(_e308)), vec4(_e312));
                    } else {
                        if override_type_3_7 {
                            param_42 = 1u;
                            let _e315 = frag_tex_coord1_1;
                            param_43 = _e315;
                            param_44 = 1i;
                            let _e316 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_42), (&param_43), (&param_44));
                            let _e317 = frag_color1_;
                            color1_5 = (_e316 * _e317);
                            param_45 = 2u;
                            let _e319 = frag_tex_coord2_1;
                            param_46 = _e319;
                            param_47 = 2i;
                            let _e320 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_45), (&param_46), (&param_47));
                            let _e321 = frag_color2_;
                            color2_5 = (_e320 * _e321);
                            let _e323 = color2_5;
                            let _e325 = color2_5[3u];
                            let _e328 = color1_5;
                            let _e330 = color1_5[3u];
                            let _e334 = color0_;
                            base = (((_e323 + vec4(_e325)) * (_e328 + vec4(_e330))) * _e334);
                        } else {
                            param_48 = 1u;
                            let _e336 = frag_tex_coord1_1;
                            param_49 = _e336;
                            param_50 = 1i;
                            let _e337 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_48), (&param_49), (&param_50));
                            let _e338 = frag_color1_;
                            color1_6 = (_e337 * _e338);
                            param_51 = 2u;
                            let _e340 = frag_tex_coord2_1;
                            param_52 = _e340;
                            param_53 = 2i;
                            let _e341 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_51), (&param_52), (&param_53));
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
    if override_type_3_8 {
        let _e368 = base[3u];
        if (_e368 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e370 = base;
            let _e372 = base;
            if (dot(_e370.xyz, _e372.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e376 = base;
    out_color = _e376;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
