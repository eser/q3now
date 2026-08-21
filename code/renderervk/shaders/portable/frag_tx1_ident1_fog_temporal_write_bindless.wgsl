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
@id(10) override acff: i32 = 0i;
override override_type_3_2: bool = (acff == 1i);
override override_type_3_3: bool = (acff == 2i);
override override_type_3_4: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_5: bool = (discard_mode == 1i);
override override_type_3_6: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e61 = (*value);
    let _e62 = (*value);
    let _e64 = all((_e61 == _e62));
    phi_66_ = _e64;
    if _e64 {
        let _e65 = (*value);
        phi_66_ = all((abs(_e65) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e70 = phi_66_;
    return _e70;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e61 = (*value_1);
    let _e62 = (*value_1);
    let _e64 = all((_e61 == _e62));
    phi_51_ = _e64;
    if _e64 {
        let _e65 = (*value_1);
        phi_51_ = all((abs(_e65) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e70 = phi_51_;
    return _e70;
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
    let _e70 = temporalOutcome_1;
    let _e71 = (_e70 != 1u);
    phi_89_ = _e71;
    if !(_e71) {
        let _e73 = temporalCurrentClip_1;
        param = _e73;
        let _e74 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e74);
    }
    let _e77 = phi_89_;
    phi_98_ = _e77;
    if !(_e77) {
        let _e79 = temporalPreviousClip_1;
        param_1 = _e79;
        let _e80 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e80);
    }
    let _e83 = phi_98_;
    phi_108_ = _e83;
    if !(_e83) {
        let _e86 = temporalCurrentClip_1[3u];
        phi_108_ = (_e86 <= 0.000001f);
    }
    let _e89 = phi_108_;
    phi_115_ = _e89;
    if !(_e89) {
        let _e92 = temporalPreviousClip_1[3u];
        phi_115_ = (_e92 <= 0.000001f);
    }
    let _e95 = phi_115_;
    if _e95 {
        return;
    }
    let _e96 = temporalCurrentClip_1;
    let _e99 = temporalCurrentClip_1[3u];
    currentNdc = (_e96.xy / vec2(_e99));
    let _e102 = temporalPreviousClip_1;
    let _e105 = temporalPreviousClip_1[3u];
    previousNdc = (_e102.xy / vec2(_e105));
    let _e108 = currentNdc;
    param_2 = _e108;
    let _e109 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e110 = !(_e109);
    phi_144_ = _e110;
    if !(_e110) {
        let _e112 = previousNdc;
        param_3 = _e112;
        let _e113 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e113);
    }
    let _e116 = phi_144_;
    if _e116 {
        return;
    }
    let _e117 = currentNdc;
    currentUv = ((_e117 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e120 = previousNdc;
    previousUv = ((_e120 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e123 = currentUv;
    let _e124 = previousUv;
    velocity = (_e123 - _e124);
    let _e126 = velocity;
    param_4 = _e126;
    let _e127 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e127) {
        return;
    }
    let _e129 = velocity;
    out_temporal_velocity = _e129;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c);
    (*c) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e87 = (*uv);
    let _e88 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    c_1 = _e88;
    let _e89 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e89))) == 0i) {
        let _e94 = c_1;
        param_5 = _e94.xyz;
        let _e96 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e96.x;
        c_1[1u] = _e96.y;
        c_1[2u] = _e96.z;
    }
    let _e103 = (*slot);
    if (lightmap_slot == (_e103 + 1i)) {
        let _e108 = unnamed.worldLightParams[0u];
        let _e109 = c_1;
        let _e111 = (_e109.xyz * _e108);
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = c_1;
    return _e118;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e81 = unnamed.packed_indices[0i][3u];
    let _e87 = unnamed.packed_indices[0i][3u];
    let _e92 = fog_tex_coord_1;
    let _e93 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    fog = _e93;
    param_6 = 0u;
    let _e94 = frag_tex_coord0_1;
    param_7 = _e94;
    param_8 = 0i;
    let _e95 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    color0_ = _e95;
    if override_type_3_ {
        param_9 = 1u;
        let _e96 = frag_tex_coord1_1;
        param_10 = _e96;
        param_11 = 1i;
        let _e97 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e97;
        let _e98 = color0_;
        let _e100 = color1_;
        let _e102 = (_e98.xyz + _e100.xyz);
        let _e104 = color0_[3u];
        let _e106 = color1_[3u];
        base = vec4<f32>(_e102.x, _e102.y, _e102.z, (_e104 * _e106));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e112 = frag_tex_coord1_1;
            param_13 = _e112;
            param_14 = 1i;
            let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            color1_1 = _e113;
            let _e114 = color0_;
            let _e116 = color1_1;
            let _e118 = (_e114.xyz + _e116.xyz);
            let _e120 = color0_[3u];
            let _e122 = color1_1[3u];
            base = vec4<f32>(_e118.x, _e118.y, _e118.z, (_e120 * _e122));
        } else {
            param_15 = 1u;
            let _e128 = frag_tex_coord1_1;
            param_16 = _e128;
            param_17 = 1i;
            let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            color1_2 = _e129;
            let _e130 = color0_;
            let _e132 = color1_2;
            let _e134 = (_e130.xyz * _e132.xyz);
            base[0u] = _e134.x;
            base[1u] = _e134.y;
            base[2u] = _e134.z;
            let _e142 = color0_[3u];
            let _e144 = color1_2[3u];
            base[3u] = (_e142 * _e144);
        }
    }
    if override_type_3_2 {
        let _e147 = base;
        let _e150 = fog[3u];
        let _e152 = (_e147.xyz * (1f - _e150));
        base[0u] = _e152.x;
        base[1u] = _e152.y;
        base[2u] = _e152.z;
    } else {
        if override_type_3_3 {
            let _e159 = base;
            let _e161 = fog[3u];
            base = (_e159 * (1f - _e161));
        } else {
            if override_type_3_4 {
                let _e165 = base[3u];
                let _e167 = fog[3u];
                base[3u] = (_e165 * (1f - _e167));
            } else {
                let _e171 = base;
                let _e172 = fog;
                let _e174 = unnamed.fogColor;
                let _e177 = fog[3u];
                base = mix(_e171, (_e172 * _e174), vec4(_e177));
            }
        }
    }
    if override_type_3_5 {
        let _e181 = base[3u];
        if (_e181 == 0f) {
            discard;
        }
    } else {
        if override_type_3_6 {
            let _e183 = base;
            let _e185 = base;
            if (dot(_e183.xyz, _e185.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e189 = base;
    out_color = _e189;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
