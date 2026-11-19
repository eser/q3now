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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_6: bool = (discard_mode == 1i);
override override_type_3_7: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
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

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e84 = (*rgb);
    let _e87 = unnamed.worldLightParams[0u];
    boosted = (_e84 * _e87);
    let _e90 = boosted[0u];
    let _e92 = boosted[1u];
    let _e94 = boosted[2u];
    peak = max(_e90, max(_e92, _e94));
    let _e97 = peak;
    if (_e97 > 1f) {
        let _e99 = peak;
        let _e100 = boosted;
        boosted = (_e100 / vec3(_e99));
    }
    let _e103 = boosted;
    return _e103;
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
    var param_1: vec3<f32>;

    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e109 = (*uv);
    let _e110 = textureSample(wired_bindless_images[(_e94 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    c_1 = _e110;
    let _e111 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e111))) == 0i) {
        let _e116 = c_1;
        param = _e116.xyz;
        let _e118 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = (*slot);
    if (lightmap_slot == (_e125 + 1i)) {
        let _e128 = c_1;
        param_1 = _e128.xyz;
        let _e130 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e130.x;
        c_1[1u] = _e130.y;
        c_1[2u] = _e130.z;
    }
    let _e137 = c_1;
    return _e137;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var sampledColor1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
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
    var param_8: f32;
    var param_9: vec3<f32>;
    var param_10: f32;
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

    param_2 = 0u;
    let _e124 = frag_tex_coord0_1;
    param_3 = _e124;
    param_4 = 0i;
    let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e125;
    param_5 = 1u;
    let _e126 = frag_tex_coord1_1;
    param_6 = _e126;
    param_7 = 1i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e127;
    if override_type_3_ {
        let _e128 = sampledColor1_;
        local = _e128.xyz;
    } else {
        let _e130 = sampledColor0_;
        local = _e130.xyz;
    }
    let _e132 = local;
    materialAlbedo = _e132;
    let _e133 = sampledColor0_;
    color0_ = _e133;
    if override_type_3_1 {
        let _e134 = sampledColor1_;
        color1_ = _e134;
        let _e135 = color0_;
        let _e137 = color1_;
        let _e139 = (_e135.xyz + _e137.xyz);
        let _e141 = color0_[3u];
        let _e143 = color1_[3u];
        base = vec4<f32>(_e139.x, _e139.y, _e139.z, (_e141 * _e143));
    } else {
        if override_type_3_2 {
            let _e149 = sampledColor1_;
            color1_1 = _e149;
            let _e150 = color0_;
            let _e152 = color1_1;
            let _e154 = (_e150.xyz + _e152.xyz);
            let _e156 = color0_[3u];
            let _e158 = color1_1[3u];
            base = vec4<f32>(_e154.x, _e154.y, _e154.z, (_e156 * _e158));
        } else {
            let _e164 = sampledColor1_;
            color1_2 = _e164;
            let _e165 = color0_;
            let _e167 = color1_2;
            let _e169 = (_e165.xyz * _e167.xyz);
            base[0u] = _e169.x;
            base[1u] = _e169.y;
            base[2u] = _e169.z;
            let _e177 = color0_[3u];
            let _e179 = color1_2[3u];
            base[3u] = (_e177 * _e179);
        }
    }
    if override_type_3_3 {
        let _e184 = unnamed.worldLightParams[1u];
        wetness = clamp(_e184, 0f, 1f);
        let _e188 = unnamed.worldLightParams[2u];
        frost = clamp(_e188, 0f, 1f);
        let _e190 = base;
        luminance = dot(_e190.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e193 = wetness;
        let _e195 = base;
        let _e197 = (_e195.xyz * mix(1f, 0.82f, _e193));
        base[0u] = _e197.x;
        base[1u] = _e197.y;
        base[2u] = _e197.z;
        let _e204 = base;
        let _e206 = luminance;
        let _e208 = luminance;
        let _e210 = luminance;
        let _e212 = frost;
        let _e215 = mix(_e204.xyz, vec3<f32>((_e206 * 0.88f), (_e208 * 0.94f), _e210), vec3((_e212 * 0.55f)));
        base[0u] = _e215.x;
        base[1u] = _e215.y;
        base[2u] = _e215.z;
        let _e222 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e222).z);
        let _e228 = unnamed.worldLightParams[3u];
        let _e230 = upward;
        snowCoverage = (clamp(_e228, 0f, 1f) * _e230);
        let _e232 = base;
        let _e234 = snowCoverage;
        let _e237 = mix(_e232.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e234 * 0.82f)));
        base[0u] = _e237.x;
        base[1u] = _e237.y;
        base[2u] = _e237.z;
    }
    let _e246 = unnamed.materialFactors[0u];
    if (_e246 >= 0f) {
        let _e249 = unnamed.materialBaseColor;
        let _e250 = base;
        base = (_e250 * _e249);
    }
    let _e252 = color0_;
    let _e255 = unnamed.emissionRadiance;
    let _e258 = base;
    let _e260 = (_e258.xyz + (_e252.xyz * _e255.xyz));
    base[0u] = _e260.x;
    base[1u] = _e260.y;
    base[2u] = _e260.z;
    if override_type_3_4 {
        if override_type_3_5 {
            let _e270 = unnamed.packed_indices[2i][0u];
            let _e276 = unnamed.packed_indices[2i][0u];
            let _e281 = frag_tex_coord0_1;
            let _e282 = textureSample(wired_bindless_images[(_e270 & 4095u)], wired_bindless_samplers[((_e276 >> bitcast<u32>(12i)) & 255u)], _e281);
            local_1 = _e282.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e284 = local_1;
        orm = _e284;
        let _e286 = orm[0u];
        let _e289 = unnamed.materialFactors[2u];
        ao = mix(1f, _e286, clamp(_e289, 0f, 1f));
        let _e293 = orm[1u];
        let _e296 = unnamed.materialFactors[1u];
        let _e300 = unnamed.worldLightParams[1u];
        let _e305 = unnamed.worldLightParams[2u];
        let _e310 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e293 * _e296) - (0.35f * _e300)) + (0.3f * _e305)) + (0.4f * _e310)), 0.04f, 1f);
        let _e315 = orm[2u];
        let _e318 = unnamed.materialFactors[0u];
        metalness = clamp((_e315 * _e318), 0f, 1f);
        let _e321 = ibl_N_1;
        ibl_n = normalize(_e321);
        let _e323 = ibl_V_1;
        ibl_v = normalize(_e323);
        let _e325 = materialAlbedo;
        let _e327 = unnamed.materialBaseColor;
        let _e330 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e325 * _e327.xyz), vec3(_e330));
        let _e333 = ibl_n;
        let _e334 = ibl_v;
        NdotV = max(dot(_e333, _e334), 0f);
        let _e337 = NdotV;
        param_8 = _e337;
        let _e338 = F0_1;
        param_9 = _e338;
        let _e339 = roughness_1;
        param_10 = _e339;
        let _e340 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_8), (&param_9), (&param_10));
        F_amb = _e340;
        let _e341 = ibl_v;
        let _e343 = ibl_n;
        R = reflect(-(_e341), _e343);
        let _e347 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e347, 0f, 1f);
        let _e349 = R;
        let _e350 = roughness_1;
        let _e351 = textureNumLevels(radianceCube);
        let _e356 = textureSampleLevel(radianceCube, radianceCube_sampler, _e349, (_e350 * f32((i32(_e351) - 1i))));
        currentPrefiltered = _e356.xyz;
        let _e358 = R;
        let _e359 = roughness_1;
        let _e360 = textureNumLevels(previousRadianceCube);
        let _e365 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e358, (_e359 * f32((i32(_e360) - 1i))));
        previousPrefiltered = _e365.xyz;
        let _e367 = previousPrefiltered;
        let _e368 = currentPrefiltered;
        let _e369 = environmentBlend;
        prefiltered = mix(_e367, _e368, vec3(_e369));
        let _e372 = NdotV;
        let _e373 = roughness_1;
        let _e375 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e372, _e373), 0f);
        envBRDF = _e375.xy;
        let _e377 = prefiltered;
        let _e378 = F_amb;
        let _e380 = envBRDF[0u];
        let _e383 = envBRDF[1u];
        let _e387 = ao;
        specularIBL = ((_e377 * ((_e378 * _e380) + vec3(_e383))) * _e387);
        let _e389 = gl_FragCoord_1;
        let _e391 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e389.xy / vec2<f32>(vec2<i32>(_e391)));
        let _e395 = gtaoUV;
        let _e396 = textureSample(gtaoMap, gtaoMap_sampler, _e395);
        gtao_vis = _e396.x;
        let _e398 = gtao_vis;
        let _e399 = specularIBL;
        specularIBL = (_e399 * _e398);
        let _e401 = specularIBL;
        let _e402 = base;
        let _e404 = (_e402.xyz + _e401);
        base[0u] = _e404.x;
        base[1u] = _e404.y;
        base[2u] = _e404.z;
    }
    let _e411 = wired_advanced_fog_enabled_u0028_();
    if _e411 {
        let _e412 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e412;
        let _e413 = base;
        let _e416 = unnamed.advancedFogColorDensity;
        let _e418 = fogAmount;
        let _e420 = mix(_e413.xyz, _e416.xyz, vec3(_e418));
        base[0u] = _e420.x;
        base[1u] = _e420.y;
        base[2u] = _e420.z;
    }
    if override_type_3_6 {
        let _e428 = base[3u];
        if (_e428 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e430 = base;
            let _e432 = base;
            if (dot(_e430.xyz, _e432.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e436 = base;
    out_color = _e436;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e11 = out_color;
    return _e11;
}
