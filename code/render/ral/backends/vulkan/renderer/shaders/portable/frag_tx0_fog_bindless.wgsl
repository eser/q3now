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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e72 + 0.5f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e79 = fogType;
    let _e82 = fogType;
    return (((_e77 > 0.5f) && (_e79 >= 1i)) && (_e82 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e72 = wired_advanced_fog_enabled_u0028_();
    if !(_e72) {
        return 0f;
    }
    let _e75 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e75, 0.000001f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e80 + 0.5f));
    let _e83 = fogType_1;
    if (_e83 == 1i) {
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e87 <= 0f) {
            return 0f;
        }
        let _e89 = viewDepth;
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e89 / _e92), 0f, 1f);
    }
    let _e97 = unnamed.advancedFogColorDensity[3u];
    let _e99 = viewDepth;
    opticalDepth = (max(_e97, 0f) * _e99);
    let _e101 = fogType_1;
    if (_e101 == 2i) {
        let _e103 = opticalDepth;
        return clamp((1f - exp(-(_e103))), 0f, 1f);
    }
    let _e108 = opticalDepth;
    let _e109 = opticalDepth;
    return clamp((1f - exp(-((_e108 * _e109)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e72 = (*rgb);
    let _e75 = unnamed.worldLightParams[0u];
    boosted = (_e72 * _e75);
    let _e78 = boosted[0u];
    let _e80 = boosted[1u];
    let _e82 = boosted[2u];
    peak = max(_e78, max(_e80, _e82));
    let _e85 = peak;
    if (_e85 > 1f) {
        let _e87 = peak;
        let _e88 = boosted;
        boosted = (_e88 / vec3(_e87));
    }
    let _e91 = boosted;
    return _e91;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e73 = (*c);
    (*c) = max(_e73, vec3<f32>(0f, 0f, 0f));
    let _e75 = (*c);
    cutoff = (_e75 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e77 = (*c);
    lo = (_e77 / vec3(12.92f));
    let _e80 = (*c);
    hi = pow(((_e80 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e85 = hi;
    let _e86 = lo;
    let _e87 = cutoff;
    return mix(_e85, _e86, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e87));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e116 = c_1;
        param_1 = _e116.xyz;
        let _e118 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = c_1;
    return _e125;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e85 = unnamed.packed_indices[0i][3u];
    let _e91 = unnamed.packed_indices[0i][3u];
    let _e96 = fog_tex_coord_1;
    let _e97 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e91 >> bitcast<u32>(12i)) & 255u)], _e96);
    fog = _e97;
    let _e98 = frag_color0In_1;
    param_2 = _e98.xyz;
    let _e100 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e102 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e100.x, _e100.y, _e100.z, _e102);
    param_3 = 0u;
    let _e107 = frag_tex_coord0_1;
    param_4 = _e107;
    param_5 = 0i;
    let _e108 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e108;
    let _e109 = sampledColor0_;
    let _e110 = frag_color0_;
    color0_ = (_e109 * _e110);
    let _e112 = color0_;
    base = _e112;
    if override_type_3_ {
        let _e114 = color0_[3u];
        if (_e114 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e117 = color0_[3u];
            if (_e117 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e120 = color0_[3u];
                if (_e120 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e122 = color0_;
    base = _e122;
    if override_type_3_3 {
        let _e125 = unnamed.worldLightParams[1u];
        wetness = clamp(_e125, 0f, 1f);
        let _e129 = unnamed.worldLightParams[2u];
        frost = clamp(_e129, 0f, 1f);
        let _e131 = base;
        luminance = dot(_e131.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e134 = wetness;
        let _e136 = base;
        let _e138 = (_e136.xyz * mix(1f, 0.82f, _e134));
        base[0u] = _e138.x;
        base[1u] = _e138.y;
        base[2u] = _e138.z;
        let _e145 = base;
        let _e147 = luminance;
        let _e149 = luminance;
        let _e151 = luminance;
        let _e153 = frost;
        let _e156 = mix(_e145.xyz, vec3<f32>((_e147 * 0.88f), (_e149 * 0.94f), _e151), vec3((_e153 * 0.55f)));
        base[0u] = _e156.x;
        base[1u] = _e156.y;
        base[2u] = _e156.z;
    }
    let _e165 = unnamed.materialFactors[0u];
    if (_e165 >= 0f) {
        let _e168 = unnamed.materialBaseColor;
        let _e169 = base;
        base = (_e169 * _e168);
    }
    let _e171 = color0_;
    let _e174 = unnamed.emissionRadiance;
    let _e177 = base;
    let _e179 = (_e177.xyz + (_e171.xyz * _e174.xyz));
    base[0u] = _e179.x;
    base[1u] = _e179.y;
    base[2u] = _e179.z;
    let _e186 = wired_advanced_fog_enabled_u0028_();
    if _e186 {
        let _e187 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e187;
        if override_type_3_4 {
            let _e188 = fogAmount;
            let _e190 = base;
            let _e192 = (_e190.xyz * (1f - _e188));
            base[0u] = _e192.x;
            base[1u] = _e192.y;
            base[2u] = _e192.z;
        } else {
            if override_type_3_5 {
                let _e199 = fogAmount;
                let _e201 = base;
                base = (_e201 * (1f - _e199));
            } else {
                if override_type_3_6 {
                    let _e203 = fogAmount;
                    let _e206 = base[3u];
                    base[3u] = (_e206 * (1f - _e203));
                } else {
                    let _e209 = base;
                    let _e212 = unnamed.advancedFogColorDensity;
                    let _e214 = fogAmount;
                    let _e216 = mix(_e209.xyz, _e212.xyz, vec3(_e214));
                    base[0u] = _e216.x;
                    base[1u] = _e216.y;
                    base[2u] = _e216.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e223 = base;
            let _e226 = fog[3u];
            let _e228 = (_e223.xyz * (1f - _e226));
            base[0u] = _e228.x;
            base[1u] = _e228.y;
            base[2u] = _e228.z;
        } else {
            if override_type_3_8 {
                let _e235 = base;
                let _e237 = fog[3u];
                base = (_e235 * (1f - _e237));
            } else {
                if override_type_3_9 {
                    let _e241 = base[3u];
                    let _e243 = fog[3u];
                    base[3u] = (_e241 * (1f - _e243));
                } else {
                    let _e247 = base;
                    let _e248 = fog;
                    let _e250 = unnamed.fogColor;
                    let _e253 = fog[3u];
                    base = mix(_e247, (_e248 * _e250), vec4(_e253));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e257 = base[3u];
        if (_e257 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e259 = base;
            let _e261 = base;
            if (dot(_e259.xyz, _e261.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e265 = base;
    out_color = _e265;
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
