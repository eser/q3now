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
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_4: bool = (discard_mode == 1i);
override override_type_3_5: bool = (discard_mode == 2i);
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

    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e74 + 0.5f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e81 = fogType;
    let _e84 = fogType;
    return (((_e79 > 0.5f) && (_e81 >= 1i)) && (_e84 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e74 = wired_advanced_fog_enabled_u0028_();
    if !(_e74) {
        return 0f;
    }
    let _e77 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e77, 0.000001f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e82 + 0.5f));
    let _e85 = fogType_1;
    if (_e85 == 1i) {
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e89 <= 0f) {
            return 0f;
        }
        let _e91 = viewDepth;
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e91 / _e94), 0f, 1f);
    }
    let _e99 = unnamed.advancedFogColorDensity[3u];
    let _e101 = viewDepth;
    opticalDepth = (max(_e99, 0f) * _e101);
    let _e103 = fogType_1;
    if (_e103 == 2i) {
        let _e105 = opticalDepth;
        return clamp((1f - exp(-(_e105))), 0f, 1f);
    }
    let _e110 = opticalDepth;
    let _e111 = opticalDepth;
    return clamp((1f - exp(-((_e110 * _e111)))), 0f, 1f);
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
    var phi_331_: bool;
    var phi_338_: bool;
    var phi_345_: bool;
    var phi_352_: bool;

    let _e81 = (*c);
    let _e84 = unnamed.cascadeMVP[_e81];
    let _e85 = (*worldPos);
    sc4_ = (_e84 * vec4<f32>(_e85.x, _e85.y, _e85.z, 1f));
    let _e91 = sc4_;
    let _e94 = sc4_[3u];
    sc = (_e91.xyz / vec3(_e94));
    let _e97 = sc;
    let _e101 = ((_e97.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e101.x;
    sc[1u] = _e101.y;
    let _e107 = sc[0u];
    let _e108 = (_e107 < 0f);
    phi_331_ = _e108;
    if !(_e108) {
        let _e111 = sc[0u];
        phi_331_ = (_e111 > 1f);
    }
    let _e114 = phi_331_;
    phi_338_ = _e114;
    if !(_e114) {
        let _e117 = sc[1u];
        phi_338_ = (_e117 < 0f);
    }
    let _e120 = phi_338_;
    phi_345_ = _e120;
    if !(_e120) {
        let _e123 = sc[1u];
        phi_345_ = (_e123 > 1f);
    }
    let _e126 = phi_345_;
    phi_352_ = _e126;
    if !(_e126) {
        let _e129 = sc[2u];
        phi_352_ = (_e129 > 1f);
    }
    let _e132 = phi_352_;
    if _e132 {
        return 1f;
    }
    let _e133 = (*c);
    layer = f32(_e133);
    let _e135 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e135).xy));
    let _e142 = sc[2u];
    currentDepth = (_e142 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e144 = currentDepth;
        let _e145 = sc;
        let _e146 = _e145.xy;
        let _e147 = layer;
        let _e150 = vec3<f32>(_e146.x, _e146.y, _e147);
        let _e156 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e150.x, _e150.y), i32(_e150.z));
        shadow = step(_e144, _e156.x);
    } else {
        if override_type_3_1 {
            let _e159 = currentDepth;
            let _e160 = sc;
            let _e161 = _e160.xy;
            let _e162 = layer;
            let _e165 = vec3<f32>(_e161.x, _e161.y, _e162);
            let _e171 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e165.x, _e165.y), i32(_e165.z));
            let _e174 = shadow;
            shadow = (_e174 + step(_e159, _e171.x));
            let _e176 = currentDepth;
            let _e177 = sc;
            let _e180 = texelSize[0u];
            let _e182 = (_e177.xy + vec2<f32>(_e180, 0f));
            let _e183 = layer;
            let _e186 = vec3<f32>(_e182.x, _e182.y, _e183);
            let _e192 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e186.x, _e186.y), i32(_e186.z));
            let _e195 = shadow;
            shadow = (_e195 + step(_e176, _e192.x));
            let _e197 = currentDepth;
            let _e198 = sc;
            let _e201 = texelSize[0u];
            let _e203 = (_e198.xy - vec2<f32>(_e201, 0f));
            let _e204 = layer;
            let _e207 = vec3<f32>(_e203.x, _e203.y, _e204);
            let _e213 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e207.x, _e207.y), i32(_e207.z));
            let _e216 = shadow;
            shadow = (_e216 + step(_e197, _e213.x));
            let _e218 = currentDepth;
            let _e219 = sc;
            let _e222 = texelSize[1u];
            let _e224 = (_e219.xy + vec2<f32>(0f, _e222));
            let _e225 = layer;
            let _e228 = vec3<f32>(_e224.x, _e224.y, _e225);
            let _e234 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e228.x, _e228.y), i32(_e228.z));
            let _e237 = shadow;
            shadow = (_e237 + step(_e218, _e234.x));
            let _e239 = currentDepth;
            let _e240 = sc;
            let _e243 = texelSize[1u];
            let _e245 = (_e240.xy - vec2<f32>(0f, _e243));
            let _e246 = layer;
            let _e249 = vec3<f32>(_e245.x, _e245.y, _e246);
            let _e255 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e249.x, _e249.y), i32(_e249.z));
            let _e258 = shadow;
            shadow = (_e258 + step(_e239, _e255.x));
            let _e260 = shadow;
            shadow = (_e260 / 5f);
        } else {
            x = -1i;
            loop {
                let _e262 = x;
                if (_e262 <= 1i) {
                    y = -1i;
                    loop {
                        let _e264 = y;
                        if (_e264 <= 1i) {
                            let _e266 = currentDepth;
                            let _e267 = sc;
                            let _e269 = x;
                            let _e271 = y;
                            let _e274 = texelSize;
                            let _e276 = (_e267.xy + (vec2<f32>(f32(_e269), f32(_e271)) * _e274));
                            let _e277 = layer;
                            let _e280 = vec3<f32>(_e276.x, _e276.y, _e277);
                            let _e286 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e280.x, _e280.y), i32(_e280.z));
                            let _e289 = shadow;
                            shadow = (_e289 + step(_e266, _e286.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e291 = y;
                            y = (_e291 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e293 = x;
                    x = (_e293 + 1i);
                }
            }
            let _e295 = shadow;
            shadow = (_e295 / 9f);
        }
    }
    let _e297 = shadow;
    return _e297;
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

    let _e88 = unnamed.cascadeSplits;
    let _e89 = (*viewDepth_1);
    cmp = step(_e88, vec4(_e89));
    let _e93 = cmp[0u];
    let _e95 = cmp[1u];
    let _e98 = cmp[2u];
    let _e101 = cmp[3u];
    cascade = min(i32((((_e93 + _e95) + _e98) + _e101)), 3i);
    let _e105 = cascade;
    (*outCascade) = _e105;
    let _e106 = cascade;
    if (_e106 == 0i) {
        local = 0f;
    } else {
        let _e108 = cascade;
        let _e113 = unnamed.cascadeSplits[max((_e108 - 1i), 0i)];
        local = _e113;
    }
    let _e114 = local;
    prevSplit = _e114;
    let _e115 = cascade;
    let _e118 = unnamed.cascadeSplits[_e115];
    farSplit = _e118;
    let _e119 = farSplit;
    let _e120 = prevSplit;
    blendRange = max((0.1f * (_e119 - _e120)), 1f);
    let _e124 = farSplit;
    let _e125 = (*viewDepth_1);
    let _e127 = blendRange;
    blendT = clamp(((_e124 - _e125) / _e127), 0f, 1f);
    let _e130 = cascade;
    param = _e130;
    let _e131 = (*worldPos_1);
    param_1 = _e131;
    let _e132 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e132;
    let _e133 = cascade;
    param_2 = min((_e133 + 1i), 3i);
    let _e136 = (*worldPos_1);
    param_3 = _e136;
    let _e137 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e137;
    let _e138 = s1_;
    let _e139 = s0_;
    let _e140 = blendT;
    return mix(_e138, _e139, _e140);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e82 = unnamed.packed_indices[1i][3u];
    let _e88 = unnamed.packed_indices[1i][3u];
    let _e93 = (*lm_uv);
    let _e94 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    sun_mask = _e94.x;
    let _e96 = sun_mask;
    if (_e96 > 0.001f) {
        let _e98 = shadowData_1;
        param_4 = _e98.xyz;
        let _e101 = shadowData_1[3u];
        param_5 = _e101;
        let _e102 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e103 = param_6;
        ignoredCascade = _e103;
        shadow_1 = _e102;
        let _e104 = shadow_1;
        let _e105 = sun_mask;
        let _e107 = (*rgb);
        (*rgb) = (_e107 * mix(1f, _e104, _e105));
    }
    let _e109 = (*rgb);
    return _e109;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e74 = (*rgb_1);
        param_7 = _e74;
        let _e75 = frag_tex_coord0_1;
        param_8 = _e75;
        let _e76 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e76;
    }
    let _e77 = (*rgb_1);
    return _e77;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e74 = (*rgb_2);
    let _e77 = unnamed.worldLightParams[0u];
    boosted = (_e74 * _e77);
    let _e80 = boosted[0u];
    let _e82 = boosted[1u];
    let _e84 = boosted[2u];
    peak = max(_e80, max(_e82, _e84));
    let _e87 = peak;
    if (_e87 > 1f) {
        let _e89 = peak;
        let _e90 = boosted;
        boosted = (_e90 / vec3(_e89));
    }
    let _e93 = boosted;
    return _e93;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e75 = (*c_1);
    (*c_1) = max(_e75, vec3<f32>(0f, 0f, 0f));
    let _e77 = (*c_1);
    cutoff = (_e77 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e79 = (*c_1);
    lo = (_e79 / vec3(12.92f));
    let _e82 = (*c_1);
    hi = pow(((_e82 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e87 = hi;
    let _e88 = lo;
    let _e89 = cutoff;
    return mix(_e87, _e88, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e89));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

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
        param_9 = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e108.x;
        c_2[1u] = _e108.y;
        c_2[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e118 = c_2;
        param_10 = _e118.xyz;
        let _e120 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e120.x;
        c_2[1u] = _e120.y;
        c_2[2u] = _e120.z;
    }
    let _e127 = c_2;
    return _e127;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_11: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_15: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e84 = frag_color0In_1;
    param_11 = _e84.xyz;
    let _e86 = sRGBToLinear_u0028_vf3_u003b((&param_11));
    let _e88 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e86.x, _e86.y, _e86.z, _e88);
    param_12 = 0u;
    let _e93 = frag_tex_coord0_1;
    param_13 = _e93;
    param_14 = 0i;
    let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    sampledColor0_ = _e94;
    let _e95 = sampledColor0_;
    let _e96 = frag_color0_;
    color0_ = (_e95 * _e96);
    let _e98 = color0_;
    base = _e98;
    let _e99 = color0_;
    base = _e99;
    let _e100 = base;
    param_15 = _e100.xyz;
    let _e102 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_15));
    base[0u] = _e102.x;
    base[1u] = _e102.y;
    base[2u] = _e102.z;
    if override_type_3_3 {
        let _e111 = unnamed.worldLightParams[1u];
        wetness = clamp(_e111, 0f, 1f);
        let _e115 = unnamed.worldLightParams[2u];
        frost = clamp(_e115, 0f, 1f);
        let _e117 = base;
        luminance = dot(_e117.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e120 = wetness;
        let _e122 = base;
        let _e124 = (_e122.xyz * mix(1f, 0.82f, _e120));
        base[0u] = _e124.x;
        base[1u] = _e124.y;
        base[2u] = _e124.z;
        let _e131 = base;
        let _e133 = luminance;
        let _e135 = luminance;
        let _e137 = luminance;
        let _e139 = frost;
        let _e142 = mix(_e131.xyz, vec3<f32>((_e133 * 0.88f), (_e135 * 0.94f), _e137), vec3((_e139 * 0.55f)));
        base[0u] = _e142.x;
        base[1u] = _e142.y;
        base[2u] = _e142.z;
    }
    let _e151 = unnamed.materialFactors[0u];
    if (_e151 >= 0f) {
        let _e154 = unnamed.materialBaseColor;
        let _e155 = base;
        base = (_e155 * _e154);
    }
    let _e157 = color0_;
    let _e160 = unnamed.emissionRadiance;
    let _e163 = base;
    let _e165 = (_e163.xyz + (_e157.xyz * _e160.xyz));
    base[0u] = _e165.x;
    base[1u] = _e165.y;
    base[2u] = _e165.z;
    let _e172 = wired_advanced_fog_enabled_u0028_();
    if _e172 {
        let _e173 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e173;
        let _e174 = base;
        let _e177 = unnamed.advancedFogColorDensity;
        let _e179 = fogAmount;
        let _e181 = mix(_e174.xyz, _e177.xyz, vec3(_e179));
        base[0u] = _e181.x;
        base[1u] = _e181.y;
        base[2u] = _e181.z;
    }
    if override_type_3_4 {
        let _e189 = base[3u];
        if (_e189 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e191 = base;
            let _e193 = base;
            if (dot(_e191.xyz, _e193.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e197 = base;
    out_color = _e197;
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
