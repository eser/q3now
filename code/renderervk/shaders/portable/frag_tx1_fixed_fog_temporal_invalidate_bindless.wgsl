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
    var param: vec3<f32>;

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
        param = _e90.xyz;
        let _e92 = sRGBToLinear_u0028_vf3_u003b((&param));
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
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var color1_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_2: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;

    let _e78 = unnamed.packed_indices[0i][3u];
    let _e84 = unnamed.packed_indices[0i][3u];
    let _e89 = fog_tex_coord_1;
    let _e90 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    fog = _e90;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e95 = frag_tex_coord0_1;
    param_2 = _e95;
    param_3 = 0i;
    let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e97 = frag_color;
    color0_ = (_e96 * _e97);
    if override_type_12_ {
        param_4 = 1u;
        let _e99 = frag_tex_coord1_1;
        param_5 = _e99;
        param_6 = 1i;
        let _e100 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e100;
        let _e101 = color0_;
        let _e103 = color1_;
        let _e105 = (_e101.xyz + _e103.xyz);
        let _e107 = color0_[3u];
        let _e109 = color1_[3u];
        base = vec4<f32>(_e105.x, _e105.y, _e105.z, (_e107 * _e109));
    } else {
        if override_type_12_1 {
            param_7 = 1u;
            let _e115 = frag_tex_coord1_1;
            param_8 = _e115;
            param_9 = 1i;
            let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e117 = frag_color;
            color1_1 = (_e116 * _e117);
            let _e119 = color0_;
            let _e121 = color1_1;
            let _e123 = (_e119.xyz + _e121.xyz);
            let _e125 = color0_[3u];
            let _e127 = color1_1[3u];
            base = vec4<f32>(_e123.x, _e123.y, _e123.z, (_e125 * _e127));
        } else {
            param_10 = 1u;
            let _e133 = frag_tex_coord1_1;
            param_11 = _e133;
            param_12 = 1i;
            let _e134 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e135 = frag_color;
            color1_2 = (_e134 * _e135);
            let _e137 = color0_;
            let _e139 = color1_2;
            let _e141 = (_e137.xyz * _e139.xyz);
            base[0u] = _e141.x;
            base[1u] = _e141.y;
            base[2u] = _e141.z;
            let _e149 = color0_[3u];
            let _e151 = color1_2[3u];
            base[3u] = (_e149 * _e151);
        }
    }
    if override_type_12_2 {
        let _e154 = base;
        let _e157 = fog[3u];
        let _e159 = (_e154.xyz * (1f - _e157));
        base[0u] = _e159.x;
        base[1u] = _e159.y;
        base[2u] = _e159.z;
    } else {
        if override_type_12_3 {
            let _e166 = base;
            let _e168 = fog[3u];
            base = (_e166 * (1f - _e168));
        } else {
            if override_type_12_4 {
                let _e172 = base[3u];
                let _e174 = fog[3u];
                base[3u] = (_e172 * (1f - _e174));
            } else {
                let _e178 = base;
                let _e179 = fog;
                let _e181 = unnamed.fogColor;
                let _e184 = fog[3u];
                base = mix(_e178, (_e179 * _e181), vec4(_e184));
            }
        }
    }
    if override_type_12_5 {
        let _e188 = base[3u];
        if (_e188 == 0f) {
            discard;
        }
    } else {
        if override_type_12_6 {
            let _e190 = base;
            let _e192 = base;
            if (dot(_e190.xyz, _e192.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e196 = base;
    out_color = _e196;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
