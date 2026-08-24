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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e62 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e62 + 0.5f));
    let _e67 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e69 = fogType;
    let _e72 = fogType;
    return (((_e67 > 0.5f) && (_e69 >= 1i)) && (_e72 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e62 = wired_advanced_fog_enabled_u0028_();
    if !(_e62) {
        return 0f;
    }
    let _e65 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e65, 0.000001f));
    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e70 + 0.5f));
    let _e73 = fogType_1;
    if (_e73 == 1i) {
        let _e77 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e77 <= 0f) {
            return 0f;
        }
        let _e79 = viewDepth;
        let _e82 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e79 / _e82), 0f, 1f);
    }
    let _e87 = unnamed.advancedFogColorDensity[3u];
    let _e89 = viewDepth;
    opticalDepth = (max(_e87, 0f) * _e89);
    let _e91 = fogType_1;
    if (_e91 == 2i) {
        let _e93 = opticalDepth;
        return clamp((1f - exp(-(_e93))), 0f, 1f);
    }
    let _e98 = opticalDepth;
    let _e99 = opticalDepth;
    return clamp((1f - exp(-((_e98 * _e99)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e63 = (*c);
    (*c) = max(_e63, vec3<f32>(0f, 0f, 0f));
    let _e65 = (*c);
    cutoff = (_e65 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e67 = (*c);
    lo = (_e67 / vec3(12.92f));
    let _e70 = (*c);
    hi = pow(((_e70 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e75 = hi;
    let _e76 = lo;
    let _e77 = cutoff;
    return mix(_e75, _e76, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e77));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e64 = (*role);
    let _e66 = (*role);
    let _e71 = unnamed.packed_indices[(_e64 / 4u)][(_e66 % 4u)];
    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e86 = (*uv);
    let _e87 = textureSample(wired_bindless_images[(_e71 & 4095u)], wired_bindless_samplers[((_e81 >> bitcast<u32>(12i)) & 255u)], _e86);
    c_1 = _e87;
    let _e88 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e88))) == 0i) {
        let _e93 = c_1;
        param = _e93.xyz;
        let _e95 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e95.x;
        c_1[1u] = _e95.y;
        c_1[2u] = _e95.z;
    }
    let _e102 = (*slot);
    if (lightmap_slot == (_e102 + 1i)) {
        let _e107 = unnamed.worldLightParams[0u];
        let _e108 = c_1;
        let _e110 = (_e108.xyz * _e107);
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = c_1;
    return _e117;
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
    let _e89 = frag_color0In_1;
    param_1 = _e89.xyz;
    let _e91 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e93 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e91.x, _e91.y, _e91.z, _e93);
    param_2 = 0u;
    let _e98 = frag_tex_coord0_1;
    param_3 = _e98;
    param_4 = 0i;
    let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e100 = frag_color0_;
    color0_ = (_e99 * _e100);
    let _e102 = color0_;
    base = _e102;
    if override_type_3_ {
        let _e104 = color0_[3u];
        if (_e104 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e107 = color0_[3u];
            if (_e107 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e110 = color0_[3u];
                if (_e110 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e112 = color0_;
    base = _e112;
    let _e113 = wired_advanced_fog_enabled_u0028_();
    if _e113 {
        let _e114 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e114;
        if override_type_3_3 {
            let _e115 = fogAmount;
            let _e117 = base;
            let _e119 = (_e117.xyz * (1f - _e115));
            base[0u] = _e119.x;
            base[1u] = _e119.y;
            base[2u] = _e119.z;
        } else {
            if override_type_3_4 {
                let _e126 = fogAmount;
                let _e128 = base;
                base = (_e128 * (1f - _e126));
            } else {
                if override_type_3_5 {
                    let _e130 = fogAmount;
                    let _e133 = base[3u];
                    base[3u] = (_e133 * (1f - _e130));
                } else {
                    let _e136 = base;
                    let _e139 = unnamed.advancedFogColorDensity;
                    let _e141 = fogAmount;
                    let _e143 = mix(_e136.xyz, _e139.xyz, vec3(_e141));
                    base[0u] = _e143.x;
                    base[1u] = _e143.y;
                    base[2u] = _e143.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e150 = base;
            let _e153 = fog[3u];
            let _e155 = (_e150.xyz * (1f - _e153));
            base[0u] = _e155.x;
            base[1u] = _e155.y;
            base[2u] = _e155.z;
        } else {
            if override_type_3_7 {
                let _e162 = base;
                let _e164 = fog[3u];
                base = (_e162 * (1f - _e164));
            } else {
                if override_type_3_8 {
                    let _e168 = base[3u];
                    let _e170 = fog[3u];
                    base[3u] = (_e168 * (1f - _e170));
                } else {
                    let _e174 = base;
                    let _e175 = fog;
                    let _e177 = unnamed.fogColor;
                    let _e180 = fog[3u];
                    base = mix(_e174, (_e175 * _e177), vec4(_e180));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e184 = base[3u];
        if (_e184 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e186 = base;
            let _e188 = base;
            if (dot(_e186.xyz, _e188.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e192 = gl_FragCoord_1;
    let _e197 = unnamed.packed_indices[1i][0u];
    let _e203 = unnamed.packed_indices[1i][0u];
    let _e208 = textureDimensions(wired_bindless_images[(_e197 & 4095u)], 0i);
    screenUV = (_e192.xy / vec2<f32>(vec2<i32>(_e208)));
    let _e215 = unnamed.packed_indices[1i][0u];
    let _e221 = unnamed.packed_indices[1i][0u];
    let _e226 = screenUV;
    let _e227 = textureSample(wired_bindless_images[(_e215 & 4095u)], wired_bindless_samplers[((_e221 >> bitcast<u32>(12i)) & 255u)], _e226);
    sceneDepth = _e227.x;
    let _e230 = gl_FragCoord_1[2u];
    fragDepth = _e230;
    let _e231 = fragDepth;
    let _e232 = sceneDepth;
    depthDiff = (_e231 - _e232);
    let _e235 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e235);
    let _e237 = fadeFactor;
    let _e239 = base[3u];
    base[3u] = (_e239 * _e237);
    let _e242 = fadeFactor;
    let _e243 = base;
    let _e245 = (_e243.xyz * _e242);
    base[0u] = _e245.x;
    base[1u] = _e245.y;
    base[2u] = _e245.z;
    let _e252 = base;
    out_color = _e252;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
