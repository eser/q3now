enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    ent_color0_: vec4<f32>,
    ent_color1_: vec4<f32>,
    ent_color2_: vec4<f32>,
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

    let _e57 = (*value);
    let _e58 = (*value);
    let _e60 = all((_e57 == _e58));
    phi_66_ = _e60;
    if _e60 {
        let _e61 = (*value);
        phi_66_ = all((abs(_e61) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e66 = phi_66_;
    return _e66;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e57 = (*value_1);
    let _e58 = (*value_1);
    let _e60 = all((_e57 == _e58));
    phi_51_ = _e60;
    if _e60 {
        let _e61 = (*value_1);
        phi_51_ = all((abs(_e61) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e66 = phi_51_;
    return _e66;
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
    let _e66 = temporalOutcome_1;
    let _e67 = (_e66 != 1u);
    phi_89_ = _e67;
    if !(_e67) {
        let _e69 = temporalCurrentClip_1;
        param = _e69;
        let _e70 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e70);
    }
    let _e73 = phi_89_;
    phi_98_ = _e73;
    if !(_e73) {
        let _e75 = temporalPreviousClip_1;
        param_1 = _e75;
        let _e76 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e76);
    }
    let _e79 = phi_98_;
    phi_108_ = _e79;
    if !(_e79) {
        let _e82 = temporalCurrentClip_1[3u];
        phi_108_ = (_e82 <= 0.000001f);
    }
    let _e85 = phi_108_;
    phi_115_ = _e85;
    if !(_e85) {
        let _e88 = temporalPreviousClip_1[3u];
        phi_115_ = (_e88 <= 0.000001f);
    }
    let _e91 = phi_115_;
    if _e91 {
        return;
    }
    let _e92 = temporalCurrentClip_1;
    let _e95 = temporalCurrentClip_1[3u];
    currentNdc = (_e92.xy / vec2(_e95));
    let _e98 = temporalPreviousClip_1;
    let _e101 = temporalPreviousClip_1[3u];
    previousNdc = (_e98.xy / vec2(_e101));
    let _e104 = currentNdc;
    param_2 = _e104;
    let _e105 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e106 = !(_e105);
    phi_144_ = _e106;
    if !(_e106) {
        let _e108 = previousNdc;
        param_3 = _e108;
        let _e109 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e109);
    }
    let _e112 = phi_144_;
    if _e112 {
        return;
    }
    let _e113 = currentNdc;
    currentUv = ((_e113 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e116 = previousNdc;
    previousUv = ((_e116 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e119 = currentUv;
    let _e120 = previousUv;
    velocity = (_e119 - _e120);
    let _e122 = velocity;
    param_4 = _e122;
    let _e123 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e123) {
        return;
    }
    let _e125 = velocity;
    out_temporal_velocity = _e125;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c);
    (*c) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e83 = (*uv);
    let _e84 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    c_1 = _e84;
    let _e85 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e85))) == 0i) {
        let _e90 = c_1;
        param_5 = _e90.xyz;
        let _e92 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e92.x;
        c_1[1u] = _e92.y;
        c_1[2u] = _e92.z;
    }
    let _e99 = (*slot);
    if (lightmap_slot == (_e99 + 1i)) {
        let _e104 = unnamed.worldLightParams[0u];
        let _e105 = c_1;
        let _e107 = (_e105.xyz * _e104);
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = c_1;
    return _e114;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;

    let _e65 = unnamed.packed_indices[0i][3u];
    let _e71 = unnamed.packed_indices[0i][3u];
    let _e76 = fog_tex_coord_1;
    let _e77 = textureSample(wired_bindless_images[(_e65 & 4095u)], wired_bindless_samplers[((_e71 >> bitcast<u32>(12i)) & 255u)], _e76);
    fog = _e77;
    param_6 = 0u;
    let _e78 = frag_tex_coord0_1;
    param_7 = _e78;
    param_8 = 0i;
    let _e79 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    let _e81 = unnamed.ent_color0_;
    color0_ = (_e79 * _e81);
    let _e83 = color0_;
    base = _e83;
    let _e84 = color0_;
    base = _e84;
    if override_type_3_ {
        let _e85 = base;
        let _e88 = fog[3u];
        let _e90 = (_e85.xyz * (1f - _e88));
        base[0u] = _e90.x;
        base[1u] = _e90.y;
        base[2u] = _e90.z;
    } else {
        if override_type_3_1 {
            let _e97 = base;
            let _e99 = fog[3u];
            base = (_e97 * (1f - _e99));
        } else {
            if override_type_3_2 {
                let _e103 = base[3u];
                let _e105 = fog[3u];
                base[3u] = (_e103 * (1f - _e105));
            } else {
                let _e109 = base;
                let _e110 = fog;
                let _e112 = unnamed.fogColor;
                let _e115 = fog[3u];
                base = mix(_e109, (_e110 * _e112), vec4(_e115));
            }
        }
    }
    if override_type_3_3 {
        let _e119 = base[3u];
        if (_e119 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e121 = base;
            let _e123 = base;
            if (dot(_e121.xyz, _e123.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e127 = base;
    out_color = _e127;
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
