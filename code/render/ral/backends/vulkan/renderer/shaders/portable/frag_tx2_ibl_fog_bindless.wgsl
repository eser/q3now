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
var<private> frag_tex_coord2_1: vec2<f32>;
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

    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e95 + 0.5f));
    let _e100 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e102 = fogType;
    let _e105 = fogType;
    return (((_e100 > 0.5f) && (_e102 >= 1i)) && (_e105 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e95 = wired_advanced_fog_enabled_u0028_();
    if !(_e95) {
        return 0f;
    }
    let _e98 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e98, 0.000001f));
    let _e103 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e103 + 0.5f));
    let _e106 = fogType_1;
    if (_e106 == 1i) {
        let _e110 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e110 <= 0f) {
            return 0f;
        }
        let _e112 = viewDepth;
        let _e115 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e112 / _e115), 0f, 1f);
    }
    let _e120 = unnamed.advancedFogColorDensity[3u];
    let _e122 = viewDepth;
    opticalDepth = (max(_e120, 0f) * _e122);
    let _e124 = fogType_1;
    if (_e124 == 2i) {
        let _e126 = opticalDepth;
        return clamp((1f - exp(-(_e126))), 0f, 1f);
    }
    let _e131 = opticalDepth;
    let _e132 = opticalDepth;
    return clamp((1f - exp(-((_e131 * _e132)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e98 = (*cosTheta);
    t = (1f - _e98);
    let _e100 = t;
    let _e101 = t;
    t2_ = (_e100 * _e101);
    let _e103 = (*roughness);
    let _e106 = (*F0_);
    Fmax = max(vec3((1f - _e103)), _e106);
    let _e108 = (*F0_);
    let _e109 = Fmax;
    let _e110 = (*F0_);
    let _e112 = t2_;
    let _e113 = t2_;
    let _e115 = t;
    return (_e108 + ((_e109 - _e110) * ((_e112 * _e113) * _e115)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e95 = (*rgb);
    let _e98 = unnamed.worldLightParams[0u];
    boosted = (_e95 * _e98);
    let _e101 = boosted[0u];
    let _e103 = boosted[1u];
    let _e105 = boosted[2u];
    peak = max(_e101, max(_e103, _e105));
    let _e108 = peak;
    if (_e108 > 1f) {
        let _e110 = peak;
        let _e111 = boosted;
        boosted = (_e111 / vec3(_e110));
    }
    let _e114 = boosted;
    return _e114;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e96 = (*c);
    (*c) = max(_e96, vec3<f32>(0f, 0f, 0f));
    let _e98 = (*c);
    cutoff = (_e98 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e100 = (*c);
    lo = (_e100 / vec3(12.92f));
    let _e103 = (*c);
    hi = pow(((_e103 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e108 = hi;
    let _e109 = lo;
    let _e110 = cutoff;
    return mix(_e108, _e109, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e110));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e98 = (*role);
    let _e100 = (*role);
    let _e105 = unnamed.packed_indices[(_e98 / 4u)][(_e100 % 4u)];
    let _e108 = (*role);
    let _e110 = (*role);
    let _e115 = unnamed.packed_indices[(_e108 / 4u)][(_e110 % 4u)];
    let _e120 = (*uv);
    let _e121 = textureSample(wired_bindless_images[(_e105 & 4095u)], wired_bindless_samplers[((_e115 >> bitcast<u32>(12i)) & 255u)], _e120);
    c_1 = _e121;
    let _e122 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e122))) == 0i) {
        let _e127 = c_1;
        param = _e127.xyz;
        let _e129 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e129.x;
        c_1[1u] = _e129.y;
        c_1[2u] = _e129.z;
    }
    let _e136 = (*slot);
    if (lightmap_slot == (_e136 + 1i)) {
        let _e139 = c_1;
        param_1 = _e139.xyz;
        let _e141 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e141.x;
        c_1[1u] = _e141.y;
        c_1[2u] = _e141.z;
    }
    let _e148 = c_1;
    return _e148;
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
    var sampledColor2_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var materialAlbedo: vec3<f32>;
    var local: vec3<f32>;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
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
    var param_12: f32;
    var param_13: vec3<f32>;
    var param_14: f32;
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

    let _e148 = unnamed.packed_indices[0i][3u];
    let _e154 = unnamed.packed_indices[0i][3u];
    let _e159 = fog_tex_coord_1;
    let _e160 = textureSample(wired_bindless_images[(_e148 & 4095u)], wired_bindless_samplers[((_e154 >> bitcast<u32>(12i)) & 255u)], _e159);
    fog = _e160;
    let _e161 = frag_color0In_1;
    param_2 = _e161.xyz;
    let _e163 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e165 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e163.x, _e163.y, _e163.z, _e165);
    param_3 = 0u;
    let _e170 = frag_tex_coord0_1;
    param_4 = _e170;
    param_5 = 0i;
    let _e171 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e171;
    param_6 = 1u;
    let _e172 = frag_tex_coord1_1;
    param_7 = _e172;
    param_8 = 1i;
    let _e173 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e173;
    param_9 = 2u;
    let _e174 = frag_tex_coord2_1;
    param_10 = _e174;
    param_11 = 2i;
    let _e175 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor2_ = _e175;
    if override_type_3_ {
        let _e176 = sampledColor1_;
        local = _e176.xyz;
    } else {
        let _e178 = sampledColor0_;
        local = _e178.xyz;
    }
    let _e180 = local;
    materialAlbedo = _e180;
    let _e181 = sampledColor0_;
    let _e182 = frag_color0_;
    color0_ = (_e181 * _e182);
    if override_type_3_1 {
        let _e184 = sampledColor1_;
        color1_ = _e184;
        let _e185 = sampledColor2_;
        color2_ = _e185;
        let _e186 = color0_;
        let _e188 = color1_;
        let _e191 = color2_;
        let _e193 = ((_e186.xyz + _e188.xyz) + _e191.xyz);
        let _e195 = color0_[3u];
        let _e197 = color1_[3u];
        let _e200 = color2_[3u];
        base = vec4<f32>(_e193.x, _e193.y, _e193.z, ((_e195 * _e197) * _e200));
    } else {
        if override_type_3_2 {
            let _e206 = sampledColor1_;
            let _e207 = frag_color0_;
            color1_1 = (_e206 * _e207);
            let _e209 = sampledColor2_;
            let _e210 = frag_color0_;
            color2_1 = (_e209 * _e210);
            let _e212 = color0_;
            let _e214 = color1_1;
            let _e217 = color2_1;
            let _e219 = ((_e212.xyz + _e214.xyz) + _e217.xyz);
            let _e221 = color0_[3u];
            let _e223 = color1_1[3u];
            let _e226 = color2_1[3u];
            base = vec4<f32>(_e219.x, _e219.y, _e219.z, ((_e221 * _e223) * _e226));
        } else {
            let _e232 = sampledColor1_;
            color1_2 = _e232;
            let _e233 = sampledColor2_;
            color2_2 = _e233;
            let _e234 = color0_;
            let _e236 = color1_2;
            let _e239 = color2_2;
            let _e241 = ((_e234.xyz * _e236.xyz) * _e239.xyz);
            base[0u] = _e241.x;
            base[1u] = _e241.y;
            base[2u] = _e241.z;
            let _e249 = color0_[3u];
            let _e251 = color1_2[3u];
            let _e254 = color2_2[3u];
            base[3u] = ((_e249 * _e251) * _e254);
        }
    }
    if override_type_3_3 {
        let _e259 = unnamed.worldLightParams[1u];
        wetness = clamp(_e259, 0f, 1f);
        let _e263 = unnamed.worldLightParams[2u];
        frost = clamp(_e263, 0f, 1f);
        let _e265 = base;
        luminance = dot(_e265.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e268 = wetness;
        let _e270 = base;
        let _e272 = (_e270.xyz * mix(1f, 0.82f, _e268));
        base[0u] = _e272.x;
        base[1u] = _e272.y;
        base[2u] = _e272.z;
        let _e279 = base;
        let _e281 = luminance;
        let _e283 = luminance;
        let _e285 = luminance;
        let _e287 = frost;
        let _e290 = mix(_e279.xyz, vec3<f32>((_e281 * 0.88f), (_e283 * 0.94f), _e285), vec3((_e287 * 0.55f)));
        base[0u] = _e290.x;
        base[1u] = _e290.y;
        base[2u] = _e290.z;
        let _e297 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e297).z);
        let _e303 = unnamed.worldLightParams[3u];
        let _e305 = upward;
        snowCoverage = (clamp(_e303, 0f, 1f) * _e305);
        let _e307 = base;
        let _e309 = snowCoverage;
        let _e312 = mix(_e307.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e309 * 0.82f)));
        base[0u] = _e312.x;
        base[1u] = _e312.y;
        base[2u] = _e312.z;
    }
    let _e321 = unnamed.materialFactors[0u];
    if (_e321 >= 0f) {
        let _e324 = unnamed.materialBaseColor;
        let _e325 = base;
        base = (_e325 * _e324);
    }
    let _e327 = color0_;
    let _e330 = unnamed.emissionRadiance;
    let _e333 = base;
    let _e335 = (_e333.xyz + (_e327.xyz * _e330.xyz));
    base[0u] = _e335.x;
    base[1u] = _e335.y;
    base[2u] = _e335.z;
    if override_type_3_4 {
        if override_type_3_5 {
            let _e345 = unnamed.packed_indices[2i][0u];
            let _e351 = unnamed.packed_indices[2i][0u];
            let _e356 = frag_tex_coord0_1;
            let _e357 = textureSample(wired_bindless_images[(_e345 & 4095u)], wired_bindless_samplers[((_e351 >> bitcast<u32>(12i)) & 255u)], _e356);
            local_1 = _e357.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e359 = local_1;
        orm = _e359;
        let _e361 = orm[0u];
        let _e364 = unnamed.materialFactors[2u];
        ao = mix(1f, _e361, clamp(_e364, 0f, 1f));
        let _e368 = orm[1u];
        let _e371 = unnamed.materialFactors[1u];
        let _e375 = unnamed.worldLightParams[1u];
        let _e380 = unnamed.worldLightParams[2u];
        let _e385 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e368 * _e371) - (0.35f * _e375)) + (0.3f * _e380)) + (0.4f * _e385)), 0.04f, 1f);
        let _e390 = orm[2u];
        let _e393 = unnamed.materialFactors[0u];
        metalness = clamp((_e390 * _e393), 0f, 1f);
        let _e396 = ibl_N_1;
        ibl_n = normalize(_e396);
        let _e398 = ibl_V_1;
        ibl_v = normalize(_e398);
        let _e400 = materialAlbedo;
        let _e402 = unnamed.materialBaseColor;
        let _e405 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e400 * _e402.xyz), vec3(_e405));
        let _e408 = ibl_n;
        let _e409 = ibl_v;
        NdotV = max(dot(_e408, _e409), 0f);
        let _e412 = NdotV;
        param_12 = _e412;
        let _e413 = F0_1;
        param_13 = _e413;
        let _e414 = roughness_1;
        param_14 = _e414;
        let _e415 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
        F_amb = _e415;
        let _e416 = ibl_v;
        let _e418 = ibl_n;
        R = reflect(-(_e416), _e418);
        let _e422 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e422, 0f, 1f);
        let _e424 = R;
        let _e425 = roughness_1;
        let _e426 = textureNumLevels(radianceCube);
        let _e431 = textureSampleLevel(radianceCube, radianceCube_sampler, _e424, (_e425 * f32((i32(_e426) - 1i))));
        currentPrefiltered = _e431.xyz;
        let _e433 = R;
        let _e434 = roughness_1;
        let _e435 = textureNumLevels(previousRadianceCube);
        let _e440 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e433, (_e434 * f32((i32(_e435) - 1i))));
        previousPrefiltered = _e440.xyz;
        let _e442 = previousPrefiltered;
        let _e443 = currentPrefiltered;
        let _e444 = environmentBlend;
        prefiltered = mix(_e442, _e443, vec3(_e444));
        let _e447 = NdotV;
        let _e448 = roughness_1;
        let _e450 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e447, _e448), 0f);
        envBRDF = _e450.xy;
        let _e452 = prefiltered;
        let _e453 = F_amb;
        let _e455 = envBRDF[0u];
        let _e458 = envBRDF[1u];
        let _e462 = ao;
        specularIBL = ((_e452 * ((_e453 * _e455) + vec3(_e458))) * _e462);
        let _e464 = gl_FragCoord_1;
        let _e466 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e464.xy / vec2<f32>(vec2<i32>(_e466)));
        let _e470 = gtaoUV;
        let _e471 = textureSample(gtaoMap, gtaoMap_sampler, _e470);
        gtao_vis = _e471.x;
        let _e473 = gtao_vis;
        let _e474 = specularIBL;
        specularIBL = (_e474 * _e473);
        let _e476 = specularIBL;
        let _e477 = base;
        let _e479 = (_e477.xyz + _e476);
        base[0u] = _e479.x;
        base[1u] = _e479.y;
        base[2u] = _e479.z;
    }
    let _e486 = wired_advanced_fog_enabled_u0028_();
    if _e486 {
        let _e487 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e487;
        if override_type_3_6 {
            let _e488 = fogAmount;
            let _e490 = base;
            let _e492 = (_e490.xyz * (1f - _e488));
            base[0u] = _e492.x;
            base[1u] = _e492.y;
            base[2u] = _e492.z;
        } else {
            if override_type_3_7 {
                let _e499 = fogAmount;
                let _e501 = base;
                base = (_e501 * (1f - _e499));
            } else {
                if override_type_3_8 {
                    let _e503 = fogAmount;
                    let _e506 = base[3u];
                    base[3u] = (_e506 * (1f - _e503));
                } else {
                    let _e509 = base;
                    let _e512 = unnamed.advancedFogColorDensity;
                    let _e514 = fogAmount;
                    let _e516 = mix(_e509.xyz, _e512.xyz, vec3(_e514));
                    base[0u] = _e516.x;
                    base[1u] = _e516.y;
                    base[2u] = _e516.z;
                }
            }
        }
    } else {
        if override_type_3_9 {
            let _e523 = base;
            let _e526 = fog[3u];
            let _e528 = (_e523.xyz * (1f - _e526));
            base[0u] = _e528.x;
            base[1u] = _e528.y;
            base[2u] = _e528.z;
        } else {
            if override_type_3_10 {
                let _e535 = base;
                let _e537 = fog[3u];
                base = (_e535 * (1f - _e537));
            } else {
                if override_type_3_11 {
                    let _e541 = base[3u];
                    let _e543 = fog[3u];
                    base[3u] = (_e541 * (1f - _e543));
                } else {
                    let _e547 = base;
                    let _e548 = fog;
                    let _e550 = unnamed.fogColor;
                    let _e553 = fog[3u];
                    base = mix(_e547, (_e548 * _e550), vec4(_e553));
                }
            }
        }
    }
    if override_type_3_12 {
        let _e557 = base[3u];
        if (_e557 == 0f) {
            discard;
        }
    } else {
        if override_type_3_13 {
            let _e559 = base;
            let _e561 = base;
            if (dot(_e559.xyz, _e561.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e565 = base;
    out_color = _e565;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e17 = out_color;
    return _e17;
}
