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

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(0) override alpha_test_func: i32 = 0i;
override override_type_10_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_10_1: bool = (alpha_test_func == 2i);
override override_type_10_2: bool = (alpha_test_func == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_10_3: bool = (discard_mode == 1i);
override override_type_10_4: bool = (discard_mode == 2i);
@id(11) override depth_fade_scale: f32 = 2f;
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e50 = (*c);
    (*c) = max(_e50, vec3<f32>(0f, 0f, 0f));
    let _e52 = (*c);
    cutoff = (_e52 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e54 = (*c);
    lo = (_e54 / vec3(12.92f));
    let _e57 = (*c);
    hi = pow(((_e57 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e62 = hi;
    let _e63 = lo;
    let _e64 = cutoff;
    return mix(_e62, _e63, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e64));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e51 = (*role);
    let _e53 = (*role);
    let _e58 = unnamed.packed_indices[(_e51 / 4u)][(_e53 % 4u)];
    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e73 = (*uv);
    let _e74 = textureSample(wired_bindless_images[(_e58 & 4095u)], wired_bindless_samplers[((_e68 >> bitcast<u32>(12i)) & 255u)], _e73);
    c_1 = _e74;
    let _e75 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e75))) == 0i) {
        let _e80 = c_1;
        param = _e80.xyz;
        let _e82 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e82.x;
        c_1[1u] = _e82.y;
        c_1[2u] = _e82.z;
    }
    let _e89 = (*slot);
    if (lightmap_slot == (_e89 + 1i)) {
        let _e94 = unnamed.worldLightParams[0u];
        let _e95 = c_1;
        let _e97 = (_e95.xyz * _e94);
        c_1[0u] = _e97.x;
        c_1[1u] = _e97.y;
        c_1[2u] = _e97.z;
    }
    let _e104 = c_1;
    return _e104;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    param_1 = 0u;
    let _e56 = frag_tex_coord0_1;
    param_2 = _e56;
    param_3 = 0i;
    let _e57 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e59 = unnamed.ent_color0_;
    color0_ = (_e57 * _e59);
    let _e61 = color0_;
    base = _e61;
    if override_type_10_ {
        let _e63 = color0_[3u];
        if (_e63 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e66 = color0_[3u];
            if (_e66 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e69 = color0_[3u];
                if (_e69 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e71 = color0_;
    base = _e71;
    if override_type_10_3 {
        let _e73 = base[3u];
        if (_e73 == 0f) {
            discard;
        }
    } else {
        if override_type_10_4 {
            let _e75 = base;
            let _e77 = base;
            if (dot(_e75.xyz, _e77.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e81 = gl_FragCoord_1;
    let _e86 = unnamed.packed_indices[1i][0u];
    let _e92 = unnamed.packed_indices[1i][0u];
    let _e97 = textureDimensions(wired_bindless_images[(_e86 & 4095u)], 0i);
    screenUV = (_e81.xy / vec2<f32>(vec2<i32>(_e97)));
    let _e104 = unnamed.packed_indices[1i][0u];
    let _e110 = unnamed.packed_indices[1i][0u];
    let _e115 = screenUV;
    let _e116 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    sceneDepth = _e116.x;
    let _e119 = gl_FragCoord_1[2u];
    fragDepth = _e119;
    let _e120 = fragDepth;
    let _e121 = sceneDepth;
    depthDiff = (_e120 - _e121);
    let _e124 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e124);
    let _e126 = fadeFactor;
    let _e128 = base[3u];
    base[3u] = (_e128 * _e126);
    let _e131 = fadeFactor;
    let _e132 = base;
    let _e134 = (_e132.xyz * _e131);
    base[0u] = _e134.x;
    base[1u] = _e134.y;
    base[2u] = _e134.z;
    let _e141 = base;
    out_color = _e141;
    return;
}

@fragment 
fn main(@location(1) frag_tex_coord0_: vec2<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord0_1 = frag_tex_coord0_;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e5 = out_color;
    return _e5;
}
