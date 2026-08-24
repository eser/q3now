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
    _pad_to_cascadeMVP: array<vec4<f32>, 1>,
    cascadeMVP: array<mat4x4<f32>, 4>,
    cascadeSplits: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 8>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
@group(2) @binding(0)
var shadowMap: texture_2d_array<f32>;
@group(2) @binding(32)
var shadowMap_sampler: sampler;
var<private> shadowData_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
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

fn sampleCascade_u0028_i1_u003b_vf3_u003b(c: ptr<function, i32>, worldPos: ptr<function, vec3<f32>>) -> f32 {
    var sc4_: vec4<f32>;
    var sc: vec3<f32>;
    var layer: f32;
    var texelSize: vec2<f32>;
    var currentDepth: f32;
    var shadow: f32;
    var x: i32;
    var y: i32;
    var phi_304_: bool;
    var phi_311_: bool;
    var phi_318_: bool;
    var phi_325_: bool;

    let _e69 = (*c);
    let _e72 = unnamed.cascadeMVP[_e69];
    let _e73 = (*worldPos);
    sc4_ = (_e72 * vec4<f32>(_e73.x, _e73.y, _e73.z, 1f));
    let _e79 = sc4_;
    let _e82 = sc4_[3u];
    sc = (_e79.xyz / vec3(_e82));
    let _e85 = sc;
    let _e89 = ((_e85.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e89.x;
    sc[1u] = _e89.y;
    let _e95 = sc[0u];
    let _e96 = (_e95 < 0f);
    phi_304_ = _e96;
    if !(_e96) {
        let _e99 = sc[0u];
        phi_304_ = (_e99 > 1f);
    }
    let _e102 = phi_304_;
    phi_311_ = _e102;
    if !(_e102) {
        let _e105 = sc[1u];
        phi_311_ = (_e105 < 0f);
    }
    let _e108 = phi_311_;
    phi_318_ = _e108;
    if !(_e108) {
        let _e111 = sc[1u];
        phi_318_ = (_e111 > 1f);
    }
    let _e114 = phi_318_;
    phi_325_ = _e114;
    if !(_e114) {
        let _e117 = sc[2u];
        phi_325_ = (_e117 > 1f);
    }
    let _e120 = phi_325_;
    if _e120 {
        return 1f;
    }
    let _e121 = (*c);
    layer = f32(_e121);
    let _e123 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e123).xy));
    let _e130 = sc[2u];
    currentDepth = (_e130 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e132 = currentDepth;
        let _e133 = sc;
        let _e134 = _e133.xy;
        let _e135 = layer;
        let _e138 = vec3<f32>(_e134.x, _e134.y, _e135);
        let _e144 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e138.x, _e138.y), i32(_e138.z));
        shadow = step(_e132, _e144.x);
    } else {
        if override_type_3_1 {
            let _e147 = currentDepth;
            let _e148 = sc;
            let _e149 = _e148.xy;
            let _e150 = layer;
            let _e153 = vec3<f32>(_e149.x, _e149.y, _e150);
            let _e159 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e153.x, _e153.y), i32(_e153.z));
            let _e162 = shadow;
            shadow = (_e162 + step(_e147, _e159.x));
            let _e164 = currentDepth;
            let _e165 = sc;
            let _e168 = texelSize[0u];
            let _e170 = (_e165.xy + vec2<f32>(_e168, 0f));
            let _e171 = layer;
            let _e174 = vec3<f32>(_e170.x, _e170.y, _e171);
            let _e180 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e174.x, _e174.y), i32(_e174.z));
            let _e183 = shadow;
            shadow = (_e183 + step(_e164, _e180.x));
            let _e185 = currentDepth;
            let _e186 = sc;
            let _e189 = texelSize[0u];
            let _e191 = (_e186.xy - vec2<f32>(_e189, 0f));
            let _e192 = layer;
            let _e195 = vec3<f32>(_e191.x, _e191.y, _e192);
            let _e201 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e195.x, _e195.y), i32(_e195.z));
            let _e204 = shadow;
            shadow = (_e204 + step(_e185, _e201.x));
            let _e206 = currentDepth;
            let _e207 = sc;
            let _e210 = texelSize[1u];
            let _e212 = (_e207.xy + vec2<f32>(0f, _e210));
            let _e213 = layer;
            let _e216 = vec3<f32>(_e212.x, _e212.y, _e213);
            let _e222 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e216.x, _e216.y), i32(_e216.z));
            let _e225 = shadow;
            shadow = (_e225 + step(_e206, _e222.x));
            let _e227 = currentDepth;
            let _e228 = sc;
            let _e231 = texelSize[1u];
            let _e233 = (_e228.xy - vec2<f32>(0f, _e231));
            let _e234 = layer;
            let _e237 = vec3<f32>(_e233.x, _e233.y, _e234);
            let _e243 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e237.x, _e237.y), i32(_e237.z));
            let _e246 = shadow;
            shadow = (_e246 + step(_e227, _e243.x));
            let _e248 = shadow;
            shadow = (_e248 / 5f);
        } else {
            x = -1i;
            loop {
                let _e250 = x;
                if (_e250 <= 1i) {
                    y = -1i;
                    loop {
                        let _e252 = y;
                        if (_e252 <= 1i) {
                            let _e254 = currentDepth;
                            let _e255 = sc;
                            let _e257 = x;
                            let _e259 = y;
                            let _e262 = texelSize;
                            let _e264 = (_e255.xy + (vec2<f32>(f32(_e257), f32(_e259)) * _e262));
                            let _e265 = layer;
                            let _e268 = vec3<f32>(_e264.x, _e264.y, _e265);
                            let _e274 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e268.x, _e268.y), i32(_e268.z));
                            let _e277 = shadow;
                            shadow = (_e277 + step(_e254, _e274.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e279 = y;
                            y = (_e279 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e281 = x;
                    x = (_e281 + 1i);
                }
            }
            let _e283 = shadow;
            shadow = (_e283 / 9f);
        }
    }
    let _e285 = shadow;
    return _e285;
}

fn sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b(worldPos_1: ptr<function, vec3<f32>>, viewDepth_1: ptr<function, f32>, outCascade: ptr<function, i32>) -> f32 {
    var cmp: vec4<f32>;
    var cascade: i32;
    var prevSplit: f32;
    var local: f32;
    var farSplit: f32;
    var blendRange: f32;
    var blendT: f32;
    var s0_: f32;
    var param: i32;
    var param_1: vec3<f32>;
    var s1_: f32;
    var param_2: i32;
    var param_3: vec3<f32>;

    let _e76 = unnamed.cascadeSplits;
    let _e77 = (*viewDepth_1);
    cmp = step(_e76, vec4(_e77));
    let _e81 = cmp[0u];
    let _e83 = cmp[1u];
    let _e86 = cmp[2u];
    let _e89 = cmp[3u];
    cascade = min(i32((((_e81 + _e83) + _e86) + _e89)), 3i);
    let _e93 = cascade;
    (*outCascade) = _e93;
    let _e94 = cascade;
    if (_e94 == 0i) {
        local = 0f;
    } else {
        let _e96 = cascade;
        let _e101 = unnamed.cascadeSplits[max((_e96 - 1i), 0i)];
        local = _e101;
    }
    let _e102 = local;
    prevSplit = _e102;
    let _e103 = cascade;
    let _e106 = unnamed.cascadeSplits[_e103];
    farSplit = _e106;
    let _e107 = farSplit;
    let _e108 = prevSplit;
    blendRange = max((0.1f * (_e107 - _e108)), 1f);
    let _e112 = farSplit;
    let _e113 = (*viewDepth_1);
    let _e115 = blendRange;
    blendT = clamp(((_e112 - _e113) / _e115), 0f, 1f);
    let _e118 = cascade;
    param = _e118;
    let _e119 = (*worldPos_1);
    param_1 = _e119;
    let _e120 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e120;
    let _e121 = cascade;
    param_2 = min((_e121 + 1i), 3i);
    let _e124 = (*worldPos_1);
    param_3 = _e124;
    let _e125 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e125;
    let _e126 = s1_;
    let _e127 = s0_;
    let _e128 = blendT;
    return mix(_e126, _e127, _e128);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e70 = unnamed.packed_indices[1i][3u];
    let _e76 = unnamed.packed_indices[1i][3u];
    let _e81 = (*lm_uv);
    let _e82 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e76 >> bitcast<u32>(12i)) & 255u)], _e81);
    sun_mask = _e82.x;
    let _e84 = sun_mask;
    if (_e84 > 0.001f) {
        let _e86 = shadowData_1;
        param_4 = _e86.xyz;
        let _e89 = shadowData_1[3u];
        param_5 = _e89;
        let _e90 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e91 = param_6;
        ignoredCascade = _e91;
        shadow_1 = _e90;
        let _e92 = shadow_1;
        let _e93 = sun_mask;
        let _e95 = (*rgb);
        (*rgb) = (_e95 * mix(1f, _e92, _e93));
    }
    let _e97 = (*rgb);
    return _e97;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e62 = (*rgb_1);
        param_7 = _e62;
        let _e63 = frag_tex_coord0_1;
        param_8 = _e63;
        let _e64 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e64;
    }
    let _e65 = (*rgb_1);
    return _e65;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e63 = (*c_1);
    (*c_1) = max(_e63, vec3<f32>(0f, 0f, 0f));
    let _e65 = (*c_1);
    cutoff = (_e65 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e67 = (*c_1);
    lo = (_e67 / vec3(12.92f));
    let _e70 = (*c_1);
    hi = pow(((_e70 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e75 = hi;
    let _e76 = lo;
    let _e77 = cutoff;
    return mix(_e75, _e76, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e77));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e64 = (*role);
    let _e66 = (*role);
    let _e71 = unnamed.packed_indices[(_e64 / 4u)][(_e66 % 4u)];
    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e86 = (*uv);
    let _e87 = textureSample(wired_bindless_images[(_e71 & 4095u)], wired_bindless_samplers[((_e81 >> bitcast<u32>(12i)) & 255u)], _e86);
    c_2 = _e87;
    let _e88 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e88))) == 0i) {
        let _e93 = c_2;
        param_9 = _e93.xyz;
        let _e95 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e95.x;
        c_2[1u] = _e95.y;
        c_2[2u] = _e95.z;
    }
    let _e102 = (*slot);
    if (lightmap_slot == (_e102 + 1i)) {
        let _e107 = unnamed.worldLightParams[0u];
        let _e108 = c_2;
        let _e110 = (_e108.xyz * _e107);
        c_2[0u] = _e110.x;
        c_2[1u] = _e110.y;
        c_2[2u] = _e110.z;
    }
    let _e117 = c_2;
    return _e117;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_10: vec3<f32>;
    var color0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var param_14: vec3<f32>;
    var fogAmount: f32;

    let _e68 = frag_color0In_1;
    param_10 = _e68.xyz;
    let _e70 = sRGBToLinear_u0028_vf3_u003b((&param_10));
    let _e72 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e70.x, _e70.y, _e70.z, _e72);
    param_11 = 0u;
    let _e77 = frag_tex_coord0_1;
    param_12 = _e77;
    param_13 = 0i;
    let _e78 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    let _e79 = frag_color0_;
    color0_ = (_e78 * _e79);
    let _e81 = color0_;
    base = _e81;
    let _e82 = color0_;
    base = _e82;
    let _e83 = base;
    param_14 = _e83.xyz;
    let _e85 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e85.x;
    base[1u] = _e85.y;
    base[2u] = _e85.z;
    let _e92 = wired_advanced_fog_enabled_u0028_();
    if _e92 {
        let _e93 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e93;
        let _e94 = base;
        let _e97 = unnamed.advancedFogColorDensity;
        let _e99 = fogAmount;
        let _e101 = mix(_e94.xyz, _e97.xyz, vec3(_e99));
        base[0u] = _e101.x;
        base[1u] = _e101.y;
        base[2u] = _e101.z;
    }
    if override_type_3_3 {
        let _e109 = base[3u];
        if (_e109 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e111 = base;
            let _e113 = base;
            if (dot(_e111.xyz, _e113.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e117 = base;
    out_color = _e117;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e9 = out_color;
    return _e9;
}
