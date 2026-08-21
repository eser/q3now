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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e62 = (*value);
    let _e63 = (*value);
    let _e65 = all((_e62 == _e63));
    phi_66_ = _e65;
    if _e65 {
        let _e66 = (*value);
        phi_66_ = all((abs(_e66) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e71 = phi_66_;
    return _e71;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e62 = (*value_1);
    let _e63 = (*value_1);
    let _e65 = all((_e62 == _e63));
    phi_51_ = _e65;
    if _e65 {
        let _e66 = (*value_1);
        phi_51_ = all((abs(_e66) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e71 = phi_51_;
    return _e71;
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
    let _e71 = temporalOutcome_1;
    let _e72 = (_e71 != 1u);
    phi_89_ = _e72;
    if !(_e72) {
        let _e74 = temporalCurrentClip_1;
        param = _e74;
        let _e75 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e75);
    }
    let _e78 = phi_89_;
    phi_98_ = _e78;
    if !(_e78) {
        let _e80 = temporalPreviousClip_1;
        param_1 = _e80;
        let _e81 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e81);
    }
    let _e84 = phi_98_;
    phi_108_ = _e84;
    if !(_e84) {
        let _e87 = temporalCurrentClip_1[3u];
        phi_108_ = (_e87 <= 0.000001f);
    }
    let _e90 = phi_108_;
    phi_115_ = _e90;
    if !(_e90) {
        let _e93 = temporalPreviousClip_1[3u];
        phi_115_ = (_e93 <= 0.000001f);
    }
    let _e96 = phi_115_;
    if _e96 {
        return;
    }
    let _e97 = temporalCurrentClip_1;
    let _e100 = temporalCurrentClip_1[3u];
    currentNdc = (_e97.xy / vec2(_e100));
    let _e103 = temporalPreviousClip_1;
    let _e106 = temporalPreviousClip_1[3u];
    previousNdc = (_e103.xy / vec2(_e106));
    let _e109 = currentNdc;
    param_2 = _e109;
    let _e110 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e111 = !(_e110);
    phi_144_ = _e111;
    if !(_e111) {
        let _e113 = previousNdc;
        param_3 = _e113;
        let _e114 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e114);
    }
    let _e117 = phi_144_;
    if _e117 {
        return;
    }
    let _e118 = currentNdc;
    currentUv = ((_e118 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e121 = previousNdc;
    previousUv = ((_e121 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e124 = currentUv;
    let _e125 = previousUv;
    velocity = (_e124 - _e125);
    let _e127 = velocity;
    param_4 = _e127;
    let _e128 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e128) {
        return;
    }
    let _e130 = velocity;
    out_temporal_velocity = _e130;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e65 = (*c);
    (*c) = max(_e65, vec3<f32>(0f, 0f, 0f));
    let _e67 = (*c);
    cutoff = (_e67 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e69 = (*c);
    lo = (_e69 / vec3(12.92f));
    let _e72 = (*c);
    hi = pow(((_e72 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e77 = hi;
    let _e78 = lo;
    let _e79 = cutoff;
    return mix(_e77, _e78, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e79));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e88 = (*uv);
    let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    c_1 = _e89;
    let _e90 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e90))) == 0i) {
        let _e95 = c_1;
        param_5 = _e95.xyz;
        let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e97.x;
        c_1[1u] = _e97.y;
        c_1[2u] = _e97.z;
    }
    let _e104 = (*slot);
    if (lightmap_slot == (_e104 + 1i)) {
        let _e109 = unnamed.worldLightParams[0u];
        let _e110 = c_1;
        let _e112 = (_e110.xyz * _e109);
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = c_1;
    return _e119;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e84 = unnamed.packed_indices[0i][3u];
    let _e90 = unnamed.packed_indices[0i][3u];
    let _e95 = fog_tex_coord_1;
    let _e96 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    fog = _e96;
    let _e97 = frag_color0In_1;
    param_6 = _e97.xyz;
    let _e99 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e101 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e99.x, _e99.y, _e99.z, _e101);
    param_7 = 0u;
    let _e106 = frag_tex_coord0_1;
    param_8 = _e106;
    param_9 = 0i;
    let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e108 = frag_color0_;
    color0_ = (_e107 * _e108);
    if override_type_3_ {
        param_10 = 1u;
        let _e110 = frag_tex_coord1_1;
        param_11 = _e110;
        param_12 = 1i;
        let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e111;
        let _e112 = color0_;
        let _e114 = color1_;
        let _e116 = (_e112.xyz + _e114.xyz);
        let _e118 = color0_[3u];
        let _e120 = color1_[3u];
        base = vec4<f32>(_e116.x, _e116.y, _e116.z, (_e118 * _e120));
    } else {
        if override_type_3_1 {
            param_13 = 1u;
            let _e126 = frag_tex_coord1_1;
            param_14 = _e126;
            param_15 = 1i;
            let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e128 = frag_color0_;
            color1_1 = (_e127 * _e128);
            let _e130 = color0_;
            let _e132 = color1_1;
            let _e134 = (_e130.xyz + _e132.xyz);
            let _e136 = color0_[3u];
            let _e138 = color1_1[3u];
            base = vec4<f32>(_e134.x, _e134.y, _e134.z, (_e136 * _e138));
        } else {
            param_16 = 1u;
            let _e144 = frag_tex_coord1_1;
            param_17 = _e144;
            param_18 = 1i;
            let _e145 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            color1_2 = _e145;
            let _e146 = color0_;
            let _e148 = color1_2;
            let _e150 = (_e146.xyz * _e148.xyz);
            base[0u] = _e150.x;
            base[1u] = _e150.y;
            base[2u] = _e150.z;
            let _e158 = color0_[3u];
            let _e160 = color1_2[3u];
            base[3u] = (_e158 * _e160);
        }
    }
    if override_type_3_2 {
        let _e163 = base;
        let _e166 = fog[3u];
        let _e168 = (_e163.xyz * (1f - _e166));
        base[0u] = _e168.x;
        base[1u] = _e168.y;
        base[2u] = _e168.z;
    } else {
        if override_type_3_3 {
            let _e175 = base;
            let _e177 = fog[3u];
            base = (_e175 * (1f - _e177));
        } else {
            if override_type_3_4 {
                let _e181 = base[3u];
                let _e183 = fog[3u];
                base[3u] = (_e181 * (1f - _e183));
            } else {
                let _e187 = base;
                let _e188 = fog;
                let _e190 = unnamed.fogColor;
                let _e193 = fog[3u];
                base = mix(_e187, (_e188 * _e190), vec4(_e193));
            }
        }
    }
    if override_type_3_5 {
        let _e197 = base[3u];
        if (_e197 == 0f) {
            discard;
        }
    } else {
        if override_type_3_6 {
            let _e199 = base;
            let _e201 = base;
            if (dot(_e199.xyz, _e201.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e205 = base;
    out_color = _e205;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
