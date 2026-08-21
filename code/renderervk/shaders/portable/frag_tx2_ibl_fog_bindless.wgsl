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
@id(14) override ibl_enabled: i32 = 0i;
override override_type_11_2: bool = (ibl_enabled != 0i);
@id(10) override acff: i32 = 0i;
override override_type_11_3: bool = (acff == 1i);
override override_type_11_4: bool = (acff == 2i);
override override_type_11_5: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_6: bool = (discard_mode == 1i);
override override_type_11_7: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
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

    let _e72 = (*cosTheta);
    t = (1f - _e72);
    let _e74 = t;
    let _e75 = t;
    t2_ = (_e74 * _e75);
    let _e77 = (*roughness);
    let _e80 = (*F0_);
    Fmax = max(vec3((1f - _e77)), _e80);
    let _e82 = (*F0_);
    let _e83 = Fmax;
    let _e84 = (*F0_);
    let _e86 = t2_;
    let _e87 = t2_;
    let _e89 = t;
    return (_e82 + ((_e83 - _e84) * ((_e86 * _e87) * _e89)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c);
    (*c) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_1 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_1;
        param = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e114 = unnamed.worldLightParams[0u];
        let _e115 = c_1;
        let _e117 = (_e115.xyz * _e114);
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
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

    let _e119 = unnamed.packed_indices[0i][3u];
    let _e125 = unnamed.packed_indices[0i][3u];
    let _e130 = fog_tex_coord_1;
    let _e131 = textureSample(wired_bindless_images[(_e119 & 4095u)], wired_bindless_samplers[((_e125 >> bitcast<u32>(12i)) & 255u)], _e130);
    fog = _e131;
    let _e132 = frag_color0In_1;
    param_1 = _e132.xyz;
    let _e134 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e136 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e134.x, _e134.y, _e134.z, _e136);
    param_2 = 0u;
    let _e141 = frag_tex_coord0_1;
    param_3 = _e141;
    param_4 = 0i;
    let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e143 = frag_color0_;
    color0_ = (_e142 * _e143);
    if override_type_11_ {
        param_5 = 1u;
        let _e145 = frag_tex_coord1_1;
        param_6 = _e145;
        param_7 = 1i;
        let _e146 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e146;
        param_8 = 2u;
        let _e147 = frag_tex_coord2_1;
        param_9 = _e147;
        param_10 = 2i;
        let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e148;
        let _e149 = color0_;
        let _e151 = color1_;
        let _e154 = color2_;
        let _e156 = ((_e149.xyz + _e151.xyz) + _e154.xyz);
        let _e158 = color0_[3u];
        let _e160 = color1_[3u];
        let _e163 = color2_[3u];
        base = vec4<f32>(_e156.x, _e156.y, _e156.z, ((_e158 * _e160) * _e163));
    } else {
        if override_type_11_1 {
            param_11 = 1u;
            let _e169 = frag_tex_coord1_1;
            param_12 = _e169;
            param_13 = 1i;
            let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e171 = frag_color0_;
            color1_1 = (_e170 * _e171);
            param_14 = 2u;
            let _e173 = frag_tex_coord2_1;
            param_15 = _e173;
            param_16 = 2i;
            let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e175 = frag_color0_;
            color2_1 = (_e174 * _e175);
            let _e177 = color0_;
            let _e179 = color1_1;
            let _e182 = color2_1;
            let _e184 = ((_e177.xyz + _e179.xyz) + _e182.xyz);
            let _e186 = color0_[3u];
            let _e188 = color1_1[3u];
            let _e191 = color2_1[3u];
            base = vec4<f32>(_e184.x, _e184.y, _e184.z, ((_e186 * _e188) * _e191));
        } else {
            param_17 = 1u;
            let _e197 = frag_tex_coord1_1;
            param_18 = _e197;
            param_19 = 1i;
            let _e198 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e198;
            param_20 = 2u;
            let _e199 = frag_tex_coord2_1;
            param_21 = _e199;
            param_22 = 2i;
            let _e200 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e200;
            let _e201 = color0_;
            let _e203 = color1_2;
            let _e206 = color2_2;
            let _e208 = ((_e201.xyz * _e203.xyz) * _e206.xyz);
            base[0u] = _e208.x;
            base[1u] = _e208.y;
            base[2u] = _e208.z;
            let _e216 = color0_[3u];
            let _e218 = color1_2[3u];
            let _e221 = color2_2[3u];
            base[3u] = ((_e216 * _e218) * _e221);
        }
    }
    if override_type_11_2 {
        let _e227 = unnamed.packed_indices[2i][0u];
        let _e233 = unnamed.packed_indices[2i][0u];
        let _e238 = frag_tex_coord0_1;
        let _e239 = textureSample(wired_bindless_images[(_e227 & 4095u)], wired_bindless_samplers[((_e233 >> bitcast<u32>(12i)) & 255u)], _e238);
        orm = _e239.xyz;
        let _e242 = orm[0u];
        ao = _e242;
        let _e244 = orm[1u];
        roughness_1 = clamp(_e244, 0.04f, 1f);
        let _e247 = orm[2u];
        metalness = _e247;
        let _e248 = ibl_N_1;
        ibl_n = normalize(_e248);
        let _e250 = ibl_V_1;
        ibl_v = normalize(_e250);
        let _e252 = base;
        let _e254 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e252.xyz, vec3(_e254));
        let _e257 = ibl_n;
        let _e258 = ibl_v;
        NdotV = max(dot(_e257, _e258), 0f);
        let _e261 = NdotV;
        param_23 = _e261;
        let _e262 = F0_1;
        param_24 = _e262;
        let _e263 = roughness_1;
        param_25 = _e263;
        let _e264 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_23), (&param_24), (&param_25));
        F_amb = _e264;
        let _e265 = ibl_v;
        let _e267 = ibl_n;
        R = reflect(-(_e265), _e267);
        let _e269 = R;
        let _e270 = roughness_1;
        let _e272 = textureSampleLevel(radianceCube, radianceCube_sampler, _e269, (_e270 * 5f));
        prefiltered = _e272.xyz;
        let _e274 = NdotV;
        let _e275 = roughness_1;
        let _e277 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e274, _e275), 0f);
        envBRDF = _e277.xy;
        let _e279 = prefiltered;
        let _e280 = F_amb;
        let _e282 = envBRDF[0u];
        let _e285 = envBRDF[1u];
        let _e289 = ao;
        specularIBL = ((_e279 * ((_e280 * _e282) + vec3(_e285))) * _e289);
        let _e291 = gl_FragCoord_1;
        let _e293 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e291.xy / vec2<f32>(vec2<i32>(_e293)));
        let _e297 = gtaoUV;
        let _e298 = textureSample(gtaoMap, gtaoMap_sampler, _e297);
        gtao_vis = _e298.x;
        let _e300 = gtao_vis;
        let _e301 = specularIBL;
        specularIBL = (_e301 * _e300);
        let _e303 = specularIBL;
        let _e304 = base;
        let _e306 = (_e304.xyz + _e303);
        base[0u] = _e306.x;
        base[1u] = _e306.y;
        base[2u] = _e306.z;
    }
    if override_type_11_3 {
        let _e313 = base;
        let _e316 = fog[3u];
        let _e318 = (_e313.xyz * (1f - _e316));
        base[0u] = _e318.x;
        base[1u] = _e318.y;
        base[2u] = _e318.z;
    } else {
        if override_type_11_4 {
            let _e325 = base;
            let _e327 = fog[3u];
            base = (_e325 * (1f - _e327));
        } else {
            if override_type_11_5 {
                let _e331 = base[3u];
                let _e333 = fog[3u];
                base[3u] = (_e331 * (1f - _e333));
            } else {
                let _e337 = base;
                let _e338 = fog;
                let _e340 = unnamed.fogColor;
                let _e343 = fog[3u];
                base = mix(_e337, (_e338 * _e340), vec4(_e343));
            }
        }
    }
    if override_type_11_6 {
        let _e347 = base[3u];
        if (_e347 == 0f) {
            discard;
        }
    } else {
        if override_type_11_7 {
            let _e349 = base;
            let _e351 = base;
            if (dot(_e349.xyz, _e351.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e355 = base;
    out_color = _e355;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e17 = out_color;
    return _e17;
}
