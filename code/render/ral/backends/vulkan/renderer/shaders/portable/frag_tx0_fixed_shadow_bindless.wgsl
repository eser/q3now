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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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

    let _e63 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e63 + 0.5f));
    let _e68 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e70 = fogType;
    let _e73 = fogType;
    return (((_e68 > 0.5f) && (_e70 >= 1i)) && (_e73 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e63 = wired_advanced_fog_enabled_u0028_();
    if !(_e63) {
        return 0f;
    }
    let _e66 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e66, 0.000001f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e71 + 0.5f));
    let _e74 = fogType_1;
    if (_e74 == 1i) {
        let _e78 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e78 <= 0f) {
            return 0f;
        }
        let _e80 = viewDepth;
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e80 / _e83), 0f, 1f);
    }
    let _e88 = unnamed.advancedFogColorDensity[3u];
    let _e90 = viewDepth;
    opticalDepth = (max(_e88, 0f) * _e90);
    let _e92 = fogType_1;
    if (_e92 == 2i) {
        let _e94 = opticalDepth;
        return clamp((1f - exp(-(_e94))), 0f, 1f);
    }
    let _e99 = opticalDepth;
    let _e100 = opticalDepth;
    return clamp((1f - exp(-((_e99 * _e100)))), 0f, 1f);
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

    let _e70 = (*c);
    let _e73 = unnamed.cascadeMVP[_e70];
    let _e74 = (*worldPos);
    sc4_ = (_e73 * vec4<f32>(_e74.x, _e74.y, _e74.z, 1f));
    let _e80 = sc4_;
    let _e83 = sc4_[3u];
    sc = (_e80.xyz / vec3(_e83));
    let _e86 = sc;
    let _e90 = ((_e86.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e90.x;
    sc[1u] = _e90.y;
    let _e96 = sc[0u];
    let _e97 = (_e96 < 0f);
    phi_304_ = _e97;
    if !(_e97) {
        let _e100 = sc[0u];
        phi_304_ = (_e100 > 1f);
    }
    let _e103 = phi_304_;
    phi_311_ = _e103;
    if !(_e103) {
        let _e106 = sc[1u];
        phi_311_ = (_e106 < 0f);
    }
    let _e109 = phi_311_;
    phi_318_ = _e109;
    if !(_e109) {
        let _e112 = sc[1u];
        phi_318_ = (_e112 > 1f);
    }
    let _e115 = phi_318_;
    phi_325_ = _e115;
    if !(_e115) {
        let _e118 = sc[2u];
        phi_325_ = (_e118 > 1f);
    }
    let _e121 = phi_325_;
    if _e121 {
        return 1f;
    }
    let _e122 = (*c);
    layer = f32(_e122);
    let _e124 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e124).xy));
    let _e131 = sc[2u];
    currentDepth = (_e131 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e133 = currentDepth;
        let _e134 = sc;
        let _e135 = _e134.xy;
        let _e136 = layer;
        let _e139 = vec3<f32>(_e135.x, _e135.y, _e136);
        let _e145 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e139.x, _e139.y), i32(_e139.z));
        shadow = step(_e133, _e145.x);
    } else {
        if override_type_3_1 {
            let _e148 = currentDepth;
            let _e149 = sc;
            let _e150 = _e149.xy;
            let _e151 = layer;
            let _e154 = vec3<f32>(_e150.x, _e150.y, _e151);
            let _e160 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e154.x, _e154.y), i32(_e154.z));
            let _e163 = shadow;
            shadow = (_e163 + step(_e148, _e160.x));
            let _e165 = currentDepth;
            let _e166 = sc;
            let _e169 = texelSize[0u];
            let _e171 = (_e166.xy + vec2<f32>(_e169, 0f));
            let _e172 = layer;
            let _e175 = vec3<f32>(_e171.x, _e171.y, _e172);
            let _e181 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e175.x, _e175.y), i32(_e175.z));
            let _e184 = shadow;
            shadow = (_e184 + step(_e165, _e181.x));
            let _e186 = currentDepth;
            let _e187 = sc;
            let _e190 = texelSize[0u];
            let _e192 = (_e187.xy - vec2<f32>(_e190, 0f));
            let _e193 = layer;
            let _e196 = vec3<f32>(_e192.x, _e192.y, _e193);
            let _e202 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e196.x, _e196.y), i32(_e196.z));
            let _e205 = shadow;
            shadow = (_e205 + step(_e186, _e202.x));
            let _e207 = currentDepth;
            let _e208 = sc;
            let _e211 = texelSize[1u];
            let _e213 = (_e208.xy + vec2<f32>(0f, _e211));
            let _e214 = layer;
            let _e217 = vec3<f32>(_e213.x, _e213.y, _e214);
            let _e223 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e217.x, _e217.y), i32(_e217.z));
            let _e226 = shadow;
            shadow = (_e226 + step(_e207, _e223.x));
            let _e228 = currentDepth;
            let _e229 = sc;
            let _e232 = texelSize[1u];
            let _e234 = (_e229.xy - vec2<f32>(0f, _e232));
            let _e235 = layer;
            let _e238 = vec3<f32>(_e234.x, _e234.y, _e235);
            let _e244 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e238.x, _e238.y), i32(_e238.z));
            let _e247 = shadow;
            shadow = (_e247 + step(_e228, _e244.x));
            let _e249 = shadow;
            shadow = (_e249 / 5f);
        } else {
            x = -1i;
            loop {
                let _e251 = x;
                if (_e251 <= 1i) {
                    y = -1i;
                    loop {
                        let _e253 = y;
                        if (_e253 <= 1i) {
                            let _e255 = currentDepth;
                            let _e256 = sc;
                            let _e258 = x;
                            let _e260 = y;
                            let _e263 = texelSize;
                            let _e265 = (_e256.xy + (vec2<f32>(f32(_e258), f32(_e260)) * _e263));
                            let _e266 = layer;
                            let _e269 = vec3<f32>(_e265.x, _e265.y, _e266);
                            let _e275 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e269.x, _e269.y), i32(_e269.z));
                            let _e278 = shadow;
                            shadow = (_e278 + step(_e255, _e275.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e280 = y;
                            y = (_e280 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e282 = x;
                    x = (_e282 + 1i);
                }
            }
            let _e284 = shadow;
            shadow = (_e284 / 9f);
        }
    }
    let _e286 = shadow;
    return _e286;
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

    let _e77 = unnamed.cascadeSplits;
    let _e78 = (*viewDepth_1);
    cmp = step(_e77, vec4(_e78));
    let _e82 = cmp[0u];
    let _e84 = cmp[1u];
    let _e87 = cmp[2u];
    let _e90 = cmp[3u];
    cascade = min(i32((((_e82 + _e84) + _e87) + _e90)), 3i);
    let _e94 = cascade;
    (*outCascade) = _e94;
    let _e95 = cascade;
    if (_e95 == 0i) {
        local = 0f;
    } else {
        let _e97 = cascade;
        let _e102 = unnamed.cascadeSplits[max((_e97 - 1i), 0i)];
        local = _e102;
    }
    let _e103 = local;
    prevSplit = _e103;
    let _e104 = cascade;
    let _e107 = unnamed.cascadeSplits[_e104];
    farSplit = _e107;
    let _e108 = farSplit;
    let _e109 = prevSplit;
    blendRange = max((0.1f * (_e108 - _e109)), 1f);
    let _e113 = farSplit;
    let _e114 = (*viewDepth_1);
    let _e116 = blendRange;
    blendT = clamp(((_e113 - _e114) / _e116), 0f, 1f);
    let _e119 = cascade;
    param = _e119;
    let _e120 = (*worldPos_1);
    param_1 = _e120;
    let _e121 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e121;
    let _e122 = cascade;
    param_2 = min((_e122 + 1i), 3i);
    let _e125 = (*worldPos_1);
    param_3 = _e125;
    let _e126 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e126;
    let _e127 = s1_;
    let _e128 = s0_;
    let _e129 = blendT;
    return mix(_e127, _e128, _e129);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e71 = unnamed.packed_indices[1i][3u];
    let _e77 = unnamed.packed_indices[1i][3u];
    let _e82 = (*lm_uv);
    let _e83 = textureSample(wired_bindless_images[(_e71 & 4095u)], wired_bindless_samplers[((_e77 >> bitcast<u32>(12i)) & 255u)], _e82);
    sun_mask = _e83.x;
    let _e85 = sun_mask;
    if (_e85 > 0.001f) {
        let _e87 = shadowData_1;
        param_4 = _e87.xyz;
        let _e90 = shadowData_1[3u];
        param_5 = _e90;
        let _e91 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e92 = param_6;
        ignoredCascade = _e92;
        shadow_1 = _e91;
        let _e93 = shadow_1;
        let _e94 = sun_mask;
        let _e96 = (*rgb);
        (*rgb) = (_e96 * mix(1f, _e93, _e94));
    }
    let _e98 = (*rgb);
    return _e98;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e63 = (*rgb_1);
        param_7 = _e63;
        let _e64 = frag_tex_coord0_1;
        param_8 = _e64;
        let _e65 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e65;
    }
    let _e66 = (*rgb_1);
    return _e66;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c_1);
    (*c_1) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c_1);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c_1);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c_1);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e87 = (*uv);
    let _e88 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    c_2 = _e88;
    let _e89 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e89))) == 0i) {
        let _e94 = c_2;
        param_9 = _e94.xyz;
        let _e96 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e96.x;
        c_2[1u] = _e96.y;
        c_2[2u] = _e96.z;
    }
    let _e103 = (*slot);
    if (lightmap_slot == (_e103 + 1i)) {
        let _e108 = unnamed.worldLightParams[0u];
        let _e109 = c_2;
        let _e111 = (_e109.xyz * _e108);
        c_2[0u] = _e111.x;
        c_2[1u] = _e111.y;
        c_2[2u] = _e111.z;
    }
    let _e118 = c_2;
    return _e118;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var param_13: vec3<f32>;
    var fogAmount: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_10 = 0u;
    let _e72 = frag_tex_coord0_1;
    param_11 = _e72;
    param_12 = 0i;
    let _e73 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    let _e74 = frag_color;
    color0_ = (_e73 * _e74);
    let _e76 = color0_;
    base = _e76;
    let _e77 = color0_;
    base = _e77;
    let _e78 = base;
    param_13 = _e78.xyz;
    let _e80 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_13));
    base[0u] = _e80.x;
    base[1u] = _e80.y;
    base[2u] = _e80.z;
    let _e87 = wired_advanced_fog_enabled_u0028_();
    if _e87 {
        let _e88 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e88;
        let _e89 = base;
        let _e92 = unnamed.advancedFogColorDensity;
        let _e94 = fogAmount;
        let _e96 = mix(_e89.xyz, _e92.xyz, vec3(_e94));
        base[0u] = _e96.x;
        base[1u] = _e96.y;
        base[2u] = _e96.z;
    }
    if override_type_3_3 {
        let _e104 = base[3u];
        if (_e104 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e106 = base;
            let _e108 = base;
            if (dot(_e106.xyz, _e108.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e112 = base;
    out_color = _e112;
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
