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
@id(10) override acff: i32 = 0i;
override override_type_3_3: bool = (acff == 1i);
override override_type_3_4: bool = (acff == 2i);
override override_type_3_5: bool = (acff == 3i);
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e77 = (*c);
    let _e80 = unnamed.cascadeMVP[_e77];
    let _e81 = (*worldPos);
    sc4_ = (_e80 * vec4<f32>(_e81.x, _e81.y, _e81.z, 1f));
    let _e87 = sc4_;
    let _e90 = sc4_[3u];
    sc = (_e87.xyz / vec3(_e90));
    let _e93 = sc;
    let _e97 = ((_e93.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e97.x;
    sc[1u] = _e97.y;
    let _e103 = sc[0u];
    let _e104 = (_e103 < 0f);
    phi_304_ = _e104;
    if !(_e104) {
        let _e107 = sc[0u];
        phi_304_ = (_e107 > 1f);
    }
    let _e110 = phi_304_;
    phi_311_ = _e110;
    if !(_e110) {
        let _e113 = sc[1u];
        phi_311_ = (_e113 < 0f);
    }
    let _e116 = phi_311_;
    phi_318_ = _e116;
    if !(_e116) {
        let _e119 = sc[1u];
        phi_318_ = (_e119 > 1f);
    }
    let _e122 = phi_318_;
    phi_325_ = _e122;
    if !(_e122) {
        let _e125 = sc[2u];
        phi_325_ = (_e125 > 1f);
    }
    let _e128 = phi_325_;
    if _e128 {
        return 1f;
    }
    let _e129 = (*c);
    layer = f32(_e129);
    let _e131 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e131).xy));
    let _e138 = sc[2u];
    currentDepth = (_e138 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e140 = currentDepth;
        let _e141 = sc;
        let _e142 = _e141.xy;
        let _e143 = layer;
        let _e146 = vec3<f32>(_e142.x, _e142.y, _e143);
        let _e152 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e146.x, _e146.y), i32(_e146.z));
        shadow = step(_e140, _e152.x);
    } else {
        if override_type_3_1 {
            let _e155 = currentDepth;
            let _e156 = sc;
            let _e157 = _e156.xy;
            let _e158 = layer;
            let _e161 = vec3<f32>(_e157.x, _e157.y, _e158);
            let _e167 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e161.x, _e161.y), i32(_e161.z));
            let _e170 = shadow;
            shadow = (_e170 + step(_e155, _e167.x));
            let _e172 = currentDepth;
            let _e173 = sc;
            let _e176 = texelSize[0u];
            let _e178 = (_e173.xy + vec2<f32>(_e176, 0f));
            let _e179 = layer;
            let _e182 = vec3<f32>(_e178.x, _e178.y, _e179);
            let _e188 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e182.x, _e182.y), i32(_e182.z));
            let _e191 = shadow;
            shadow = (_e191 + step(_e172, _e188.x));
            let _e193 = currentDepth;
            let _e194 = sc;
            let _e197 = texelSize[0u];
            let _e199 = (_e194.xy - vec2<f32>(_e197, 0f));
            let _e200 = layer;
            let _e203 = vec3<f32>(_e199.x, _e199.y, _e200);
            let _e209 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e203.x, _e203.y), i32(_e203.z));
            let _e212 = shadow;
            shadow = (_e212 + step(_e193, _e209.x));
            let _e214 = currentDepth;
            let _e215 = sc;
            let _e218 = texelSize[1u];
            let _e220 = (_e215.xy + vec2<f32>(0f, _e218));
            let _e221 = layer;
            let _e224 = vec3<f32>(_e220.x, _e220.y, _e221);
            let _e230 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e224.x, _e224.y), i32(_e224.z));
            let _e233 = shadow;
            shadow = (_e233 + step(_e214, _e230.x));
            let _e235 = currentDepth;
            let _e236 = sc;
            let _e239 = texelSize[1u];
            let _e241 = (_e236.xy - vec2<f32>(0f, _e239));
            let _e242 = layer;
            let _e245 = vec3<f32>(_e241.x, _e241.y, _e242);
            let _e251 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e245.x, _e245.y), i32(_e245.z));
            let _e254 = shadow;
            shadow = (_e254 + step(_e235, _e251.x));
            let _e256 = shadow;
            shadow = (_e256 / 5f);
        } else {
            x = -1i;
            loop {
                let _e258 = x;
                if (_e258 <= 1i) {
                    y = -1i;
                    loop {
                        let _e260 = y;
                        if (_e260 <= 1i) {
                            let _e262 = currentDepth;
                            let _e263 = sc;
                            let _e265 = x;
                            let _e267 = y;
                            let _e270 = texelSize;
                            let _e272 = (_e263.xy + (vec2<f32>(f32(_e265), f32(_e267)) * _e270));
                            let _e273 = layer;
                            let _e276 = vec3<f32>(_e272.x, _e272.y, _e273);
                            let _e282 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e276.x, _e276.y), i32(_e276.z));
                            let _e285 = shadow;
                            shadow = (_e285 + step(_e262, _e282.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e287 = y;
                            y = (_e287 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e289 = x;
                    x = (_e289 + 1i);
                }
            }
            let _e291 = shadow;
            shadow = (_e291 / 9f);
        }
    }
    let _e293 = shadow;
    return _e293;
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

    let _e84 = unnamed.cascadeSplits;
    let _e85 = (*viewDepth_1);
    cmp = step(_e84, vec4(_e85));
    let _e89 = cmp[0u];
    let _e91 = cmp[1u];
    let _e94 = cmp[2u];
    let _e97 = cmp[3u];
    cascade = min(i32((((_e89 + _e91) + _e94) + _e97)), 3i);
    let _e101 = cascade;
    (*outCascade) = _e101;
    let _e102 = cascade;
    if (_e102 == 0i) {
        local = 0f;
    } else {
        let _e104 = cascade;
        let _e109 = unnamed.cascadeSplits[max((_e104 - 1i), 0i)];
        local = _e109;
    }
    let _e110 = local;
    prevSplit = _e110;
    let _e111 = cascade;
    let _e114 = unnamed.cascadeSplits[_e111];
    farSplit = _e114;
    let _e115 = farSplit;
    let _e116 = prevSplit;
    blendRange = max((0.1f * (_e115 - _e116)), 1f);
    let _e120 = farSplit;
    let _e121 = (*viewDepth_1);
    let _e123 = blendRange;
    blendT = clamp(((_e120 - _e121) / _e123), 0f, 1f);
    let _e126 = cascade;
    param = _e126;
    let _e127 = (*worldPos_1);
    param_1 = _e127;
    let _e128 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e128;
    let _e129 = cascade;
    param_2 = min((_e129 + 1i), 3i);
    let _e132 = (*worldPos_1);
    param_3 = _e132;
    let _e133 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e133;
    let _e134 = s1_;
    let _e135 = s0_;
    let _e136 = blendT;
    return mix(_e134, _e135, _e136);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e78 = unnamed.packed_indices[1i][3u];
    let _e84 = unnamed.packed_indices[1i][3u];
    let _e89 = (*lm_uv);
    let _e90 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    sun_mask = _e90.x;
    let _e92 = sun_mask;
    if (_e92 > 0.001f) {
        let _e94 = shadowData_1;
        param_4 = _e94.xyz;
        let _e97 = shadowData_1[3u];
        param_5 = _e97;
        let _e98 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e99 = param_6;
        ignoredCascade = _e99;
        shadow_1 = _e98;
        let _e100 = shadow_1;
        let _e101 = sun_mask;
        let _e103 = (*rgb);
        (*rgb) = (_e103 * mix(1f, _e100, _e101));
    }
    let _e105 = (*rgb);
    return _e105;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e70 = (*rgb_1);
        param_7 = _e70;
        let _e71 = frag_tex_coord0_1;
        param_8 = _e71;
        let _e72 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e72;
    }
    let _e73 = (*rgb_1);
    return _e73;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e71 = (*c_1);
    (*c_1) = max(_e71, vec3<f32>(0f, 0f, 0f));
    let _e73 = (*c_1);
    cutoff = (_e73 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e75 = (*c_1);
    lo = (_e75 / vec3(12.92f));
    let _e78 = (*c_1);
    hi = pow(((_e78 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e83 = hi;
    let _e84 = lo;
    let _e85 = cutoff;
    return mix(_e83, _e84, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e85));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e72 = (*role);
    let _e74 = (*role);
    let _e79 = unnamed.packed_indices[(_e72 / 4u)][(_e74 % 4u)];
    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e94 = (*uv);
    let _e95 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    c_2 = _e95;
    let _e96 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e96))) == 0i) {
        let _e101 = c_2;
        param_9 = _e101.xyz;
        let _e103 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e103.x;
        c_2[1u] = _e103.y;
        c_2[2u] = _e103.z;
    }
    let _e110 = (*slot);
    if (lightmap_slot == (_e110 + 1i)) {
        let _e115 = unnamed.worldLightParams[0u];
        let _e116 = c_2;
        let _e118 = (_e116.xyz * _e115);
        c_2[0u] = _e118.x;
        c_2[1u] = _e118.y;
        c_2[2u] = _e118.z;
    }
    let _e125 = c_2;
    return _e125;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var param_13: vec3<f32>;
    var fogAmount: f32;

    let _e78 = unnamed.packed_indices[0i][3u];
    let _e84 = unnamed.packed_indices[0i][3u];
    let _e89 = fog_tex_coord_1;
    let _e90 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    fog = _e90;
    param_10 = 0u;
    let _e91 = frag_tex_coord0_1;
    param_11 = _e91;
    param_12 = 0i;
    let _e92 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    color0_ = _e92;
    let _e93 = color0_;
    base = _e93;
    let _e94 = color0_;
    base = _e94;
    let _e95 = base;
    param_13 = _e95.xyz;
    let _e97 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_13));
    base[0u] = _e97.x;
    base[1u] = _e97.y;
    base[2u] = _e97.z;
    let _e104 = wired_advanced_fog_enabled_u0028_();
    if _e104 {
        let _e105 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e105;
        if override_type_3_3 {
            let _e106 = fogAmount;
            let _e108 = base;
            let _e110 = (_e108.xyz * (1f - _e106));
            base[0u] = _e110.x;
            base[1u] = _e110.y;
            base[2u] = _e110.z;
        } else {
            if override_type_3_4 {
                let _e117 = fogAmount;
                let _e119 = base;
                base = (_e119 * (1f - _e117));
            } else {
                if override_type_3_5 {
                    let _e121 = fogAmount;
                    let _e124 = base[3u];
                    base[3u] = (_e124 * (1f - _e121));
                } else {
                    let _e127 = base;
                    let _e130 = unnamed.advancedFogColorDensity;
                    let _e132 = fogAmount;
                    let _e134 = mix(_e127.xyz, _e130.xyz, vec3(_e132));
                    base[0u] = _e134.x;
                    base[1u] = _e134.y;
                    base[2u] = _e134.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e141 = base;
            let _e144 = fog[3u];
            let _e146 = (_e141.xyz * (1f - _e144));
            base[0u] = _e146.x;
            base[1u] = _e146.y;
            base[2u] = _e146.z;
        } else {
            if override_type_3_7 {
                let _e153 = base;
                let _e155 = fog[3u];
                base = (_e153 * (1f - _e155));
            } else {
                if override_type_3_8 {
                    let _e159 = base[3u];
                    let _e161 = fog[3u];
                    base[3u] = (_e159 * (1f - _e161));
                } else {
                    let _e165 = base;
                    let _e166 = fog;
                    let _e168 = unnamed.fogColor;
                    let _e171 = fog[3u];
                    base = mix(_e165, (_e166 * _e168), vec4(_e171));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e175 = base[3u];
        if (_e175 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e177 = base;
            let _e179 = base;
            if (dot(_e177.xyz, _e179.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e183 = base;
    out_color = _e183;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e9 = out_color;
    return _e9;
}
