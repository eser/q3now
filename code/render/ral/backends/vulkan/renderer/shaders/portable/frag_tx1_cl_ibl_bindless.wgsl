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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
override override_type_3_8: bool = (lightmap_slot != 0i);
@id(14) override ibl_enabled: i32 = 0i;
override override_type_3_9: bool = (ibl_enabled != 0i);
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
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var color0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var color1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var color1_2: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_3: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color1_4: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color1_5: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color1_6: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
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
    var param_28: f32;
    var param_29: vec3<f32>;
    var param_30: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e151 = frag_color0In_1;
    param_2 = _e151.xyz;
    let _e153 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e155 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e153.x, _e153.y, _e153.z, _e155);
    let _e160 = frag_color1In_1;
    param_3 = _e160.xyz;
    let _e162 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e164 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e162.x, _e162.y, _e162.z, _e164);
    param_4 = 0u;
    let _e169 = frag_tex_coord0_1;
    param_5 = _e169;
    param_6 = 0i;
    let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e171 = frag_color0_;
    color0_ = (_e170 * _e171);
    if override_type_3_2 {
        param_7 = 1u;
        let _e173 = frag_tex_coord1_1;
        param_8 = _e173;
        param_9 = 1i;
        let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e175 = frag_color1_;
        color1_ = (_e174 * _e175);
        let _e177 = color0_;
        let _e179 = color1_;
        let _e181 = (_e177.xyz + _e179.xyz);
        let _e183 = color0_[3u];
        let _e185 = color1_[3u];
        base = vec4<f32>(_e181.x, _e181.y, _e181.z, (_e183 * _e185));
    } else {
        if override_type_3_3 {
            param_10 = 1u;
            let _e191 = frag_tex_coord1_1;
            param_11 = _e191;
            param_12 = 1i;
            let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e193 = frag_color1_;
            color1_1 = (_e192 * _e193);
            let _e196 = color0_[3u];
            let _e197 = color0_;
            color0_ = (_e197 * _e196);
            let _e200 = color1_1[3u];
            let _e201 = color1_1;
            color1_1 = (_e201 * _e200);
            let _e203 = color0_;
            let _e205 = color1_1;
            let _e207 = (_e203.xyz + _e205.xyz);
            let _e209 = color0_[3u];
            let _e211 = color1_1[3u];
            base = vec4<f32>(_e207.x, _e207.y, _e207.z, (_e209 * _e211));
        } else {
            if override_type_3_4 {
                param_13 = 1u;
                let _e217 = frag_tex_coord1_1;
                param_14 = _e217;
                param_15 = 1i;
                let _e218 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
                let _e219 = frag_color1_;
                color1_2 = (_e218 * _e219);
                let _e222 = color0_[3u];
                let _e224 = color0_;
                color0_ = (_e224 * (1f - _e222));
                let _e227 = color1_2[3u];
                let _e229 = color1_2;
                color1_2 = (_e229 * (1f - _e227));
                let _e231 = color0_;
                let _e233 = color1_2;
                let _e235 = (_e231.xyz + _e233.xyz);
                let _e237 = color0_[3u];
                let _e239 = color1_2[3u];
                base = vec4<f32>(_e235.x, _e235.y, _e235.z, (_e237 * _e239));
            } else {
                if override_type_3_5 {
                    param_16 = 1u;
                    let _e245 = frag_tex_coord1_1;
                    param_17 = _e245;
                    param_18 = 1i;
                    let _e246 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
                    let _e247 = frag_color1_;
                    color1_3 = (_e246 * _e247);
                    let _e249 = color0_;
                    let _e250 = color1_3;
                    let _e252 = color1_3[3u];
                    base = mix(_e249, _e250, vec4(_e252));
                } else {
                    if override_type_3_6 {
                        param_19 = 1u;
                        let _e255 = frag_tex_coord1_1;
                        param_20 = _e255;
                        param_21 = 1i;
                        let _e256 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                        let _e257 = frag_color1_;
                        color1_4 = (_e256 * _e257);
                        let _e259 = color1_4;
                        let _e260 = color0_;
                        let _e262 = color1_4[3u];
                        base = mix(_e259, _e260, vec4(_e262));
                    } else {
                        if override_type_3_7 {
                            param_22 = 1u;
                            let _e265 = frag_tex_coord1_1;
                            param_23 = _e265;
                            param_24 = 1i;
                            let _e266 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                            let _e267 = frag_color1_;
                            color1_5 = (_e266 * _e267);
                            let _e269 = color1_5;
                            let _e271 = color1_5[3u];
                            let _e274 = color0_;
                            base = ((_e269 + vec4(_e271)) * _e274);
                        } else {
                            param_25 = 1u;
                            let _e276 = frag_tex_coord1_1;
                            param_26 = _e276;
                            param_27 = 1i;
                            let _e277 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                            let _e278 = frag_color1_;
                            color1_6 = (_e277 * _e278);
                            let _e280 = color0_;
                            let _e282 = color1_6;
                            let _e284 = (_e280.xyz * _e282.xyz);
                            base[0u] = _e284.x;
                            base[1u] = _e284.y;
                            base[2u] = _e284.z;
                            let _e292 = color0_[3u];
                            let _e294 = color1_6[3u];
                            base[3u] = (_e292 * _e294);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e299 = unnamed.worldLightParams[1u];
        wetness = clamp(_e299, 0f, 1f);
        let _e303 = unnamed.worldLightParams[2u];
        frost = clamp(_e303, 0f, 1f);
        let _e305 = base;
        luminance = dot(_e305.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e308 = wetness;
        let _e310 = base;
        let _e312 = (_e310.xyz * mix(1f, 0.82f, _e308));
        base[0u] = _e312.x;
        base[1u] = _e312.y;
        base[2u] = _e312.z;
        let _e319 = base;
        let _e321 = luminance;
        let _e323 = luminance;
        let _e325 = luminance;
        let _e327 = frost;
        let _e330 = mix(_e319.xyz, vec3<f32>((_e321 * 0.88f), (_e323 * 0.94f), _e325), vec3((_e327 * 0.55f)));
        base[0u] = _e330.x;
        base[1u] = _e330.y;
        base[2u] = _e330.z;
        let _e337 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e337).z);
        let _e343 = unnamed.worldLightParams[3u];
        let _e345 = upward;
        snowCoverage = (clamp(_e343, 0f, 1f) * _e345);
        let _e347 = base;
        let _e349 = snowCoverage;
        let _e352 = mix(_e347.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e349 * 0.82f)));
        base[0u] = _e352.x;
        base[1u] = _e352.y;
        base[2u] = _e352.z;
    }
    let _e359 = color0_;
    let _e362 = unnamed.emissionRadiance;
    let _e365 = base;
    let _e367 = (_e365.xyz + (_e359.xyz * _e362.xyz));
    base[0u] = _e367.x;
    base[1u] = _e367.y;
    base[2u] = _e367.z;
    if override_type_3_9 {
        let _e377 = unnamed.packed_indices[2i][0u];
        let _e383 = unnamed.packed_indices[2i][0u];
        let _e388 = frag_tex_coord0_1;
        let _e389 = textureSample(wired_bindless_images[(_e377 & 4095u)], wired_bindless_samplers[((_e383 >> bitcast<u32>(12i)) & 255u)], _e388);
        orm = _e389.xyz;
        let _e392 = orm[0u];
        ao = _e392;
        let _e394 = orm[1u];
        let _e397 = unnamed.worldLightParams[1u];
        let _e402 = unnamed.worldLightParams[2u];
        let _e407 = unnamed.worldLightParams[3u];
        roughness_1 = clamp((((_e394 - (0.35f * _e397)) + (0.3f * _e402)) + (0.4f * _e407)), 0.04f, 1f);
        let _e412 = orm[2u];
        metalness = _e412;
        let _e413 = ibl_N_1;
        ibl_n = normalize(_e413);
        let _e415 = ibl_V_1;
        ibl_v = normalize(_e415);
        let _e417 = base;
        let _e419 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e417.xyz, vec3(_e419));
        let _e422 = ibl_n;
        let _e423 = ibl_v;
        NdotV = max(dot(_e422, _e423), 0f);
        let _e426 = NdotV;
        param_28 = _e426;
        let _e427 = F0_1;
        param_29 = _e427;
        let _e428 = roughness_1;
        param_30 = _e428;
        let _e429 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_28), (&param_29), (&param_30));
        F_amb = _e429;
        let _e430 = ibl_v;
        let _e432 = ibl_n;
        R = reflect(-(_e430), _e432);
        let _e434 = R;
        let _e435 = roughness_1;
        let _e437 = textureSampleLevel(radianceCube, radianceCube_sampler, _e434, (_e435 * 5f));
        prefiltered = _e437.xyz;
        let _e439 = NdotV;
        let _e440 = roughness_1;
        let _e442 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e439, _e440), 0f);
        envBRDF = _e442.xy;
        let _e444 = prefiltered;
        let _e445 = F_amb;
        let _e447 = envBRDF[0u];
        let _e450 = envBRDF[1u];
        let _e454 = ao;
        specularIBL = ((_e444 * ((_e445 * _e447) + vec3(_e450))) * _e454);
        let _e456 = gl_FragCoord_1;
        let _e458 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e456.xy / vec2<f32>(vec2<i32>(_e458)));
        let _e462 = gtaoUV;
        let _e463 = textureSample(gtaoMap, gtaoMap_sampler, _e462);
        gtao_vis = _e463.x;
        let _e465 = gtao_vis;
        let _e466 = specularIBL;
        specularIBL = (_e466 * _e465);
        let _e468 = specularIBL;
        let _e469 = base;
        let _e471 = (_e469.xyz + _e468);
        base[0u] = _e471.x;
        base[1u] = _e471.y;
        base[2u] = _e471.z;
    }
    let _e478 = wired_advanced_fog_enabled_u0028_();
    if _e478 {
        let _e479 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e479;
        let _e480 = base;
        let _e483 = unnamed.advancedFogColorDensity;
        let _e485 = fogAmount;
        let _e487 = mix(_e480.xyz, _e483.xyz, vec3(_e485));
        base[0u] = _e487.x;
        base[1u] = _e487.y;
        base[2u] = _e487.z;
    }
    if override_type_3_10 {
        let _e495 = base[3u];
        if (_e495 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e497 = base;
            let _e499 = base;
            if (dot(_e497.xyz, _e499.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e503 = base;
    out_color = _e503;
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
