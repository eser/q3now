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
@id(10) override acff: i32 = 0i;
override override_type_3_: bool = (acff == 1i);
override override_type_3_1: bool = (acff == 2i);
override override_type_3_2: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e59 = (*value);
    let _e60 = (*value);
    let _e62 = all((_e59 == _e60));
    phi_66_ = _e62;
    if _e62 {
        let _e63 = (*value);
        phi_66_ = all((abs(_e63) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e68 = phi_66_;
    return _e68;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e59 = (*value_1);
    let _e60 = (*value_1);
    let _e62 = all((_e59 == _e60));
    phi_51_ = _e62;
    if _e62 {
        let _e63 = (*value_1);
        phi_51_ = all((abs(_e63) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e68 = phi_51_;
    return _e68;
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
    let _e68 = temporalOutcome_1;
    let _e69 = (_e68 != 1u);
    phi_89_ = _e69;
    if !(_e69) {
        let _e71 = temporalCurrentClip_1;
        param = _e71;
        let _e72 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e72);
    }
    let _e75 = phi_89_;
    phi_98_ = _e75;
    if !(_e75) {
        let _e77 = temporalPreviousClip_1;
        param_1 = _e77;
        let _e78 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e78);
    }
    let _e81 = phi_98_;
    phi_108_ = _e81;
    if !(_e81) {
        let _e84 = temporalCurrentClip_1[3u];
        phi_108_ = (_e84 <= 0.000001f);
    }
    let _e87 = phi_108_;
    phi_115_ = _e87;
    if !(_e87) {
        let _e90 = temporalPreviousClip_1[3u];
        phi_115_ = (_e90 <= 0.000001f);
    }
    let _e93 = phi_115_;
    if _e93 {
        return;
    }
    let _e94 = temporalCurrentClip_1;
    let _e97 = temporalCurrentClip_1[3u];
    currentNdc = (_e94.xy / vec2(_e97));
    let _e100 = temporalPreviousClip_1;
    let _e103 = temporalPreviousClip_1[3u];
    previousNdc = (_e100.xy / vec2(_e103));
    let _e106 = currentNdc;
    param_2 = _e106;
    let _e107 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e108 = !(_e107);
    phi_144_ = _e108;
    if !(_e108) {
        let _e110 = previousNdc;
        param_3 = _e110;
        let _e111 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e111);
    }
    let _e114 = phi_144_;
    if _e114 {
        return;
    }
    let _e115 = currentNdc;
    currentUv = ((_e115 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e118 = previousNdc;
    previousUv = ((_e118 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e121 = currentUv;
    let _e122 = previousUv;
    velocity = (_e121 - _e122);
    let _e124 = velocity;
    param_4 = _e124;
    let _e125 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e125) {
        return;
    }
    let _e127 = velocity;
    out_temporal_velocity = _e127;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c);
    (*c) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_1 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_1;
        param_5 = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_1;
        let _e109 = (_e107.xyz * _e106);
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;

    let _e68 = unnamed.packed_indices[0i][3u];
    let _e74 = unnamed.packed_indices[0i][3u];
    let _e79 = fog_tex_coord_1;
    let _e80 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e74 >> bitcast<u32>(12i)) & 255u)], _e79);
    fog = _e80;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e85 = frag_tex_coord0_1;
    param_7 = _e85;
    param_8 = 0i;
    let _e86 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e87 = frag_color;
    color0_ = (_e86 * _e87);
    let _e89 = color0_;
    base = _e89;
    let _e90 = color0_;
    base = _e90;
    if override_type_3_ {
        let _e91 = base;
        let _e94 = fog[3u];
        let _e96 = (_e91.xyz * (1f - _e94));
        base[0u] = _e96.x;
        base[1u] = _e96.y;
        base[2u] = _e96.z;
    } else {
        if override_type_3_1 {
            let _e103 = base;
            let _e105 = fog[3u];
            base = (_e103 * (1f - _e105));
        } else {
            if override_type_3_2 {
                let _e109 = base[3u];
                let _e111 = fog[3u];
                base[3u] = (_e109 * (1f - _e111));
            } else {
                let _e115 = base;
                let _e116 = fog;
                let _e118 = unnamed.fogColor;
                let _e121 = fog[3u];
                base = mix(_e115, (_e116 * _e118), vec4(_e121));
            }
        }
    }
    if override_type_3_3 {
        let _e125 = base[3u];
        if (_e125 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e127 = base;
            let _e129 = base;
            if (dot(_e127.xyz, _e129.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e133 = base;
    out_color = _e133;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
