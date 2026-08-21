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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_2: bool = (discard_mode == 1i);
override override_type_3_3: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e56 = (*value);
    let _e57 = (*value);
    let _e59 = all((_e56 == _e57));
    phi_66_ = _e59;
    if _e59 {
        let _e60 = (*value);
        phi_66_ = all((abs(_e60) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e65 = phi_66_;
    return _e65;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e56 = (*value_1);
    let _e57 = (*value_1);
    let _e59 = all((_e56 == _e57));
    phi_51_ = _e59;
    if _e59 {
        let _e60 = (*value_1);
        phi_51_ = all((abs(_e60) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e65 = phi_51_;
    return _e65;
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
    let _e65 = temporalOutcome_1;
    let _e66 = (_e65 != 1u);
    phi_89_ = _e66;
    if !(_e66) {
        let _e68 = temporalCurrentClip_1;
        param = _e68;
        let _e69 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e69);
    }
    let _e72 = phi_89_;
    phi_98_ = _e72;
    if !(_e72) {
        let _e74 = temporalPreviousClip_1;
        param_1 = _e74;
        let _e75 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e75);
    }
    let _e78 = phi_98_;
    phi_108_ = _e78;
    if !(_e78) {
        let _e81 = temporalCurrentClip_1[3u];
        phi_108_ = (_e81 <= 0.000001f);
    }
    let _e84 = phi_108_;
    phi_115_ = _e84;
    if !(_e84) {
        let _e87 = temporalPreviousClip_1[3u];
        phi_115_ = (_e87 <= 0.000001f);
    }
    let _e90 = phi_115_;
    if _e90 {
        return;
    }
    let _e91 = temporalCurrentClip_1;
    let _e94 = temporalCurrentClip_1[3u];
    currentNdc = (_e91.xy / vec2(_e94));
    let _e97 = temporalPreviousClip_1;
    let _e100 = temporalPreviousClip_1[3u];
    previousNdc = (_e97.xy / vec2(_e100));
    let _e103 = currentNdc;
    param_2 = _e103;
    let _e104 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e105 = !(_e104);
    phi_144_ = _e105;
    if !(_e105) {
        let _e107 = previousNdc;
        param_3 = _e107;
        let _e108 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e108);
    }
    let _e111 = phi_144_;
    if _e111 {
        return;
    }
    let _e112 = currentNdc;
    currentUv = ((_e112 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e115 = previousNdc;
    previousUv = ((_e115 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e118 = currentUv;
    let _e119 = previousUv;
    velocity = (_e118 - _e119);
    let _e121 = velocity;
    param_4 = _e121;
    let _e122 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e122) {
        return;
    }
    let _e124 = velocity;
    out_temporal_velocity = _e124;
    out_temporal_validity = 1f;
    return;
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
    var param_5: vec3<f32>;

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
        param_5 = _e89.xyz;
        let _e91 = sRGBToLinear_u0028_vf3_u003b((&param_5));
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
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var color2_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color2_1: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color2_2: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;

    let _e86 = frag_color0In_1;
    param_6 = _e86.xyz;
    let _e88 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e90 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e88.x, _e88.y, _e88.z, _e90);
    param_7 = 0u;
    let _e95 = frag_tex_coord0_1;
    param_8 = _e95;
    param_9 = 0i;
    let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e97 = frag_color0_;
    color0_ = (_e96 * _e97);
    if override_type_3_ {
        param_10 = 1u;
        let _e99 = frag_tex_coord1_1;
        param_11 = _e99;
        param_12 = 1i;
        let _e100 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e100;
        param_13 = 2u;
        let _e101 = frag_tex_coord2_1;
        param_14 = _e101;
        param_15 = 2i;
        let _e102 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
        color2_ = _e102;
        let _e103 = color0_;
        let _e105 = color1_;
        let _e108 = color2_;
        let _e110 = ((_e103.xyz + _e105.xyz) + _e108.xyz);
        let _e112 = color0_[3u];
        let _e114 = color1_[3u];
        let _e117 = color2_[3u];
        base = vec4<f32>(_e110.x, _e110.y, _e110.z, ((_e112 * _e114) * _e117));
    } else {
        if override_type_3_1 {
            param_16 = 1u;
            let _e123 = frag_tex_coord1_1;
            param_17 = _e123;
            param_18 = 1i;
            let _e124 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e125 = frag_color0_;
            color1_1 = (_e124 * _e125);
            param_19 = 2u;
            let _e127 = frag_tex_coord2_1;
            param_20 = _e127;
            param_21 = 2i;
            let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            let _e129 = frag_color0_;
            color2_1 = (_e128 * _e129);
            let _e131 = color0_;
            let _e133 = color1_1;
            let _e136 = color2_1;
            let _e138 = ((_e131.xyz + _e133.xyz) + _e136.xyz);
            let _e140 = color0_[3u];
            let _e142 = color1_1[3u];
            let _e145 = color2_1[3u];
            base = vec4<f32>(_e138.x, _e138.y, _e138.z, ((_e140 * _e142) * _e145));
        } else {
            param_22 = 1u;
            let _e151 = frag_tex_coord1_1;
            param_23 = _e151;
            param_24 = 1i;
            let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            color1_2 = _e152;
            param_25 = 2u;
            let _e153 = frag_tex_coord2_1;
            param_26 = _e153;
            param_27 = 2i;
            let _e154 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
            color2_2 = _e154;
            let _e155 = color0_;
            let _e157 = color1_2;
            let _e160 = color2_2;
            let _e162 = ((_e155.xyz * _e157.xyz) * _e160.xyz);
            base[0u] = _e162.x;
            base[1u] = _e162.y;
            base[2u] = _e162.z;
            let _e170 = color0_[3u];
            let _e172 = color1_2[3u];
            let _e175 = color2_2[3u];
            base[3u] = ((_e170 * _e172) * _e175);
        }
    }
    if override_type_3_2 {
        let _e179 = base[3u];
        if (_e179 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e181 = base;
            let _e183 = base;
            if (dot(_e181.xyz, _e183.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e187 = base;
    out_color = _e187;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
