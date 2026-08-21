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

    let _e66 = unnamed.packed_indices[0i][3u];
    let _e72 = unnamed.packed_indices[0i][3u];
    let _e77 = fog_tex_coord_1;
    let _e78 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e72 >> bitcast<u32>(12i)) & 255u)], _e77);
    fog = _e78;
    param_1 = 0u;
    let _e79 = frag_tex_coord0_1;
    param_2 = _e79;
    param_3 = 0i;
    let _e80 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e82 = unnamed.ent_color0_;
    color0_ = (_e80 * _e82);
    let _e84 = color0_;
    base = _e84;
    if override_type_10_ {
        let _e86 = color0_[3u];
        if (_e86 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_10_1 {
            let _e89 = color0_[3u];
            if (_e89 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_10_2 {
                let _e92 = color0_[3u];
                if (_e92 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e94 = color0_;
    base = _e94;
    if override_type_10_3 {
        let _e95 = base;
        let _e98 = fog[3u];
        let _e100 = (_e95.xyz * (1f - _e98));
        base[0u] = _e100.x;
        base[1u] = _e100.y;
        base[2u] = _e100.z;
    } else {
        if override_type_10_4 {
            let _e107 = base;
            let _e109 = fog[3u];
            base = (_e107 * (1f - _e109));
        } else {
            if override_type_10_5 {
                let _e113 = base[3u];
                let _e115 = fog[3u];
                base[3u] = (_e113 * (1f - _e115));
            } else {
                let _e119 = base;
                let _e120 = fog;
                let _e122 = unnamed.fogColor;
                let _e125 = fog[3u];
                base = mix(_e119, (_e120 * _e122), vec4(_e125));
            }
        }
    }
    if override_type_10_6 {
        let _e129 = base[3u];
        if (_e129 == 0f) {
            discard;
        }
    } else {
        if override_type_10_7 {
            let _e131 = base;
            let _e133 = base;
            if (dot(_e131.xyz, _e133.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e137 = gl_FragCoord_1;
    let _e142 = unnamed.packed_indices[1i][0u];
    let _e148 = unnamed.packed_indices[1i][0u];
    let _e153 = textureDimensions(wired_bindless_images[(_e142 & 4095u)], 0i);
    screenUV = (_e137.xy / vec2<f32>(vec2<i32>(_e153)));
    let _e160 = unnamed.packed_indices[1i][0u];
    let _e166 = unnamed.packed_indices[1i][0u];
    let _e171 = screenUV;
    let _e172 = textureSample(wired_bindless_images[(_e160 & 4095u)], wired_bindless_samplers[((_e166 >> bitcast<u32>(12i)) & 255u)], _e171);
    sceneDepth = _e172.x;
    let _e175 = gl_FragCoord_1[2u];
    fragDepth = _e175;
    let _e176 = fragDepth;
    let _e177 = sceneDepth;
    depthDiff = (_e176 - _e177);
    let _e180 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e180);
    let _e182 = fadeFactor;
    let _e184 = base[3u];
    base[3u] = (_e184 * _e182);
    let _e187 = fadeFactor;
    let _e188 = base;
    let _e190 = (_e188.xyz * _e187);
    base[0u] = _e190.x;
    base[1u] = _e190.y;
    base[2u] = _e190.z;
    let _e197 = base;
    out_color = _e197;
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
