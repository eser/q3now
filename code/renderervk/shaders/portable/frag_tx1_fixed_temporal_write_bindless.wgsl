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
    var frag_color: vec4<f32>;
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

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e77 = frag_tex_coord0_1;
    param_7 = _e77;
    param_8 = 0i;
    let _e78 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e79 = frag_color;
    color0_ = (_e78 * _e79);
    if override_type_3_ {
        param_9 = 1u;
        let _e81 = frag_tex_coord1_1;
        param_10 = _e81;
        param_11 = 1i;
        let _e82 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e82;
        let _e83 = color0_;
        let _e85 = color1_;
        let _e87 = (_e83.xyz + _e85.xyz);
        let _e89 = color0_[3u];
        let _e91 = color1_[3u];
        base = vec4<f32>(_e87.x, _e87.y, _e87.z, (_e89 * _e91));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e97 = frag_tex_coord1_1;
            param_13 = _e97;
            param_14 = 1i;
            let _e98 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            let _e99 = frag_color;
            color1_1 = (_e98 * _e99);
            let _e101 = color0_;
            let _e103 = color1_1;
            let _e105 = (_e101.xyz + _e103.xyz);
            let _e107 = color0_[3u];
            let _e109 = color1_1[3u];
            base = vec4<f32>(_e105.x, _e105.y, _e105.z, (_e107 * _e109));
        } else {
            param_15 = 1u;
            let _e115 = frag_tex_coord1_1;
            param_16 = _e115;
            param_17 = 1i;
            let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            let _e117 = frag_color;
            color1_2 = (_e116 * _e117);
            let _e119 = color0_;
            let _e121 = color1_2;
            let _e123 = (_e119.xyz * _e121.xyz);
            base[0u] = _e123.x;
            base[1u] = _e123.y;
            base[2u] = _e123.z;
            let _e131 = color0_[3u];
            let _e133 = color1_2[3u];
            base[3u] = (_e131 * _e133);
        }
    }
    if override_type_3_2 {
        let _e137 = base[3u];
        if (_e137 == 0f) {
            discard;
        }
    } else {
        if override_type_3_3 {
            let _e139 = base;
            let _e141 = base;
            if (dot(_e139.xyz, _e141.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e145 = base;
    out_color = _e145;
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
