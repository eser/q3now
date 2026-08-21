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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e51 = (*c);
    (*c) = max(_e51, vec3<f32>(0f, 0f, 0f));
    let _e53 = (*c);
    cutoff = (_e53 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e55 = (*c);
    lo = (_e55 / vec3(12.92f));
    let _e58 = (*c);
    hi = pow(((_e58 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e63 = hi;
    let _e64 = lo;
    let _e65 = cutoff;
    return mix(_e63, _e64, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e65));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e52 = (*role);
    let _e54 = (*role);
    let _e59 = unnamed.packed_indices[(_e52 / 4u)][(_e54 % 4u)];
    let _e62 = (*role);
    let _e64 = (*role);
    let _e69 = unnamed.packed_indices[(_e62 / 4u)][(_e64 % 4u)];
    let _e74 = (*uv);
    let _e75 = textureSample(wired_bindless_images[(_e59 & 4095u)], wired_bindless_samplers[((_e69 >> bitcast<u32>(12i)) & 255u)], _e74);
    c_1 = _e75;
    let _e76 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e76))) == 0i) {
        let _e81 = c_1;
        param = _e81.xyz;
        let _e83 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e83.x;
        c_1[1u] = _e83.y;
        c_1[2u] = _e83.z;
    }
    let _e90 = (*slot);
    if (lightmap_slot == (_e90 + 1i)) {
        let _e95 = unnamed.worldLightParams[0u];
        let _e96 = c_1;
        let _e98 = (_e96.xyz * _e95);
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = c_1;
    return _e105;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    let _e59 = frag_color0In_1;
    param_1 = _e59.xyz;
    let _e61 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e63 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e61.x, _e61.y, _e61.z, _e63);
    param_2 = 0u;
    let _e68 = frag_tex_coord0_1;
    param_3 = _e68;
    param_4 = 0i;
    let _e69 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e70 = frag_color0_;
    color0_ = (_e69 * _e70);
    let _e72 = color0_;
    base = _e72;
    if override_type_10_ {
        let _e74 = color0_[3u];
        if (_e74 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e77 = color0_[3u];
            if (_e77 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e80 = color0_[3u];
                if (_e80 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e82 = color0_;
    base = _e82;
    if override_type_10_3 {
        let _e84 = base[3u];
        if (_e84 == 0f) {
            discard;
        }
    } else {
        if override_type_10_4 {
            let _e86 = base;
            let _e88 = base;
            if (dot(_e86.xyz, _e88.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e92 = gl_FragCoord_1;
    let _e97 = unnamed.packed_indices[1i][0u];
    let _e103 = unnamed.packed_indices[1i][0u];
    let _e108 = textureDimensions(wired_bindless_images[(_e97 & 4095u)], 0i);
    screenUV = (_e92.xy / vec2<f32>(vec2<i32>(_e108)));
    let _e115 = unnamed.packed_indices[1i][0u];
    let _e121 = unnamed.packed_indices[1i][0u];
    let _e126 = screenUV;
    let _e127 = textureSample(wired_bindless_images[(_e115 & 4095u)], wired_bindless_samplers[((_e121 >> bitcast<u32>(12i)) & 255u)], _e126);
    sceneDepth = _e127.x;
    let _e130 = gl_FragCoord_1[2u];
    fragDepth = _e130;
    let _e131 = fragDepth;
    let _e132 = sceneDepth;
    depthDiff = (_e131 - _e132);
    let _e135 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e135);
    let _e137 = fadeFactor;
    let _e139 = base[3u];
    base[3u] = (_e139 * _e137);
    let _e142 = fadeFactor;
    let _e143 = base;
    let _e145 = (_e143.xyz * _e142);
    base[0u] = _e145.x;
    base[1u] = _e145.y;
    base[2u] = _e145.z;
    let _e152 = base;
    out_color = _e152;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e7 = out_color;
    return _e7;
}
