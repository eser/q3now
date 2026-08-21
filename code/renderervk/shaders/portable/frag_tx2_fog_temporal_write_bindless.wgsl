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
var<private> frag_tex_coord2_1: vec2<f32>;
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
    var color2_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color2_1: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color2_2: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;

    let _e97 = unnamed.packed_indices[0i][3u];
    let _e103 = unnamed.packed_indices[0i][3u];
    let _e108 = fog_tex_coord_1;
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    fog = _e109;
    let _e110 = frag_color0In_1;
    param_6 = _e110.xyz;
    let _e112 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e114 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e112.x, _e112.y, _e112.z, _e114);
    param_7 = 0u;
    let _e119 = frag_tex_coord0_1;
    param_8 = _e119;
    param_9 = 0i;
    let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e121 = frag_color0_;
    color0_ = (_e120 * _e121);
    if override_type_3_ {
        param_10 = 1u;
        let _e123 = frag_tex_coord1_1;
        param_11 = _e123;
        param_12 = 1i;
        let _e124 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e124;
        param_13 = 2u;
        let _e125 = frag_tex_coord2_1;
        param_14 = _e125;
        param_15 = 2i;
        let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
        color2_ = _e126;
        let _e127 = color0_;
        let _e129 = color1_;
        let _e132 = color2_;
        let _e134 = ((_e127.xyz + _e129.xyz) + _e132.xyz);
        let _e136 = color0_[3u];
        let _e138 = color1_[3u];
        let _e141 = color2_[3u];
        base = vec4<f32>(_e134.x, _e134.y, _e134.z, ((_e136 * _e138) * _e141));
    } else {
        if override_type_3_1 {
            param_16 = 1u;
            let _e147 = frag_tex_coord1_1;
            param_17 = _e147;
            param_18 = 1i;
            let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e149 = frag_color0_;
            color1_1 = (_e148 * _e149);
            param_19 = 2u;
            let _e151 = frag_tex_coord2_1;
            param_20 = _e151;
            param_21 = 2i;
            let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            let _e153 = frag_color0_;
            color2_1 = (_e152 * _e153);
            let _e155 = color0_;
            let _e157 = color1_1;
            let _e160 = color2_1;
            let _e162 = ((_e155.xyz + _e157.xyz) + _e160.xyz);
            let _e164 = color0_[3u];
            let _e166 = color1_1[3u];
            let _e169 = color2_1[3u];
            base = vec4<f32>(_e162.x, _e162.y, _e162.z, ((_e164 * _e166) * _e169));
        } else {
            param_22 = 1u;
            let _e175 = frag_tex_coord1_1;
            param_23 = _e175;
            param_24 = 1i;
            let _e176 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            color1_2 = _e176;
            param_25 = 2u;
            let _e177 = frag_tex_coord2_1;
            param_26 = _e177;
            param_27 = 2i;
            let _e178 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
            color2_2 = _e178;
            let _e179 = color0_;
            let _e181 = color1_2;
            let _e184 = color2_2;
            let _e186 = ((_e179.xyz * _e181.xyz) * _e184.xyz);
            base[0u] = _e186.x;
            base[1u] = _e186.y;
            base[2u] = _e186.z;
            let _e194 = color0_[3u];
            let _e196 = color1_2[3u];
            let _e199 = color2_2[3u];
            base[3u] = ((_e194 * _e196) * _e199);
        }
    }
    if override_type_3_2 {
        let _e202 = base;
        let _e205 = fog[3u];
        let _e207 = (_e202.xyz * (1f - _e205));
        base[0u] = _e207.x;
        base[1u] = _e207.y;
        base[2u] = _e207.z;
    } else {
        if override_type_3_3 {
            let _e214 = base;
            let _e216 = fog[3u];
            base = (_e214 * (1f - _e216));
        } else {
            if override_type_3_4 {
                let _e220 = base[3u];
                let _e222 = fog[3u];
                base[3u] = (_e220 * (1f - _e222));
            } else {
                let _e226 = base;
                let _e227 = fog;
                let _e229 = unnamed.fogColor;
                let _e232 = fog[3u];
                base = mix(_e226, (_e227 * _e229), vec4(_e232));
            }
        }
    }
    if override_type_3_5 {
        let _e236 = base[3u];
        if (_e236 == 0f) {
            discard;
        }
    } else {
        if override_type_3_6 {
            let _e238 = base;
            let _e240 = base;
            if (dot(_e238.xyz, _e240.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e244 = base;
    out_color = _e244;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
