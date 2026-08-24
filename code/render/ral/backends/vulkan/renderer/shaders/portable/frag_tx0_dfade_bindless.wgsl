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
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(11) override depth_fade_scale: f32 = 2f;
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e53 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e53 + 0.5f));
    let _e58 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e60 = fogType;
    let _e63 = fogType;
    return (((_e58 > 0.5f) && (_e60 >= 1i)) && (_e63 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e53 = wired_advanced_fog_enabled_u0028_();
    if !(_e53) {
        return 0f;
    }
    let _e56 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e56, 0.000001f));
    let _e61 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e61 + 0.5f));
    let _e64 = fogType_1;
    if (_e64 == 1i) {
        let _e68 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e68 <= 0f) {
            return 0f;
        }
        let _e70 = viewDepth;
        let _e73 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e70 / _e73), 0f, 1f);
    }
    let _e78 = unnamed.advancedFogColorDensity[3u];
    let _e80 = viewDepth;
    opticalDepth = (max(_e78, 0f) * _e80);
    let _e82 = fogType_1;
    if (_e82 == 2i) {
        let _e84 = opticalDepth;
        return clamp((1f - exp(-(_e84))), 0f, 1f);
    }
    let _e89 = opticalDepth;
    let _e90 = opticalDepth;
    return clamp((1f - exp(-((_e89 * _e90)))), 0f, 1f);
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
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    let _e63 = frag_color0In_1;
    param_1 = _e63.xyz;
    let _e65 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e67 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e65.x, _e65.y, _e65.z, _e67);
    param_2 = 0u;
    let _e72 = frag_tex_coord0_1;
    param_3 = _e72;
    param_4 = 0i;
    let _e73 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e74 = frag_color0_;
    color0_ = (_e73 * _e74);
    let _e76 = color0_;
    base = _e76;
    if override_type_3_ {
        let _e78 = color0_[3u];
        if (_e78 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e81 = color0_[3u];
            if (_e81 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e84 = color0_[3u];
                if (_e84 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e86 = color0_;
    base = _e86;
    let _e87 = wired_advanced_fog_enabled_u0028_();
    if _e87 {
        let _e88 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e88;
        let _e89 = base;
        let _e92 = unnamed.advancedFogColorDensity;
        let _e94 = fogAmount;
        let _e96 = mix(_e89.xyz, _e92.xyz, vec3(_e94));
        base[0u] = _e96.x;
        base[1u] = _e96.y;
        base[2u] = _e96.z;
    }
    if override_type_3_3 {
        let _e104 = base[3u];
        if (_e104 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e106 = base;
            let _e108 = base;
            if (dot(_e106.xyz, _e108.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e112 = gl_FragCoord_1;
    let _e117 = unnamed.packed_indices[1i][0u];
    let _e123 = unnamed.packed_indices[1i][0u];
    let _e128 = textureDimensions(wired_bindless_images[(_e117 & 4095u)], 0i);
    screenUV = (_e112.xy / vec2<f32>(vec2<i32>(_e128)));
    let _e135 = unnamed.packed_indices[1i][0u];
    let _e141 = unnamed.packed_indices[1i][0u];
    let _e146 = screenUV;
    let _e147 = textureSample(wired_bindless_images[(_e135 & 4095u)], wired_bindless_samplers[((_e141 >> bitcast<u32>(12i)) & 255u)], _e146);
    sceneDepth = _e147.x;
    let _e150 = gl_FragCoord_1[2u];
    fragDepth = _e150;
    let _e151 = fragDepth;
    let _e152 = sceneDepth;
    depthDiff = (_e151 - _e152);
    let _e155 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e155);
    let _e157 = fadeFactor;
    let _e159 = base[3u];
    base[3u] = (_e159 * _e157);
    let _e162 = fadeFactor;
    let _e163 = base;
    let _e165 = (_e163.xyz * _e162);
    base[0u] = _e165.x;
    base[1u] = _e165.y;
    base[2u] = _e165.z;
    let _e172 = base;
    out_color = _e172;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
