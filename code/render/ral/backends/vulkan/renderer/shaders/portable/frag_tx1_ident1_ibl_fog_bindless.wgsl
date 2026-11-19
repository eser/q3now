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

    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e93 + 0.5f));
    let _e98 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e100 = fogType;
    let _e103 = fogType;
    return (((_e98 > 0.5f) && (_e100 >= 1i)) && (_e103 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e93 = wired_advanced_fog_enabled_u0028_();
    if !(_e93) {
        return 0f;
    }
    let _e96 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e96, 0.000001f));
    let _e101 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e101 + 0.5f));
    let _e104 = fogType_1;
    if (_e104 == 1i) {
        let _e108 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e108 <= 0f) {
            return 0f;
        }
        let _e110 = viewDepth;
        let _e113 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e110 / _e113), 0f, 1f);
    }
    let _e118 = unnamed.advancedFogColorDensity[3u];
    let _e120 = viewDepth;
    opticalDepth = (max(_e118, 0f) * _e120);
    let _e122 = fogType_1;
    if (_e122 == 2i) {
        let _e124 = opticalDepth;
        return clamp((1f - exp(-(_e124))), 0f, 1f);
    }
    let _e129 = opticalDepth;
    let _e130 = opticalDepth;
    return clamp((1f - exp(-((_e129 * _e130)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e96 = (*cosTheta);
    t = (1f - _e96);
    let _e98 = t;
    let _e99 = t;
    t2_ = (_e98 * _e99);
    let _e101 = (*roughness);
    let _e104 = (*F0_);
    Fmax = max(vec3((1f - _e101)), _e104);
    let _e106 = (*F0_);
    let _e107 = Fmax;
    let _e108 = (*F0_);
    let _e110 = t2_;
    let _e111 = t2_;
    let _e113 = t;
    return (_e106 + ((_e107 - _e108) * ((_e110 * _e111) * _e113)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e93 = (*rgb);
    let _e96 = unnamed.worldLightParams[0u];
    boosted = (_e93 * _e96);
    let _e99 = boosted[0u];
    let _e101 = boosted[1u];
    let _e103 = boosted[2u];
    peak = max(_e99, max(_e101, _e103));
    let _e106 = peak;
    if (_e106 > 1f) {
        let _e108 = peak;
        let _e109 = boosted;
        boosted = (_e109 / vec3(_e108));
    }
    let _e112 = boosted;
    return _e112;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e94 = (*c);
    (*c) = max(_e94, vec3<f32>(0f, 0f, 0f));
    let _e96 = (*c);
    cutoff = (_e96 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e98 = (*c);
    lo = (_e98 / vec3(12.92f));
    let _e101 = (*c);
    hi = pow(((_e101 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e106 = hi;
    let _e107 = lo;
    let _e108 = cutoff;
    return mix(_e106, _e107, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e108));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e106 = (*role);
    let _e108 = (*role);
    let _e113 = unnamed.packed_indices[(_e106 / 4u)][(_e108 % 4u)];
    let _e118 = (*uv);
    let _e119 = textureSample(wired_bindless_images[(_e103 & 4095u)], wired_bindless_samplers[((_e113 >> bitcast<u32>(12i)) & 255u)], _e118);
    c_1 = _e119;
    let _e120 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e120))) == 0i) {
        let _e125 = c_1;
        param = _e125.xyz;
        let _e127 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e127.x;
        c_1[1u] = _e127.y;
        c_1[2u] = _e127.z;
    }
    let _e134 = (*slot);
    if (lightmap_slot == (_e134 + 1i)) {
        let _e137 = c_1;
        param_1 = _e137.xyz;
        let _e139 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e139.x;
        c_1[1u] = _e139.y;
        c_1[2u] = _e139.z;
    }
    let _e146 = c_1;
    return _e146;
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

    let _e137 = unnamed.packed_indices[0i][3u];
    let _e143 = unnamed.packed_indices[0i][3u];
    let _e148 = fog_tex_coord_1;
    let _e149 = textureSample(wired_bindless_images[(_e137 & 4095u)], wired_bindless_samplers[((_e143 >> bitcast<u32>(12i)) & 255u)], _e148);
    fog = _e149;
    param_2 = 0u;
    let _e150 = frag_tex_coord0_1;
    param_3 = _e150;
    param_4 = 0i;
    let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e151;
    param_5 = 1u;
    let _e152 = frag_tex_coord1_1;
    param_6 = _e152;
    param_7 = 1i;
    let _e153 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e153;
    if override_type_3_ {
        let _e154 = sampledColor1_;
        local = _e154.xyz;
    } else {
        let _e156 = sampledColor0_;
        local = _e156.xyz;
    }
    let _e158 = local;
    materialAlbedo = _e158;
    let _e159 = sampledColor0_;
    color0_ = _e159;
    if override_type_3_1 {
        let _e160 = sampledColor1_;
        color1_ = _e160;
        let _e161 = color0_;
        let _e163 = color1_;
        let _e165 = (_e161.xyz + _e163.xyz);
        let _e167 = color0_[3u];
        let _e169 = color1_[3u];
        base = vec4<f32>(_e165.x, _e165.y, _e165.z, (_e167 * _e169));
    } else {
        if override_type_3_2 {
            let _e175 = sampledColor1_;
            color1_1 = _e175;
            let _e176 = color0_;
            let _e178 = color1_1;
            let _e180 = (_e176.xyz + _e178.xyz);
            let _e182 = color0_[3u];
            let _e184 = color1_1[3u];
            base = vec4<f32>(_e180.x, _e180.y, _e180.z, (_e182 * _e184));
        } else {
            let _e190 = sampledColor1_;
            color1_2 = _e190;
            let _e191 = color0_;
            let _e193 = color1_2;
            let _e195 = (_e191.xyz * _e193.xyz);
            base[0u] = _e195.x;
            base[1u] = _e195.y;
            base[2u] = _e195.z;
            let _e203 = color0_[3u];
            let _e205 = color1_2[3u];
            base[3u] = (_e203 * _e205);
        }
    }
    if override_type_3_3 {
        let _e210 = unnamed.worldLightParams[1u];
        wetness = clamp(_e210, 0f, 1f);
        let _e214 = unnamed.worldLightParams[2u];
        frost = clamp(_e214, 0f, 1f);
        let _e216 = base;
        luminance = dot(_e216.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e219 = wetness;
        let _e221 = base;
        let _e223 = (_e221.xyz * mix(1f, 0.82f, _e219));
        base[0u] = _e223.x;
        base[1u] = _e223.y;
        base[2u] = _e223.z;
        let _e230 = base;
        let _e232 = luminance;
        let _e234 = luminance;
        let _e236 = luminance;
        let _e238 = frost;
        let _e241 = mix(_e230.xyz, vec3<f32>((_e232 * 0.88f), (_e234 * 0.94f), _e236), vec3((_e238 * 0.55f)));
        base[0u] = _e241.x;
        base[1u] = _e241.y;
        base[2u] = _e241.z;
        let _e248 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e248).z);
        let _e254 = unnamed.worldLightParams[3u];
        let _e256 = upward;
        snowCoverage = (clamp(_e254, 0f, 1f) * _e256);
        let _e258 = base;
        let _e260 = snowCoverage;
        let _e263 = mix(_e258.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e260 * 0.82f)));
        base[0u] = _e263.x;
        base[1u] = _e263.y;
        base[2u] = _e263.z;
    }
    let _e272 = unnamed.materialFactors[0u];
    if (_e272 >= 0f) {
        let _e275 = unnamed.materialBaseColor;
        let _e276 = base;
        base = (_e276 * _e275);
    }
    let _e278 = color0_;
    let _e281 = unnamed.emissionRadiance;
    let _e284 = base;
    let _e286 = (_e284.xyz + (_e278.xyz * _e281.xyz));
    base[0u] = _e286.x;
    base[1u] = _e286.y;
    base[2u] = _e286.z;
    if override_type_3_4 {
        if override_type_3_5 {
            let _e296 = unnamed.packed_indices[2i][0u];
            let _e302 = unnamed.packed_indices[2i][0u];
            let _e307 = frag_tex_coord0_1;
            let _e308 = textureSample(wired_bindless_images[(_e296 & 4095u)], wired_bindless_samplers[((_e302 >> bitcast<u32>(12i)) & 255u)], _e307);
            local_1 = _e308.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e310 = local_1;
        orm = _e310;
        let _e312 = orm[0u];
        let _e315 = unnamed.materialFactors[2u];
        ao = mix(1f, _e312, clamp(_e315, 0f, 1f));
        let _e319 = orm[1u];
        let _e322 = unnamed.materialFactors[1u];
        let _e326 = unnamed.worldLightParams[1u];
        let _e331 = unnamed.worldLightParams[2u];
        let _e336 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e319 * _e322) - (0.35f * _e326)) + (0.3f * _e331)) + (0.4f * _e336)), 0.04f, 1f);
        let _e341 = orm[2u];
        let _e344 = unnamed.materialFactors[0u];
        metalness = clamp((_e341 * _e344), 0f, 1f);
        let _e347 = ibl_N_1;
        ibl_n = normalize(_e347);
        let _e349 = ibl_V_1;
        ibl_v = normalize(_e349);
        let _e351 = materialAlbedo;
        let _e353 = unnamed.materialBaseColor;
        let _e356 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e351 * _e353.xyz), vec3(_e356));
        let _e359 = ibl_n;
        let _e360 = ibl_v;
        NdotV = max(dot(_e359, _e360), 0f);
        let _e363 = NdotV;
        param_8 = _e363;
        let _e364 = F0_1;
        param_9 = _e364;
        let _e365 = roughness_1;
        param_10 = _e365;
        let _e366 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_8), (&param_9), (&param_10));
        F_amb = _e366;
        let _e367 = ibl_v;
        let _e369 = ibl_n;
        R = reflect(-(_e367), _e369);
        let _e373 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e373, 0f, 1f);
        let _e375 = R;
        let _e376 = roughness_1;
        let _e377 = textureNumLevels(radianceCube);
        let _e382 = textureSampleLevel(radianceCube, radianceCube_sampler, _e375, (_e376 * f32((i32(_e377) - 1i))));
        currentPrefiltered = _e382.xyz;
        let _e384 = R;
        let _e385 = roughness_1;
        let _e386 = textureNumLevels(previousRadianceCube);
        let _e391 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e384, (_e385 * f32((i32(_e386) - 1i))));
        previousPrefiltered = _e391.xyz;
        let _e393 = previousPrefiltered;
        let _e394 = currentPrefiltered;
        let _e395 = environmentBlend;
        prefiltered = mix(_e393, _e394, vec3(_e395));
        let _e398 = NdotV;
        let _e399 = roughness_1;
        let _e401 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e398, _e399), 0f);
        envBRDF = _e401.xy;
        let _e403 = prefiltered;
        let _e404 = F_amb;
        let _e406 = envBRDF[0u];
        let _e409 = envBRDF[1u];
        let _e413 = ao;
        specularIBL = ((_e403 * ((_e404 * _e406) + vec3(_e409))) * _e413);
        let _e415 = gl_FragCoord_1;
        let _e417 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e415.xy / vec2<f32>(vec2<i32>(_e417)));
        let _e421 = gtaoUV;
        let _e422 = textureSample(gtaoMap, gtaoMap_sampler, _e421);
        gtao_vis = _e422.x;
        let _e424 = gtao_vis;
        let _e425 = specularIBL;
        specularIBL = (_e425 * _e424);
        let _e427 = specularIBL;
        let _e428 = base;
        let _e430 = (_e428.xyz + _e427);
        base[0u] = _e430.x;
        base[1u] = _e430.y;
        base[2u] = _e430.z;
    }
    let _e437 = wired_advanced_fog_enabled_u0028_();
    if _e437 {
        let _e438 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e438;
        if override_type_3_6 {
            let _e439 = fogAmount;
            let _e441 = base;
            let _e443 = (_e441.xyz * (1f - _e439));
            base[0u] = _e443.x;
            base[1u] = _e443.y;
            base[2u] = _e443.z;
        } else {
            if override_type_3_7 {
                let _e450 = fogAmount;
                let _e452 = base;
                base = (_e452 * (1f - _e450));
            } else {
                if override_type_3_8 {
                    let _e454 = fogAmount;
                    let _e457 = base[3u];
                    base[3u] = (_e457 * (1f - _e454));
                } else {
                    let _e460 = base;
                    let _e463 = unnamed.advancedFogColorDensity;
                    let _e465 = fogAmount;
                    let _e467 = mix(_e460.xyz, _e463.xyz, vec3(_e465));
                    base[0u] = _e467.x;
                    base[1u] = _e467.y;
                    base[2u] = _e467.z;
                }
            }
        }
    } else {
        if override_type_3_9 {
            let _e474 = base;
            let _e477 = fog[3u];
            let _e479 = (_e474.xyz * (1f - _e477));
            base[0u] = _e479.x;
            base[1u] = _e479.y;
            base[2u] = _e479.z;
        } else {
            if override_type_3_10 {
                let _e486 = base;
                let _e488 = fog[3u];
                base = (_e486 * (1f - _e488));
            } else {
                if override_type_3_11 {
                    let _e492 = base[3u];
                    let _e494 = fog[3u];
                    base[3u] = (_e492 * (1f - _e494));
                } else {
                    let _e498 = base;
                    let _e499 = fog;
                    let _e501 = unnamed.fogColor;
                    let _e504 = fog[3u];
                    base = mix(_e498, (_e499 * _e501), vec4(_e504));
                }
            }
        }
    }
    if override_type_3_12 {
        let _e508 = base[3u];
        if (_e508 == 0f) {
            discard;
        }
    } else {
        if override_type_3_13 {
            let _e510 = base;
            let _e512 = base;
            if (dot(_e510.xyz, _e512.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e516 = base;
    out_color = _e516;
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
