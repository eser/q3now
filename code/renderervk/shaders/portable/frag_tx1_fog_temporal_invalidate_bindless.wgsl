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
override override_type_12_: bool = (tex_mode == 1i);
override override_type_12_1: bool = (tex_mode == 2i);
@id(10) override acff: i32 = 0i;
override override_type_12_2: bool = (acff == 1i);
override override_type_12_3: bool = (acff == 2i);
override override_type_12_4: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_12_5: bool = (discard_mode == 1i);
override override_type_12_6: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_() {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
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
    var param: vec3<f32>;

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
        param = _e89.xyz;
        let _e91 = sRGBToLinear_u0028_vf3_u003b((&param));
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
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;

    let _e78 = unnamed.packed_indices[0i][3u];
    let _e84 = unnamed.packed_indices[0i][3u];
    let _e89 = fog_tex_coord_1;
    let _e90 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    fog = _e90;
    let _e91 = frag_color0In_1;
    param_1 = _e91.xyz;
    let _e93 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e95 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e93.x, _e93.y, _e93.z, _e95);
    param_2 = 0u;
    let _e100 = frag_tex_coord0_1;
    param_3 = _e100;
    param_4 = 0i;
    let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e102 = frag_color0_;
    color0_ = (_e101 * _e102);
    if override_type_12_ {
        param_5 = 1u;
        let _e104 = frag_tex_coord1_1;
        param_6 = _e104;
        param_7 = 1i;
        let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e105;
        let _e106 = color0_;
        let _e108 = color1_;
        let _e110 = (_e106.xyz + _e108.xyz);
        let _e112 = color0_[3u];
        let _e114 = color1_[3u];
        base = vec4<f32>(_e110.x, _e110.y, _e110.z, (_e112 * _e114));
    } else {
        if override_type_12_1 {
            param_8 = 1u;
            let _e120 = frag_tex_coord1_1;
            param_9 = _e120;
            param_10 = 1i;
            let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e122 = frag_color0_;
            color1_1 = (_e121 * _e122);
            let _e124 = color0_;
            let _e126 = color1_1;
            let _e128 = (_e124.xyz + _e126.xyz);
            let _e130 = color0_[3u];
            let _e132 = color1_1[3u];
            base = vec4<f32>(_e128.x, _e128.y, _e128.z, (_e130 * _e132));
        } else {
            param_11 = 1u;
            let _e138 = frag_tex_coord1_1;
            param_12 = _e138;
            param_13 = 1i;
            let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e139;
            let _e140 = color0_;
            let _e142 = color1_2;
            let _e144 = (_e140.xyz * _e142.xyz);
            base[0u] = _e144.x;
            base[1u] = _e144.y;
            base[2u] = _e144.z;
            let _e152 = color0_[3u];
            let _e154 = color1_2[3u];
            base[3u] = (_e152 * _e154);
        }
    }
    if override_type_12_2 {
        let _e157 = base;
        let _e160 = fog[3u];
        let _e162 = (_e157.xyz * (1f - _e160));
        base[0u] = _e162.x;
        base[1u] = _e162.y;
        base[2u] = _e162.z;
    } else {
        if override_type_12_3 {
            let _e169 = base;
            let _e171 = fog[3u];
            base = (_e169 * (1f - _e171));
        } else {
            if override_type_12_4 {
                let _e175 = base[3u];
                let _e177 = fog[3u];
                base[3u] = (_e175 * (1f - _e177));
            } else {
                let _e181 = base;
                let _e182 = fog;
                let _e184 = unnamed.fogColor;
                let _e187 = fog[3u];
                base = mix(_e181, (_e182 * _e184), vec4(_e187));
            }
        }
    }
    if override_type_12_5 {
        let _e191 = base[3u];
        if (_e191 == 0f) {
            discard;
        }
    } else {
        if override_type_12_6 {
            let _e193 = base;
            let _e195 = base;
            if (dot(_e193.xyz, _e195.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e199 = base;
    out_color = _e199;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
