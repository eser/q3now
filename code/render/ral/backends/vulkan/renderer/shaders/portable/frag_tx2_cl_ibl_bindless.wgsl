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
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
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

    let _e98 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e98 + 0.5f));
    let _e103 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e105 = fogType;
    let _e108 = fogType;
    return (((_e103 > 0.5f) && (_e105 >= 1i)) && (_e108 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e98 = wired_advanced_fog_enabled_u0028_();
    if !(_e98) {
        return 0f;
    }
    let _e101 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e101, 0.000001f));
    let _e106 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e106 + 0.5f));
    let _e109 = fogType_1;
    if (_e109 == 1i) {
        let _e113 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e113 <= 0f) {
            return 0f;
        }
        let _e115 = viewDepth;
        let _e118 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e115 / _e118), 0f, 1f);
    }
    let _e123 = unnamed.advancedFogColorDensity[3u];
    let _e125 = viewDepth;
    opticalDepth = (max(_e123, 0f) * _e125);
    let _e127 = fogType_1;
    if (_e127 == 2i) {
        let _e129 = opticalDepth;
        return clamp((1f - exp(-(_e129))), 0f, 1f);
    }
    let _e134 = opticalDepth;
    let _e135 = opticalDepth;
    return clamp((1f - exp(-((_e134 * _e135)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e101 = (*cosTheta);
    t = (1f - _e101);
    let _e103 = t;
    let _e104 = t;
    t2_ = (_e103 * _e104);
    let _e106 = (*roughness);
    let _e109 = (*F0_);
    Fmax = max(vec3((1f - _e106)), _e109);
    let _e111 = (*F0_);
    let _e112 = Fmax;
    let _e113 = (*F0_);
    let _e115 = t2_;
    let _e116 = t2_;
    let _e118 = t;
    return (_e111 + ((_e112 - _e113) * ((_e115 * _e116) * _e118)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e98 = (*rgb);
    let _e101 = unnamed.worldLightParams[0u];
    boosted = (_e98 * _e101);
    let _e104 = boosted[0u];
    let _e106 = boosted[1u];
    let _e108 = boosted[2u];
    peak = max(_e104, max(_e106, _e108));
    let _e111 = peak;
    if (_e111 > 1f) {
        let _e113 = peak;
        let _e114 = boosted;
        boosted = (_e114 / vec3(_e113));
    }
    let _e117 = boosted;
    return _e117;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e99 = (*c);
    (*c) = max(_e99, vec3<f32>(0f, 0f, 0f));
    let _e101 = (*c);
    cutoff = (_e101 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e103 = (*c);
    lo = (_e103 / vec3(12.92f));
    let _e106 = (*c);
    hi = pow(((_e106 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e111 = hi;
    let _e112 = lo;
    let _e113 = cutoff;
    return mix(_e111, _e112, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e113));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e101 = (*role);
    let _e103 = (*role);
    let _e108 = unnamed.packed_indices[(_e101 / 4u)][(_e103 % 4u)];
    let _e111 = (*role);
    let _e113 = (*role);
    let _e118 = unnamed.packed_indices[(_e111 / 4u)][(_e113 % 4u)];
    let _e123 = (*uv);
    let _e124 = textureSample(wired_bindless_images[(_e108 & 4095u)], wired_bindless_samplers[((_e118 >> bitcast<u32>(12i)) & 255u)], _e123);
    c_1 = _e124;
    let _e125 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e125))) == 0i) {
        let _e130 = c_1;
        param = _e130.xyz;
        let _e132 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e132.x;
        c_1[1u] = _e132.y;
        c_1[2u] = _e132.z;
    }
    let _e139 = (*slot);
    if (lightmap_slot == (_e139 + 1i)) {
        let _e142 = c_1;
        param_1 = _e142.xyz;
        let _e144 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e144.x;
        c_1[1u] = _e144.y;
        c_1[2u] = _e144.z;
    }
    let _e151 = c_1;
    return _e151;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_4: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var sampledColor1_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var sampledColor2_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var materialAlbedo: vec3<f32>;
    var local: vec3<f32>;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color2_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color2_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color2_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var color2_6: vec4<f32>;
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
    var param_14: f32;
    var param_15: vec3<f32>;
    var param_16: f32;
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

    let _e159 = frag_color0In_1;
    param_2 = _e159.xyz;
    let _e161 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e163 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e161.x, _e161.y, _e161.z, _e163);
    let _e168 = frag_color1In_1;
    param_3 = _e168.xyz;
    let _e170 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e172 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e170.x, _e170.y, _e170.z, _e172);
    let _e177 = frag_color2In_1;
    param_4 = _e177.xyz;
    let _e179 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e181 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e179.x, _e179.y, _e179.z, _e181);
    param_5 = 0u;
    let _e186 = frag_tex_coord0_1;
    param_6 = _e186;
    param_7 = 0i;
    let _e187 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor0_ = _e187;
    param_8 = 1u;
    let _e188 = frag_tex_coord1_1;
    param_9 = _e188;
    param_10 = 1i;
    let _e189 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor1_ = _e189;
    param_11 = 2u;
    let _e190 = frag_tex_coord2_1;
    param_12 = _e190;
    param_13 = 2i;
    let _e191 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor2_ = _e191;
    if override_type_3_ {
        let _e192 = sampledColor1_;
        local = _e192.xyz;
    } else {
        let _e194 = sampledColor0_;
        local = _e194.xyz;
    }
    let _e196 = local;
    materialAlbedo = _e196;
    let _e197 = sampledColor0_;
    let _e198 = frag_color0_;
    color0_ = (_e197 * _e198);
    if override_type_3_3 {
        let _e200 = sampledColor1_;
        let _e201 = frag_color1_;
        color1_ = (_e200 * _e201);
        let _e203 = sampledColor2_;
        let _e204 = frag_color2_;
        color2_ = (_e203 * _e204);
        let _e206 = color0_;
        let _e208 = color1_;
        let _e211 = color2_;
        let _e213 = ((_e206.xyz + _e208.xyz) + _e211.xyz);
        let _e215 = color0_[3u];
        let _e217 = color1_[3u];
        let _e220 = color2_[3u];
        base = vec4<f32>(_e213.x, _e213.y, _e213.z, ((_e215 * _e217) * _e220));
    } else {
        if override_type_3_4 {
            let _e226 = sampledColor1_;
            let _e227 = frag_color1_;
            color1_1 = (_e226 * _e227);
            let _e229 = sampledColor2_;
            let _e230 = frag_color2_;
            color2_1 = (_e229 * _e230);
            let _e233 = color0_[3u];
            let _e234 = color0_;
            color0_ = (_e234 * _e233);
            let _e237 = color1_1[3u];
            let _e238 = color1_1;
            color1_1 = (_e238 * _e237);
            let _e241 = color2_1[3u];
            let _e242 = color2_1;
            color2_1 = (_e242 * _e241);
            let _e244 = color0_;
            let _e246 = color1_1;
            let _e249 = color2_1;
            let _e251 = ((_e244.xyz + _e246.xyz) + _e249.xyz);
            let _e253 = color0_[3u];
            let _e255 = color1_1[3u];
            let _e258 = color2_1[3u];
            base = vec4<f32>(_e251.x, _e251.y, _e251.z, ((_e253 * _e255) * _e258));
        } else {
            if override_type_3_5 {
                let _e264 = sampledColor1_;
                let _e265 = frag_color1_;
                color1_2 = (_e264 * _e265);
                let _e267 = sampledColor2_;
                let _e268 = frag_color2_;
                color2_2 = (_e267 * _e268);
                let _e271 = color0_[3u];
                let _e273 = color0_;
                color0_ = (_e273 * (1f - _e271));
                let _e276 = color1_2[3u];
                let _e278 = color1_2;
                color1_2 = (_e278 * (1f - _e276));
                let _e281 = color2_2[3u];
                let _e283 = color2_2;
                color2_2 = (_e283 * (1f - _e281));
                let _e285 = color0_;
                let _e287 = color1_2;
                let _e290 = color2_2;
                let _e292 = ((_e285.xyz + _e287.xyz) + _e290.xyz);
                let _e294 = color0_[3u];
                let _e296 = color1_2[3u];
                let _e299 = color2_2[3u];
                base = vec4<f32>(_e292.x, _e292.y, _e292.z, ((_e294 * _e296) * _e299));
            } else {
                if override_type_3_6 {
                    let _e305 = sampledColor1_;
                    let _e306 = frag_color1_;
                    color1_3 = (_e305 * _e306);
                    let _e308 = sampledColor2_;
                    let _e309 = frag_color2_;
                    color2_3 = (_e308 * _e309);
                    let _e311 = color0_;
                    let _e312 = color1_3;
                    let _e314 = color1_3[3u];
                    let _e317 = color2_3;
                    let _e319 = color2_3[3u];
                    base = mix(mix(_e311, _e312, vec4(_e314)), _e317, vec4(_e319));
                } else {
                    if override_type_3_7 {
                        let _e322 = sampledColor1_;
                        let _e323 = frag_color1_;
                        color1_4 = (_e322 * _e323);
                        let _e325 = sampledColor2_;
                        let _e326 = frag_color2_;
                        color2_4 = (_e325 * _e326);
                        let _e328 = color2_4;
                        let _e329 = color1_4;
                        let _e330 = color0_;
                        let _e332 = color1_4[3u];
                        let _e336 = color2_4[3u];
                        base = mix(_e328, mix(_e329, _e330, vec4(_e332)), vec4(_e336));
                    } else {
                        if override_type_3_8 {
                            let _e339 = sampledColor1_;
                            let _e340 = frag_color1_;
                            color1_5 = (_e339 * _e340);
                            let _e342 = sampledColor2_;
                            let _e343 = frag_color2_;
                            color2_5 = (_e342 * _e343);
                            let _e345 = color2_5;
                            let _e347 = color2_5[3u];
                            let _e350 = color1_5;
                            let _e352 = color1_5[3u];
                            let _e356 = color0_;
                            base = (((_e345 + vec4(_e347)) * (_e350 + vec4(_e352))) * _e356);
                        } else {
                            let _e358 = sampledColor1_;
                            let _e359 = frag_color1_;
                            color1_6 = (_e358 * _e359);
                            let _e361 = sampledColor2_;
                            let _e362 = frag_color2_;
                            color2_6 = (_e361 * _e362);
                            let _e364 = color0_;
                            let _e366 = color1_6;
                            let _e369 = color2_6;
                            let _e371 = ((_e364.xyz * _e366.xyz) * _e369.xyz);
                            base[0u] = _e371.x;
                            base[1u] = _e371.y;
                            base[2u] = _e371.z;
                            let _e379 = color0_[3u];
                            let _e381 = color1_6[3u];
                            let _e384 = color2_6[3u];
                            base[3u] = ((_e379 * _e381) * _e384);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_9 {
        let _e389 = unnamed.worldLightParams[1u];
        wetness = clamp(_e389, 0f, 1f);
        let _e393 = unnamed.worldLightParams[2u];
        frost = clamp(_e393, 0f, 1f);
        let _e395 = base;
        luminance = dot(_e395.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e398 = wetness;
        let _e400 = base;
        let _e402 = (_e400.xyz * mix(1f, 0.82f, _e398));
        base[0u] = _e402.x;
        base[1u] = _e402.y;
        base[2u] = _e402.z;
        let _e409 = base;
        let _e411 = luminance;
        let _e413 = luminance;
        let _e415 = luminance;
        let _e417 = frost;
        let _e420 = mix(_e409.xyz, vec3<f32>((_e411 * 0.88f), (_e413 * 0.94f), _e415), vec3((_e417 * 0.55f)));
        base[0u] = _e420.x;
        base[1u] = _e420.y;
        base[2u] = _e420.z;
        let _e427 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e427).z);
        let _e433 = unnamed.worldLightParams[3u];
        let _e435 = upward;
        snowCoverage = (clamp(_e433, 0f, 1f) * _e435);
        let _e437 = base;
        let _e439 = snowCoverage;
        let _e442 = mix(_e437.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e439 * 0.82f)));
        base[0u] = _e442.x;
        base[1u] = _e442.y;
        base[2u] = _e442.z;
    }
    let _e451 = unnamed.materialFactors[0u];
    if (_e451 >= 0f) {
        let _e454 = unnamed.materialBaseColor;
        let _e455 = base;
        base = (_e455 * _e454);
    }
    let _e457 = color0_;
    let _e460 = unnamed.emissionRadiance;
    let _e463 = base;
    let _e465 = (_e463.xyz + (_e457.xyz * _e460.xyz));
    base[0u] = _e465.x;
    base[1u] = _e465.y;
    base[2u] = _e465.z;
    if override_type_3_10 {
        if override_type_3_11 {
            let _e475 = unnamed.packed_indices[2i][0u];
            let _e481 = unnamed.packed_indices[2i][0u];
            let _e486 = frag_tex_coord0_1;
            let _e487 = textureSample(wired_bindless_images[(_e475 & 4095u)], wired_bindless_samplers[((_e481 >> bitcast<u32>(12i)) & 255u)], _e486);
            local_1 = _e487.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e489 = local_1;
        orm = _e489;
        let _e491 = orm[0u];
        let _e494 = unnamed.materialFactors[2u];
        ao = mix(1f, _e491, clamp(_e494, 0f, 1f));
        let _e498 = orm[1u];
        let _e501 = unnamed.materialFactors[1u];
        let _e505 = unnamed.worldLightParams[1u];
        let _e510 = unnamed.worldLightParams[2u];
        let _e515 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e498 * _e501) - (0.35f * _e505)) + (0.3f * _e510)) + (0.4f * _e515)), 0.04f, 1f);
        let _e520 = orm[2u];
        let _e523 = unnamed.materialFactors[0u];
        metalness = clamp((_e520 * _e523), 0f, 1f);
        let _e526 = ibl_N_1;
        ibl_n = normalize(_e526);
        let _e528 = ibl_V_1;
        ibl_v = normalize(_e528);
        let _e530 = materialAlbedo;
        let _e532 = unnamed.materialBaseColor;
        let _e535 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e530 * _e532.xyz), vec3(_e535));
        let _e538 = ibl_n;
        let _e539 = ibl_v;
        NdotV = max(dot(_e538, _e539), 0f);
        let _e542 = NdotV;
        param_14 = _e542;
        let _e543 = F0_1;
        param_15 = _e543;
        let _e544 = roughness_1;
        param_16 = _e544;
        let _e545 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        F_amb = _e545;
        let _e546 = ibl_v;
        let _e548 = ibl_n;
        R = reflect(-(_e546), _e548);
        let _e552 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e552, 0f, 1f);
        let _e554 = R;
        let _e555 = roughness_1;
        let _e556 = textureNumLevels(radianceCube);
        let _e561 = textureSampleLevel(radianceCube, radianceCube_sampler, _e554, (_e555 * f32((i32(_e556) - 1i))));
        currentPrefiltered = _e561.xyz;
        let _e563 = R;
        let _e564 = roughness_1;
        let _e565 = textureNumLevels(previousRadianceCube);
        let _e570 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e563, (_e564 * f32((i32(_e565) - 1i))));
        previousPrefiltered = _e570.xyz;
        let _e572 = previousPrefiltered;
        let _e573 = currentPrefiltered;
        let _e574 = environmentBlend;
        prefiltered = mix(_e572, _e573, vec3(_e574));
        let _e577 = NdotV;
        let _e578 = roughness_1;
        let _e580 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e577, _e578), 0f);
        envBRDF = _e580.xy;
        let _e582 = prefiltered;
        let _e583 = F_amb;
        let _e585 = envBRDF[0u];
        let _e588 = envBRDF[1u];
        let _e592 = ao;
        specularIBL = ((_e582 * ((_e583 * _e585) + vec3(_e588))) * _e592);
        let _e594 = gl_FragCoord_1;
        let _e596 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e594.xy / vec2<f32>(vec2<i32>(_e596)));
        let _e600 = gtaoUV;
        let _e601 = textureSample(gtaoMap, gtaoMap_sampler, _e600);
        gtao_vis = _e601.x;
        let _e603 = gtao_vis;
        let _e604 = specularIBL;
        specularIBL = (_e604 * _e603);
        let _e606 = specularIBL;
        let _e607 = base;
        let _e609 = (_e607.xyz + _e606);
        base[0u] = _e609.x;
        base[1u] = _e609.y;
        base[2u] = _e609.z;
    }
    let _e616 = wired_advanced_fog_enabled_u0028_();
    if _e616 {
        let _e617 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e617;
        let _e618 = base;
        let _e621 = unnamed.advancedFogColorDensity;
        let _e623 = fogAmount;
        let _e625 = mix(_e618.xyz, _e621.xyz, vec3(_e623));
        base[0u] = _e625.x;
        base[1u] = _e625.y;
        base[2u] = _e625.z;
    }
    if override_type_3_12 {
        let _e633 = base[3u];
        if (_e633 == 0f) {
            discard;
        }
    } else {
        if override_type_3_13 {
            let _e635 = base;
            let _e637 = base;
            if (dot(_e635.xyz, _e637.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e641 = base;
    out_color = _e641;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e19 = out_color;
    return _e19;
}
