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
@id(10) override acff: i32 = 0i;
override override_type_10_3: bool = (acff == 1i);
override override_type_10_4: bool = (acff == 2i);
override override_type_10_5: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_10_6: bool = (discard_mode == 1i);
override override_type_10_7: bool = (discard_mode == 2i);
@id(11) override depth_fade_scale: f32 = 2f;
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e57 = (*c);
    (*c) = max(_e57, vec3<f32>(0f, 0f, 0f));
    let _e59 = (*c);
    cutoff = (_e59 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e61 = (*c);
    lo = (_e61 / vec3(12.92f));
    let _e64 = (*c);
    hi = pow(((_e64 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e69 = hi;
    let _e70 = lo;
    let _e71 = cutoff;
    return mix(_e69, _e70, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e71));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e58 = (*role);
    let _e60 = (*role);
    let _e65 = unnamed.packed_indices[(_e58 / 4u)][(_e60 % 4u)];
    let _e68 = (*role);
    let _e70 = (*role);
    let _e75 = unnamed.packed_indices[(_e68 / 4u)][(_e70 % 4u)];
    let _e80 = (*uv);
    let _e81 = textureSample(wired_bindless_images[(_e65 & 4095u)], wired_bindless_samplers[((_e75 >> bitcast<u32>(12i)) & 255u)], _e80);
    c_1 = _e81;
    let _e82 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e82))) == 0i) {
        let _e87 = c_1;
        param = _e87.xyz;
        let _e89 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e89.x;
        c_1[1u] = _e89.y;
        c_1[2u] = _e89.z;
    }
    let _e96 = (*slot);
    if (lightmap_slot == (_e96 + 1i)) {
        let _e101 = unnamed.worldLightParams[0u];
        let _e102 = c_1;
        let _e104 = (_e102.xyz * _e101);
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = c_1;
    return _e111;
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
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    let _e69 = unnamed.packed_indices[0i][3u];
    let _e75 = unnamed.packed_indices[0i][3u];
    let _e80 = fog_tex_coord_1;
    let _e81 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e75 >> bitcast<u32>(12i)) & 255u)], _e80);
    fog = _e81;
    let _e82 = frag_color0In_1;
    param_1 = _e82.xyz;
    let _e84 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e86 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e84.x, _e84.y, _e84.z, _e86);
    param_2 = 0u;
    let _e91 = frag_tex_coord0_1;
    param_3 = _e91;
    param_4 = 0i;
    let _e92 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e93 = frag_color0_;
    color0_ = (_e92 * _e93);
    let _e95 = color0_;
    base = _e95;
    if override_type_10_ {
        let _e97 = color0_[3u];
        if (_e97 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e100 = color0_[3u];
            if (_e100 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e103 = color0_[3u];
                if (_e103 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e105 = color0_;
    base = _e105;
    if override_type_10_3 {
        let _e106 = base;
        let _e109 = fog[3u];
        let _e111 = (_e106.xyz * (1f - _e109));
        base[0u] = _e111.x;
        base[1u] = _e111.y;
        base[2u] = _e111.z;
    } else {
        if override_type_10_4 {
            let _e118 = base;
            let _e120 = fog[3u];
            base = (_e118 * (1f - _e120));
        } else {
            if override_type_10_5 {
                let _e124 = base[3u];
                let _e126 = fog[3u];
                base[3u] = (_e124 * (1f - _e126));
            } else {
                let _e130 = base;
                let _e131 = fog;
                let _e133 = unnamed.fogColor;
                let _e136 = fog[3u];
                base = mix(_e130, (_e131 * _e133), vec4(_e136));
            }
        }
    }
    if override_type_10_6 {
        let _e140 = base[3u];
        if (_e140 == 0f) {
            discard;
        }
    } else {
        if override_type_10_7 {
            let _e142 = base;
            let _e144 = base;
            if (dot(_e142.xyz, _e144.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e148 = gl_FragCoord_1;
    let _e153 = unnamed.packed_indices[1i][0u];
    let _e159 = unnamed.packed_indices[1i][0u];
    let _e164 = textureDimensions(wired_bindless_images[(_e153 & 4095u)], 0i);
    screenUV = (_e148.xy / vec2<f32>(vec2<i32>(_e164)));
    let _e171 = unnamed.packed_indices[1i][0u];
    let _e177 = unnamed.packed_indices[1i][0u];
    let _e182 = screenUV;
    let _e183 = textureSample(wired_bindless_images[(_e171 & 4095u)], wired_bindless_samplers[((_e177 >> bitcast<u32>(12i)) & 255u)], _e182);
    sceneDepth = _e183.x;
    let _e186 = gl_FragCoord_1[2u];
    fragDepth = _e186;
    let _e187 = fragDepth;
    let _e188 = sceneDepth;
    depthDiff = (_e187 - _e188);
    let _e191 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e191);
    let _e193 = fadeFactor;
    let _e195 = base[3u];
    base[3u] = (_e195 * _e193);
    let _e198 = fadeFactor;
    let _e199 = base;
    let _e201 = (_e199.xyz * _e198);
    base[0u] = _e201.x;
    base[1u] = _e201.y;
    base[2u] = _e201.z;
    let _e208 = base;
    out_color = _e208;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e9 = out_color;
    return _e9;
}
