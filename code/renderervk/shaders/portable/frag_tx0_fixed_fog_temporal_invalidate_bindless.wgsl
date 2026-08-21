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

    let _e56 = (*c);
    (*c) = max(_e56, vec3<f32>(0f, 0f, 0f));
    let _e58 = (*c);
    cutoff = (_e58 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e60 = (*c);
    lo = (_e60 / vec3(12.92f));
    let _e63 = (*c);
    hi = pow(((_e63 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e68 = hi;
    let _e69 = lo;
    let _e70 = cutoff;
    return mix(_e68, _e69, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e70));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e57 = (*role);
    let _e59 = (*role);
    let _e64 = unnamed.packed_indices[(_e57 / 4u)][(_e59 % 4u)];
    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e79 = (*uv);
    let _e80 = textureSample(wired_bindless_images[(_e64 & 4095u)], wired_bindless_samplers[((_e74 >> bitcast<u32>(12i)) & 255u)], _e79);
    c_1 = _e80;
    let _e81 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e81))) == 0i) {
        let _e86 = c_1;
        param = _e86.xyz;
        let _e88 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e88.x;
        c_1[1u] = _e88.y;
        c_1[2u] = _e88.z;
    }
    let _e95 = (*slot);
    if (lightmap_slot == (_e95 + 1i)) {
        let _e100 = unnamed.worldLightParams[0u];
        let _e101 = c_1;
        let _e103 = (_e101.xyz * _e100);
        c_1[0u] = _e103.x;
        c_1[1u] = _e103.y;
        c_1[2u] = _e103.z;
    }
    let _e110 = c_1;
    return _e110;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;

    let _e62 = unnamed.packed_indices[0i][3u];
    let _e68 = unnamed.packed_indices[0i][3u];
    let _e73 = fog_tex_coord_1;
    let _e74 = textureSample(wired_bindless_images[(_e62 & 4095u)], wired_bindless_samplers[((_e68 >> bitcast<u32>(12i)) & 255u)], _e73);
    fog = _e74;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e79 = frag_tex_coord0_1;
    param_2 = _e79;
    param_3 = 0i;
    let _e80 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e81 = frag_color;
    color0_ = (_e80 * _e81);
    let _e83 = color0_;
    base = _e83;
    let _e84 = color0_;
    base = _e84;
    if override_type_12_ {
        let _e85 = base;
        let _e88 = fog[3u];
        let _e90 = (_e85.xyz * (1f - _e88));
        base[0u] = _e90.x;
        base[1u] = _e90.y;
        base[2u] = _e90.z;
    } else {
        if override_type_12_1 {
            let _e97 = base;
            let _e99 = fog[3u];
            base = (_e97 * (1f - _e99));
        } else {
            if override_type_12_2 {
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
    if override_type_12_3 {
        let _e119 = base[3u];
        if (_e119 == 0f) {
            discard;
        }
    } else {
        if override_type_12_4 {
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
