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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
@id(10) override acff: i32 = 0i;
override override_type_3_3: bool = (acff == 1i);
override override_type_3_4: bool = (acff == 2i);
override override_type_3_5: bool = (acff == 3i);
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
@id(11) override depth_fade_scale: f32 = 2f;
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e63 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e63 + 0.5f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e70 = fogType;
    let _e73 = fogType;
    return (((_e68 > 0.5f) && (_e70 >= 1i)) && (_e73 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e63 = wired_advanced_fog_enabled_u0028_();
    if !(_e63) {
        return 0f;
    }
    let _e66 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e66, 0.000001f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e71 + 0.5f));
    let _e74 = fogType_1;
    if (_e74 == 1i) {
        let _e78 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e78 <= 0f) {
            return 0f;
        }
        let _e80 = viewDepth;
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e80 / _e83), 0f, 1f);
    }
    let _e88 = unnamed.advancedFogColorDensity[3u];
    let _e90 = viewDepth;
    opticalDepth = (max(_e88, 0f) * _e90);
    let _e92 = fogType_1;
    if (_e92 == 2i) {
        let _e94 = opticalDepth;
        return clamp((1f - exp(-(_e94))), 0f, 1f);
    }
    let _e99 = opticalDepth;
    let _e100 = opticalDepth;
    return clamp((1f - exp(-((_e99 * _e100)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c);
    (*c) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e87 = (*uv);
    let _e88 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    c_1 = _e88;
    let _e89 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e89))) == 0i) {
        let _e94 = c_1;
        param = _e94.xyz;
        let _e96 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e96.x;
        c_1[1u] = _e96.y;
        c_1[2u] = _e96.z;
    }
    let _e103 = (*slot);
    if (lightmap_slot == (_e103 + 1i)) {
        let _e108 = unnamed.worldLightParams[0u];
        let _e109 = c_1;
        let _e111 = (_e109.xyz * _e108);
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = c_1;
    return _e118;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var fogAmount: f32;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    let _e76 = unnamed.packed_indices[0i][3u];
    let _e82 = unnamed.packed_indices[0i][3u];
    let _e87 = fog_tex_coord_1;
    let _e88 = textureSample(wired_bindless_images[(_e76 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    fog = _e88;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e93 = frag_tex_coord0_1;
    param_2 = _e93;
    param_3 = 0i;
    let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e95 = frag_color;
    color0_ = (_e94 * _e95);
    let _e97 = color0_;
    base = _e97;
    if override_type_3_ {
        let _e99 = color0_[3u];
        if (_e99 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e102 = color0_[3u];
            if (_e102 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e105 = color0_[3u];
                if (_e105 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e107 = color0_;
    base = _e107;
    let _e108 = wired_advanced_fog_enabled_u0028_();
    if _e108 {
        let _e109 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e109;
        if override_type_3_3 {
            let _e110 = fogAmount;
            let _e112 = base;
            let _e114 = (_e112.xyz * (1f - _e110));
            base[0u] = _e114.x;
            base[1u] = _e114.y;
            base[2u] = _e114.z;
        } else {
            if override_type_3_4 {
                let _e121 = fogAmount;
                let _e123 = base;
                base = (_e123 * (1f - _e121));
            } else {
                if override_type_3_5 {
                    let _e125 = fogAmount;
                    let _e128 = base[3u];
                    base[3u] = (_e128 * (1f - _e125));
                } else {
                    let _e131 = base;
                    let _e134 = unnamed.advancedFogColorDensity;
                    let _e136 = fogAmount;
                    let _e138 = mix(_e131.xyz, _e134.xyz, vec3(_e136));
                    base[0u] = _e138.x;
                    base[1u] = _e138.y;
                    base[2u] = _e138.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e145 = base;
            let _e148 = fog[3u];
            let _e150 = (_e145.xyz * (1f - _e148));
            base[0u] = _e150.x;
            base[1u] = _e150.y;
            base[2u] = _e150.z;
        } else {
            if override_type_3_7 {
                let _e157 = base;
                let _e159 = fog[3u];
                base = (_e157 * (1f - _e159));
            } else {
                if override_type_3_8 {
                    let _e163 = base[3u];
                    let _e165 = fog[3u];
                    base[3u] = (_e163 * (1f - _e165));
                } else {
                    let _e169 = base;
                    let _e170 = fog;
                    let _e172 = unnamed.fogColor;
                    let _e175 = fog[3u];
                    base = mix(_e169, (_e170 * _e172), vec4(_e175));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e179 = base[3u];
        if (_e179 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e181 = base;
            let _e183 = base;
            if (dot(_e181.xyz, _e183.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e187 = gl_FragCoord_1;
    let _e192 = unnamed.packed_indices[1i][0u];
    let _e198 = unnamed.packed_indices[1i][0u];
    let _e203 = textureDimensions(wired_bindless_images[(_e192 & 4095u)], 0i);
    screenUV = (_e187.xy / vec2<f32>(vec2<i32>(_e203)));
    let _e210 = unnamed.packed_indices[1i][0u];
    let _e216 = unnamed.packed_indices[1i][0u];
    let _e221 = screenUV;
    let _e222 = textureSample(wired_bindless_images[(_e210 & 4095u)], wired_bindless_samplers[((_e216 >> bitcast<u32>(12i)) & 255u)], _e221);
    sceneDepth = _e222.x;
    let _e225 = gl_FragCoord_1[2u];
    fragDepth = _e225;
    let _e226 = fragDepth;
    let _e227 = sceneDepth;
    depthDiff = (_e226 - _e227);
    let _e230 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e230);
    let _e232 = fadeFactor;
    let _e234 = base[3u];
    base[3u] = (_e234 * _e232);
    let _e237 = fadeFactor;
    let _e238 = base;
    let _e240 = (_e238.xyz * _e237);
    base[0u] = _e240.x;
    base[1u] = _e240.y;
    base[2u] = _e240.z;
    let _e247 = base;
    out_color = _e247;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
