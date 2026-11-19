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
override override_type_3_3: bool = (override_type_3_1 || override_type_3_2);
override override_type_3_4: bool = (tex_mode == 3i);
override override_type_3_5: bool = (tex_mode == 4i);
override override_type_3_6: bool = (tex_mode == 5i);
override override_type_3_7: bool = (tex_mode == 6i);
override override_type_3_8: bool = (tex_mode == 7i);
override override_type_3_9: bool = (lightmap_slot != 0i);
@id(14) override ibl_enabled: i32 = 0i;
override override_type_3_10: bool = (ibl_enabled != 0i);
override override_type_3_11: bool = (ibl_enabled == 1i);
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

    let _e96 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e96 + 0.5f));
    let _e101 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e103 = fogType;
    let _e106 = fogType;
    return (((_e101 > 0.5f) && (_e103 >= 1i)) && (_e106 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e96 = wired_advanced_fog_enabled_u0028_();
    if !(_e96) {
        return 0f;
    }
    let _e99 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e99, 0.000001f));
    let _e104 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e104 + 0.5f));
    let _e107 = fogType_1;
    if (_e107 == 1i) {
        let _e111 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e111 <= 0f) {
            return 0f;
        }
        let _e113 = viewDepth;
        let _e116 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e113 / _e116), 0f, 1f);
    }
    let _e121 = unnamed.advancedFogColorDensity[3u];
    let _e123 = viewDepth;
    opticalDepth = (max(_e121, 0f) * _e123);
    let _e125 = fogType_1;
    if (_e125 == 2i) {
        let _e127 = opticalDepth;
        return clamp((1f - exp(-(_e127))), 0f, 1f);
    }
    let _e132 = opticalDepth;
    let _e133 = opticalDepth;
    return clamp((1f - exp(-((_e132 * _e133)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e99 = (*cosTheta);
    t = (1f - _e99);
    let _e101 = t;
    let _e102 = t;
    t2_ = (_e101 * _e102);
    let _e104 = (*roughness);
    let _e107 = (*F0_);
    Fmax = max(vec3((1f - _e104)), _e107);
    let _e109 = (*F0_);
    let _e110 = Fmax;
    let _e111 = (*F0_);
    let _e113 = t2_;
    let _e114 = t2_;
    let _e116 = t;
    return (_e109 + ((_e110 - _e111) * ((_e113 * _e114) * _e116)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e96 = (*rgb);
    let _e99 = unnamed.worldLightParams[0u];
    boosted = (_e96 * _e99);
    let _e102 = boosted[0u];
    let _e104 = boosted[1u];
    let _e106 = boosted[2u];
    peak = max(_e102, max(_e104, _e106));
    let _e109 = peak;
    if (_e109 > 1f) {
        let _e111 = peak;
        let _e112 = boosted;
        boosted = (_e112 / vec3(_e111));
    }
    let _e115 = boosted;
    return _e115;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e97 = (*c);
    (*c) = max(_e97, vec3<f32>(0f, 0f, 0f));
    let _e99 = (*c);
    cutoff = (_e99 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e101 = (*c);
    lo = (_e101 / vec3(12.92f));
    let _e104 = (*c);
    hi = pow(((_e104 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e109 = hi;
    let _e110 = lo;
    let _e111 = cutoff;
    return mix(_e109, _e110, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e111));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e109 = (*role);
    let _e111 = (*role);
    let _e116 = unnamed.packed_indices[(_e109 / 4u)][(_e111 % 4u)];
    let _e121 = (*uv);
    let _e122 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e116 >> bitcast<u32>(12i)) & 255u)], _e121);
    c_1 = _e122;
    let _e123 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e123))) == 0i) {
        let _e128 = c_1;
        param = _e128.xyz;
        let _e130 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e130.x;
        c_1[1u] = _e130.y;
        c_1[2u] = _e130.z;
    }
    let _e137 = (*slot);
    if (lightmap_slot == (_e137 + 1i)) {
        let _e140 = c_1;
        param_1 = _e140.xyz;
        let _e142 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e142.x;
        c_1[1u] = _e142.y;
        c_1[2u] = _e142.z;
    }
    let _e149 = c_1;
    return _e149;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var sampledColor1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var materialAlbedo: vec3<f32>;
    var local: vec3<f32>;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
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
    var param_10: f32;
    var param_11: vec3<f32>;
    var param_12: f32;
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

    let _e144 = frag_color0In_1;
    param_2 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e148 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e146.x, _e146.y, _e146.z, _e148);
    let _e153 = frag_color1In_1;
    param_3 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e157 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e155.x, _e155.y, _e155.z, _e157);
    param_4 = 0u;
    let _e162 = frag_tex_coord0_1;
    param_5 = _e162;
    param_6 = 0i;
    let _e163 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    sampledColor0_ = _e163;
    param_7 = 1u;
    let _e164 = frag_tex_coord1_1;
    param_8 = _e164;
    param_9 = 1i;
    let _e165 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
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
    if override_type_3_3 {
        let _e174 = sampledColor1_;
        let _e175 = frag_color1_;
        color1_ = (_e174 * _e175);
        let _e177 = color0_;
        let _e179 = color1_;
        let _e181 = (_e177.xyz + _e179.xyz);
        let _e183 = color0_[3u];
        let _e185 = color1_[3u];
        base = vec4<f32>(_e181.x, _e181.y, _e181.z, (_e183 * _e185));
    } else {
        if override_type_3_4 {
            let _e191 = sampledColor1_;
            let _e192 = frag_color1_;
            color1_1 = (_e191 * _e192);
            let _e195 = color0_[3u];
            let _e196 = color0_;
            color0_ = (_e196 * _e195);
            let _e199 = color1_1[3u];
            let _e200 = color1_1;
            color1_1 = (_e200 * _e199);
            let _e202 = color0_;
            let _e204 = color1_1;
            let _e206 = (_e202.xyz + _e204.xyz);
            let _e208 = color0_[3u];
            let _e210 = color1_1[3u];
            base = vec4<f32>(_e206.x, _e206.y, _e206.z, (_e208 * _e210));
        } else {
            if override_type_3_5 {
                let _e216 = sampledColor1_;
                let _e217 = frag_color1_;
                color1_2 = (_e216 * _e217);
                let _e220 = color0_[3u];
                let _e222 = color0_;
                color0_ = (_e222 * (1f - _e220));
                let _e225 = color1_2[3u];
                let _e227 = color1_2;
                color1_2 = (_e227 * (1f - _e225));
                let _e229 = color0_;
                let _e231 = color1_2;
                let _e233 = (_e229.xyz + _e231.xyz);
                let _e235 = color0_[3u];
                let _e237 = color1_2[3u];
                base = vec4<f32>(_e233.x, _e233.y, _e233.z, (_e235 * _e237));
            } else {
                if override_type_3_6 {
                    let _e243 = sampledColor1_;
                    let _e244 = frag_color1_;
                    color1_3 = (_e243 * _e244);
                    let _e246 = color0_;
                    let _e247 = color1_3;
                    let _e249 = color1_3[3u];
                    base = mix(_e246, _e247, vec4(_e249));
                } else {
                    if override_type_3_7 {
                        let _e252 = sampledColor1_;
                        let _e253 = frag_color1_;
                        color1_4 = (_e252 * _e253);
                        let _e255 = color1_4;
                        let _e256 = color0_;
                        let _e258 = color1_4[3u];
                        base = mix(_e255, _e256, vec4(_e258));
                    } else {
                        if override_type_3_8 {
                            let _e261 = sampledColor1_;
                            let _e262 = frag_color1_;
                            color1_5 = (_e261 * _e262);
                            let _e264 = color1_5;
                            let _e266 = color1_5[3u];
                            let _e269 = color0_;
                            base = ((_e264 + vec4(_e266)) * _e269);
                        } else {
                            let _e271 = sampledColor1_;
                            let _e272 = frag_color1_;
                            color1_6 = (_e271 * _e272);
                            let _e274 = color0_;
                            let _e276 = color1_6;
                            let _e278 = (_e274.xyz * _e276.xyz);
                            base[0u] = _e278.x;
                            base[1u] = _e278.y;
                            base[2u] = _e278.z;
                            let _e286 = color0_[3u];
                            let _e288 = color1_6[3u];
                            base[3u] = (_e286 * _e288);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_9 {
        let _e293 = unnamed.worldLightParams[1u];
        wetness = clamp(_e293, 0f, 1f);
        let _e297 = unnamed.worldLightParams[2u];
        frost = clamp(_e297, 0f, 1f);
        let _e299 = base;
        luminance = dot(_e299.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e302 = wetness;
        let _e304 = base;
        let _e306 = (_e304.xyz * mix(1f, 0.82f, _e302));
        base[0u] = _e306.x;
        base[1u] = _e306.y;
        base[2u] = _e306.z;
        let _e313 = base;
        let _e315 = luminance;
        let _e317 = luminance;
        let _e319 = luminance;
        let _e321 = frost;
        let _e324 = mix(_e313.xyz, vec3<f32>((_e315 * 0.88f), (_e317 * 0.94f), _e319), vec3((_e321 * 0.55f)));
        base[0u] = _e324.x;
        base[1u] = _e324.y;
        base[2u] = _e324.z;
        let _e331 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e331).z);
        let _e337 = unnamed.worldLightParams[3u];
        let _e339 = upward;
        snowCoverage = (clamp(_e337, 0f, 1f) * _e339);
        let _e341 = base;
        let _e343 = snowCoverage;
        let _e346 = mix(_e341.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e343 * 0.82f)));
        base[0u] = _e346.x;
        base[1u] = _e346.y;
        base[2u] = _e346.z;
    }
    let _e355 = unnamed.materialFactors[0u];
    if (_e355 >= 0f) {
        let _e358 = unnamed.materialBaseColor;
        let _e359 = base;
        base = (_e359 * _e358);
    }
    let _e361 = color0_;
    let _e364 = unnamed.emissionRadiance;
    let _e367 = base;
    let _e369 = (_e367.xyz + (_e361.xyz * _e364.xyz));
    base[0u] = _e369.x;
    base[1u] = _e369.y;
    base[2u] = _e369.z;
    if override_type_3_10 {
        if override_type_3_11 {
            let _e379 = unnamed.packed_indices[2i][0u];
            let _e385 = unnamed.packed_indices[2i][0u];
            let _e390 = frag_tex_coord0_1;
            let _e391 = textureSample(wired_bindless_images[(_e379 & 4095u)], wired_bindless_samplers[((_e385 >> bitcast<u32>(12i)) & 255u)], _e390);
            local_1 = _e391.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e393 = local_1;
        orm = _e393;
        let _e395 = orm[0u];
        let _e398 = unnamed.materialFactors[2u];
        ao = mix(1f, _e395, clamp(_e398, 0f, 1f));
        let _e402 = orm[1u];
        let _e405 = unnamed.materialFactors[1u];
        let _e409 = unnamed.worldLightParams[1u];
        let _e414 = unnamed.worldLightParams[2u];
        let _e419 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e402 * _e405) - (0.35f * _e409)) + (0.3f * _e414)) + (0.4f * _e419)), 0.04f, 1f);
        let _e424 = orm[2u];
        let _e427 = unnamed.materialFactors[0u];
        metalness = clamp((_e424 * _e427), 0f, 1f);
        let _e430 = ibl_N_1;
        ibl_n = normalize(_e430);
        let _e432 = ibl_V_1;
        ibl_v = normalize(_e432);
        let _e434 = materialAlbedo;
        let _e436 = unnamed.materialBaseColor;
        let _e439 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e434 * _e436.xyz), vec3(_e439));
        let _e442 = ibl_n;
        let _e443 = ibl_v;
        NdotV = max(dot(_e442, _e443), 0f);
        let _e446 = NdotV;
        param_10 = _e446;
        let _e447 = F0_1;
        param_11 = _e447;
        let _e448 = roughness_1;
        param_12 = _e448;
        let _e449 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_10), (&param_11), (&param_12));
        F_amb = _e449;
        let _e450 = ibl_v;
        let _e452 = ibl_n;
        R = reflect(-(_e450), _e452);
        let _e456 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e456, 0f, 1f);
        let _e458 = R;
        let _e459 = roughness_1;
        let _e460 = textureNumLevels(radianceCube);
        let _e465 = textureSampleLevel(radianceCube, radianceCube_sampler, _e458, (_e459 * f32((i32(_e460) - 1i))));
        currentPrefiltered = _e465.xyz;
        let _e467 = R;
        let _e468 = roughness_1;
        let _e469 = textureNumLevels(previousRadianceCube);
        let _e474 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e467, (_e468 * f32((i32(_e469) - 1i))));
        previousPrefiltered = _e474.xyz;
        let _e476 = previousPrefiltered;
        let _e477 = currentPrefiltered;
        let _e478 = environmentBlend;
        prefiltered = mix(_e476, _e477, vec3(_e478));
        let _e481 = NdotV;
        let _e482 = roughness_1;
        let _e484 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e481, _e482), 0f);
        envBRDF = _e484.xy;
        let _e486 = prefiltered;
        let _e487 = F_amb;
        let _e489 = envBRDF[0u];
        let _e492 = envBRDF[1u];
        let _e496 = ao;
        specularIBL = ((_e486 * ((_e487 * _e489) + vec3(_e492))) * _e496);
        let _e498 = gl_FragCoord_1;
        let _e500 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e498.xy / vec2<f32>(vec2<i32>(_e500)));
        let _e504 = gtaoUV;
        let _e505 = textureSample(gtaoMap, gtaoMap_sampler, _e504);
        gtao_vis = _e505.x;
        let _e507 = gtao_vis;
        let _e508 = specularIBL;
        specularIBL = (_e508 * _e507);
        let _e510 = specularIBL;
        let _e511 = base;
        let _e513 = (_e511.xyz + _e510);
        base[0u] = _e513.x;
        base[1u] = _e513.y;
        base[2u] = _e513.z;
    }
    let _e520 = wired_advanced_fog_enabled_u0028_();
    if _e520 {
        let _e521 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e521;
        let _e522 = base;
        let _e525 = unnamed.advancedFogColorDensity;
        let _e527 = fogAmount;
        let _e529 = mix(_e522.xyz, _e525.xyz, vec3(_e527));
        base[0u] = _e529.x;
        base[1u] = _e529.y;
        base[2u] = _e529.z;
    }
    if override_type_3_12 {
        let _e537 = base[3u];
        if (_e537 == 0f) {
            discard;
        }
    } else {
        if override_type_3_13 {
            let _e539 = base;
            let _e541 = base;
            if (dot(_e539.xyz, _e541.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e545 = base;
    out_color = _e545;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e15 = out_color;
    return _e15;
}
