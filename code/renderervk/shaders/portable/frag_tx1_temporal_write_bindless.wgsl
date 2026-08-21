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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e55 = (*value);
    let _e56 = (*value);
    let _e58 = all((_e55 == _e56));
    phi_66_ = _e58;
    if _e58 {
        let _e59 = (*value);
        phi_66_ = all((abs(_e59) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e64 = phi_66_;
    return _e64;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e55 = (*value_1);
    let _e56 = (*value_1);
    let _e58 = all((_e55 == _e56));
    phi_51_ = _e58;
    if _e58 {
        let _e59 = (*value_1);
        phi_51_ = all((abs(_e59) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e64 = phi_51_;
    return _e64;
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
    let _e64 = temporalOutcome_1;
    let _e65 = (_e64 != 1u);
    phi_89_ = _e65;
    if !(_e65) {
        let _e67 = temporalCurrentClip_1;
        param = _e67;
        let _e68 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e68);
    }
    let _e71 = phi_89_;
    phi_98_ = _e71;
    if !(_e71) {
        let _e73 = temporalPreviousClip_1;
        param_1 = _e73;
        let _e74 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e74);
    }
    let _e77 = phi_98_;
    phi_108_ = _e77;
    if !(_e77) {
        let _e80 = temporalCurrentClip_1[3u];
        phi_108_ = (_e80 <= 0.000001f);
    }
    let _e83 = phi_108_;
    phi_115_ = _e83;
    if !(_e83) {
        let _e86 = temporalPreviousClip_1[3u];
        phi_115_ = (_e86 <= 0.000001f);
    }
    let _e89 = phi_115_;
    if _e89 {
        return;
    }
    let _e90 = temporalCurrentClip_1;
    let _e93 = temporalCurrentClip_1[3u];
    currentNdc = (_e90.xy / vec2(_e93));
    let _e96 = temporalPreviousClip_1;
    let _e99 = temporalPreviousClip_1[3u];
    previousNdc = (_e96.xy / vec2(_e99));
    let _e102 = currentNdc;
    param_2 = _e102;
    let _e103 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e104 = !(_e103);
    phi_144_ = _e104;
    if !(_e104) {
        let _e106 = previousNdc;
        param_3 = _e106;
        let _e107 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e107);
    }
    let _e110 = phi_144_;
    if _e110 {
        return;
    }
    let _e111 = currentNdc;
    currentUv = ((_e111 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e114 = previousNdc;
    previousUv = ((_e114 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e117 = currentUv;
    let _e118 = previousUv;
    velocity = (_e117 - _e118);
    let _e120 = velocity;
    param_4 = _e120;
    let _e121 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e121) {
        return;
    }
    let _e123 = velocity;
    out_temporal_velocity = _e123;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e58 = (*c);
    (*c) = max(_e58, vec3<f32>(0f, 0f, 0f));
    let _e60 = (*c);
    cutoff = (_e60 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e62 = (*c);
    lo = (_e62 / vec3(12.92f));
    let _e65 = (*c);
    hi = pow(((_e65 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e70 = hi;
    let _e71 = lo;
    let _e72 = cutoff;
    return mix(_e70, _e71, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e72));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e59 = (*role);
    let _e61 = (*role);
    let _e66 = unnamed.packed_indices[(_e59 / 4u)][(_e61 % 4u)];
    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e81 = (*uv);
    let _e82 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e76 >> bitcast<u32>(12i)) & 255u)], _e81);
    c_1 = _e82;
    let _e83 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e83))) == 0i) {
        let _e88 = c_1;
        param_5 = _e88.xyz;
        let _e90 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e90.x;
        c_1[1u] = _e90.y;
        c_1[2u] = _e90.z;
    }
    let _e97 = (*slot);
    if (lightmap_slot == (_e97 + 1i)) {
        let _e102 = unnamed.worldLightParams[0u];
        let _e103 = c_1;
        let _e105 = (_e103.xyz * _e102);
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = c_1;
    return _e112;
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
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_2: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;

    let _e73 = frag_color0In_1;
    param_6 = _e73.xyz;
    let _e75 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e77 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e75.x, _e75.y, _e75.z, _e77);
    param_7 = 0u;
    let _e82 = frag_tex_coord0_1;
    param_8 = _e82;
    param_9 = 0i;
    let _e83 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e84 = frag_color0_;
    color0_ = (_e83 * _e84);
    if override_type_3_ {
        param_10 = 1u;
        let _e86 = frag_tex_coord1_1;
        param_11 = _e86;
        param_12 = 1i;
        let _e87 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e87;
        let _e88 = color0_;
        let _e90 = color1_;
        let _e92 = (_e88.xyz + _e90.xyz);
        let _e94 = color0_[3u];
        let _e96 = color1_[3u];
        base = vec4<f32>(_e92.x, _e92.y, _e92.z, (_e94 * _e96));
    } else {
        if override_type_3_1 {
            param_13 = 1u;
            let _e102 = frag_tex_coord1_1;
            param_14 = _e102;
            param_15 = 1i;
            let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e104 = frag_color0_;
            color1_1 = (_e103 * _e104);
            let _e106 = color0_;
            let _e108 = color1_1;
            let _e110 = (_e106.xyz + _e108.xyz);
            let _e112 = color0_[3u];
            let _e114 = color1_1[3u];
            base = vec4<f32>(_e110.x, _e110.y, _e110.z, (_e112 * _e114));
        } else {
            param_16 = 1u;
            let _e120 = frag_tex_coord1_1;
            param_17 = _e120;
            param_18 = 1i;
            let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            color1_2 = _e121;
            let _e122 = color0_;
            let _e124 = color1_2;
            let _e126 = (_e122.xyz * _e124.xyz);
            base[0u] = _e126.x;
            base[1u] = _e126.y;
            base[2u] = _e126.z;
            let _e134 = color0_[3u];
            let _e136 = color1_2[3u];
            base[3u] = (_e134 * _e136);
        }
    }
    if override_type_3_2 {
        let _e140 = base[3u];
        if (_e140 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e142 = base;
            let _e144 = base;
            if (dot(_e142.xyz, _e144.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e148 = base;
    out_color = _e148;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
