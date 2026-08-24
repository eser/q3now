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

    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e74 + 0.5f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e81 = fogType;
    let _e84 = fogType;
    return (((_e79 > 0.5f) && (_e81 >= 1i)) && (_e84 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e74 = wired_advanced_fog_enabled_u0028_();
    if !(_e74) {
        return 0f;
    }
    let _e77 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e77, 0.000001f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e82 + 0.5f));
    let _e85 = fogType_1;
    if (_e85 == 1i) {
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e89 <= 0f) {
            return 0f;
        }
        let _e91 = viewDepth;
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e91 / _e94), 0f, 1f);
    }
    let _e99 = unnamed.advancedFogColorDensity[3u];
    let _e101 = viewDepth;
    opticalDepth = (max(_e99, 0f) * _e101);
    let _e103 = fogType_1;
    if (_e103 == 2i) {
        let _e105 = opticalDepth;
        return clamp((1f - exp(-(_e105))), 0f, 1f);
    }
    let _e110 = opticalDepth;
    let _e111 = opticalDepth;
    return clamp((1f - exp(-((_e110 * _e111)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e77 = (*cosTheta);
    t = (1f - _e77);
    let _e79 = t;
    let _e80 = t;
    t2_ = (_e79 * _e80);
    let _e82 = (*roughness);
    let _e85 = (*F0_);
    Fmax = max(vec3((1f - _e82)), _e85);
    let _e87 = (*F0_);
    let _e88 = Fmax;
    let _e89 = (*F0_);
    let _e91 = t2_;
    let _e92 = t2_;
    let _e94 = t;
    return (_e87 + ((_e88 - _e89) * ((_e91 * _e92) * _e94)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e75 = (*c);
    (*c) = max(_e75, vec3<f32>(0f, 0f, 0f));
    let _e77 = (*c);
    cutoff = (_e77 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e79 = (*c);
    lo = (_e79 / vec3(12.92f));
    let _e82 = (*c);
    hi = pow(((_e82 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e87 = hi;
    let _e88 = lo;
    let _e89 = cutoff;
    return mix(_e87, _e88, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e89));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_1 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_1;
        param = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e119 = unnamed.worldLightParams[0u];
        let _e120 = c_1;
        let _e122 = (_e120.xyz * _e119);
        c_1[0u] = _e122.x;
        c_1[1u] = _e122.y;
        c_1[2u] = _e122.z;
    }
    let _e129 = c_1;
    return _e129;
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
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var orm: vec3<f32>;
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
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e113 = unnamed.packed_indices[0i][3u];
    let _e119 = unnamed.packed_indices[0i][3u];
    let _e124 = fog_tex_coord_1;
    let _e125 = textureSample(wired_bindless_images[(_e113 & 4095u)], wired_bindless_samplers[((_e119 >> bitcast<u32>(12i)) & 255u)], _e124);
    fog = _e125;
    let _e126 = frag_color0In_1;
    param_1 = _e126.xyz;
    let _e128 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e130 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e128.x, _e128.y, _e128.z, _e130);
    param_2 = 0u;
    let _e135 = frag_tex_coord0_1;
    param_3 = _e135;
    param_4 = 0i;
    let _e136 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e137 = frag_color0_;
    color0_ = (_e136 * _e137);
    if override_type_3_ {
        param_5 = 1u;
        let _e139 = frag_tex_coord1_1;
        param_6 = _e139;
        param_7 = 1i;
        let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e140;
        let _e141 = color0_;
        let _e143 = color1_;
        let _e145 = (_e141.xyz + _e143.xyz);
        let _e147 = color0_[3u];
        let _e149 = color1_[3u];
        base = vec4<f32>(_e145.x, _e145.y, _e145.z, (_e147 * _e149));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e155 = frag_tex_coord1_1;
            param_9 = _e155;
            param_10 = 1i;
            let _e156 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e157 = frag_color0_;
            color1_1 = (_e156 * _e157);
            let _e159 = color0_;
            let _e161 = color1_1;
            let _e163 = (_e159.xyz + _e161.xyz);
            let _e165 = color0_[3u];
            let _e167 = color1_1[3u];
            base = vec4<f32>(_e163.x, _e163.y, _e163.z, (_e165 * _e167));
        } else {
            param_11 = 1u;
            let _e173 = frag_tex_coord1_1;
            param_12 = _e173;
            param_13 = 1i;
            let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e174;
            let _e175 = color0_;
            let _e177 = color1_2;
            let _e179 = (_e175.xyz * _e177.xyz);
            base[0u] = _e179.x;
            base[1u] = _e179.y;
            base[2u] = _e179.z;
            let _e187 = color0_[3u];
            let _e189 = color1_2[3u];
            base[3u] = (_e187 * _e189);
        }
    }
    if override_type_3_2 {
        let _e195 = unnamed.packed_indices[2i][0u];
        let _e201 = unnamed.packed_indices[2i][0u];
        let _e206 = frag_tex_coord0_1;
        let _e207 = textureSample(wired_bindless_images[(_e195 & 4095u)], wired_bindless_samplers[((_e201 >> bitcast<u32>(12i)) & 255u)], _e206);
        orm = _e207.xyz;
        let _e210 = orm[0u];
        ao = _e210;
        let _e212 = orm[1u];
        roughness_1 = clamp(_e212, 0.04f, 1f);
        let _e215 = orm[2u];
        metalness = _e215;
        let _e216 = ibl_N_1;
        ibl_n = normalize(_e216);
        let _e218 = ibl_V_1;
        ibl_v = normalize(_e218);
        let _e220 = base;
        let _e222 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e220.xyz, vec3(_e222));
        let _e225 = ibl_n;
        let _e226 = ibl_v;
        NdotV = max(dot(_e225, _e226), 0f);
        let _e229 = NdotV;
        param_14 = _e229;
        let _e230 = F0_1;
        param_15 = _e230;
        let _e231 = roughness_1;
        param_16 = _e231;
        let _e232 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        F_amb = _e232;
        let _e233 = ibl_v;
        let _e235 = ibl_n;
        R = reflect(-(_e233), _e235);
        let _e237 = R;
        let _e238 = roughness_1;
        let _e240 = textureSampleLevel(radianceCube, radianceCube_sampler, _e237, (_e238 * 5f));
        prefiltered = _e240.xyz;
        let _e242 = NdotV;
        let _e243 = roughness_1;
        let _e245 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e242, _e243), 0f);
        envBRDF = _e245.xy;
        let _e247 = prefiltered;
        let _e248 = F_amb;
        let _e250 = envBRDF[0u];
        let _e253 = envBRDF[1u];
        let _e257 = ao;
        specularIBL = ((_e247 * ((_e248 * _e250) + vec3(_e253))) * _e257);
        let _e259 = gl_FragCoord_1;
        let _e261 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e259.xy / vec2<f32>(vec2<i32>(_e261)));
        let _e265 = gtaoUV;
        let _e266 = textureSample(gtaoMap, gtaoMap_sampler, _e265);
        gtao_vis = _e266.x;
        let _e268 = gtao_vis;
        let _e269 = specularIBL;
        specularIBL = (_e269 * _e268);
        let _e271 = specularIBL;
        let _e272 = base;
        let _e274 = (_e272.xyz + _e271);
        base[0u] = _e274.x;
        base[1u] = _e274.y;
        base[2u] = _e274.z;
    }
    let _e281 = wired_advanced_fog_enabled_u0028_();
    if _e281 {
        let _e282 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e282;
        if override_type_3_3 {
            let _e283 = fogAmount;
            let _e285 = base;
            let _e287 = (_e285.xyz * (1f - _e283));
            base[0u] = _e287.x;
            base[1u] = _e287.y;
            base[2u] = _e287.z;
        } else {
            if override_type_3_4 {
                let _e294 = fogAmount;
                let _e296 = base;
                base = (_e296 * (1f - _e294));
            } else {
                if override_type_3_5 {
                    let _e298 = fogAmount;
                    let _e301 = base[3u];
                    base[3u] = (_e301 * (1f - _e298));
                } else {
                    let _e304 = base;
                    let _e307 = unnamed.advancedFogColorDensity;
                    let _e309 = fogAmount;
                    let _e311 = mix(_e304.xyz, _e307.xyz, vec3(_e309));
                    base[0u] = _e311.x;
                    base[1u] = _e311.y;
                    base[2u] = _e311.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e318 = base;
            let _e321 = fog[3u];
            let _e323 = (_e318.xyz * (1f - _e321));
            base[0u] = _e323.x;
            base[1u] = _e323.y;
            base[2u] = _e323.z;
        } else {
            if override_type_3_7 {
                let _e330 = base;
                let _e332 = fog[3u];
                base = (_e330 * (1f - _e332));
            } else {
                if override_type_3_8 {
                    let _e336 = base[3u];
                    let _e338 = fog[3u];
                    base[3u] = (_e336 * (1f - _e338));
                } else {
                    let _e342 = base;
                    let _e343 = fog;
                    let _e345 = unnamed.fogColor;
                    let _e348 = fog[3u];
                    base = mix(_e342, (_e343 * _e345), vec4(_e348));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e352 = base[3u];
        if (_e352 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e354 = base;
            let _e356 = base;
            if (dot(_e354.xyz, _e356.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e360 = base;
    out_color = _e360;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e15 = out_color;
    return _e15;
}
