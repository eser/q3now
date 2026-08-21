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
var<private> frag_tex_coord2_1: vec2<f32>;
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
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;

    let _e91 = unnamed.packed_indices[0i][3u];
    let _e97 = unnamed.packed_indices[0i][3u];
    let _e102 = fog_tex_coord_1;
    let _e103 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    fog = _e103;
    let _e104 = frag_color0In_1;
    param_1 = _e104.xyz;
    let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e108 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e106.x, _e106.y, _e106.z, _e108);
    param_2 = 0u;
    let _e113 = frag_tex_coord0_1;
    param_3 = _e113;
    param_4 = 0i;
    let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e115 = frag_color0_;
    color0_ = (_e114 * _e115);
    if override_type_12_ {
        param_5 = 1u;
        let _e117 = frag_tex_coord1_1;
        param_6 = _e117;
        param_7 = 1i;
        let _e118 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e118;
        param_8 = 2u;
        let _e119 = frag_tex_coord2_1;
        param_9 = _e119;
        param_10 = 2i;
        let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e120;
        let _e121 = color0_;
        let _e123 = color1_;
        let _e126 = color2_;
        let _e128 = ((_e121.xyz + _e123.xyz) + _e126.xyz);
        let _e130 = color0_[3u];
        let _e132 = color1_[3u];
        let _e135 = color2_[3u];
        base = vec4<f32>(_e128.x, _e128.y, _e128.z, ((_e130 * _e132) * _e135));
    } else {
        if override_type_12_1 {
            param_11 = 1u;
            let _e141 = frag_tex_coord1_1;
            param_12 = _e141;
            param_13 = 1i;
            let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e143 = frag_color0_;
            color1_1 = (_e142 * _e143);
            param_14 = 2u;
            let _e145 = frag_tex_coord2_1;
            param_15 = _e145;
            param_16 = 2i;
            let _e146 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e147 = frag_color0_;
            color2_1 = (_e146 * _e147);
            let _e149 = color0_;
            let _e151 = color1_1;
            let _e154 = color2_1;
            let _e156 = ((_e149.xyz + _e151.xyz) + _e154.xyz);
            let _e158 = color0_[3u];
            let _e160 = color1_1[3u];
            let _e163 = color2_1[3u];
            base = vec4<f32>(_e156.x, _e156.y, _e156.z, ((_e158 * _e160) * _e163));
        } else {
            param_17 = 1u;
            let _e169 = frag_tex_coord1_1;
            param_18 = _e169;
            param_19 = 1i;
            let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e170;
            param_20 = 2u;
            let _e171 = frag_tex_coord2_1;
            param_21 = _e171;
            param_22 = 2i;
            let _e172 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e172;
            let _e173 = color0_;
            let _e175 = color1_2;
            let _e178 = color2_2;
            let _e180 = ((_e173.xyz * _e175.xyz) * _e178.xyz);
            base[0u] = _e180.x;
            base[1u] = _e180.y;
            base[2u] = _e180.z;
            let _e188 = color0_[3u];
            let _e190 = color1_2[3u];
            let _e193 = color2_2[3u];
            base[3u] = ((_e188 * _e190) * _e193);
        }
    }
    if override_type_12_2 {
        let _e196 = base;
        let _e199 = fog[3u];
        let _e201 = (_e196.xyz * (1f - _e199));
        base[0u] = _e201.x;
        base[1u] = _e201.y;
        base[2u] = _e201.z;
    } else {
        if override_type_12_3 {
            let _e208 = base;
            let _e210 = fog[3u];
            base = (_e208 * (1f - _e210));
        } else {
            if override_type_12_4 {
                let _e214 = base[3u];
                let _e216 = fog[3u];
                base[3u] = (_e214 * (1f - _e216));
            } else {
                let _e220 = base;
                let _e221 = fog;
                let _e223 = unnamed.fogColor;
                let _e226 = fog[3u];
                base = mix(_e220, (_e221 * _e223), vec4(_e226));
            }
        }
    }
    if override_type_12_5 {
        let _e230 = base[3u];
        if (_e230 == 0f) {
            discard;
        }
    } else {
        if override_type_12_6 {
            let _e232 = base;
            let _e234 = base;
            if (dot(_e232.xyz, _e234.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e238 = base;
    out_color = _e238;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
