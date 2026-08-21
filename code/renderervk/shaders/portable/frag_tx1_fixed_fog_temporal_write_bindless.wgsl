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

    let _e63 = (*value);
    let _e64 = (*value);
    let _e66 = all((_e63 == _e64));
    phi_66_ = _e66;
    if _e66 {
        let _e67 = (*value);
        phi_66_ = all((abs(_e67) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e72 = phi_66_;
    return _e72;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e63 = (*value_1);
    let _e64 = (*value_1);
    let _e66 = all((_e63 == _e64));
    phi_51_ = _e66;
    if _e66 {
        let _e67 = (*value_1);
        phi_51_ = all((abs(_e67) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e72 = phi_51_;
    return _e72;
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
    let _e72 = temporalOutcome_1;
    let _e73 = (_e72 != 1u);
    phi_89_ = _e73;
    if !(_e73) {
        let _e75 = temporalCurrentClip_1;
        param = _e75;
        let _e76 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e76);
    }
    let _e79 = phi_89_;
    phi_98_ = _e79;
    if !(_e79) {
        let _e81 = temporalPreviousClip_1;
        param_1 = _e81;
        let _e82 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e82);
    }
    let _e85 = phi_98_;
    phi_108_ = _e85;
    if !(_e85) {
        let _e88 = temporalCurrentClip_1[3u];
        phi_108_ = (_e88 <= 0.000001f);
    }
    let _e91 = phi_108_;
    phi_115_ = _e91;
    if !(_e91) {
        let _e94 = temporalPreviousClip_1[3u];
        phi_115_ = (_e94 <= 0.000001f);
    }
    let _e97 = phi_115_;
    if _e97 {
        return;
    }
    let _e98 = temporalCurrentClip_1;
    let _e101 = temporalCurrentClip_1[3u];
    currentNdc = (_e98.xy / vec2(_e101));
    let _e104 = temporalPreviousClip_1;
    let _e107 = temporalPreviousClip_1[3u];
    previousNdc = (_e104.xy / vec2(_e107));
    let _e110 = currentNdc;
    param_2 = _e110;
    let _e111 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e112 = !(_e111);
    phi_144_ = _e112;
    if !(_e112) {
        let _e114 = previousNdc;
        param_3 = _e114;
        let _e115 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e115);
    }
    let _e118 = phi_144_;
    if _e118 {
        return;
    }
    let _e119 = currentNdc;
    currentUv = ((_e119 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e122 = previousNdc;
    previousUv = ((_e122 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e125 = currentUv;
    let _e126 = previousUv;
    velocity = (_e125 - _e126);
    let _e128 = velocity;
    param_4 = _e128;
    let _e129 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e129) {
        return;
    }
    let _e131 = velocity;
    out_temporal_velocity = _e131;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e66 = (*c);
    (*c) = max(_e66, vec3<f32>(0f, 0f, 0f));
    let _e68 = (*c);
    cutoff = (_e68 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e70 = (*c);
    lo = (_e70 / vec3(12.92f));
    let _e73 = (*c);
    hi = pow(((_e73 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e78 = hi;
    let _e79 = lo;
    let _e80 = cutoff;
    return mix(_e78, _e79, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e80));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e89 = (*uv);
    let _e90 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    c_1 = _e90;
    let _e91 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e91))) == 0i) {
        let _e96 = c_1;
        param_5 = _e96.xyz;
        let _e98 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = (*slot);
    if (lightmap_slot == (_e105 + 1i)) {
        let _e110 = unnamed.worldLightParams[0u];
        let _e111 = c_1;
        let _e113 = (_e111.xyz * _e110);
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = c_1;
    return _e120;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e84 = unnamed.packed_indices[0i][3u];
    let _e90 = unnamed.packed_indices[0i][3u];
    let _e95 = fog_tex_coord_1;
    let _e96 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    fog = _e96;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_6 = 0u;
    let _e101 = frag_tex_coord0_1;
    param_7 = _e101;
    param_8 = 0i;
    let _e102 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e103 = frag_color;
    color0_ = (_e102 * _e103);
    if override_type_3_ {
        param_9 = 1u;
        let _e105 = frag_tex_coord1_1;
        param_10 = _e105;
        param_11 = 1i;
        let _e106 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e106;
        let _e107 = color0_;
        let _e109 = color1_;
        let _e111 = (_e107.xyz + _e109.xyz);
        let _e113 = color0_[3u];
        let _e115 = color1_[3u];
        base = vec4<f32>(_e111.x, _e111.y, _e111.z, (_e113 * _e115));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e121 = frag_tex_coord1_1;
            param_13 = _e121;
            param_14 = 1i;
            let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            let _e123 = frag_color;
            color1_1 = (_e122 * _e123);
            let _e125 = color0_;
            let _e127 = color1_1;
            let _e129 = (_e125.xyz + _e127.xyz);
            let _e131 = color0_[3u];
            let _e133 = color1_1[3u];
            base = vec4<f32>(_e129.x, _e129.y, _e129.z, (_e131 * _e133));
        } else {
            param_15 = 1u;
            let _e139 = frag_tex_coord1_1;
            param_16 = _e139;
            param_17 = 1i;
            let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            let _e141 = frag_color;
            color1_2 = (_e140 * _e141);
            let _e143 = color0_;
            let _e145 = color1_2;
            let _e147 = (_e143.xyz * _e145.xyz);
            base[0u] = _e147.x;
            base[1u] = _e147.y;
            base[2u] = _e147.z;
            let _e155 = color0_[3u];
            let _e157 = color1_2[3u];
            base[3u] = (_e155 * _e157);
        }
    }
    if override_type_3_2 {
        let _e160 = base;
        let _e163 = fog[3u];
        let _e165 = (_e160.xyz * (1f - _e163));
        base[0u] = _e165.x;
        base[1u] = _e165.y;
        base[2u] = _e165.z;
    } else {
        if override_type_3_3 {
            let _e172 = base;
            let _e174 = fog[3u];
            base = (_e172 * (1f - _e174));
        } else {
            if override_type_3_4 {
                let _e178 = base[3u];
                let _e180 = fog[3u];
                base[3u] = (_e178 * (1f - _e180));
            } else {
                let _e184 = base;
                let _e185 = fog;
                let _e187 = unnamed.fogColor;
                let _e190 = fog[3u];
                base = mix(_e184, (_e185 * _e187), vec4(_e190));
            }
        }
    }
    if override_type_3_5 {
        let _e194 = base[3u];
        if (_e194 == 0f) {
            discard;
        }
    } else {
        if override_type_3_6 {
            let _e196 = base;
            let _e198 = base;
            if (dot(_e196.xyz, _e198.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e202 = base;
    out_color = _e202;
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
