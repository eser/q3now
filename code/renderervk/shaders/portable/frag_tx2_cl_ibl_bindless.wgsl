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
@id(7) override discard_mode: i32 = 0i;
override override_type_11_9: bool = (discard_mode == 1i);
override override_type_11_10: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
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

    let _e78 = (*cosTheta);
    t = (1f - _e78);
    let _e80 = t;
    let _e81 = t;
    t2_ = (_e80 * _e81);
    let _e83 = (*roughness);
    let _e86 = (*F0_);
    Fmax = max(vec3((1f - _e83)), _e86);
    let _e88 = (*F0_);
    let _e89 = Fmax;
    let _e90 = (*F0_);
    let _e92 = t2_;
    let _e93 = t2_;
    let _e95 = t;
    return (_e88 + ((_e89 - _e90) * ((_e92 * _e93) * _e95)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e76 = (*c);
    (*c) = max(_e76, vec3<f32>(0f, 0f, 0f));
    let _e78 = (*c);
    cutoff = (_e78 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e80 = (*c);
    lo = (_e80 / vec3(12.92f));
    let _e83 = (*c);
    hi = pow(((_e83 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e88 = hi;
    let _e89 = lo;
    let _e90 = cutoff;
    return mix(_e88, _e89, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e90));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e99 = (*uv);
    let _e100 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    c_1 = _e100;
    let _e101 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e101))) == 0i) {
        let _e106 = c_1;
        param = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e120 = unnamed.worldLightParams[0u];
        let _e121 = c_1;
        let _e123 = (_e121.xyz * _e120);
        c_1[0u] = _e123.x;
        c_1[1u] = _e123.y;
        c_1[2u] = _e123.z;
    }
    let _e130 = c_1;
    return _e130;
}

