enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    _pad_to_cascadeMVP: array<vec4<f32>, 5>,
    cascadeMVP: array<mat4x4<f32>, 4>,
    cascadeSplits: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 8>,
    packed_indices: array<vec4<u32>, 3>,
    _pad_worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
}

@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_8_: bool = (shadow_pcf <= 1i);
override override_type_8_1: bool = (shadow_pcf <= 5i);
@id(4) override tex_domain: i32 = 0i;
@id(3) override alpha_to_coverage: i32 = 0i;
override override_type_8_2: bool = (alpha_to_coverage != 0i);
@id(0) override alpha_test_func: i32 = 0i;
override override_type_8_3: bool = (alpha_test_func == 1i);
override override_type_8_4: bool = (alpha_test_func == 2i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_8_5: bool = (alpha_test_func == 3i);
override override_type_8_6: bool = (alpha_test_func == 1i);
override override_type_8_7: bool = (alpha_test_func == 2i);
override override_type_8_8: bool = (alpha_test_func == 3i);
@id(5) override abs_light: i32 = 0i;
override override_type_8_9: bool = (abs_light != 0i);

@group(0) @binding(0)
var<uniform> unnamed: UBO;
@group(2) @binding(0)
var shadowMap: texture_2d_array<f32>;
@group(2) @binding(32)
var shadowMap_sampler: sampler;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> N_1: vec3<f32>;
var<private> L_1: vec4<f32>;
var<private> V_1: vec4<f32>;
var<private> shadowData_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e78 + 0.5f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e85 = fogType;
    let _e88 = fogType;
    return (((_e83 > 0.5f) && (_e85 >= 1i)) && (_e88 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e78 = wired_advanced_fog_enabled_u0028_();
    if !(_e78) {
        return 0f;
    }
    let _e81 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e81, 0.000001f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e86 + 0.5f));
    let _e89 = fogType_1;
    if (_e89 == 1i) {
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e93 <= 0f) {
            return 0f;
        }
        let _e95 = viewDepth;
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e95 / _e98), 0f, 1f);
    }
    let _e103 = unnamed.advancedFogColorDensity[3u];
    let _e105 = viewDepth;
    opticalDepth = (max(_e103, 0f) * _e105);
    let _e107 = fogType_1;
    if (_e107 == 2i) {
        let _e109 = opticalDepth;
        return clamp((1f - exp(-(_e109))), 0f, 1f);
    }
    let _e114 = opticalDepth;
    let _e115 = opticalDepth;
    return clamp((1f - exp(-((_e114 * _e115)))), 0f, 1f);
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
    var phi_107_: bool;
    var phi_114_: bool;
    var phi_121_: bool;
    var phi_129_: bool;

    let _e85 = (*c);
    let _e88 = unnamed.cascadeMVP[_e85];
    let _e89 = (*worldPos);
    sc4_ = (_e88 * vec4<f32>(_e89.x, _e89.y, _e89.z, 1f));
    let _e95 = sc4_;
    let _e98 = sc4_[3u];
    sc = (_e95.xyz / vec3(_e98));
    let _e101 = sc;
    let _e105 = ((_e101.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e105.x;
    sc[1u] = _e105.y;
    let _e111 = sc[0u];
    let _e112 = (_e111 < 0f);
    phi_107_ = _e112;
    if !(_e112) {
        let _e115 = sc[0u];
        phi_107_ = (_e115 > 1f);
    }
    let _e118 = phi_107_;
    phi_114_ = _e118;
    if !(_e118) {
        let _e121 = sc[1u];
        phi_114_ = (_e121 < 0f);
    }
    let _e124 = phi_114_;
    phi_121_ = _e124;
    if !(_e124) {
        let _e127 = sc[1u];
        phi_121_ = (_e127 > 1f);
    }
    let _e130 = phi_121_;
    phi_129_ = _e130;
    if !(_e130) {
        let _e133 = sc[2u];
        phi_129_ = (_e133 > 1f);
    }
    let _e136 = phi_129_;
    if _e136 {
        return 1f;
    }
    let _e137 = (*c);
    layer = f32(_e137);
    let _e139 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e139).xy));
    let _e146 = sc[2u];
    currentDepth = (_e146 - shadow_bias);
    shadow = 0f;
    if override_type_8_ {
        let _e148 = currentDepth;
        let _e149 = sc;
        let _e150 = _e149.xy;
        let _e151 = layer;
        let _e154 = vec3<f32>(_e150.x, _e150.y, _e151);
        let _e160 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e154.x, _e154.y), i32(_e154.z));
        shadow = step(_e148, _e160.x);
    } else {
        if override_type_8_1 {
            let _e163 = currentDepth;
            let _e164 = sc;
            let _e165 = _e164.xy;
            let _e166 = layer;
            let _e169 = vec3<f32>(_e165.x, _e165.y, _e166);
            let _e175 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e169.x, _e169.y), i32(_e169.z));
            let _e178 = shadow;
            shadow = (_e178 + step(_e163, _e175.x));
            let _e180 = currentDepth;
            let _e181 = sc;
            let _e184 = texelSize[0u];
            let _e186 = (_e181.xy + vec2<f32>(_e184, 0f));
            let _e187 = layer;
            let _e190 = vec3<f32>(_e186.x, _e186.y, _e187);
            let _e196 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e190.x, _e190.y), i32(_e190.z));
            let _e199 = shadow;
            shadow = (_e199 + step(_e180, _e196.x));
            let _e201 = currentDepth;
            let _e202 = sc;
            let _e205 = texelSize[0u];
            let _e207 = (_e202.xy - vec2<f32>(_e205, 0f));
            let _e208 = layer;
            let _e211 = vec3<f32>(_e207.x, _e207.y, _e208);
            let _e217 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e211.x, _e211.y), i32(_e211.z));
            let _e220 = shadow;
            shadow = (_e220 + step(_e201, _e217.x));
            let _e222 = currentDepth;
            let _e223 = sc;
            let _e226 = texelSize[1u];
            let _e228 = (_e223.xy + vec2<f32>(0f, _e226));
            let _e229 = layer;
            let _e232 = vec3<f32>(_e228.x, _e228.y, _e229);
            let _e238 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e232.x, _e232.y), i32(_e232.z));
            let _e241 = shadow;
            shadow = (_e241 + step(_e222, _e238.x));
            let _e243 = currentDepth;
            let _e244 = sc;
            let _e247 = texelSize[1u];
            let _e249 = (_e244.xy - vec2<f32>(0f, _e247));
            let _e250 = layer;
            let _e253 = vec3<f32>(_e249.x, _e249.y, _e250);
            let _e259 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e253.x, _e253.y), i32(_e253.z));
            let _e262 = shadow;
            shadow = (_e262 + step(_e243, _e259.x));
            let _e264 = shadow;
            shadow = (_e264 / 5f);
        } else {
            x = -1i;
            loop {
                let _e266 = x;
                if (_e266 <= 1i) {
                    y = -1i;
                    loop {
                        let _e268 = y;
                        if (_e268 <= 1i) {
                            let _e270 = currentDepth;
                            let _e271 = sc;
                            let _e273 = x;
                            let _e275 = y;
                            let _e278 = texelSize;
                            let _e280 = (_e271.xy + (vec2<f32>(f32(_e273), f32(_e275)) * _e278));
                            let _e281 = layer;
                            let _e284 = vec3<f32>(_e280.x, _e280.y, _e281);
                            let _e290 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e284.x, _e284.y), i32(_e284.z));
                            let _e293 = shadow;
                            shadow = (_e293 + step(_e270, _e290.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e295 = y;
                            y = (_e295 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e297 = x;
                    x = (_e297 + 1i);
                }
            }
            let _e299 = shadow;
            shadow = (_e299 / 9f);
        }
    }
    let _e301 = shadow;
    return _e301;
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

    let _e92 = unnamed.cascadeSplits;
    let _e93 = (*viewDepth_1);
    cmp = step(_e92, vec4(_e93));
    let _e97 = cmp[0u];
    let _e99 = cmp[1u];
    let _e102 = cmp[2u];
    let _e105 = cmp[3u];
    cascade = min(i32((((_e97 + _e99) + _e102) + _e105)), 3i);
    let _e109 = cascade;
    (*outCascade) = _e109;
    let _e110 = cascade;
    if (_e110 == 0i) {
        local = 0f;
    } else {
        let _e112 = cascade;
        let _e117 = unnamed.cascadeSplits[max((_e112 - 1i), 0i)];
        local = _e117;
    }
    let _e118 = local;
    prevSplit = _e118;
    let _e119 = cascade;
    let _e122 = unnamed.cascadeSplits[_e119];
    farSplit = _e122;
    let _e123 = farSplit;
    let _e124 = prevSplit;
    blendRange = max((0.1f * (_e123 - _e124)), 1f);
    let _e128 = farSplit;
    let _e129 = (*viewDepth_1);
    let _e131 = blendRange;
    blendT = clamp(((_e128 - _e129) / _e131), 0f, 1f);
    let _e134 = cascade;
    param = _e134;
    let _e135 = (*worldPos_1);
    param_1 = _e135;
    let _e136 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e136;
    let _e137 = cascade;
    param_2 = min((_e137 + 1i), 3i);
    let _e140 = (*worldPos_1);
    param_3 = _e140;
    let _e141 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e141;
    let _e142 = s1_;
    let _e143 = s0_;
    let _e144 = blendT;
    return mix(_e142, _e143, _e144);
}

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e87 = unnamed.packed_indices[0i][0u];
    let _e93 = unnamed.packed_indices[0i][0u];
    let _e98 = textureDimensions(wired_bindless_images[(_e87 & 4095u)], 0i);
    ts = vec2<i32>(_e98);
    let _e101 = (*tc)[0u];
    let _e103 = ts[0u];
    let _e106 = dpdx((_e101 * f32(_e103)));
    dx = max(abs(_e106), 0.001f);
    let _e110 = (*tc)[1u];
    let _e112 = ts[1u];
    let _e115 = dpdy((_e110 * f32(_e112)));
    dy = max(abs(_e115), 0.001f);
    let _e118 = dx;
    let _e119 = dy;
    dxy = max(_e118, _e119);
    let _e121 = dxy;
    scale = max((1f / _e121), 1f);
    let _e124 = (*threshold);
    let _e125 = (*alpha);
    let _e126 = (*threshold);
    let _e128 = scale;
    ac = (_e124 + ((_e125 - _e126) * _e128));
    let _e131 = ac;
    return _e131;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e79 = (*c_1);
    (*c_1) = max(_e79, vec3<f32>(0f, 0f, 0f));
    let _e81 = (*c_1);
    cutoff = (_e81 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e83 = (*c_1);
    lo = (_e83 / vec3(12.92f));
    let _e86 = (*c_1);
    hi = pow(((_e86 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e91 = hi;
    let _e92 = lo;
    let _e93 = cutoff;
    return mix(_e91, _e92, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e93));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_4: vec3<f32>;

    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e102 = (*uv);
    let _e103 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    c_2 = _e103;
    let _e104 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e104))) != 0i) {
        let _e109 = c_2;
        return _e109;
    }
    let _e110 = c_2;
    param_4 = _e110.xyz;
    let _e112 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    c_2[0u] = _e112.x;
    c_2[1u] = _e112.y;
    c_2[2u] = _e112.z;
    let _e119 = c_2;
    return _e119;
}

