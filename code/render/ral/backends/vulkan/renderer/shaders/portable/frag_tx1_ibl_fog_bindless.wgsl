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
    emissionRadiance: vec4<f32>,
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
override override_type_3_: bool = (lightmap_slot == 1i);
@id(6) override tex_mode: i32 = 0i;
override override_type_3_1: bool = (tex_mode == 1i);
override override_type_3_2: bool = (tex_mode == 2i);
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(14) override ibl_enabled: i32 = 0i;
override override_type_3_4: bool = (ibl_enabled != 0i);
override override_type_3_5: bool = (ibl_enabled == 1i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> ibl_N_1: vec3<f32>;
var<private> ibl_V_1: vec3<f32>;
@group(2) @binding(3)
var radianceCube: texture_cube<f32>;
@group(2) @binding(35)
var radianceCube_sampler: sampler;
@group(2) @binding(6)
var previousRadianceCube: texture_cube<f32>;
@group(2) @binding(38)
var previousRadianceCube_sampler: sampler;
@group(2) @binding(1)
var brdfLut: texture_2d<f32>;
@group(2) @binding(33)
var brdfLut_sampler: sampler;
@group(2) @binding(4)
var gtaoMap: texture_2d<f32>;
@group(2) @binding(36)
var gtaoMap_sampler: sampler;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e94 + 0.5f));
    let _e99 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e101 = fogType;
    let _e104 = fogType;
    return (((_e99 > 0.5f) && (_e101 >= 1i)) && (_e104 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e94 = wired_advanced_fog_enabled_u0028_();
    if !(_e94) {
        return 0f;
    }
    let _e97 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e97, 0.000001f));
    let _e102 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e102 + 0.5f));
    let _e105 = fogType_1;
    if (_e105 == 1i) {
        let _e109 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e109 <= 0f) {
            return 0f;
        }
        let _e111 = viewDepth;
        let _e114 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e111 / _e114), 0f, 1f);
    }
    let _e119 = unnamed.advancedFogColorDensity[3u];
    let _e121 = viewDepth;
    opticalDepth = (max(_e119, 0f) * _e121);
    let _e123 = fogType_1;
    if (_e123 == 2i) {
        let _e125 = opticalDepth;
        return clamp((1f - exp(-(_e125))), 0f, 1f);
    }
    let _e130 = opticalDepth;
    let _e131 = opticalDepth;
    return clamp((1f - exp(-((_e130 * _e131)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e97 = (*cosTheta);
    t = (1f - _e97);
    let _e99 = t;
    let _e100 = t;
    t2_ = (_e99 * _e100);
    let _e102 = (*roughness);
    let _e105 = (*F0_);
    Fmax = max(vec3((1f - _e102)), _e105);
    let _e107 = (*F0_);
    let _e108 = Fmax;
    let _e109 = (*F0_);
    let _e111 = t2_;
    let _e112 = t2_;
    let _e114 = t;
    return (_e107 + ((_e108 - _e109) * ((_e111 * _e112) * _e114)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e94 = (*rgb);
    let _e97 = unnamed.worldLightParams[0u];
    boosted = (_e94 * _e97);
    let _e100 = boosted[0u];
    let _e102 = boosted[1u];
    let _e104 = boosted[2u];
    peak = max(_e100, max(_e102, _e104));
    let _e107 = peak;
    if (_e107 > 1f) {
        let _e109 = peak;
        let _e110 = boosted;
        boosted = (_e110 / vec3(_e109));
    }
    let _e113 = boosted;
    return _e113;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e95 = (*c);
    (*c) = max(_e95, vec3<f32>(0f, 0f, 0f));
    let _e97 = (*c);
    cutoff = (_e97 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e99 = (*c);
    lo = (_e99 / vec3(12.92f));
    let _e102 = (*c);
    hi = pow(((_e102 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e107 = hi;
    let _e108 = lo;
    let _e109 = cutoff;
    return mix(_e107, _e108, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e109));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e107 = (*role);
    let _e109 = (*role);
    let _e114 = unnamed.packed_indices[(_e107 / 4u)][(_e109 % 4u)];
    let _e119 = (*uv);
    let _e120 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    c_1 = _e120;
    let _e121 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e121))) == 0i) {
        let _e126 = c_1;
        param = _e126.xyz;
        let _e128 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = (*slot);
    if (lightmap_slot == (_e135 + 1i)) {
        let _e138 = c_1;
        param_1 = _e138.xyz;
        let _e140 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e140.x;
        c_1[1u] = _e140.y;
        c_1[2u] = _e140.z;
    }
    let _e147 = c_1;
    return _e147;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var sampledColor1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var materialAlbedo: vec3<f32>;
    var local: vec3<f32>;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var upward: f32;
    var snowCoverage: f32;
    var orm: vec3<f32>;
    var local_1: vec3<f32>;
    var ao: f32;
    var roughness_1: f32;
    var metalness: f32;
    var ibl_n: vec3<f32>;
    var ibl_v: vec3<f32>;
    var F0_1: vec3<f32>;
    var NdotV: f32;
    var F_amb: vec3<f32>;
    var param_9: f32;
    var param_10: vec3<f32>;
    var param_11: f32;
    var R: vec3<f32>;
    var environmentBlend: f32;
    var currentPrefiltered: vec3<f32>;
    var previousPrefiltered: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e140 = unnamed.packed_indices[0i][3u];
    let _e146 = unnamed.packed_indices[0i][3u];
    let _e151 = fog_tex_coord_1;
    let _e152 = textureSample(wired_bindless_images[(_e140 & 4095u)], wired_bindless_samplers[((_e146 >> bitcast<u32>(12i)) & 255u)], _e151);
    fog = _e152;
    let _e153 = frag_color0In_1;
    param_2 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e157 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    param_3 = 0u;
    let _e162 = frag_tex_coord0_1;
    param_4 = _e162;
    param_5 = 0i;
    let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e163;
    param_6 = 1u;
    let _e164 = frag_tex_coord1_1;
    param_7 = _e164;
    param_8 = 1i;
    let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e165;
    if override_type_3_ {
        let _e166 = sampledColor1_;
        local = _e166.xyz;
    } else {
        let _e168 = sampledColor0_;
        local = _e168.xyz;
    }
    let _e170 = local;
    materialAlbedo = _e170;
    let _e171 = sampledColor0_;
    let _e172 = frag_color0_;
    color0_ = (_e171 * _e172);
    if override_type_3_1 {
        let _e174 = sampledColor1_;
        color1_ = _e174;
        let _e175 = color0_;
        let _e177 = color1_;
        let _e179 = (_e175.xyz + _e177.xyz);
        let _e181 = color0_[3u];
        let _e183 = color1_[3u];
        base = vec4<f32>(_e179.x, _e179.y, _e179.z, (_e181 * _e183));
    } else {
        if override_type_3_2 {
            let _e189 = sampledColor1_;
            let _e190 = frag_color0_;
            color1_1 = (_e189 * _e190);
            let _e192 = color0_;
            let _e194 = color1_1;
            let _e196 = (_e192.xyz + _e194.xyz);
            let _e198 = color0_[3u];
            let _e200 = color1_1[3u];
            base = vec4<f32>(_e196.x, _e196.y, _e196.z, (_e198 * _e200));
        } else {
            let _e206 = sampledColor1_;
            color1_2 = _e206;
            let _e207 = color0_;
            let _e209 = color1_2;
            let _e211 = (_e207.xyz * _e209.xyz);
            base[0u] = _e211.x;
            base[1u] = _e211.y;
            base[2u] = _e211.z;
            let _e219 = color0_[3u];
            let _e221 = color1_2[3u];
            base[3u] = (_e219 * _e221);
        }
    }
    if override_type_3_3 {
        let _e226 = unnamed.worldLightParams[1u];
        wetness = clamp(_e226, 0f, 1f);
        let _e230 = unnamed.worldLightParams[2u];
        frost = clamp(_e230, 0f, 1f);
        let _e232 = base;
        luminance = dot(_e232.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e235 = wetness;
        let _e237 = base;
        let _e239 = (_e237.xyz * mix(1f, 0.82f, _e235));
        base[0u] = _e239.x;
        base[1u] = _e239.y;
        base[2u] = _e239.z;
        let _e246 = base;
        let _e248 = luminance;
        let _e250 = luminance;
        let _e252 = luminance;
        let _e254 = frost;
        let _e257 = mix(_e246.xyz, vec3<f32>((_e248 * 0.88f), (_e250 * 0.94f), _e252), vec3((_e254 * 0.55f)));
        base[0u] = _e257.x;
        base[1u] = _e257.y;
        base[2u] = _e257.z;
        let _e264 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e264).z);
        let _e270 = unnamed.worldLightParams[3u];
        let _e272 = upward;
        snowCoverage = (clamp(_e270, 0f, 1f) * _e272);
        let _e274 = base;
        let _e276 = snowCoverage;
        let _e279 = mix(_e274.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e276 * 0.82f)));
        base[0u] = _e279.x;
        base[1u] = _e279.y;
        base[2u] = _e279.z;
    }
    let _e288 = unnamed.materialFactors[0u];
    if (_e288 >= 0f) {
        let _e291 = unnamed.materialBaseColor;
        let _e292 = base;
        base = (_e292 * _e291);
    }
    let _e294 = color0_;
    let _e297 = unnamed.emissionRadiance;
    let _e300 = base;
    let _e302 = (_e300.xyz + (_e294.xyz * _e297.xyz));
    base[0u] = _e302.x;
    base[1u] = _e302.y;
    base[2u] = _e302.z;
    if override_type_3_4 {
        if override_type_3_5 {
            let _e312 = unnamed.packed_indices[2i][0u];
            let _e318 = unnamed.packed_indices[2i][0u];
            let _e323 = frag_tex_coord0_1;
            let _e324 = textureSample(wired_bindless_images[(_e312 & 4095u)], wired_bindless_samplers[((_e318 >> bitcast<u32>(12i)) & 255u)], _e323);
            local_1 = _e324.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e326 = local_1;
        orm = _e326;
        let _e328 = orm[0u];
        let _e331 = unnamed.materialFactors[2u];
        ao = mix(1f, _e328, clamp(_e331, 0f, 1f));
        let _e335 = orm[1u];
        let _e338 = unnamed.materialFactors[1u];
        let _e342 = unnamed.worldLightParams[1u];
        let _e347 = unnamed.worldLightParams[2u];
        let _e352 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e335 * _e338) - (0.35f * _e342)) + (0.3f * _e347)) + (0.4f * _e352)), 0.04f, 1f);
        let _e357 = orm[2u];
        let _e360 = unnamed.materialFactors[0u];
        metalness = clamp((_e357 * _e360), 0f, 1f);
        let _e363 = ibl_N_1;
        ibl_n = normalize(_e363);
        let _e365 = ibl_V_1;
        ibl_v = normalize(_e365);
        let _e367 = materialAlbedo;
        let _e369 = unnamed.materialBaseColor;
        let _e372 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e367 * _e369.xyz), vec3(_e372));
        let _e375 = ibl_n;
        let _e376 = ibl_v;
        NdotV = max(dot(_e375, _e376), 0f);
        let _e379 = NdotV;
        param_9 = _e379;
        let _e380 = F0_1;
        param_10 = _e380;
        let _e381 = roughness_1;
        param_11 = _e381;
        let _e382 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_9), (&param_10), (&param_11));
        F_amb = _e382;
        let _e383 = ibl_v;
        let _e385 = ibl_n;
        R = reflect(-(_e383), _e385);
        let _e389 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e389, 0f, 1f);
        let _e391 = R;
        let _e392 = roughness_1;
        let _e393 = textureNumLevels(radianceCube);
        let _e398 = textureSampleLevel(radianceCube, radianceCube_sampler, _e391, (_e392 * f32((i32(_e393) - 1i))));
        currentPrefiltered = _e398.xyz;
        let _e400 = R;
        let _e401 = roughness_1;
        let _e402 = textureNumLevels(previousRadianceCube);
        let _e407 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e400, (_e401 * f32((i32(_e402) - 1i))));
        previousPrefiltered = _e407.xyz;
        let _e409 = previousPrefiltered;
        let _e410 = currentPrefiltered;
        let _e411 = environmentBlend;
        prefiltered = mix(_e409, _e410, vec3(_e411));
        let _e414 = NdotV;
        let _e415 = roughness_1;
        let _e417 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e414, _e415), 0f);
        envBRDF = _e417.xy;
        let _e419 = prefiltered;
        let _e420 = F_amb;
        let _e422 = envBRDF[0u];
        let _e425 = envBRDF[1u];
        let _e429 = ao;
        specularIBL = ((_e419 * ((_e420 * _e422) + vec3(_e425))) * _e429);
        let _e431 = gl_FragCoord_1;
        let _e433 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e431.xy / vec2<f32>(vec2<i32>(_e433)));
        let _e437 = gtaoUV;
        let _e438 = textureSample(gtaoMap, gtaoMap_sampler, _e437);
        gtao_vis = _e438.x;
        let _e440 = gtao_vis;
        let _e441 = specularIBL;
        specularIBL = (_e441 * _e440);
        let _e443 = specularIBL;
        let _e444 = base;
        let _e446 = (_e444.xyz + _e443);
        base[0u] = _e446.x;
        base[1u] = _e446.y;
        base[2u] = _e446.z;
    }
    let _e453 = wired_advanced_fog_enabled_u0028_();
    if _e453 {
        let _e454 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e454;
        if override_type_3_6 {
            let _e455 = fogAmount;
            let _e457 = base;
            let _e459 = (_e457.xyz * (1f - _e455));
            base[0u] = _e459.x;
            base[1u] = _e459.y;
            base[2u] = _e459.z;
        } else {
            if override_type_3_7 {
                let _e466 = fogAmount;
                let _e468 = base;
                base = (_e468 * (1f - _e466));
            } else {
                if override_type_3_8 {
                    let _e470 = fogAmount;
                    let _e473 = base[3u];
                    base[3u] = (_e473 * (1f - _e470));
                } else {
                    let _e476 = base;
                    let _e479 = unnamed.advancedFogColorDensity;
                    let _e481 = fogAmount;
                    let _e483 = mix(_e476.xyz, _e479.xyz, vec3(_e481));
                    base[0u] = _e483.x;
                    base[1u] = _e483.y;
                    base[2u] = _e483.z;
                }
            }
        }
    } else {
        if override_type_3_9 {
            let _e490 = base;
            let _e493 = fog[3u];
            let _e495 = (_e490.xyz * (1f - _e493));
            base[0u] = _e495.x;
            base[1u] = _e495.y;
            base[2u] = _e495.z;
        } else {
            if override_type_3_10 {
                let _e502 = base;
                let _e504 = fog[3u];
                base = (_e502 * (1f - _e504));
            } else {
                if override_type_3_11 {
                    let _e508 = base[3u];
                    let _e510 = fog[3u];
                    base[3u] = (_e508 * (1f - _e510));
                } else {
                    let _e514 = base;
                    let _e515 = fog;
                    let _e517 = unnamed.fogColor;
                    let _e520 = fog[3u];
                    base = mix(_e514, (_e515 * _e517), vec4(_e520));
                }
            }
        }
    }
    if override_type_3_12 {
        let _e524 = base[3u];
        if (_e524 == 0f) {
            discard;
        }
    } else {
        if override_type_3_13 {
            let _e526 = base;
            let _e528 = base;
            if (dot(_e526.xyz, _e528.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e532 = base;
    out_color = _e532;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e15 = out_color;
    return _e15;
}