fn main_1() {
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

    let _e157 = frag_color0In_1;
    param_1 = _e157.xyz;
    let _e159 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e161 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e159.x, _e159.y, _e159.z, _e161);
    let _e166 = frag_color1In_1;
    param_2 = _e166.xyz;
    let _e168 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e170 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e168.x, _e168.y, _e168.z, _e170);
    let _e175 = frag_color2In_1;
    param_3 = _e175.xyz;
    let _e177 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e179 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e177.x, _e177.y, _e177.z, _e179);
    param_4 = 0u;
    let _e184 = frag_tex_coord0_1;
    param_5 = _e184;
    param_6 = 0i;
    let _e185 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e186 = frag_color0_;
    color0_ = (_e185 * _e186);
    if override_type_11_2 {
        param_7 = 1u;
        let _e188 = frag_tex_coord1_1;
        param_8 = _e188;
        param_9 = 1i;
        let _e189 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e190 = frag_color1_;
        color1_ = (_e189 * _e190);
        param_10 = 2u;
        let _e192 = frag_tex_coord2_1;
        param_11 = _e192;
        param_12 = 2i;
        let _e193 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e194 = frag_color2_;
        color2_ = (_e193 * _e194);
        let _e196 = color0_;
        let _e198 = color1_;
        let _e201 = color2_;
        let _e203 = ((_e196.xyz + _e198.xyz) + _e201.xyz);
        let _e205 = color0_[3u];
        let _e207 = color1_[3u];
        let _e210 = color2_[3u];
        base = vec4<f32>(_e203.x, _e203.y, _e203.z, ((_e205 * _e207) * _e210));
    } else {
        if override_type_11_3 {
            param_13 = 1u;
            let _e216 = frag_tex_coord1_1;
            param_14 = _e216;
            param_15 = 1i;
            let _e217 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e218 = frag_color1_;
            color1_1 = (_e217 * _e218);
            param_16 = 2u;
            let _e220 = frag_tex_coord2_1;
            param_17 = _e220;
            param_18 = 2i;
            let _e221 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e222 = frag_color2_;
            color2_1 = (_e221 * _e222);
            let _e225 = color0_[3u];
            let _e226 = color0_;
            color0_ = (_e226 * _e225);
            let _e229 = color1_1[3u];
            let _e230 = color1_1;
            color1_1 = (_e230 * _e229);
            let _e233 = color2_1[3u];
            let _e234 = color2_1;
            color2_1 = (_e234 * _e233);
            let _e236 = color0_;
            let _e238 = color1_1;
            let _e241 = color2_1;
            let _e243 = ((_e236.xyz + _e238.xyz) + _e241.xyz);
            let _e245 = color0_[3u];
            let _e247 = color1_1[3u];
            let _e250 = color2_1[3u];
            base = vec4<f32>(_e243.x, _e243.y, _e243.z, ((_e245 * _e247) * _e250));
        } else {
            if override_type_11_4 {
                param_19 = 1u;
                let _e256 = frag_tex_coord1_1;
                param_20 = _e256;
                param_21 = 1i;
                let _e257 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e258 = frag_color1_;
                color1_2 = (_e257 * _e258);
                param_22 = 2u;
                let _e260 = frag_tex_coord2_1;
                param_23 = _e260;
                param_24 = 2i;
                let _e261 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e262 = frag_color2_;
                color2_2 = (_e261 * _e262);
                let _e265 = color0_[3u];
                let _e267 = color0_;
                color0_ = (_e267 * (1f - _e265));
                let _e270 = color1_2[3u];
                let _e272 = color1_2;
                color1_2 = (_e272 * (1f - _e270));
                let _e275 = color2_2[3u];
                let _e277 = color2_2;
                color2_2 = (_e277 * (1f - _e275));
                let _e279 = color0_;
                let _e281 = color1_2;
                let _e284 = color2_2;
                let _e286 = ((_e279.xyz + _e281.xyz) + _e284.xyz);
                let _e288 = color0_[3u];
                let _e290 = color1_2[3u];
                let _e293 = color2_2[3u];
                base = vec4<f32>(_e286.x, _e286.y, _e286.z, ((_e288 * _e290) * _e293));
            } else {
                if override_type_11_5 {
                    param_25 = 1u;
                    let _e299 = frag_tex_coord1_1;
                    param_26 = _e299;
                    param_27 = 1i;
                    let _e300 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e301 = frag_color1_;
                    color1_3 = (_e300 * _e301);
                    param_28 = 2u;
                    let _e303 = frag_tex_coord2_1;
                    param_29 = _e303;
                    param_30 = 2i;
                    let _e304 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e305 = frag_color2_;
                    color2_3 = (_e304 * _e305);
                    let _e307 = color0_;
                    let _e308 = color1_3;
                    let _e310 = color1_3[3u];
                    let _e313 = color2_3;
                    let _e315 = color2_3[3u];
                    base = mix(mix(_e307, _e308, vec4(_e310)), _e313, vec4(_e315));
                } else {
                    if override_type_11_6 {
                        param_31 = 1u;
                        let _e318 = frag_tex_coord1_1;
                        param_32 = _e318;
                        param_33 = 1i;
                        let _e319 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e320 = frag_color1_;
                        color1_4 = (_e319 * _e320);
                        param_34 = 2u;
                        let _e322 = frag_tex_coord2_1;
                        param_35 = _e322;
                        param_36 = 2i;
                        let _e323 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e324 = frag_color2_;
                        color2_4 = (_e323 * _e324);
                        let _e326 = color2_4;
                        let _e327 = color1_4;
                        let _e328 = color0_;
                        let _e330 = color1_4[3u];
                        let _e334 = color2_4[3u];
                        base = mix(_e326, mix(_e327, _e328, vec4(_e330)), vec4(_e334));
                    } else {
                        if override_type_11_7 {
                            param_37 = 1u;
                            let _e337 = frag_tex_coord1_1;
                            param_38 = _e337;
                            param_39 = 1i;
                            let _e338 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e339 = frag_color1_;
                            color1_5 = (_e338 * _e339);
                            param_40 = 2u;
                            let _e341 = frag_tex_coord2_1;
                            param_41 = _e341;
                            param_42 = 2i;
                            let _e342 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e343 = frag_color2_;
                            color2_5 = (_e342 * _e343);
                            let _e345 = color2_5;
                            let _e347 = color2_5[3u];
                            let _e350 = color1_5;
                            let _e352 = color1_5[3u];
                            let _e356 = color0_;
                            base = (((_e345 + vec4(_e347)) * (_e350 + vec4(_e352))) * _e356);
                        } else {
                            param_43 = 1u;
                            let _e358 = frag_tex_coord1_1;
                            param_44 = _e358;
                            param_45 = 1i;
                            let _e359 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e360 = frag_color1_;
                            color1_6 = (_e359 * _e360);
                            param_46 = 2u;
                            let _e362 = frag_tex_coord2_1;
                            param_47 = _e362;
                            param_48 = 2i;
                            let _e363 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e364 = frag_color2_;
                            color2_6 = (_e363 * _e364);
                            let _e366 = color0_;
                            let _e368 = color1_6;
                            let _e371 = color2_6;
                            let _e373 = ((_e366.xyz * _e368.xyz) * _e371.xyz);
                            base[0u] = _e373.x;
                            base[1u] = _e373.y;
                            base[2u] = _e373.z;
                            let _e381 = color0_[3u];
                            let _e383 = color1_6[3u];
                            let _e386 = color2_6[3u];
                            base[3u] = ((_e381 * _e383) * _e386);
                        }
                    }
                }
            }
        }
    }
    if override_type_11_8 {
        let _e392 = unnamed.packed_indices[2i][0u];
        let _e398 = unnamed.packed_indices[2i][0u];
        let _e403 = frag_tex_coord0_1;
        let _e404 = textureSample(wired_bindless_images[(_e392 & 4095u)], wired_bindless_samplers[((_e398 >> bitcast<u32>(12i)) & 255u)], _e403);
        orm = _e404.xyz;
        let _e407 = orm[0u];
        ao = _e407;
        let _e409 = orm[1u];
        roughness_1 = clamp(_e409, 0.04f, 1f);
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
        param_49 = _e426;
        let _e427 = F0_1;
        param_50 = _e427;
        let _e428 = roughness_1;
        param_51 = _e428;
        let _e429 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_49), (&param_50), (&param_51));
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
    if override_type_11_9 {
        let _e479 = base[3u];
        if (_e479 == 0f) {
            discard;
        }
    } else {
        if override_type_11_10 {
            let _e481 = base;
            let _e483 = base;
            if (dot(_e481.xyz, _e483.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e487 = base;
    out_color = _e487;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
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
    let _e19 = out_color;
    return _e19;
}
