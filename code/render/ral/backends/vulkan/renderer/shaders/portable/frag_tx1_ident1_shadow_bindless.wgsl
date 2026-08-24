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
override override_type_3_3: bool = (lightmap_slot == 2i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_4: bool = (tex_mode == 1i);
override override_type_3_5: bool = (tex_mode == 2i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_6: bool = (discard_mode == 1i);
override override_type_3_7: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord1_1: vec2<f32>;
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

    let _e73 = (*c);
    let _e76 = unnamed.cascadeMVP[_e73];
    let _e77 = (*worldPos);
    sc4_ = (_e76 * vec4<f32>(_e77.x, _e77.y, _e77.z, 1f));
    let _e83 = sc4_;
    let _e86 = sc4_[3u];
    sc = (_e83.xyz / vec3(_e86));
    let _e89 = sc;
    let _e93 = ((_e89.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e93.x;
    sc[1u] = _e93.y;
    let _e99 = sc[0u];
    let _e100 = (_e99 < 0f);
    phi_304_ = _e100;
    if !(_e100) {
        let _e103 = sc[0u];
        phi_304_ = (_e103 > 1f);
    }
    let _e106 = phi_304_;
    phi_311_ = _e106;
    if !(_e106) {
        let _e109 = sc[1u];
        phi_311_ = (_e109 < 0f);
    }
    let _e112 = phi_311_;
    phi_318_ = _e112;
    if !(_e112) {
        let _e115 = sc[1u];
        phi_318_ = (_e115 > 1f);
    }
    let _e118 = phi_318_;
    phi_325_ = _e118;
    if !(_e118) {
        let _e121 = sc[2u];
        phi_325_ = (_e121 > 1f);
    }
    let _e124 = phi_325_;
    if _e124 {
        return 1f;
    }
    let _e125 = (*c);
    layer = f32(_e125);
    let _e127 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e127).xy));
    let _e134 = sc[2u];
    currentDepth = (_e134 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e136 = currentDepth;
        let _e137 = sc;
        let _e138 = _e137.xy;
        let _e139 = layer;
        let _e142 = vec3<f32>(_e138.x, _e138.y, _e139);
        let _e148 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e142.x, _e142.y), i32(_e142.z));
        shadow = step(_e136, _e148.x);
    } else {
        if override_type_3_1 {
            let _e151 = currentDepth;
            let _e152 = sc;
            let _e153 = _e152.xy;
            let _e154 = layer;
            let _e157 = vec3<f32>(_e153.x, _e153.y, _e154);
            let _e163 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e157.x, _e157.y), i32(_e157.z));
            let _e166 = shadow;
            shadow = (_e166 + step(_e151, _e163.x));
            let _e168 = currentDepth;
            let _e169 = sc;
            let _e172 = texelSize[0u];
            let _e174 = (_e169.xy + vec2<f32>(_e172, 0f));
            let _e175 = layer;
            let _e178 = vec3<f32>(_e174.x, _e174.y, _e175);
            let _e184 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e178.x, _e178.y), i32(_e178.z));
            let _e187 = shadow;
            shadow = (_e187 + step(_e168, _e184.x));
            let _e189 = currentDepth;
            let _e190 = sc;
            let _e193 = texelSize[0u];
            let _e195 = (_e190.xy - vec2<f32>(_e193, 0f));
            let _e196 = layer;
            let _e199 = vec3<f32>(_e195.x, _e195.y, _e196);
            let _e205 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e199.x, _e199.y), i32(_e199.z));
            let _e208 = shadow;
            shadow = (_e208 + step(_e189, _e205.x));
            let _e210 = currentDepth;
            let _e211 = sc;
            let _e214 = texelSize[1u];
            let _e216 = (_e211.xy + vec2<f32>(0f, _e214));
            let _e217 = layer;
            let _e220 = vec3<f32>(_e216.x, _e216.y, _e217);
            let _e226 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e220.x, _e220.y), i32(_e220.z));
            let _e229 = shadow;
            shadow = (_e229 + step(_e210, _e226.x));
            let _e231 = currentDepth;
            let _e232 = sc;
            let _e235 = texelSize[1u];
            let _e237 = (_e232.xy - vec2<f32>(0f, _e235));
            let _e238 = layer;
            let _e241 = vec3<f32>(_e237.x, _e237.y, _e238);
            let _e247 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e241.x, _e241.y), i32(_e241.z));
            let _e250 = shadow;
            shadow = (_e250 + step(_e231, _e247.x));
            let _e252 = shadow;
            shadow = (_e252 / 5f);
        } else {
            x = -1i;
            loop {
                let _e254 = x;
                if (_e254 <= 1i) {
                    y = -1i;
                    loop {
                        let _e256 = y;
                        if (_e256 <= 1i) {
                            let _e258 = currentDepth;
                            let _e259 = sc;
                            let _e261 = x;
                            let _e263 = y;
                            let _e266 = texelSize;
                            let _e268 = (_e259.xy + (vec2<f32>(f32(_e261), f32(_e263)) * _e266));
                            let _e269 = layer;
                            let _e272 = vec3<f32>(_e268.x, _e268.y, _e269);
                            let _e278 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e272.x, _e272.y), i32(_e272.z));
                            let _e281 = shadow;
                            shadow = (_e281 + step(_e258, _e278.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e283 = y;
                            y = (_e283 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e285 = x;
                    x = (_e285 + 1i);
                }
            }
            let _e287 = shadow;
            shadow = (_e287 / 9f);
        }
    }
    let _e289 = shadow;
    return _e289;
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

    let _e80 = unnamed.cascadeSplits;
    let _e81 = (*viewDepth_1);
    cmp = step(_e80, vec4(_e81));
    let _e85 = cmp[0u];
    let _e87 = cmp[1u];
    let _e90 = cmp[2u];
    let _e93 = cmp[3u];
    cascade = min(i32((((_e85 + _e87) + _e90) + _e93)), 3i);
    let _e97 = cascade;
    (*outCascade) = _e97;
    let _e98 = cascade;
    if (_e98 == 0i) {
        local = 0f;
    } else {
        let _e100 = cascade;
        let _e105 = unnamed.cascadeSplits[max((_e100 - 1i), 0i)];
        local = _e105;
    }
    let _e106 = local;
    prevSplit = _e106;
    let _e107 = cascade;
    let _e110 = unnamed.cascadeSplits[_e107];
    farSplit = _e110;
    let _e111 = farSplit;
    let _e112 = prevSplit;
    blendRange = max((0.1f * (_e111 - _e112)), 1f);
    let _e116 = farSplit;
    let _e117 = (*viewDepth_1);
    let _e119 = blendRange;
    blendT = clamp(((_e116 - _e117) / _e119), 0f, 1f);
    let _e122 = cascade;
    param = _e122;
    let _e123 = (*worldPos_1);
    param_1 = _e123;
    let _e124 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e124;
    let _e125 = cascade;
    param_2 = min((_e125 + 1i), 3i);
    let _e128 = (*worldPos_1);
    param_3 = _e128;
    let _e129 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e129;
    let _e130 = s1_;
    let _e131 = s0_;
    let _e132 = blendT;
    return mix(_e130, _e131, _e132);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e74 = unnamed.packed_indices[1i][3u];
    let _e80 = unnamed.packed_indices[1i][3u];
    let _e85 = (*lm_uv);
    let _e86 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    sun_mask = _e86.x;
    let _e88 = sun_mask;
    if (_e88 > 0.001f) {
        let _e90 = shadowData_1;
        param_4 = _e90.xyz;
        let _e93 = shadowData_1[3u];
        param_5 = _e93;
        let _e94 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e95 = param_6;
        ignoredCascade = _e95;
        shadow_1 = _e94;
        let _e96 = shadow_1;
        let _e97 = sun_mask;
        let _e99 = (*rgb);
        (*rgb) = (_e99 * mix(1f, _e96, _e97));
    }
    let _e101 = (*rgb);
    return _e101;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e68 = (*rgb_1);
        param_7 = _e68;
        let _e69 = frag_tex_coord0_1;
        param_8 = _e69;
        let _e70 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e70;
    }
    if override_type_3_3 {
        let _e71 = (*rgb_1);
        param_9 = _e71;
        let _e72 = frag_tex_coord1_1;
        param_10 = _e72;
        let _e73 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e73;
    }
    let _e74 = (*rgb_1);
    return _e74;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e67 = (*c_1);
    (*c_1) = max(_e67, vec3<f32>(0f, 0f, 0f));
    let _e69 = (*c_1);
    cutoff = (_e69 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e71 = (*c_1);
    lo = (_e71 / vec3(12.92f));
    let _e74 = (*c_1);
    hi = pow(((_e74 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e79 = hi;
    let _e80 = lo;
    let _e81 = cutoff;
    return mix(_e79, _e80, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e81));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e68 = (*role);
    let _e70 = (*role);
    let _e75 = unnamed.packed_indices[(_e68 / 4u)][(_e70 % 4u)];
    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e90 = (*uv);
    let _e91 = textureSample(wired_bindless_images[(_e75 & 4095u)], wired_bindless_samplers[((_e85 >> bitcast<u32>(12i)) & 255u)], _e90);
    c_2 = _e91;
    let _e92 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e92))) == 0i) {
        let _e97 = c_2;
        param_11 = _e97.xyz;
        let _e99 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e99.x;
        c_2[1u] = _e99.y;
        c_2[2u] = _e99.z;
    }
    let _e106 = (*slot);
    if (lightmap_slot == (_e106 + 1i)) {
        let _e111 = unnamed.worldLightParams[0u];
        let _e112 = c_2;
        let _e114 = (_e112.xyz * _e111);
        c_2[0u] = _e114.x;
        c_2[1u] = _e114.y;
        c_2[2u] = _e114.z;
    }
    let _e121 = c_2;
    return _e121;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_2: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var param_24: vec3<f32>;
    var fogAmount: f32;

    param_12 = 0u;
    let _e82 = frag_tex_coord0_1;
    param_13 = _e82;
    param_14 = 0i;
    let _e83 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    color0_ = _e83;
    if override_type_3_4 {
        param_15 = 1u;
        let _e84 = frag_tex_coord1_1;
        param_16 = _e84;
        param_17 = 1i;
        let _e85 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        color1_ = _e85;
        let _e86 = color0_;
        let _e88 = color1_;
        let _e90 = (_e86.xyz + _e88.xyz);
        let _e92 = color0_[3u];
        let _e94 = color1_[3u];
        base = vec4<f32>(_e90.x, _e90.y, _e90.z, (_e92 * _e94));
    } else {
        if override_type_3_5 {
            param_18 = 1u;
            let _e100 = frag_tex_coord1_1;
            param_19 = _e100;
            param_20 = 1i;
            let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            color1_1 = _e101;
            let _e102 = color0_;
            let _e104 = color1_1;
            let _e106 = (_e102.xyz + _e104.xyz);
            let _e108 = color0_[3u];
            let _e110 = color1_1[3u];
            base = vec4<f32>(_e106.x, _e106.y, _e106.z, (_e108 * _e110));
        } else {
            param_21 = 1u;
            let _e116 = frag_tex_coord1_1;
            param_22 = _e116;
            param_23 = 1i;
            let _e117 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            color1_2 = _e117;
            let _e118 = color0_;
            let _e120 = color1_2;
            let _e122 = (_e118.xyz * _e120.xyz);
            base[0u] = _e122.x;
            base[1u] = _e122.y;
            base[2u] = _e122.z;
            let _e130 = color0_[3u];
            let _e132 = color1_2[3u];
            base[3u] = (_e130 * _e132);
        }
    }
    let _e135 = base;
    param_24 = _e135.xyz;
    let _e137 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_24));
    base[0u] = _e137.x;
    base[1u] = _e137.y;
    base[2u] = _e137.z;
    let _e144 = wired_advanced_fog_enabled_u0028_();
    if _e144 {
        let _e145 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e145;
        let _e146 = base;
        let _e149 = unnamed.advancedFogColorDensity;
        let _e151 = fogAmount;
        let _e153 = mix(_e146.xyz, _e149.xyz, vec3(_e151));
        base[0u] = _e153.x;
        base[1u] = _e153.y;
        base[2u] = _e153.z;
    }
    if override_type_3_6 {
        let _e161 = base[3u];
        if (_e161 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e163 = base;
            let _e165 = base;
            if (dot(_e163.xyz, _e165.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e169 = base;
    out_color = _e169;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e9 = out_color;
    return _e9;
}
