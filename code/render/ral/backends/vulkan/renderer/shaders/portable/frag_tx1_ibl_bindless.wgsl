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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
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

    let _e65 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e65 + 0.5f));
    let _e70 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e72 = fogType;
    let _e75 = fogType;
    return (((_e70 > 0.5f) && (_e72 >= 1i)) && (_e75 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e65 = wired_advanced_fog_enabled_u0028_();
    if !(_e65) {
        return 0f;
    }
    let _e68 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e68, 0.000001f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e73 + 0.5f));
    let _e76 = fogType_1;
    if (_e76 == 1i) {
        let _e80 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e80 <= 0f) {
            return 0f;
        }
        let _e82 = viewDepth;
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e82 / _e85), 0f, 1f);
    }
    let _e90 = unnamed.advancedFogColorDensity[3u];
    let _e92 = viewDepth;
    opticalDepth = (max(_e90, 0f) * _e92);
    let _e94 = fogType_1;
    if (_e94 == 2i) {
        let _e96 = opticalDepth;
        return clamp((1f - exp(-(_e96))), 0f, 1f);
    }
    let _e101 = opticalDepth;
    let _e102 = opticalDepth;
    return clamp((1f - exp(-((_e101 * _e102)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e68 = (*cosTheta);
    t = (1f - _e68);
    let _e70 = t;
    let _e71 = t;
    t2_ = (_e70 * _e71);
    let _e73 = (*roughness);
    let _e76 = (*F0_);
    Fmax = max(vec3((1f - _e73)), _e76);
    let _e78 = (*F0_);
    let _e79 = Fmax;
    let _e80 = (*F0_);
    let _e82 = t2_;
    let _e83 = t2_;
    let _e85 = t;
    return (_e78 + ((_e79 - _e80) * ((_e82 * _e83) * _e85)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e66 = (*c);
    (*c) = max(_e66, vec3<f32>(0f, 0f, 0f));
    let _e68 = (*c);
    cutoff = (_e68 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e70 = (*c);
    lo = (_e70 / vec3(12.92f));
    let _e73 = (*c);
    hi = pow(((_e73 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e78 = hi;
    let _e79 = lo;
    let _e80 = cutoff;
    return mix(_e78, _e79, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e80));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e89 = (*uv);
    let _e90 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    c_1 = _e90;
    let _e91 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e91))) == 0i) {
        let _e96 = c_1;
        param = _e96.xyz;
        let _e98 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e98.x;
        c_1[1u] = _e98.y;
        c_1[2u] = _e98.z;
    }
    let _e105 = (*slot);
    if (lightmap_slot == (_e105 + 1i)) {
        let _e110 = unnamed.worldLightParams[0u];
        let _e111 = c_1;
        let _e113 = (_e111.xyz * _e110);
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = c_1;
    return _e120;
}

fn main_1() {
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

    let _e100 = frag_color0In_1;
    param_1 = _e100.xyz;
    let _e102 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e104 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e102.x, _e102.y, _e102.z, _e104);
    param_2 = 0u;
    let _e109 = frag_tex_coord0_1;
    param_3 = _e109;
    param_4 = 0i;
    let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e111 = frag_color0_;
    color0_ = (_e110 * _e111);
    if override_type_3_ {
        param_5 = 1u;
        let _e113 = frag_tex_coord1_1;
        param_6 = _e113;
        param_7 = 1i;
        let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e114;
        let _e115 = color0_;
        let _e117 = color1_;
        let _e119 = (_e115.xyz + _e117.xyz);
        let _e121 = color0_[3u];
        let _e123 = color1_[3u];
        base = vec4<f32>(_e119.x, _e119.y, _e119.z, (_e121 * _e123));
    } else {
        if override_type_3_1 {
            param_8 = 1u;
            let _e129 = frag_tex_coord1_1;
            param_9 = _e129;
            param_10 = 1i;
            let _e130 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e131 = frag_color0_;
            color1_1 = (_e130 * _e131);
            let _e133 = color0_;
            let _e135 = color1_1;
            let _e137 = (_e133.xyz + _e135.xyz);
            let _e139 = color0_[3u];
            let _e141 = color1_1[3u];
            base = vec4<f32>(_e137.x, _e137.y, _e137.z, (_e139 * _e141));
        } else {
            param_11 = 1u;
            let _e147 = frag_tex_coord1_1;
            param_12 = _e147;
            param_13 = 1i;
            let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e148;
            let _e149 = color0_;
            let _e151 = color1_2;
            let _e153 = (_e149.xyz * _e151.xyz);
            base[0u] = _e153.x;
            base[1u] = _e153.y;
            base[2u] = _e153.z;
            let _e161 = color0_[3u];
            let _e163 = color1_2[3u];
            base[3u] = (_e161 * _e163);
        }
    }
    if override_type_3_2 {
        let _e169 = unnamed.packed_indices[2i][0u];
        let _e175 = unnamed.packed_indices[2i][0u];
        let _e180 = frag_tex_coord0_1;
        let _e181 = textureSample(wired_bindless_images[(_e169 & 4095u)], wired_bindless_samplers[((_e175 >> bitcast<u32>(12i)) & 255u)], _e180);
        orm = _e181.xyz;
        let _e184 = orm[0u];
        ao = _e184;
        let _e186 = orm[1u];
        roughness_1 = clamp(_e186, 0.04f, 1f);
        let _e189 = orm[2u];
        metalness = _e189;
        let _e190 = ibl_N_1;
        ibl_n = normalize(_e190);
        let _e192 = ibl_V_1;
        ibl_v = normalize(_e192);
        let _e194 = base;
        let _e196 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e194.xyz, vec3(_e196));
        let _e199 = ibl_n;
        let _e200 = ibl_v;
        NdotV = max(dot(_e199, _e200), 0f);
        let _e203 = NdotV;
        param_14 = _e203;
        let _e204 = F0_1;
        param_15 = _e204;
        let _e205 = roughness_1;
        param_16 = _e205;
        let _e206 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        F_amb = _e206;
        let _e207 = ibl_v;
        let _e209 = ibl_n;
        R = reflect(-(_e207), _e209);
        let _e211 = R;
        let _e212 = roughness_1;
        let _e214 = textureSampleLevel(radianceCube, radianceCube_sampler, _e211, (_e212 * 5f));
        prefiltered = _e214.xyz;
        let _e216 = NdotV;
        let _e217 = roughness_1;
        let _e219 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e216, _e217), 0f);
        envBRDF = _e219.xy;
        let _e221 = prefiltered;
        let _e222 = F_amb;
        let _e224 = envBRDF[0u];
        let _e227 = envBRDF[1u];
        let _e231 = ao;
        specularIBL = ((_e221 * ((_e222 * _e224) + vec3(_e227))) * _e231);
        let _e233 = gl_FragCoord_1;
        let _e235 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e233.xy / vec2<f32>(vec2<i32>(_e235)));
        let _e239 = gtaoUV;
        let _e240 = textureSample(gtaoMap, gtaoMap_sampler, _e239);
        gtao_vis = _e240.x;
        let _e242 = gtao_vis;
        let _e243 = specularIBL;
        specularIBL = (_e243 * _e242);
        let _e245 = specularIBL;
        let _e246 = base;
        let _e248 = (_e246.xyz + _e245);
        base[0u] = _e248.x;
        base[1u] = _e248.y;
        base[2u] = _e248.z;
    }
    let _e255 = wired_advanced_fog_enabled_u0028_();
    if _e255 {
        let _e256 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e256;
        let _e257 = base;
        let _e260 = unnamed.advancedFogColorDensity;
        let _e262 = fogAmount;
        let _e264 = mix(_e257.xyz, _e260.xyz, vec3(_e262));
        base[0u] = _e264.x;
        base[1u] = _e264.y;
        base[2u] = _e264.z;
    }
    if override_type_3_3 {
        let _e272 = base[3u];
        if (_e272 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e274 = base;
            let _e276 = base;
            if (dot(_e274.xyz, _e276.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e280 = base;
    out_color = _e280;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e13 = out_color;
    return _e13;
}
