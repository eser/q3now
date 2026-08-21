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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_: bool = (discard_mode == 1i);
override override_type_3_1: bool = (discard_mode == 2i);
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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e51 = (*value);
    let _e52 = (*value);
    let _e54 = all((_e51 == _e52));
    phi_66_ = _e54;
    if _e54 {
        let _e55 = (*value);
        phi_66_ = all((abs(_e55) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e60 = phi_66_;
    return _e60;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e51 = (*value_1);
    let _e52 = (*value_1);
    let _e54 = all((_e51 == _e52));
    phi_51_ = _e54;
    if _e54 {
        let _e55 = (*value_1);
        phi_51_ = all((abs(_e55) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e60 = phi_51_;
    return _e60;
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
    let _e60 = temporalOutcome_1;
    let _e61 = (_e60 != 1u);
    phi_89_ = _e61;
    if !(_e61) {
        let _e63 = temporalCurrentClip_1;
        param = _e63;
        let _e64 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e64);
    }
    let _e67 = phi_89_;
    phi_98_ = _e67;
    if !(_e67) {
        let _e69 = temporalPreviousClip_1;
        param_1 = _e69;
        let _e70 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e70);
    }
    let _e73 = phi_98_;
    phi_108_ = _e73;
    if !(_e73) {
        let _e76 = temporalCurrentClip_1[3u];
        phi_108_ = (_e76 <= 0.000001f);
    }
    let _e79 = phi_108_;
    phi_115_ = _e79;
    if !(_e79) {
        let _e82 = temporalPreviousClip_1[3u];
        phi_115_ = (_e82 <= 0.000001f);
    }
    let _e85 = phi_115_;
    if _e85 {
        return;
    }
    let _e86 = temporalCurrentClip_1;
    let _e89 = temporalCurrentClip_1[3u];
    currentNdc = (_e86.xy / vec2(_e89));
    let _e92 = temporalPreviousClip_1;
    let _e95 = temporalPreviousClip_1[3u];
    previousNdc = (_e92.xy / vec2(_e95));
    let _e98 = currentNdc;
    param_2 = _e98;
    let _e99 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e100 = !(_e99);
    phi_144_ = _e100;
    if !(_e100) {
        let _e102 = previousNdc;
        param_3 = _e102;
        let _e103 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e103);
    }
    let _e106 = phi_144_;
    if _e106 {
        return;
    }
    let _e107 = currentNdc;
    currentUv = ((_e107 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e110 = previousNdc;
    previousUv = ((_e110 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e113 = currentUv;
    let _e114 = previousUv;
    velocity = (_e113 - _e114);
    let _e116 = velocity;
    param_4 = _e116;
    let _e117 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e117) {
        return;
    }
    let _e119 = velocity;
    out_temporal_velocity = _e119;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e54 = (*c);
    (*c) = max(_e54, vec3<f32>(0f, 0f, 0f));
    let _e56 = (*c);
    cutoff = (_e56 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e58 = (*c);
    lo = (_e58 / vec3(12.92f));
    let _e61 = (*c);
    hi = pow(((_e61 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e66 = hi;
    let _e67 = lo;
    let _e68 = cutoff;
    return mix(_e66, _e67, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e68));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e55 = (*role);
    let _e57 = (*role);
    let _e62 = unnamed.packed_indices[(_e55 / 4u)][(_e57 % 4u)];
    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e77 = (*uv);
    let _e78 = textureSample(wired_bindless_images[(_e62 & 4095u)], wired_bindless_samplers[((_e72 >> bitcast<u32>(12i)) & 255u)], _e77);
    c_1 = _e78;
    let _e79 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e79))) == 0i) {
        let _e84 = c_1;
        param_5 = _e84.xyz;
        let _e86 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e86.x;
        c_1[1u] = _e86.y;
        c_1[2u] = _e86.z;
    }
    let _e93 = (*slot);
    if (lightmap_slot == (_e93 + 1i)) {
        let _e98 = unnamed.worldLightParams[0u];
        let _e99 = c_1;
        let _e101 = (_e99.xyz * _e98);
        c_1[0u] = _e101.x;
        c_1[1u] = _e101.y;
        c_1[2u] = _e101.z;
    }
    let _e108 = c_1;
    return _e108;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var base: vec4<f32>;

    let _e57 = frag_color0In_1;
    param_6 = _e57.xyz;
    let _e59 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e61 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e59.x, _e59.y, _e59.z, _e61);
    param_7 = 0u;
    let _e66 = frag_tex_coord0_1;
    param_8 = _e66;
    param_9 = 0i;
    let _e67 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e68 = frag_color0_;
    color0_ = (_e67 * _e68);
    let _e70 = color0_;
    base = _e70;
    let _e71 = color0_;
    base = _e71;
    if override_type_3_ {
        let _e73 = base[3u];
        if (_e73 == 0f) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e75 = base;
            let _e77 = base;
            if (dot(_e75.xyz, _e77.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e81 = base;
    out_color = _e81;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
