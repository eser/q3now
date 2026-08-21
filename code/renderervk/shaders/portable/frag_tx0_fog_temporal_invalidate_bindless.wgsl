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
var<private> frag_color0In_1: vec4<f32>;
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

    let _e55 = (*c);
    (*c) = max(_e55, vec3<f32>(0f, 0f, 0f));
    let _e57 = (*c);
    cutoff = (_e57 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e59 = (*c);
    lo = (_e59 / vec3(12.92f));
    let _e62 = (*c);
    hi = pow(((_e62 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e67 = hi;
    let _e68 = lo;
    let _e69 = cutoff;
    return mix(_e67, _e68, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e69));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e56 = (*role);
    let _e58 = (*role);
    let _e63 = unnamed.packed_indices[(_e56 / 4u)][(_e58 % 4u)];
    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e78 = (*uv);
    let _e79 = textureSample(wired_bindless_images[(_e63 & 4095u)], wired_bindless_samplers[((_e73 >> bitcast<u32>(12i)) & 255u)], _e78);
    c_1 = _e79;
    let _e80 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e80))) == 0i) {
        let _e85 = c_1;
        param = _e85.xyz;
        let _e87 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e87.x;
        c_1[1u] = _e87.y;
        c_1[2u] = _e87.z;
    }
    let _e94 = (*slot);
    if (lightmap_slot == (_e94 + 1i)) {
        let _e99 = unnamed.worldLightParams[0u];
        let _e100 = c_1;
        let _e102 = (_e100.xyz * _e99);
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = c_1;
    return _e109;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;

    let _e62 = unnamed.packed_indices[0i][3u];
    let _e68 = unnamed.packed_indices[0i][3u];
    let _e73 = fog_tex_coord_1;
    let _e74 = textureSample(wired_bindless_images[(_e62 & 4095u)], wired_bindless_samplers[((_e68 >> bitcast<u32>(12i)) & 255u)], _e73);
    fog = _e74;
    let _e75 = frag_color0In_1;
    param_1 = _e75.xyz;
    let _e77 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e79 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e77.x, _e77.y, _e77.z, _e79);
    param_2 = 0u;
    let _e84 = frag_tex_coord0_1;
    param_3 = _e84;
    param_4 = 0i;
    let _e85 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e86 = frag_color0_;
    color0_ = (_e85 * _e86);
    let _e88 = color0_;
    base = _e88;
    let _e89 = color0_;
    base = _e89;
    if override_type_12_ {
        let _e90 = base;
        let _e93 = fog[3u];
        let _e95 = (_e90.xyz * (1f - _e93));
        base[0u] = _e95.x;
        base[1u] = _e95.y;
        base[2u] = _e95.z;
    } else {
        if override_type_12_1 {
            let _e102 = base;
            let _e104 = fog[3u];
            base = (_e102 * (1f - _e104));
        } else {
            if override_type_12_2 {
                let _e108 = base[3u];
                let _e110 = fog[3u];
                base[3u] = (_e108 * (1f - _e110));
            } else {
                let _e114 = base;
                let _e115 = fog;
                let _e117 = unnamed.fogColor;
                let _e120 = fog[3u];
                base = mix(_e114, (_e115 * _e117), vec4(_e120));
            }
        }
    }
    if override_type_12_3 {
        let _e124 = base[3u];
        if (_e124 == 0f) {
            discard;
        }
    } else {
        if override_type_12_4 {
            let _e126 = base;
            let _e128 = base;
            if (dot(_e126.xyz, _e128.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e132 = base;
    out_color = _e132;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
