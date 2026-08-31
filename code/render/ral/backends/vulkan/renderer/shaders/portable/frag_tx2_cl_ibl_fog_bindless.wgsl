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

    let _e103 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e103 + 0.5f));
    let _e108 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e110 = fogType;
    let _e113 = fogType;
    return (((_e108 > 0.5f) && (_e110 >= 1i)) && (_e113 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e103 = wired_advanced_fog_enabled_u0028_();
    if !(_e103) {
        return 0f;
    }
    let _e106 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e106, 0.000001f));
    let _e111 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e111 + 0.5f));
    let _e114 = fogType_1;
    if (_e114 == 1i) {
        let _e118 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e118 <= 0f) {
            return 0f;
        }
        let _e120 = viewDepth;
        let _e123 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e120 / _e123), 0f, 1f);
    }
    let _e128 = unnamed.advancedFogColorDensity[3u];
    let _e130 = viewDepth;
    opticalDepth = (max(_e128, 0f) * _e130);
    let _e132 = fogType_1;
    if (_e132 == 2i) {
        let _e134 = opticalDepth;
        return clamp((1f - exp(-(_e134))), 0f, 1f);
    }
    let _e139 = opticalDepth;
    let _e140 = opticalDepth;
    return clamp((1f - exp(-((_e139 * _e140)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e106 = (*cosTheta);
    t = (1f - _e106);
    let _e108 = t;
    let _e109 = t;
    t2_ = (_e108 * _e109);
    let _e111 = (*roughness);
    let _e114 = (*F0_);
    Fmax = max(vec3((1f - _e111)), _e114);
    let _e116 = (*F0_);
    let _e117 = Fmax;
    let _e118 = (*F0_);
    let _e120 = t2_;
    let _e121 = t2_;
    let _e123 = t;
    return (_e116 + ((_e117 - _e118) * ((_e120 * _e121) * _e123)));
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e103 = (*rgb);
    let _e106 = unnamed.worldLightParams[0u];
    boosted = (_e103 * _e106);
    let _e109 = boosted[0u];
    let _e111 = boosted[1u];
    let _e113 = boosted[2u];
    peak = max(_e109, max(_e111, _e113));
    let _e116 = peak;
    if (_e116 > 1f) {
        let _e118 = peak;
        let _e119 = boosted;
        boosted = (_e119 / vec3(_e118));
    }
    let _e122 = boosted;
    return _e122;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e104 = (*c);
    (*c) = max(_e104, vec3<f32>(0f, 0f, 0f));
    let _e106 = (*c);
    cutoff = (_e106 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e108 = (*c);
    lo = (_e108 / vec3(12.92f));
    let _e111 = (*c);
    hi = pow(((_e111 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e116 = hi;
    let _e117 = lo;
    let _e118 = cutoff;
    return mix(_e116, _e117, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e118));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e106 = (*role);
    let _e108 = (*role);
    let _e113 = unnamed.packed_indices[(_e106 / 4u)][(_e108 % 4u)];
    let _e116 = (*role);
    let _e118 = (*role);
    let _e123 = unnamed.packed_indices[(_e116 / 4u)][(_e118 % 4u)];
    let _e128 = (*uv);
    let _e129 = textureSample(wired_bindless_images[(_e113 & 4095u)], wired_bindless_samplers[((_e123 >> bitcast<u32>(12i)) & 255u)], _e128);
    c_1 = _e129;
    let _e130 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e130))) == 0i) {
        let _e135 = c_1;
        param = _e135.xyz;
        let _e137 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e137.x;
        c_1[1u] = _e137.y;
        c_1[2u] = _e137.z;
    }
    let _e144 = (*slot);
    if (lightmap_slot == (_e144 + 1i)) {
        let _e147 = c_1;
        param_1 = _e147.xyz;
        let _e149 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e149.x;
        c_1[1u] = _e149.y;
        c_1[2u] = _e149.z;
    }
    let _e156 = c_1;
    return _e156;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_4: vec3<f32>;
    var color0_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color1_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color2_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color2_1: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color1_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color2_2: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var color1_3: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color2_3: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
    var color1_4: vec4<f32>;
    var param_32: u32;
    var param_33: vec2<f32>;
    var param_34: i32;
    var color2_4: vec4<f32>;
    var param_35: u32;
    var param_36: vec2<f32>;
    var param_37: i32;
    var color1_5: vec4<f32>;
    var param_38: u32;
    var param_39: vec2<f32>;
    var param_40: i32;
    var color2_5: vec4<f32>;
    var param_41: u32;
    var param_42: vec2<f32>;
    var param_43: i32;
    var color1_6: vec4<f32>;
    var param_44: u32;
    var param_45: vec2<f32>;
    var param_46: i32;
    var color2_6: vec4<f32>;
    var param_47: u32;
    var param_48: vec2<f32>;
    var param_49: i32;
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
    var param_50: f32;
    var param_51: vec3<f32>;
    var param_52: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e195 = unnamed.packed_indices[0i][3u];
    let _e201 = unnamed.packed_indices[0i][3u];
    let _e206 = fog_tex_coord_1;
    let _e207 = textureSample(wired_bindless_images[(_e195 & 4095u)], wired_bindless_samplers[((_e201 >> bitcast<u32>(12i)) & 255u)], _e206);
    fog = _e207;
    let _e208 = frag_color0In_1;
    param_2 = _e208.xyz;
    let _e210 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e212 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e210.x, _e210.y, _e210.z, _e212);
    let _e217 = frag_color1In_1;
    param_3 = _e217.xyz;
    let _e219 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e221 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e219.x, _e219.y, _e219.z, _e221);
    let _e226 = frag_color2In_1;
    param_4 = _e226.xyz;
    let _e228 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e230 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e228.x, _e228.y, _e228.z, _e230);
    param_5 = 0u;
    let _e235 = frag_tex_coord0_1;
    param_6 = _e235;
    param_7 = 0i;
    let _e236 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    let _e237 = frag_color0_;
    color0_ = (_e236 * _e237);
    if override_type_3_2 {
        param_8 = 1u;
        let _e239 = frag_tex_coord1_1;
        param_9 = _e239;
        param_10 = 1i;
        let _e240 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        let _e241 = frag_color1_;
        color1_ = (_e240 * _e241);
        param_11 = 2u;
        let _e243 = frag_tex_coord2_1;
        param_12 = _e243;
        param_13 = 2i;
        let _e244 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
        let _e245 = frag_color2_;
        color2_ = (_e244 * _e245);
        let _e247 = color0_;
        let _e249 = color1_;
        let _e252 = color2_;
        let _e254 = ((_e247.xyz + _e249.xyz) + _e252.xyz);
        let _e256 = color0_[3u];
        let _e258 = color1_[3u];
        let _e261 = color2_[3u];
        base = vec4<f32>(_e254.x, _e254.y, _e254.z, ((_e256 * _e258) * _e261));
    } else {
        if override_type_3_3 {
            param_14 = 1u;
            let _e267 = frag_tex_coord1_1;
            param_15 = _e267;
            param_16 = 1i;
            let _e268 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e269 = frag_color1_;
            color1_1 = (_e268 * _e269);
            param_17 = 2u;
            let _e271 = frag_tex_coord2_1;
            param_18 = _e271;
            param_19 = 2i;
            let _e272 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            let _e273 = frag_color2_;
            color2_1 = (_e272 * _e273);
            let _e276 = color0_[3u];
            let _e277 = color0_;
            color0_ = (_e277 * _e276);
            let _e280 = color1_1[3u];
            let _e281 = color1_1;
            color1_1 = (_e281 * _e280);
            let _e284 = color2_1[3u];
            let _e285 = color2_1;
            color2_1 = (_e285 * _e284);
            let _e287 = color0_;
            let _e289 = color1_1;
            let _e292 = color2_1;
            let _e294 = ((_e287.xyz + _e289.xyz) + _e292.xyz);
            let _e296 = color0_[3u];
            let _e298 = color1_1[3u];
            let _e301 = color2_1[3u];
            base = vec4<f32>(_e294.x, _e294.y, _e294.z, ((_e296 * _e298) * _e301));
        } else {
            if override_type_3_4 {
                param_20 = 1u;
                let _e307 = frag_tex_coord1_1;
                param_21 = _e307;
                param_22 = 1i;
                let _e308 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
                let _e309 = frag_color1_;
                color1_2 = (_e308 * _e309);
                param_23 = 2u;
                let _e311 = frag_tex_coord2_1;
                param_24 = _e311;
                param_25 = 2i;
                let _e312 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                let _e313 = frag_color2_;
                color2_2 = (_e312 * _e313);
                let _e316 = color0_[3u];
                let _e318 = color0_;
                color0_ = (_e318 * (1f - _e316));
                let _e321 = color1_2[3u];
                let _e323 = color1_2;
                color1_2 = (_e323 * (1f - _e321));
                let _e326 = color2_2[3u];
                let _e328 = color2_2;
                color2_2 = (_e328 * (1f - _e326));
                let _e330 = color0_;
                let _e332 = color1_2;
                let _e335 = color2_2;
                let _e337 = ((_e330.xyz + _e332.xyz) + _e335.xyz);
                let _e339 = color0_[3u];
                let _e341 = color1_2[3u];
                let _e344 = color2_2[3u];
                base = vec4<f32>(_e337.x, _e337.y, _e337.z, ((_e339 * _e341) * _e344));
            } else {
                if override_type_3_5 {
                    param_26 = 1u;
                    let _e350 = frag_tex_coord1_1;
                    param_27 = _e350;
                    param_28 = 1i;
                    let _e351 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                    let _e352 = frag_color1_;
                    color1_3 = (_e351 * _e352);
                    param_29 = 2u;
                    let _e354 = frag_tex_coord2_1;
                    param_30 = _e354;
                    param_31 = 2i;
                    let _e355 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                    let _e356 = frag_color2_;
                    color2_3 = (_e355 * _e356);
                    let _e358 = color0_;
                    let _e359 = color1_3;
                    let _e361 = color1_3[3u];
                    let _e364 = color2_3;
                    let _e366 = color2_3[3u];
                    base = mix(mix(_e358, _e359, vec4(_e361)), _e364, vec4(_e366));
                } else {
                    if override_type_3_6 {
                        param_32 = 1u;
                        let _e369 = frag_tex_coord1_1;
                        param_33 = _e369;
                        param_34 = 1i;
                        let _e370 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                        let _e371 = frag_color1_;
                        color1_4 = (_e370 * _e371);
                        param_35 = 2u;
                        let _e373 = frag_tex_coord2_1;
                        param_36 = _e373;
                        param_37 = 2i;
                        let _e374 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                        let _e375 = frag_color2_;
                        color2_4 = (_e374 * _e375);
                        let _e377 = color2_4;
                        let _e378 = color1_4;
                        let _e379 = color0_;
                        let _e381 = color1_4[3u];
                        let _e385 = color2_4[3u];
                        base = mix(_e377, mix(_e378, _e379, vec4(_e381)), vec4(_e385));
                    } else {
                        if override_type_3_7 {
                            param_38 = 1u;
                            let _e388 = frag_tex_coord1_1;
                            param_39 = _e388;
                            param_40 = 1i;
                            let _e389 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_38), (&param_39), (&param_40));
                            let _e390 = frag_color1_;
                            color1_5 = (_e389 * _e390);
                            param_41 = 2u;
                            let _e392 = frag_tex_coord2_1;
                            param_42 = _e392;
                            param_43 = 2i;
                            let _e393 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_41), (&param_42), (&param_43));
                            let _e394 = frag_color2_;
                            color2_5 = (_e393 * _e394);
                            let _e396 = color2_5;
                            let _e398 = color2_5[3u];
                            let _e401 = color1_5;
                            let _e403 = color1_5[3u];
                            let _e407 = color0_;
                            base = (((_e396 + vec4(_e398)) * (_e401 + vec4(_e403))) * _e407);
                        } else {
                            param_44 = 1u;
                            let _e409 = frag_tex_coord1_1;
                            param_45 = _e409;
                            param_46 = 1i;
                            let _e410 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_44), (&param_45), (&param_46));
                            let _e411 = frag_color1_;
                            color1_6 = (_e410 * _e411);
                            param_47 = 2u;
                            let _e413 = frag_tex_coord2_1;
                            param_48 = _e413;
                            param_49 = 2i;
                            let _e414 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_47), (&param_48), (&param_49));
                            let _e415 = frag_color2_;
                            color2_6 = (_e414 * _e415);
                            let _e417 = color0_;
                            let _e419 = color1_6;
                            let _e422 = color2_6;
                            let _e424 = ((_e417.xyz * _e419.xyz) * _e422.xyz);
                            base[0u] = _e424.x;
                            base[1u] = _e424.y;
                            base[2u] = _e424.z;
                            let _e432 = color0_[3u];
                            let _e434 = color1_6[3u];
                            let _e437 = color2_6[3u];
                            base[3u] = ((_e432 * _e434) * _e437);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e442 = unnamed.worldLightParams[1u];
        wetness = clamp(_e442, 0f, 1f);
        let _e446 = unnamed.worldLightParams[2u];
        frost = clamp(_e446, 0f, 1f);
        let _e448 = base;
        luminance = dot(_e448.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e451 = wetness;
        let _e453 = base;
        let _e455 = (_e453.xyz * mix(1f, 0.82f, _e451));
        base[0u] = _e455.x;
        base[1u] = _e455.y;
        base[2u] = _e455.z;
        let _e462 = base;
        let _e464 = luminance;
        let _e466 = luminance;
        let _e468 = luminance;
        let _e470 = frost;
        let _e473 = mix(_e462.xyz, vec3<f32>((_e464 * 0.88f), (_e466 * 0.94f), _e468), vec3((_e470 * 0.55f)));
        base[0u] = _e473.x;
        base[1u] = _e473.y;
        base[2u] = _e473.z;
        let _e480 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e480).z);
        let _e486 = unnamed.worldLightParams[3u];
        let _e488 = upward;
        snowCoverage = (clamp(_e486, 0f, 1f) * _e488);
        let _e490 = base;
        let _e492 = snowCoverage;
        let _e495 = mix(_e490.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e492 * 0.82f)));
        base[0u] = _e495.x;
        base[1u] = _e495.y;
        base[2u] = _e495.z;
    }
    let _e502 = color0_;
    let _e505 = unnamed.emissionRadiance;
    let _e508 = base;
    let _e510 = (_e508.xyz + (_e502.xyz * _e505.xyz));
    base[0u] = _e510.x;
    base[1u] = _e510.y;
    base[2u] = _e510.z;
    if override_type_3_9 {
        let _e520 = unnamed.packed_indices[2i][0u];
        let _e526 = unnamed.packed_indices[2i][0u];
        let _e531 = frag_tex_coord0_1;
        let _e532 = textureSample(wired_bindless_images[(_e520 & 4095u)], wired_bindless_samplers[((_e526 >> bitcast<u32>(12i)) & 255u)], _e531);
        orm = _e532.xyz;
        let _e535 = orm[0u];
        ao = _e535;
        let _e537 = orm[1u];
        let _e540 = unnamed.worldLightParams[1u];
        let _e545 = unnamed.worldLightParams[2u];
        let _e550 = unnamed.worldLightParams[3u];
        roughness_1 = clamp((((_e537 - (0.35f * _e540)) + (0.3f * _e545)) + (0.4f * _e550)), 0.04f, 1f);
        let _e555 = orm[2u];
        metalness = _e555;
        let _e556 = ibl_N_1;
        ibl_n = normalize(_e556);
        let _e558 = ibl_V_1;
        ibl_v = normalize(_e558);
        let _e560 = base;
        let _e562 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e560.xyz, vec3(_e562));
        let _e565 = ibl_n;
        let _e566 = ibl_v;
        NdotV = max(dot(_e565, _e566), 0f);
        let _e569 = NdotV;
        param_50 = _e569;
        let _e570 = F0_1;
        param_51 = _e570;
        let _e571 = roughness_1;
        param_52 = _e571;
        let _e572 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_50), (&param_51), (&param_52));
        F_amb = _e572;
        let _e573 = ibl_v;
        let _e575 = ibl_n;
        R = reflect(-(_e573), _e575);
        let _e577 = R;
        let _e578 = roughness_1;
        let _e580 = textureSampleLevel(radianceCube, radianceCube_sampler, _e577, (_e578 * 5f));
        prefiltered = _e580.xyz;
        let _e582 = NdotV;
        let _e583 = roughness_1;
        let _e585 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e582, _e583), 0f);
        envBRDF = _e585.xy;
        let _e587 = prefiltered;
        let _e588 = F_amb;
        let _e590 = envBRDF[0u];
        let _e593 = envBRDF[1u];
        let _e597 = ao;
        specularIBL = ((_e587 * ((_e588 * _e590) + vec3(_e593))) * _e597);
        let _e599 = gl_FragCoord_1;
        let _e601 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e599.xy / vec2<f32>(vec2<i32>(_e601)));
        let _e605 = gtaoUV;
        let _e606 = textureSample(gtaoMap, gtaoMap_sampler, _e605);
        gtao_vis = _e606.x;
        let _e608 = gtao_vis;
        let _e609 = specularIBL;
        specularIBL = (_e609 * _e608);
        let _e611 = specularIBL;
        let _e612 = base;
        let _e614 = (_e612.xyz + _e611);
        base[0u] = _e614.x;
        base[1u] = _e614.y;
        base[2u] = _e614.z;
    }
    let _e621 = wired_advanced_fog_enabled_u0028_();
    if _e621 {
        let _e622 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e622;
        if override_type_3_10 {
            let _e623 = fogAmount;
            let _e625 = base;
            let _e627 = (_e625.xyz * (1f - _e623));
            base[0u] = _e627.x;
            base[1u] = _e627.y;
            base[2u] = _e627.z;
        } else {
            if override_type_3_11 {
                let _e634 = fogAmount;
                let _e636 = base;
                base = (_e636 * (1f - _e634));
            } else {
                if override_type_3_12 {
                    let _e638 = fogAmount;
                    let _e641 = base[3u];
                    base[3u] = (_e641 * (1f - _e638));
                } else {
                    let _e644 = base;
                    let _e647 = unnamed.advancedFogColorDensity;
                    let _e649 = fogAmount;
                    let _e651 = mix(_e644.xyz, _e647.xyz, vec3(_e649));
                    base[0u] = _e651.x;
                    base[1u] = _e651.y;
                    base[2u] = _e651.z;
                }
            }
        }
    } else {
        if override_type_3_13 {
            let _e658 = base;
            let _e661 = fog[3u];
            let _e663 = (_e658.xyz * (1f - _e661));
            base[0u] = _e663.x;
            base[1u] = _e663.y;
            base[2u] = _e663.z;
        } else {
            if override_type_3_14 {
                let _e670 = base;
                let _e672 = fog[3u];
                base = (_e670 * (1f - _e672));
            } else {
                if override_type_3_15 {
                    let _e676 = base[3u];
                    let _e678 = fog[3u];
                    base[3u] = (_e676 * (1f - _e678));
                } else {
                    let _e682 = base;
                    let _e683 = fog;
                    let _e685 = unnamed.fogColor;
                    let _e688 = fog[3u];
                    base = mix(_e682, (_e683 * _e685), vec4(_e688));
                }
            }
        }
    }
    if override_type_3_16 {
        let _e692 = base[3u];
        if (_e692 == 0f) {
            discard;
        }
    } else {
        if override_type_3_17 {
            let _e694 = base;
            let _e696 = base;
            if (dot(_e694.xyz, _e696.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e700 = base;
    out_color = _e700;
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
