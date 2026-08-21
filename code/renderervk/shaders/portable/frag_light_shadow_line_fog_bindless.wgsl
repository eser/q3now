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
}

@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_20_: bool = (shadow_pcf <= 1i);
override override_type_20_1: bool = (shadow_pcf <= 5i);
@id(4) override tex_domain: i32 = 0i;
@id(3) override alpha_to_coverage: i32 = 0i;
override override_type_20_2: bool = (alpha_to_coverage != 0i);
@id(0) override alpha_test_func: i32 = 0i;
override override_type_20_3: bool = (alpha_test_func == 1i);
override override_type_20_4: bool = (alpha_test_func == 2i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_20_5: bool = (alpha_test_func == 3i);
override override_type_20_6: bool = (alpha_test_func == 1i);
override override_type_20_7: bool = (alpha_test_func == 2i);
override override_type_20_8: bool = (alpha_test_func == 3i);
@id(5) override abs_light: i32 = 0i;
override override_type_20_9: bool = (abs_light != 0i);

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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> N_1: vec3<f32>;
var<private> L_1: vec4<f32>;
var<private> V_1: vec4<f32>;
var<private> shadowData_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn sampleCascade_u0028_i1_u003b_vf3_u003b(c: ptr<function, i32>, worldPos: ptr<function, vec3<f32>>) -> f32 {
    var sc4_: vec4<f32>;
    var sc: vec3<f32>;
    var layer: f32;
    var texelSize: vec2<f32>;
    var currentDepth: f32;
    var shadow: f32;
    var x: i32;
    var y: i32;
    var phi_100_: bool;
    var phi_107_: bool;
    var phi_114_: bool;
    var phi_122_: bool;

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
    phi_100_ = _e108;
    if !(_e108) {
        let _e111 = sc[0u];
        phi_100_ = (_e111 > 1f);
    }
    let _e114 = phi_100_;
    phi_107_ = _e114;
    if !(_e114) {
        let _e117 = sc[1u];
        phi_107_ = (_e117 < 0f);
    }
    let _e120 = phi_107_;
    phi_114_ = _e120;
    if !(_e120) {
        let _e123 = sc[1u];
        phi_114_ = (_e123 > 1f);
    }
    let _e126 = phi_114_;
    phi_122_ = _e126;
    if !(_e126) {
        let _e129 = sc[2u];
        phi_122_ = (_e129 > 1f);
    }
    let _e132 = phi_122_;
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
    if override_type_20_ {
        let _e144 = currentDepth;
        let _e145 = sc;
        let _e146 = _e145.xy;
        let _e147 = layer;
        let _e150 = vec3<f32>(_e146.x, _e146.y, _e147);
        let _e156 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e150.x, _e150.y), i32(_e150.z));
        shadow = step(_e144, _e156.x);
    } else {
        if override_type_20_1 {
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

fn sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b(worldPos_1: ptr<function, vec3<f32>>, viewDepth: ptr<function, f32>, outCascade: ptr<function, i32>) -> f32 {
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
    let _e89 = (*viewDepth);
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
    let _e125 = (*viewDepth);
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

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e83 = unnamed.packed_indices[0i][0u];
    let _e89 = unnamed.packed_indices[0i][0u];
    let _e94 = textureDimensions(wired_bindless_images[(_e83 & 4095u)], 0i);
    ts = vec2<i32>(_e94);
    let _e97 = (*tc)[0u];
    let _e99 = ts[0u];
    let _e102 = dpdx((_e97 * f32(_e99)));
    dx = max(abs(_e102), 0.001f);
    let _e106 = (*tc)[1u];
    let _e108 = ts[1u];
    let _e111 = dpdy((_e106 * f32(_e108)));
    dy = max(abs(_e111), 0.001f);
    let _e114 = dx;
    let _e115 = dy;
    dxy = max(_e114, _e115);
    let _e117 = dxy;
    scale = max((1f / _e117), 1f);
    let _e120 = (*threshold);
    let _e121 = (*alpha);
    let _e122 = (*threshold);
    let _e124 = scale;
    ac = (_e120 + ((_e121 - _e122) * _e124));
    let _e127 = ac;
    return _e127;
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
    var param_4: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_2 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) != 0i) {
        let _e105 = c_2;
        return _e105;
    }
    let _e106 = c_2;
    param_4 = _e106.xyz;
    let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    c_2[0u] = _e108.x;
    c_2[1u] = _e108.y;
    c_2[2u] = _e108.z;
    let _e115 = c_2;
    return _e115;
}

