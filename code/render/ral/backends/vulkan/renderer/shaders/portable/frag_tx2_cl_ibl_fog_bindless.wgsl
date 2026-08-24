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
@id(14) override ibl_enabled: i32 = 0i;
override override_type_3_8: bool = (ibl_enabled != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_15: bool = (discard_mode == 1i);
override override_type_3_16: bool = (discard_mode == 2i);
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

    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e86 + 0.5f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e93 = fogType;
    let _e96 = fogType;
    return (((_e91 > 0.5f) && (_e93 >= 1i)) && (_e96 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e86 = wired_advanced_fog_enabled_u0028_();
    if !(_e86) {
        return 0f;
    }
    let _e89 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e89, 0.000001f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e94 + 0.5f));
    let _e97 = fogType_1;
    if (_e97 == 1i) {
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e101 <= 0f) {
            return 0f;
        }
        let _e103 = viewDepth;
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e103 / _e106), 0f, 1f);
    }
    let _e111 = unnamed.advancedFogColorDensity[3u];
    let _e113 = viewDepth;
    opticalDepth = (max(_e111, 0f) * _e113);
    let _e115 = fogType_1;
    if (_e115 == 2i) {
        let _e117 = opticalDepth;
        return clamp((1f - exp(-(_e117))), 0f, 1f);
    }
    let _e122 = opticalDepth;
    let _e123 = opticalDepth;
    return clamp((1f - exp(-((_e122 * _e123)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e89 = (*cosTheta);
    t = (1f - _e89);
    let _e91 = t;
    let _e92 = t;
    t2_ = (_e91 * _e92);
    let _e94 = (*roughness);
    let _e97 = (*F0_);
    Fmax = max(vec3((1f - _e94)), _e97);
    let _e99 = (*F0_);
    let _e100 = Fmax;
    let _e101 = (*F0_);
    let _e103 = t2_;
    let _e104 = t2_;
    let _e106 = t;
    return (_e99 + ((_e100 - _e101) * ((_e103 * _e104) * _e106)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e87 = (*c);
    (*c) = max(_e87, vec3<f32>(0f, 0f, 0f));
    let _e89 = (*c);
    cutoff = (_e89 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e91 = (*c);
    lo = (_e91 / vec3(12.92f));
    let _e94 = (*c);
    hi = pow(((_e94 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e99 = hi;
    let _e100 = lo;
    let _e101 = cutoff;
    return mix(_e99, _e100, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e101));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e98 = (*role);
    let _e100 = (*role);
    let _e105 = unnamed.packed_indices[(_e98 / 4u)][(_e100 % 4u)];
    let _e110 = (*uv);
    let _e111 = textureSample(wired_bindless_images[(_e95 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    c_1 = _e111;
    let _e112 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e112))) == 0i) {
        let _e117 = c_1;
        param = _e117.xyz;
        let _e119 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = (*slot);
    if (lightmap_slot == (_e126 + 1i)) {
        let _e131 = unnamed.worldLightParams[0u];
        let _e132 = c_1;
        let _e134 = (_e132.xyz * _e131);
        c_1[0u] = _e134.x;
        c_1[1u] = _e134.y;
        c_1[2u] = _e134.z;
    }
    let _e141 = c_1;
    return _e141;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_3: vec3<f32>;
    var color0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var color1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color2_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color2_1: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color1_2: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color2_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color1_3: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var color2_3: vec4<f32>;
    var param_28: u32;
    var param_29: vec2<f32>;
    var param_30: i32;
    var color1_4: vec4<f32>;
    var param_31: u32;
    var param_32: vec2<f32>;
    var param_33: i32;
    var color2_4: vec4<f32>;
    var param_34: u32;
    var param_35: vec2<f32>;
    var param_36: i32;
    var color1_5: vec4<f32>;
    var param_37: u32;
    var param_38: vec2<f32>;
    var param_39: i32;
    var color2_5: vec4<f32>;
    var param_40: u32;
    var param_41: vec2<f32>;
    var param_42: i32;
    var color1_6: vec4<f32>;
    var param_43: u32;
    var param_44: vec2<f32>;
    var param_45: i32;
    var color2_6: vec4<f32>;
    var param_46: u32;
    var param_47: vec2<f32>;
    var param_48: i32;
    var orm: vec3<f32>;
    var ao: f32;
    var roughness_1: f32;
    var metalness: f32;
    var ibl_n: vec3<f32>;
    var ibl_v: vec3<f32>;
    var F0_1: vec3<f32>;
    var NdotV: f32;
    var F_amb: vec3<f32>;
    var param_49: f32;
    var param_50: vec3<f32>;
    var param_51: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e173 = unnamed.packed_indices[0i][3u];
    let _e179 = unnamed.packed_indices[0i][3u];
    let _e184 = fog_tex_coord_1;
    let _e185 = textureSample(wired_bindless_images[(_e173 & 4095u)], wired_bindless_samplers[((_e179 >> bitcast<u32>(12i)) & 255u)], _e184);
    fog = _e185;
    let _e186 = frag_color0In_1;
    param_1 = _e186.xyz;
    let _e188 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e190 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e188.x, _e188.y, _e188.z, _e190);
    let _e195 = frag_color1In_1;
    param_2 = _e195.xyz;
    let _e197 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e199 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e197.x, _e197.y, _e197.z, _e199);
    let _e204 = frag_color2In_1;
    param_3 = _e204.xyz;
    let _e206 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e208 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e206.x, _e206.y, _e206.z, _e208);
    param_4 = 0u;
    let _e213 = frag_tex_coord0_1;
    param_5 = _e213;
    param_6 = 0i;
    let _e214 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e215 = frag_color0_;
    color0_ = (_e214 * _e215);
    if override_type_3_2 {
        param_7 = 1u;
        let _e217 = frag_tex_coord1_1;
        param_8 = _e217;
        param_9 = 1i;
        let _e218 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e219 = frag_color1_;
        color1_ = (_e218 * _e219);
        param_10 = 2u;
        let _e221 = frag_tex_coord2_1;
        param_11 = _e221;
        param_12 = 2i;
        let _e222 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e223 = frag_color2_;
        color2_ = (_e222 * _e223);
        let _e225 = color0_;
        let _e227 = color1_;
        let _e230 = color2_;
        let _e232 = ((_e225.xyz + _e227.xyz) + _e230.xyz);
        let _e234 = color0_[3u];
        let _e236 = color1_[3u];
        let _e239 = color2_[3u];
        base = vec4<f32>(_e232.x, _e232.y, _e232.z, ((_e234 * _e236) * _e239));
    } else {
        if override_type_3_3 {
            param_13 = 1u;
            let _e245 = frag_tex_coord1_1;
            param_14 = _e245;
            param_15 = 1i;
            let _e246 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e247 = frag_color1_;
            color1_1 = (_e246 * _e247);
            param_16 = 2u;
            let _e249 = frag_tex_coord2_1;
            param_17 = _e249;
            param_18 = 2i;
            let _e250 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e251 = frag_color2_;
            color2_1 = (_e250 * _e251);
            let _e254 = color0_[3u];
            let _e255 = color0_;
            color0_ = (_e255 * _e254);
            let _e258 = color1_1[3u];
            let _e259 = color1_1;
            color1_1 = (_e259 * _e258);
            let _e262 = color2_1[3u];
            let _e263 = color2_1;
            color2_1 = (_e263 * _e262);
            let _e265 = color0_;
            let _e267 = color1_1;
            let _e270 = color2_1;
            let _e272 = ((_e265.xyz + _e267.xyz) + _e270.xyz);
            let _e274 = color0_[3u];
            let _e276 = color1_1[3u];
            let _e279 = color2_1[3u];
            base = vec4<f32>(_e272.x, _e272.y, _e272.z, ((_e274 * _e276) * _e279));
        } else {
            if override_type_3_4 {
                param_19 = 1u;
                let _e285 = frag_tex_coord1_1;
                param_20 = _e285;
                param_21 = 1i;
                let _e286 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e287 = frag_color1_;
                color1_2 = (_e286 * _e287);
                param_22 = 2u;
                let _e289 = frag_tex_coord2_1;
                param_23 = _e289;
                param_24 = 2i;
                let _e290 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e291 = frag_color2_;
                color2_2 = (_e290 * _e291);
                let _e294 = color0_[3u];
                let _e296 = color0_;
                color0_ = (_e296 * (1f - _e294));
                let _e299 = color1_2[3u];
                let _e301 = color1_2;
                color1_2 = (_e301 * (1f - _e299));
                let _e304 = color2_2[3u];
                let _e306 = color2_2;
                color2_2 = (_e306 * (1f - _e304));
                let _e308 = color0_;
                let _e310 = color1_2;
                let _e313 = color2_2;
                let _e315 = ((_e308.xyz + _e310.xyz) + _e313.xyz);
                let _e317 = color0_[3u];
                let _e319 = color1_2[3u];
                let _e322 = color2_2[3u];
                base = vec4<f32>(_e315.x, _e315.y, _e315.z, ((_e317 * _e319) * _e322));
            } else {
                if override_type_3_5 {
                    param_25 = 1u;
                    let _e328 = frag_tex_coord1_1;
                    param_26 = _e328;
                    param_27 = 1i;
                    let _e329 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e330 = frag_color1_;
                    color1_3 = (_e329 * _e330);
                    param_28 = 2u;
                    let _e332 = frag_tex_coord2_1;
                    param_29 = _e332;
                    param_30 = 2i;
                    let _e333 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e334 = frag_color2_;
                    color2_3 = (_e333 * _e334);
                    let _e336 = color0_;
                    let _e337 = color1_3;
                    let _e339 = color1_3[3u];
                    let _e342 = color2_3;
                    let _e344 = color2_3[3u];
                    base = mix(mix(_e336, _e337, vec4(_e339)), _e342, vec4(_e344));
                } else {
                    if override_type_3_6 {
                        param_31 = 1u;
                        let _e347 = frag_tex_coord1_1;
                        param_32 = _e347;
                        param_33 = 1i;
                        let _e348 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e349 = frag_color1_;
                        color1_4 = (_e348 * _e349);
                        param_34 = 2u;
                        let _e351 = frag_tex_coord2_1;
                        param_35 = _e351;
                        param_36 = 2i;
                        let _e352 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e353 = frag_color2_;
                        color2_4 = (_e352 * _e353);
                        let _e355 = color2_4;
                        let _e356 = color1_4;
                        let _e357 = color0_;
                        let _e359 = color1_4[3u];
                        let _e363 = color2_4[3u];
                        base = mix(_e355, mix(_e356, _e357, vec4(_e359)), vec4(_e363));
                    } else {
                        if override_type_3_7 {
                            param_37 = 1u;
                            let _e366 = frag_tex_coord1_1;
                            param_38 = _e366;
                            param_39 = 1i;
                            let _e367 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e368 = frag_color1_;
                            color1_5 = (_e367 * _e368);
                            param_40 = 2u;
                            let _e370 = frag_tex_coord2_1;
                            param_41 = _e370;
                            param_42 = 2i;
                            let _e371 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e372 = frag_color2_;
                            color2_5 = (_e371 * _e372);
                            let _e374 = color2_5;
                            let _e376 = color2_5[3u];
                            let _e379 = color1_5;
                            let _e381 = color1_5[3u];
                            let _e385 = color0_;
                            base = (((_e374 + vec4(_e376)) * (_e379 + vec4(_e381))) * _e385);
                        } else {
                            param_43 = 1u;
                            let _e387 = frag_tex_coord1_1;
                            param_44 = _e387;
                            param_45 = 1i;
                            let _e388 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e389 = frag_color1_;
                            color1_6 = (_e388 * _e389);
                            param_46 = 2u;
                            let _e391 = frag_tex_coord2_1;
                            param_47 = _e391;
                            param_48 = 2i;
                            let _e392 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e393 = frag_color2_;
                            color2_6 = (_e392 * _e393);
                            let _e395 = color0_;
                            let _e397 = color1_6;
                            let _e400 = color2_6;
                            let _e402 = ((_e395.xyz * _e397.xyz) * _e400.xyz);
                            base[0u] = _e402.x;
                            base[1u] = _e402.y;
                            base[2u] = _e402.z;
                            let _e410 = color0_[3u];
                            let _e412 = color1_6[3u];
                            let _e415 = color2_6[3u];
                            base[3u] = ((_e410 * _e412) * _e415);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e421 = unnamed.packed_indices[2i][0u];
        let _e427 = unnamed.packed_indices[2i][0u];
        let _e432 = frag_tex_coord0_1;
        let _e433 = textureSample(wired_bindless_images[(_e421 & 4095u)], wired_bindless_samplers[((_e427 >> bitcast<u32>(12i)) & 255u)], _e432);
        orm = _e433.xyz;
        let _e436 = orm[0u];
        ao = _e436;
        let _e438 = orm[1u];
        roughness_1 = clamp(_e438, 0.04f, 1f);
        let _e441 = orm[2u];
        metalness = _e441;
        let _e442 = ibl_N_1;
        ibl_n = normalize(_e442);
        let _e444 = ibl_V_1;
        ibl_v = normalize(_e444);
        let _e446 = base;
        let _e448 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e446.xyz, vec3(_e448));
        let _e451 = ibl_n;
        let _e452 = ibl_v;
        NdotV = max(dot(_e451, _e452), 0f);
        let _e455 = NdotV;
        param_49 = _e455;
        let _e456 = F0_1;
        param_50 = _e456;
        let _e457 = roughness_1;
        param_51 = _e457;
        let _e458 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_49), (&param_50), (&param_51));
        F_amb = _e458;
        let _e459 = ibl_v;
        let _e461 = ibl_n;
        R = reflect(-(_e459), _e461);
        let _e463 = R;
        let _e464 = roughness_1;
        let _e466 = textureSampleLevel(radianceCube, radianceCube_sampler, _e463, (_e464 * 5f));
        prefiltered = _e466.xyz;
        let _e468 = NdotV;
        let _e469 = roughness_1;
        let _e471 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e468, _e469), 0f);
        envBRDF = _e471.xy;
        let _e473 = prefiltered;
        let _e474 = F_amb;
        let _e476 = envBRDF[0u];
        let _e479 = envBRDF[1u];
        let _e483 = ao;
        specularIBL = ((_e473 * ((_e474 * _e476) + vec3(_e479))) * _e483);
        let _e485 = gl_FragCoord_1;
        let _e487 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e485.xy / vec2<f32>(vec2<i32>(_e487)));
        let _e491 = gtaoUV;
        let _e492 = textureSample(gtaoMap, gtaoMap_sampler, _e491);
        gtao_vis = _e492.x;
        let _e494 = gtao_vis;
        let _e495 = specularIBL;
        specularIBL = (_e495 * _e494);
        let _e497 = specularIBL;
        let _e498 = base;
        let _e500 = (_e498.xyz + _e497);
        base[0u] = _e500.x;
        base[1u] = _e500.y;
        base[2u] = _e500.z;
    }
    let _e507 = wired_advanced_fog_enabled_u0028_();
    if _e507 {
        let _e508 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e508;
        if override_type_3_9 {
            let _e509 = fogAmount;
            let _e511 = base;
            let _e513 = (_e511.xyz * (1f - _e509));
            base[0u] = _e513.x;
            base[1u] = _e513.y;
            base[2u] = _e513.z;
        } else {
            if override_type_3_10 {
                let _e520 = fogAmount;
                let _e522 = base;
                base = (_e522 * (1f - _e520));
            } else {
                if override_type_3_11 {
                    let _e524 = fogAmount;
                    let _e527 = base[3u];
                    base[3u] = (_e527 * (1f - _e524));
                } else {
                    let _e530 = base;
                    let _e533 = unnamed.advancedFogColorDensity;
                    let _e535 = fogAmount;
                    let _e537 = mix(_e530.xyz, _e533.xyz, vec3(_e535));
                    base[0u] = _e537.x;
                    base[1u] = _e537.y;
                    base[2u] = _e537.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e544 = base;
            let _e547 = fog[3u];
            let _e549 = (_e544.xyz * (1f - _e547));
            base[0u] = _e549.x;
            base[1u] = _e549.y;
            base[2u] = _e549.z;
        } else {
            if override_type_3_13 {
                let _e556 = base;
                let _e558 = fog[3u];
                base = (_e556 * (1f - _e558));
            } else {
                if override_type_3_14 {
                    let _e562 = base[3u];
                    let _e564 = fog[3u];
                    base[3u] = (_e562 * (1f - _e564));
                } else {
                    let _e568 = base;
                    let _e569 = fog;
                    let _e571 = unnamed.fogColor;
                    let _e574 = fog[3u];
                    base = mix(_e568, (_e569 * _e571), vec4(_e574));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e578 = base[3u];
        if (_e578 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e580 = base;
            let _e582 = base;
            if (dot(_e580.xyz, _e582.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e586 = base;
    out_color = _e586;
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
