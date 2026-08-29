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

    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e82 + 0.5f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e89 = fogType;
    let _e92 = fogType;
    return (((_e87 > 0.5f) && (_e89 >= 1i)) && (_e92 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e82 = wired_advanced_fog_enabled_u0028_();
    if !(_e82) {
        return 0f;
    }
    let _e85 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e85, 0.000001f));
    let _e90 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e90 + 0.5f));
    let _e93 = fogType_1;
    if (_e93 == 1i) {
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e97 <= 0f) {
            return 0f;
        }
        let _e99 = viewDepth;
        let _e102 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e99 / _e102), 0f, 1f);
    }
    let _e107 = unnamed.advancedFogColorDensity[3u];
    let _e109 = viewDepth;
    opticalDepth = (max(_e107, 0f) * _e109);
    let _e111 = fogType_1;
    if (_e111 == 2i) {
        let _e113 = opticalDepth;
        return clamp((1f - exp(-(_e113))), 0f, 1f);
    }
    let _e118 = opticalDepth;
    let _e119 = opticalDepth;
    return clamp((1f - exp(-((_e118 * _e119)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e85 = (*cosTheta);
    t = (1f - _e85);
    let _e87 = t;
    let _e88 = t;
    t2_ = (_e87 * _e88);
    let _e90 = (*roughness);
    let _e93 = (*F0_);
    Fmax = max(vec3((1f - _e90)), _e93);
    let _e95 = (*F0_);
    let _e96 = Fmax;
    let _e97 = (*F0_);
    let _e99 = t2_;
    let _e100 = t2_;
    let _e102 = t;
    return (_e95 + ((_e96 - _e97) * ((_e99 * _e100) * _e102)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e83 = (*c);
    (*c) = max(_e83, vec3<f32>(0f, 0f, 0f));
    let _e85 = (*c);
    cutoff = (_e85 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e87 = (*c);
    lo = (_e87 / vec3(12.92f));
    let _e90 = (*c);
    hi = pow(((_e90 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e95 = hi;
    let _e96 = lo;
    let _e97 = cutoff;
    return mix(_e95, _e96, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e97));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e94 = (*role);
    let _e96 = (*role);
    let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
    let _e106 = (*uv);
    let _e107 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e101 >> bitcast<u32>(12i)) & 255u)], _e106);
    c_1 = _e107;
    let _e108 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e108))) == 0i) {
        let _e113 = c_1;
        param = _e113.xyz;
        let _e115 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = (*slot);
    if (lightmap_slot == (_e122 + 1i)) {
        let _e127 = unnamed.worldLightParams[0u];
        let _e128 = c_1;
        let _e130 = (_e128.xyz * _e127);
        c_1[0u] = _e130.x;
        c_1[1u] = _e130.y;
        c_1[2u] = _e130.z;
    }
    let _e137 = c_1;
    return _e137;
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

    let _e122 = frag_color0In_1;
    param_1 = _e122.xyz;
    let _e124 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e126 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e124.x, _e124.y, _e124.z, _e126);
    param_2 = 0u;
    let _e131 = frag_tex_coord0_1;
    param_3 = _e131;
    param_4 = 0i;
    let _e132 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e133 = frag_color0_;
    color0_ = (_e132 * _e133);
    if override_type_3_ {
        param_5 = 1u;
        let _e135 = frag_tex_coord1_1;
        param_6 = _e135;
        param_7 = 1i;
        let _e136 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e136;
        let _e137 = color0_;
        let _e139 = color1_;
        let _e141 = (_e137.xyz + _e139.xyz);
        let _e143 = color0_[3u];
        let _e145 = color1_[3u];
        base = vec4<f32>(_e141.x, _e141.y, _e141.z, (_e143 * _e145));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e151 = frag_tex_coord1_1;
            param_9 = _e151;
            param_10 = 1i;
            let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e153 = frag_color0_;
            color1_1 = (_e152 * _e153);
            let _e155 = color0_;
            let _e157 = color1_1;
            let _e159 = (_e155.xyz + _e157.xyz);
            let _e161 = color0_[3u];
            let _e163 = color1_1[3u];
            base = vec4<f32>(_e159.x, _e159.y, _e159.z, (_e161 * _e163));
        } else {
            param_11 = 1u;
            let _e169 = frag_tex_coord1_1;
            param_12 = _e169;
            param_13 = 1i;
            let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e170;
            let _e171 = color0_;
            let _e173 = color1_2;
            let _e175 = (_e171.xyz * _e173.xyz);
            base[0u] = _e175.x;
            base[1u] = _e175.y;
            base[2u] = _e175.z;
            let _e183 = color0_[3u];
            let _e185 = color1_2[3u];
            base[3u] = (_e183 * _e185);
        }
    }
    if override_type_3_2 {
        let _e190 = unnamed.worldLightParams[1u];
        wetness = clamp(_e190, 0f, 1f);
        let _e194 = unnamed.worldLightParams[2u];
        frost = clamp(_e194, 0f, 1f);
        let _e196 = base;
        luminance = dot(_e196.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e199 = wetness;
        let _e201 = base;
        let _e203 = (_e201.xyz * mix(1f, 0.82f, _e199));
        base[0u] = _e203.x;
        base[1u] = _e203.y;
        base[2u] = _e203.z;
        let _e210 = base;
        let _e212 = luminance;
        let _e214 = luminance;
        let _e216 = luminance;
        let _e218 = frost;
        let _e221 = mix(_e210.xyz, vec3<f32>((_e212 * 0.88f), (_e214 * 0.94f), _e216), vec3((_e218 * 0.55f)));
        base[0u] = _e221.x;
        base[1u] = _e221.y;
        base[2u] = _e221.z;
        let _e228 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e228).z);
        let _e234 = unnamed.worldLightParams[3u];
        let _e236 = upward;
        snowCoverage = (clamp(_e234, 0f, 1f) * _e236);
        let _e238 = base;
        let _e240 = snowCoverage;
        let _e243 = mix(_e238.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e240 * 0.82f)));
        base[0u] = _e243.x;
        base[1u] = _e243.y;
        base[2u] = _e243.z;
    }
    let _e250 = color0_;
    let _e253 = unnamed.emissionRadiance;
    let _e256 = base;
    let _e258 = (_e256.xyz + (_e250.xyz * _e253.xyz));
    base[0u] = _e258.x;
    base[1u] = _e258.y;
    base[2u] = _e258.z;
    if override_type_3_3 {
        let _e268 = unnamed.packed_indices[2i][0u];
        let _e274 = unnamed.packed_indices[2i][0u];
        let _e279 = frag_tex_coord0_1;
        let _e280 = textureSample(wired_bindless_images[(_e268 & 4095u)], wired_bindless_samplers[((_e274 >> bitcast<u32>(12i)) & 255u)], _e279);
        orm = _e280.xyz;
        let _e283 = orm[0u];
        ao = _e283;
        let _e285 = orm[1u];
        let _e288 = unnamed.worldLightParams[1u];
        let _e293 = unnamed.worldLightParams[2u];
        let _e298 = unnamed.worldLightParams[3u];
        roughness_1 = clamp((((_e285 - (0.35f * _e288)) + (0.3f * _e293)) + (0.4f * _e298)), 0.04f, 1f);
        let _e303 = orm[2u];
        metalness = _e303;
        let _e304 = ibl_N_1;
        ibl_n = normalize(_e304);
        let _e306 = ibl_V_1;
        ibl_v = normalize(_e306);
        let _e308 = base;
        let _e310 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e308.xyz, vec3(_e310));
        let _e313 = ibl_n;
        let _e314 = ibl_v;
        NdotV = max(dot(_e313, _e314), 0f);
        let _e317 = NdotV;
        param_14 = _e317;
        let _e318 = F0_1;
        param_15 = _e318;
        let _e319 = roughness_1;
        param_16 = _e319;
        let _e320 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        F_amb = _e320;
        let _e321 = ibl_v;
        let _e323 = ibl_n;
        R = reflect(-(_e321), _e323);
        let _e325 = R;
        let _e326 = roughness_1;
        let _e328 = textureSampleLevel(radianceCube, radianceCube_sampler, _e325, (_e326 * 5f));
        prefiltered = _e328.xyz;
        let _e330 = NdotV;
        let _e331 = roughness_1;
        let _e333 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e330, _e331), 0f);
        envBRDF = _e333.xy;
        let _e335 = prefiltered;
        let _e336 = F_amb;
        let _e338 = envBRDF[0u];
        let _e341 = envBRDF[1u];
        let _e345 = ao;
        specularIBL = ((_e335 * ((_e336 * _e338) + vec3(_e341))) * _e345);
        let _e347 = gl_FragCoord_1;
        let _e349 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e347.xy / vec2<f32>(vec2<i32>(_e349)));
        let _e353 = gtaoUV;
        let _e354 = textureSample(gtaoMap, gtaoMap_sampler, _e353);
        gtao_vis = _e354.x;
        let _e356 = gtao_vis;
        let _e357 = specularIBL;
        specularIBL = (_e357 * _e356);
        let _e359 = specularIBL;
        let _e360 = base;
        let _e362 = (_e360.xyz + _e359);
        base[0u] = _e362.x;
        base[1u] = _e362.y;
        base[2u] = _e362.z;
    }
    let _e369 = wired_advanced_fog_enabled_u0028_();
    if _e369 {
        let _e370 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e370;
        let _e371 = base;
        let _e374 = unnamed.advancedFogColorDensity;
        let _e376 = fogAmount;
        let _e378 = mix(_e371.xyz, _e374.xyz, vec3(_e376));
        base[0u] = _e378.x;
        base[1u] = _e378.y;
        base[2u] = _e378.z;
    }
    if override_type_3_4 {
        let _e386 = base[3u];
        if (_e386 == 0f) {
            discard;
        }
    } else {
        if override_type_3_5 {
            let _e388 = base;
            let _e390 = base;
            if (dot(_e388.xyz, _e390.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e394 = base;
    out_color = _e394;
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
