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

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

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
        let _e118 = unnamed.worldLightParams[0u];
        let _e119 = c_1;
        let _e121 = (_e119.xyz * _e118);
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = c_1;
    return _e128;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var base: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var screenUV: vec2<f32>;
    var sceneDepth: f32;
    var fragDepth: f32;
    var depthDiff: f32;
    var fadeFactor: f32;

    let _e89 = unnamed.packed_indices[0i][3u];
    let _e95 = unnamed.packed_indices[0i][3u];
    let _e100 = fog_tex_coord_1;
    let _e101 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    fog = _e101;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e106 = frag_tex_coord0_1;
    param_2 = _e106;
    param_3 = 0i;
    let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e108 = frag_color;
    color0_ = (_e107 * _e108);
    let _e110 = color0_;
    base = _e110;
    if override_type_3_ {
        let _e112 = color0_[3u];
        if (_e112 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e115 = color0_[3u];
            if (_e115 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e118 = color0_[3u];
                if (_e118 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e120 = color0_;
    base = _e120;
    if override_type_3_3 {
        let _e123 = unnamed.worldLightParams[1u];
        wetness = clamp(_e123, 0f, 1f);
        let _e127 = unnamed.worldLightParams[2u];
        frost = clamp(_e127, 0f, 1f);
        let _e129 = base;
        luminance = dot(_e129.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e132 = wetness;
        let _e134 = base;
        let _e136 = (_e134.xyz * mix(1f, 0.82f, _e132));
        base[0u] = _e136.x;
        base[1u] = _e136.y;
        base[2u] = _e136.z;
        let _e143 = base;
        let _e145 = luminance;
        let _e147 = luminance;
        let _e149 = luminance;
        let _e151 = frost;
        let _e154 = mix(_e143.xyz, vec3<f32>((_e145 * 0.88f), (_e147 * 0.94f), _e149), vec3((_e151 * 0.55f)));
        base[0u] = _e154.x;
        base[1u] = _e154.y;
        base[2u] = _e154.z;
    }
    let _e161 = color0_;
    let _e164 = unnamed.emissionRadiance;
    let _e167 = base;
    let _e169 = (_e167.xyz + (_e161.xyz * _e164.xyz));
    base[0u] = _e169.x;
    base[1u] = _e169.y;
    base[2u] = _e169.z;
    let _e176 = wired_advanced_fog_enabled_u0028_();
    if _e176 {
        let _e177 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e177;
        if override_type_3_4 {
            let _e178 = fogAmount;
            let _e180 = base;
            let _e182 = (_e180.xyz * (1f - _e178));
            base[0u] = _e182.x;
            base[1u] = _e182.y;
            base[2u] = _e182.z;
        } else {
            if override_type_3_5 {
                let _e189 = fogAmount;
                let _e191 = base;
                base = (_e191 * (1f - _e189));
            } else {
                if override_type_3_6 {
                    let _e193 = fogAmount;
                    let _e196 = base[3u];
                    base[3u] = (_e196 * (1f - _e193));
                } else {
                    let _e199 = base;
                    let _e202 = unnamed.advancedFogColorDensity;
                    let _e204 = fogAmount;
                    let _e206 = mix(_e199.xyz, _e202.xyz, vec3(_e204));
                    base[0u] = _e206.x;
                    base[1u] = _e206.y;
                    base[2u] = _e206.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e213 = base;
            let _e216 = fog[3u];
            let _e218 = (_e213.xyz * (1f - _e216));
            base[0u] = _e218.x;
            base[1u] = _e218.y;
            base[2u] = _e218.z;
        } else {
            if override_type_3_8 {
                let _e225 = base;
                let _e227 = fog[3u];
                base = (_e225 * (1f - _e227));
            } else {
                if override_type_3_9 {
                    let _e231 = base[3u];
                    let _e233 = fog[3u];
                    base[3u] = (_e231 * (1f - _e233));
                } else {
                    let _e237 = base;
                    let _e238 = fog;
                    let _e240 = unnamed.fogColor;
                    let _e243 = fog[3u];
                    base = mix(_e237, (_e238 * _e240), vec4(_e243));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e247 = base[3u];
        if (_e247 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e249 = base;
            let _e251 = base;
            if (dot(_e249.xyz, _e251.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e255 = gl_FragCoord_1;
    let _e260 = unnamed.packed_indices[1i][0u];
    let _e266 = unnamed.packed_indices[1i][0u];
    let _e271 = textureDimensions(wired_bindless_images[(_e260 & 4095u)], 0i);
    screenUV = (_e255.xy / vec2<f32>(vec2<i32>(_e271)));
    let _e278 = unnamed.packed_indices[1i][0u];
    let _e284 = unnamed.packed_indices[1i][0u];
    let _e289 = screenUV;
    let _e290 = textureSample(wired_bindless_images[(_e278 & 4095u)], wired_bindless_samplers[((_e284 >> bitcast<u32>(12i)) & 255u)], _e289);
    sceneDepth = _e290.x;
    let _e293 = gl_FragCoord_1[2u];
    fragDepth = _e293;
    let _e294 = fragDepth;
    let _e295 = sceneDepth;
    depthDiff = (_e294 - _e295);
    let _e298 = depthDiff;
    fadeFactor = smoothstep(0f, (depth_fade_scale * 0.0005f), _e298);
    let _e300 = fadeFactor;
    let _e302 = base[3u];
    base[3u] = (_e302 * _e300);
    let _e305 = fadeFactor;
    let _e306 = base;
    let _e308 = (_e306.xyz * _e305);
    base[0u] = _e308.x;
    base[1u] = _e308.y;
    base[2u] = _e308.z;
    let _e315 = base;
    out_color = _e315;
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
