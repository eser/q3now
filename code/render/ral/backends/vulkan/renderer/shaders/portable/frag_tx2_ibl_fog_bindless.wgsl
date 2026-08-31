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
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(14) override ibl_enabled: i32 = 0i;
override override_type_3_3: bool = (ibl_enabled != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_4: bool = (acff == 1i);
override override_type_3_5: bool = (acff == 2i);
override override_type_3_6: bool = (acff == 3i);
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_10: bool = (discard_mode == 1i);
override override_type_3_11: bool = (discard_mode == 2i);
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
@group(2) @binding(1)
var brdfLut: texture_2d<f32>;
@group(2) @binding(33)
var brdfLut_sampler: sampler;
@group(2) @binding(4)
var gtaoMap: texture_2d<f32>;
@group(2) @binding(36)
var gtaoMap_sampler: sampler;
var<private> out_color: vec4<f32>;
@group(2) @binding(2)
var irradianceCube: texture_cube<f32>;
@group(2) @binding(34)
var irradianceCube_sampler: sampler;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e92 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e92 + 0.5f));
    let _e97 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e99 = fogType;
    let _e102 = fogType;
    return (((_e97 > 0.5f) && (_e99 >= 1i)) && (_e102 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e92 = wired_advanced_fog_enabled_u0028_();
    if !(_e92) {
        return 0f;
    }
    let _e95 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e95, 0.000001f));
    let _e100 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e100 + 0.5f));
    let _e103 = fogType_1;
    if (_e103 == 1i) {
        let _e107 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e107 <= 0f) {
            return 0f;
        }
        let _e109 = viewDepth;
        let _e112 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e109 / _e112), 0f, 1f);
    }
    let _e117 = unnamed.advancedFogColorDensity[3u];
    let _e119 = viewDepth;
    opticalDepth = (max(_e117, 0f) * _e119);
    let _e121 = fogType_1;
    if (_e121 == 2i) {
        let _e123 = opticalDepth;
        return clamp((1f - exp(-(_e123))), 0f, 1f);
    }
    let _e128 = opticalDepth;
    let _e129 = opticalDepth;
    return clamp((1f - exp(-((_e128 * _e129)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e95 = (*cosTheta);
    t = (1f - _e95);
    let _e97 = t;
    let _e98 = t;
    t2_ = (_e97 * _e98);
    let _e100 = (*roughness);
    let _e103 = (*F0_);
    Fmax = max(vec3((1f - _e100)), _e103);
    let _e105 = (*F0_);
    let _e106 = Fmax;
    let _e107 = (*F0_);
    let _e109 = t2_;
    let _e110 = t2_;
    let _e112 = t;
    return (_e105 + ((_e106 - _e107) * ((_e109 * _e110) * _e112)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e92 = (*rgb);
    let _e95 = unnamed.worldLightParams[0u];
    boosted = (_e92 * _e95);
    let _e98 = boosted[0u];
    let _e100 = boosted[1u];
    let _e102 = boosted[2u];
    peak = max(_e98, max(_e100, _e102));
    let _e105 = peak;
    if (_e105 > 1f) {
        let _e107 = peak;
        let _e108 = boosted;
        boosted = (_e108 / vec3(_e107));
    }
    let _e111 = boosted;
    return _e111;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e93 = (*c);
    (*c) = max(_e93, vec3<f32>(0f, 0f, 0f));
    let _e95 = (*c);
    cutoff = (_e95 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e97 = (*c);
    lo = (_e97 / vec3(12.92f));
    let _e100 = (*c);
    hi = pow(((_e100 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e105 = hi;
    let _e106 = lo;
    let _e107 = cutoff;
    return mix(_e105, _e106, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e107));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e95 = (*role);
    let _e97 = (*role);
    let _e102 = unnamed.packed_indices[(_e95 / 4u)][(_e97 % 4u)];
    let _e105 = (*role);
    let _e107 = (*role);
    let _e112 = unnamed.packed_indices[(_e105 / 4u)][(_e107 % 4u)];
    let _e117 = (*uv);
    let _e118 = textureSample(wired_bindless_images[(_e102 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    c_1 = _e118;
    let _e119 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e119))) == 0i) {
        let _e124 = c_1;
        param = _e124.xyz;
        let _e126 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e126.x;
        c_1[1u] = _e126.y;
        c_1[2u] = _e126.z;
    }
    let _e133 = (*slot);
    if (lightmap_slot == (_e133 + 1i)) {
        let _e136 = c_1;
        param_1 = _e136.xyz;
        let _e138 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e138.x;
        c_1[1u] = _e138.y;
        c_1[2u] = _e138.z;
    }
    let _e145 = c_1;
    return _e145;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var color2_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color2_1: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_2: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color2_2: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var upward: f32;
    var snowCoverage: f32;
    var orm: vec3<f32>;
    var ao: f32;
    var roughness_1: f32;
    var metalness: f32;
    var ibl_n: vec3<f32>;
    var ibl_v: vec3<f32>;
    var F0_1: vec3<f32>;
    var NdotV: f32;
    var F_amb: vec3<f32>;
    var param_24: f32;
    var param_25: vec3<f32>;
    var param_26: f32;
    var R: vec3<f32>;
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
    let _e172 = frag_color0_;
    color0_ = (_e171 * _e172);
    if override_type_3_ {
        param_6 = 1u;
        let _e174 = frag_tex_coord1_1;
        param_7 = _e174;
        param_8 = 1i;
        let _e175 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        color1_ = _e175;
        param_9 = 2u;
        let _e176 = frag_tex_coord2_1;
        param_10 = _e176;
        param_11 = 2i;
        let _e177 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color2_ = _e177;
        let _e178 = color0_;
        let _e180 = color1_;
        let _e183 = color2_;
        let _e185 = ((_e178.xyz + _e180.xyz) + _e183.xyz);
        let _e187 = color0_[3u];
        let _e189 = color1_[3u];
        let _e192 = color2_[3u];
        base = vec4<f32>(_e185.x, _e185.y, _e185.z, ((_e187 * _e189) * _e192));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e198 = frag_tex_coord1_1;
            param_13 = _e198;
            param_14 = 1i;
            let _e199 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            let _e200 = frag_color0_;
            color1_1 = (_e199 * _e200);
            param_15 = 2u;
            let _e202 = frag_tex_coord2_1;
            param_16 = _e202;
            param_17 = 2i;
            let _e203 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            let _e204 = frag_color0_;
            color2_1 = (_e203 * _e204);
            let _e206 = color0_;
            let _e208 = color1_1;
            let _e211 = color2_1;
            let _e213 = ((_e206.xyz + _e208.xyz) + _e211.xyz);
            let _e215 = color0_[3u];
            let _e217 = color1_1[3u];
            let _e220 = color2_1[3u];
            base = vec4<f32>(_e213.x, _e213.y, _e213.z, ((_e215 * _e217) * _e220));
        } else {
            param_18 = 1u;
            let _e226 = frag_tex_coord1_1;
            param_19 = _e226;
            param_20 = 1i;
            let _e227 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            color1_2 = _e227;
            param_21 = 2u;
            let _e228 = frag_tex_coord2_1;
            param_22 = _e228;
            param_23 = 2i;
            let _e229 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            color2_2 = _e229;
            let _e230 = color0_;
            let _e232 = color1_2;
            let _e235 = color2_2;
            let _e237 = ((_e230.xyz * _e232.xyz) * _e235.xyz);
            base[0u] = _e237.x;
            base[1u] = _e237.y;
            base[2u] = _e237.z;
            let _e245 = color0_[3u];
            let _e247 = color1_2[3u];
            let _e250 = color2_2[3u];
            base[3u] = ((_e245 * _e247) * _e250);
        }
    }
    if override_type_3_2 {
        let _e255 = unnamed.worldLightParams[1u];
        wetness = clamp(_e255, 0f, 1f);
        let _e259 = unnamed.worldLightParams[2u];
        frost = clamp(_e259, 0f, 1f);
        let _e261 = base;
        luminance = dot(_e261.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e264 = wetness;
        let _e266 = base;
        let _e268 = (_e266.xyz * mix(1f, 0.82f, _e264));
        base[0u] = _e268.x;
        base[1u] = _e268.y;
        base[2u] = _e268.z;
        let _e275 = base;
        let _e277 = luminance;
        let _e279 = luminance;
        let _e281 = luminance;
        let _e283 = frost;
        let _e286 = mix(_e275.xyz, vec3<f32>((_e277 * 0.88f), (_e279 * 0.94f), _e281), vec3((_e283 * 0.55f)));
        base[0u] = _e286.x;
        base[1u] = _e286.y;
        base[2u] = _e286.z;
        let _e293 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e293).z);
        let _e299 = unnamed.worldLightParams[3u];
        let _e301 = upward;
        snowCoverage = (clamp(_e299, 0f, 1f) * _e301);
        let _e303 = base;
        let _e305 = snowCoverage;
        let _e308 = mix(_e303.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e305 * 0.82f)));
        base[0u] = _e308.x;
        base[1u] = _e308.y;
        base[2u] = _e308.z;
    }
    let _e315 = color0_;
    let _e318 = unnamed.emissionRadiance;
    let _e321 = base;
    let _e323 = (_e321.xyz + (_e315.xyz * _e318.xyz));
    base[0u] = _e323.x;
    base[1u] = _e323.y;
    base[2u] = _e323.z;
    if override_type_3_3 {
        let _e333 = unnamed.packed_indices[2i][0u];
        let _e339 = unnamed.packed_indices[2i][0u];
        let _e344 = frag_tex_coord0_1;
        let _e345 = textureSample(wired_bindless_images[(_e333 & 4095u)], wired_bindless_samplers[((_e339 >> bitcast<u32>(12i)) & 255u)], _e344);
        orm = _e345.xyz;
        let _e348 = orm[0u];
        ao = _e348;
        let _e350 = orm[1u];
        let _e353 = unnamed.worldLightParams[1u];
        let _e358 = unnamed.worldLightParams[2u];
        let _e363 = unnamed.worldLightParams[3u];
        roughness_1 = clamp((((_e350 - (0.35f * _e353)) + (0.3f * _e358)) + (0.4f * _e363)), 0.04f, 1f);
        let _e368 = orm[2u];
        metalness = _e368;
        let _e369 = ibl_N_1;
        ibl_n = normalize(_e369);
        let _e371 = ibl_V_1;
        ibl_v = normalize(_e371);
        let _e373 = base;
        let _e375 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e373.xyz, vec3(_e375));
        let _e378 = ibl_n;
        let _e379 = ibl_v;
        NdotV = max(dot(_e378, _e379), 0f);
        let _e382 = NdotV;
        param_24 = _e382;
        let _e383 = F0_1;
        param_25 = _e383;
        let _e384 = roughness_1;
        param_26 = _e384;
        let _e385 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_24), (&param_25), (&param_26));
        F_amb = _e385;
        let _e386 = ibl_v;
        let _e388 = ibl_n;
        R = reflect(-(_e386), _e388);
        let _e390 = R;
        let _e391 = roughness_1;
        let _e393 = textureSampleLevel(radianceCube, radianceCube_sampler, _e390, (_e391 * 5f));
        prefiltered = _e393.xyz;
        let _e395 = NdotV;
        let _e396 = roughness_1;
        let _e398 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e395, _e396), 0f);
        envBRDF = _e398.xy;
        let _e400 = prefiltered;
        let _e401 = F_amb;
        let _e403 = envBRDF[0u];
        let _e406 = envBRDF[1u];
        let _e410 = ao;
        specularIBL = ((_e400 * ((_e401 * _e403) + vec3(_e406))) * _e410);
        let _e412 = gl_FragCoord_1;
        let _e414 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e412.xy / vec2<f32>(vec2<i32>(_e414)));
        let _e418 = gtaoUV;
        let _e419 = textureSample(gtaoMap, gtaoMap_sampler, _e418);
        gtao_vis = _e419.x;
        let _e421 = gtao_vis;
        let _e422 = specularIBL;
        specularIBL = (_e422 * _e421);
        let _e424 = specularIBL;
        let _e425 = base;
        let _e427 = (_e425.xyz + _e424);
        base[0u] = _e427.x;
        base[1u] = _e427.y;
        base[2u] = _e427.z;
    }
    let _e434 = wired_advanced_fog_enabled_u0028_();
    if _e434 {
        let _e435 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e435;
        if override_type_3_4 {
            let _e436 = fogAmount;
            let _e438 = base;
            let _e440 = (_e438.xyz * (1f - _e436));
            base[0u] = _e440.x;
            base[1u] = _e440.y;
            base[2u] = _e440.z;
        } else {
            if override_type_3_5 {
                let _e447 = fogAmount;
                let _e449 = base;
                base = (_e449 * (1f - _e447));
            } else {
                if override_type_3_6 {
                    let _e451 = fogAmount;
                    let _e454 = base[3u];
                    base[3u] = (_e454 * (1f - _e451));
                } else {
                    let _e457 = base;
                    let _e460 = unnamed.advancedFogColorDensity;
                    let _e462 = fogAmount;
                    let _e464 = mix(_e457.xyz, _e460.xyz, vec3(_e462));
                    base[0u] = _e464.x;
                    base[1u] = _e464.y;
                    base[2u] = _e464.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e471 = base;
            let _e474 = fog[3u];
            let _e476 = (_e471.xyz * (1f - _e474));
            base[0u] = _e476.x;
            base[1u] = _e476.y;
            base[2u] = _e476.z;
        } else {
            if override_type_3_8 {
                let _e483 = base;
                let _e485 = fog[3u];
                base = (_e483 * (1f - _e485));
            } else {
                if override_type_3_9 {
                    let _e489 = base[3u];
                    let _e491 = fog[3u];
                    base[3u] = (_e489 * (1f - _e491));
                } else {
                    let _e495 = base;
                    let _e496 = fog;
                    let _e498 = unnamed.fogColor;
                    let _e501 = fog[3u];
                    base = mix(_e495, (_e496 * _e498), vec4(_e501));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e505 = base[3u];
        if (_e505 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e507 = base;
            let _e509 = base;
            if (dot(_e507.xyz, _e509.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e513 = base;
    out_color = _e513;
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
