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

    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e76 + 0.5f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e83 = fogType;
    let _e86 = fogType;
    return (((_e81 > 0.5f) && (_e83 >= 1i)) && (_e86 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e76 = wired_advanced_fog_enabled_u0028_();
    if !(_e76) {
        return 0f;
    }
    let _e79 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e79, 0.000001f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e84 + 0.5f));
    let _e87 = fogType_1;
    if (_e87 == 1i) {
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e91 <= 0f) {
            return 0f;
        }
        let _e93 = viewDepth;
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e93 / _e96), 0f, 1f);
    }
    let _e101 = unnamed.advancedFogColorDensity[3u];
    let _e103 = viewDepth;
    opticalDepth = (max(_e101, 0f) * _e103);
    let _e105 = fogType_1;
    if (_e105 == 2i) {
        let _e107 = opticalDepth;
        return clamp((1f - exp(-(_e107))), 0f, 1f);
    }
    let _e112 = opticalDepth;
    let _e113 = opticalDepth;
    return clamp((1f - exp(-((_e112 * _e113)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e79 = (*cosTheta);
    t = (1f - _e79);
    let _e81 = t;
    let _e82 = t;
    t2_ = (_e81 * _e82);
    let _e84 = (*roughness);
    let _e87 = (*F0_);
    Fmax = max(vec3((1f - _e84)), _e87);
    let _e89 = (*F0_);
    let _e90 = Fmax;
    let _e91 = (*F0_);
    let _e93 = t2_;
    let _e94 = t2_;
    let _e96 = t;
    return (_e89 + ((_e90 - _e91) * ((_e93 * _e94) * _e96)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e77 = (*c);
    (*c) = max(_e77, vec3<f32>(0f, 0f, 0f));
    let _e79 = (*c);
    cutoff = (_e79 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e81 = (*c);
    lo = (_e81 / vec3(12.92f));
    let _e84 = (*c);
    hi = pow(((_e84 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e89 = hi;
    let _e90 = lo;
    let _e91 = cutoff;
    return mix(_e89, _e90, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e91));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e100 = (*uv);
    let _e101 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    c_1 = _e101;
    let _e102 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e102))) == 0i) {
        let _e107 = c_1;
        param = _e107.xyz;
        let _e109 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = (*slot);
    if (lightmap_slot == (_e116 + 1i)) {
        let _e121 = unnamed.worldLightParams[0u];
        let _e122 = c_1;
        let _e124 = (_e122.xyz * _e121);
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = c_1;
    return _e131;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_2: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_3: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_4: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_5: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_6: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var orm: vec3<f32>;
    var ao: f32;
    var roughness_1: f32;
    var metalness: f32;
    var ibl_n: vec3<f32>;
    var ibl_v: vec3<f32>;
    var F0_1: vec3<f32>;
    var NdotV: f32;
    var F_amb: vec3<f32>;
    var param_27: f32;
    var param_28: vec3<f32>;
    var param_29: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e129 = frag_color0In_1;
    param_1 = _e129.xyz;
    let _e131 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e133 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e131.x, _e131.y, _e131.z, _e133);
    let _e138 = frag_color1In_1;
    param_2 = _e138.xyz;
    let _e140 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e142 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e140.x, _e140.y, _e140.z, _e142);
    param_3 = 0u;
    let _e147 = frag_tex_coord0_1;
    param_4 = _e147;
    param_5 = 0i;
    let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e149 = frag_color0_;
    color0_ = (_e148 * _e149);
    if override_type_3_2 {
        param_6 = 1u;
        let _e151 = frag_tex_coord1_1;
        param_7 = _e151;
        param_8 = 1i;
        let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e153 = frag_color1_;
        color1_ = (_e152 * _e153);
        let _e155 = color0_;
        let _e157 = color1_;
        let _e159 = (_e155.xyz + _e157.xyz);
        let _e161 = color0_[3u];
        let _e163 = color1_[3u];
        base = vec4<f32>(_e159.x, _e159.y, _e159.z, (_e161 * _e163));
    } else {
        if override_type_3_3 {
            param_9 = 1u;
            let _e169 = frag_tex_coord1_1;
            param_10 = _e169;
            param_11 = 1i;
            let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e171 = frag_color1_;
            color1_1 = (_e170 * _e171);
            let _e174 = color0_[3u];
            let _e175 = color0_;
            color0_ = (_e175 * _e174);
            let _e178 = color1_1[3u];
            let _e179 = color1_1;
            color1_1 = (_e179 * _e178);
            let _e181 = color0_;
            let _e183 = color1_1;
            let _e185 = (_e181.xyz + _e183.xyz);
            let _e187 = color0_[3u];
            let _e189 = color1_1[3u];
            base = vec4<f32>(_e185.x, _e185.y, _e185.z, (_e187 * _e189));
        } else {
            if override_type_3_4 {
                param_12 = 1u;
                let _e195 = frag_tex_coord1_1;
                param_13 = _e195;
                param_14 = 1i;
                let _e196 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e197 = frag_color1_;
                color1_2 = (_e196 * _e197);
                let _e200 = color0_[3u];
                let _e202 = color0_;
                color0_ = (_e202 * (1f - _e200));
                let _e205 = color1_2[3u];
                let _e207 = color1_2;
                color1_2 = (_e207 * (1f - _e205));
                let _e209 = color0_;
                let _e211 = color1_2;
                let _e213 = (_e209.xyz + _e211.xyz);
                let _e215 = color0_[3u];
                let _e217 = color1_2[3u];
                base = vec4<f32>(_e213.x, _e213.y, _e213.z, (_e215 * _e217));
            } else {
                if override_type_3_5 {
                    param_15 = 1u;
                    let _e223 = frag_tex_coord1_1;
                    param_16 = _e223;
                    param_17 = 1i;
                    let _e224 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e225 = frag_color1_;
                    color1_3 = (_e224 * _e225);
                    let _e227 = color0_;
                    let _e228 = color1_3;
                    let _e230 = color1_3[3u];
                    base = mix(_e227, _e228, vec4(_e230));
                } else {
                    if override_type_3_6 {
                        param_18 = 1u;
                        let _e233 = frag_tex_coord1_1;
                        param_19 = _e233;
                        param_20 = 1i;
                        let _e234 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e235 = frag_color1_;
                        color1_4 = (_e234 * _e235);
                        let _e237 = color1_4;
                        let _e238 = color0_;
                        let _e240 = color1_4[3u];
                        base = mix(_e237, _e238, vec4(_e240));
                    } else {
                        if override_type_3_7 {
                            param_21 = 1u;
                            let _e243 = frag_tex_coord1_1;
                            param_22 = _e243;
                            param_23 = 1i;
                            let _e244 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e245 = frag_color1_;
                            color1_5 = (_e244 * _e245);
                            let _e247 = color1_5;
                            let _e249 = color1_5[3u];
                            let _e252 = color0_;
                            base = ((_e247 + vec4(_e249)) * _e252);
                        } else {
                            param_24 = 1u;
                            let _e254 = frag_tex_coord1_1;
                            param_25 = _e254;
                            param_26 = 1i;
                            let _e255 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e256 = frag_color1_;
                            color1_6 = (_e255 * _e256);
                            let _e258 = color0_;
                            let _e260 = color1_6;
                            let _e262 = (_e258.xyz * _e260.xyz);
                            base[0u] = _e262.x;
                            base[1u] = _e262.y;
                            base[2u] = _e262.z;
                            let _e270 = color0_[3u];
                            let _e272 = color1_6[3u];
                            base[3u] = (_e270 * _e272);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e278 = unnamed.packed_indices[2i][0u];
        let _e284 = unnamed.packed_indices[2i][0u];
        let _e289 = frag_tex_coord0_1;
        let _e290 = textureSample(wired_bindless_images[(_e278 & 4095u)], wired_bindless_samplers[((_e284 >> bitcast<u32>(12i)) & 255u)], _e289);
        orm = _e290.xyz;
        let _e293 = orm[0u];
        ao = _e293;
        let _e295 = orm[1u];
        roughness_1 = clamp(_e295, 0.04f, 1f);
        let _e298 = orm[2u];
        metalness = _e298;
        let _e299 = ibl_N_1;
        ibl_n = normalize(_e299);
        let _e301 = ibl_V_1;
        ibl_v = normalize(_e301);
        let _e303 = base;
        let _e305 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e303.xyz, vec3(_e305));
        let _e308 = ibl_n;
        let _e309 = ibl_v;
        NdotV = max(dot(_e308, _e309), 0f);
        let _e312 = NdotV;
        param_27 = _e312;
        let _e313 = F0_1;
        param_28 = _e313;
        let _e314 = roughness_1;
        param_29 = _e314;
        let _e315 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_27), (&param_28), (&param_29));
        F_amb = _e315;
        let _e316 = ibl_v;
        let _e318 = ibl_n;
        R = reflect(-(_e316), _e318);
        let _e320 = R;
        let _e321 = roughness_1;
        let _e323 = textureSampleLevel(radianceCube, radianceCube_sampler, _e320, (_e321 * 5f));
        prefiltered = _e323.xyz;
        let _e325 = NdotV;
        let _e326 = roughness_1;
        let _e328 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e325, _e326), 0f);
        envBRDF = _e328.xy;
        let _e330 = prefiltered;
        let _e331 = F_amb;
        let _e333 = envBRDF[0u];
        let _e336 = envBRDF[1u];
        let _e340 = ao;
        specularIBL = ((_e330 * ((_e331 * _e333) + vec3(_e336))) * _e340);
        let _e342 = gl_FragCoord_1;
        let _e344 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e342.xy / vec2<f32>(vec2<i32>(_e344)));
        let _e348 = gtaoUV;
        let _e349 = textureSample(gtaoMap, gtaoMap_sampler, _e348);
        gtao_vis = _e349.x;
        let _e351 = gtao_vis;
        let _e352 = specularIBL;
        specularIBL = (_e352 * _e351);
        let _e354 = specularIBL;
        let _e355 = base;
        let _e357 = (_e355.xyz + _e354);
        base[0u] = _e357.x;
        base[1u] = _e357.y;
        base[2u] = _e357.z;
    }
    let _e364 = wired_advanced_fog_enabled_u0028_();
    if _e364 {
        let _e365 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e365;
        let _e366 = base;
        let _e369 = unnamed.advancedFogColorDensity;
        let _e371 = fogAmount;
        let _e373 = mix(_e366.xyz, _e369.xyz, vec3(_e371));
        base[0u] = _e373.x;
        base[1u] = _e373.y;
        base[2u] = _e373.z;
    }
    if override_type_3_9 {
        let _e381 = base[3u];
        if (_e381 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e383 = base;
            let _e385 = base;
            if (dot(_e383.xyz, _e385.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e389 = base;
    out_color = _e389;
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
