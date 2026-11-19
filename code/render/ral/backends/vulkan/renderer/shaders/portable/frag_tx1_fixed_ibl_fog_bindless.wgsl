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

    let _e140 = unnamed.packed_indices[0i][3u];
    let _e146 = unnamed.packed_indices[0i][3u];
    let _e151 = fog_tex_coord_1;
    let _e152 = textureSample(wired_bindless_images[(_e140 & 4095u)], wired_bindless_samplers[((_e146 >> bitcast<u32>(12i)) & 255u)], _e151);
    fog = _e152;
    param_2 = 0u;
    let _e153 = frag_tex_coord0_1;
    param_3 = _e153;
    param_4 = 0i;
    let _e154 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e154;
    param_5 = 1u;
    let _e155 = frag_tex_coord1_1;
    param_6 = _e155;
    param_7 = 1i;
    let _e156 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e156;
    if override_type_3_ {
        let _e157 = sampledColor1_;
        local = _e157.xyz;
    } else {
        let _e159 = sampledColor0_;
        local = _e159.xyz;
    }
    let _e161 = local;
    materialAlbedo = _e161;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e166 = sampledColor0_;
    let _e167 = frag_color;
    color0_ = (_e166 * _e167);
    if override_type_3_1 {
        let _e169 = sampledColor1_;
        color1_ = _e169;
        let _e170 = color0_;
        let _e172 = color1_;
        let _e174 = (_e170.xyz + _e172.xyz);
        let _e176 = color0_[3u];
        let _e178 = color1_[3u];
        base = vec4<f32>(_e174.x, _e174.y, _e174.z, (_e176 * _e178));
    } else {
        if override_type_3_2 {
            let _e184 = sampledColor1_;
            let _e185 = frag_color;
            color1_1 = (_e184 * _e185);
            let _e187 = color0_;
            let _e189 = color1_1;
            let _e191 = (_e187.xyz + _e189.xyz);
            let _e193 = color0_[3u];
            let _e195 = color1_1[3u];
            base = vec4<f32>(_e191.x, _e191.y, _e191.z, (_e193 * _e195));
        } else {
            let _e201 = sampledColor1_;
            let _e202 = frag_color;
            color1_2 = (_e201 * _e202);
            let _e204 = color0_;
            let _e206 = color1_2;
            let _e208 = (_e204.xyz * _e206.xyz);
            base[0u] = _e208.x;
            base[1u] = _e208.y;
            base[2u] = _e208.z;
            let _e216 = color0_[3u];
            let _e218 = color1_2[3u];
            base[3u] = (_e216 * _e218);
        }
    }
    if override_type_3_3 {
        let _e223 = unnamed.worldLightParams[1u];
        wetness = clamp(_e223, 0f, 1f);
        let _e227 = unnamed.worldLightParams[2u];
        frost = clamp(_e227, 0f, 1f);
        let _e229 = base;
        luminance = dot(_e229.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e232 = wetness;
        let _e234 = base;
        let _e236 = (_e234.xyz * mix(1f, 0.82f, _e232));
        base[0u] = _e236.x;
        base[1u] = _e236.y;
        base[2u] = _e236.z;
        let _e243 = base;
        let _e245 = luminance;
        let _e247 = luminance;
        let _e249 = luminance;
        let _e251 = frost;
        let _e254 = mix(_e243.xyz, vec3<f32>((_e245 * 0.88f), (_e247 * 0.94f), _e249), vec3((_e251 * 0.55f)));
        base[0u] = _e254.x;
        base[1u] = _e254.y;
        base[2u] = _e254.z;
        let _e261 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e261).z);
        let _e267 = unnamed.worldLightParams[3u];
        let _e269 = upward;
        snowCoverage = (clamp(_e267, 0f, 1f) * _e269);
        let _e271 = base;
        let _e273 = snowCoverage;
        let _e276 = mix(_e271.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e273 * 0.82f)));
        base[0u] = _e276.x;
        base[1u] = _e276.y;
        base[2u] = _e276.z;
    }
    let _e285 = unnamed.materialFactors[0u];
    if (_e285 >= 0f) {
        let _e288 = unnamed.materialBaseColor;
        let _e289 = base;
        base = (_e289 * _e288);
    }
    let _e291 = color0_;
    let _e294 = unnamed.emissionRadiance;
    let _e297 = base;
    let _e299 = (_e297.xyz + (_e291.xyz * _e294.xyz));
    base[0u] = _e299.x;
    base[1u] = _e299.y;
    base[2u] = _e299.z;
    if override_type_3_4 {
        if override_type_3_5 {
            let _e309 = unnamed.packed_indices[2i][0u];
            let _e315 = unnamed.packed_indices[2i][0u];
            let _e320 = frag_tex_coord0_1;
            let _e321 = textureSample(wired_bindless_images[(_e309 & 4095u)], wired_bindless_samplers[((_e315 >> bitcast<u32>(12i)) & 255u)], _e320);
            local_1 = _e321.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e323 = local_1;
        orm = _e323;
        let _e325 = orm[0u];
        let _e328 = unnamed.materialFactors[2u];
        ao = mix(1f, _e325, clamp(_e328, 0f, 1f));
        let _e332 = orm[1u];
        let _e335 = unnamed.materialFactors[1u];
        let _e339 = unnamed.worldLightParams[1u];
        let _e344 = unnamed.worldLightParams[2u];
        let _e349 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e332 * _e335) - (0.35f * _e339)) + (0.3f * _e344)) + (0.4f * _e349)), 0.04f, 1f);
        let _e354 = orm[2u];
        let _e357 = unnamed.materialFactors[0u];
        metalness = clamp((_e354 * _e357), 0f, 1f);
        let _e360 = ibl_N_1;
        ibl_n = normalize(_e360);
        let _e362 = ibl_V_1;
        ibl_v = normalize(_e362);
        let _e364 = materialAlbedo;
        let _e366 = unnamed.materialBaseColor;
        let _e369 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e364 * _e366.xyz), vec3(_e369));
        let _e372 = ibl_n;
        let _e373 = ibl_v;
        NdotV = max(dot(_e372, _e373), 0f);
        let _e376 = NdotV;
        param_8 = _e376;
        let _e377 = F0_1;
        param_9 = _e377;
        let _e378 = roughness_1;
        param_10 = _e378;
        let _e379 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_8), (&param_9), (&param_10));
        F_amb = _e379;
        let _e380 = ibl_v;
        let _e382 = ibl_n;
        R = reflect(-(_e380), _e382);
        let _e386 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e386, 0f, 1f);
        let _e388 = R;
        let _e389 = roughness_1;
        let _e390 = textureNumLevels(radianceCube);
        let _e395 = textureSampleLevel(radianceCube, radianceCube_sampler, _e388, (_e389 * f32((i32(_e390) - 1i))));
        currentPrefiltered = _e395.xyz;
        let _e397 = R;
        let _e398 = roughness_1;
        let _e399 = textureNumLevels(previousRadianceCube);
        let _e404 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e397, (_e398 * f32((i32(_e399) - 1i))));
        previousPrefiltered = _e404.xyz;
        let _e406 = previousPrefiltered;
        let _e407 = currentPrefiltered;
        let _e408 = environmentBlend;
        prefiltered = mix(_e406, _e407, vec3(_e408));
        let _e411 = NdotV;
        let _e412 = roughness_1;
        let _e414 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e411, _e412), 0f);
        envBRDF = _e414.xy;
        let _e416 = prefiltered;
        let _e417 = F_amb;
        let _e419 = envBRDF[0u];
        let _e422 = envBRDF[1u];
        let _e426 = ao;
        specularIBL = ((_e416 * ((_e417 * _e419) + vec3(_e422))) * _e426);
        let _e428 = gl_FragCoord_1;
        let _e430 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e428.xy / vec2<f32>(vec2<i32>(_e430)));
        let _e434 = gtaoUV;
        let _e435 = textureSample(gtaoMap, gtaoMap_sampler, _e434);
        gtao_vis = _e435.x;
        let _e437 = gtao_vis;
        let _e438 = specularIBL;
        specularIBL = (_e438 * _e437);
        let _e440 = specularIBL;
        let _e441 = base;
        let _e443 = (_e441.xyz + _e440);
        base[0u] = _e443.x;
        base[1u] = _e443.y;
        base[2u] = _e443.z;
    }
    let _e450 = wired_advanced_fog_enabled_u0028_();
    if _e450 {
        let _e451 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e451;
        if override_type_3_6 {
            let _e452 = fogAmount;
            let _e454 = base;
            let _e456 = (_e454.xyz * (1f - _e452));
            base[0u] = _e456.x;
            base[1u] = _e456.y;
            base[2u] = _e456.z;
        } else {
            if override_type_3_7 {
                let _e463 = fogAmount;
                let _e465 = base;
                base = (_e465 * (1f - _e463));
            } else {
                if override_type_3_8 {
                    let _e467 = fogAmount;
                    let _e470 = base[3u];
                    base[3u] = (_e470 * (1f - _e467));
                } else {
                    let _e473 = base;
                    let _e476 = unnamed.advancedFogColorDensity;
                    let _e478 = fogAmount;
                    let _e480 = mix(_e473.xyz, _e476.xyz, vec3(_e478));
                    base[0u] = _e480.x;
                    base[1u] = _e480.y;
                    base[2u] = _e480.z;
                }
            }
        }
    } else {
        if override_type_3_9 {
            let _e487 = base;
            let _e490 = fog[3u];
            let _e492 = (_e487.xyz * (1f - _e490));
            base[0u] = _e492.x;
            base[1u] = _e492.y;
            base[2u] = _e492.z;
        } else {
            if override_type_3_10 {
                let _e499 = base;
                let _e501 = fog[3u];
                base = (_e499 * (1f - _e501));
            } else {
                if override_type_3_11 {
                    let _e505 = base[3u];
                    let _e507 = fog[3u];
                    base[3u] = (_e505 * (1f - _e507));
                } else {
                    let _e511 = base;
                    let _e512 = fog;
                    let _e514 = unnamed.fogColor;
                    let _e517 = fog[3u];
                    base = mix(_e511, (_e512 * _e514), vec4(_e517));
                }
            }
        }
    }
    if override_type_3_12 {
        let _e521 = base[3u];
        if (_e521 == 0f) {
            discard;
        }
    } else {
        if override_type_3_13 {
            let _e523 = base;
            let _e525 = base;
            if (dot(_e523.xyz, _e525.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e529 = base;
    out_color = _e529;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e13 = out_color;
    return _e13;
}
