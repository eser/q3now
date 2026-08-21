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
@id(10) override acff: i32 = 0i;
override override_type_3_8: bool = (acff == 1i);
override override_type_3_9: bool = (acff == 2i);
override override_type_3_10: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_11: bool = (discard_mode == 1i);
override override_type_3_12: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e72 = (*value);
    let _e73 = (*value);
    let _e75 = all((_e72 == _e73));
    phi_66_ = _e75;
    if _e75 {
        let _e76 = (*value);
        phi_66_ = all((abs(_e76) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e81 = phi_66_;
    return _e81;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e72 = (*value_1);
    let _e73 = (*value_1);
    let _e75 = all((_e72 == _e73));
    phi_51_ = _e75;
    if _e75 {
        let _e76 = (*value_1);
        phi_51_ = all((abs(_e76) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e81 = phi_51_;
    return _e81;
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
    let _e81 = temporalOutcome_1;
    let _e82 = (_e81 != 1u);
    phi_89_ = _e82;
    if !(_e82) {
        let _e84 = temporalCurrentClip_1;
        param = _e84;
        let _e85 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e85);
    }
    let _e88 = phi_89_;
    phi_98_ = _e88;
    if !(_e88) {
        let _e90 = temporalPreviousClip_1;
        param_1 = _e90;
        let _e91 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e91);
    }
    let _e94 = phi_98_;
    phi_108_ = _e94;
    if !(_e94) {
        let _e97 = temporalCurrentClip_1[3u];
        phi_108_ = (_e97 <= 0.000001f);
    }
    let _e100 = phi_108_;
    phi_115_ = _e100;
    if !(_e100) {
        let _e103 = temporalPreviousClip_1[3u];
        phi_115_ = (_e103 <= 0.000001f);
    }
    let _e106 = phi_115_;
    if _e106 {
        return;
    }
    let _e107 = temporalCurrentClip_1;
    let _e110 = temporalCurrentClip_1[3u];
    currentNdc = (_e107.xy / vec2(_e110));
    let _e113 = temporalPreviousClip_1;
    let _e116 = temporalPreviousClip_1[3u];
    previousNdc = (_e113.xy / vec2(_e116));
    let _e119 = currentNdc;
    param_2 = _e119;
    let _e120 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e121 = !(_e120);
    phi_144_ = _e121;
    if !(_e121) {
        let _e123 = previousNdc;
        param_3 = _e123;
        let _e124 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e124);
    }
    let _e127 = phi_144_;
    if _e127 {
        return;
    }
    let _e128 = currentNdc;
    currentUv = ((_e128 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e131 = previousNdc;
    previousUv = ((_e131 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e134 = currentUv;
    let _e135 = previousUv;
    velocity = (_e134 - _e135);
    let _e137 = velocity;
    param_4 = _e137;
    let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e138) {
        return;
    }
    let _e140 = velocity;
    out_temporal_velocity = _e140;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e75 = (*c);
    (*c) = max(_e75, vec3<f32>(0f, 0f, 0f));
    let _e77 = (*c);
    cutoff = (_e77 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e79 = (*c);
    lo = (_e79 / vec3(12.92f));
    let _e82 = (*c);
    hi = pow(((_e82 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e87 = hi;
    let _e88 = lo;
    let _e89 = cutoff;
    return mix(_e87, _e88, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e89));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_1 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_1;
        param_5 = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e119 = unnamed.worldLightParams[0u];
        let _e120 = c_1;
        let _e122 = (_e120.xyz * _e119);
        c_1[0u] = _e122.x;
        c_1[1u] = _e122.y;
        c_1[2u] = _e122.z;
    }
    let _e129 = c_1;
    return _e129;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_7: vec3<f32>;
    var color0_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color1_3: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color1_4: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var color1_5: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color1_6: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;

    let _e112 = unnamed.packed_indices[0i][3u];
    let _e118 = unnamed.packed_indices[0i][3u];
    let _e123 = fog_tex_coord_1;
    let _e124 = textureSample(wired_bindless_images[(_e112 & 4095u)], wired_bindless_samplers[((_e118 >> bitcast<u32>(12i)) & 255u)], _e123);
    fog = _e124;
    let _e125 = frag_color0In_1;
    param_6 = _e125.xyz;
    let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e129 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e127.x, _e127.y, _e127.z, _e129);
    let _e134 = frag_color1In_1;
    param_7 = _e134.xyz;
    let _e136 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e138 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e136.x, _e136.y, _e136.z, _e138);
    param_8 = 0u;
    let _e143 = frag_tex_coord0_1;
    param_9 = _e143;
    param_10 = 0i;
    let _e144 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    let _e145 = frag_color0_;
    color0_ = (_e144 * _e145);
    if override_type_3_2 {
        param_11 = 1u;
        let _e147 = frag_tex_coord1_1;
        param_12 = _e147;
        param_13 = 1i;
        let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
        let _e149 = frag_color1_;
        color1_ = (_e148 * _e149);
        let _e151 = color0_;
        let _e153 = color1_;
        let _e155 = (_e151.xyz + _e153.xyz);
        let _e157 = color0_[3u];
        let _e159 = color1_[3u];
        base = vec4<f32>(_e155.x, _e155.y, _e155.z, (_e157 * _e159));
    } else {
        if override_type_3_3 {
            param_14 = 1u;
            let _e165 = frag_tex_coord1_1;
            param_15 = _e165;
            param_16 = 1i;
            let _e166 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e167 = frag_color1_;
            color1_1 = (_e166 * _e167);
            let _e170 = color0_[3u];
            let _e171 = color0_;
            color0_ = (_e171 * _e170);
            let _e174 = color1_1[3u];
            let _e175 = color1_1;
            color1_1 = (_e175 * _e174);
            let _e177 = color0_;
            let _e179 = color1_1;
            let _e181 = (_e177.xyz + _e179.xyz);
            let _e183 = color0_[3u];
            let _e185 = color1_1[3u];
            base = vec4<f32>(_e181.x, _e181.y, _e181.z, (_e183 * _e185));
        } else {
            if override_type_3_4 {
                param_17 = 1u;
                let _e191 = frag_tex_coord1_1;
                param_18 = _e191;
                param_19 = 1i;
                let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
                let _e193 = frag_color1_;
                color1_2 = (_e192 * _e193);
                let _e196 = color0_[3u];
                let _e198 = color0_;
                color0_ = (_e198 * (1f - _e196));
                let _e201 = color1_2[3u];
                let _e203 = color1_2;
                color1_2 = (_e203 * (1f - _e201));
                let _e205 = color0_;
                let _e207 = color1_2;
                let _e209 = (_e205.xyz + _e207.xyz);
                let _e211 = color0_[3u];
                let _e213 = color1_2[3u];
                base = vec4<f32>(_e209.x, _e209.y, _e209.z, (_e211 * _e213));
            } else {
                if override_type_3_5 {
                    param_20 = 1u;
                    let _e219 = frag_tex_coord1_1;
                    param_21 = _e219;
                    param_22 = 1i;
                    let _e220 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
                    let _e221 = frag_color1_;
                    color1_3 = (_e220 * _e221);
                    let _e223 = color0_;
                    let _e224 = color1_3;
                    let _e226 = color1_3[3u];
                    base = mix(_e223, _e224, vec4(_e226));
                } else {
                    if override_type_3_6 {
                        param_23 = 1u;
                        let _e229 = frag_tex_coord1_1;
                        param_24 = _e229;
                        param_25 = 1i;
                        let _e230 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                        let _e231 = frag_color1_;
                        color1_4 = (_e230 * _e231);
                        let _e233 = color1_4;
                        let _e234 = color0_;
                        let _e236 = color1_4[3u];
                        base = mix(_e233, _e234, vec4(_e236));
                    } else {
                        if override_type_3_7 {
                            param_26 = 1u;
                            let _e239 = frag_tex_coord1_1;
                            param_27 = _e239;
                            param_28 = 1i;
                            let _e240 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                            let _e241 = frag_color1_;
                            color1_5 = (_e240 * _e241);
                            let _e243 = color1_5;
                            let _e245 = color1_5[3u];
                            let _e248 = color0_;
                            base = ((_e243 + vec4(_e245)) * _e248);
                        } else {
                            param_29 = 1u;
                            let _e250 = frag_tex_coord1_1;
                            param_30 = _e250;
                            param_31 = 1i;
                            let _e251 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                            let _e252 = frag_color1_;
                            color1_6 = (_e251 * _e252);
                            let _e254 = color0_;
                            let _e256 = color1_6;
                            let _e258 = (_e254.xyz * _e256.xyz);
                            base[0u] = _e258.x;
                            base[1u] = _e258.y;
                            base[2u] = _e258.z;
                            let _e266 = color0_[3u];
                            let _e268 = color1_6[3u];
                            base[3u] = (_e266 * _e268);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e271 = base;
        let _e274 = fog[3u];
        let _e276 = (_e271.xyz * (1f - _e274));
        base[0u] = _e276.x;
        base[1u] = _e276.y;
        base[2u] = _e276.z;
    } else {
        if override_type_3_9 {
            let _e283 = base;
            let _e285 = fog[3u];
            base = (_e283 * (1f - _e285));
        } else {
            if override_type_3_10 {
                let _e289 = base[3u];
                let _e291 = fog[3u];
                base[3u] = (_e289 * (1f - _e291));
            } else {
                let _e295 = base;
                let _e296 = fog;
                let _e298 = unnamed.fogColor;
                let _e301 = fog[3u];
                base = mix(_e295, (_e296 * _e298), vec4(_e301));
            }
        }
    }
    if override_type_3_11 {
        let _e305 = base[3u];
        if (_e305 == 0f) {
            discard;
        }
    } else {
        if override_type_3_12 {
            let _e307 = base;
            let _e309 = base;
            if (dot(_e307.xyz, _e309.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e313 = base;
    out_color = _e313;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
