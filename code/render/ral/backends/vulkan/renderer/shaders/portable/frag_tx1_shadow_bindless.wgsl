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
    emissionRadiance: vec4<f32>,
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
override override_type_3_6: bool = (lightmap_slot != 0i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e77 + 0.5f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e84 = fogType;
    let _e87 = fogType;
    return (((_e82 > 0.5f) && (_e84 >= 1i)) && (_e87 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e77 = wired_advanced_fog_enabled_u0028_();
    if !(_e77) {
        return 0f;
    }
    let _e80 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e80, 0.000001f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e85 + 0.5f));
    let _e88 = fogType_1;
    if (_e88 == 1i) {
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e92 <= 0f) {
            return 0f;
        }
        let _e94 = viewDepth;
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e94 / _e97), 0f, 1f);
    }
    let _e102 = unnamed.advancedFogColorDensity[3u];
    let _e104 = viewDepth;
    opticalDepth = (max(_e102, 0f) * _e104);
    let _e106 = fogType_1;
    if (_e106 == 2i) {
        let _e108 = opticalDepth;
        return clamp((1f - exp(-(_e108))), 0f, 1f);
    }
    let _e113 = opticalDepth;
    let _e114 = opticalDepth;
    return clamp((1f - exp(-((_e113 * _e114)))), 0f, 1f);
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

    let _e84 = (*c);
    let _e87 = unnamed.cascadeMVP[_e84];
    let _e88 = (*worldPos);
    sc4_ = (_e87 * vec4<f32>(_e88.x, _e88.y, _e88.z, 1f));
    let _e94 = sc4_;
    let _e97 = sc4_[3u];
    sc = (_e94.xyz / vec3(_e97));
    let _e100 = sc;
    let _e104 = ((_e100.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e104.x;
    sc[1u] = _e104.y;
    let _e110 = sc[0u];
    let _e111 = (_e110 < 0f);
    phi_304_ = _e111;
    if !(_e111) {
        let _e114 = sc[0u];
        phi_304_ = (_e114 > 1f);
    }
    let _e117 = phi_304_;
    phi_311_ = _e117;
    if !(_e117) {
        let _e120 = sc[1u];
        phi_311_ = (_e120 < 0f);
    }
    let _e123 = phi_311_;
    phi_318_ = _e123;
    if !(_e123) {
        let _e126 = sc[1u];
        phi_318_ = (_e126 > 1f);
    }
    let _e129 = phi_318_;
    phi_325_ = _e129;
    if !(_e129) {
        let _e132 = sc[2u];
        phi_325_ = (_e132 > 1f);
    }
    let _e135 = phi_325_;
    if _e135 {
        return 1f;
    }
    let _e136 = (*c);
    layer = f32(_e136);
    let _e138 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e138).xy));
    let _e145 = sc[2u];
    currentDepth = (_e145 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e147 = currentDepth;
        let _e148 = sc;
        let _e149 = _e148.xy;
        let _e150 = layer;
        let _e153 = vec3<f32>(_e149.x, _e149.y, _e150);
        let _e159 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e153.x, _e153.y), i32(_e153.z));
        shadow = step(_e147, _e159.x);
    } else {
        if override_type_3_1 {
            let _e162 = currentDepth;
            let _e163 = sc;
            let _e164 = _e163.xy;
            let _e165 = layer;
            let _e168 = vec3<f32>(_e164.x, _e164.y, _e165);
            let _e174 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e168.x, _e168.y), i32(_e168.z));
            let _e177 = shadow;
            shadow = (_e177 + step(_e162, _e174.x));
            let _e179 = currentDepth;
            let _e180 = sc;
            let _e183 = texelSize[0u];
            let _e185 = (_e180.xy + vec2<f32>(_e183, 0f));
            let _e186 = layer;
            let _e189 = vec3<f32>(_e185.x, _e185.y, _e186);
            let _e195 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e189.x, _e189.y), i32(_e189.z));
            let _e198 = shadow;
            shadow = (_e198 + step(_e179, _e195.x));
            let _e200 = currentDepth;
            let _e201 = sc;
            let _e204 = texelSize[0u];
            let _e206 = (_e201.xy - vec2<f32>(_e204, 0f));
            let _e207 = layer;
            let _e210 = vec3<f32>(_e206.x, _e206.y, _e207);
            let _e216 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e210.x, _e210.y), i32(_e210.z));
            let _e219 = shadow;
            shadow = (_e219 + step(_e200, _e216.x));
            let _e221 = currentDepth;
            let _e222 = sc;
            let _e225 = texelSize[1u];
            let _e227 = (_e222.xy + vec2<f32>(0f, _e225));
            let _e228 = layer;
            let _e231 = vec3<f32>(_e227.x, _e227.y, _e228);
            let _e237 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e231.x, _e231.y), i32(_e231.z));
            let _e240 = shadow;
            shadow = (_e240 + step(_e221, _e237.x));
            let _e242 = currentDepth;
            let _e243 = sc;
            let _e246 = texelSize[1u];
            let _e248 = (_e243.xy - vec2<f32>(0f, _e246));
            let _e249 = layer;
            let _e252 = vec3<f32>(_e248.x, _e248.y, _e249);
            let _e258 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e252.x, _e252.y), i32(_e252.z));
            let _e261 = shadow;
            shadow = (_e261 + step(_e242, _e258.x));
            let _e263 = shadow;
            shadow = (_e263 / 5f);
        } else {
            x = -1i;
            loop {
                let _e265 = x;
                if (_e265 <= 1i) {
                    y = -1i;
                    loop {
                        let _e267 = y;
                        if (_e267 <= 1i) {
                            let _e269 = currentDepth;
                            let _e270 = sc;
                            let _e272 = x;
                            let _e274 = y;
                            let _e277 = texelSize;
                            let _e279 = (_e270.xy + (vec2<f32>(f32(_e272), f32(_e274)) * _e277));
                            let _e280 = layer;
                            let _e283 = vec3<f32>(_e279.x, _e279.y, _e280);
                            let _e289 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e283.x, _e283.y), i32(_e283.z));
                            let _e292 = shadow;
                            shadow = (_e292 + step(_e269, _e289.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e294 = y;
                            y = (_e294 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e296 = x;
                    x = (_e296 + 1i);
                }
            }
            let _e298 = shadow;
            shadow = (_e298 / 9f);
        }
    }
    let _e300 = shadow;
    return _e300;
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

    let _e91 = unnamed.cascadeSplits;
    let _e92 = (*viewDepth_1);
    cmp = step(_e91, vec4(_e92));
    let _e96 = cmp[0u];
    let _e98 = cmp[1u];
    let _e101 = cmp[2u];
    let _e104 = cmp[3u];
    cascade = min(i32((((_e96 + _e98) + _e101) + _e104)), 3i);
    let _e108 = cascade;
    (*outCascade) = _e108;
    let _e109 = cascade;
    if (_e109 == 0i) {
        local = 0f;
    } else {
        let _e111 = cascade;
        let _e116 = unnamed.cascadeSplits[max((_e111 - 1i), 0i)];
        local = _e116;
    }
    let _e117 = local;
    prevSplit = _e117;
    let _e118 = cascade;
    let _e121 = unnamed.cascadeSplits[_e118];
    farSplit = _e121;
    let _e122 = farSplit;
    let _e123 = prevSplit;
    blendRange = max((0.1f * (_e122 - _e123)), 1f);
    let _e127 = farSplit;
    let _e128 = (*viewDepth_1);
    let _e130 = blendRange;
    blendT = clamp(((_e127 - _e128) / _e130), 0f, 1f);
    let _e133 = cascade;
    param = _e133;
    let _e134 = (*worldPos_1);
    param_1 = _e134;
    let _e135 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e135;
    let _e136 = cascade;
    param_2 = min((_e136 + 1i), 3i);
    let _e139 = (*worldPos_1);
    param_3 = _e139;
    let _e140 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e140;
    let _e141 = s1_;
    let _e142 = s0_;
    let _e143 = blendT;
    return mix(_e141, _e142, _e143);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e85 = unnamed.packed_indices[1i][3u];
    let _e91 = unnamed.packed_indices[1i][3u];
    let _e96 = (*lm_uv);
    let _e97 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e91 >> bitcast<u32>(12i)) & 255u)], _e96);
    sun_mask = _e97.x;
    let _e99 = sun_mask;
    if (_e99 > 0.001f) {
        let _e101 = shadowData_1;
        param_4 = _e101.xyz;
        let _e104 = shadowData_1[3u];
        param_5 = _e104;
        let _e105 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e106 = param_6;
        ignoredCascade = _e106;
        shadow_1 = _e105;
        let _e107 = shadow_1;
        let _e108 = sun_mask;
        let _e110 = (*rgb);
        (*rgb) = (_e110 * mix(1f, _e107, _e108));
    }
    let _e112 = (*rgb);
    return _e112;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_3_2 {
        let _e79 = (*rgb_1);
        param_7 = _e79;
        let _e80 = frag_tex_coord0_1;
        param_8 = _e80;
        let _e81 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e81;
    }
    if override_type_3_3 {
        let _e82 = (*rgb_1);
        param_9 = _e82;
        let _e83 = frag_tex_coord1_1;
        param_10 = _e83;
        let _e84 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e84;
    }
    let _e85 = (*rgb_1);
    return _e85;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e78 = (*c_1);
    (*c_1) = max(_e78, vec3<f32>(0f, 0f, 0f));
    let _e80 = (*c_1);
    cutoff = (_e80 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e82 = (*c_1);
    lo = (_e82 / vec3(12.92f));
    let _e85 = (*c_1);
    hi = pow(((_e85 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e90 = hi;
    let _e91 = lo;
    let _e92 = cutoff;
    return mix(_e90, _e91, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e92));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e101 = (*uv);
    let _e102 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    c_2 = _e102;
    let _e103 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e103))) == 0i) {
        let _e108 = c_2;
        param_11 = _e108.xyz;
        let _e110 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e110.x;
        c_2[1u] = _e110.y;
        c_2[2u] = _e110.z;
    }
    let _e117 = (*slot);
    if (lightmap_slot == (_e117 + 1i)) {
        let _e122 = unnamed.worldLightParams[0u];
        let _e123 = c_2;
        let _e125 = (_e123.xyz * _e122);
        c_2[0u] = _e125.x;
        c_2[1u] = _e125.y;
        c_2[2u] = _e125.z;
    }
    let _e132 = c_2;
    return _e132;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_12: vec3<f32>;
    var color0_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var param_25: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e98 = frag_color0In_1;
    param_12 = _e98.xyz;
    let _e100 = sRGBToLinear_u0028_vf3_u003b((&param_12));
    let _e102 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e100.x, _e100.y, _e100.z, _e102);
    param_13 = 0u;
    let _e107 = frag_tex_coord0_1;
    param_14 = _e107;
    param_15 = 0i;
    let _e108 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    let _e109 = frag_color0_;
    color0_ = (_e108 * _e109);
    if override_type_3_4 {
        param_16 = 1u;
        let _e111 = frag_tex_coord1_1;
        param_17 = _e111;
        param_18 = 1i;
        let _e112 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
        color1_ = _e112;
        let _e113 = color0_;
        let _e115 = color1_;
        let _e117 = (_e113.xyz + _e115.xyz);
        let _e119 = color0_[3u];
        let _e121 = color1_[3u];
        base = vec4<f32>(_e117.x, _e117.y, _e117.z, (_e119 * _e121));
    } else {
        if override_type_3_5 {
            param_19 = 1u;
            let _e127 = frag_tex_coord1_1;
            param_20 = _e127;
            param_21 = 1i;
            let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            let _e129 = frag_color0_;
            color1_1 = (_e128 * _e129);
            let _e131 = color0_;
            let _e133 = color1_1;
            let _e135 = (_e131.xyz + _e133.xyz);
            let _e137 = color0_[3u];
            let _e139 = color1_1[3u];
            base = vec4<f32>(_e135.x, _e135.y, _e135.z, (_e137 * _e139));
        } else {
            param_22 = 1u;
            let _e145 = frag_tex_coord1_1;
            param_23 = _e145;
            param_24 = 1i;
            let _e146 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            color1_2 = _e146;
            let _e147 = color0_;
            let _e149 = color1_2;
            let _e151 = (_e147.xyz * _e149.xyz);
            base[0u] = _e151.x;
            base[1u] = _e151.y;
            base[2u] = _e151.z;
            let _e159 = color0_[3u];
            let _e161 = color1_2[3u];
            base[3u] = (_e159 * _e161);
        }
    }
    let _e164 = base;
    param_25 = _e164.xyz;
    let _e166 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_25));
    base[0u] = _e166.x;
    base[1u] = _e166.y;
    base[2u] = _e166.z;
    if override_type_3_6 {
        let _e175 = unnamed.worldLightParams[1u];
        wetness = clamp(_e175, 0f, 1f);
        let _e179 = unnamed.worldLightParams[2u];
        frost = clamp(_e179, 0f, 1f);
        let _e181 = base;
        luminance = dot(_e181.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e184 = wetness;
        let _e186 = base;
        let _e188 = (_e186.xyz * mix(1f, 0.82f, _e184));
        base[0u] = _e188.x;
        base[1u] = _e188.y;
        base[2u] = _e188.z;
        let _e195 = base;
        let _e197 = luminance;
        let _e199 = luminance;
        let _e201 = luminance;
        let _e203 = frost;
        let _e206 = mix(_e195.xyz, vec3<f32>((_e197 * 0.88f), (_e199 * 0.94f), _e201), vec3((_e203 * 0.55f)));
        base[0u] = _e206.x;
        base[1u] = _e206.y;
        base[2u] = _e206.z;
    }
    let _e213 = color0_;
    let _e216 = unnamed.emissionRadiance;
    let _e219 = base;
    let _e221 = (_e219.xyz + (_e213.xyz * _e216.xyz));
    base[0u] = _e221.x;
    base[1u] = _e221.y;
    base[2u] = _e221.z;
    let _e228 = wired_advanced_fog_enabled_u0028_();
    if _e228 {
        let _e229 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e229;
        let _e230 = base;
        let _e233 = unnamed.advancedFogColorDensity;
        let _e235 = fogAmount;
        let _e237 = mix(_e230.xyz, _e233.xyz, vec3(_e235));
        base[0u] = _e237.x;
        base[1u] = _e237.y;
        base[2u] = _e237.z;
    }
    if override_type_3_7 {
        let _e245 = base[3u];
        if (_e245 == 0f) {
            discard;
        }
    } else {
        if override_type_3_8 {
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
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e11 = out_color;
    return _e11;
}
