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

    let _e68 = (*c);
    let _e71 = unnamed.cascadeMVP[_e68];
    let _e72 = (*worldPos);
    sc4_ = (_e71 * vec4<f32>(_e72.x, _e72.y, _e72.z, 1f));
    let _e78 = sc4_;
    let _e81 = sc4_[3u];
    sc = (_e78.xyz / vec3(_e81));
    let _e84 = sc;
    let _e88 = ((_e84.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e88.x;
    sc[1u] = _e88.y;
    let _e94 = sc[0u];
    let _e95 = (_e94 < 0f);
    phi_304_ = _e95;
    if !(_e95) {
        let _e98 = sc[0u];
        phi_304_ = (_e98 > 1f);
    }
    let _e101 = phi_304_;
    phi_311_ = _e101;
    if !(_e101) {
        let _e104 = sc[1u];
        phi_311_ = (_e104 < 0f);
    }
    let _e107 = phi_311_;
    phi_318_ = _e107;
    if !(_e107) {
        let _e110 = sc[1u];
        phi_318_ = (_e110 > 1f);
    }
    let _e113 = phi_318_;
    phi_325_ = _e113;
    if !(_e113) {
        let _e116 = sc[2u];
        phi_325_ = (_e116 > 1f);
    }
    let _e119 = phi_325_;
    if _e119 {
        return 1f;
    }
    let _e120 = (*c);
    layer = f32(_e120);
    let _e122 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e122).xy));
    let _e129 = sc[2u];
    currentDepth = (_e129 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e131 = currentDepth;
        let _e132 = sc;
        let _e133 = _e132.xy;
        let _e134 = layer;
        let _e137 = vec3<f32>(_e133.x, _e133.y, _e134);
        let _e143 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e137.x, _e137.y), i32(_e137.z));
        shadow = step(_e131, _e143.x);
    } else {
        if override_type_3_1 {
            let _e146 = currentDepth;
            let _e147 = sc;
            let _e148 = _e147.xy;
            let _e149 = layer;
            let _e152 = vec3<f32>(_e148.x, _e148.y, _e149);
            let _e158 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e152.x, _e152.y), i32(_e152.z));
            let _e161 = shadow;
            shadow = (_e161 + step(_e146, _e158.x));
            let _e163 = currentDepth;
            let _e164 = sc;
            let _e167 = texelSize[0u];
            let _e169 = (_e164.xy + vec2<f32>(_e167, 0f));
            let _e170 = layer;
            let _e173 = vec3<f32>(_e169.x, _e169.y, _e170);
            let _e179 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e173.x, _e173.y), i32(_e173.z));
            let _e182 = shadow;
            shadow = (_e182 + step(_e163, _e179.x));
            let _e184 = currentDepth;
            let _e185 = sc;
            let _e188 = texelSize[0u];
            let _e190 = (_e185.xy - vec2<f32>(_e188, 0f));
            let _e191 = layer;
            let _e194 = vec3<f32>(_e190.x, _e190.y, _e191);
            let _e200 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e194.x, _e194.y), i32(_e194.z));
            let _e203 = shadow;
            shadow = (_e203 + step(_e184, _e200.x));
            let _e205 = currentDepth;
            let _e206 = sc;
            let _e209 = texelSize[1u];
            let _e211 = (_e206.xy + vec2<f32>(0f, _e209));
            let _e212 = layer;
            let _e215 = vec3<f32>(_e211.x, _e211.y, _e212);
            let _e221 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e215.x, _e215.y), i32(_e215.z));
            let _e224 = shadow;
            shadow = (_e224 + step(_e205, _e221.x));
            let _e226 = currentDepth;
            let _e227 = sc;
            let _e230 = texelSize[1u];
            let _e232 = (_e227.xy - vec2<f32>(0f, _e230));
            let _e233 = layer;
            let _e236 = vec3<f32>(_e232.x, _e232.y, _e233);
            let _e242 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e236.x, _e236.y), i32(_e236.z));
            let _e245 = shadow;
            shadow = (_e245 + step(_e226, _e242.x));
            let _e247 = shadow;
            shadow = (_e247 / 5f);
        } else {
            x = -1i;
            loop {
                let _e249 = x;
                if (_e249 <= 1i) {
                    y = -1i;
                    loop {
                        let _e251 = y;
                        if (_e251 <= 1i) {
                            let _e253 = currentDepth;
                            let _e254 = sc;
                            let _e256 = x;
                            let _e258 = y;
                            let _e261 = texelSize;
                            let _e263 = (_e254.xy + (vec2<f32>(f32(_e256), f32(_e258)) * _e261));
                            let _e264 = layer;
                            let _e267 = vec3<f32>(_e263.x, _e263.y, _e264);
                            let _e273 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e267.x, _e267.y), i32(_e267.z));
                            let _e276 = shadow;
                            shadow = (_e276 + step(_e253, _e273.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e278 = y;
                            y = (_e278 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e280 = x;
                    x = (_e280 + 1i);
                }
            }
            let _e282 = shadow;
            shadow = (_e282 / 9f);
        }
    }
    let _e284 = shadow;
    return _e284;
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

    let _e75 = unnamed.cascadeSplits;
    let _e76 = (*viewDepth_1);
    cmp = step(_e75, vec4(_e76));
    let _e80 = cmp[0u];
    let _e82 = cmp[1u];
    let _e85 = cmp[2u];
    let _e88 = cmp[3u];
    cascade = min(i32((((_e80 + _e82) + _e85) + _e88)), 3i);
    let _e92 = cascade;
    (*outCascade) = _e92;
    let _e93 = cascade;
    if (_e93 == 0i) {
        local = 0f;
    } else {
        let _e95 = cascade;
        let _e100 = unnamed.cascadeSplits[max((_e95 - 1i), 0i)];
        local = _e100;
    }
    let _e101 = local;
    prevSplit = _e101;
    let _e102 = cascade;
    let _e105 = unnamed.cascadeSplits[_e102];
    farSplit = _e105;
    let _e106 = farSplit;
    let _e107 = prevSplit;
    blendRange = max((0.1f * (_e106 - _e107)), 1f);
    let _e111 = farSplit;
    let _e112 = (*viewDepth_1);
    let _e114 = blendRange;
    blendT = clamp(((_e111 - _e112) / _e114), 0f, 1f);
    let _e117 = cascade;
    param = _e117;
    let _e118 = (*worldPos_1);
    param_1 = _e118;
    let _e119 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e119;
    let _e120 = cascade;
    param_2 = min((_e120 + 1i), 3i);
    let _e123 = (*worldPos_1);
    param_3 = _e123;
    let _e124 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e124;
    let _e125 = s1_;
    let _e126 = s0_;
    let _e127 = blendT;
    return mix(_e125, _e126, _e127);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e69 = unnamed.packed_indices[1i][3u];
    let _e75 = unnamed.packed_indices[1i][3u];
    let _e80 = (*lm_uv);
    let _e81 = textureSample(wired_bindless_images[(_e69 & 4095u)], wired_bindless_samplers[((_e75 >> bitcast<u32>(12i)) & 255u)], _e80);
    sun_mask = _e81.x;
    let _e83 = sun_mask;
    if (_e83 > 0.001f) {
        let _e85 = shadowData_1;
        param_4 = _e85.xyz;
        let _e88 = shadowData_1[3u];
        param_5 = _e88;
        let _e89 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e90 = param_6;
        ignoredCascade = _e90;
        shadow_1 = _e89;
        let _e91 = shadow_1;
        let _e92 = sun_mask;
        let _e94 = (*rgb);
        (*rgb) = (_e94 * mix(1f, _e91, _e92));
    }
    let _e96 = (*rgb);
    return _e96;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e61 = (*rgb_1);
        param_7 = _e61;
        let _e62 = frag_tex_coord0_1;
        param_8 = _e62;
        let _e63 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e63;
    }
    let _e64 = (*rgb_1);
    return _e64;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c_1);
    (*c_1) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c_1);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c_1);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c_1);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_2 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_2;
        param_9 = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e94.x;
        c_2[1u] = _e94.y;
        c_2[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_2;
        let _e109 = (_e107.xyz * _e106);
        c_2[0u] = _e109.x;
        c_2[1u] = _e109.y;
        c_2[2u] = _e109.z;
    }
    let _e116 = c_2;
    return _e116;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var param_13: vec3<f32>;
    var fogAmount: f32;

    param_10 = 0u;
    let _e65 = frag_tex_coord0_1;
    param_11 = _e65;
    param_12 = 0i;
    let _e66 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    color0_ = _e66;
    let _e67 = color0_;
    base = _e67;
    let _e68 = color0_;
    base = _e68;
    let _e69 = base;
    param_13 = _e69.xyz;
    let _e71 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_13));
    base[0u] = _e71.x;
    base[1u] = _e71.y;
    base[2u] = _e71.z;
    let _e78 = wired_advanced_fog_enabled_u0028_();
    if _e78 {
        let _e79 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e79;
        let _e80 = base;
        let _e83 = unnamed.advancedFogColorDensity;
        let _e85 = fogAmount;
        let _e87 = mix(_e80.xyz, _e83.xyz, vec3(_e85));
        base[0u] = _e87.x;
        base[1u] = _e87.y;
        base[2u] = _e87.z;
    }
    if override_type_3_3 {
        let _e95 = base[3u];
        if (_e95 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e97 = base;
            let _e99 = base;
            if (dot(_e97.xyz, _e99.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e103 = base;
    out_color = _e103;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
