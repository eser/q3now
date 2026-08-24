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
override override_type_3_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_5: bool = (tex_mode == 1i);
override override_type_3_6: bool = (tex_mode == 2i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_7: bool = (discard_mode == 1i);
override override_type_3_8: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
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

    let _e76 = (*c);
    let _e79 = unnamed.cascadeMVP[_e76];
    let _e80 = (*worldPos);
    sc4_ = (_e79 * vec4<f32>(_e80.x, _e80.y, _e80.z, 1f));
    let _e86 = sc4_;
    let _e89 = sc4_[3u];
    sc = (_e86.xyz / vec3(_e89));
    let _e92 = sc;
    let _e96 = ((_e92.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e96.x;
    sc[1u] = _e96.y;
    let _e102 = sc[0u];
    let _e103 = (_e102 < 0f);
    phi_304_ = _e103;
    if !(_e103) {
        let _e106 = sc[0u];
        phi_304_ = (_e106 > 1f);
    }
    let _e109 = phi_304_;
    phi_311_ = _e109;
    if !(_e109) {
        let _e112 = sc[1u];
        phi_311_ = (_e112 < 0f);
    }
    let _e115 = phi_311_;
    phi_318_ = _e115;
    if !(_e115) {
        let _e118 = sc[1u];
        phi_318_ = (_e118 > 1f);
    }
    let _e121 = phi_318_;
    phi_325_ = _e121;
    if !(_e121) {
        let _e124 = sc[2u];
        phi_325_ = (_e124 > 1f);
    }
    let _e127 = phi_325_;
    if _e127 {
        return 1f;
    }
    let _e128 = (*c);
    layer = f32(_e128);
    let _e130 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e130).xy));
    let _e137 = sc[2u];
    currentDepth = (_e137 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e139 = currentDepth;
        let _e140 = sc;
        let _e141 = _e140.xy;
        let _e142 = layer;
        let _e145 = vec3<f32>(_e141.x, _e141.y, _e142);
        let _e151 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e145.x, _e145.y), i32(_e145.z));
        shadow = step(_e139, _e151.x);
    } else {
        if override_type_3_1 {
            let _e154 = currentDepth;
            let _e155 = sc;
            let _e156 = _e155.xy;
            let _e157 = layer;
            let _e160 = vec3<f32>(_e156.x, _e156.y, _e157);
            let _e166 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e160.x, _e160.y), i32(_e160.z));
            let _e169 = shadow;
            shadow = (_e169 + step(_e154, _e166.x));
            let _e171 = currentDepth;
            let _e172 = sc;
            let _e175 = texelSize[0u];
            let _e177 = (_e172.xy + vec2<f32>(_e175, 0f));
            let _e178 = layer;
            let _e181 = vec3<f32>(_e177.x, _e177.y, _e178);
            let _e187 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e181.x, _e181.y), i32(_e181.z));
            let _e190 = shadow;
            shadow = (_e190 + step(_e171, _e187.x));
            let _e192 = currentDepth;
            let _e193 = sc;
            let _e196 = texelSize[0u];
            let _e198 = (_e193.xy - vec2<f32>(_e196, 0f));
            let _e199 = layer;
            let _e202 = vec3<f32>(_e198.x, _e198.y, _e199);
            let _e208 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e202.x, _e202.y), i32(_e202.z));
            let _e211 = shadow;
            shadow = (_e211 + step(_e192, _e208.x));
            let _e213 = currentDepth;
            let _e214 = sc;
            let _e217 = texelSize[1u];
            let _e219 = (_e214.xy + vec2<f32>(0f, _e217));
            let _e220 = layer;
            let _e223 = vec3<f32>(_e219.x, _e219.y, _e220);
            let _e229 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e223.x, _e223.y), i32(_e223.z));
            let _e232 = shadow;
            shadow = (_e232 + step(_e213, _e229.x));
            let _e234 = currentDepth;
            let _e235 = sc;
            let _e238 = texelSize[1u];
            let _e240 = (_e235.xy - vec2<f32>(0f, _e238));
            let _e241 = layer;
            let _e244 = vec3<f32>(_e240.x, _e240.y, _e241);
            let _e250 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e244.x, _e244.y), i32(_e244.z));
            let _e253 = shadow;
            shadow = (_e253 + step(_e234, _e250.x));
            let _e255 = shadow;
            shadow = (_e255 / 5f);
        } else {
            x = -1i;
            loop {
                let _e257 = x;
                if (_e257 <= 1i) {
                    y = -1i;
                    loop {
                        let _e259 = y;
                        if (_e259 <= 1i) {
                            let _e261 = currentDepth;
                            let _e262 = sc;
                            let _e264 = x;
                            let _e266 = y;
                            let _e269 = texelSize;
                            let _e271 = (_e262.xy + (vec2<f32>(f32(_e264), f32(_e266)) * _e269));
                            let _e272 = layer;
                            let _e275 = vec3<f32>(_e271.x, _e271.y, _e272);
                            let _e281 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e275.x, _e275.y), i32(_e275.z));
                            let _e284 = shadow;
                            shadow = (_e284 + step(_e261, _e281.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e286 = y;
                            y = (_e286 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e288 = x;
                    x = (_e288 + 1i);
                }
            }
            let _e290 = shadow;
            shadow = (_e290 / 9f);
        }
    }
    let _e292 = shadow;
    return _e292;
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

    let _e83 = unnamed.cascadeSplits;
    let _e84 = (*viewDepth_1);
    cmp = step(_e83, vec4(_e84));
    let _e88 = cmp[0u];
    let _e90 = cmp[1u];
    let _e93 = cmp[2u];
    let _e96 = cmp[3u];
    cascade = min(i32((((_e88 + _e90) + _e93) + _e96)), 3i);
    let _e100 = cascade;
    (*outCascade) = _e100;
    let _e101 = cascade;
    if (_e101 == 0i) {
        local = 0f;
    } else {
        let _e103 = cascade;
        let _e108 = unnamed.cascadeSplits[max((_e103 - 1i), 0i)];
        local = _e108;
    }
    let _e109 = local;
    prevSplit = _e109;
    let _e110 = cascade;
    let _e113 = unnamed.cascadeSplits[_e110];
    farSplit = _e113;
    let _e114 = farSplit;
    let _e115 = prevSplit;
    blendRange = max((0.1f * (_e114 - _e115)), 1f);
    let _e119 = farSplit;
    let _e120 = (*viewDepth_1);
    let _e122 = blendRange;
    blendT = clamp(((_e119 - _e120) / _e122), 0f, 1f);
    let _e125 = cascade;
    param = _e125;
    let _e126 = (*worldPos_1);
    param_1 = _e126;
    let _e127 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e127;
    let _e128 = cascade;
    param_2 = min((_e128 + 1i), 3i);
    let _e131 = (*worldPos_1);
    param_3 = _e131;
    let _e132 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e132;
    let _e133 = s1_;
    let _e134 = s0_;
    let _e135 = blendT;
    return mix(_e133, _e134, _e135);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e77 = unnamed.packed_indices[1i][3u];
    let _e83 = unnamed.packed_indices[1i][3u];
    let _e88 = (*lm_uv);
    let _e89 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    sun_mask = _e89.x;
    let _e91 = sun_mask;
    if (_e91 > 0.001f) {
        let _e93 = shadowData_1;
        param_4 = _e93.xyz;
        let _e96 = shadowData_1[3u];
        param_5 = _e96;
        let _e97 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e98 = param_6;
        ignoredCascade = _e98;
        shadow_1 = _e97;
        let _e99 = shadow_1;
        let _e100 = sun_mask;
        let _e102 = (*rgb);
        (*rgb) = (_e102 * mix(1f, _e99, _e100));
    }
    let _e104 = (*rgb);
    return _e104;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_3_2 {
        let _e73 = (*rgb_1);
        param_7 = _e73;
        let _e74 = frag_tex_coord0_1;
        param_8 = _e74;
        let _e75 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e75;
    }
    if override_type_3_3 {
        let _e76 = (*rgb_1);
        param_9 = _e76;
        let _e77 = frag_tex_coord1_1;
        param_10 = _e77;
        let _e78 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e78;
    }
    if override_type_3_4 {
        let _e79 = (*rgb_1);
        param_11 = _e79;
        let _e80 = frag_tex_coord2_1;
        param_12 = _e80;
        let _e81 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e81;
    }
    let _e82 = (*rgb_1);
    return _e82;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c_1);
    (*c_1) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c_1);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c_1);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c_1);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_2 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_2;
        param_13 = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e102.x;
        c_2[1u] = _e102.y;
        c_2[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e114 = unnamed.worldLightParams[0u];
        let _e115 = c_2;
        let _e117 = (_e115.xyz * _e114);
        c_2[0u] = _e117.x;
        c_2[1u] = _e117.y;
        c_2[2u] = _e117.z;
    }
    let _e124 = c_2;
    return _e124;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_14: vec3<f32>;
    var color0_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color2_: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color2_1: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color1_2: vec4<f32>;
    var param_30: u32;
    var param_31: vec2<f32>;
    var param_32: i32;
    var color2_2: vec4<f32>;
    var param_33: u32;
    var param_34: vec2<f32>;
    var param_35: i32;
    var param_36: vec3<f32>;
    var fogAmount: f32;

    let _e99 = frag_color0In_1;
    param_14 = _e99.xyz;
    let _e101 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e103 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e101.x, _e101.y, _e101.z, _e103);
    param_15 = 0u;
    let _e108 = frag_tex_coord0_1;
    param_16 = _e108;
    param_17 = 0i;
    let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
    let _e110 = frag_color0_;
    color0_ = (_e109 * _e110);
    if override_type_3_5 {
        param_18 = 1u;
        let _e112 = frag_tex_coord1_1;
        param_19 = _e112;
        param_20 = 1i;
        let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
        color1_ = _e113;
        param_21 = 2u;
        let _e114 = frag_tex_coord2_1;
        param_22 = _e114;
        param_23 = 2i;
        let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
        color2_ = _e115;
        let _e116 = color0_;
        let _e118 = color1_;
        let _e121 = color2_;
        let _e123 = ((_e116.xyz + _e118.xyz) + _e121.xyz);
        let _e125 = color0_[3u];
        let _e127 = color1_[3u];
        let _e130 = color2_[3u];
        base = vec4<f32>(_e123.x, _e123.y, _e123.z, ((_e125 * _e127) * _e130));
    } else {
        if override_type_3_6 {
            param_24 = 1u;
            let _e136 = frag_tex_coord1_1;
            param_25 = _e136;
            param_26 = 1i;
            let _e137 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
            let _e138 = frag_color0_;
            color1_1 = (_e137 * _e138);
            param_27 = 2u;
            let _e140 = frag_tex_coord2_1;
            param_28 = _e140;
            param_29 = 2i;
            let _e141 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
            let _e142 = frag_color0_;
            color2_1 = (_e141 * _e142);
            let _e144 = color0_;
            let _e146 = color1_1;
            let _e149 = color2_1;
            let _e151 = ((_e144.xyz + _e146.xyz) + _e149.xyz);
            let _e153 = color0_[3u];
            let _e155 = color1_1[3u];
            let _e158 = color2_1[3u];
            base = vec4<f32>(_e151.x, _e151.y, _e151.z, ((_e153 * _e155) * _e158));
        } else {
            param_30 = 1u;
            let _e164 = frag_tex_coord1_1;
            param_31 = _e164;
            param_32 = 1i;
            let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
            color1_2 = _e165;
            param_33 = 2u;
            let _e166 = frag_tex_coord2_1;
            param_34 = _e166;
            param_35 = 2i;
            let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_33), (&param_34), (&param_35));
            color2_2 = _e167;
            let _e168 = color0_;
            let _e170 = color1_2;
            let _e173 = color2_2;
            let _e175 = ((_e168.xyz * _e170.xyz) * _e173.xyz);
            base[0u] = _e175.x;
            base[1u] = _e175.y;
            base[2u] = _e175.z;
            let _e183 = color0_[3u];
            let _e185 = color1_2[3u];
            let _e188 = color2_2[3u];
            base[3u] = ((_e183 * _e185) * _e188);
        }
    }
    let _e191 = base;
    param_36 = _e191.xyz;
    let _e193 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_36));
    base[0u] = _e193.x;
    base[1u] = _e193.y;
    base[2u] = _e193.z;
    let _e200 = wired_advanced_fog_enabled_u0028_();
    if _e200 {
        let _e201 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e201;
        let _e202 = base;
        let _e205 = unnamed.advancedFogColorDensity;
        let _e207 = fogAmount;
        let _e209 = mix(_e202.xyz, _e205.xyz, vec3(_e207));
        base[0u] = _e209.x;
        base[1u] = _e209.y;
        base[2u] = _e209.z;
    }
    if override_type_3_7 {
        let _e217 = base[3u];
        if (_e217 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
            let _e219 = base;
            let _e221 = base;
            if (dot(_e219.xyz, _e221.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e225 = base;
    out_color = _e225;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e13 = out_color;
    return _e13;
}
