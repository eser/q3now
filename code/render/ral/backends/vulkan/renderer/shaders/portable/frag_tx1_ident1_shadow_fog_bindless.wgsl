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
@id(10) override acff: i32 = 0i;
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_12: bool = (discard_mode == 1i);
override override_type_3_13: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e75 + 0.5f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e82 = fogType;
    let _e85 = fogType;
    return (((_e80 > 0.5f) && (_e82 >= 1i)) && (_e85 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e75 = wired_advanced_fog_enabled_u0028_();
    if !(_e75) {
        return 0f;
    }
    let _e78 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e78, 0.000001f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e83 + 0.5f));
    let _e86 = fogType_1;
    if (_e86 == 1i) {
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e90 <= 0f) {
            return 0f;
        }
        let _e92 = viewDepth;
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e92 / _e95), 0f, 1f);
    }
    let _e100 = unnamed.advancedFogColorDensity[3u];
    let _e102 = viewDepth;
    opticalDepth = (max(_e100, 0f) * _e102);
    let _e104 = fogType_1;
    if (_e104 == 2i) {
        let _e106 = opticalDepth;
        return clamp((1f - exp(-(_e106))), 0f, 1f);
    }
    let _e111 = opticalDepth;
    let _e112 = opticalDepth;
    return clamp((1f - exp(-((_e111 * _e112)))), 0f, 1f);
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

    let _e82 = (*c);
    let _e85 = unnamed.cascadeMVP[_e82];
    let _e86 = (*worldPos);
    sc4_ = (_e85 * vec4<f32>(_e86.x, _e86.y, _e86.z, 1f));
    let _e92 = sc4_;
    let _e95 = sc4_[3u];
    sc = (_e92.xyz / vec3(_e95));
    let _e98 = sc;
    let _e102 = ((_e98.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e102.x;
    sc[1u] = _e102.y;
    let _e108 = sc[0u];
    let _e109 = (_e108 < 0f);
    phi_304_ = _e109;
    if !(_e109) {
        let _e112 = sc[0u];
        phi_304_ = (_e112 > 1f);
    }
    let _e115 = phi_304_;
    phi_311_ = _e115;
    if !(_e115) {
        let _e118 = sc[1u];
        phi_311_ = (_e118 < 0f);
    }
    let _e121 = phi_311_;
    phi_318_ = _e121;
    if !(_e121) {
        let _e124 = sc[1u];
        phi_318_ = (_e124 > 1f);
    }
    let _e127 = phi_318_;
    phi_325_ = _e127;
    if !(_e127) {
        let _e130 = sc[2u];
        phi_325_ = (_e130 > 1f);
    }
    let _e133 = phi_325_;
    if _e133 {
        return 1f;
    }
    let _e134 = (*c);
    layer = f32(_e134);
    let _e136 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e136).xy));
    let _e143 = sc[2u];
    currentDepth = (_e143 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e145 = currentDepth;
        let _e146 = sc;
        let _e147 = _e146.xy;
        let _e148 = layer;
        let _e151 = vec3<f32>(_e147.x, _e147.y, _e148);
        let _e157 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e151.x, _e151.y), i32(_e151.z));
        shadow = step(_e145, _e157.x);
    } else {
        if override_type_3_1 {
            let _e160 = currentDepth;
            let _e161 = sc;
            let _e162 = _e161.xy;
            let _e163 = layer;
            let _e166 = vec3<f32>(_e162.x, _e162.y, _e163);
            let _e172 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e166.x, _e166.y), i32(_e166.z));
            let _e175 = shadow;
            shadow = (_e175 + step(_e160, _e172.x));
            let _e177 = currentDepth;
            let _e178 = sc;
            let _e181 = texelSize[0u];
            let _e183 = (_e178.xy + vec2<f32>(_e181, 0f));
            let _e184 = layer;
            let _e187 = vec3<f32>(_e183.x, _e183.y, _e184);
            let _e193 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e187.x, _e187.y), i32(_e187.z));
            let _e196 = shadow;
            shadow = (_e196 + step(_e177, _e193.x));
            let _e198 = currentDepth;
            let _e199 = sc;
            let _e202 = texelSize[0u];
            let _e204 = (_e199.xy - vec2<f32>(_e202, 0f));
            let _e205 = layer;
            let _e208 = vec3<f32>(_e204.x, _e204.y, _e205);
            let _e214 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e208.x, _e208.y), i32(_e208.z));
            let _e217 = shadow;
            shadow = (_e217 + step(_e198, _e214.x));
            let _e219 = currentDepth;
            let _e220 = sc;
            let _e223 = texelSize[1u];
            let _e225 = (_e220.xy + vec2<f32>(0f, _e223));
            let _e226 = layer;
            let _e229 = vec3<f32>(_e225.x, _e225.y, _e226);
            let _e235 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e229.x, _e229.y), i32(_e229.z));
            let _e238 = shadow;
            shadow = (_e238 + step(_e219, _e235.x));
            let _e240 = currentDepth;
            let _e241 = sc;
            let _e244 = texelSize[1u];
            let _e246 = (_e241.xy - vec2<f32>(0f, _e244));
            let _e247 = layer;
            let _e250 = vec3<f32>(_e246.x, _e246.y, _e247);
            let _e256 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e250.x, _e250.y), i32(_e250.z));
            let _e259 = shadow;
            shadow = (_e259 + step(_e240, _e256.x));
            let _e261 = shadow;
            shadow = (_e261 / 5f);
        } else {
            x = -1i;
            loop {
                let _e263 = x;
                if (_e263 <= 1i) {
                    y = -1i;
                    loop {
                        let _e265 = y;
                        if (_e265 <= 1i) {
                            let _e267 = currentDepth;
                            let _e268 = sc;
                            let _e270 = x;
                            let _e272 = y;
                            let _e275 = texelSize;
                            let _e277 = (_e268.xy + (vec2<f32>(f32(_e270), f32(_e272)) * _e275));
                            let _e278 = layer;
                            let _e281 = vec3<f32>(_e277.x, _e277.y, _e278);
                            let _e287 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e281.x, _e281.y), i32(_e281.z));
                            let _e290 = shadow;
                            shadow = (_e290 + step(_e267, _e287.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e292 = y;
                            y = (_e292 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e294 = x;
                    x = (_e294 + 1i);
                }
            }
            let _e296 = shadow;
            shadow = (_e296 / 9f);
        }
    }
    let _e298 = shadow;
    return _e298;
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

    let _e89 = unnamed.cascadeSplits;
    let _e90 = (*viewDepth_1);
    cmp = step(_e89, vec4(_e90));
    let _e94 = cmp[0u];
    let _e96 = cmp[1u];
    let _e99 = cmp[2u];
    let _e102 = cmp[3u];
    cascade = min(i32((((_e94 + _e96) + _e99) + _e102)), 3i);
    let _e106 = cascade;
    (*outCascade) = _e106;
    let _e107 = cascade;
    if (_e107 == 0i) {
        local = 0f;
    } else {
        let _e109 = cascade;
        let _e114 = unnamed.cascadeSplits[max((_e109 - 1i), 0i)];
        local = _e114;
    }
    let _e115 = local;
    prevSplit = _e115;
    let _e116 = cascade;
    let _e119 = unnamed.cascadeSplits[_e116];
    farSplit = _e119;
    let _e120 = farSplit;
    let _e121 = prevSplit;
    blendRange = max((0.1f * (_e120 - _e121)), 1f);
    let _e125 = farSplit;
    let _e126 = (*viewDepth_1);
    let _e128 = blendRange;
    blendT = clamp(((_e125 - _e126) / _e128), 0f, 1f);
    let _e131 = cascade;
    param = _e131;
    let _e132 = (*worldPos_1);
    param_1 = _e132;
    let _e133 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e133;
    let _e134 = cascade;
    param_2 = min((_e134 + 1i), 3i);
    let _e137 = (*worldPos_1);
    param_3 = _e137;
    let _e138 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e138;
    let _e139 = s1_;
    let _e140 = s0_;
    let _e141 = blendT;
    return mix(_e139, _e140, _e141);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e83 = unnamed.packed_indices[1i][3u];
    let _e89 = unnamed.packed_indices[1i][3u];
    let _e94 = (*lm_uv);
    let _e95 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    sun_mask = _e95.x;
    let _e97 = sun_mask;
    if (_e97 > 0.001f) {
        let _e99 = shadowData_1;
        param_4 = _e99.xyz;
        let _e102 = shadowData_1[3u];
        param_5 = _e102;
        let _e103 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e104 = param_6;
        ignoredCascade = _e104;
        shadow_1 = _e103;
        let _e105 = shadow_1;
        let _e106 = sun_mask;
        let _e108 = (*rgb);
        (*rgb) = (_e108 * mix(1f, _e105, _e106));
    }
    let _e110 = (*rgb);
    return _e110;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e77 = (*rgb_1);
        param_7 = _e77;
        let _e78 = frag_tex_coord0_1;
        param_8 = _e78;
        let _e79 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e79;
    }
    if override_type_3_3 {
        let _e80 = (*rgb_1);
        param_9 = _e80;
        let _e81 = frag_tex_coord1_1;
        param_10 = _e81;
        let _e82 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e82;
    }
    let _e83 = (*rgb_1);
    return _e83;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e76 = (*c_1);
    (*c_1) = max(_e76, vec3<f32>(0f, 0f, 0f));
    let _e78 = (*c_1);
    cutoff = (_e78 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e80 = (*c_1);
    lo = (_e80 / vec3(12.92f));
    let _e83 = (*c_1);
    hi = pow(((_e83 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e88 = hi;
    let _e89 = lo;
    let _e90 = cutoff;
    return mix(_e88, _e89, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e90));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e99 = (*uv);
    let _e100 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    c_2 = _e100;
    let _e101 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e101))) == 0i) {
        let _e106 = c_2;
        param_11 = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e108.x;
        c_2[1u] = _e108.y;
        c_2[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e120 = unnamed.worldLightParams[0u];
        let _e121 = c_2;
        let _e123 = (_e121.xyz * _e120);
        c_2[0u] = _e123.x;
        c_2[1u] = _e123.y;
        c_2[2u] = _e123.z;
    }
    let _e130 = c_2;
    return _e130;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e95 = unnamed.packed_indices[0i][3u];
    let _e101 = unnamed.packed_indices[0i][3u];
    let _e106 = fog_tex_coord_1;
    let _e107 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e101 >> bitcast<u32>(12i)) & 255u)], _e106);
    fog = _e107;
    param_12 = 0u;
    let _e108 = frag_tex_coord0_1;
    param_13 = _e108;
    param_14 = 0i;
    let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    color0_ = _e109;
    if override_type_3_4 {
        param_15 = 1u;
        let _e110 = frag_tex_coord1_1;
        param_16 = _e110;
        param_17 = 1i;
        let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        color1_ = _e111;
        let _e112 = color0_;
        let _e114 = color1_;
        let _e116 = (_e112.xyz + _e114.xyz);
        let _e118 = color0_[3u];
        let _e120 = color1_[3u];
        base = vec4<f32>(_e116.x, _e116.y, _e116.z, (_e118 * _e120));
    } else {
        if override_type_3_5 {
            param_18 = 1u;
            let _e126 = frag_tex_coord1_1;
            param_19 = _e126;
            param_20 = 1i;
            let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            color1_1 = _e127;
            let _e128 = color0_;
            let _e130 = color1_1;
            let _e132 = (_e128.xyz + _e130.xyz);
            let _e134 = color0_[3u];
            let _e136 = color1_1[3u];
            base = vec4<f32>(_e132.x, _e132.y, _e132.z, (_e134 * _e136));
        } else {
            param_21 = 1u;
            let _e142 = frag_tex_coord1_1;
            param_22 = _e142;
            param_23 = 1i;
            let _e143 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            color1_2 = _e143;
            let _e144 = color0_;
            let _e146 = color1_2;
            let _e148 = (_e144.xyz * _e146.xyz);
            base[0u] = _e148.x;
            base[1u] = _e148.y;
            base[2u] = _e148.z;
            let _e156 = color0_[3u];
            let _e158 = color1_2[3u];
            base[3u] = (_e156 * _e158);
        }
    }
    let _e161 = base;
    param_24 = _e161.xyz;
    let _e163 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_24));
    base[0u] = _e163.x;
    base[1u] = _e163.y;
    base[2u] = _e163.z;
    let _e170 = wired_advanced_fog_enabled_u0028_();
    if _e170 {
        let _e171 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e171;
        if override_type_3_6 {
            let _e172 = fogAmount;
            let _e174 = base;
            let _e176 = (_e174.xyz * (1f - _e172));
            base[0u] = _e176.x;
            base[1u] = _e176.y;
            base[2u] = _e176.z;
        } else {
            if override_type_3_7 {
                let _e183 = fogAmount;
                let _e185 = base;
                base = (_e185 * (1f - _e183));
            } else {
                if override_type_3_8 {
                    let _e187 = fogAmount;
                    let _e190 = base[3u];
                    base[3u] = (_e190 * (1f - _e187));
                } else {
                    let _e193 = base;
                    let _e196 = unnamed.advancedFogColorDensity;
                    let _e198 = fogAmount;
                    let _e200 = mix(_e193.xyz, _e196.xyz, vec3(_e198));
                    base[0u] = _e200.x;
                    base[1u] = _e200.y;
                    base[2u] = _e200.z;
                }
            }
        }
    } else {
        if override_type_3_9 {
            let _e207 = base;
            let _e210 = fog[3u];
            let _e212 = (_e207.xyz * (1f - _e210));
            base[0u] = _e212.x;
            base[1u] = _e212.y;
            base[2u] = _e212.z;
        } else {
            if override_type_3_10 {
                let _e219 = base;
                let _e221 = fog[3u];
                base = (_e219 * (1f - _e221));
            } else {
                if override_type_3_11 {
                    let _e225 = base[3u];
                    let _e227 = fog[3u];
                    base[3u] = (_e225 * (1f - _e227));
                } else {
                    let _e231 = base;
                    let _e232 = fog;
                    let _e234 = unnamed.fogColor;
                    let _e237 = fog[3u];
                    base = mix(_e231, (_e232 * _e234), vec4(_e237));
                }
            }
        }
    }
    if override_type_3_12 {
        let _e241 = base[3u];
        if (_e241 == 0f) {
            discard;
        }
    } else {
        if override_type_3_13 {
            let _e243 = base;
            let _e245 = base;
            if (dot(_e243.xyz, _e245.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e249 = base;
    out_color = _e249;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e11 = out_color;
    return _e11;
}
