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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e58 = (*c);
    (*c) = max(_e58, vec3<f32>(0f, 0f, 0f));
    let _e60 = (*c);
    cutoff = (_e60 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e62 = (*c);
    lo = (_e62 / vec3(12.92f));
    let _e65 = (*c);
    hi = pow(((_e65 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e70 = hi;
    let _e71 = lo;
    let _e72 = cutoff;
    return mix(_e70, _e71, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e72));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e59 = (*role);
    let _e61 = (*role);
    let _e66 = unnamed.packed_indices[(_e59 / 4u)][(_e61 % 4u)];
    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e81 = (*uv);
    let _e82 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e76 >> bitcast<u32>(12i)) & 255u)], _e81);
    c_1 = _e82;
    let _e83 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e83))) == 0i) {
        let _e88 = c_1;
        param = _e88.xyz;
        let _e90 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e90.x;
        c_1[1u] = _e90.y;
        c_1[2u] = _e90.z;
    }
    let _e97 = (*slot);
    if (lightmap_slot == (_e97 + 1i)) {
        let _e102 = unnamed.worldLightParams[0u];
        let _e103 = c_1;
        let _e105 = (_e103.xyz * _e102);
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = c_1;
    return _e112;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e69 = unnamed.packed_indices[0i][3u];
    let _e75 = unnamed.packed_indices[0i][3u];
    let _e80 = fog_tex_coord_1;
    let _e81 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e75 >> bitcast<u32>(12i)) & 255u)], _e80);
    fog = _e81;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e86 = frag_tex_coord0_1;
    param_2 = _e86;
    param_3 = 0i;
    let _e87 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e88 = frag_color;
    color0_ = (_e87 * _e88);
    let _e90 = color0_;
    base = _e90;
    if override_type_10_ {
        let _e92 = color0_[3u];
        if (_e92 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e95 = color0_[3u];
            if (_e95 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e98 = color0_[3u];
                if (_e98 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e100 = color0_;
    base = _e100;
    if override_type_10_3 {
        let _e101 = base;
        let _e104 = fog[3u];
        let _e106 = (_e101.xyz * (1f - _e104));
        base[0u] = _e106.x;
        base[1u] = _e106.y;
        base[2u] = _e106.z;
    } else {
        if override_type_10_4 {
            let _e113 = base;
            let _e115 = fog[3u];
            base = (_e113 * (1f - _e115));
        } else {
            if override_type_10_5 {
                let _e119 = base[3u];
                let _e121 = fog[3u];
                base[3u] = (_e119 * (1f - _e121));
            } else {
                let _e125 = base;
                let _e126 = fog;
                let _e128 = unnamed.fogColor;
                let _e131 = fog[3u];
                base = mix(_e125, (_e126 * _e128), vec4(_e131));
            }
        }
    }
    if override_type_10_6 {
        let _e135 = base[3u];
        if (_e135 == 0f) {
            discard;
        }
    } else {
        if override_type_10_7 {
            let _e137 = base;
            let _e139 = base;
            if (dot(_e137.xyz, _e139.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e143 = gl_FragCoord_1;
    let _e148 = unnamed.packed_indices[1i][0u];
    let _e154 = unnamed.packed_indices[1i][0u];
    let _e159 = textureDimensions(wired_bindless_images[(_e148 & 4095u)], 0i);
    screenUV = (_e143.xy / vec2<f32>(vec2<i32>(_e159)));
    let _e166 = unnamed.packed_indices[1i][0u];
    let _e172 = unnamed.packed_indices[1i][0u];
    let _e177 = screenUV;
    let _e178 = textureSample(wired_bindless_images[(_e166 & 4095u)], wired_bindless_samplers[((_e172 >> bitcast<u32>(12i)) & 255u)], _e177);
    sceneDepth = _e178.x;
    let _e181 = gl_FragCoord_1[2u];
    fragDepth = _e181;
    let _e182 = fragDepth;
    let _e183 = sceneDepth;
    depthDiff = (_e182 - _e183);
    let _e186 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e186);
    let _e188 = fadeFactor;
    let _e190 = base[3u];
    base[3u] = (_e190 * _e188);
    let _e193 = fadeFactor;
    let _e194 = base;
    let _e196 = (_e194.xyz * _e193);
    base[0u] = _e196.x;
    base[1u] = _e196.y;
    base[2u] = _e196.z;
    let _e203 = base;
    out_color = _e203;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e7 = out_color;
    return _e7;
}
