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
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_11_: bool = (tex_mode == 1i);
override override_type_11_1: bool = (tex_mode == 2i);
override override_type_11_2: bool = (override_type_11_ || override_type_11_1);
override override_type_11_3: bool = (tex_mode == 3i);
override override_type_11_4: bool = (tex_mode == 4i);
override override_type_11_5: bool = (tex_mode == 5i);
override override_type_11_6: bool = (tex_mode == 6i);
override override_type_11_7: bool = (tex_mode == 7i);
@id(14) override ibl_enabled: i32 = 0i;
override override_type_11_8: bool = (ibl_enabled != 0i);
@id(10) override acff: i32 = 0i;
override override_type_11_9: bool = (acff == 1i);
override override_type_11_10: bool = (acff == 2i);
override override_type_11_11: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_12: bool = (discard_mode == 1i);
override override_type_11_13: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
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
var<private> gl_FragCoord_1: vec4<f32>;
@group(2) @binding(4) 
var gtaoMap: texture_2d<f32>;
@group(2) @binding(36) 
var gtaoMap_sampler: sampler;
var<private> out_color: vec4<f32>;
@group(2) @binding(2) 
var irradianceCube: texture_cube<f32>;
@group(2) @binding(34) 
var irradianceCube_sampler: sampler;

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e83 = (*cosTheta);
    t = (1f - _e83);
    let _e85 = t;
    let _e86 = t;
    t2_ = (_e85 * _e86);
    let _e88 = (*roughness);
    let _e91 = (*F0_);
    Fmax = max(vec3((1f - _e88)), _e91);
    let _e93 = (*F0_);
    let _e94 = Fmax;
    let _e95 = (*F0_);
    let _e97 = t2_;
    let _e98 = t2_;
    let _e100 = t;
    return (_e93 + ((_e94 - _e95) * ((_e97 * _e98) * _e100)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e81 = (*c);
    (*c) = max(_e81, vec3<f32>(0f, 0f, 0f));
    let _e83 = (*c);
    cutoff = (_e83 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e85 = (*c);
    lo = (_e85 / vec3(12.92f));
    let _e88 = (*c);
    hi = pow(((_e88 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e93 = hi;
    let _e94 = lo;
    let _e95 = cutoff;
    return mix(_e93, _e94, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e95));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e104 = (*uv);
    let _e105 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    c_1 = _e105;
    let _e106 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e106))) == 0i) {
        let _e111 = c_1;
        param = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e125 = unnamed.worldLightParams[0u];
        let _e126 = c_1;
        let _e128 = (_e126.xyz * _e125);
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = c_1;
    return _e135;
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

    let _e166 = unnamed.packed_indices[0i][3u];
    let _e172 = unnamed.packed_indices[0i][3u];
    let _e177 = fog_tex_coord_1;
    let _e178 = textureSample(wired_bindless_images[(_e166 & 4095u)], wired_bindless_samplers[((_e172 >> bitcast<u32>(12i)) & 255u)], _e177);
    fog = _e178;
    let _e179 = frag_color0In_1;
    param_1 = _e179.xyz;
    let _e181 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e183 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e181.x, _e181.y, _e181.z, _e183);
    let _e188 = frag_color1In_1;
    param_2 = _e188.xyz;
    let _e190 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e192 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e190.x, _e190.y, _e190.z, _e192);
    let _e197 = frag_color2In_1;
    param_3 = _e197.xyz;
    let _e199 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e201 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e199.x, _e199.y, _e199.z, _e201);
    param_4 = 0u;
    let _e206 = frag_tex_coord0_1;
    param_5 = _e206;
    param_6 = 0i;
    let _e207 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e208 = frag_color0_;
    color0_ = (_e207 * _e208);
    if override_type_11_2 {
        param_7 = 1u;
        let _e210 = frag_tex_coord1_1;
        param_8 = _e210;
        param_9 = 1i;
        let _e211 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e212 = frag_color1_;
        color1_ = (_e211 * _e212);
        param_10 = 2u;
        let _e214 = frag_tex_coord2_1;
        param_11 = _e214;
        param_12 = 2i;
        let _e215 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e216 = frag_color2_;
        color2_ = (_e215 * _e216);
        let _e218 = color0_;
        let _e220 = color1_;
        let _e223 = color2_;
        let _e225 = ((_e218.xyz + _e220.xyz) + _e223.xyz);
        let _e227 = color0_[3u];
        let _e229 = color1_[3u];
        let _e232 = color2_[3u];
        base = vec4<f32>(_e225.x, _e225.y, _e225.z, ((_e227 * _e229) * _e232));
    } else {
        if override_type_11_3 {
            param_13 = 1u;
            let _e238 = frag_tex_coord1_1;
            param_14 = _e238;
            param_15 = 1i;
            let _e239 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e240 = frag_color1_;
            color1_1 = (_e239 * _e240);
            param_16 = 2u;
            let _e242 = frag_tex_coord2_1;
            param_17 = _e242;
            param_18 = 2i;
            let _e243 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e244 = frag_color2_;
            color2_1 = (_e243 * _e244);
            let _e247 = color0_[3u];
            let _e248 = color0_;
            color0_ = (_e248 * _e247);
            let _e251 = color1_1[3u];
            let _e252 = color1_1;
            color1_1 = (_e252 * _e251);
            let _e255 = color2_1[3u];
            let _e256 = color2_1;
            color2_1 = (_e256 * _e255);
            let _e258 = color0_;
            let _e260 = color1_1;
            let _e263 = color2_1;
            let _e265 = ((_e258.xyz + _e260.xyz) + _e263.xyz);
            let _e267 = color0_[3u];
            let _e269 = color1_1[3u];
            let _e272 = color2_1[3u];
            base = vec4<f32>(_e265.x, _e265.y, _e265.z, ((_e267 * _e269) * _e272));
        } else {
            if override_type_11_4 {
                param_19 = 1u;
                let _e278 = frag_tex_coord1_1;
                param_20 = _e278;
                param_21 = 1i;
                let _e279 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e280 = frag_color1_;
                color1_2 = (_e279 * _e280);
                param_22 = 2u;
                let _e282 = frag_tex_coord2_1;
                param_23 = _e282;
                param_24 = 2i;
                let _e283 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e284 = frag_color2_;
                color2_2 = (_e283 * _e284);
                let _e287 = color0_[3u];
                let _e289 = color0_;
                color0_ = (_e289 * (1f - _e287));
                let _e292 = color1_2[3u];
                let _e294 = color1_2;
                color1_2 = (_e294 * (1f - _e292));
                let _e297 = color2_2[3u];
                let _e299 = color2_2;
                color2_2 = (_e299 * (1f - _e297));
                let _e301 = color0_;
                let _e303 = color1_2;
                let _e306 = color2_2;
                let _e308 = ((_e301.xyz + _e303.xyz) + _e306.xyz);
                let _e310 = color0_[3u];
                let _e312 = color1_2[3u];
                let _e315 = color2_2[3u];
                base = vec4<f32>(_e308.x, _e308.y, _e308.z, ((_e310 * _e312) * _e315));
            } else {
                if override_type_11_5 {
                    param_25 = 1u;
                    let _e321 = frag_tex_coord1_1;
                    param_26 = _e321;
                    param_27 = 1i;
                    let _e322 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e323 = frag_color1_;
                    color1_3 = (_e322 * _e323);
                    param_28 = 2u;
                    let _e325 = frag_tex_coord2_1;
                    param_29 = _e325;
                    param_30 = 2i;
                    let _e326 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e327 = frag_color2_;
                    color2_3 = (_e326 * _e327);
                    let _e329 = color0_;
                    let _e330 = color1_3;
                    let _e332 = color1_3[3u];
                    let _e335 = color2_3;
                    let _e337 = color2_3[3u];
                    base = mix(mix(_e329, _e330, vec4(_e332)), _e335, vec4(_e337));
                } else {
                    if override_type_11_6 {
                        param_31 = 1u;
                        let _e340 = frag_tex_coord1_1;
                        param_32 = _e340;
                        param_33 = 1i;
                        let _e341 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e342 = frag_color1_;
                        color1_4 = (_e341 * _e342);
                        param_34 = 2u;
                        let _e344 = frag_tex_coord2_1;
                        param_35 = _e344;
                        param_36 = 2i;
                        let _e345 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e346 = frag_color2_;
                        color2_4 = (_e345 * _e346);
                        let _e348 = color2_4;
                        let _e349 = color1_4;
                        let _e350 = color0_;
                        let _e352 = color1_4[3u];
                        let _e356 = color2_4[3u];
                        base = mix(_e348, mix(_e349, _e350, vec4(_e352)), vec4(_e356));
                    } else {
                        if override_type_11_7 {
                            param_37 = 1u;
                            let _e359 = frag_tex_coord1_1;
                            param_38 = _e359;
                            param_39 = 1i;
                            let _e360 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e361 = frag_color1_;
                            color1_5 = (_e360 * _e361);
                            param_40 = 2u;
                            let _e363 = frag_tex_coord2_1;
                            param_41 = _e363;
                            param_42 = 2i;
                            let _e364 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e365 = frag_color2_;
                            color2_5 = (_e364 * _e365);
                            let _e367 = color2_5;
                            let _e369 = color2_5[3u];
                            let _e372 = color1_5;
                            let _e374 = color1_5[3u];
                            let _e378 = color0_;
                            base = (((_e367 + vec4(_e369)) * (_e372 + vec4(_e374))) * _e378);
                        } else {
                            param_43 = 1u;
                            let _e380 = frag_tex_coord1_1;
                            param_44 = _e380;
                            param_45 = 1i;
                            let _e381 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e382 = frag_color1_;
                            color1_6 = (_e381 * _e382);
                            param_46 = 2u;
                            let _e384 = frag_tex_coord2_1;
                            param_47 = _e384;
                            param_48 = 2i;
                            let _e385 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e386 = frag_color2_;
                            color2_6 = (_e385 * _e386);
                            let _e388 = color0_;
                            let _e390 = color1_6;
                            let _e393 = color2_6;
                            let _e395 = ((_e388.xyz * _e390.xyz) * _e393.xyz);
                            base[0u] = _e395.x;
                            base[1u] = _e395.y;
                            base[2u] = _e395.z;
                            let _e403 = color0_[3u];
                            let _e405 = color1_6[3u];
                            let _e408 = color2_6[3u];
                            base[3u] = ((_e403 * _e405) * _e408);
                        }
                    }
                }
            }
        }
    }
    if override_type_11_8 {
        let _e414 = unnamed.packed_indices[2i][0u];
        let _e420 = unnamed.packed_indices[2i][0u];
        let _e425 = frag_tex_coord0_1;
        let _e426 = textureSample(wired_bindless_images[(_e414 & 4095u)], wired_bindless_samplers[((_e420 >> bitcast<u32>(12i)) & 255u)], _e425);
        orm = _e426.xyz;
        let _e429 = orm[0u];
        ao = _e429;
        let _e431 = orm[1u];
        roughness_1 = clamp(_e431, 0.04f, 1f);
        let _e434 = orm[2u];
        metalness = _e434;
        let _e435 = ibl_N_1;
        ibl_n = normalize(_e435);
        let _e437 = ibl_V_1;
        ibl_v = normalize(_e437);
        let _e439 = base;
        let _e441 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e439.xyz, vec3(_e441));
        let _e444 = ibl_n;
        let _e445 = ibl_v;
        NdotV = max(dot(_e444, _e445), 0f);
        let _e448 = NdotV;
        param_49 = _e448;
        let _e449 = F0_1;
        param_50 = _e449;
        let _e450 = roughness_1;
        param_51 = _e450;
        let _e451 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_49), (&param_50), (&param_51));
        F_amb = _e451;
        let _e452 = ibl_v;
        let _e454 = ibl_n;
        R = reflect(-(_e452), _e454);
        let _e456 = R;
        let _e457 = roughness_1;
        let _e459 = textureSampleLevel(radianceCube, radianceCube_sampler, _e456, (_e457 * 5f));
        prefiltered = _e459.xyz;
        let _e461 = NdotV;
        let _e462 = roughness_1;
        let _e464 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e461, _e462), 0f);
        envBRDF = _e464.xy;
        let _e466 = prefiltered;
        let _e467 = F_amb;
        let _e469 = envBRDF[0u];
        let _e472 = envBRDF[1u];
        let _e476 = ao;
        specularIBL = ((_e466 * ((_e467 * _e469) + vec3(_e472))) * _e476);
        let _e478 = gl_FragCoord_1;
        let _e480 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e478.xy / vec2<f32>(vec2<i32>(_e480)));
        let _e484 = gtaoUV;
        let _e485 = textureSample(gtaoMap, gtaoMap_sampler, _e484);
        gtao_vis = _e485.x;
        let _e487 = gtao_vis;
        let _e488 = specularIBL;
        specularIBL = (_e488 * _e487);
        let _e490 = specularIBL;
        let _e491 = base;
        let _e493 = (_e491.xyz + _e490);
        base[0u] = _e493.x;
        base[1u] = _e493.y;
        base[2u] = _e493.z;
    }
    if override_type_11_9 {
        let _e500 = base;
        let _e503 = fog[3u];
        let _e505 = (_e500.xyz * (1f - _e503));
        base[0u] = _e505.x;
        base[1u] = _e505.y;
        base[2u] = _e505.z;
    } else {
        if override_type_11_10 {
            let _e512 = base;
            let _e514 = fog[3u];
            base = (_e512 * (1f - _e514));
        } else {
            if override_type_11_11 {
                let _e518 = base[3u];
                let _e520 = fog[3u];
                base[3u] = (_e518 * (1f - _e520));
            } else {
                let _e524 = base;
                let _e525 = fog;
                let _e527 = unnamed.fogColor;
                let _e530 = fog[3u];
                base = mix(_e524, (_e525 * _e527), vec4(_e530));
            }
        }
    }
    if override_type_11_12 {
        let _e534 = base[3u];
        if (_e534 == 0f) {
            discard;
        }
    } else {
        if override_type_11_13 {
            let _e536 = base;
            let _e538 = base;
            if (dot(_e536.xyz, _e538.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e542 = base;
    out_color = _e542;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e21 = out_color;
    return _e21;
}
