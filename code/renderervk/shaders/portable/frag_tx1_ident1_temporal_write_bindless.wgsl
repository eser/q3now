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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e54 = (*value);
    let _e55 = (*value);
    let _e57 = all((_e54 == _e55));
    phi_66_ = _e57;
    if _e57 {
        let _e58 = (*value);
        phi_66_ = all((abs(_e58) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e63 = phi_66_;
    return _e63;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e54 = (*value_1);
    let _e55 = (*value_1);
    let _e57 = all((_e54 == _e55));
    phi_51_ = _e57;
    if _e57 {
        let _e58 = (*value_1);
        phi_51_ = all((abs(_e58) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e63 = phi_51_;
    return _e63;
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
    let _e63 = temporalOutcome_1;
    let _e64 = (_e63 != 1u);
    phi_89_ = _e64;
    if !(_e64) {
        let _e66 = temporalCurrentClip_1;
        param = _e66;
        let _e67 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e67);
    }
    let _e70 = phi_89_;
    phi_98_ = _e70;
    if !(_e70) {
        let _e72 = temporalPreviousClip_1;
        param_1 = _e72;
        let _e73 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e73);
    }
    let _e76 = phi_98_;
    phi_108_ = _e76;
    if !(_e76) {
        let _e79 = temporalCurrentClip_1[3u];
        phi_108_ = (_e79 <= 0.000001f);
    }
    let _e82 = phi_108_;
    phi_115_ = _e82;
    if !(_e82) {
        let _e85 = temporalPreviousClip_1[3u];
        phi_115_ = (_e85 <= 0.000001f);
    }
    let _e88 = phi_115_;
    if _e88 {
        return;
    }
    let _e89 = temporalCurrentClip_1;
    let _e92 = temporalCurrentClip_1[3u];
    currentNdc = (_e89.xy / vec2(_e92));
    let _e95 = temporalPreviousClip_1;
    let _e98 = temporalPreviousClip_1[3u];
    previousNdc = (_e95.xy / vec2(_e98));
    let _e101 = currentNdc;
    param_2 = _e101;
    let _e102 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e103 = !(_e102);
    phi_144_ = _e103;
    if !(_e103) {
        let _e105 = previousNdc;
        param_3 = _e105;
        let _e106 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e106);
    }
    let _e109 = phi_144_;
    if _e109 {
        return;
    }
    let _e110 = currentNdc;
    currentUv = ((_e110 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e113 = previousNdc;
    previousUv = ((_e113 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e116 = currentUv;
    let _e117 = previousUv;
    velocity = (_e116 - _e117);
    let _e119 = velocity;
    param_4 = _e119;
    let _e120 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e120) {
        return;
    }
    let _e122 = velocity;
    out_temporal_velocity = _e122;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e57 = (*c);
    (*c) = max(_e57, vec3<f32>(0f, 0f, 0f));
    let _e59 = (*c);
    cutoff = (_e59 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e61 = (*c);
    lo = (_e61 / vec3(12.92f));
    let _e64 = (*c);
    hi = pow(((_e64 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e69 = hi;
    let _e70 = lo;
    let _e71 = cutoff;
    return mix(_e69, _e70, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e71));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e58 = (*role);
    let _e60 = (*role);
    let _e65 = unnamed.packed_indices[(_e58 / 4u)][(_e60 % 4u)];
    let _e68 = (*role);
    let _e70 = (*role);
    let _e75 = unnamed.packed_indices[(_e68 / 4u)][(_e70 % 4u)];
    let _e80 = (*uv);
    let _e81 = textureSample(wired_bindless_images[(_e65 & 4095u)], wired_bindless_samplers[((_e75 >> bitcast<u32>(12i)) & 255u)], _e80);
    c_1 = _e81;
    let _e82 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e82))) == 0i) {
        let _e87 = c_1;
        param_5 = _e87.xyz;
        let _e89 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e89.x;
        c_1[1u] = _e89.y;
        c_1[2u] = _e89.z;
    }
    let _e96 = (*slot);
    if (lightmap_slot == (_e96 + 1i)) {
        let _e101 = unnamed.worldLightParams[0u];
        let _e102 = c_1;
        let _e104 = (_e102.xyz * _e101);
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = c_1;
    return _e111;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var color1_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_2: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;

    param_6 = 0u;
    let _e70 = frag_tex_coord0_1;
    param_7 = _e70;
    param_8 = 0i;
    let _e71 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    color0_ = _e71;
    if override_type_3_ {
        param_9 = 1u;
        let _e72 = frag_tex_coord1_1;
        param_10 = _e72;
        param_11 = 1i;
        let _e73 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e73;
        let _e74 = color0_;
        let _e76 = color1_;
        let _e78 = (_e74.xyz + _e76.xyz);
        let _e80 = color0_[3u];
        let _e82 = color1_[3u];
        base = vec4<f32>(_e78.x, _e78.y, _e78.z, (_e80 * _e82));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e88 = frag_tex_coord1_1;
            param_13 = _e88;
            param_14 = 1i;
            let _e89 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            color1_1 = _e89;
            let _e90 = color0_;
            let _e92 = color1_1;
            let _e94 = (_e90.xyz + _e92.xyz);
            let _e96 = color0_[3u];
            let _e98 = color1_1[3u];
            base = vec4<f32>(_e94.x, _e94.y, _e94.z, (_e96 * _e98));
        } else {
            param_15 = 1u;
            let _e104 = frag_tex_coord1_1;
            param_16 = _e104;
            param_17 = 1i;
            let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            color1_2 = _e105;
            let _e106 = color0_;
            let _e108 = color1_2;
            let _e110 = (_e106.xyz * _e108.xyz);
            base[0u] = _e110.x;
            base[1u] = _e110.y;
            base[2u] = _e110.z;
            let _e118 = color0_[3u];
            let _e120 = color1_2[3u];
            base[3u] = (_e118 * _e120);
        }
    }
    if override_type_3_2 {
        let _e124 = base[3u];
        if (_e124 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e126 = base;
            let _e128 = base;
            if (dot(_e126.xyz, _e128.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e132 = base;
    out_color = _e132;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
