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

    let _e106 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e106 + 0.5f));
    let _e111 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e113 = fogType;
    let _e116 = fogType;
    return (((_e111 > 0.5f) && (_e113 >= 1i)) && (_e116 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e106 = wired_advanced_fog_enabled_u0028_();
    if !(_e106) {
        return 0f;
    }
    let _e109 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e109, 0.000001f));
    let _e114 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e114 + 0.5f));
    let _e117 = fogType_1;
    if (_e117 == 1i) {
        let _e121 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e121 <= 0f) {
            return 0f;
        }
        let _e123 = viewDepth;
        let _e126 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e123 / _e126), 0f, 1f);
    }
    let _e131 = unnamed.advancedFogColorDensity[3u];
    let _e133 = viewDepth;
    opticalDepth = (max(_e131, 0f) * _e133);
    let _e135 = fogType_1;
    if (_e135 == 2i) {
        let _e137 = opticalDepth;
        return clamp((1f - exp(-(_e137))), 0f, 1f);
    }
    let _e142 = opticalDepth;
    let _e143 = opticalDepth;
    return clamp((1f - exp(-((_e142 * _e143)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e109 = (*cosTheta);
    t = (1f - _e109);
    let _e111 = t;
    let _e112 = t;
    t2_ = (_e111 * _e112);
    let _e114 = (*roughness);
    let _e117 = (*F0_);
    Fmax = max(vec3((1f - _e114)), _e117);
    let _e119 = (*F0_);
    let _e120 = Fmax;
    let _e121 = (*F0_);
    let _e123 = t2_;
    let _e124 = t2_;
    let _e126 = t;
    return (_e119 + ((_e120 - _e121) * ((_e123 * _e124) * _e126)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e106 = (*rgb);
    let _e109 = unnamed.worldLightParams[0u];
    boosted = (_e106 * _e109);
    let _e112 = boosted[0u];
    let _e114 = boosted[1u];
    let _e116 = boosted[2u];
    peak = max(_e112, max(_e114, _e116));
    let _e119 = peak;
    if (_e119 > 1f) {
        let _e121 = peak;
        let _e122 = boosted;
        boosted = (_e122 / vec3(_e121));
    }
    let _e125 = boosted;
    return _e125;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e107 = (*c);
    (*c) = max(_e107, vec3<f32>(0f, 0f, 0f));
    let _e109 = (*c);
    cutoff = (_e109 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e111 = (*c);
    lo = (_e111 / vec3(12.92f));
    let _e114 = (*c);
    hi = pow(((_e114 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e119 = hi;
    let _e120 = lo;
    let _e121 = cutoff;
    return mix(_e119, _e120, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e121));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e109 = (*role);
    let _e111 = (*role);
    let _e116 = unnamed.packed_indices[(_e109 / 4u)][(_e111 % 4u)];
    let _e119 = (*role);
    let _e121 = (*role);
    let _e126 = unnamed.packed_indices[(_e119 / 4u)][(_e121 % 4u)];
    let _e131 = (*uv);
    let _e132 = textureSample(wired_bindless_images[(_e116 & 4095u)], wired_bindless_samplers[((_e126 >> bitcast<u32>(12i)) & 255u)], _e131);
    c_1 = _e132;
    let _e133 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e133))) == 0i) {
        let _e138 = c_1;
        param = _e138.xyz;
        let _e140 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e140.x;
        c_1[1u] = _e140.y;
        c_1[2u] = _e140.z;
    }
    let _e147 = (*slot);
    if (lightmap_slot == (_e147 + 1i)) {
        let _e150 = c_1;
        param_1 = _e150.xyz;
        let _e152 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e152.x;
        c_1[1u] = _e152.y;
        c_1[2u] = _e152.z;
    }
    let _e159 = c_1;
    return _e159;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e171 = unnamed.packed_indices[0i][3u];
    let _e177 = unnamed.packed_indices[0i][3u];
    let _e182 = fog_tex_coord_1;
    let _e183 = textureSample(wired_bindless_images[(_e171 & 4095u)], wired_bindless_samplers[((_e177 >> bitcast<u32>(12i)) & 255u)], _e182);
    fog = _e183;
    let _e184 = frag_color0In_1;
    param_2 = _e184.xyz;
    let _e186 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e188 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e186.x, _e186.y, _e186.z, _e188);
    let _e193 = frag_color1In_1;
    param_3 = _e193.xyz;
    let _e195 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e197 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e195.x, _e195.y, _e195.z, _e197);
    let _e202 = frag_color2In_1;
    param_4 = _e202.xyz;
    let _e204 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e206 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e204.x, _e204.y, _e204.z, _e206);
    param_5 = 0u;
    let _e211 = frag_tex_coord0_1;
    param_6 = _e211;
    param_7 = 0i;
    let _e212 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor0_ = _e212;
    param_8 = 1u;
    let _e213 = frag_tex_coord1_1;
    param_9 = _e213;
    param_10 = 1i;
    let _e214 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor1_ = _e214;
    param_11 = 2u;
    let _e215 = frag_tex_coord2_1;
    param_12 = _e215;
    param_13 = 2i;
    let _e216 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor2_ = _e216;
    if override_type_3_ {
        let _e217 = sampledColor1_;
        local = _e217.xyz;
    } else {
        let _e219 = sampledColor0_;
        local = _e219.xyz;
    }
    let _e221 = local;
    materialAlbedo = _e221;
    let _e222 = sampledColor0_;
    let _e223 = frag_color0_;
    color0_ = (_e222 * _e223);
    if override_type_3_3 {
        let _e225 = sampledColor1_;
        let _e226 = frag_color1_;
        color1_ = (_e225 * _e226);
        let _e228 = sampledColor2_;
        let _e229 = frag_color2_;
        color2_ = (_e228 * _e229);
        let _e231 = color0_;
        let _e233 = color1_;
        let _e236 = color2_;
        let _e238 = ((_e231.xyz + _e233.xyz) + _e236.xyz);
        let _e240 = color0_[3u];
        let _e242 = color1_[3u];
        let _e245 = color2_[3u];
        base = vec4<f32>(_e238.x, _e238.y, _e238.z, ((_e240 * _e242) * _e245));
    } else {
        if override_type_3_4 {
            let _e251 = sampledColor1_;
            let _e252 = frag_color1_;
            color1_1 = (_e251 * _e252);
            let _e254 = sampledColor2_;
            let _e255 = frag_color2_;
            color2_1 = (_e254 * _e255);
            let _e258 = color0_[3u];
            let _e259 = color0_;
            color0_ = (_e259 * _e258);
            let _e262 = color1_1[3u];
            let _e263 = color1_1;
            color1_1 = (_e263 * _e262);
            let _e266 = color2_1[3u];
            let _e267 = color2_1;
            color2_1 = (_e267 * _e266);
            let _e269 = color0_;
            let _e271 = color1_1;
            let _e274 = color2_1;
            let _e276 = ((_e269.xyz + _e271.xyz) + _e274.xyz);
            let _e278 = color0_[3u];
            let _e280 = color1_1[3u];
            let _e283 = color2_1[3u];
            base = vec4<f32>(_e276.x, _e276.y, _e276.z, ((_e278 * _e280) * _e283));
        } else {
            if override_type_3_5 {
                let _e289 = sampledColor1_;
                let _e290 = frag_color1_;
                color1_2 = (_e289 * _e290);
                let _e292 = sampledColor2_;
                let _e293 = frag_color2_;
                color2_2 = (_e292 * _e293);
                let _e296 = color0_[3u];
                let _e298 = color0_;
                color0_ = (_e298 * (1f - _e296));
                let _e301 = color1_2[3u];
                let _e303 = color1_2;
                color1_2 = (_e303 * (1f - _e301));
                let _e306 = color2_2[3u];
                let _e308 = color2_2;
                color2_2 = (_e308 * (1f - _e306));
                let _e310 = color0_;
                let _e312 = color1_2;
                let _e315 = color2_2;
                let _e317 = ((_e310.xyz + _e312.xyz) + _e315.xyz);
                let _e319 = color0_[3u];
                let _e321 = color1_2[3u];
                let _e324 = color2_2[3u];
                base = vec4<f32>(_e317.x, _e317.y, _e317.z, ((_e319 * _e321) * _e324));
            } else {
                if override_type_3_6 {
                    let _e330 = sampledColor1_;
                    let _e331 = frag_color1_;
                    color1_3 = (_e330 * _e331);
                    let _e333 = sampledColor2_;
                    let _e334 = frag_color2_;
                    color2_3 = (_e333 * _e334);
                    let _e336 = color0_;
                    let _e337 = color1_3;
                    let _e339 = color1_3[3u];
                    let _e342 = color2_3;
                    let _e344 = color2_3[3u];
                    base = mix(mix(_e336, _e337, vec4(_e339)), _e342, vec4(_e344));
                } else {
                    if override_type_3_7 {
                        let _e347 = sampledColor1_;
                        let _e348 = frag_color1_;
                        color1_4 = (_e347 * _e348);
                        let _e350 = sampledColor2_;
                        let _e351 = frag_color2_;
                        color2_4 = (_e350 * _e351);
                        let _e353 = color2_4;
                        let _e354 = color1_4;
                        let _e355 = color0_;
                        let _e357 = color1_4[3u];
                        let _e361 = color2_4[3u];
                        base = mix(_e353, mix(_e354, _e355, vec4(_e357)), vec4(_e361));
                    } else {
                        if override_type_3_8 {
                            let _e364 = sampledColor1_;
                            let _e365 = frag_color1_;
                            color1_5 = (_e364 * _e365);
                            let _e367 = sampledColor2_;
                            let _e368 = frag_color2_;
                            color2_5 = (_e367 * _e368);
                            let _e370 = color2_5;
                            let _e372 = color2_5[3u];
                            let _e375 = color1_5;
                            let _e377 = color1_5[3u];
                            let _e381 = color0_;
                            base = (((_e370 + vec4(_e372)) * (_e375 + vec4(_e377))) * _e381);
                        } else {
                            let _e383 = sampledColor1_;
                            let _e384 = frag_color1_;
                            color1_6 = (_e383 * _e384);
                            let _e386 = sampledColor2_;
                            let _e387 = frag_color2_;
                            color2_6 = (_e386 * _e387);
                            let _e389 = color0_;
                            let _e391 = color1_6;
                            let _e394 = color2_6;
                            let _e396 = ((_e389.xyz * _e391.xyz) * _e394.xyz);
                            base[0u] = _e396.x;
                            base[1u] = _e396.y;
                            base[2u] = _e396.z;
                            let _e404 = color0_[3u];
                            let _e406 = color1_6[3u];
                            let _e409 = color2_6[3u];
                            base[3u] = ((_e404 * _e406) * _e409);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_9 {
        let _e414 = unnamed.worldLightParams[1u];
        wetness = clamp(_e414, 0f, 1f);
        let _e418 = unnamed.worldLightParams[2u];
        frost = clamp(_e418, 0f, 1f);
        let _e420 = base;
        luminance = dot(_e420.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e423 = wetness;
        let _e425 = base;
        let _e427 = (_e425.xyz * mix(1f, 0.82f, _e423));
        base[0u] = _e427.x;
        base[1u] = _e427.y;
        base[2u] = _e427.z;
        let _e434 = base;
        let _e436 = luminance;
        let _e438 = luminance;
        let _e440 = luminance;
        let _e442 = frost;
        let _e445 = mix(_e434.xyz, vec3<f32>((_e436 * 0.88f), (_e438 * 0.94f), _e440), vec3((_e442 * 0.55f)));
        base[0u] = _e445.x;
        base[1u] = _e445.y;
        base[2u] = _e445.z;
        let _e452 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e452).z);
        let _e458 = unnamed.worldLightParams[3u];
        let _e460 = upward;
        snowCoverage = (clamp(_e458, 0f, 1f) * _e460);
        let _e462 = base;
        let _e464 = snowCoverage;
        let _e467 = mix(_e462.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e464 * 0.82f)));
        base[0u] = _e467.x;
        base[1u] = _e467.y;
        base[2u] = _e467.z;
    }
    let _e476 = unnamed.materialFactors[0u];
    if (_e476 >= 0f) {
        let _e479 = unnamed.materialBaseColor;
        let _e480 = base;
        base = (_e480 * _e479);
    }
    let _e482 = color0_;
    let _e485 = unnamed.emissionRadiance;
    let _e488 = base;
    let _e490 = (_e488.xyz + (_e482.xyz * _e485.xyz));
    base[0u] = _e490.x;
    base[1u] = _e490.y;
    base[2u] = _e490.z;
    if override_type_3_10 {
        if override_type_3_11 {
            let _e500 = unnamed.packed_indices[2i][0u];
            let _e506 = unnamed.packed_indices[2i][0u];
            let _e511 = frag_tex_coord0_1;
            let _e512 = textureSample(wired_bindless_images[(_e500 & 4095u)], wired_bindless_samplers[((_e506 >> bitcast<u32>(12i)) & 255u)], _e511);
            local_1 = _e512.xyz;
        } else {
            local_1 = vec3<f32>(1f, 1f, 1f);
        }
        let _e514 = local_1;
        orm = _e514;
        let _e516 = orm[0u];
        let _e519 = unnamed.materialFactors[2u];
        ao = mix(1f, _e516, clamp(_e519, 0f, 1f));
        let _e523 = orm[1u];
        let _e526 = unnamed.materialFactors[1u];
        let _e530 = unnamed.worldLightParams[1u];
        let _e535 = unnamed.worldLightParams[2u];
        let _e540 = unnamed.worldLightParams[3u];
        roughness_1 = clamp(((((_e523 * _e526) - (0.35f * _e530)) + (0.3f * _e535)) + (0.4f * _e540)), 0.04f, 1f);
        let _e545 = orm[2u];
        let _e548 = unnamed.materialFactors[0u];
        metalness = clamp((_e545 * _e548), 0f, 1f);
        let _e551 = ibl_N_1;
        ibl_n = normalize(_e551);
        let _e553 = ibl_V_1;
        ibl_v = normalize(_e553);
        let _e555 = materialAlbedo;
        let _e557 = unnamed.materialBaseColor;
        let _e560 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), (_e555 * _e557.xyz), vec3(_e560));
        let _e563 = ibl_n;
        let _e564 = ibl_v;
        NdotV = max(dot(_e563, _e564), 0f);
        let _e567 = NdotV;
        param_14 = _e567;
        let _e568 = F0_1;
        param_15 = _e568;
        let _e569 = roughness_1;
        param_16 = _e569;
        let _e570 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        F_amb = _e570;
        let _e571 = ibl_v;
        let _e573 = ibl_n;
        R = reflect(-(_e571), _e573);
        let _e577 = unnamed.materialFactors[3u];
        environmentBlend = clamp(_e577, 0f, 1f);
        let _e579 = R;
        let _e580 = roughness_1;
        let _e581 = textureNumLevels(radianceCube);
        let _e586 = textureSampleLevel(radianceCube, radianceCube_sampler, _e579, (_e580 * f32((i32(_e581) - 1i))));
        currentPrefiltered = _e586.xyz;
        let _e588 = R;
        let _e589 = roughness_1;
        let _e590 = textureNumLevels(previousRadianceCube);
        let _e595 = textureSampleLevel(previousRadianceCube, previousRadianceCube_sampler, _e588, (_e589 * f32((i32(_e590) - 1i))));
        previousPrefiltered = _e595.xyz;
        let _e597 = previousPrefiltered;
        let _e598 = currentPrefiltered;
        let _e599 = environmentBlend;
        prefiltered = mix(_e597, _e598, vec3(_e599));
        let _e602 = NdotV;
        let _e603 = roughness_1;
        let _e605 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e602, _e603), 0f);
        envBRDF = _e605.xy;
        let _e607 = prefiltered;
        let _e608 = F_amb;
        let _e610 = envBRDF[0u];
        let _e613 = envBRDF[1u];
        let _e617 = ao;
        specularIBL = ((_e607 * ((_e608 * _e610) + vec3(_e613))) * _e617);
        let _e619 = gl_FragCoord_1;
        let _e621 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e619.xy / vec2<f32>(vec2<i32>(_e621)));
        let _e625 = gtaoUV;
        let _e626 = textureSample(gtaoMap, gtaoMap_sampler, _e625);
        gtao_vis = _e626.x;
        let _e628 = gtao_vis;
        let _e629 = specularIBL;
        specularIBL = (_e629 * _e628);
        let _e631 = specularIBL;
        let _e632 = base;
        let _e634 = (_e632.xyz + _e631);
        base[0u] = _e634.x;
        base[1u] = _e634.y;
        base[2u] = _e634.z;
    }
    let _e641 = wired_advanced_fog_enabled_u0028_();
    if _e641 {
        let _e642 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e642;
        if override_type_3_12 {
            let _e643 = fogAmount;
            let _e645 = base;
            let _e647 = (_e645.xyz * (1f - _e643));
            base[0u] = _e647.x;
            base[1u] = _e647.y;
            base[2u] = _e647.z;
        } else {
            if override_type_3_13 {
                let _e654 = fogAmount;
                let _e656 = base;
                base = (_e656 * (1f - _e654));
            } else {
                if override_type_3_14 {
                    let _e658 = fogAmount;
                    let _e661 = base[3u];
                    base[3u] = (_e661 * (1f - _e658));
                } else {
                    let _e664 = base;
                    let _e667 = unnamed.advancedFogColorDensity;
                    let _e669 = fogAmount;
                    let _e671 = mix(_e664.xyz, _e667.xyz, vec3(_e669));
                    base[0u] = _e671.x;
                    base[1u] = _e671.y;
                    base[2u] = _e671.z;
                }
            }
        }
    } else {
        if override_type_3_15 {
            let _e678 = base;
            let _e681 = fog[3u];
            let _e683 = (_e678.xyz * (1f - _e681));
            base[0u] = _e683.x;
            base[1u] = _e683.y;
            base[2u] = _e683.z;
        } else {
            if override_type_3_16 {
                let _e690 = base;
                let _e692 = fog[3u];
                base = (_e690 * (1f - _e692));
            } else {
                if override_type_3_17 {
                    let _e696 = base[3u];
                    let _e698 = fog[3u];
                    base[3u] = (_e696 * (1f - _e698));
                } else {
                    let _e702 = base;
                    let _e703 = fog;
                    let _e705 = unnamed.fogColor;
                    let _e708 = fog[3u];
                    base = mix(_e702, (_e703 * _e705), vec4(_e708));
                }
            }
        }
    }
    if override_type_3_18 {
        let _e712 = base[3u];
        if (_e712 == 0f) {
            discard;
        }
    } else {
        if override_type_3_19 {
            let _e714 = base;
            let _e716 = base;
            if (dot(_e714.xyz, _e716.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e720 = base;
    out_color = _e720;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e21 = out_color;
    return _e21;
}