fn main_1() {
    var fog: vec4<f32>;
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
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;
    var tint: vec3<f32>;
    var local_1: vec3<f32>;
    var local_2: vec3<f32>;

    let _e104 = unnamed.packed_indices[0i][1u];
    let _e110 = unnamed.packed_indices[0i][1u];
    let _e115 = fog_tex_coord_1;
    let _e116 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    fog = _e116;
    let _e117 = frag_tex_coord_1;
    parallax_tc = _e117;
    let _e118 = N_1;
    Np = _e118;
    param_5 = 0u;
    let _e119 = parallax_tc;
    param_6 = _e119;
    param_7 = 0i;
    let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    base = _e120;
    if override_type_20_2 {
        if override_type_20_3 {
            let _e122 = base[3u];
            base[3u] = select(0f, 1f, (_e122 > 0f));
        } else {
            if override_type_20_4 {
                let _e127 = base[3u];
                param_8 = alpha_test_value;
                param_9 = (1f - _e127);
                let _e129 = frag_tex_coord_1;
                param_10 = _e129;
                let _e130 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_8), (&param_9), (&param_10));
                base[3u] = _e130;
            } else {
                if override_type_20_5 {
                    param_11 = alpha_test_value;
                    let _e133 = base[3u];
                    param_12 = _e133;
                    let _e134 = frag_tex_coord_1;
                    param_13 = _e134;
                    let _e135 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_11), (&param_12), (&param_13));
                    base[3u] = _e135;
                }
            }
        }
    } else {
        if override_type_20_6 {
            let _e138 = base[3u];
            if (_e138 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_20_7 {
                let _e141 = base[3u];
                if (_e141 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_20_8 {
                    let _e144 = base[3u];
                    if (_e144 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e147 = unnamed.lightColor;
    lightColorRadius = _e147;
    let _e148 = L_1;
    let _e152 = unnamed.lightVector;
    let _e157 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e148.xyz), _e152.xyz) * _e157), 0f, 1f);
    let _e161 = unnamed.lightVector;
    let _e162 = scale_1;
    let _e164 = L_1;
    LL = ((_e161 * _e162) + _e164);
    let _e166 = LL;
    nL = normalize(_e166.xyz);
    let _e169 = V_1;
    nV = normalize(_e169.xyz);
    let _e172 = LL;
    let _e174 = LL;
    let _e178 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e172.xyz, _e174.xyz) * _e178));
    let _e181 = intensFactor;
    if (_e181 <= 0f) {
        discard;
    }
    let _e183 = lightColorRadius;
    let _e185 = intensFactor;
    intens = (_e183.xyz * _e185);
    dbgCascade = 0i;
    let _e187 = shadowData_1;
    param_14 = _e187.xyz;
    let _e190 = shadowData_1[3u];
    param_15 = _e190;
    let _e191 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    let _e192 = param_16;
    dbgCascade = _e192;
    let _e193 = intens;
    intens = (_e193 * _e191);
    let _e195 = base;
    let _e198 = fog[3u];
    let _e200 = (_e195.xyz * (1f - _e198));
    base[0u] = _e200.x;
    base[1u] = _e200.y;
    base[2u] = _e200.z;
    let _e207 = Np;
    let _e208 = nL;
    diffuse = dot(_e207, _e208);
    let _e210 = Np;
    let _e211 = nL;
    let _e212 = nV;
    specFactor = dot(_e210, normalize((_e211 + _e212)));
    if override_type_20_9 {
        let _e216 = diffuse;
        let _e217 = Np;
        let _e218 = nV;
        if ((_e216 * dot(_e217, _e218)) <= 0f) {
            discard;
        }
        let _e222 = diffuse;
        diffuse = abs(_e222);
        let _e224 = specFactor;
        specFactor = abs(_e224);
    } else {
        let _e226 = diffuse;
        diffuse = max(_e226, 0f);
        let _e228 = specFactor;
        specFactor = max(_e228, 0f);
    }
    let _e230 = specFactor;
    let _e234 = base;
    spec = ((vec4((pow(_e230, 10f) * 0.25f)) * _e234) * 0.8f);
    let _e237 = base;
    let _e238 = diffuse;
    let _e241 = spec;
    let _e243 = intens;
    out_color = (((_e237 * vec4(_e238)) + _e241) * vec4<f32>(_e243.x, _e243.y, _e243.z, 1f));
    let _e251 = unnamed.eyePos[3u];
    if (_e251 > 0.5f) {
        let _e253 = dbgCascade;
        if (_e253 == 0i) {
            local_1 = vec3<f32>(1f, 0.3f, 0.3f);
        } else {
            let _e255 = dbgCascade;
            if (_e255 == 1i) {
                local_2 = vec3<f32>(0.3f, 1f, 0.3f);
            } else {
                let _e257 = dbgCascade;
                local_2 = select(vec3<f32>(1f, 1f, 0.3f), vec3<f32>(0.3f, 0.3f, 1f), vec3((_e257 == 2i)));
            }
            let _e261 = local_2;
            local_1 = _e261;
        }
        let _e262 = local_1;
        tint = _e262;
        let _e263 = tint;
        let _e264 = out_color;
        let _e266 = (_e264.xyz * _e263);
        out_color[0u] = _e266.x;
        out_color[1u] = _e266.y;
        out_color[2u] = _e266.z;
    }
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(2) L: vec4<f32>, @location(3) V: vec4<f32>, @location(6) shadowData: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    L_1 = L;
    V_1 = V;
    shadowData_1 = shadowData;
    main_1();
    let _e13 = out_color;
    return _e13;
}
