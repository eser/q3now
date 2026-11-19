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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
    var frag_color: vec4<f32>;
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
    let _e127 = frag_tex_coord0_1;
    param_3 = _e127;
    param_4 = 0i;
    let _e128 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e128;
    param_5 = 1u;
    let _e129 = frag_tex_coord1_1;
    param_6 = _e129;
    param_7 = 1i;
    let _e130 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e130;
    if override_type_3_ {
        let _e131 = sampledColor1_;
        local = _e131.xyz;
    } else {
        let _e133 = sampledColor0_;
        local = _e133.xyz;
    }
    let _e135 = local;
    materialAlbedo = _e135;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e140 = sampledColor0_;
    let _e141 = frag_color;
    color0_ = (_e140 * _e141);
    if override_type_3_1 {
        let _e143 = sampledColor1_;
        color1_ = _e143;
        let _e144 = color0_;
        let _e146 = color1_;
        let _e148 = (_e144.xyz + _e146.xyz);
        let _e150 = color0_[3u];
        let _e152 = color1_[3u];
        base = vec4<f32>(_e148.x, _e148.y, _e148.z, (_e150 * _e152));
    } else {
        if override_type_3_2 {
            let _e158 = sampledColor1_;
            let _e159 = frag_color;
            color1_1 = (_e158 * _e159);
            let _e161 = color0_;
            let _e163 = color1_1;
            let _e165 = (_e161.xyz + _e163.xyz);
            let _e167 = color0_[3u];
            let _e169 = color1_1[3u];
            base = vec4<f32>(_e165.x, _e165.y, _e165.z, (_e167 * _e169));
        } else {
            let _e175 = sampledColor1_;
            let _e176 = frag_color;
            color1_2 = (_e175 * _e176);
            let _e178 = color0_;
            let _e180 = color1_2;
            let _e182 = (_e178.xyz * _e180.xyz);
            base[0u] = _e182.x;
            base[1u] = _e182.y;
            base[2u] = _e182.z;
            let _e190 = color0_[3u];
            let _e192 = color1_2[3u];
            base[3u] = (_e190 * _e192);
        }
    }
    if override_type_3_3 {
        let _e197 = unnamed.worldLightParams[1u];
        wetness = clamp(_e197, 0f, 1f);
        let _e201 = unnamed.worldLightParams[2u];
        frost = clamp(_e201, 0f, 1f);
        let _e203 = base;
        luminance = dot(_e203.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e206 = wetness;
        let _e208 = base;
        let _e210 = (_e208.xyz * mix(1f, 0.82f, _e206));
        base[0u] = _e210.x;
        base[1u] = _e210.y;
        base[2u] = _e210.z;
        let _e217 = base;
        let _e219 = luminance;
        let _e221 = luminance;
        let _e223 = luminance;
        let _e225 = frost;
        let _e228 = mix(_e217.xyz, vec3<f32>((_e219 * 0.88f), (_e221 * 0.94f), _e223), vec3((_e225 * 0.55f)));
        base[0u] = _e228.x;
        base[1u] = _e228.y;
        base[2u] = _e228.z;
        let _e235 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e235).z);
        let _e241 = unnamed.worldLightParams[3u];
        let _e243 = upward;
        snowCoverage = (clamp(_e241, 0f, 1f) * _e243);
        let _e245 = base;
        let _e247 = snowCoverage;
        let _e250 = mix(_e245.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e247 * 0.82f)));
        base[0u] = _e250.x;
        base[1u] = _e250.y;
        base[2u] = _e250.z;
    }
    let _e259 = unnamed.materialFactors[0u];
    if (_e259 >= 0f) {
        let _e262 = unnamed.materialBaseColor;
        let _e263 = base;
        base = (_e263 * _e262);
    }
    let _e265 = color0_;
    let _e268 = unnamed.emissionRadiance;
    let _e271 = base;
    let _e273 = (_e271.xyz + (_e265.xyz * _e268.xyz));
    base[0u] = _e273.x;
    base[1u] = _e273.y;
    base[2u] = _e273.z;
    if override_type_3_4 {
        if override_type_3_5 {
            let _e283 = unnamed.packed_indices[2i][0u];
            let _e289 = unnamed.packed_indices[2i][0u];
            let _e294 = frag_tex_coord0_1;
            let _e295 = textureSample(wired_bindless_images[(_e283 & 4095u)], wired_bindless_samplers[((_e289 >> bitcast<u32>(12i)) & 255u)], _e294);
            local_1 = _e295.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e297 = local_1;
        orm = _e297;
        let _e299 = orm[0u];
        let _e302 = unnamed.materialFactors[2u];
        ao = mix(1f, _e299, clamp(_e302, 0f, 1f));
        let _e306 = orm[1u];
        let _e309 = unnamed.materialFactors[1u];
        let _e313 = unnamed.worldLightParams[1u];
        let _e318 = unnamed.worldLightParams[2u];
        let _e323 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e306 * _e309) - (0.35f * _e313)) + (0.3f * _e318)) + (0.4f * _e323)), 0.04f, 1f);
        let _e328 = orm[2u];
        let _e331 = unnamed.materialFactors[0u];
        metalness = clamp((_e328 * _e331), 0f, 1f);
        let _e334 = ibl_N_1;
        ibl_n = normalize(_e334);
        let _e336 = ibl_V_1;
        ibl_v = normalize(_e336);
        let _e338 = materialAlbedo;
        let _e340 = unnamed.materialBaseColor;
        let _e343 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e338 * _e340.xyz), vec3(_e343));
        let _e346 = ibl_n;
        let _e347 = ibl_v;
        NdotV = max(dot(_e346, _e347), 0f);
        let _e350 = NdotV;
        param_8 = _e350;
        let _e351 = F0_1;
        param_9 = _e351;
        let _e352 = roughness_1;
        param_10 = _e352;
        let _e353 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_8), (&param_9), (&param_10));
        F_amb = _e353;
        let _e354 = ibl_v;
        let _e356 = ibl_n;
        R = reflect(-(_e354), _e356);
        let _e360 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e360, 0f, 1f);
        let _e362 = R;
        let _e363 = roughness_1;
        let _e364 = textureNumLevels(radianceCube);
        let _e369 = textureSampleLevel(radianceCube, radianceCube_sampler, _e362, (_e363 * f32((i32(_e364) - 1i))));
        currentPrefiltered = _e369.xyz;
        let _e371 = R;
        let _e372 = roughness_1;
        let _e373 = textureNumLevels(previousRadianceCube);
        let _e378 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e371, (_e372 * f32((i32(_e373) - 1i))));
        previousPrefiltered = _e378.xyz;
        let _e380 = previousPrefiltered;
        let _e381 = currentPrefiltered;
        let _e382 = environmentBlend;
        prefiltered = mix(_e380, _e381, vec3(_e382));
        let _e385 = NdotV;
        let _e386 = roughness_1;
        let _e388 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e385, _e386), 0f);
        envBRDF = _e388.xy;
        let _e390 = prefiltered;
        let _e391 = F_amb;
        let _e393 = envBRDF[0u];
        let _e396 = envBRDF[1u];
        let _e400 = ao;
        specularIBL = ((_e390 * ((_e391 * _e393) + vec3(_e396))) * _e400);
        let _e402 = gl_FragCoord_1;
        let _e404 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e402.xy / vec2<f32>(vec2<i32>(_e404)));
        let _e408 = gtaoUV;
        let _e409 = textureSample(gtaoMap, gtaoMap_sampler, _e408);
        gtao_vis = _e409.x;
        let _e411 = gtao_vis;
        let _e412 = specularIBL;
        specularIBL = (_e412 * _e411);
        let _e414 = specularIBL;
        let _e415 = base;
        let _e417 = (_e415.xyz + _e414);
        base[0u] = _e417.x;
        base[1u] = _e417.y;
        base[2u] = _e417.z;
    }
    let _e424 = wired_advanced_fog_enabled_u0028_();
    if _e424 {
        let _e425 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e425;
        let _e426 = base;
        let _e429 = unnamed.advancedFogColorDensity;
        let _e431 = fogAmount;
        let _e433 = mix(_e426.xyz, _e429.xyz, vec3(_e431));
        base[0u] = _e433.x;
        base[1u] = _e433.y;
        base[2u] = _e433.z;
    }
    if override_type_3_6 {
        let _e441 = base[3u];
        if (_e441 == 0f) {
            discard;
        }
    } else {
        if override_type_3_7 {
            let _e443 = base;
            let _e445 = base;
            if (dot(_e443.xyz, _e445.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e449 = base;
    out_color = _e449;
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
