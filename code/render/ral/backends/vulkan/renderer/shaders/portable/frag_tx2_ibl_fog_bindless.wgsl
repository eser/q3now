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
@id(14) override ibl_enabled: i32 = 0i;
override override_type_3_2: bool = (ibl_enabled != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_3: bool = (acff == 1i);
override override_type_3_4: bool = (acff == 2i);
override override_type_3_5: bool = (acff == 3i);
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
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

    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e75 + 0.5f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e82 = fogType;
    let _e85 = fogType;
    return (((_e80 > 0.5f) && (_e82 >= 1i)) && (_e85 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e75 = wired_advanced_fog_enabled_u0028_();
    if !(_e75) {
        return 0f;
    }
    let _e78 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e78, 0.000001f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e83 + 0.5f));
    let _e86 = fogType_1;
    if (_e86 == 1i) {
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e90 <= 0f) {
            return 0f;
        }
        let _e92 = viewDepth;
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e92 / _e95), 0f, 1f);
    }
    let _e100 = unnamed.advancedFogColorDensity[3u];
    let _e102 = viewDepth;
    opticalDepth = (max(_e100, 0f) * _e102);
    let _e104 = fogType_1;
    if (_e104 == 2i) {
        let _e106 = opticalDepth;
        return clamp((1f - exp(-(_e106))), 0f, 1f);
    }
    let _e111 = opticalDepth;
    let _e112 = opticalDepth;
    return clamp((1f - exp(-((_e111 * _e112)))), 0f, 1f);
}

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
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var orm: vec3<f32>;
    var ao: f32;
    var roughness_1: f32;
    var metalness: f32;
    var ibl_n: vec3<f32>;
    var ibl_v: vec3<f32>;
    var F0_1: vec3<f32>;
    var NdotV: f32;
    var F_amb: vec3<f32>;
    var param_23: f32;
    var param_24: vec3<f32>;
    var param_25: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e126 = unnamed.packed_indices[0i][3u];
    let _e132 = unnamed.packed_indices[0i][3u];
    let _e137 = fog_tex_coord_1;
    let _e138 = textureSample(wired_bindless_images[(_e126 & 4095u)], wired_bindless_samplers[((_e132 >> bitcast<u32>(12i)) & 255u)], _e137);
    fog = _e138;
    let _e139 = frag_color0In_1;
    param_1 = _e139.xyz;
    let _e141 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e143 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e141.x, _e141.y, _e141.z, _e143);
    param_2 = 0u;
    let _e148 = frag_tex_coord0_1;
    param_3 = _e148;
    param_4 = 0i;
    let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e150 = frag_color0_;
    color0_ = (_e149 * _e150);
    if override_type_3_ {
        param_5 = 1u;
        let _e152 = frag_tex_coord1_1;
        param_6 = _e152;
        param_7 = 1i;
        let _e153 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e153;
        param_8 = 2u;
        let _e154 = frag_tex_coord2_1;
        param_9 = _e154;
        param_10 = 2i;
        let _e155 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e155;
        let _e156 = color0_;
        let _e158 = color1_;
        let _e161 = color2_;
        let _e163 = ((_e156.xyz + _e158.xyz) + _e161.xyz);
        let _e165 = color0_[3u];
        let _e167 = color1_[3u];
        let _e170 = color2_[3u];
        base = vec4<f32>(_e163.x, _e163.y, _e163.z, ((_e165 * _e167) * _e170));
    } else {
        if override_type_3_1 {
            param_11 = 1u;
            let _e176 = frag_tex_coord1_1;
            param_12 = _e176;
            param_13 = 1i;
            let _e177 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e178 = frag_color0_;
            color1_1 = (_e177 * _e178);
            param_14 = 2u;
            let _e180 = frag_tex_coord2_1;
            param_15 = _e180;
            param_16 = 2i;
            let _e181 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e182 = frag_color0_;
            color2_1 = (_e181 * _e182);
            let _e184 = color0_;
            let _e186 = color1_1;
            let _e189 = color2_1;
            let _e191 = ((_e184.xyz + _e186.xyz) + _e189.xyz);
            let _e193 = color0_[3u];
            let _e195 = color1_1[3u];
            let _e198 = color2_1[3u];
            base = vec4<f32>(_e191.x, _e191.y, _e191.z, ((_e193 * _e195) * _e198));
        } else {
            param_17 = 1u;
            let _e204 = frag_tex_coord1_1;
            param_18 = _e204;
            param_19 = 1i;
            let _e205 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e205;
            param_20 = 2u;
            let _e206 = frag_tex_coord2_1;
            param_21 = _e206;
            param_22 = 2i;
            let _e207 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e207;
            let _e208 = color0_;
            let _e210 = color1_2;
            let _e213 = color2_2;
            let _e215 = ((_e208.xyz * _e210.xyz) * _e213.xyz);
            base[0u] = _e215.x;
            base[1u] = _e215.y;
            base[2u] = _e215.z;
            let _e223 = color0_[3u];
            let _e225 = color1_2[3u];
            let _e228 = color2_2[3u];
            base[3u] = ((_e223 * _e225) * _e228);
        }
    }
    if override_type_3_2 {
        let _e234 = unnamed.packed_indices[2i][0u];
        let _e240 = unnamed.packed_indices[2i][0u];
        let _e245 = frag_tex_coord0_1;
        let _e246 = textureSample(wired_bindless_images[(_e234 & 4095u)], wired_bindless_samplers[((_e240 >> bitcast<u32>(12i)) & 255u)], _e245);
        orm = _e246.xyz;
        let _e249 = orm[0u];
        ao = _e249;
        let _e251 = orm[1u];
        roughness_1 = clamp(_e251, 0.04f, 1f);
        let _e254 = orm[2u];
        metalness = _e254;
        let _e255 = ibl_N_1;
        ibl_n = normalize(_e255);
        let _e257 = ibl_V_1;
        ibl_v = normalize(_e257);
        let _e259 = base;
        let _e261 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e259.xyz, vec3(_e261));
        let _e264 = ibl_n;
        let _e265 = ibl_v;
        NdotV = max(dot(_e264, _e265), 0f);
        let _e268 = NdotV;
        param_23 = _e268;
        let _e269 = F0_1;
        param_24 = _e269;
        let _e270 = roughness_1;
        param_25 = _e270;
        let _e271 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_23), (&param_24), (&param_25));
        F_amb = _e271;
        let _e272 = ibl_v;
        let _e274 = ibl_n;
        R = reflect(-(_e272), _e274);
        let _e276 = R;
        let _e277 = roughness_1;
        let _e279 = textureSampleLevel(radianceCube, radianceCube_sampler, _e276, (_e277 * 5f));
        prefiltered = _e279.xyz;
        let _e281 = NdotV;
        let _e282 = roughness_1;
        let _e284 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e281, _e282), 0f);
        envBRDF = _e284.xy;
        let _e286 = prefiltered;
        let _e287 = F_amb;
        let _e289 = envBRDF[0u];
        let _e292 = envBRDF[1u];
        let _e296 = ao;
        specularIBL = ((_e286 * ((_e287 * _e289) + vec3(_e292))) * _e296);
        let _e298 = gl_FragCoord_1;
        let _e300 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e298.xy / vec2<f32>(vec2<i32>(_e300)));
        let _e304 = gtaoUV;
        let _e305 = textureSample(gtaoMap, gtaoMap_sampler, _e304);
        gtao_vis = _e305.x;
        let _e307 = gtao_vis;
        let _e308 = specularIBL;
        specularIBL = (_e308 * _e307);
        let _e310 = specularIBL;
        let _e311 = base;
        let _e313 = (_e311.xyz + _e310);
        base[0u] = _e313.x;
        base[1u] = _e313.y;
        base[2u] = _e313.z;
    }
    let _e320 = wired_advanced_fog_enabled_u0028_();
    if _e320 {
        let _e321 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e321;
        if override_type_3_3 {
            let _e322 = fogAmount;
            let _e324 = base;
            let _e326 = (_e324.xyz * (1f - _e322));
            base[0u] = _e326.x;
            base[1u] = _e326.y;
            base[2u] = _e326.z;
        } else {
            if override_type_3_4 {
                let _e333 = fogAmount;
                let _e335 = base;
                base = (_e335 * (1f - _e333));
            } else {
                if override_type_3_5 {
                    let _e337 = fogAmount;
                    let _e340 = base[3u];
                    base[3u] = (_e340 * (1f - _e337));
                } else {
                    let _e343 = base;
                    let _e346 = unnamed.advancedFogColorDensity;
                    let _e348 = fogAmount;
                    let _e350 = mix(_e343.xyz, _e346.xyz, vec3(_e348));
                    base[0u] = _e350.x;
                    base[1u] = _e350.y;
                    base[2u] = _e350.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e357 = base;
            let _e360 = fog[3u];
            let _e362 = (_e357.xyz * (1f - _e360));
            base[0u] = _e362.x;
            base[1u] = _e362.y;
            base[2u] = _e362.z;
        } else {
            if override_type_3_7 {
                let _e369 = base;
                let _e371 = fog[3u];
                base = (_e369 * (1f - _e371));
            } else {
                if override_type_3_8 {
                    let _e375 = base[3u];
                    let _e377 = fog[3u];
                    base[3u] = (_e375 * (1f - _e377));
                } else {
                    let _e381 = base;
                    let _e382 = fog;
                    let _e384 = unnamed.fogColor;
                    let _e387 = fog[3u];
                    base = mix(_e381, (_e382 * _e384), vec4(_e387));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e391 = base[3u];
        if (_e391 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e393 = base;
            let _e395 = base;
            if (dot(_e393.xyz, _e395.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e399 = base;
    out_color = _e399;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e17 = out_color;
    return _e17;
}
