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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e61 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e61 + 0.5f));
    let _e66 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e68 = fogType;
    let _e71 = fogType;
    return (((_e66 > 0.5f) && (_e68 >= 1i)) && (_e71 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e61 = wired_advanced_fog_enabled_u0028_();
    if !(_e61) {
        return 0f;
    }
    let _e64 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e64, 0.000001f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e69 + 0.5f));
    let _e72 = fogType_1;
    if (_e72 == 1i) {
        let _e76 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e76 <= 0f) {
            return 0f;
        }
        let _e78 = viewDepth;
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e78 / _e81), 0f, 1f);
    }
    let _e86 = unnamed.advancedFogColorDensity[3u];
    let _e88 = viewDepth;
    opticalDepth = (max(_e86, 0f) * _e88);
    let _e90 = fogType_1;
    if (_e90 == 2i) {
        let _e92 = opticalDepth;
        return clamp((1f - exp(-(_e92))), 0f, 1f);
    }
    let _e97 = opticalDepth;
    let _e98 = opticalDepth;
    return clamp((1f - exp(-((_e97 * _e98)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c);
    (*c) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_1 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_1;
        param = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_1;
        let _e109 = (_e107.xyz * _e106);
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e73 = unnamed.packed_indices[0i][3u];
    let _e79 = unnamed.packed_indices[0i][3u];
    let _e84 = fog_tex_coord_1;
    let _e85 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    fog = _e85;
    param_1 = 0u;
    let _e86 = frag_tex_coord0_1;
    param_2 = _e86;
    param_3 = 0i;
    let _e87 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e89 = unnamed.ent_color0_;
    color0_ = (_e87 * _e89);
    let _e91 = color0_;
    base = _e91;
    if override_type_3_ {
        let _e93 = color0_[3u];
        if (_e93 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e96 = color0_[3u];
            if (_e96 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e99 = color0_[3u];
                if (_e99 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e101 = color0_;
    base = _e101;
    let _e102 = wired_advanced_fog_enabled_u0028_();
    if _e102 {
        let _e103 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e103;
        if override_type_3_3 {
            let _e104 = fogAmount;
            let _e106 = base;
            let _e108 = (_e106.xyz * (1f - _e104));
            base[0u] = _e108.x;
            base[1u] = _e108.y;
            base[2u] = _e108.z;
        } else {
            if override_type_3_4 {
                let _e115 = fogAmount;
                let _e117 = base;
                base = (_e117 * (1f - _e115));
            } else {
                if override_type_3_5 {
                    let _e119 = fogAmount;
                    let _e122 = base[3u];
                    base[3u] = (_e122 * (1f - _e119));
                } else {
                    let _e125 = base;
                    let _e128 = unnamed.advancedFogColorDensity;
                    let _e130 = fogAmount;
                    let _e132 = mix(_e125.xyz, _e128.xyz, vec3(_e130));
                    base[0u] = _e132.x;
                    base[1u] = _e132.y;
                    base[2u] = _e132.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e139 = base;
            let _e142 = fog[3u];
            let _e144 = (_e139.xyz * (1f - _e142));
            base[0u] = _e144.x;
            base[1u] = _e144.y;
            base[2u] = _e144.z;
        } else {
            if override_type_3_7 {
                let _e151 = base;
                let _e153 = fog[3u];
                base = (_e151 * (1f - _e153));
            } else {
                if override_type_3_8 {
                    let _e157 = base[3u];
                    let _e159 = fog[3u];
                    base[3u] = (_e157 * (1f - _e159));
                } else {
                    let _e163 = base;
                    let _e164 = fog;
                    let _e166 = unnamed.fogColor;
                    let _e169 = fog[3u];
                    base = mix(_e163, (_e164 * _e166), vec4(_e169));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e173 = base[3u];
        if (_e173 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e175 = base;
            let _e177 = base;
            if (dot(_e175.xyz, _e177.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e181 = gl_FragCoord_1;
    let _e186 = unnamed.packed_indices[1i][0u];
    let _e192 = unnamed.packed_indices[1i][0u];
    let _e197 = textureDimensions(wired_bindless_images[(_e186 & 4095u)], 0i);
    screenUV = (_e181.xy / vec2<f32>(vec2<i32>(_e197)));
    let _e204 = unnamed.packed_indices[1i][0u];
    let _e210 = unnamed.packed_indices[1i][0u];
    let _e215 = screenUV;
    let _e216 = textureSample(wired_bindless_images[(_e204 & 4095u)], wired_bindless_samplers[((_e210 >> bitcast<u32>(12i)) & 255u)], _e215);
    sceneDepth = _e216.x;
    let _e219 = gl_FragCoord_1[2u];
    fragDepth = _e219;
    let _e220 = fragDepth;
    let _e221 = sceneDepth;
    depthDiff = (_e220 - _e221);
    let _e224 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e224);
    let _e226 = fadeFactor;
    let _e228 = base[3u];
    base[3u] = (_e228 * _e226);
    let _e231 = fadeFactor;
    let _e232 = base;
    let _e234 = (_e232.xyz * _e231);
    base[0u] = _e234.x;
    base[1u] = _e234.y;
    base[2u] = _e234.z;
    let _e241 = base;
    out_color = _e241;
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
