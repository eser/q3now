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
var<private> frag_color0In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e71 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e71 + 0.5f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e78 = fogType;
    let _e81 = fogType;
    return (((_e76 > 0.5f) && (_e78 >= 1i)) && (_e81 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e71 = wired_advanced_fog_enabled_u0028_();
    if !(_e71) {
        return 0f;
    }
    let _e74 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e74, 0.000001f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e79 + 0.5f));
    let _e82 = fogType_1;
    if (_e82 == 1i) {
        let _e86 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e86 <= 0f) {
            return 0f;
        }
        let _e88 = viewDepth;
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e88 / _e91), 0f, 1f);
    }
    let _e96 = unnamed.advancedFogColorDensity[3u];
    let _e98 = viewDepth;
    opticalDepth = (max(_e96, 0f) * _e98);
    let _e100 = fogType_1;
    if (_e100 == 2i) {
        let _e102 = opticalDepth;
        return clamp((1f - exp(-(_e102))), 0f, 1f);
    }
    let _e107 = opticalDepth;
    let _e108 = opticalDepth;
    return clamp((1f - exp(-((_e107 * _e108)))), 0f, 1f);
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

    let _e78 = (*c);
    let _e81 = unnamed.cascadeMVP[_e78];
    let _e82 = (*worldPos);
    sc4_ = (_e81 * vec4<f32>(_e82.x, _e82.y, _e82.z, 1f));
    let _e88 = sc4_;
    let _e91 = sc4_[3u];
    sc = (_e88.xyz / vec3(_e91));
    let _e94 = sc;
    let _e98 = ((_e94.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e98.x;
    sc[1u] = _e98.y;
    let _e104 = sc[0u];
    let _e105 = (_e104 < 0f);
    phi_304_ = _e105;
    if !(_e105) {
        let _e108 = sc[0u];
        phi_304_ = (_e108 > 1f);
    }
    let _e111 = phi_304_;
    phi_311_ = _e111;
    if !(_e111) {
        let _e114 = sc[1u];
        phi_311_ = (_e114 < 0f);
    }
    let _e117 = phi_311_;
    phi_318_ = _e117;
    if !(_e117) {
        let _e120 = sc[1u];
        phi_318_ = (_e120 > 1f);
    }
    let _e123 = phi_318_;
    phi_325_ = _e123;
    if !(_e123) {
        let _e126 = sc[2u];
        phi_325_ = (_e126 > 1f);
    }
    let _e129 = phi_325_;
    if _e129 {
        return 1f;
    }
    let _e130 = (*c);
    layer = f32(_e130);
    let _e132 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e132).xy));
    let _e139 = sc[2u];
    currentDepth = (_e139 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e141 = currentDepth;
        let _e142 = sc;
        let _e143 = _e142.xy;
        let _e144 = layer;
        let _e147 = vec3<f32>(_e143.x, _e143.y, _e144);
        let _e153 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e147.x, _e147.y), i32(_e147.z));
        shadow = step(_e141, _e153.x);
    } else {
        if override_type_3_1 {
            let _e156 = currentDepth;
            let _e157 = sc;
            let _e158 = _e157.xy;
            let _e159 = layer;
            let _e162 = vec3<f32>(_e158.x, _e158.y, _e159);
            let _e168 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e162.x, _e162.y), i32(_e162.z));
            let _e171 = shadow;
            shadow = (_e171 + step(_e156, _e168.x));
            let _e173 = currentDepth;
            let _e174 = sc;
            let _e177 = texelSize[0u];
            let _e179 = (_e174.xy + vec2<f32>(_e177, 0f));
            let _e180 = layer;
            let _e183 = vec3<f32>(_e179.x, _e179.y, _e180);
            let _e189 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e183.x, _e183.y), i32(_e183.z));
            let _e192 = shadow;
            shadow = (_e192 + step(_e173, _e189.x));
            let _e194 = currentDepth;
            let _e195 = sc;
            let _e198 = texelSize[0u];
            let _e200 = (_e195.xy - vec2<f32>(_e198, 0f));
            let _e201 = layer;
            let _e204 = vec3<f32>(_e200.x, _e200.y, _e201);
            let _e210 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e204.x, _e204.y), i32(_e204.z));
            let _e213 = shadow;
            shadow = (_e213 + step(_e194, _e210.x));
            let _e215 = currentDepth;
            let _e216 = sc;
            let _e219 = texelSize[1u];
            let _e221 = (_e216.xy + vec2<f32>(0f, _e219));
            let _e222 = layer;
            let _e225 = vec3<f32>(_e221.x, _e221.y, _e222);
            let _e231 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e225.x, _e225.y), i32(_e225.z));
            let _e234 = shadow;
            shadow = (_e234 + step(_e215, _e231.x));
            let _e236 = currentDepth;
            let _e237 = sc;
            let _e240 = texelSize[1u];
            let _e242 = (_e237.xy - vec2<f32>(0f, _e240));
            let _e243 = layer;
            let _e246 = vec3<f32>(_e242.x, _e242.y, _e243);
            let _e252 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e246.x, _e246.y), i32(_e246.z));
            let _e255 = shadow;
            shadow = (_e255 + step(_e236, _e252.x));
            let _e257 = shadow;
            shadow = (_e257 / 5f);
        } else {
            x = -1i;
            loop {
                let _e259 = x;
                if (_e259 <= 1i) {
                    y = -1i;
                    loop {
                        let _e261 = y;
                        if (_e261 <= 1i) {
                            let _e263 = currentDepth;
                            let _e264 = sc;
                            let _e266 = x;
                            let _e268 = y;
                            let _e271 = texelSize;
                            let _e273 = (_e264.xy + (vec2<f32>(f32(_e266), f32(_e268)) * _e271));
                            let _e274 = layer;
                            let _e277 = vec3<f32>(_e273.x, _e273.y, _e274);
                            let _e283 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e277.x, _e277.y), i32(_e277.z));
                            let _e286 = shadow;
                            shadow = (_e286 + step(_e263, _e283.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e288 = y;
                            y = (_e288 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e290 = x;
                    x = (_e290 + 1i);
                }
            }
            let _e292 = shadow;
            shadow = (_e292 / 9f);
        }
    }
    let _e294 = shadow;
    return _e294;
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

    let _e85 = unnamed.cascadeSplits;
    let _e86 = (*viewDepth_1);
    cmp = step(_e85, vec4(_e86));
    let _e90 = cmp[0u];
    let _e92 = cmp[1u];
    let _e95 = cmp[2u];
    let _e98 = cmp[3u];
    cascade = min(i32((((_e90 + _e92) + _e95) + _e98)), 3i);
    let _e102 = cascade;
    (*outCascade) = _e102;
    let _e103 = cascade;
    if (_e103 == 0i) {
        local = 0f;
    } else {
        let _e105 = cascade;
        let _e110 = unnamed.cascadeSplits[max((_e105 - 1i), 0i)];
        local = _e110;
    }
    let _e111 = local;
    prevSplit = _e111;
    let _e112 = cascade;
    let _e115 = unnamed.cascadeSplits[_e112];
    farSplit = _e115;
    let _e116 = farSplit;
    let _e117 = prevSplit;
    blendRange = max((0.1f * (_e116 - _e117)), 1f);
    let _e121 = farSplit;
    let _e122 = (*viewDepth_1);
    let _e124 = blendRange;
    blendT = clamp(((_e121 - _e122) / _e124), 0f, 1f);
    let _e127 = cascade;
    param = _e127;
    let _e128 = (*worldPos_1);
    param_1 = _e128;
    let _e129 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e129;
    let _e130 = cascade;
    param_2 = min((_e130 + 1i), 3i);
    let _e133 = (*worldPos_1);
    param_3 = _e133;
    let _e134 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e134;
    let _e135 = s1_;
    let _e136 = s0_;
    let _e137 = blendT;
    return mix(_e135, _e136, _e137);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e79 = unnamed.packed_indices[1i][3u];
    let _e85 = unnamed.packed_indices[1i][3u];
    let _e90 = (*lm_uv);
    let _e91 = textureSample(wired_bindless_images[(_e79 & 4095u)], wired_bindless_samplers[((_e85 >> bitcast<u32>(12i)) & 255u)], _e90);
    sun_mask = _e91.x;
    let _e93 = sun_mask;
    if (_e93 > 0.001f) {
        let _e95 = shadowData_1;
        param_4 = _e95.xyz;
        let _e98 = shadowData_1[3u];
        param_5 = _e98;
        let _e99 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e100 = param_6;
        ignoredCascade = _e100;
        shadow_1 = _e99;
        let _e101 = shadow_1;
        let _e102 = sun_mask;
        let _e104 = (*rgb);
        (*rgb) = (_e104 * mix(1f, _e101, _e102));
    }
    let _e106 = (*rgb);
    return _e106;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e71 = (*rgb_1);
        param_7 = _e71;
        let _e72 = frag_tex_coord0_1;
        param_8 = _e72;
        let _e73 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e73;
    }
    let _e74 = (*rgb_1);
    return _e74;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e72 = (*c_1);
    (*c_1) = max(_e72, vec3<f32>(0f, 0f, 0f));
    let _e74 = (*c_1);
    cutoff = (_e74 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e76 = (*c_1);
    lo = (_e76 / vec3(12.92f));
    let _e79 = (*c_1);
    hi = pow(((_e79 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e84 = hi;
    let _e85 = lo;
    let _e86 = cutoff;
    return mix(_e84, _e85, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e86));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e95 = (*uv);
    let _e96 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    c_2 = _e96;
    let _e97 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e97))) == 0i) {
        let _e102 = c_2;
        param_9 = _e102.xyz;
        let _e104 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e104.x;
        c_2[1u] = _e104.y;
        c_2[2u] = _e104.z;
    }
    let _e111 = (*slot);
    if (lightmap_slot == (_e111 + 1i)) {
        let _e116 = unnamed.worldLightParams[0u];
        let _e117 = c_2;
        let _e119 = (_e117.xyz * _e116);
        c_2[0u] = _e119.x;
        c_2[1u] = _e119.y;
        c_2[2u] = _e119.z;
    }
    let _e126 = c_2;
    return _e126;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_10: vec3<f32>;
    var color0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var param_14: vec3<f32>;
    var fogAmount: f32;

    let _e81 = unnamed.packed_indices[0i][3u];
    let _e87 = unnamed.packed_indices[0i][3u];
    let _e92 = fog_tex_coord_1;
    let _e93 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    fog = _e93;
    let _e94 = frag_color0In_1;
    param_10 = _e94.xyz;
    let _e96 = sRGBToLinear_u0028_vf3_u003b((&param_10));
    let _e98 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e96.x, _e96.y, _e96.z, _e98);
    param_11 = 0u;
    let _e103 = frag_tex_coord0_1;
    param_12 = _e103;
    param_13 = 0i;
    let _e104 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    let _e105 = frag_color0_;
    color0_ = (_e104 * _e105);
    let _e107 = color0_;
    base = _e107;
    let _e108 = color0_;
    base = _e108;
    let _e109 = base;
    param_14 = _e109.xyz;
    let _e111 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e111.x;
    base[1u] = _e111.y;
    base[2u] = _e111.z;
    let _e118 = wired_advanced_fog_enabled_u0028_();
    if _e118 {
        let _e119 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e119;
        if override_type_3_3 {
            let _e120 = fogAmount;
            let _e122 = base;
            let _e124 = (_e122.xyz * (1f - _e120));
            base[0u] = _e124.x;
            base[1u] = _e124.y;
            base[2u] = _e124.z;
        } else {
            if override_type_3_4 {
                let _e131 = fogAmount;
                let _e133 = base;
                base = (_e133 * (1f - _e131));
            } else {
                if override_type_3_5 {
                    let _e135 = fogAmount;
                    let _e138 = base[3u];
                    base[3u] = (_e138 * (1f - _e135));
                } else {
                    let _e141 = base;
                    let _e144 = unnamed.advancedFogColorDensity;
                    let _e146 = fogAmount;
                    let _e148 = mix(_e141.xyz, _e144.xyz, vec3(_e146));
                    base[0u] = _e148.x;
                    base[1u] = _e148.y;
                    base[2u] = _e148.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e155 = base;
            let _e158 = fog[3u];
            let _e160 = (_e155.xyz * (1f - _e158));
            base[0u] = _e160.x;
            base[1u] = _e160.y;
            base[2u] = _e160.z;
        } else {
            if override_type_3_7 {
                let _e167 = base;
                let _e169 = fog[3u];
                base = (_e167 * (1f - _e169));
            } else {
                if override_type_3_8 {
                    let _e173 = base[3u];
                    let _e175 = fog[3u];
                    base[3u] = (_e173 * (1f - _e175));
                } else {
                    let _e179 = base;
                    let _e180 = fog;
                    let _e182 = unnamed.fogColor;
                    let _e185 = fog[3u];
                    base = mix(_e179, (_e180 * _e182), vec4(_e185));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e189 = base[3u];
        if (_e189 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
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
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e11 = out_color;
    return _e11;
}
