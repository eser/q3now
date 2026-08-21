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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e52 = (*value);
    let _e53 = (*value);
    let _e55 = all((_e52 == _e53));
    phi_66_ = _e55;
    if _e55 {
        let _e56 = (*value);
        phi_66_ = all((abs(_e56) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e61 = phi_66_;
    return _e61;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e52 = (*value_1);
    let _e53 = (*value_1);
    let _e55 = all((_e52 == _e53));
    phi_51_ = _e55;
    if _e55 {
        let _e56 = (*value_1);
        phi_51_ = all((abs(_e56) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e61 = phi_51_;
    return _e61;
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
    let _e61 = temporalOutcome_1;
    let _e62 = (_e61 != 1u);
    phi_89_ = _e62;
    if !(_e62) {
        let _e64 = temporalCurrentClip_1;
        param = _e64;
        let _e65 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e65);
    }
    let _e68 = phi_89_;
    phi_98_ = _e68;
    if !(_e68) {
        let _e70 = temporalPreviousClip_1;
        param_1 = _e70;
        let _e71 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e71);
    }
    let _e74 = phi_98_;
    phi_108_ = _e74;
    if !(_e74) {
        let _e77 = temporalCurrentClip_1[3u];
        phi_108_ = (_e77 <= 0.000001f);
    }
    let _e80 = phi_108_;
    phi_115_ = _e80;
    if !(_e80) {
        let _e83 = temporalPreviousClip_1[3u];
        phi_115_ = (_e83 <= 0.000001f);
    }
    let _e86 = phi_115_;
    if _e86 {
        return;
    }
    let _e87 = temporalCurrentClip_1;
    let _e90 = temporalCurrentClip_1[3u];
    currentNdc = (_e87.xy / vec2(_e90));
    let _e93 = temporalPreviousClip_1;
    let _e96 = temporalPreviousClip_1[3u];
    previousNdc = (_e93.xy / vec2(_e96));
    let _e99 = currentNdc;
    param_2 = _e99;
    let _e100 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e101 = !(_e100);
    phi_144_ = _e101;
    if !(_e101) {
        let _e103 = previousNdc;
        param_3 = _e103;
        let _e104 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e104);
    }
    let _e107 = phi_144_;
    if _e107 {
        return;
    }
    let _e108 = currentNdc;
    currentUv = ((_e108 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e111 = previousNdc;
    previousUv = ((_e111 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e114 = currentUv;
    let _e115 = previousUv;
    velocity = (_e114 - _e115);
    let _e117 = velocity;
    param_4 = _e117;
    let _e118 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e118) {
        return;
    }
    let _e120 = velocity;
    out_temporal_velocity = _e120;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e55 = (*c);
    (*c) = max(_e55, vec3<f32>(0f, 0f, 0f));
    let _e57 = (*c);
    cutoff = (_e57 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e59 = (*c);
    lo = (_e59 / vec3(12.92f));
    let _e62 = (*c);
    hi = pow(((_e62 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e67 = hi;
    let _e68 = lo;
    let _e69 = cutoff;
    return mix(_e67, _e68, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e69));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e56 = (*role);
    let _e58 = (*role);
    let _e63 = unnamed.packed_indices[(_e56 / 4u)][(_e58 % 4u)];
    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e78 = (*uv);
    let _e79 = textureSample(wired_bindless_images[(_e63 & 4095u)], wired_bindless_samplers[((_e73 >> bitcast<u32>(12i)) & 255u)], _e78);
    c_1 = _e79;
    let _e80 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e80))) == 0i) {
        let _e85 = c_1;
        param_5 = _e85.xyz;
        let _e87 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e87.x;
        c_1[1u] = _e87.y;
        c_1[2u] = _e87.z;
    }
    let _e94 = (*slot);
    if (lightmap_slot == (_e94 + 1i)) {
        let _e99 = unnamed.worldLightParams[0u];
        let _e100 = c_1;
        let _e102 = (_e100.xyz * _e99);
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = c_1;
    return _e109;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e61 = frag_tex_coord0_1;
    param_7 = _e61;
    param_8 = 0i;
    let _e62 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e63 = frag_color;
    color0_ = (_e62 * _e63);
    let _e65 = color0_;
    base = _e65;
    let _e66 = color0_;
    base = _e66;
    if override_type_3_ {
        let _e68 = base[3u];
        if (_e68 == 0f) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e70 = base;
            let _e72 = base;
            if (dot(_e70.xyz, _e72.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e76 = base;
    out_color = _e76;
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
