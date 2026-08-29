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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_4: bool = (discard_mode == 1i);
override override_type_3_5: bool = (discard_mode == 2i);
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

    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e83 + 0.5f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e90 = fogType;
    let _e93 = fogType;
    return (((_e88 > 0.5f) && (_e90 >= 1i)) && (_e93 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e83 = wired_advanced_fog_enabled_u0028_();
    if !(_e83) {
        return 0f;
    }
    let _e86 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e86, 0.000001f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e91 + 0.5f));
    let _e94 = fogType_1;
    if (_e94 == 1i) {
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e98 <= 0f) {
            return 0f;
        }
        let _e100 = viewDepth;
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e100 / _e103), 0f, 1f);
    }
    let _e108 = unnamed.advancedFogColorDensity[3u];
    let _e110 = viewDepth;
    opticalDepth = (max(_e108, 0f) * _e110);
    let _e112 = fogType_1;
    if (_e112 == 2i) {
        let _e114 = opticalDepth;
        return clamp((1f - exp(-(_e114))), 0f, 1f);
    }
    let _e119 = opticalDepth;
    let _e120 = opticalDepth;
    return clamp((1f - exp(-((_e119 * _e120)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e86 = (*cosTheta);
    t = (1f - _e86);
    let _e88 = t;
    let _e89 = t;
    t2_ = (_e88 * _e89);
    let _e91 = (*roughness);
    let _e94 = (*F0_);
    Fmax = max(vec3((1f - _e91)), _e94);
    let _e96 = (*F0_);
    let _e97 = Fmax;
    let _e98 = (*F0_);
    let _e100 = t2_;
    let _e101 = t2_;
    let _e103 = t;
    return (_e96 + ((_e97 - _e98) * ((_e100 * _e101) * _e103)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e84 = (*c);
    (*c) = max(_e84, vec3<f32>(0f, 0f, 0f));
    let _e86 = (*c);
    cutoff = (_e86 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e88 = (*c);
    lo = (_e88 / vec3(12.92f));
    let _e91 = (*c);
    hi = pow(((_e91 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e96 = hi;
    let _e97 = lo;
    let _e98 = cutoff;
    return mix(_e96, _e97, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e98));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e95 = (*role);
    let _e97 = (*role);
    let _e102 = unnamed.packed_indices[(_e95 / 4u)][(_e97 % 4u)];
    let _e107 = (*uv);
    let _e108 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e102 >> bitcast<u32>(12i)) & 255u)], _e107);
    c_1 = _e108;
    let _e109 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e109))) == 0i) {
        let _e114 = c_1;
        param = _e114.xyz;
        let _e116 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = (*slot);
    if (lightmap_slot == (_e123 + 1i)) {
        let _e128 = unnamed.worldLightParams[0u];
        let _e129 = c_1;
        let _e131 = (_e129.xyz * _e128);
        c_1[0u] = _e131.x;
        c_1[1u] = _e131.y;
        c_1[2u] = _e131.z;
    }
    let _e138 = c_1;
    return _e138;
}

fn main_1() {
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
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
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
    var param_23: f32;
    var param_24: vec3<f32>;
    var param_25: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e135 = frag_color0In_1;
    param_1 = _e135.xyz;
    let _e137 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e139 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e137.x, _e137.y, _e137.z, _e139);
    param_2 = 0u;
    let _e144 = frag_tex_coord0_1;
    param_3 = _e144;
    param_4 = 0i;
    let _e145 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e146 = frag_color0_;
    color0_ = (_e145 * _e146);
    if override_type_3_ {
        param_5 = 1u;
        let _e148 = frag_tex_coord1_1;
        param_6 = _e148;
        param_7 = 1i;
        let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e149;
        param_8 = 2u;
        let _e150 = frag_tex_coord2_1;
        param_9 = _e150;
        param_10 = 2i;
        let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e151;
        let _e152 = color0_;
        let _e154 = color1_;
        let _e157 = color2_;
        let _e159 = ((_e152.xyz + _e154.xyz) + _e157.xyz);
        let _e161 = color0_[3u];
        let _e163 = color1_[3u];
        let _e166 = color2_[3u];
        base = vec4<f32>(_e159.x, _e159.y, _e159.z, ((_e161 * _e163) * _e166));
    } else {
        if override_type_3_1 {
            param_11 = 1u;
            let _e172 = frag_tex_coord1_1;
            param_12 = _e172;
            param_13 = 1i;
            let _e173 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e174 = frag_color0_;
            color1_1 = (_e173 * _e174);
            param_14 = 2u;
            let _e176 = frag_tex_coord2_1;
            param_15 = _e176;
            param_16 = 2i;
            let _e177 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e178 = frag_color0_;
            color2_1 = (_e177 * _e178);
            let _e180 = color0_;
            let _e182 = color1_1;
            let _e185 = color2_1;
            let _e187 = ((_e180.xyz + _e182.xyz) + _e185.xyz);
            let _e189 = color0_[3u];
            let _e191 = color1_1[3u];
            let _e194 = color2_1[3u];
            base = vec4<f32>(_e187.x, _e187.y, _e187.z, ((_e189 * _e191) * _e194));
        } else {
            param_17 = 1u;
            let _e200 = frag_tex_coord1_1;
            param_18 = _e200;
            param_19 = 1i;
            let _e201 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e201;
            param_20 = 2u;
            let _e202 = frag_tex_coord2_1;
            param_21 = _e202;
            param_22 = 2i;
            let _e203 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e203;
            let _e204 = color0_;
            let _e206 = color1_2;
            let _e209 = color2_2;
            let _e211 = ((_e204.xyz * _e206.xyz) * _e209.xyz);
            base[0u] = _e211.x;
            base[1u] = _e211.y;
            base[2u] = _e211.z;
            let _e219 = color0_[3u];
            let _e221 = color1_2[3u];
            let _e224 = color2_2[3u];
            base[3u] = ((_e219 * _e221) * _e224);
        }
    }
    if override_type_3_2 {
        let _e229 = unnamed.worldLightParams[1u];
        wetness = clamp(_e229, 0f, 1f);
        let _e233 = unnamed.worldLightParams[2u];
        frost = clamp(_e233, 0f, 1f);
        let _e235 = base;
        luminance = dot(_e235.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e238 = wetness;
        let _e240 = base;
        let _e242 = (_e240.xyz * mix(1f, 0.82f, _e238));
        base[0u] = _e242.x;
        base[1u] = _e242.y;
        base[2u] = _e242.z;
        let _e249 = base;
        let _e251 = luminance;
        let _e253 = luminance;
        let _e255 = luminance;
        let _e257 = frost;
        let _e260 = mix(_e249.xyz, vec3<f32>((_e251 * 0.88f), (_e253 * 0.94f), _e255), vec3((_e257 * 0.55f)));
        base[0u] = _e260.x;
        base[1u] = _e260.y;
        base[2u] = _e260.z;
        let _e267 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e267).z);
        let _e273 = unnamed.worldLightParams[3u];
        let _e275 = upward;
        snowCoverage = (clamp(_e273, 0f, 1f) * _e275);
        let _e277 = base;
        let _e279 = snowCoverage;
        let _e282 = mix(_e277.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e279 * 0.82f)));
        base[0u] = _e282.x;
        base[1u] = _e282.y;
        base[2u] = _e282.z;
    }
    let _e289 = color0_;
    let _e292 = unnamed.emissionRadiance;
    let _e295 = base;
    let _e297 = (_e295.xyz + (_e289.xyz * _e292.xyz));
    base[0u] = _e297.x;
    base[1u] = _e297.y;
    base[2u] = _e297.z;
    if override_type_3_3 {
        let _e307 = unnamed.packed_indices[2i][0u];
        let _e313 = unnamed.packed_indices[2i][0u];
        let _e318 = frag_tex_coord0_1;
        let _e319 = textureSample(wired_bindless_images[(_e307 & 4095u)], wired_bindless_samplers[((_e313 >> bitcast<u32>(12i)) & 255u)], _e318);
        orm = _e319.xyz;
        let _e322 = orm[0u];
        ao = _e322;
        let _e324 = orm[1u];
        let _e327 = unnamed.worldLightParams[1u];
        let _e332 = unnamed.worldLightParams[2u];
        let _e337 = unnamed.worldLightParams[3u];
        roughness_1 = clamp((((_e324 - (0.35f * _e327)) + (0.3f * _e332)) + (0.4f * _e337)), 0.04f, 1f);
        let _e342 = orm[2u];
        metalness = _e342;
        let _e343 = ibl_N_1;
        ibl_n = normalize(_e343);
        let _e345 = ibl_V_1;
        ibl_v = normalize(_e345);
        let _e347 = base;
        let _e349 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e347.xyz, vec3(_e349));
        let _e352 = ibl_n;
        let _e353 = ibl_v;
        NdotV = max(dot(_e352, _e353), 0f);
        let _e356 = NdotV;
        param_23 = _e356;
        let _e357 = F0_1;
        param_24 = _e357;
        let _e358 = roughness_1;
        param_25 = _e358;
        let _e359 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_23), (&param_24), (&param_25));
        F_amb = _e359;
        let _e360 = ibl_v;
        let _e362 = ibl_n;
        R = reflect(-(_e360), _e362);
        let _e364 = R;
        let _e365 = roughness_1;
        let _e367 = textureSampleLevel(radianceCube, radianceCube_sampler, _e364, (_e365 * 5f));
        prefiltered = _e367.xyz;
        let _e369 = NdotV;
        let _e370 = roughness_1;
        let _e372 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e369, _e370), 0f);
        envBRDF = _e372.xy;
        let _e374 = prefiltered;
        let _e375 = F_amb;
        let _e377 = envBRDF[0u];
        let _e380 = envBRDF[1u];
        let _e384 = ao;
        specularIBL = ((_e374 * ((_e375 * _e377) + vec3(_e380))) * _e384);
        let _e386 = gl_FragCoord_1;
        let _e388 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e386.xy / vec2<f32>(vec2<i32>(_e388)));
        let _e392 = gtaoUV;
        let _e393 = textureSample(gtaoMap, gtaoMap_sampler, _e392);
        gtao_vis = _e393.x;
        let _e395 = gtao_vis;
        let _e396 = specularIBL;
        specularIBL = (_e396 * _e395);
        let _e398 = specularIBL;
        let _e399 = base;
        let _e401 = (_e399.xyz + _e398);
        base[0u] = _e401.x;
        base[1u] = _e401.y;
        base[2u] = _e401.z;
    }
    let _e408 = wired_advanced_fog_enabled_u0028_();
    if _e408 {
        let _e409 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e409;
        let _e410 = base;
        let _e413 = unnamed.advancedFogColorDensity;
        let _e415 = fogAmount;
        let _e417 = mix(_e410.xyz, _e413.xyz, vec3(_e415));
        base[0u] = _e417.x;
        base[1u] = _e417.y;
        base[2u] = _e417.z;
    }
    if override_type_3_4 {
        let _e425 = base[3u];
        if (_e425 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e427 = base;
            let _e429 = base;
            if (dot(_e427.xyz, _e429.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e433 = base;
    out_color = _e433;
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
