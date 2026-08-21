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
@id(7) override discard_mode: i32 = 0i;
override override_type_11_3: bool = (discard_mode == 1i);
override override_type_11_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
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

    let _e64 = (*cosTheta);
    t = (1f - _e64);
    let _e66 = t;
    let _e67 = t;
    t2_ = (_e66 * _e67);
    let _e69 = (*roughness);
    let _e72 = (*F0_);
    Fmax = max(vec3((1f - _e69)), _e72);
    let _e74 = (*F0_);
    let _e75 = Fmax;
    let _e76 = (*F0_);
    let _e78 = t2_;
    let _e79 = t2_;
    let _e81 = t;
    return (_e74 + ((_e75 - _e76) * ((_e78 * _e79) * _e81)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c);
    (*c) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_1 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_1;
        param = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_1;
        let _e109 = (_e107.xyz * _e106);
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
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

    let _e95 = frag_color0In_1;
    param_1 = _e95.xyz;
    let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e99 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e97.x, _e97.y, _e97.z, _e99);
    param_2 = 0u;
    let _e104 = frag_tex_coord0_1;
    param_3 = _e104;
    param_4 = 0i;
    let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e106 = frag_color0_;
    color0_ = (_e105 * _e106);
    if override_type_11_ {
        param_5 = 1u;
        let _e108 = frag_tex_coord1_1;
        param_6 = _e108;
        param_7 = 1i;
        let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e109;
        let _e110 = color0_;
        let _e112 = color1_;
        let _e114 = (_e110.xyz + _e112.xyz);
        let _e116 = color0_[3u];
        let _e118 = color1_[3u];
        base = vec4<f32>(_e114.x, _e114.y, _e114.z, (_e116 * _e118));
    } else {
        if override_type_11_1 {
            param_8 = 1u;
            let _e124 = frag_tex_coord1_1;
            param_9 = _e124;
            param_10 = 1i;
            let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e126 = frag_color0_;
            color1_1 = (_e125 * _e126);
            let _e128 = color0_;
            let _e130 = color1_1;
            let _e132 = (_e128.xyz + _e130.xyz);
            let _e134 = color0_[3u];
            let _e136 = color1_1[3u];
            base = vec4<f32>(_e132.x, _e132.y, _e132.z, (_e134 * _e136));
        } else {
            param_11 = 1u;
            let _e142 = frag_tex_coord1_1;
            param_12 = _e142;
            param_13 = 1i;
            let _e143 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e143;
            let _e144 = color0_;
            let _e146 = color1_2;
            let _e148 = (_e144.xyz * _e146.xyz);
            base[0u] = _e148.x;
            base[1u] = _e148.y;
            base[2u] = _e148.z;
            let _e156 = color0_[3u];
            let _e158 = color1_2[3u];
            base[3u] = (_e156 * _e158);
        }
    }
    if override_type_11_2 {
        let _e164 = unnamed.packed_indices[2i][0u];
        let _e170 = unnamed.packed_indices[2i][0u];
        let _e175 = frag_tex_coord0_1;
        let _e176 = textureSample(wired_bindless_images[(_e164 & 4095u)], wired_bindless_samplers[((_e170 >> bitcast<u32>(12i)) & 255u)], _e175);
        orm = _e176.xyz;
        let _e179 = orm[0u];
        ao = _e179;
        let _e181 = orm[1u];
        roughness_1 = clamp(_e181, 0.04f, 1f);
        let _e184 = orm[2u];
        metalness = _e184;
        let _e185 = ibl_N_1;
        ibl_n = normalize(_e185);
        let _e187 = ibl_V_1;
        ibl_v = normalize(_e187);
        let _e189 = base;
        let _e191 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e189.xyz, vec3(_e191));
        let _e194 = ibl_n;
        let _e195 = ibl_v;
        NdotV = max(dot(_e194, _e195), 0f);
        let _e198 = NdotV;
        param_14 = _e198;
        let _e199 = F0_1;
        param_15 = _e199;
        let _e200 = roughness_1;
        param_16 = _e200;
        let _e201 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        F_amb = _e201;
        let _e202 = ibl_v;
        let _e204 = ibl_n;
        R = reflect(-(_e202), _e204);
        let _e206 = R;
        let _e207 = roughness_1;
        let _e209 = textureSampleLevel(radianceCube, radianceCube_sampler, _e206, (_e207 * 5f));
        prefiltered = _e209.xyz;
        let _e211 = NdotV;
        let _e212 = roughness_1;
        let _e214 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e211, _e212), 0f);
        envBRDF = _e214.xy;
        let _e216 = prefiltered;
        let _e217 = F_amb;
        let _e219 = envBRDF[0u];
        let _e222 = envBRDF[1u];
        let _e226 = ao;
        specularIBL = ((_e216 * ((_e217 * _e219) + vec3(_e222))) * _e226);
        let _e228 = gl_FragCoord_1;
        let _e230 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e228.xy / vec2<f32>(vec2<i32>(_e230)));
        let _e234 = gtaoUV;
        let _e235 = textureSample(gtaoMap, gtaoMap_sampler, _e234);
        gtao_vis = _e235.x;
        let _e237 = gtao_vis;
        let _e238 = specularIBL;
        specularIBL = (_e238 * _e237);
        let _e240 = specularIBL;
        let _e241 = base;
        let _e243 = (_e241.xyz + _e240);
        base[0u] = _e243.x;
        base[1u] = _e243.y;
        base[2u] = _e243.z;
    }
    if override_type_11_3 {
        let _e251 = base[3u];
        if (_e251 == 0f) {
            discard;
        }
    } else {
        if override_type_11_4 {
            let _e253 = base;
            let _e255 = base;
            if (dot(_e253.xyz, _e255.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e259 = base;
    out_color = _e259;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e13 = out_color;
    return _e13;
}
