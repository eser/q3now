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
    emissionRadiance: vec4<f32>,
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_4: bool = (acff == 1i);
override override_type_3_5: bool = (acff == 2i);
override override_type_3_6: bool = (acff == 3i);
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_10: bool = (discard_mode == 1i);
override override_type_3_11: bool = (discard_mode == 2i);
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

    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e71 + 0.5f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e78 = fogType;
    let _e81 = fogType;
    return (((_e76 > 0.5f) && (_e78 >= 1i)) && (_e81 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e71 = wired_advanced_fog_enabled_u0028_();
    if !(_e71) {
        return 0f;
    }
    let _e74 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e74, 0.000001f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e79 + 0.5f));
    let _e82 = fogType_1;
    if (_e82 == 1i) {
        let _e86 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e86 <= 0f) {
            return 0f;
        }
        let _e88 = viewDepth;
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e88 / _e91), 0f, 1f);
    }
    let _e96 = unnamed.advancedFogColorDensity[3u];
    let _e98 = viewDepth;
    opticalDepth = (max(_e96, 0f) * _e98);
    let _e100 = fogType_1;
    if (_e100 == 2i) {
        let _e102 = opticalDepth;
        return clamp((1f - exp(-(_e102))), 0f, 1f);
    }
    let _e107 = opticalDepth;
    let _e108 = opticalDepth;
    return clamp((1f - exp(-((_e107 * _e108)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e71 = (*rgb);
    let _e74 = unnamed.worldLightParams[0u];
    boosted = (_e71 * _e74);
    let _e77 = boosted[0u];
    let _e79 = boosted[1u];
    let _e81 = boosted[2u];
    peak = max(_e77, max(_e79, _e81));
    let _e84 = peak;
    if (_e84 > 1f) {
        let _e86 = peak;
        let _e87 = boosted;
        boosted = (_e87 / vec3(_e86));
    }
    let _e90 = boosted;
    return _e90;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e72 = (*c);
    (*c) = max(_e72, vec3<f32>(0f, 0f, 0f));
    let _e74 = (*c);
    cutoff = (_e74 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e76 = (*c);
    lo = (_e76 / vec3(12.92f));
    let _e79 = (*c);
    hi = pow(((_e79 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e84 = hi;
    let _e85 = lo;
    let _e86 = cutoff;
    return mix(_e84, _e85, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e86));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e96 = (*uv);
    let _e97 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e91 >> bitcast<u32>(12i)) & 255u)], _e96);
    c_1 = _e97;
    let _e98 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e98))) == 0i) {
        let _e103 = c_1;
        param = _e103.xyz;
        let _e105 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = (*slot);
    if (lightmap_slot == (_e112 + 1i)) {
        let _e115 = c_1;
        param_1 = _e115.xyz;
        let _e117 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e82 = unnamed.packed_indices[0i][3u];
    let _e88 = unnamed.packed_indices[0i][3u];
    let _e93 = fog_tex_coord_1;
    let _e94 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    fog = _e94;
    param_2 = 0u;
    let _e95 = frag_tex_coord0_1;
    param_3 = _e95;
    param_4 = 0i;
    let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e96;
    let _e97 = sampledColor0_;
    color0_ = _e97;
    let _e98 = color0_;
    base = _e98;
    if override_type_3_ {
        let _e100 = color0_[3u];
        if (_e100 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e103 = color0_[3u];
            if (_e103 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e106 = color0_[3u];
                if (_e106 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e108 = color0_;
    base = _e108;
    if override_type_3_3 {
        let _e111 = unnamed.worldLightParams[1u];
        wetness = clamp(_e111, 0f, 1f);
        let _e115 = unnamed.worldLightParams[2u];
        frost = clamp(_e115, 0f, 1f);
        let _e117 = base;
        luminance = dot(_e117.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e120 = wetness;
        let _e122 = base;
        let _e124 = (_e122.xyz * mix(1f, 0.82f, _e120));
        base[0u] = _e124.x;
        base[1u] = _e124.y;
        base[2u] = _e124.z;
        let _e131 = base;
        let _e133 = luminance;
        let _e135 = luminance;
        let _e137 = luminance;
        let _e139 = frost;
        let _e142 = mix(_e131.xyz, vec3<f32>((_e133 * 0.88f), (_e135 * 0.94f), _e137), vec3((_e139 * 0.55f)));
        base[0u] = _e142.x;
        base[1u] = _e142.y;
        base[2u] = _e142.z;
    }
    let _e151 = unnamed.materialFactors[0u];
    if (_e151 >= 0f) {
        let _e154 = unnamed.materialBaseColor;
        let _e155 = base;
        base = (_e155 * _e154);
    }
    let _e157 = color0_;
    let _e160 = unnamed.emissionRadiance;
    let _e163 = base;
    let _e165 = (_e163.xyz + (_e157.xyz * _e160.xyz));
    base[0u] = _e165.x;
    base[1u] = _e165.y;
    base[2u] = _e165.z;
    let _e172 = wired_advanced_fog_enabled_u0028_();
    if _e172 {
        let _e173 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e173;
        if override_type_3_4 {
            let _e174 = fogAmount;
            let _e176 = base;
            let _e178 = (_e176.xyz * (1f - _e174));
            base[0u] = _e178.x;
            base[1u] = _e178.y;
            base[2u] = _e178.z;
        } else {
            if override_type_3_5 {
                let _e185 = fogAmount;
                let _e187 = base;
                base = (_e187 * (1f - _e185));
            } else {
                if override_type_3_6 {
                    let _e189 = fogAmount;
                    let _e192 = base[3u];
                    base[3u] = (_e192 * (1f - _e189));
                } else {
                    let _e195 = base;
                    let _e198 = unnamed.advancedFogColorDensity;
                    let _e200 = fogAmount;
                    let _e202 = mix(_e195.xyz, _e198.xyz, vec3(_e200));
                    base[0u] = _e202.x;
                    base[1u] = _e202.y;
                    base[2u] = _e202.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e209 = base;
            let _e212 = fog[3u];
            let _e214 = (_e209.xyz * (1f - _e212));
            base[0u] = _e214.x;
            base[1u] = _e214.y;
            base[2u] = _e214.z;
        } else {
            if override_type_3_8 {
                let _e221 = base;
                let _e223 = fog[3u];
                base = (_e221 * (1f - _e223));
            } else {
                if override_type_3_9 {
                    let _e227 = base[3u];
                    let _e229 = fog[3u];
                    base[3u] = (_e227 * (1f - _e229));
                } else {
                    let _e233 = base;
                    let _e234 = fog;
                    let _e236 = unnamed.fogColor;
                    let _e239 = fog[3u];
                    base = mix(_e233, (_e234 * _e236), vec4(_e239));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e243 = base[3u];
        if (_e243 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e245 = base;
            let _e247 = base;
            if (dot(_e245.xyz, _e247.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e251 = base;
    out_color = _e251;
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
