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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
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

    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e78 + 0.5f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e85 = fogType;
    let _e88 = fogType;
    return (((_e83 > 0.5f) && (_e85 >= 1i)) && (_e88 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e78 = wired_advanced_fog_enabled_u0028_();
    if !(_e78) {
        return 0f;
    }
    let _e81 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e81, 0.000001f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e86 + 0.5f));
    let _e89 = fogType_1;
    if (_e89 == 1i) {
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e93 <= 0f) {
            return 0f;
        }
        let _e95 = viewDepth;
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e95 / _e98), 0f, 1f);
    }
    let _e103 = unnamed.advancedFogColorDensity[3u];
    let _e105 = viewDepth;
    opticalDepth = (max(_e103, 0f) * _e105);
    let _e107 = fogType_1;
    if (_e107 == 2i) {
        let _e109 = opticalDepth;
        return clamp((1f - exp(-(_e109))), 0f, 1f);
    }
    let _e114 = opticalDepth;
    let _e115 = opticalDepth;
    return clamp((1f - exp(-((_e114 * _e115)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e81 = (*cosTheta);
    t = (1f - _e81);
    let _e83 = t;
    let _e84 = t;
    t2_ = (_e83 * _e84);
    let _e86 = (*roughness);
    let _e89 = (*F0_);
    Fmax = max(vec3((1f - _e86)), _e89);
    let _e91 = (*F0_);
    let _e92 = Fmax;
    let _e93 = (*F0_);
    let _e95 = t2_;
    let _e96 = t2_;
    let _e98 = t;
    return (_e91 + ((_e92 - _e93) * ((_e95 * _e96) * _e98)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e79 = (*c);
    (*c) = max(_e79, vec3<f32>(0f, 0f, 0f));
    let _e81 = (*c);
    cutoff = (_e81 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e83 = (*c);
    lo = (_e83 / vec3(12.92f));
    let _e86 = (*c);
    hi = pow(((_e86 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e91 = hi;
    let _e92 = lo;
    let _e93 = cutoff;
    return mix(_e91, _e92, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e93));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e102 = (*uv);
    let _e103 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    c_1 = _e103;
    let _e104 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e104))) == 0i) {
        let _e109 = c_1;
        param = _e109.xyz;
        let _e111 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = (*slot);
    if (lightmap_slot == (_e118 + 1i)) {
        let _e123 = unnamed.worldLightParams[0u];
        let _e124 = c_1;
        let _e126 = (_e124.xyz * _e123);
        c_1[0u] = _e126.x;
        c_1[1u] = _e126.y;
        c_1[2u] = _e126.z;
    }
    let _e133 = c_1;
    return _e133;
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
    var fogAmount: f32;

    let _e161 = frag_color0In_1;
    param_1 = _e161.xyz;
    let _e163 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e165 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e163.x, _e163.y, _e163.z, _e165);
    let _e170 = frag_color1In_1;
    param_2 = _e170.xyz;
    let _e172 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e174 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e172.x, _e172.y, _e172.z, _e174);
    let _e179 = frag_color2In_1;
    param_3 = _e179.xyz;
    let _e181 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e183 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e181.x, _e181.y, _e181.z, _e183);
    param_4 = 0u;
    let _e188 = frag_tex_coord0_1;
    param_5 = _e188;
    param_6 = 0i;
    let _e189 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e190 = frag_color0_;
    color0_ = (_e189 * _e190);
    if override_type_3_2 {
        param_7 = 1u;
        let _e192 = frag_tex_coord1_1;
        param_8 = _e192;
        param_9 = 1i;
        let _e193 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e194 = frag_color1_;
        color1_ = (_e193 * _e194);
        param_10 = 2u;
        let _e196 = frag_tex_coord2_1;
        param_11 = _e196;
        param_12 = 2i;
        let _e197 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e198 = frag_color2_;
        color2_ = (_e197 * _e198);
        let _e200 = color0_;
        let _e202 = color1_;
        let _e205 = color2_;
        let _e207 = ((_e200.xyz + _e202.xyz) + _e205.xyz);
        let _e209 = color0_[3u];
        let _e211 = color1_[3u];
        let _e214 = color2_[3u];
        base = vec4<f32>(_e207.x, _e207.y, _e207.z, ((_e209 * _e211) * _e214));
    } else {
        if override_type_3_3 {
            param_13 = 1u;
            let _e220 = frag_tex_coord1_1;
            param_14 = _e220;
            param_15 = 1i;
            let _e221 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e222 = frag_color1_;
            color1_1 = (_e221 * _e222);
            param_16 = 2u;
            let _e224 = frag_tex_coord2_1;
            param_17 = _e224;
            param_18 = 2i;
            let _e225 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e226 = frag_color2_;
            color2_1 = (_e225 * _e226);
            let _e229 = color0_[3u];
            let _e230 = color0_;
            color0_ = (_e230 * _e229);
            let _e233 = color1_1[3u];
            let _e234 = color1_1;
            color1_1 = (_e234 * _e233);
            let _e237 = color2_1[3u];
            let _e238 = color2_1;
            color2_1 = (_e238 * _e237);
            let _e240 = color0_;
            let _e242 = color1_1;
            let _e245 = color2_1;
            let _e247 = ((_e240.xyz + _e242.xyz) + _e245.xyz);
            let _e249 = color0_[3u];
            let _e251 = color1_1[3u];
            let _e254 = color2_1[3u];
            base = vec4<f32>(_e247.x, _e247.y, _e247.z, ((_e249 * _e251) * _e254));
        } else {
            if override_type_3_4 {
                param_19 = 1u;
                let _e260 = frag_tex_coord1_1;
                param_20 = _e260;
                param_21 = 1i;
                let _e261 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e262 = frag_color1_;
                color1_2 = (_e261 * _e262);
                param_22 = 2u;
                let _e264 = frag_tex_coord2_1;
                param_23 = _e264;
                param_24 = 2i;
                let _e265 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e266 = frag_color2_;
                color2_2 = (_e265 * _e266);
                let _e269 = color0_[3u];
                let _e271 = color0_;
                color0_ = (_e271 * (1f - _e269));
                let _e274 = color1_2[3u];
                let _e276 = color1_2;
                color1_2 = (_e276 * (1f - _e274));
                let _e279 = color2_2[3u];
                let _e281 = color2_2;
                color2_2 = (_e281 * (1f - _e279));
                let _e283 = color0_;
                let _e285 = color1_2;
                let _e288 = color2_2;
                let _e290 = ((_e283.xyz + _e285.xyz) + _e288.xyz);
                let _e292 = color0_[3u];
                let _e294 = color1_2[3u];
                let _e297 = color2_2[3u];
                base = vec4<f32>(_e290.x, _e290.y, _e290.z, ((_e292 * _e294) * _e297));
            } else {
                if override_type_3_5 {
                    param_25 = 1u;
                    let _e303 = frag_tex_coord1_1;
                    param_26 = _e303;
                    param_27 = 1i;
                    let _e304 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e305 = frag_color1_;
                    color1_3 = (_e304 * _e305);
                    param_28 = 2u;
                    let _e307 = frag_tex_coord2_1;
                    param_29 = _e307;
                    param_30 = 2i;
                    let _e308 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e309 = frag_color2_;
                    color2_3 = (_e308 * _e309);
                    let _e311 = color0_;
                    let _e312 = color1_3;
                    let _e314 = color1_3[3u];
                    let _e317 = color2_3;
                    let _e319 = color2_3[3u];
                    base = mix(mix(_e311, _e312, vec4(_e314)), _e317, vec4(_e319));
                } else {
                    if override_type_3_6 {
                        param_31 = 1u;
                        let _e322 = frag_tex_coord1_1;
                        param_32 = _e322;
                        param_33 = 1i;
                        let _e323 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e324 = frag_color1_;
                        color1_4 = (_e323 * _e324);
                        param_34 = 2u;
                        let _e326 = frag_tex_coord2_1;
                        param_35 = _e326;
                        param_36 = 2i;
                        let _e327 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e328 = frag_color2_;
                        color2_4 = (_e327 * _e328);
                        let _e330 = color2_4;
                        let _e331 = color1_4;
                        let _e332 = color0_;
                        let _e334 = color1_4[3u];
                        let _e338 = color2_4[3u];
                        base = mix(_e330, mix(_e331, _e332, vec4(_e334)), vec4(_e338));
                    } else {
                        if override_type_3_7 {
                            param_37 = 1u;
                            let _e341 = frag_tex_coord1_1;
                            param_38 = _e341;
                            param_39 = 1i;
                            let _e342 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e343 = frag_color1_;
                            color1_5 = (_e342 * _e343);
                            param_40 = 2u;
                            let _e345 = frag_tex_coord2_1;
                            param_41 = _e345;
                            param_42 = 2i;
                            let _e346 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e347 = frag_color2_;
                            color2_5 = (_e346 * _e347);
                            let _e349 = color2_5;
                            let _e351 = color2_5[3u];
                            let _e354 = color1_5;
                            let _e356 = color1_5[3u];
                            let _e360 = color0_;
                            base = (((_e349 + vec4(_e351)) * (_e354 + vec4(_e356))) * _e360);
                        } else {
                            param_43 = 1u;
                            let _e362 = frag_tex_coord1_1;
                            param_44 = _e362;
                            param_45 = 1i;
                            let _e363 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e364 = frag_color1_;
                            color1_6 = (_e363 * _e364);
                            param_46 = 2u;
                            let _e366 = frag_tex_coord2_1;
                            param_47 = _e366;
                            param_48 = 2i;
                            let _e367 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e368 = frag_color2_;
                            color2_6 = (_e367 * _e368);
                            let _e370 = color0_;
                            let _e372 = color1_6;
                            let _e375 = color2_6;
                            let _e377 = ((_e370.xyz * _e372.xyz) * _e375.xyz);
                            base[0u] = _e377.x;
                            base[1u] = _e377.y;
                            base[2u] = _e377.z;
                            let _e385 = color0_[3u];
                            let _e387 = color1_6[3u];
                            let _e390 = color2_6[3u];
                            base[3u] = ((_e385 * _e387) * _e390);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e396 = unnamed.packed_indices[2i][0u];
        let _e402 = unnamed.packed_indices[2i][0u];
        let _e407 = frag_tex_coord0_1;
        let _e408 = textureSample(wired_bindless_images[(_e396 & 4095u)], wired_bindless_samplers[((_e402 >> bitcast<u32>(12i)) & 255u)], _e407);
        orm = _e408.xyz;
        let _e411 = orm[0u];
        ao = _e411;
        let _e413 = orm[1u];
        roughness_1 = clamp(_e413, 0.04f, 1f);
        let _e416 = orm[2u];
        metalness = _e416;
        let _e417 = ibl_N_1;
        ibl_n = normalize(_e417);
        let _e419 = ibl_V_1;
        ibl_v = normalize(_e419);
        let _e421 = base;
        let _e423 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e421.xyz, vec3(_e423));
        let _e426 = ibl_n;
        let _e427 = ibl_v;
        NdotV = max(dot(_e426, _e427), 0f);
        let _e430 = NdotV;
        param_49 = _e430;
        let _e431 = F0_1;
        param_50 = _e431;
        let _e432 = roughness_1;
        param_51 = _e432;
        let _e433 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_49), (&param_50), (&param_51));
        F_amb = _e433;
        let _e434 = ibl_v;
        let _e436 = ibl_n;
        R = reflect(-(_e434), _e436);
        let _e438 = R;
        let _e439 = roughness_1;
        let _e441 = textureSampleLevel(radianceCube, radianceCube_sampler, _e438, (_e439 * 5f));
        prefiltered = _e441.xyz;
        let _e443 = NdotV;
        let _e444 = roughness_1;
        let _e446 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e443, _e444), 0f);
        envBRDF = _e446.xy;
        let _e448 = prefiltered;
        let _e449 = F_amb;
        let _e451 = envBRDF[0u];
        let _e454 = envBRDF[1u];
        let _e458 = ao;
        specularIBL = ((_e448 * ((_e449 * _e451) + vec3(_e454))) * _e458);
        let _e460 = gl_FragCoord_1;
        let _e462 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e460.xy / vec2<f32>(vec2<i32>(_e462)));
        let _e466 = gtaoUV;
        let _e467 = textureSample(gtaoMap, gtaoMap_sampler, _e466);
        gtao_vis = _e467.x;
        let _e469 = gtao_vis;
        let _e470 = specularIBL;
        specularIBL = (_e470 * _e469);
        let _e472 = specularIBL;
        let _e473 = base;
        let _e475 = (_e473.xyz + _e472);
        base[0u] = _e475.x;
        base[1u] = _e475.y;
        base[2u] = _e475.z;
    }
    let _e482 = wired_advanced_fog_enabled_u0028_();
    if _e482 {
        let _e483 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e483;
        let _e484 = base;
        let _e487 = unnamed.advancedFogColorDensity;
        let _e489 = fogAmount;
        let _e491 = mix(_e484.xyz, _e487.xyz, vec3(_e489));
        base[0u] = _e491.x;
        base[1u] = _e491.y;
        base[2u] = _e491.z;
    }
    if override_type_3_9 {
        let _e499 = base[3u];
        if (_e499 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e501 = base;
            let _e503 = base;
            if (dot(_e501.xyz, _e503.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e507 = base;
    out_color = _e507;
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
