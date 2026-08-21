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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e67 = (*value);
    let _e68 = (*value);
    let _e70 = all((_e67 == _e68));
    phi_66_ = _e70;
    if _e70 {
        let _e71 = (*value);
        phi_66_ = all((abs(_e71) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e76 = phi_66_;
    return _e76;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e67 = (*value_1);
    let _e68 = (*value_1);
    let _e70 = all((_e67 == _e68));
    phi_51_ = _e70;
    if _e70 {
        let _e71 = (*value_1);
        phi_51_ = all((abs(_e71) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e76 = phi_51_;
    return _e76;
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
    let _e76 = temporalOutcome_1;
    let _e77 = (_e76 != 1u);
    phi_89_ = _e77;
    if !(_e77) {
        let _e79 = temporalCurrentClip_1;
        param = _e79;
        let _e80 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e80);
    }
    let _e83 = phi_89_;
    phi_98_ = _e83;
    if !(_e83) {
        let _e85 = temporalPreviousClip_1;
        param_1 = _e85;
        let _e86 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e86);
    }
    let _e89 = phi_98_;
    phi_108_ = _e89;
    if !(_e89) {
        let _e92 = temporalCurrentClip_1[3u];
        phi_108_ = (_e92 <= 0.000001f);
    }
    let _e95 = phi_108_;
    phi_115_ = _e95;
    if !(_e95) {
        let _e98 = temporalPreviousClip_1[3u];
        phi_115_ = (_e98 <= 0.000001f);
    }
    let _e101 = phi_115_;
    if _e101 {
        return;
    }
    let _e102 = temporalCurrentClip_1;
    let _e105 = temporalCurrentClip_1[3u];
    currentNdc = (_e102.xy / vec2(_e105));
    let _e108 = temporalPreviousClip_1;
    let _e111 = temporalPreviousClip_1[3u];
    previousNdc = (_e108.xy / vec2(_e111));
    let _e114 = currentNdc;
    param_2 = _e114;
    let _e115 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e116 = !(_e115);
    phi_144_ = _e116;
    if !(_e116) {
        let _e118 = previousNdc;
        param_3 = _e118;
        let _e119 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e119);
    }
    let _e122 = phi_144_;
    if _e122 {
        return;
    }
    let _e123 = currentNdc;
    currentUv = ((_e123 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e126 = previousNdc;
    previousUv = ((_e126 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e129 = currentUv;
    let _e130 = previousUv;
    velocity = (_e129 - _e130);
    let _e132 = velocity;
    param_4 = _e132;
    let _e133 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e133) {
        return;
    }
    let _e135 = velocity;
    out_temporal_velocity = _e135;
    out_temporal_validity = 1f;
    return;
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
    var param_5: vec3<f32>;

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
        param_5 = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param_5));
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

    let _e103 = frag_color0In_1;
    param_6 = _e103.xyz;
    let _e105 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e107 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e105.x, _e105.y, _e105.z, _e107);
    let _e112 = frag_color1In_1;
    param_7 = _e112.xyz;
    let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e116 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e114.x, _e114.y, _e114.z, _e116);
    param_8 = 0u;
    let _e121 = frag_tex_coord0_1;
    param_9 = _e121;
    param_10 = 0i;
    let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    let _e123 = frag_color0_;
    color0_ = (_e122 * _e123);
    if override_type_3_2 {
        param_11 = 1u;
        let _e125 = frag_tex_coord1_1;
        param_12 = _e125;
        param_13 = 1i;
        let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
        let _e127 = frag_color1_;
        color1_ = (_e126 * _e127);
        let _e129 = color0_;
        let _e131 = color1_;
        let _e133 = (_e129.xyz + _e131.xyz);
        let _e135 = color0_[3u];
        let _e137 = color1_[3u];
        base = vec4<f32>(_e133.x, _e133.y, _e133.z, (_e135 * _e137));
    } else {
        if override_type_3_3 {
            param_14 = 1u;
            let _e143 = frag_tex_coord1_1;
            param_15 = _e143;
            param_16 = 1i;
            let _e144 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e145 = frag_color1_;
            color1_1 = (_e144 * _e145);
            let _e148 = color0_[3u];
            let _e149 = color0_;
            color0_ = (_e149 * _e148);
            let _e152 = color1_1[3u];
            let _e153 = color1_1;
            color1_1 = (_e153 * _e152);
            let _e155 = color0_;
            let _e157 = color1_1;
            let _e159 = (_e155.xyz + _e157.xyz);
            let _e161 = color0_[3u];
            let _e163 = color1_1[3u];
            base = vec4<f32>(_e159.x, _e159.y, _e159.z, (_e161 * _e163));
        } else {
            if override_type_3_4 {
                param_17 = 1u;
                let _e169 = frag_tex_coord1_1;
                param_18 = _e169;
                param_19 = 1i;
                let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
                let _e171 = frag_color1_;
                color1_2 = (_e170 * _e171);
                let _e174 = color0_[3u];
                let _e176 = color0_;
                color0_ = (_e176 * (1f - _e174));
                let _e179 = color1_2[3u];
                let _e181 = color1_2;
                color1_2 = (_e181 * (1f - _e179));
                let _e183 = color0_;
                let _e185 = color1_2;
                let _e187 = (_e183.xyz + _e185.xyz);
                let _e189 = color0_[3u];
                let _e191 = color1_2[3u];
                base = vec4<f32>(_e187.x, _e187.y, _e187.z, (_e189 * _e191));
            } else {
                if override_type_3_5 {
                    param_20 = 1u;
                    let _e197 = frag_tex_coord1_1;
                    param_21 = _e197;
                    param_22 = 1i;
                    let _e198 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
                    let _e199 = frag_color1_;
                    color1_3 = (_e198 * _e199);
                    let _e201 = color0_;
                    let _e202 = color1_3;
                    let _e204 = color1_3[3u];
                    base = mix(_e201, _e202, vec4(_e204));
                } else {
                    if override_type_3_6 {
                        param_23 = 1u;
                        let _e207 = frag_tex_coord1_1;
                        param_24 = _e207;
                        param_25 = 1i;
                        let _e208 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                        let _e209 = frag_color1_;
                        color1_4 = (_e208 * _e209);
                        let _e211 = color1_4;
                        let _e212 = color0_;
                        let _e214 = color1_4[3u];
                        base = mix(_e211, _e212, vec4(_e214));
                    } else {
                        if override_type_3_7 {
                            param_26 = 1u;
                            let _e217 = frag_tex_coord1_1;
                            param_27 = _e217;
                            param_28 = 1i;
                            let _e218 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                            let _e219 = frag_color1_;
                            color1_5 = (_e218 * _e219);
                            let _e221 = color1_5;
                            let _e223 = color1_5[3u];
                            let _e226 = color0_;
                            base = ((_e221 + vec4(_e223)) * _e226);
                        } else {
                            param_29 = 1u;
                            let _e228 = frag_tex_coord1_1;
                            param_30 = _e228;
                            param_31 = 1i;
                            let _e229 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                            let _e230 = frag_color1_;
                            color1_6 = (_e229 * _e230);
                            let _e232 = color0_;
                            let _e234 = color1_6;
                            let _e236 = (_e232.xyz * _e234.xyz);
                            base[0u] = _e236.x;
                            base[1u] = _e236.y;
                            base[2u] = _e236.z;
                            let _e244 = color0_[3u];
                            let _e246 = color1_6[3u];
                            base[3u] = (_e244 * _e246);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e250 = base[3u];
        if (_e250 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e252 = base;
            let _e254 = base;
            if (dot(_e252.xyz, _e254.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e258 = base;
    out_color = _e258;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
