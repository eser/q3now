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

    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e86 + 0.5f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e93 = fogType;
    let _e96 = fogType;
    return (((_e91 > 0.5f) && (_e93 >= 1i)) && (_e96 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e86 = wired_advanced_fog_enabled_u0028_();
    if !(_e86) {
        return 0f;
    }
    let _e89 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e89, 0.000001f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e94 + 0.5f));
    let _e97 = fogType_1;
    if (_e97 == 1i) {
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e101 <= 0f) {
            return 0f;
        }
        let _e103 = viewDepth;
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e103 / _e106), 0f, 1f);
    }
    let _e111 = unnamed.advancedFogColorDensity[3u];
    let _e113 = viewDepth;
    opticalDepth = (max(_e111, 0f) * _e113);
    let _e115 = fogType_1;
    if (_e115 == 2i) {
        let _e117 = opticalDepth;
        return clamp((1f - exp(-(_e117))), 0f, 1f);
    }
    let _e122 = opticalDepth;
    let _e123 = opticalDepth;
    return clamp((1f - exp(-((_e122 * _e123)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e89 = (*cosTheta);
    t = (1f - _e89);
    let _e91 = t;
    let _e92 = t;
    t2_ = (_e91 * _e92);
    let _e94 = (*roughness);
    let _e97 = (*F0_);
    Fmax = max(vec3((1f - _e94)), _e97);
    let _e99 = (*F0_);
    let _e100 = Fmax;
    let _e101 = (*F0_);
    let _e103 = t2_;
    let _e104 = t2_;
    let _e106 = t;
    return (_e99 + ((_e100 - _e101) * ((_e103 * _e104) * _e106)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e86 = (*rgb);
    let _e89 = unnamed.worldLightParams[0u];
    boosted = (_e86 * _e89);
    let _e92 = boosted[0u];
    let _e94 = boosted[1u];
    let _e96 = boosted[2u];
    peak = max(_e92, max(_e94, _e96));
    let _e99 = peak;
    if (_e99 > 1f) {
        let _e101 = peak;
        let _e102 = boosted;
        boosted = (_e102 / vec3(_e101));
    }
    let _e105 = boosted;
    return _e105;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e87 = (*c);
    (*c) = max(_e87, vec3<f32>(0f, 0f, 0f));
    let _e89 = (*c);
    cutoff = (_e89 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e91 = (*c);
    lo = (_e91 / vec3(12.92f));
    let _e94 = (*c);
    hi = pow(((_e94 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e99 = hi;
    let _e100 = lo;
    let _e101 = cutoff;
    return mix(_e99, _e100, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e101));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e111 = (*uv);
    let _e112 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    c_1 = _e112;
    let _e113 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e113))) == 0i) {
        let _e118 = c_1;
        param = _e118.xyz;
        let _e120 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = (*slot);
    if (lightmap_slot == (_e127 + 1i)) {
        let _e130 = c_1;
        param_1 = _e130.xyz;
        let _e132 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e132.x;
        c_1[1u] = _e132.y;
        c_1[2u] = _e132.z;
    }
    let _e139 = c_1;
    return _e139;
}

fn main_1() {
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

    let _e135 = frag_color0In_1;
    param_2 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e139 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    param_3 = 0u;
    let _e144 = frag_tex_coord0_1;
    param_4 = _e144;
    param_5 = 0i;
    let _e145 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e145;
    param_6 = 1u;
    let _e146 = frag_tex_coord1_1;
    param_7 = _e146;
    param_8 = 1i;
    let _e147 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e147;
    param_9 = 2u;
    let _e148 = frag_tex_coord2_1;
    param_10 = _e148;
    param_11 = 2i;
    let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor2_ = _e149;
    if override_type_3_ {
        let _e150 = sampledColor1_;
        local = _e150.xyz;
    } else {
        let _e152 = sampledColor0_;
        local = _e152.xyz;
    }
    let _e154 = local;
    materialAlbedo = _e154;
    let _e155 = sampledColor0_;
    let _e156 = frag_color0_;
    color0_ = (_e155 * _e156);
    if override_type_3_1 {
        let _e158 = sampledColor1_;
        color1_ = _e158;
        let _e159 = sampledColor2_;
        color2_ = _e159;
        let _e160 = color0_;
        let _e162 = color1_;
        let _e165 = color2_;
        let _e167 = ((_e160.xyz + _e162.xyz) + _e165.xyz);
        let _e169 = color0_[3u];
        let _e171 = color1_[3u];
        let _e174 = color2_[3u];
        base = vec4<f32>(_e167.x, _e167.y, _e167.z, ((_e169 * _e171) * _e174));
    } else {
        if override_type_3_2 {
            let _e180 = sampledColor1_;
            let _e181 = frag_color0_;
            color1_1 = (_e180 * _e181);
            let _e183 = sampledColor2_;
            let _e184 = frag_color0_;
            color2_1 = (_e183 * _e184);
            let _e186 = color0_;
            let _e188 = color1_1;
            let _e191 = color2_1;
            let _e193 = ((_e186.xyz + _e188.xyz) + _e191.xyz);
            let _e195 = color0_[3u];
            let _e197 = color1_1[3u];
            let _e200 = color2_1[3u];
            base = vec4<f32>(_e193.x, _e193.y, _e193.z, ((_e195 * _e197) * _e200));
        } else {
            let _e206 = sampledColor1_;
            color1_2 = _e206;
            let _e207 = sampledColor2_;
            color2_2 = _e207;
            let _e208 = color0_;
            let _e210 = color1_2;
            let _e213 = color2_2;
            let _e215 = ((_e208.xyz * _e210.xyz) * _e213.xyz);
            base[0u] = _e215.x;
            base[1u] = _e215.y;
            base[2u] = _e215.z;
            let _e223 = color0_[3u];
            let _e225 = color1_2[3u];
            let _e228 = color2_2[3u];
            base[3u] = ((_e223 * _e225) * _e228);
        }
    }
    if override_type_3_3 {
        let _e233 = unnamed.worldLightParams[1u];
        wetness = clamp(_e233, 0f, 1f);
        let _e237 = unnamed.worldLightParams[2u];
        frost = clamp(_e237, 0f, 1f);
        let _e239 = base;
        luminance = dot(_e239.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e242 = wetness;
        let _e244 = base;
        let _e246 = (_e244.xyz * mix(1f, 0.82f, _e242));
        base[0u] = _e246.x;
        base[1u] = _e246.y;
        base[2u] = _e246.z;
        let _e253 = base;
        let _e255 = luminance;
        let _e257 = luminance;
        let _e259 = luminance;
        let _e261 = frost;
        let _e264 = mix(_e253.xyz, vec3<f32>((_e255 * 0.88f), (_e257 * 0.94f), _e259), vec3((_e261 * 0.55f)));
        base[0u] = _e264.x;
        base[1u] = _e264.y;
        base[2u] = _e264.z;
        let _e271 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e271).z);
        let _e277 = unnamed.worldLightParams[3u];
        let _e279 = upward;
        snowCoverage = (clamp(_e277, 0f, 1f) * _e279);
        let _e281 = base;
        let _e283 = snowCoverage;
        let _e286 = mix(_e281.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e283 * 0.82f)));
        base[0u] = _e286.x;
        base[1u] = _e286.y;
        base[2u] = _e286.z;
    }
    let _e295 = unnamed.materialFactors[0u];
    if (_e295 >= 0f) {
        let _e298 = unnamed.materialBaseColor;
        let _e299 = base;
        base = (_e299 * _e298);
    }
    let _e301 = color0_;
    let _e304 = unnamed.emissionRadiance;
    let _e307 = base;
    let _e309 = (_e307.xyz + (_e301.xyz * _e304.xyz));
    base[0u] = _e309.x;
    base[1u] = _e309.y;
    base[2u] = _e309.z;
    if override_type_3_4 {
        if override_type_3_5 {
            let _e319 = unnamed.packed_indices[2i][0u];
            let _e325 = unnamed.packed_indices[2i][0u];
            let _e330 = frag_tex_coord0_1;
            let _e331 = textureSample(wired_bindless_images[(_e319 & 4095u)], wired_bindless_samplers[((_e325 >> bitcast<u32>(12i)) & 255u)], _e330);
            local_1 = _e331.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e333 = local_1;
        orm = _e333;
        let _e335 = orm[0u];
        let _e338 = unnamed.materialFactors[2u];
        ao = mix(1f, _e335, clamp(_e338, 0f, 1f));
        let _e342 = orm[1u];
        let _e345 = unnamed.materialFactors[1u];
        let _e349 = unnamed.worldLightParams[1u];
        let _e354 = unnamed.worldLightParams[2u];
        let _e359 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e342 * _e345) - (0.35f * _e349)) + (0.3f * _e354)) + (0.4f * _e359)), 0.04f, 1f);
        let _e364 = orm[2u];
        let _e367 = unnamed.materialFactors[0u];
        metalness = clamp((_e364 * _e367), 0f, 1f);
        let _e370 = ibl_N_1;
        ibl_n = normalize(_e370);
        let _e372 = ibl_V_1;
        ibl_v = normalize(_e372);
        let _e374 = materialAlbedo;
        let _e376 = unnamed.materialBaseColor;
        let _e379 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e374 * _e376.xyz), vec3(_e379));
        let _e382 = ibl_n;
        let _e383 = ibl_v;
        NdotV = max(dot(_e382, _e383), 0f);
        let _e386 = NdotV;
        param_12 = _e386;
        let _e387 = F0_1;
        param_13 = _e387;
        let _e388 = roughness_1;
        param_14 = _e388;
        let _e389 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
        F_amb = _e389;
        let _e390 = ibl_v;
        let _e392 = ibl_n;
        R = reflect(-(_e390), _e392);
        let _e396 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e396, 0f, 1f);
        let _e398 = R;
        let _e399 = roughness_1;
        let _e400 = textureNumLevels(radianceCube);
        let _e405 = textureSampleLevel(radianceCube, radianceCube_sampler, _e398, (_e399 * f32((i32(_e400) - 1i))));
        currentPrefiltered = _e405.xyz;
        let _e407 = R;
        let _e408 = roughness_1;
        let _e409 = textureNumLevels(previousRadianceCube);
        let _e414 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e407, (_e408 * f32((i32(_e409) - 1i))));
        previousPrefiltered = _e414.xyz;
        let _e416 = previousPrefiltered;
        let _e417 = currentPrefiltered;
        let _e418 = environmentBlend;
        prefiltered = mix(_e416, _e417, vec3(_e418));
        let _e421 = NdotV;
        let _e422 = roughness_1;
        let _e424 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e421, _e422), 0f);
        envBRDF = _e424.xy;
        let _e426 = prefiltered;
        let _e427 = F_amb;
        let _e429 = envBRDF[0u];
        let _e432 = envBRDF[1u];
        let _e436 = ao;
        specularIBL = ((_e426 * ((_e427 * _e429) + vec3(_e432))) * _e436);
        let _e438 = gl_FragCoord_1;
        let _e440 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e438.xy / vec2<f32>(vec2<i32>(_e440)));
        let _e444 = gtaoUV;
        let _e445 = textureSample(gtaoMap, gtaoMap_sampler, _e444);
        gtao_vis = _e445.x;
        let _e447 = gtao_vis;
        let _e448 = specularIBL;
        specularIBL = (_e448 * _e447);
        let _e450 = specularIBL;
        let _e451 = base;
        let _e453 = (_e451.xyz + _e450);
        base[0u] = _e453.x;
        base[1u] = _e453.y;
        base[2u] = _e453.z;
    }
    let _e460 = wired_advanced_fog_enabled_u0028_();
    if _e460 {
        let _e461 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e461;
        let _e462 = base;
        let _e465 = unnamed.advancedFogColorDensity;
        let _e467 = fogAmount;
        let _e469 = mix(_e462.xyz, _e465.xyz, vec3(_e467));
        base[0u] = _e469.x;
        base[1u] = _e469.y;
        base[2u] = _e469.z;
    }
    if override_type_3_6 {
        let _e477 = base[3u];
        if (_e477 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e479 = base;
            let _e481 = base;
            if (dot(_e479.xyz, _e481.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e485 = base;
    out_color = _e485;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e15 = out_color;
    return _e15;
}
