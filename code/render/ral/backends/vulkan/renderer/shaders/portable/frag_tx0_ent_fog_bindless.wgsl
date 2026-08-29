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
    emissionRadiance: vec4<f32>,
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

    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e69 + 0.5f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e76 = fogType;
    let _e79 = fogType;
    return (((_e74 > 0.5f) && (_e76 >= 1i)) && (_e79 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e69 = wired_advanced_fog_enabled_u0028_();
    if !(_e69) {
        return 0f;
    }
    let _e72 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e72, 0.000001f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e77 + 0.5f));
    let _e80 = fogType_1;
    if (_e80 == 1i) {
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e84 <= 0f) {
            return 0f;
        }
        let _e86 = viewDepth;
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e86 / _e89), 0f, 1f);
    }
    let _e94 = unnamed.advancedFogColorDensity[3u];
    let _e96 = viewDepth;
    opticalDepth = (max(_e94, 0f) * _e96);
    let _e98 = fogType_1;
    if (_e98 == 2i) {
        let _e100 = opticalDepth;
        return clamp((1f - exp(-(_e100))), 0f, 1f);
    }
    let _e105 = opticalDepth;
    let _e106 = opticalDepth;
    return clamp((1f - exp(-((_e105 * _e106)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c);
    (*c) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_1 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_1;
        param = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e114 = unnamed.worldLightParams[0u];
        let _e115 = c_1;
        let _e117 = (_e115.xyz * _e114);
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e79 = unnamed.packed_indices[0i][3u];
    let _e85 = unnamed.packed_indices[0i][3u];
    let _e90 = fog_tex_coord_1;
    let _e91 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e85 >> bitcast<u32>(12i)) & 255u)], _e90);
    fog = _e91;
    param_1 = 0u;
    let _e92 = frag_tex_coord0_1;
    param_2 = _e92;
    param_3 = 0i;
    let _e93 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e95 = unnamed.ent_color0_;
    color0_ = (_e93 * _e95);
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
    if override_type_3_3 {
        let _e110 = unnamed.worldLightParams[1u];
        wetness = clamp(_e110, 0f, 1f);
        let _e114 = unnamed.worldLightParams[2u];
        frost = clamp(_e114, 0f, 1f);
        let _e116 = base;
        luminance = dot(_e116.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e119 = wetness;
        let _e121 = base;
        let _e123 = (_e121.xyz * mix(1f, 0.82f, _e119));
        base[0u] = _e123.x;
        base[1u] = _e123.y;
        base[2u] = _e123.z;
        let _e130 = base;
        let _e132 = luminance;
        let _e134 = luminance;
        let _e136 = luminance;
        let _e138 = frost;
        let _e141 = mix(_e130.xyz, vec3<f32>((_e132 * 0.88f), (_e134 * 0.94f), _e136), vec3((_e138 * 0.55f)));
        base[0u] = _e141.x;
        base[1u] = _e141.y;
        base[2u] = _e141.z;
    }
    let _e148 = color0_;
    let _e151 = unnamed.emissionRadiance;
    let _e154 = base;
    let _e156 = (_e154.xyz + (_e148.xyz * _e151.xyz));
    base[0u] = _e156.x;
    base[1u] = _e156.y;
    base[2u] = _e156.z;
    let _e163 = wired_advanced_fog_enabled_u0028_();
    if _e163 {
        let _e164 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e164;
        if override_type_3_4 {
            let _e165 = fogAmount;
            let _e167 = base;
            let _e169 = (_e167.xyz * (1f - _e165));
            base[0u] = _e169.x;
            base[1u] = _e169.y;
            base[2u] = _e169.z;
        } else {
            if override_type_3_5 {
                let _e176 = fogAmount;
                let _e178 = base;
                base = (_e178 * (1f - _e176));
            } else {
                if override_type_3_6 {
                    let _e180 = fogAmount;
                    let _e183 = base[3u];
                    base[3u] = (_e183 * (1f - _e180));
                } else {
                    let _e186 = base;
                    let _e189 = unnamed.advancedFogColorDensity;
                    let _e191 = fogAmount;
                    let _e193 = mix(_e186.xyz, _e189.xyz, vec3(_e191));
                    base[0u] = _e193.x;
                    base[1u] = _e193.y;
                    base[2u] = _e193.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e200 = base;
            let _e203 = fog[3u];
            let _e205 = (_e200.xyz * (1f - _e203));
            base[0u] = _e205.x;
            base[1u] = _e205.y;
            base[2u] = _e205.z;
        } else {
            if override_type_3_8 {
                let _e212 = base;
                let _e214 = fog[3u];
                base = (_e212 * (1f - _e214));
            } else {
                if override_type_3_9 {
                    let _e218 = base[3u];
                    let _e220 = fog[3u];
                    base[3u] = (_e218 * (1f - _e220));
                } else {
                    let _e224 = base;
                    let _e225 = fog;
                    let _e227 = unnamed.fogColor;
                    let _e230 = fog[3u];
                    base = mix(_e224, (_e225 * _e227), vec4(_e230));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e234 = base[3u];
        if (_e234 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e236 = base;
            let _e238 = base;
            if (dot(_e236.xyz, _e238.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e242 = base;
    out_color = _e242;
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
