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
@id(10) override acff: i32 = 0i;
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
override override_type_3_15: bool = (acff == 1i);
override override_type_3_16: bool = (acff == 2i);
override override_type_3_17: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_18: bool = (discard_mode == 1i);
override override_type_3_19: bool = (discard_mode == 2i);
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

    let _e104 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e104 + 0.5f));
    let _e109 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e111 = fogType;
    let _e114 = fogType;
    return (((_e109 > 0.5f) && (_e111 >= 1i)) && (_e114 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e104 = wired_advanced_fog_enabled_u0028_();
    if !(_e104) {
        return 0f;
    }
    let _e107 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e107, 0.000001f));
    let _e112 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e112 + 0.5f));
    let _e115 = fogType_1;
    if (_e115 == 1i) {
        let _e119 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e119 <= 0f) {
            return 0f;
        }
        let _e121 = viewDepth;
        let _e124 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e121 / _e124), 0f, 1f);
    }
    let _e129 = unnamed.advancedFogColorDensity[3u];
    let _e131 = viewDepth;
    opticalDepth = (max(_e129, 0f) * _e131);
    let _e133 = fogType_1;
    if (_e133 == 2i) {
        let _e135 = opticalDepth;
        return clamp((1f - exp(-(_e135))), 0f, 1f);
    }
    let _e140 = opticalDepth;
    let _e141 = opticalDepth;
    return clamp((1f - exp(-((_e140 * _e141)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e107 = (*cosTheta);
    t = (1f - _e107);
    let _e109 = t;
    let _e110 = t;
    t2_ = (_e109 * _e110);
    let _e112 = (*roughness);
    let _e115 = (*F0_);
    Fmax = max(vec3((1f - _e112)), _e115);
    let _e117 = (*F0_);
    let _e118 = Fmax;
    let _e119 = (*F0_);
    let _e121 = t2_;
    let _e122 = t2_;
    let _e124 = t;
    return (_e117 + ((_e118 - _e119) * ((_e121 * _e122) * _e124)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e104 = (*rgb);
    let _e107 = unnamed.worldLightParams[0u];
    boosted = (_e104 * _e107);
    let _e110 = boosted[0u];
    let _e112 = boosted[1u];
    let _e114 = boosted[2u];
    peak = max(_e110, max(_e112, _e114));
    let _e117 = peak;
    if (_e117 > 1f) {
        let _e119 = peak;
        let _e120 = boosted;
        boosted = (_e120 / vec3(_e119));
    }
    let _e123 = boosted;
    return _e123;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e105 = (*c);
    (*c) = max(_e105, vec3<f32>(0f, 0f, 0f));
    let _e107 = (*c);
    cutoff = (_e107 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e109 = (*c);
    lo = (_e109 / vec3(12.92f));
    let _e112 = (*c);
    hi = pow(((_e112 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e117 = hi;
    let _e118 = lo;
    let _e119 = cutoff;
    return mix(_e117, _e118, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e119));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e107 = (*role);
    let _e109 = (*role);
    let _e114 = unnamed.packed_indices[(_e107 / 4u)][(_e109 % 4u)];
    let _e117 = (*role);
    let _e119 = (*role);
    let _e124 = unnamed.packed_indices[(_e117 / 4u)][(_e119 % 4u)];
    let _e129 = (*uv);
    let _e130 = textureSample(wired_bindless_images[(_e114 & 4095u)], wired_bindless_samplers[((_e124 >> bitcast<u32>(12i)) & 255u)], _e129);
    c_1 = _e130;
    let _e131 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e131))) == 0i) {
        let _e136 = c_1;
        param = _e136.xyz;
        let _e138 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e138.x;
        c_1[1u] = _e138.y;
        c_1[2u] = _e138.z;
    }
    let _e145 = (*slot);
    if (lightmap_slot == (_e145 + 1i)) {
        let _e148 = c_1;
        param_1 = _e148.xyz;
        let _e150 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e150.x;
        c_1[1u] = _e150.y;
        c_1[2u] = _e150.z;
    }
    let _e157 = c_1;
    return _e157;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e156 = unnamed.packed_indices[0i][3u];
    let _e162 = unnamed.packed_indices[0i][3u];
    let _e167 = fog_tex_coord_1;
    let _e168 = textureSample(wired_bindless_images[(_e156 & 4095u)], wired_bindless_samplers[((_e162 >> bitcast<u32>(12i)) & 255u)], _e167);
    fog = _e168;
    let _e169 = frag_color0In_1;
    param_2 = _e169.xyz;
    let _e171 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e173 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e171.x, _e171.y, _e171.z, _e173);
    let _e178 = frag_color1In_1;
    param_3 = _e178.xyz;
    let _e180 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e182 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e180.x, _e180.y, _e180.z, _e182);
    param_4 = 0u;
    let _e187 = frag_tex_coord0_1;
    param_5 = _e187;
    param_6 = 0i;
    let _e188 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    sampledColor0_ = _e188;
    param_7 = 1u;
    let _e189 = frag_tex_coord1_1;
    param_8 = _e189;
    param_9 = 1i;
    let _e190 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor1_ = _e190;
    if override_type_3_ {
        let _e191 = sampledColor1_;
        local = _e191.xyz;
    } else {
        let _e193 = sampledColor0_;
        local = _e193.xyz;
    }
    let _e195 = local;
    materialAlbedo = _e195;
    let _e196 = sampledColor0_;
    let _e197 = frag_color0_;
    color0_ = (_e196 * _e197);
    if override_type_3_3 {
        let _e199 = sampledColor1_;
        let _e200 = frag_color1_;
        color1_ = (_e199 * _e200);
        let _e202 = color0_;
        let _e204 = color1_;
        let _e206 = (_e202.xyz + _e204.xyz);
        let _e208 = color0_[3u];
        let _e210 = color1_[3u];
        base = vec4<f32>(_e206.x, _e206.y, _e206.z, (_e208 * _e210));
    } else {
        if override_type_3_4 {
            let _e216 = sampledColor1_;
            let _e217 = frag_color1_;
            color1_1 = (_e216 * _e217);
            let _e220 = color0_[3u];
            let _e221 = color0_;
            color0_ = (_e221 * _e220);
            let _e224 = color1_1[3u];
            let _e225 = color1_1;
            color1_1 = (_e225 * _e224);
            let _e227 = color0_;
            let _e229 = color1_1;
            let _e231 = (_e227.xyz + _e229.xyz);
            let _e233 = color0_[3u];
            let _e235 = color1_1[3u];
            base = vec4<f32>(_e231.x, _e231.y, _e231.z, (_e233 * _e235));
        } else {
            if override_type_3_5 {
                let _e241 = sampledColor1_;
                let _e242 = frag_color1_;
                color1_2 = (_e241 * _e242);
                let _e245 = color0_[3u];
                let _e247 = color0_;
                color0_ = (_e247 * (1f - _e245));
                let _e250 = color1_2[3u];
                let _e252 = color1_2;
                color1_2 = (_e252 * (1f - _e250));
                let _e254 = color0_;
                let _e256 = color1_2;
                let _e258 = (_e254.xyz + _e256.xyz);
                let _e260 = color0_[3u];
                let _e262 = color1_2[3u];
                base = vec4<f32>(_e258.x, _e258.y, _e258.z, (_e260 * _e262));
            } else {
                if override_type_3_6 {
                    let _e268 = sampledColor1_;
                    let _e269 = frag_color1_;
                    color1_3 = (_e268 * _e269);
                    let _e271 = color0_;
                    let _e272 = color1_3;
                    let _e274 = color1_3[3u];
                    base = mix(_e271, _e272, vec4(_e274));
                } else {
                    if override_type_3_7 {
                        let _e277 = sampledColor1_;
                        let _e278 = frag_color1_;
                        color1_4 = (_e277 * _e278);
                        let _e280 = color1_4;
                        let _e281 = color0_;
                        let _e283 = color1_4[3u];
                        base = mix(_e280, _e281, vec4(_e283));
                    } else {
                        if override_type_3_8 {
                            let _e286 = sampledColor1_;
                            let _e287 = frag_color1_;
                            color1_5 = (_e286 * _e287);
                            let _e289 = color1_5;
                            let _e291 = color1_5[3u];
                            let _e294 = color0_;
                            base = ((_e289 + vec4(_e291)) * _e294);
                        } else {
                            let _e296 = sampledColor1_;
                            let _e297 = frag_color1_;
                            color1_6 = (_e296 * _e297);
                            let _e299 = color0_;
                            let _e301 = color1_6;
                            let _e303 = (_e299.xyz * _e301.xyz);
                            base[0u] = _e303.x;
                            base[1u] = _e303.y;
                            base[2u] = _e303.z;
                            let _e311 = color0_[3u];
                            let _e313 = color1_6[3u];
                            base[3u] = (_e311 * _e313);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_9 {
        let _e318 = unnamed.worldLightParams[1u];
        wetness = clamp(_e318, 0f, 1f);
        let _e322 = unnamed.worldLightParams[2u];
        frost = clamp(_e322, 0f, 1f);
        let _e324 = base;
        luminance = dot(_e324.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e327 = wetness;
        let _e329 = base;
        let _e331 = (_e329.xyz * mix(1f, 0.82f, _e327));
        base[0u] = _e331.x;
        base[1u] = _e331.y;
        base[2u] = _e331.z;
        let _e338 = base;
        let _e340 = luminance;
        let _e342 = luminance;
        let _e344 = luminance;
        let _e346 = frost;
        let _e349 = mix(_e338.xyz, vec3<f32>((_e340 * 0.88f), (_e342 * 0.94f), _e344), vec3((_e346 * 0.55f)));
        base[0u] = _e349.x;
        base[1u] = _e349.y;
        base[2u] = _e349.z;
        let _e356 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e356).z);
        let _e362 = unnamed.worldLightParams[3u];
        let _e364 = upward;
        snowCoverage = (clamp(_e362, 0f, 1f) * _e364);
        let _e366 = base;
        let _e368 = snowCoverage;
        let _e371 = mix(_e366.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e368 * 0.82f)));
        base[0u] = _e371.x;
        base[1u] = _e371.y;
        base[2u] = _e371.z;
    }
    let _e380 = unnamed.materialFactors[0u];
    if (_e380 >= 0f) {
        let _e383 = unnamed.materialBaseColor;
        let _e384 = base;
        base = (_e384 * _e383);
    }
    let _e386 = color0_;
    let _e389 = unnamed.emissionRadiance;
    let _e392 = base;
    let _e394 = (_e392.xyz + (_e386.xyz * _e389.xyz));
    base[0u] = _e394.x;
    base[1u] = _e394.y;
    base[2u] = _e394.z;
    if override_type_3_10 {
        if override_type_3_11 {
            let _e404 = unnamed.packed_indices[2i][0u];
            let _e410 = unnamed.packed_indices[2i][0u];
            let _e415 = frag_tex_coord0_1;
            let _e416 = textureSample(wired_bindless_images[(_e404 & 4095u)], wired_bindless_samplers[((_e410 >> bitcast<u32>(12i)) & 255u)], _e415);
            local_1 = _e416.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e418 = local_1;
        orm = _e418;
        let _e420 = orm[0u];
        let _e423 = unnamed.materialFactors[2u];
        ao = mix(1f, _e420, clamp(_e423, 0f, 1f));
        let _e427 = orm[1u];
        let _e430 = unnamed.materialFactors[1u];
        let _e434 = unnamed.worldLightParams[1u];
        let _e439 = unnamed.worldLightParams[2u];
        let _e444 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e427 * _e430) - (0.35f * _e434)) + (0.3f * _e439)) + (0.4f * _e444)), 0.04f, 1f);
        let _e449 = orm[2u];
        let _e452 = unnamed.materialFactors[0u];
        metalness = clamp((_e449 * _e452), 0f, 1f);
        let _e455 = ibl_N_1;
        ibl_n = normalize(_e455);
        let _e457 = ibl_V_1;
        ibl_v = normalize(_e457);
        let _e459 = materialAlbedo;
        let _e461 = unnamed.materialBaseColor;
        let _e464 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e459 * _e461.xyz), vec3(_e464));
        let _e467 = ibl_n;
        let _e468 = ibl_v;
        NdotV = max(dot(_e467, _e468), 0f);
        let _e471 = NdotV;
        param_10 = _e471;
        let _e472 = F0_1;
        param_11 = _e472;
        let _e473 = roughness_1;
        param_12 = _e473;
        let _e474 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_10), (&param_11), (&param_12));
        F_amb = _e474;
        let _e475 = ibl_v;
        let _e477 = ibl_n;
        R = reflect(-(_e475), _e477);
        let _e481 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e481, 0f, 1f);
        let _e483 = R;
        let _e484 = roughness_1;
        let _e485 = textureNumLevels(radianceCube);
        let _e490 = textureSampleLevel(radianceCube, radianceCube_sampler, _e483, (_e484 * f32((i32(_e485) - 1i))));
        currentPrefiltered = _e490.xyz;
        let _e492 = R;
        let _e493 = roughness_1;
        let _e494 = textureNumLevels(previousRadianceCube);
        let _e499 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e492, (_e493 * f32((i32(_e494) - 1i))));
        previousPrefiltered = _e499.xyz;
        let _e501 = previousPrefiltered;
        let _e502 = currentPrefiltered;
        let _e503 = environmentBlend;
        prefiltered = mix(_e501, _e502, vec3(_e503));
        let _e506 = NdotV;
        let _e507 = roughness_1;
        let _e509 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e506, _e507), 0f);
        envBRDF = _e509.xy;
        let _e511 = prefiltered;
        let _e512 = F_amb;
        let _e514 = envBRDF[0u];
        let _e517 = envBRDF[1u];
        let _e521 = ao;
        specularIBL = ((_e511 * ((_e512 * _e514) + vec3(_e517))) * _e521);
        let _e523 = gl_FragCoord_1;
        let _e525 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e523.xy / vec2<f32>(vec2<i32>(_e525)));
        let _e529 = gtaoUV;
        let _e530 = textureSample(gtaoMap, gtaoMap_sampler, _e529);
        gtao_vis = _e530.x;
        let _e532 = gtao_vis;
        let _e533 = specularIBL;
        specularIBL = (_e533 * _e532);
        let _e535 = specularIBL;
        let _e536 = base;
        let _e538 = (_e536.xyz + _e535);
        base[0u] = _e538.x;
        base[1u] = _e538.y;
        base[2u] = _e538.z;
    }
    let _e545 = wired_advanced_fog_enabled_u0028_();
    if _e545 {
        let _e546 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e546;
        if override_type_3_12 {
            let _e547 = fogAmount;
            let _e549 = base;
            let _e551 = (_e549.xyz * (1f - _e547));
            base[0u] = _e551.x;
            base[1u] = _e551.y;
            base[2u] = _e551.z;
        } else {
            if override_type_3_13 {
                let _e558 = fogAmount;
                let _e560 = base;
                base = (_e560 * (1f - _e558));
            } else {
                if override_type_3_14 {
                    let _e562 = fogAmount;
                    let _e565 = base[3u];
                    base[3u] = (_e565 * (1f - _e562));
                } else {
                    let _e568 = base;
                    let _e571 = unnamed.advancedFogColorDensity;
                    let _e573 = fogAmount;
                    let _e575 = mix(_e568.xyz, _e571.xyz, vec3(_e573));
                    base[0u] = _e575.x;
                    base[1u] = _e575.y;
                    base[2u] = _e575.z;
                }
            }
        }
    } else {
        if override_type_3_15 {
            let _e582 = base;
            let _e585 = fog[3u];
            let _e587 = (_e582.xyz * (1f - _e585));
            base[0u] = _e587.x;
            base[1u] = _e587.y;
            base[2u] = _e587.z;
        } else {
            if override_type_3_16 {
                let _e594 = base;
                let _e596 = fog[3u];
                base = (_e594 * (1f - _e596));
            } else {
                if override_type_3_17 {
                    let _e600 = base[3u];
                    let _e602 = fog[3u];
                    base[3u] = (_e600 * (1f - _e602));
                } else {
                    let _e606 = base;
                    let _e607 = fog;
                    let _e609 = unnamed.fogColor;
                    let _e612 = fog[3u];
                    base = mix(_e606, (_e607 * _e609), vec4(_e612));
                }
            }
        }
    }
    if override_type_3_18 {
        let _e616 = base[3u];
        if (_e616 == 0f) {
            discard;
        }
    } else {
        if override_type_3_19 {
            let _e618 = base;
            let _e620 = base;
            if (dot(_e618.xyz, _e620.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e624 = base;
    out_color = _e624;
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