fn main_1() {
    var parallax_tc: vec2<f32>;
    var Np: vec3<f32>;
    var base: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var param_8: f32;
    var param_9: f32;
    var param_10: vec2<f32>;
    var param_11: f32;
    var param_12: f32;
    var param_13: vec2<f32>;
    var lightColorRadius: vec4<f32>;
    var scale_1: f32;
    var LL: vec4<f32>;
    var nL: vec3<f32>;
    var nV: vec3<f32>;
    var intensFactor: f32;
    var intens: vec3<f32>;
    var dbgCascade: i32;
    var param_14: vec3<f32>;
    var param_15: f32;
    var param_16: i32;
    var fogAmount: f32;
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;
    var tint: vec3<f32>;
    var local_1: vec3<f32>;
    var local_2: vec3<f32>;

    let _e105 = frag_tex_coord_1;
    parallax_tc = _e105;
    let _e106 = N_1;
    Np = _e106;
    param_5 = 0u;
    let _e107 = parallax_tc;
    param_6 = _e107;
    param_7 = 0i;
    let _e108 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    base = _e108;
    if override_type_8_2 {
        if override_type_8_3 {
            let _e110 = base[3u];
            base[3u] = select(0f, 1f, (_e110 > 0f));
        } else {
            if override_type_8_4 {
                let _e115 = base[3u];
                param_8 = alpha_test_value;
                param_9 = (1f - _e115);
                let _e117 = frag_tex_coord_1;
                param_10 = _e117;
                let _e118 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_8), (&param_9), (&param_10));
                base[3u] = _e118;
            } else {
                if override_type_8_5 {
                    param_11 = alpha_test_value;
                    let _e121 = base[3u];
                    param_12 = _e121;
                    let _e122 = frag_tex_coord_1;
                    param_13 = _e122;
                    let _e123 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_11), (&param_12), (&param_13));
                    base[3u] = _e123;
                }
            }
        }
    } else {
        if override_type_8_6 {
            let _e126 = base[3u];
            if (_e126 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_8_7 {
                let _e129 = base[3u];
                if (_e129 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_8_8 {
                    let _e132 = base[3u];
                    if (_e132 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e135 = unnamed.lightColor;
    lightColorRadius = _e135;
    let _e136 = L_1;
    let _e140 = unnamed.lightVector;
    let _e145 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e136.xyz), _e140.xyz) * _e145), 0f, 1f);
    let _e149 = unnamed.lightVector;
    let _e150 = scale_1;
    let _e152 = L_1;
    LL = ((_e149 * _e150) + _e152);
    let _e154 = LL;
    nL = normalize(_e154.xyz);
    let _e157 = V_1;
    nV = normalize(_e157.xyz);
    let _e160 = LL;
    let _e162 = LL;
    let _e166 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e160.xyz, _e162.xyz) * _e166));
    let _e169 = intensFactor;
    if (_e169 <= 0f) {
        discard;
    }
    let _e171 = lightColorRadius;
    let _e173 = intensFactor;
    intens = (_e171.xyz * _e173);
    dbgCascade = 0i;
    let _e175 = shadowData_1;
    param_14 = _e175.xyz;
    let _e178 = shadowData_1[3u];
    param_15 = _e178;
    let _e179 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    let _e180 = param_16;
    dbgCascade = _e180;
    let _e181 = intens;
    intens = (_e181 * _e179);
    let _e183 = wired_advanced_fog_amount_u0028_();
    fogAmount = _e183;
    let _e184 = fogAmount;
    let _e186 = base;
    let _e188 = (_e186.xyz * (1f - _e184));
    base[0u] = _e188.x;
    base[1u] = _e188.y;
    base[2u] = _e188.z;
    let _e195 = Np;
    let _e196 = nL;
    diffuse = dot(_e195, _e196);
    let _e198 = Np;
    let _e199 = nL;
    let _e200 = nV;
    specFactor = dot(_e198, normalize((_e199 + _e200)));
    if override_type_8_9 {
        let _e204 = diffuse;
        let _e205 = Np;
        let _e206 = nV;
        if ((_e204 * dot(_e205, _e206)) <= 0f) {
            discard;
        }
        let _e210 = diffuse;
        diffuse = abs(_e210);
        let _e212 = specFactor;
        specFactor = abs(_e212);
    } else {
        let _e214 = diffuse;
        diffuse = max(_e214, 0f);
        let _e216 = specFactor;
        specFactor = max(_e216, 0f);
    }
    let _e218 = specFactor;
    let _e222 = base;
    spec = ((vec4((pow(_e218, 10f) * 0.25f)) * _e222) * 0.8f);
    let _e225 = base;
    let _e226 = diffuse;
    let _e229 = spec;
    let _e231 = intens;
    out_color = (((_e225 * vec4(_e226)) + _e229) * vec4<f32>(_e231.x, _e231.y, _e231.z, 1f));
    let _e239 = unnamed.eyePos[3u];
    if (_e239 > 0.5f) {
        let _e241 = dbgCascade;
        if (_e241 == 0i) {
            local_1 = vec3<f32>(1f, 0.3f, 0.3f);
        } else {
            let _e243 = dbgCascade;
            if (_e243 == 1i) {
                local_2 = vec3<f32>(0.3f, 1f, 0.3f);
            } else {
                let _e245 = dbgCascade;
                local_2 = select(vec3<f32>(1f, 1f, 0.3f), vec3<f32>(0.3f, 0.3f, 1f), vec3((_e245 == 2i)));
            }
            let _e249 = local_2;
            local_1 = _e249;
        }
        let _e250 = local_1;
        tint = _e250;
        let _e251 = tint;
        let _e252 = out_color;
        let _e254 = (_e252.xyz * _e251);
        out_color[0u] = _e254.x;
        out_color[1u] = _e254.y;
        out_color[2u] = _e254.z;
    }
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(2) L: vec4<f32>, @location(3) V: vec4<f32>, @location(6) shadowData: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    L_1 = L;
    V_1 = V;
    shadowData_1 = shadowData;
    main_1();
    let _e13 = out_color;
    return _e13;
}
