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
@id(10) override acff: i32 = 0i;
override override_type_3_10: bool = (acff == 1i);
override override_type_3_11: bool = (acff == 2i);
override override_type_3_12: bool = (acff == 3i);
override override_type_3_13: bool = (acff == 1i);
override override_type_3_14: bool = (acff == 2i);
override override_type_3_15: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_16: bool = (discard_mode == 1i);
override override_type_3_17: bool = (discard_mode == 2i);
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

    let _e101 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e101 + 0.5f));
    let _e106 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e108 = fogType;
    let _e111 = fogType;
    return (((_e106 > 0.5f) && (_e108 >= 1i)) && (_e111 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e101 = wired_advanced_fog_enabled_u0028_();
    if !(_e101) {
        return 0f;
    }
    let _e104 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e104, 0.000001f));
    let _e109 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e109 + 0.5f));
    let _e112 = fogType_1;
    if (_e112 == 1i) {
        let _e116 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e116 <= 0f) {
            return 0f;
        }
        let _e118 = viewDepth;
        let _e121 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e118 / _e121), 0f, 1f);
    }
    let _e126 = unnamed.advancedFogColorDensity[3u];
    let _e128 = viewDepth;
    opticalDepth = (max(_e126, 0f) * _e128);
    let _e130 = fogType_1;
    if (_e130 == 2i) {
        let _e132 = opticalDepth;
        return clamp((1f - exp(-(_e132))), 0f, 1f);
    }
    let _e137 = opticalDepth;
    let _e138 = opticalDepth;
    return clamp((1f - exp(-((_e137 * _e138)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e104 = (*cosTheta);
    t = (1f - _e104);
    let _e106 = t;
    let _e107 = t;
    t2_ = (_e106 * _e107);
    let _e109 = (*roughness);
    let _e112 = (*F0_);
    Fmax = max(vec3((1f - _e109)), _e112);
    let _e114 = (*F0_);
    let _e115 = Fmax;
    let _e116 = (*F0_);
    let _e118 = t2_;
    let _e119 = t2_;
    let _e121 = t;
    return (_e114 + ((_e115 - _e116) * ((_e118 * _e119) * _e121)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e101 = (*rgb);
    let _e104 = unnamed.worldLightParams[0u];
    boosted = (_e101 * _e104);
    let _e107 = boosted[0u];
    let _e109 = boosted[1u];
    let _e111 = boosted[2u];
    peak = max(_e107, max(_e109, _e111));
    let _e114 = peak;
    if (_e114 > 1f) {
        let _e116 = peak;
        let _e117 = boosted;
        boosted = (_e117 / vec3(_e116));
    }
    let _e120 = boosted;
    return _e120;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e102 = (*c);
    (*c) = max(_e102, vec3<f32>(0f, 0f, 0f));
    let _e104 = (*c);
    cutoff = (_e104 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e106 = (*c);
    lo = (_e106 / vec3(12.92f));
    let _e109 = (*c);
    hi = pow(((_e109 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e114 = hi;
    let _e115 = lo;
    let _e116 = cutoff;
    return mix(_e114, _e115, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e116));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e104 = (*role);
    let _e106 = (*role);
    let _e111 = unnamed.packed_indices[(_e104 / 4u)][(_e106 % 4u)];
    let _e114 = (*role);
    let _e116 = (*role);
    let _e121 = unnamed.packed_indices[(_e114 / 4u)][(_e116 % 4u)];
    let _e126 = (*uv);
    let _e127 = textureSample(wired_bindless_images[(_e111 & 4095u)], wired_bindless_samplers[((_e121 >> bitcast<u32>(12i)) & 255u)], _e126);
    c_1 = _e127;
    let _e128 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e128))) == 0i) {
        let _e133 = c_1;
        param = _e133.xyz;
        let _e135 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e135.x;
        c_1[1u] = _e135.y;
        c_1[2u] = _e135.z;
    }
    let _e142 = (*slot);
    if (lightmap_slot == (_e142 + 1i)) {
        let _e145 = c_1;
        param_1 = _e145.xyz;
        let _e147 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e147.x;
        c_1[1u] = _e147.y;
        c_1[2u] = _e147.z;
    }
    let _e154 = c_1;
    return _e154;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e163 = unnamed.packed_indices[0i][3u];
    let _e169 = unnamed.packed_indices[0i][3u];
    let _e174 = fog_tex_coord_1;
    let _e175 = textureSample(wired_bindless_images[(_e163 & 4095u)], wired_bindless_samplers[((_e169 >> bitcast<u32>(12i)) & 255u)], _e174);
    fog = _e175;
    let _e176 = frag_color0In_1;
    param_2 = _e176.xyz;
    let _e178 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e180 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e178.x, _e178.y, _e178.z, _e180);
    let _e185 = frag_color1In_1;
    param_3 = _e185.xyz;
    let _e187 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e189 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e187.x, _e187.y, _e187.z, _e189);
    param_4 = 0u;
    let _e194 = frag_tex_coord0_1;
    param_5 = _e194;
    param_6 = 0i;
    let _e195 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e196 = frag_color0_;
    color0_ = (_e195 * _e196);
    if override_type_3_2 {
        param_7 = 1u;
        let _e198 = frag_tex_coord1_1;
        param_8 = _e198;
        param_9 = 1i;
        let _e199 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e200 = frag_color1_;
        color1_ = (_e199 * _e200);
        let _e202 = color0_;
        let _e204 = color1_;
        let _e206 = (_e202.xyz + _e204.xyz);
        let _e208 = color0_[3u];
        let _e210 = color1_[3u];
        base = vec4<f32>(_e206.x, _e206.y, _e206.z, (_e208 * _e210));
    } else {
        if override_type_3_3 {
            param_10 = 1u;
            let _e216 = frag_tex_coord1_1;
            param_11 = _e216;
            param_12 = 1i;
            let _e217 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e218 = frag_color1_;
            color1_1 = (_e217 * _e218);
            let _e221 = color0_[3u];
            let _e222 = color0_;
            color0_ = (_e222 * _e221);
            let _e225 = color1_1[3u];
            let _e226 = color1_1;
            color1_1 = (_e226 * _e225);
            let _e228 = color0_;
            let _e230 = color1_1;
            let _e232 = (_e228.xyz + _e230.xyz);
            let _e234 = color0_[3u];
            let _e236 = color1_1[3u];
            base = vec4<f32>(_e232.x, _e232.y, _e232.z, (_e234 * _e236));
        } else {
            if override_type_3_4 {
                param_13 = 1u;
                let _e242 = frag_tex_coord1_1;
                param_14 = _e242;
                param_15 = 1i;
                let _e243 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
                let _e244 = frag_color1_;
                color1_2 = (_e243 * _e244);
                let _e247 = color0_[3u];
                let _e249 = color0_;
                color0_ = (_e249 * (1f - _e247));
                let _e252 = color1_2[3u];
                let _e254 = color1_2;
                color1_2 = (_e254 * (1f - _e252));
                let _e256 = color0_;
                let _e258 = color1_2;
                let _e260 = (_e256.xyz + _e258.xyz);
                let _e262 = color0_[3u];
                let _e264 = color1_2[3u];
                base = vec4<f32>(_e260.x, _e260.y, _e260.z, (_e262 * _e264));
            } else {
                if override_type_3_5 {
                    param_16 = 1u;
                    let _e270 = frag_tex_coord1_1;
                    param_17 = _e270;
                    param_18 = 1i;
                    let _e271 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
                    let _e272 = frag_color1_;
                    color1_3 = (_e271 * _e272);
                    let _e274 = color0_;
                    let _e275 = color1_3;
                    let _e277 = color1_3[3u];
                    base = mix(_e274, _e275, vec4(_e277));
                } else {
                    if override_type_3_6 {
                        param_19 = 1u;
                        let _e280 = frag_tex_coord1_1;
                        param_20 = _e280;
                        param_21 = 1i;
                        let _e281 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                        let _e282 = frag_color1_;
                        color1_4 = (_e281 * _e282);
                        let _e284 = color1_4;
                        let _e285 = color0_;
                        let _e287 = color1_4[3u];
                        base = mix(_e284, _e285, vec4(_e287));
                    } else {
                        if override_type_3_7 {
                            param_22 = 1u;
                            let _e290 = frag_tex_coord1_1;
                            param_23 = _e290;
                            param_24 = 1i;
                            let _e291 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                            let _e292 = frag_color1_;
                            color1_5 = (_e291 * _e292);
                            let _e294 = color1_5;
                            let _e296 = color1_5[3u];
                            let _e299 = color0_;
                            base = ((_e294 + vec4(_e296)) * _e299);
                        } else {
                            param_25 = 1u;
                            let _e301 = frag_tex_coord1_1;
                            param_26 = _e301;
                            param_27 = 1i;
                            let _e302 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                            let _e303 = frag_color1_;
                            color1_6 = (_e302 * _e303);
                            let _e305 = color0_;
                            let _e307 = color1_6;
                            let _e309 = (_e305.xyz * _e307.xyz);
                            base[0u] = _e309.x;
                            base[1u] = _e309.y;
                            base[2u] = _e309.z;
                            let _e317 = color0_[3u];
                            let _e319 = color1_6[3u];
                            base[3u] = (_e317 * _e319);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e324 = unnamed.worldLightParams[1u];
        wetness = clamp(_e324, 0f, 1f);
        let _e328 = unnamed.worldLightParams[2u];
        frost = clamp(_e328, 0f, 1f);
        let _e330 = base;
        luminance = dot(_e330.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e333 = wetness;
        let _e335 = base;
        let _e337 = (_e335.xyz * mix(1f, 0.82f, _e333));
        base[0u] = _e337.x;
        base[1u] = _e337.y;
        base[2u] = _e337.z;
        let _e344 = base;
        let _e346 = luminance;
        let _e348 = luminance;
        let _e350 = luminance;
        let _e352 = frost;
        let _e355 = mix(_e344.xyz, vec3<f32>((_e346 * 0.88f), (_e348 * 0.94f), _e350), vec3((_e352 * 0.55f)));
        base[0u] = _e355.x;
        base[1u] = _e355.y;
        base[2u] = _e355.z;
        let _e362 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e362).z);
        let _e368 = unnamed.worldLightParams[3u];
        let _e370 = upward;
        snowCoverage = (clamp(_e368, 0f, 1f) * _e370);
        let _e372 = base;
        let _e374 = snowCoverage;
        let _e377 = mix(_e372.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e374 * 0.82f)));
        base[0u] = _e377.x;
        base[1u] = _e377.y;
        base[2u] = _e377.z;
    }
    let _e384 = color0_;
    let _e387 = unnamed.emissionRadiance;
    let _e390 = base;
    let _e392 = (_e390.xyz + (_e384.xyz * _e387.xyz));
    base[0u] = _e392.x;
    base[1u] = _e392.y;
    base[2u] = _e392.z;
    if override_type_3_9 {
        let _e402 = unnamed.packed_indices[2i][0u];
        let _e408 = unnamed.packed_indices[2i][0u];
        let _e413 = frag_tex_coord0_1;
        let _e414 = textureSample(wired_bindless_images[(_e402 & 4095u)], wired_bindless_samplers[((_e408 >> bitcast<u32>(12i)) & 255u)], _e413);
        orm = _e414.xyz;
        let _e417 = orm[0u];
        ao = _e417;
        let _e419 = orm[1u];
        let _e422 = unnamed.worldLightParams[1u];
        let _e427 = unnamed.worldLightParams[2u];
        let _e432 = unnamed.worldLightParams[3u];
        roughness_1 = clamp((((_e419 - (0.35f * _e422)) + (0.3f * _e427)) + (0.4f * _e432)), 0.04f, 1f);
        let _e437 = orm[2u];
        metalness = _e437;
        let _e438 = ibl_N_1;
        ibl_n = normalize(_e438);
        let _e440 = ibl_V_1;
        ibl_v = normalize(_e440);
        let _e442 = base;
        let _e444 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e442.xyz, vec3(_e444));
        let _e447 = ibl_n;
        let _e448 = ibl_v;
        NdotV = max(dot(_e447, _e448), 0f);
        let _e451 = NdotV;
        param_28 = _e451;
        let _e452 = F0_1;
        param_29 = _e452;
        let _e453 = roughness_1;
        param_30 = _e453;
        let _e454 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_28), (&param_29), (&param_30));
        F_amb = _e454;
        let _e455 = ibl_v;
        let _e457 = ibl_n;
        R = reflect(-(_e455), _e457);
        let _e459 = R;
        let _e460 = roughness_1;
        let _e462 = textureSampleLevel(radianceCube, radianceCube_sampler, _e459, (_e460 * 5f));
        prefiltered = _e462.xyz;
        let _e464 = NdotV;
        let _e465 = roughness_1;
        let _e467 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e464, _e465), 0f);
        envBRDF = _e467.xy;
        let _e469 = prefiltered;
        let _e470 = F_amb;
        let _e472 = envBRDF[0u];
        let _e475 = envBRDF[1u];
        let _e479 = ao;
        specularIBL = ((_e469 * ((_e470 * _e472) + vec3(_e475))) * _e479);
        let _e481 = gl_FragCoord_1;
        let _e483 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e481.xy / vec2<f32>(vec2<i32>(_e483)));
        let _e487 = gtaoUV;
        let _e488 = textureSample(gtaoMap, gtaoMap_sampler, _e487);
        gtao_vis = _e488.x;
        let _e490 = gtao_vis;
        let _e491 = specularIBL;
        specularIBL = (_e491 * _e490);
        let _e493 = specularIBL;
        let _e494 = base;
        let _e496 = (_e494.xyz + _e493);
        base[0u] = _e496.x;
        base[1u] = _e496.y;
        base[2u] = _e496.z;
    }
    let _e503 = wired_advanced_fog_enabled_u0028_();
    if _e503 {
        let _e504 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e504;
        if override_type_3_10 {
            let _e505 = fogAmount;
            let _e507 = base;
            let _e509 = (_e507.xyz * (1f - _e505));
            base[0u] = _e509.x;
            base[1u] = _e509.y;
            base[2u] = _e509.z;
        } else {
            if override_type_3_11 {
                let _e516 = fogAmount;
                let _e518 = base;
                base = (_e518 * (1f - _e516));
            } else {
                if override_type_3_12 {
                    let _e520 = fogAmount;
                    let _e523 = base[3u];
                    base[3u] = (_e523 * (1f - _e520));
                } else {
                    let _e526 = base;
                    let _e529 = unnamed.advancedFogColorDensity;
                    let _e531 = fogAmount;
                    let _e533 = mix(_e526.xyz, _e529.xyz, vec3(_e531));
                    base[0u] = _e533.x;
                    base[1u] = _e533.y;
                    base[2u] = _e533.z;
                }
            }
        }
    } else {
        if override_type_3_13 {
            let _e540 = base;
            let _e543 = fog[3u];
            let _e545 = (_e540.xyz * (1f - _e543));
            base[0u] = _e545.x;
            base[1u] = _e545.y;
            base[2u] = _e545.z;
        } else {
            if override_type_3_14 {
                let _e552 = base;
                let _e554 = fog[3u];
                base = (_e552 * (1f - _e554));
            } else {
                if override_type_3_15 {
                    let _e558 = base[3u];
                    let _e560 = fog[3u];
                    base[3u] = (_e558 * (1f - _e560));
                } else {
                    let _e564 = base;
                    let _e565 = fog;
                    let _e567 = unnamed.fogColor;
                    let _e570 = fog[3u];
                    base = mix(_e564, (_e565 * _e567), vec4(_e570));
                }
            }
        }
    }
    if override_type_3_16 {
        let _e574 = base[3u];
        if (_e574 == 0f) {
            discard;
        }
    } else {
        if override_type_3_17 {
            let _e576 = base;
            let _e578 = base;
            if (dot(_e576.xyz, _e578.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e582 = base;
    out_color = _e582;
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
