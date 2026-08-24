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
    _pad_to_packed_indices: array<vec4<f32>, 26>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
@id(14) override ibl_enabled: i32 = 0i;
override override_type_3_8: bool = (ibl_enabled != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_15: bool = (discard_mode == 1i);
override override_type_3_16: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> ibl_N_1: vec3<f32>;
var<private> ibl_V_1: vec3<f32>;
@group(2) @binding(3)
var radianceCube: texture_cube<f32>;
@group(2) @binding(35)
var radianceCube_sampler: sampler;
@group(2) @binding(1)
var brdfLut: texture_2d<f32>;
@group(2) @binding(33)
var brdfLut_sampler: sampler;
@group(2) @binding(4)
var gtaoMap: texture_2d<f32>;
@group(2) @binding(36)
var gtaoMap_sampler: sampler;
var<private> out_color: vec4<f32>;
@group(2) @binding(2)
var irradianceCube: texture_cube<f32>;
@group(2) @binding(34)
var irradianceCube_sampler: sampler;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e84 + 0.5f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e91 = fogType;
    let _e94 = fogType;
    return (((_e89 > 0.5f) && (_e91 >= 1i)) && (_e94 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e84 = wired_advanced_fog_enabled_u0028_();
    if !(_e84) {
        return 0f;
    }
    let _e87 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e87, 0.000001f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e92 + 0.5f));
    let _e95 = fogType_1;
    if (_e95 == 1i) {
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e99 <= 0f) {
            return 0f;
        }
        let _e101 = viewDepth;
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e101 / _e104), 0f, 1f);
    }
    let _e109 = unnamed.advancedFogColorDensity[3u];
    let _e111 = viewDepth;
    opticalDepth = (max(_e109, 0f) * _e111);
    let _e113 = fogType_1;
    if (_e113 == 2i) {
        let _e115 = opticalDepth;
        return clamp((1f - exp(-(_e115))), 0f, 1f);
    }
    let _e120 = opticalDepth;
    let _e121 = opticalDepth;
    return clamp((1f - exp(-((_e120 * _e121)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e87 = (*cosTheta);
    t = (1f - _e87);
    let _e89 = t;
    let _e90 = t;
    t2_ = (_e89 * _e90);
    let _e92 = (*roughness);
    let _e95 = (*F0_);
    Fmax = max(vec3((1f - _e92)), _e95);
    let _e97 = (*F0_);
    let _e98 = Fmax;
    let _e99 = (*F0_);
    let _e101 = t2_;
    let _e102 = t2_;
    let _e104 = t;
    return (_e97 + ((_e98 - _e99) * ((_e101 * _e102) * _e104)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e85 = (*c);
    (*c) = max(_e85, vec3<f32>(0f, 0f, 0f));
    let _e87 = (*c);
    cutoff = (_e87 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e89 = (*c);
    lo = (_e89 / vec3(12.92f));
    let _e92 = (*c);
    hi = pow(((_e92 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e97 = hi;
    let _e98 = lo;
    let _e99 = cutoff;
    return mix(_e97, _e98, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e99));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e108 = (*uv);
    let _e109 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    c_1 = _e109;
    let _e110 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e110))) == 0i) {
        let _e115 = c_1;
        param = _e115.xyz;
        let _e117 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = (*slot);
    if (lightmap_slot == (_e124 + 1i)) {
        let _e129 = unnamed.worldLightParams[0u];
        let _e130 = c_1;
        let _e132 = (_e130.xyz * _e129);
        c_1[0u] = _e132.x;
        c_1[1u] = _e132.y;
        c_1[2u] = _e132.z;
    }
    let _e139 = c_1;
    return _e139;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_2: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_3: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_4: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_5: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_6: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var orm: vec3<f32>;
    var ao: f32;
    var roughness_1: f32;
    var metalness: f32;
    var ibl_n: vec3<f32>;
    var ibl_v: vec3<f32>;
    var F0_1: vec3<f32>;
    var NdotV: f32;
    var F_amb: vec3<f32>;
    var param_27: f32;
    var param_28: vec3<f32>;
    var param_29: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e141 = unnamed.packed_indices[0i][3u];
    let _e147 = unnamed.packed_indices[0i][3u];
    let _e152 = fog_tex_coord_1;
    let _e153 = textureSample(wired_bindless_images[(_e141 & 4095u)], wired_bindless_samplers[((_e147 >> bitcast<u32>(12i)) & 255u)], _e152);
    fog = _e153;
    let _e154 = frag_color0In_1;
    param_1 = _e154.xyz;
    let _e156 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e158 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e156.x, _e156.y, _e156.z, _e158);
    let _e163 = frag_color1In_1;
    param_2 = _e163.xyz;
    let _e165 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e167 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e165.x, _e165.y, _e165.z, _e167);
    param_3 = 0u;
    let _e172 = frag_tex_coord0_1;
    param_4 = _e172;
    param_5 = 0i;
    let _e173 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e174 = frag_color0_;
    color0_ = (_e173 * _e174);
    if override_type_3_2 {
        param_6 = 1u;
        let _e176 = frag_tex_coord1_1;
        param_7 = _e176;
        param_8 = 1i;
        let _e177 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e178 = frag_color1_;
        color1_ = (_e177 * _e178);
        let _e180 = color0_;
        let _e182 = color1_;
        let _e184 = (_e180.xyz + _e182.xyz);
        let _e186 = color0_[3u];
        let _e188 = color1_[3u];
        base = vec4<f32>(_e184.x, _e184.y, _e184.z, (_e186 * _e188));
    } else {
        if override_type_3_3 {
            param_9 = 1u;
            let _e194 = frag_tex_coord1_1;
            param_10 = _e194;
            param_11 = 1i;
            let _e195 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e196 = frag_color1_;
            color1_1 = (_e195 * _e196);
            let _e199 = color0_[3u];
            let _e200 = color0_;
            color0_ = (_e200 * _e199);
            let _e203 = color1_1[3u];
            let _e204 = color1_1;
            color1_1 = (_e204 * _e203);
            let _e206 = color0_;
            let _e208 = color1_1;
            let _e210 = (_e206.xyz + _e208.xyz);
            let _e212 = color0_[3u];
            let _e214 = color1_1[3u];
            base = vec4<f32>(_e210.x, _e210.y, _e210.z, (_e212 * _e214));
        } else {
            if override_type_3_4 {
                param_12 = 1u;
                let _e220 = frag_tex_coord1_1;
                param_13 = _e220;
                param_14 = 1i;
                let _e221 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e222 = frag_color1_;
                color1_2 = (_e221 * _e222);
                let _e225 = color0_[3u];
                let _e227 = color0_;
                color0_ = (_e227 * (1f - _e225));
                let _e230 = color1_2[3u];
                let _e232 = color1_2;
                color1_2 = (_e232 * (1f - _e230));
                let _e234 = color0_;
                let _e236 = color1_2;
                let _e238 = (_e234.xyz + _e236.xyz);
                let _e240 = color0_[3u];
                let _e242 = color1_2[3u];
                base = vec4<f32>(_e238.x, _e238.y, _e238.z, (_e240 * _e242));
            } else {
                if override_type_3_5 {
                    param_15 = 1u;
                    let _e248 = frag_tex_coord1_1;
                    param_16 = _e248;
                    param_17 = 1i;
                    let _e249 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e250 = frag_color1_;
                    color1_3 = (_e249 * _e250);
                    let _e252 = color0_;
                    let _e253 = color1_3;
                    let _e255 = color1_3[3u];
                    base = mix(_e252, _e253, vec4(_e255));
                } else {
                    if override_type_3_6 {
                        param_18 = 1u;
                        let _e258 = frag_tex_coord1_1;
                        param_19 = _e258;
                        param_20 = 1i;
                        let _e259 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e260 = frag_color1_;
                        color1_4 = (_e259 * _e260);
                        let _e262 = color1_4;
                        let _e263 = color0_;
                        let _e265 = color1_4[3u];
                        base = mix(_e262, _e263, vec4(_e265));
                    } else {
                        if override_type_3_7 {
                            param_21 = 1u;
                            let _e268 = frag_tex_coord1_1;
                            param_22 = _e268;
                            param_23 = 1i;
                            let _e269 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e270 = frag_color1_;
                            color1_5 = (_e269 * _e270);
                            let _e272 = color1_5;
                            let _e274 = color1_5[3u];
                            let _e277 = color0_;
                            base = ((_e272 + vec4(_e274)) * _e277);
                        } else {
                            param_24 = 1u;
                            let _e279 = frag_tex_coord1_1;
                            param_25 = _e279;
                            param_26 = 1i;
                            let _e280 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e281 = frag_color1_;
                            color1_6 = (_e280 * _e281);
                            let _e283 = color0_;
                            let _e285 = color1_6;
                            let _e287 = (_e283.xyz * _e285.xyz);
                            base[0u] = _e287.x;
                            base[1u] = _e287.y;
                            base[2u] = _e287.z;
                            let _e295 = color0_[3u];
                            let _e297 = color1_6[3u];
                            base[3u] = (_e295 * _e297);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e303 = unnamed.packed_indices[2i][0u];
        let _e309 = unnamed.packed_indices[2i][0u];
        let _e314 = frag_tex_coord0_1;
        let _e315 = textureSample(wired_bindless_images[(_e303 & 4095u)], wired_bindless_samplers[((_e309 >> bitcast<u32>(12i)) & 255u)], _e314);
        orm = _e315.xyz;
        let _e318 = orm[0u];
        ao = _e318;
        let _e320 = orm[1u];
        roughness_1 = clamp(_e320, 0.04f, 1f);
        let _e323 = orm[2u];
        metalness = _e323;
        let _e324 = ibl_N_1;
        ibl_n = normalize(_e324);
        let _e326 = ibl_V_1;
        ibl_v = normalize(_e326);
        let _e328 = base;
        let _e330 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e328.xyz, vec3(_e330));
        let _e333 = ibl_n;
        let _e334 = ibl_v;
        NdotV = max(dot(_e333, _e334), 0f);
        let _e337 = NdotV;
        param_27 = _e337;
        let _e338 = F0_1;
        param_28 = _e338;
        let _e339 = roughness_1;
        param_29 = _e339;
        let _e340 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_27), (&param_28), (&param_29));
        F_amb = _e340;
        let _e341 = ibl_v;
        let _e343 = ibl_n;
        R = reflect(-(_e341), _e343);
        let _e345 = R;
        let _e346 = roughness_1;
        let _e348 = textureSampleLevel(radianceCube, radianceCube_sampler, _e345, (_e346 * 5f));
        prefiltered = _e348.xyz;
        let _e350 = NdotV;
        let _e351 = roughness_1;
        let _e353 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e350, _e351), 0f);
        envBRDF = _e353.xy;
        let _e355 = prefiltered;
        let _e356 = F_amb;
        let _e358 = envBRDF[0u];
        let _e361 = envBRDF[1u];
        let _e365 = ao;
        specularIBL = ((_e355 * ((_e356 * _e358) + vec3(_e361))) * _e365);
        let _e367 = gl_FragCoord_1;
        let _e369 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e367.xy / vec2<f32>(vec2<i32>(_e369)));
        let _e373 = gtaoUV;
        let _e374 = textureSample(gtaoMap, gtaoMap_sampler, _e373);
        gtao_vis = _e374.x;
        let _e376 = gtao_vis;
        let _e377 = specularIBL;
        specularIBL = (_e377 * _e376);
        let _e379 = specularIBL;
        let _e380 = base;
        let _e382 = (_e380.xyz + _e379);
        base[0u] = _e382.x;
        base[1u] = _e382.y;
        base[2u] = _e382.z;
    }
    let _e389 = wired_advanced_fog_enabled_u0028_();
    if _e389 {
        let _e390 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e390;
        if override_type_3_9 {
            let _e391 = fogAmount;
            let _e393 = base;
            let _e395 = (_e393.xyz * (1f - _e391));
            base[0u] = _e395.x;
            base[1u] = _e395.y;
            base[2u] = _e395.z;
        } else {
            if override_type_3_10 {
                let _e402 = fogAmount;
                let _e404 = base;
                base = (_e404 * (1f - _e402));
            } else {
                if override_type_3_11 {
                    let _e406 = fogAmount;
                    let _e409 = base[3u];
                    base[3u] = (_e409 * (1f - _e406));
                } else {
                    let _e412 = base;
                    let _e415 = unnamed.advancedFogColorDensity;
                    let _e417 = fogAmount;
                    let _e419 = mix(_e412.xyz, _e415.xyz, vec3(_e417));
                    base[0u] = _e419.x;
                    base[1u] = _e419.y;
                    base[2u] = _e419.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e426 = base;
            let _e429 = fog[3u];
            let _e431 = (_e426.xyz * (1f - _e429));
            base[0u] = _e431.x;
            base[1u] = _e431.y;
            base[2u] = _e431.z;
        } else {
            if override_type_3_13 {
                let _e438 = base;
                let _e440 = fog[3u];
                base = (_e438 * (1f - _e440));
            } else {
                if override_type_3_14 {
                    let _e444 = base[3u];
                    let _e446 = fog[3u];
                    base[3u] = (_e444 * (1f - _e446));
                } else {
                    let _e450 = base;
                    let _e451 = fog;
                    let _e453 = unnamed.fogColor;
                    let _e456 = fog[3u];
                    base = mix(_e450, (_e451 * _e453), vec4(_e456));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e460 = base[3u];
        if (_e460 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e462 = base;
            let _e464 = base;
            if (dot(_e462.xyz, _e464.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e468 = base;
    out_color = _e468;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e17 = out_color;
    return _e17;
}
