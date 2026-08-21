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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_66_: bool;

    let _e58 = (*value);
    let _e59 = (*value);
    let _e61 = all((_e58 == _e59));
    phi_66_ = _e61;
    if _e61 {
        let _e62 = (*value);
        phi_66_ = all((abs(_e62) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e67 = phi_66_;
    return _e67;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_51_: bool;

    let _e58 = (*value_1);
    let _e59 = (*value_1);
    let _e61 = all((_e58 == _e59));
    phi_51_ = _e61;
    if _e61 {
        let _e62 = (*value_1);
        phi_51_ = all((abs(_e62) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e67 = phi_51_;
    return _e67;
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
    let _e67 = temporalOutcome_1;
    let _e68 = (_e67 != 1u);
    phi_89_ = _e68;
    if !(_e68) {
        let _e70 = temporalCurrentClip_1;
        param = _e70;
        let _e71 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_89_ = !(_e71);
    }
    let _e74 = phi_89_;
    phi_98_ = _e74;
    if !(_e74) {
        let _e76 = temporalPreviousClip_1;
        param_1 = _e76;
        let _e77 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_98_ = !(_e77);
    }
    let _e80 = phi_98_;
    phi_108_ = _e80;
    if !(_e80) {
        let _e83 = temporalCurrentClip_1[3u];
        phi_108_ = (_e83 <= 0.000001f);
    }
    let _e86 = phi_108_;
    phi_115_ = _e86;
    if !(_e86) {
        let _e89 = temporalPreviousClip_1[3u];
        phi_115_ = (_e89 <= 0.000001f);
    }
    let _e92 = phi_115_;
    if _e92 {
        return;
    }
    let _e93 = temporalCurrentClip_1;
    let _e96 = temporalCurrentClip_1[3u];
    currentNdc = (_e93.xy / vec2(_e96));
    let _e99 = temporalPreviousClip_1;
    let _e102 = temporalPreviousClip_1[3u];
    previousNdc = (_e99.xy / vec2(_e102));
    let _e105 = currentNdc;
    param_2 = _e105;
    let _e106 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e107 = !(_e106);
    phi_144_ = _e107;
    if !(_e107) {
        let _e109 = previousNdc;
        param_3 = _e109;
        let _e110 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_144_ = !(_e110);
    }
    let _e113 = phi_144_;
    if _e113 {
        return;
    }
    let _e114 = currentNdc;
    currentUv = ((_e114 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e117 = previousNdc;
    previousUv = ((_e117 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e120 = currentUv;
    let _e121 = previousUv;
    velocity = (_e120 - _e121);
    let _e123 = velocity;
    param_4 = _e123;
    let _e124 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e124) {
        return;
    }
    let _e126 = velocity;
    out_temporal_velocity = _e126;
    out_temporal_validity = 1f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e61 = (*c);
    (*c) = max(_e61, vec3<f32>(0f, 0f, 0f));
    let _e63 = (*c);
    cutoff = (_e63 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e65 = (*c);
    lo = (_e65 / vec3(12.92f));
    let _e68 = (*c);
    hi = pow(((_e68 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e73 = hi;
    let _e74 = lo;
    let _e75 = cutoff;
    return mix(_e73, _e74, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e75));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e84 = (*uv);
    let _e85 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    c_1 = _e85;
    let _e86 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e86))) == 0i) {
        let _e91 = c_1;
        param_5 = _e91.xyz;
        let _e93 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e93.x;
        c_1[1u] = _e93.y;
        c_1[2u] = _e93.z;
    }
    let _e100 = (*slot);
    if (lightmap_slot == (_e100 + 1i)) {
        let _e105 = unnamed.worldLightParams[0u];
        let _e106 = c_1;
        let _e108 = (_e106.xyz * _e105);
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = c_1;
    return _e115;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var base: vec4<f32>;

    let _e68 = unnamed.packed_indices[0i][3u];
    let _e74 = unnamed.packed_indices[0i][3u];
    let _e79 = fog_tex_coord_1;
    let _e80 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e74 >> bitcast<u32>(12i)) & 255u)], _e79);
    fog = _e80;
    let _e81 = frag_color0In_1;
    param_6 = _e81.xyz;
    let _e83 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e85 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e83.x, _e83.y, _e83.z, _e85);
    param_7 = 0u;
    let _e90 = frag_tex_coord0_1;
    param_8 = _e90;
    param_9 = 0i;
    let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e92 = frag_color0_;
    color0_ = (_e91 * _e92);
    let _e94 = color0_;
    base = _e94;
    let _e95 = color0_;
    base = _e95;
    if override_type_3_ {
        let _e96 = base;
        let _e99 = fog[3u];
        let _e101 = (_e96.xyz * (1f - _e99));
        base[0u] = _e101.x;
        base[1u] = _e101.y;
        base[2u] = _e101.z;
    } else {
        if override_type_3_1 {
            let _e108 = base;
            let _e110 = fog[3u];
            base = (_e108 * (1f - _e110));
        } else {
            if override_type_3_2 {
                let _e114 = base[3u];
                let _e116 = fog[3u];
                base[3u] = (_e114 * (1f - _e116));
            } else {
                let _e120 = base;
                let _e121 = fog;
                let _e123 = unnamed.fogColor;
                let _e126 = fog[3u];
                base = mix(_e120, (_e121 * _e123), vec4(_e126));
            }
        }
    }
    if override_type_3_3 {
        let _e130 = base[3u];
        if (_e130 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e132 = base;
            let _e134 = base;
            if (dot(_e132.xyz, _e134.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e138 = base;
    out_color = _e138;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
