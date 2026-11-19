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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(0) override alpha_test_func: i32 = 0i;
override override_type_3_: bool = (alpha_test_func == 1i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_3_1: bool = (alpha_test_func == 2i);
override override_type_3_2: bool = (alpha_test_func == 3i);
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_4: bool = (discard_mode == 1i);
override override_type_3_5: bool = (discard_mode == 2i);
@id(11) override depth_fade_scale: f32 = 2f;
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e66 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e66 + 0.5f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e73 = fogType;
    let _e76 = fogType;
    return (((_e71 > 0.5f) && (_e73 >= 1i)) && (_e76 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e66 = wired_advanced_fog_enabled_u0028_();
    if !(_e66) {
        return 0f;
    }
    let _e69 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e69, 0.000001f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e74 + 0.5f));
    let _e77 = fogType_1;
    if (_e77 == 1i) {
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e81 <= 0f) {
            return 0f;
        }
        let _e83 = viewDepth;
        let _e86 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e83 / _e86), 0f, 1f);
    }
    let _e91 = unnamed.advancedFogColorDensity[3u];
    let _e93 = viewDepth;
    opticalDepth = (max(_e91, 0f) * _e93);
    let _e95 = fogType_1;
    if (_e95 == 2i) {
        let _e97 = opticalDepth;
        return clamp((1f - exp(-(_e97))), 0f, 1f);
    }
    let _e102 = opticalDepth;
    let _e103 = opticalDepth;
    return clamp((1f - exp(-((_e102 * _e103)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e66 = (*rgb);
    let _e69 = unnamed.worldLightParams[0u];
    boosted = (_e66 * _e69);
    let _e72 = boosted[0u];
    let _e74 = boosted[1u];
    let _e76 = boosted[2u];
    peak = max(_e72, max(_e74, _e76));
    let _e79 = peak;
    if (_e79 > 1f) {
        let _e81 = peak;
        let _e82 = boosted;
        boosted = (_e82 / vec3(_e81));
    }
    let _e85 = boosted;
    return _e85;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e67 = (*c);
    (*c) = max(_e67, vec3<f32>(0f, 0f, 0f));
    let _e69 = (*c);
    cutoff = (_e69 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e71 = (*c);
    lo = (_e71 / vec3(12.92f));
    let _e74 = (*c);
    hi = pow(((_e74 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e79 = hi;
    let _e80 = lo;
    let _e81 = cutoff;
    return mix(_e79, _e80, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e81));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e91 = (*uv);
    let _e92 = textureSample(wired_bindless_images[(_e76 & 4095u)], wired_bindless_samplers[((_e86 >> bitcast<u32>(12i)) & 255u)], _e91);
    c_1 = _e92;
    let _e93 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e93))) == 0i) {
        let _e98 = c_1;
        param = _e98.xyz;
        let _e100 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e100.x;
        c_1[1u] = _e100.y;
        c_1[2u] = _e100.z;
    }
    let _e107 = (*slot);
    if (lightmap_slot == (_e107 + 1i)) {
        let _e110 = c_1;
        param_1 = _e110.xyz;
        let _e112 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = c_1;
    return _e119;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
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

    param_2 = 0u;
    let _e79 = frag_tex_coord0_1;
    param_3 = _e79;
    param_4 = 0i;
    let _e80 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e80;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e85 = sampledColor0_;
    let _e86 = frag_color;
    color0_ = (_e85 * _e86);
    let _e88 = color0_;
    base = _e88;
    if override_type_3_ {
        let _e90 = color0_[3u];
        if (_e90 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e93 = color0_[3u];
            if (_e93 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e96 = color0_[3u];
                if (_e96 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e98 = color0_;
    base = _e98;
    if override_type_3_3 {
        let _e101 = unnamed.worldLightParams[1u];
        wetness = clamp(_e101, 0f, 1f);
        let _e105 = unnamed.worldLightParams[2u];
        frost = clamp(_e105, 0f, 1f);
        let _e107 = base;
        luminance = dot(_e107.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e110 = wetness;
        let _e112 = base;
        let _e114 = (_e112.xyz * mix(1f, 0.82f, _e110));
        base[0u] = _e114.x;
        base[1u] = _e114.y;
        base[2u] = _e114.z;
        let _e121 = base;
        let _e123 = luminance;
        let _e125 = luminance;
        let _e127 = luminance;
        let _e129 = frost;
        let _e132 = mix(_e121.xyz, vec3<f32>((_e123 * 0.88f), (_e125 * 0.94f), _e127), vec3((_e129 * 0.55f)));
        base[0u] = _e132.x;
        base[1u] = _e132.y;
        base[2u] = _e132.z;
    }
    let _e141 = unnamed.materialFactors[0u];
    if (_e141 >= 0f) {
        let _e144 = unnamed.materialBaseColor;
        let _e145 = base;
        base = (_e145 * _e144);
    }
    let _e147 = color0_;
    let _e150 = unnamed.emissionRadiance;
    let _e153 = base;
    let _e155 = (_e153.xyz + (_e147.xyz * _e150.xyz));
    base[0u] = _e155.x;
    base[1u] = _e155.y;
    base[2u] = _e155.z;
    let _e162 = wired_advanced_fog_enabled_u0028_();
    if _e162 {
        let _e163 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e163;
        let _e164 = base;
        let _e167 = unnamed.advancedFogColorDensity;
        let _e169 = fogAmount;
        let _e171 = mix(_e164.xyz, _e167.xyz, vec3(_e169));
        base[0u] = _e171.x;
        base[1u] = _e171.y;
        base[2u] = _e171.z;
    }
    if override_type_3_4 {
        let _e179 = base[3u];
        if (_e179 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
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
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e5 = out_color;
    return _e5;
}
