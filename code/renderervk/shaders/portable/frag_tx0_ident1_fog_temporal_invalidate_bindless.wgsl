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
override override_type_12_: bool = (acff == 1i);
override override_type_12_1: bool = (acff == 2i);
override override_type_12_2: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_12_3: bool = (discard_mode == 1i);
override override_type_12_4: bool = (discard_mode == 2i);
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

    let _e54 = (*c);
    (*c) = max(_e54, vec3<f32>(0f, 0f, 0f));
    let _e56 = (*c);
    cutoff = (_e56 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e58 = (*c);
    lo = (_e58 / vec3(12.92f));
    let _e61 = (*c);
    hi = pow(((_e61 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e66 = hi;
    let _e67 = lo;
    let _e68 = cutoff;
    return mix(_e66, _e67, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e68));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e55 = (*role);
    let _e57 = (*role);
    let _e62 = unnamed.packed_indices[(_e55 / 4u)][(_e57 % 4u)];
    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e77 = (*uv);
    let _e78 = textureSample(wired_bindless_images[(_e62 & 4095u)], wired_bindless_samplers[((_e72 >> bitcast<u32>(12i)) & 255u)], _e77);
    c_1 = _e78;
    let _e79 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e79))) == 0i) {
        let _e84 = c_1;
        param = _e84.xyz;
        let _e86 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e86.x;
        c_1[1u] = _e86.y;
        c_1[2u] = _e86.z;
    }
    let _e93 = (*slot);
    if (lightmap_slot == (_e93 + 1i)) {
        let _e98 = unnamed.worldLightParams[0u];
        let _e99 = c_1;
        let _e101 = (_e99.xyz * _e98);
        c_1[0u] = _e101.x;
        c_1[1u] = _e101.y;
        c_1[2u] = _e101.z;
    }
    let _e108 = c_1;
    return _e108;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;

    let _e59 = unnamed.packed_indices[0i][3u];
    let _e65 = unnamed.packed_indices[0i][3u];
    let _e70 = fog_tex_coord_1;
    let _e71 = textureSample(wired_bindless_images[(_e59 & 4095u)], wired_bindless_samplers[((_e65 >> bitcast<u32>(12i)) & 255u)], _e70);
    fog = _e71;
    param_1 = 0u;
    let _e72 = frag_tex_coord0_1;
    param_2 = _e72;
    param_3 = 0i;
    let _e73 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e73;
    let _e74 = color0_;
    base = _e74;
    let _e75 = color0_;
    base = _e75;
    if override_type_12_ {
        let _e76 = base;
        let _e79 = fog[3u];
        let _e81 = (_e76.xyz * (1f - _e79));
        base[0u] = _e81.x;
        base[1u] = _e81.y;
        base[2u] = _e81.z;
    } else {
        if override_type_12_1 {
            let _e88 = base;
            let _e90 = fog[3u];
            base = (_e88 * (1f - _e90));
        } else {
            if override_type_12_2 {
                let _e94 = base[3u];
                let _e96 = fog[3u];
                base[3u] = (_e94 * (1f - _e96));
            } else {
                let _e100 = base;
                let _e101 = fog;
                let _e103 = unnamed.fogColor;
                let _e106 = fog[3u];
                base = mix(_e100, (_e101 * _e103), vec4(_e106));
            }
        }
    }
    if override_type_12_3 {
        let _e110 = base[3u];
        if (_e110 == 0f) {
            discard;
        }
    } else {
        if override_type_12_4 {
            let _e112 = base;
            let _e114 = base;
            if (dot(_e112.xyz, _e114.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e118 = base;
    out_color = _e118;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
