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

    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e91 + 0.5f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e98 = fogType;
    let _e101 = fogType;
    return (((_e96 > 0.5f) && (_e98 >= 1i)) && (_e101 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e91 = wired_advanced_fog_enabled_u0028_();
    if !(_e91) {
        return 0f;
    }
    let _e94 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e94, 0.000001f));
    let _e99 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e99 + 0.5f));
    let _e102 = fogType_1;
    if (_e102 == 1i) {
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e106 <= 0f) {
            return 0f;
        }
        let _e108 = viewDepth;
        let _e111 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e108 / _e111), 0f, 1f);
    }
    let _e116 = unnamed.advancedFogColorDensity[3u];
    let _e118 = viewDepth;
    opticalDepth = (max(_e116, 0f) * _e118);
    let _e120 = fogType_1;
    if (_e120 == 2i) {
        let _e122 = opticalDepth;
        return clamp((1f - exp(-(_e122))), 0f, 1f);
    }
    let _e127 = opticalDepth;
    let _e128 = opticalDepth;
    return clamp((1f - exp(-((_e127 * _e128)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e94 = (*cosTheta);
    t = (1f - _e94);
    let _e96 = t;
    let _e97 = t;
    t2_ = (_e96 * _e97);
    let _e99 = (*roughness);
    let _e102 = (*F0_);
    Fmax = max(vec3((1f - _e99)), _e102);
    let _e104 = (*F0_);
    let _e105 = Fmax;
    let _e106 = (*F0_);
    let _e108 = t2_;
    let _e109 = t2_;
    let _e111 = t;
    return (_e104 + ((_e105 - _e106) * ((_e108 * _e109) * _e111)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e92 = (*c);
    (*c) = max(_e92, vec3<f32>(0f, 0f, 0f));
    let _e94 = (*c);
    cutoff = (_e94 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e96 = (*c);
    lo = (_e96 / vec3(12.92f));
    let _e99 = (*c);
    hi = pow(((_e99 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e104 = hi;
    let _e105 = lo;
    let _e106 = cutoff;
    return mix(_e104, _e105, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e106));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e103 = (*role);
    let _e105 = (*role);
    let _e110 = unnamed.packed_indices[(_e103 / 4u)][(_e105 % 4u)];
    let _e115 = (*uv);
    let _e116 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    c_1 = _e116;
    let _e117 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e117))) == 0i) {
        let _e122 = c_1;
        param = _e122.xyz;
        let _e124 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = (*slot);
    if (lightmap_slot == (_e131 + 1i)) {
        let _e136 = unnamed.worldLightParams[0u];
        let _e137 = c_1;
        let _e139 = (_e137.xyz * _e136);
        c_1[0u] = _e139.x;
        c_1[1u] = _e139.y;
        c_1[2u] = _e139.z;
    }
    let _e146 = c_1;
    return _e146;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
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
    var param_14: f32;
    var param_15: vec3<f32>;
    var param_16: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e135 = unnamed.packed_indices[0i][3u];
    let _e141 = unnamed.packed_indices[0i][3u];
    let _e146 = fog_tex_coord_1;
    let _e147 = textureSample(wired_bindless_images[(_e135 & 4095u)], wired_bindless_samplers[((_e141 >> bitcast<u32>(12i)) & 255u)], _e146);
    fog = _e147;
    let _e148 = frag_color0In_1;
    param_1 = _e148.xyz;
    let _e150 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e152 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e150.x, _e150.y, _e150.z, _e152);
    param_2 = 0u;
    let _e157 = frag_tex_coord0_1;
    param_3 = _e157;
    param_4 = 0i;
    let _e158 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e159 = frag_color0_;
    color0_ = (_e158 * _e159);
    if override_type_3_ {
        param_5 = 1u;
        let _e161 = frag_tex_coord1_1;
        param_6 = _e161;
        param_7 = 1i;
        let _e162 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e162;
        let _e163 = color0_;
        let _e165 = color1_;
        let _e167 = (_e163.xyz + _e165.xyz);
        let _e169 = color0_[3u];
        let _e171 = color1_[3u];
        base = vec4<f32>(_e167.x, _e167.y, _e167.z, (_e169 * _e171));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e177 = frag_tex_coord1_1;
            param_9 = _e177;
            param_10 = 1i;
            let _e178 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e179 = frag_color0_;
            color1_1 = (_e178 * _e179);
            let _e181 = color0_;
            let _e183 = color1_1;
            let _e185 = (_e181.xyz + _e183.xyz);
            let _e187 = color0_[3u];
            let _e189 = color1_1[3u];
            base = vec4<f32>(_e185.x, _e185.y, _e185.z, (_e187 * _e189));
        } else {
            param_11 = 1u;
            let _e195 = frag_tex_coord1_1;
            param_12 = _e195;
            param_13 = 1i;
            let _e196 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e196;
            let _e197 = color0_;
            let _e199 = color1_2;
            let _e201 = (_e197.xyz * _e199.xyz);
            base[0u] = _e201.x;
            base[1u] = _e201.y;
            base[2u] = _e201.z;
            let _e209 = color0_[3u];
            let _e211 = color1_2[3u];
            base[3u] = (_e209 * _e211);
        }
    }
    if override_type_3_2 {
        let _e216 = unnamed.worldLightParams[1u];
        wetness = clamp(_e216, 0f, 1f);
        let _e220 = unnamed.worldLightParams[2u];
        frost = clamp(_e220, 0f, 1f);
        let _e222 = base;
        luminance = dot(_e222.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e225 = wetness;
        let _e227 = base;
        let _e229 = (_e227.xyz * mix(1f, 0.82f, _e225));
        base[0u] = _e229.x;
        base[1u] = _e229.y;
        base[2u] = _e229.z;
        let _e236 = base;
        let _e238 = luminance;
        let _e240 = luminance;
        let _e242 = luminance;
        let _e244 = frost;
        let _e247 = mix(_e236.xyz, vec3<f32>((_e238 * 0.88f), (_e240 * 0.94f), _e242), vec3((_e244 * 0.55f)));
        base[0u] = _e247.x;
        base[1u] = _e247.y;
        base[2u] = _e247.z;
        let _e254 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e254).z);
        let _e260 = unnamed.worldLightParams[3u];
        let _e262 = upward;
        snowCoverage = (clamp(_e260, 0f, 1f) * _e262);
        let _e264 = base;
        let _e266 = snowCoverage;
        let _e269 = mix(_e264.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e266 * 0.82f)));
        base[0u] = _e269.x;
        base[1u] = _e269.y;
        base[2u] = _e269.z;
    }
    let _e276 = color0_;
    let _e279 = unnamed.emissionRadiance;
    let _e282 = base;
    let _e284 = (_e282.xyz + (_e276.xyz * _e279.xyz));
    base[0u] = _e284.x;
    base[1u] = _e284.y;
    base[2u] = _e284.z;
    if override_type_3_3 {
        let _e294 = unnamed.packed_indices[2i][0u];
        let _e300 = unnamed.packed_indices[2i][0u];
        let _e305 = frag_tex_coord0_1;
        let _e306 = textureSample(wired_bindless_images[(_e294 & 4095u)], wired_bindless_samplers[((_e300 >> bitcast<u32>(12i)) & 255u)], _e305);
        orm = _e306.xyz;
        let _e309 = orm[0u];
        ao = _e309;
        let _e311 = orm[1u];
        let _e314 = unnamed.worldLightParams[1u];
        let _e319 = unnamed.worldLightParams[2u];
        let _e324 = unnamed.worldLightParams[3u];
        roughness_1 = clamp((((_e311 - (0.35f * _e314)) + (0.3f * _e319)) + (0.4f * _e324)), 0.04f, 1f);
        let _e329 = orm[2u];
        metalness = _e329;
        let _e330 = ibl_N_1;
        ibl_n = normalize(_e330);
        let _e332 = ibl_V_1;
        ibl_v = normalize(_e332);
        let _e334 = base;
        let _e336 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e334.xyz, vec3(_e336));
        let _e339 = ibl_n;
        let _e340 = ibl_v;
        NdotV = max(dot(_e339, _e340), 0f);
        let _e343 = NdotV;
        param_14 = _e343;
        let _e344 = F0_1;
        param_15 = _e344;
        let _e345 = roughness_1;
        param_16 = _e345;
        let _e346 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        F_amb = _e346;
        let _e347 = ibl_v;
        let _e349 = ibl_n;
        R = reflect(-(_e347), _e349);
        let _e351 = R;
        let _e352 = roughness_1;
        let _e354 = textureSampleLevel(radianceCube, radianceCube_sampler, _e351, (_e352 * 5f));
        prefiltered = _e354.xyz;
        let _e356 = NdotV;
        let _e357 = roughness_1;
        let _e359 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e356, _e357), 0f);
        envBRDF = _e359.xy;
        let _e361 = prefiltered;
        let _e362 = F_amb;
        let _e364 = envBRDF[0u];
        let _e367 = envBRDF[1u];
        let _e371 = ao;
        specularIBL = ((_e361 * ((_e362 * _e364) + vec3(_e367))) * _e371);
        let _e373 = gl_FragCoord_1;
        let _e375 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e373.xy / vec2<f32>(vec2<i32>(_e375)));
        let _e379 = gtaoUV;
        let _e380 = textureSample(gtaoMap, gtaoMap_sampler, _e379);
        gtao_vis = _e380.x;
        let _e382 = gtao_vis;
        let _e383 = specularIBL;
        specularIBL = (_e383 * _e382);
        let _e385 = specularIBL;
        let _e386 = base;
        let _e388 = (_e386.xyz + _e385);
        base[0u] = _e388.x;
        base[1u] = _e388.y;
        base[2u] = _e388.z;
    }
    let _e395 = wired_advanced_fog_enabled_u0028_();
    if _e395 {
        let _e396 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e396;
        if override_type_3_4 {
            let _e397 = fogAmount;
            let _e399 = base;
            let _e401 = (_e399.xyz * (1f - _e397));
            base[0u] = _e401.x;
            base[1u] = _e401.y;
            base[2u] = _e401.z;
        } else {
            if override_type_3_5 {
                let _e408 = fogAmount;
                let _e410 = base;
                base = (_e410 * (1f - _e408));
            } else {
                if override_type_3_6 {
                    let _e412 = fogAmount;
                    let _e415 = base[3u];
                    base[3u] = (_e415 * (1f - _e412));
                } else {
                    let _e418 = base;
                    let _e421 = unnamed.advancedFogColorDensity;
                    let _e423 = fogAmount;
                    let _e425 = mix(_e418.xyz, _e421.xyz, vec3(_e423));
                    base[0u] = _e425.x;
                    base[1u] = _e425.y;
                    base[2u] = _e425.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e432 = base;
            let _e435 = fog[3u];
            let _e437 = (_e432.xyz * (1f - _e435));
            base[0u] = _e437.x;
            base[1u] = _e437.y;
            base[2u] = _e437.z;
        } else {
            if override_type_3_8 {
                let _e444 = base;
                let _e446 = fog[3u];
                base = (_e444 * (1f - _e446));
            } else {
                if override_type_3_9 {
                    let _e450 = base[3u];
                    let _e452 = fog[3u];
                    base[3u] = (_e450 * (1f - _e452));
                } else {
                    let _e456 = base;
                    let _e457 = fog;
                    let _e459 = unnamed.fogColor;
                    let _e462 = fog[3u];
                    base = mix(_e456, (_e457 * _e459), vec4(_e462));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e466 = base[3u];
        if (_e466 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e468 = base;
            let _e470 = base;
            if (dot(_e468.xyz, _e470.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e474 = base;
    out_color = _e474;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e15 = out_color;
    return _e15;
}
