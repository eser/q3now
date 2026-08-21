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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e50 = (*value);
    let _e51 = (*value);
    let _e53 = all((_e50 == _e51));
    phi_66_ = _e53;
    if _e53 {
        let _e54 = (*value);
        phi_66_ = all((abs(_e54) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e59 = phi_66_;
    return _e59;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e50 = (*value_1);
    let _e51 = (*value_1);
    let _e53 = all((_e50 == _e51));
    phi_51_ = _e53;
    if _e53 {
        let _e54 = (*value_1);
        phi_51_ = all((abs(_e54) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e59 = phi_51_;
    return _e59;
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
    let _e59 = temporalOutcome_1;
    let _e60 = (_e59 != 1u);
    phi_89_ = _e60;
    if !(_e60) {
        let _e62 = temporalCurrentClip_1;
        param = _e62;
        let _e63 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e63);
    }
    let _e66 = phi_89_;
    phi_98_ = _e66;
    if !(_e66) {
        let _e68 = temporalPreviousClip_1;
        param_1 = _e68;
        let _e69 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e69);
    }
    let _e72 = phi_98_;
    phi_108_ = _e72;
    if !(_e72) {
        let _e75 = temporalCurrentClip_1[3u];
        phi_108_ = (_e75 <= 0.000001f);
    }
    let _e78 = phi_108_;
    phi_115_ = _e78;
    if !(_e78) {
        let _e81 = temporalPreviousClip_1[3u];
        phi_115_ = (_e81 <= 0.000001f);
    }
    let _e84 = phi_115_;
    if _e84 {
        return;
    }
    let _e85 = temporalCurrentClip_1;
    let _e88 = temporalCurrentClip_1[3u];
    currentNdc = (_e85.xy / vec2(_e88));
    let _e91 = temporalPreviousClip_1;
    let _e94 = temporalPreviousClip_1[3u];
    previousNdc = (_e91.xy / vec2(_e94));
    let _e97 = currentNdc;
    param_2 = _e97;
    let _e98 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e99 = !(_e98);
    phi_144_ = _e99;
    if !(_e99) {
        let _e101 = previousNdc;
        param_3 = _e101;
        let _e102 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e102);
    }
    let _e105 = phi_144_;
    if _e105 {
        return;
    }
    let _e106 = currentNdc;
    currentUv = ((_e106 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e109 = previousNdc;
    previousUv = ((_e109 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e112 = currentUv;
    let _e113 = previousUv;
    velocity = (_e112 - _e113);
    let _e115 = velocity;
    param_4 = _e115;
    let _e116 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e116) {
        return;
    }
    let _e118 = velocity;
    out_temporal_velocity = _e118;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e53 = (*c);
    (*c) = max(_e53, vec3<f32>(0f, 0f, 0f));
    let _e55 = (*c);
    cutoff = (_e55 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e57 = (*c);
    lo = (_e57 / vec3(12.92f));
    let _e60 = (*c);
    hi = pow(((_e60 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e65 = hi;
    let _e66 = lo;
    let _e67 = cutoff;
    return mix(_e65, _e66, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e67));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e54 = (*role);
    let _e56 = (*role);
    let _e61 = unnamed.packed_indices[(_e54 / 4u)][(_e56 % 4u)];
    let _e64 = (*role);
    let _e66 = (*role);
    let _e71 = unnamed.packed_indices[(_e64 / 4u)][(_e66 % 4u)];
    let _e76 = (*uv);
    let _e77 = textureSample(wired_bindless_images[(_e61 & 4095u)], wired_bindless_samplers[((_e71 >> bitcast<u32>(12i)) & 255u)], _e76);
    c_1 = _e77;
    let _e78 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e78))) == 0i) {
        let _e83 = c_1;
        param_5 = _e83.xyz;
        let _e85 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e85.x;
        c_1[1u] = _e85.y;
        c_1[2u] = _e85.z;
    }
    let _e92 = (*slot);
    if (lightmap_slot == (_e92 + 1i)) {
        let _e97 = unnamed.worldLightParams[0u];
        let _e98 = c_1;
        let _e100 = (_e98.xyz * _e97);
        c_1[0u] = _e100.x;
        c_1[1u] = _e100.y;
        c_1[2u] = _e100.z;
    }
    let _e107 = c_1;
    return _e107;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;

    param_6 = 0u;
    let _e54 = frag_tex_coord0_1;
    param_7 = _e54;
    param_8 = 0i;
    let _e55 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    color0_ = _e55;
    let _e56 = color0_;
    base = _e56;
    let _e57 = color0_;
    base = _e57;
    if override_type_3_ {
        let _e59 = base[3u];
        if (_e59 == 0f) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e61 = base;
            let _e63 = base;
            if (dot(_e61.xyz, _e63.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e67 = base;
    out_color = _e67;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e11 = out_temporal_velocity;
    let _e12 = out_temporal_validity;
    let _e13 = out_color;
    return FragmentOutput(_e11, _e12, _e13);
}
