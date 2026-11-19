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

    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e85 + 0.5f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e92 = fogType;
    let _e95 = fogType;
    return (((_e90 > 0.5f) && (_e92 >= 1i)) && (_e95 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e85 = wired_advanced_fog_enabled_u0028_();
    if !(_e85) {
        return 0f;
    }
    let _e88 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e88, 0.000001f));
    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e93 + 0.5f));
    let _e96 = fogType_1;
    if (_e96 == 1i) {
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e100 <= 0f) {
            return 0f;
        }
        let _e102 = viewDepth;
        let _e105 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e102 / _e105), 0f, 1f);
    }
    let _e110 = unnamed.advancedFogColorDensity[3u];
    let _e112 = viewDepth;
    opticalDepth = (max(_e110, 0f) * _e112);
    let _e114 = fogType_1;
    if (_e114 == 2i) {
        let _e116 = opticalDepth;
        return clamp((1f - exp(-(_e116))), 0f, 1f);
    }
    let _e121 = opticalDepth;
    let _e122 = opticalDepth;
    return clamp((1f - exp(-((_e121 * _e122)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e88 = (*cosTheta);
    t = (1f - _e88);
    let _e90 = t;
    let _e91 = t;
    t2_ = (_e90 * _e91);
    let _e93 = (*roughness);
    let _e96 = (*F0_);
    Fmax = max(vec3((1f - _e93)), _e96);
    let _e98 = (*F0_);
    let _e99 = Fmax;
    let _e100 = (*F0_);
    let _e102 = t2_;
    let _e103 = t2_;
    let _e105 = t;
    return (_e98 + ((_e99 - _e100) * ((_e102 * _e103) * _e105)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e85 = (*rgb);
    let _e88 = unnamed.worldLightParams[0u];
    boosted = (_e85 * _e88);
    let _e91 = boosted[0u];
    let _e93 = boosted[1u];
    let _e95 = boosted[2u];
    peak = max(_e91, max(_e93, _e95));
    let _e98 = peak;
    if (_e98 > 1f) {
        let _e100 = peak;
        let _e101 = boosted;
        boosted = (_e101 / vec3(_e100));
    }
    let _e104 = boosted;
    return _e104;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e86 = (*c);
    (*c) = max(_e86, vec3<f32>(0f, 0f, 0f));
    let _e88 = (*c);
    cutoff = (_e88 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e90 = (*c);
    lo = (_e90 / vec3(12.92f));
    let _e93 = (*c);
    hi = pow(((_e93 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e98 = hi;
    let _e99 = lo;
    let _e100 = cutoff;
    return mix(_e98, _e99, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e100));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e98 = (*role);
    let _e100 = (*role);
    let _e105 = unnamed.packed_indices[(_e98 / 4u)][(_e100 % 4u)];
    let _e110 = (*uv);
    let _e111 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    c_1 = _e111;
    let _e112 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e112))) == 0i) {
        let _e117 = c_1;
        param = _e117.xyz;
        let _e119 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = (*slot);
    if (lightmap_slot == (_e126 + 1i)) {
        let _e129 = c_1;
        param_1 = _e129.xyz;
        let _e131 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e131.x;
        c_1[1u] = _e131.y;
        c_1[2u] = _e131.z;
    }
    let _e138 = c_1;
    return _e138;
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

    let _e127 = frag_color0In_1;
    param_2 = _e127.xyz;
    let _e129 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e131 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e129.x, _e129.y, _e129.z, _e131);
    param_3 = 0u;
    let _e136 = frag_tex_coord0_1;
    param_4 = _e136;
    param_5 = 0i;
    let _e137 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e137;
    param_6 = 1u;
    let _e138 = frag_tex_coord1_1;
    param_7 = _e138;
    param_8 = 1i;
    let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e139;
    if override_type_3_ {
        let _e140 = sampledColor1_;
        local = _e140.xyz;
    } else {
        let _e142 = sampledColor0_;
        local = _e142.xyz;
    }
    let _e144 = local;
    materialAlbedo = _e144;
    let _e145 = sampledColor0_;
    let _e146 = frag_color0_;
    color0_ = (_e145 * _e146);
    if override_type_3_1 {
        let _e148 = sampledColor1_;
        color1_ = _e148;
        let _e149 = color0_;
        let _e151 = color1_;
        let _e153 = (_e149.xyz + _e151.xyz);
        let _e155 = color0_[3u];
        let _e157 = color1_[3u];
        base = vec4<f32>(_e153.x, _e153.y, _e153.z, (_e155 * _e157));
    } else {
        if override_type_3_2 {
            let _e163 = sampledColor1_;
            let _e164 = frag_color0_;
            color1_1 = (_e163 * _e164);
            let _e166 = color0_;
            let _e168 = color1_1;
            let _e170 = (_e166.xyz + _e168.xyz);
            let _e172 = color0_[3u];
            let _e174 = color1_1[3u];
            base = vec4<f32>(_e170.x, _e170.y, _e170.z, (_e172 * _e174));
        } else {
            let _e180 = sampledColor1_;
            color1_2 = _e180;
            let _e181 = color0_;
            let _e183 = color1_2;
            let _e185 = (_e181.xyz * _e183.xyz);
            base[0u] = _e185.x;
            base[1u] = _e185.y;
            base[2u] = _e185.z;
            let _e193 = color0_[3u];
            let _e195 = color1_2[3u];
            base[3u] = (_e193 * _e195);
        }
    }
    if override_type_3_3 {
        let _e200 = unnamed.worldLightParams[1u];
        wetness = clamp(_e200, 0f, 1f);
        let _e204 = unnamed.worldLightParams[2u];
        frost = clamp(_e204, 0f, 1f);
        let _e206 = base;
        luminance = dot(_e206.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e209 = wetness;
        let _e211 = base;
        let _e213 = (_e211.xyz * mix(1f, 0.82f, _e209));
        base[0u] = _e213.x;
        base[1u] = _e213.y;
        base[2u] = _e213.z;
        let _e220 = base;
        let _e222 = luminance;
        let _e224 = luminance;
        let _e226 = luminance;
        let _e228 = frost;
        let _e231 = mix(_e220.xyz, vec3<f32>((_e222 * 0.88f), (_e224 * 0.94f), _e226), vec3((_e228 * 0.55f)));
        base[0u] = _e231.x;
        base[1u] = _e231.y;
        base[2u] = _e231.z;
        let _e238 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e238).z);
        let _e244 = unnamed.worldLightParams[3u];
        let _e246 = upward;
        snowCoverage = (clamp(_e244, 0f, 1f) * _e246);
        let _e248 = base;
        let _e250 = snowCoverage;
        let _e253 = mix(_e248.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e250 * 0.82f)));
        base[0u] = _e253.x;
        base[1u] = _e253.y;
        base[2u] = _e253.z;
    }
    let _e262 = unnamed.materialFactors[0u];
    if (_e262 >= 0f) {
        let _e265 = unnamed.materialBaseColor;
        let _e266 = base;
        base = (_e266 * _e265);
    }
    let _e268 = color0_;
    let _e271 = unnamed.emissionRadiance;
    let _e274 = base;
    let _e276 = (_e274.xyz + (_e268.xyz * _e271.xyz));
    base[0u] = _e276.x;
    base[1u] = _e276.y;
    base[2u] = _e276.z;
    if override_type_3_4 {
        if override_type_3_5 {
            let _e286 = unnamed.packed_indices[2i][0u];
            let _e292 = unnamed.packed_indices[2i][0u];
            let _e297 = frag_tex_coord0_1;
            let _e298 = textureSample(wired_bindless_images[(_e286 & 4095u)], wired_bindless_samplers[((_e292 >> bitcast<u32>(12i)) & 255u)], _e297);
            local_1 = _e298.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e300 = local_1;
        orm = _e300;
        let _e302 = orm[0u];
        let _e305 = unnamed.materialFactors[2u];
        ao = mix(1f, _e302, clamp(_e305, 0f, 1f));
        let _e309 = orm[1u];
        let _e312 = unnamed.materialFactors[1u];
        let _e316 = unnamed.worldLightParams[1u];
        let _e321 = unnamed.worldLightParams[2u];
        let _e326 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e309 * _e312) - (0.35f * _e316)) + (0.3f * _e321)) + (0.4f * _e326)), 0.04f, 1f);
        let _e331 = orm[2u];
        let _e334 = unnamed.materialFactors[0u];
        metalness = clamp((_e331 * _e334), 0f, 1f);
        let _e337 = ibl_N_1;
        ibl_n = normalize(_e337);
        let _e339 = ibl_V_1;
        ibl_v = normalize(_e339);
        let _e341 = materialAlbedo;
        let _e343 = unnamed.materialBaseColor;
        let _e346 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e341 * _e343.xyz), vec3(_e346));
        let _e349 = ibl_n;
        let _e350 = ibl_v;
        NdotV = max(dot(_e349, _e350), 0f);
        let _e353 = NdotV;
        param_9 = _e353;
        let _e354 = F0_1;
        param_10 = _e354;
        let _e355 = roughness_1;
        param_11 = _e355;
        let _e356 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_9), (&param_10), (&param_11));
        F_amb = _e356;
        let _e357 = ibl_v;
        let _e359 = ibl_n;
        R = reflect(-(_e357), _e359);
        let _e363 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e363, 0f, 1f);
        let _e365 = R;
        let _e366 = roughness_1;
        let _e367 = textureNumLevels(radianceCube);
        let _e372 = textureSampleLevel(radianceCube, radianceCube_sampler, _e365, (_e366 * f32((i32(_e367) - 1i))));
        currentPrefiltered = _e372.xyz;
        let _e374 = R;
        let _e375 = roughness_1;
        let _e376 = textureNumLevels(previousRadianceCube);
        let _e381 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e374, (_e375 * f32((i32(_e376) - 1i))));
        previousPrefiltered = _e381.xyz;
        let _e383 = previousPrefiltered;
        let _e384 = currentPrefiltered;
        let _e385 = environmentBlend;
        prefiltered = mix(_e383, _e384, vec3(_e385));
        let _e388 = NdotV;
        let _e389 = roughness_1;
        let _e391 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e388, _e389), 0f);
        envBRDF = _e391.xy;
        let _e393 = prefiltered;
        let _e394 = F_amb;
        let _e396 = envBRDF[0u];
        let _e399 = envBRDF[1u];
        let _e403 = ao;
        specularIBL = ((_e393 * ((_e394 * _e396) + vec3(_e399))) * _e403);
        let _e405 = gl_FragCoord_1;
        let _e407 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e405.xy / vec2<f32>(vec2<i32>(_e407)));
        let _e411 = gtaoUV;
        let _e412 = textureSample(gtaoMap, gtaoMap_sampler, _e411);
        gtao_vis = _e412.x;
        let _e414 = gtao_vis;
        let _e415 = specularIBL;
        specularIBL = (_e415 * _e414);
        let _e417 = specularIBL;
        let _e418 = base;
        let _e420 = (_e418.xyz + _e417);
        base[0u] = _e420.x;
        base[1u] = _e420.y;
        base[2u] = _e420.z;
    }
    let _e427 = wired_advanced_fog_enabled_u0028_();
    if _e427 {
        let _e428 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e428;
        let _e429 = base;
        let _e432 = unnamed.advancedFogColorDensity;
        let _e434 = fogAmount;
        let _e436 = mix(_e429.xyz, _e432.xyz, vec3(_e434));
        base[0u] = _e436.x;
        base[1u] = _e436.y;
        base[2u] = _e436.z;
    }
    if override_type_3_6 {
        let _e444 = base[3u];
        if (_e444 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e446 = base;
            let _e448 = base;
            if (dot(_e446.xyz, _e448.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e452 = base;
    out_color = _e452;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e13 = out_color;
    return _e13;
}
