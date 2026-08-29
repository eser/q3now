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

    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e70 + 0.5f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e77 = fogType;
    let _e80 = fogType;
    return (((_e75 > 0.5f) && (_e77 >= 1i)) && (_e80 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e70 = wired_advanced_fog_enabled_u0028_();
    if !(_e70) {
        return 0f;
    }
    let _e73 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e73, 0.000001f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e78 + 0.5f));
    let _e81 = fogType_1;
    if (_e81 == 1i) {
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e85 <= 0f) {
            return 0f;
        }
        let _e87 = viewDepth;
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e87 / _e90), 0f, 1f);
    }
    let _e95 = unnamed.advancedFogColorDensity[3u];
    let _e97 = viewDepth;
    opticalDepth = (max(_e95, 0f) * _e97);
    let _e99 = fogType_1;
    if (_e99 == 2i) {
        let _e101 = opticalDepth;
        return clamp((1f - exp(-(_e101))), 0f, 1f);
    }
    let _e106 = opticalDepth;
    let _e107 = opticalDepth;
    return clamp((1f - exp(-((_e106 * _e107)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e71 = (*c);
    (*c) = max(_e71, vec3<f32>(0f, 0f, 0f));
    let _e73 = (*c);
    cutoff = (_e73 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e75 = (*c);
    lo = (_e75 / vec3(12.92f));
    let _e78 = (*c);
    hi = pow(((_e78 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e83 = hi;
    let _e84 = lo;
    let _e85 = cutoff;
    return mix(_e83, _e84, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e85));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e94 = (*uv);
    let _e95 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    c_1 = _e95;
    let _e96 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e96))) == 0i) {
        let _e101 = c_1;
        param = _e101.xyz;
        let _e103 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e103.x;
        c_1[1u] = _e103.y;
        c_1[2u] = _e103.z;
    }
    let _e110 = (*slot);
    if (lightmap_slot == (_e110 + 1i)) {
        let _e115 = unnamed.worldLightParams[0u];
        let _e116 = c_1;
        let _e118 = (_e116.xyz * _e115);
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
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
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
    let _e95 = frag_color0In_1;
    param_1 = _e95.xyz;
    let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e99 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e97.x, _e97.y, _e97.z, _e99);
    param_2 = 0u;
    let _e104 = frag_tex_coord0_1;
    param_3 = _e104;
    param_4 = 0i;
    let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e106 = frag_color0_;
    color0_ = (_e105 * _e106);
    let _e108 = color0_;
    base = _e108;
    if override_type_3_ {
        let _e110 = color0_[3u];
        if (_e110 == alpha_test_value) {
            discard;
        }
    } else {
        if override_type_3_1 {
            let _e113 = color0_[3u];
            if (_e113 >= alpha_test_value) {
                discard;
            }
        } else {
            if override_type_3_2 {
                let _e116 = color0_[3u];
                if (_e116 < alpha_test_value) {
                    discard;
                }
            }
        }
    }
    let _e118 = color0_;
    base = _e118;
    if override_type_3_3 {
        let _e121 = unnamed.worldLightParams[1u];
        wetness = clamp(_e121, 0f, 1f);
        let _e125 = unnamed.worldLightParams[2u];
        frost = clamp(_e125, 0f, 1f);
        let _e127 = base;
        luminance = dot(_e127.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e130 = wetness;
        let _e132 = base;
        let _e134 = (_e132.xyz * mix(1f, 0.82f, _e130));
        base[0u] = _e134.x;
        base[1u] = _e134.y;
        base[2u] = _e134.z;
        let _e141 = base;
        let _e143 = luminance;
        let _e145 = luminance;
        let _e147 = luminance;
        let _e149 = frost;
        let _e152 = mix(_e141.xyz, vec3<f32>((_e143 * 0.88f), (_e145 * 0.94f), _e147), vec3((_e149 * 0.55f)));
        base[0u] = _e152.x;
        base[1u] = _e152.y;
        base[2u] = _e152.z;
    }
    let _e159 = color0_;
    let _e162 = unnamed.emissionRadiance;
    let _e165 = base;
    let _e167 = (_e165.xyz + (_e159.xyz * _e162.xyz));
    base[0u] = _e167.x;
    base[1u] = _e167.y;
    base[2u] = _e167.z;
    let _e174 = wired_advanced_fog_enabled_u0028_();
    if _e174 {
        let _e175 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e175;
        if override_type_3_4 {
            let _e176 = fogAmount;
            let _e178 = base;
            let _e180 = (_e178.xyz * (1f - _e176));
            base[0u] = _e180.x;
            base[1u] = _e180.y;
            base[2u] = _e180.z;
        } else {
            if override_type_3_5 {
                let _e187 = fogAmount;
                let _e189 = base;
                base = (_e189 * (1f - _e187));
            } else {
                if override_type_3_6 {
                    let _e191 = fogAmount;
                    let _e194 = base[3u];
                    base[3u] = (_e194 * (1f - _e191));
                } else {
                    let _e197 = base;
                    let _e200 = unnamed.advancedFogColorDensity;
                    let _e202 = fogAmount;
                    let _e204 = mix(_e197.xyz, _e200.xyz, vec3(_e202));
                    base[0u] = _e204.x;
                    base[1u] = _e204.y;
                    base[2u] = _e204.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e211 = base;
            let _e214 = fog[3u];
            let _e216 = (_e211.xyz * (1f - _e214));
            base[0u] = _e216.x;
            base[1u] = _e216.y;
            base[2u] = _e216.z;
        } else {
            if override_type_3_8 {
                let _e223 = base;
                let _e225 = fog[3u];
                base = (_e223 * (1f - _e225));
            } else {
                if override_type_3_9 {
                    let _e229 = base[3u];
                    let _e231 = fog[3u];
                    base[3u] = (_e229 * (1f - _e231));
                } else {
                    let _e235 = base;
                    let _e236 = fog;
                    let _e238 = unnamed.fogColor;
                    let _e241 = fog[3u];
                    base = mix(_e235, (_e236 * _e238), vec4(_e241));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e245 = base[3u];
        if (_e245 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e247 = base;
            let _e249 = base;
            if (dot(_e247.xyz, _e249.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e253 = base;
    out_color = _e253;
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
