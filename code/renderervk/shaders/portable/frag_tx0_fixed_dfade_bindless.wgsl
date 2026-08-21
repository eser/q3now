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

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e52 = (*c);
    (*c) = max(_e52, vec3<f32>(0f, 0f, 0f));
    let _e54 = (*c);
    cutoff = (_e54 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e56 = (*c);
    lo = (_e56 / vec3(12.92f));
    let _e59 = (*c);
    hi = pow(((_e59 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e64 = hi;
    let _e65 = lo;
    let _e66 = cutoff;
    return mix(_e64, _e65, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e66));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e53 = (*role);
    let _e55 = (*role);
    let _e60 = unnamed.packed_indices[(_e53 / 4u)][(_e55 % 4u)];
    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e75 = (*uv);
    let _e76 = textureSample(wired_bindless_images[(_e60 & 4095u)], wired_bindless_samplers[((_e70 >> bitcast<u32>(12i)) & 255u)], _e75);
    c_1 = _e76;
    let _e77 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e77))) == 0i) {
        let _e82 = c_1;
        param = _e82.xyz;
        let _e84 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e84.x;
        c_1[1u] = _e84.y;
        c_1[2u] = _e84.z;
    }
    let _e91 = (*slot);
    if (lightmap_slot == (_e91 + 1i)) {
        let _e96 = unnamed.worldLightParams[0u];
        let _e97 = c_1;
        let _e99 = (_e97.xyz * _e96);
        c_1[0u] = _e99.x;
        c_1[1u] = _e99.y;
        c_1[2u] = _e99.z;
    }
    let _e106 = c_1;
    return _e106;
}

fn main_1() {
    var frag_color: vec4<f32>;
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

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e63 = frag_tex_coord0_1;
    param_2 = _e63;
    param_3 = 0i;
    let _e64 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e65 = frag_color;
    color0_ = (_e64 * _e65);
    let _e67 = color0_;
    base = _e67;
    if override_type_10_ {
        let _e69 = color0_[3u];
        if (_e69 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e72 = color0_[3u];
            if (_e72 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e75 = color0_[3u];
                if (_e75 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e77 = color0_;
    base = _e77;
    if override_type_10_3 {
        let _e79 = base[3u];
        if (_e79 == 0f) {
            discard;
        }
    } else {
        if override_type_10_4 {
            let _e81 = base;
            let _e83 = base;
            if (dot(_e81.xyz, _e83.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e87 = gl_FragCoord_1;
    let _e92 = unnamed.packed_indices[1i][0u];
    let _e98 = unnamed.packed_indices[1i][0u];
    let _e103 = textureDimensions(wired_bindless_images[(_e92 & 4095u)], 0i);
    screenUV = (_e87.xy / vec2<f32>(vec2<i32>(_e103)));
    let _e110 = unnamed.packed_indices[1i][0u];
    let _e116 = unnamed.packed_indices[1i][0u];
    let _e121 = screenUV;
    let _e122 = textureSample(wired_bindless_images[(_e110 & 4095u)], wired_bindless_samplers[((_e116 >> bitcast<u32>(12i)) & 255u)], _e121);
    sceneDepth = _e122.x;
    let _e125 = gl_FragCoord_1[2u];
    fragDepth = _e125;
    let _e126 = fragDepth;
    let _e127 = sceneDepth;
    depthDiff = (_e126 - _e127);
    let _e130 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e130);
    let _e132 = fadeFactor;
    let _e134 = base[3u];
    base[3u] = (_e134 * _e132);
    let _e137 = fadeFactor;
    let _e138 = base;
    let _e140 = (_e138.xyz * _e137);
    base[0u] = _e140.x;
    base[1u] = _e140.y;
    base[2u] = _e140.z;
    let _e147 = base;
    out_color = _e147;
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
