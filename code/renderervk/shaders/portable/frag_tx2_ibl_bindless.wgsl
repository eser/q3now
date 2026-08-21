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

    let _e65 = (*cosTheta);
    t = (1f - _e65);
    let _e67 = t;
    let _e68 = t;
    t2_ = (_e67 * _e68);
    let _e70 = (*roughness);
    let _e73 = (*F0_);
    Fmax = max(vec3((1f - _e70)), _e73);
    let _e75 = (*F0_);
    let _e76 = Fmax;
    let _e77 = (*F0_);
    let _e79 = t2_;
    let _e80 = t2_;
    let _e82 = t;
    return (_e75 + ((_e76 - _e77) * ((_e79 * _e80) * _e82)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e63 = (*c);
    (*c) = max(_e63, vec3<f32>(0f, 0f, 0f));
    let _e65 = (*c);
    cutoff = (_e65 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e67 = (*c);
    lo = (_e67 / vec3(12.92f));
    let _e70 = (*c);
    hi = pow(((_e70 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e75 = hi;
    let _e76 = lo;
    let _e77 = cutoff;
    return mix(_e75, _e76, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e77));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e64 = (*role);
    let _e66 = (*role);
    let _e71 = unnamed.packed_indices[(_e64 / 4u)][(_e66 % 4u)];
    let _e74 = (*role);
    let _e76 = (*role);
    let _e81 = unnamed.packed_indices[(_e74 / 4u)][(_e76 % 4u)];
    let _e86 = (*uv);
    let _e87 = textureSample(wired_bindless_images[(_e71 & 4095u)], wired_bindless_samplers[((_e81 >> bitcast<u32>(12i)) & 255u)], _e86);
    c_1 = _e87;
    let _e88 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e88))) == 0i) {
        let _e93 = c_1;
        param = _e93.xyz;
        let _e95 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e95.x;
        c_1[1u] = _e95.y;
        c_1[2u] = _e95.z;
    }
    let _e102 = (*slot);
    if (lightmap_slot == (_e102 + 1i)) {
        let _e107 = unnamed.worldLightParams[0u];
        let _e108 = c_1;
        let _e110 = (_e108.xyz * _e107);
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = c_1;
    return _e117;
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

    let _e108 = frag_color0In_1;
    param_1 = _e108.xyz;
    let _e110 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e112 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e110.x, _e110.y, _e110.z, _e112);
    param_2 = 0u;
    let _e117 = frag_tex_coord0_1;
    param_3 = _e117;
    param_4 = 0i;
    let _e118 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e119 = frag_color0_;
    color0_ = (_e118 * _e119);
    if override_type_11_ {
        param_5 = 1u;
        let _e121 = frag_tex_coord1_1;
        param_6 = _e121;
        param_7 = 1i;
        let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e122;
        param_8 = 2u;
        let _e123 = frag_tex_coord2_1;
        param_9 = _e123;
        param_10 = 2i;
        let _e124 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e124;
        let _e125 = color0_;
        let _e127 = color1_;
        let _e130 = color2_;
        let _e132 = ((_e125.xyz + _e127.xyz) + _e130.xyz);
        let _e134 = color0_[3u];
        let _e136 = color1_[3u];
        let _e139 = color2_[3u];
        base = vec4<f32>(_e132.x, _e132.y, _e132.z, ((_e134 * _e136) * _e139));
    } else {
        if override_type_11_1 {
            param_11 = 1u;
            let _e145 = frag_tex_coord1_1;
            param_12 = _e145;
            param_13 = 1i;
            let _e146 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e147 = frag_color0_;
            color1_1 = (_e146 * _e147);
            param_14 = 2u;
            let _e149 = frag_tex_coord2_1;
            param_15 = _e149;
            param_16 = 2i;
            let _e150 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e151 = frag_color0_;
            color2_1 = (_e150 * _e151);
            let _e153 = color0_;
            let _e155 = color1_1;
            let _e158 = color2_1;
            let _e160 = ((_e153.xyz + _e155.xyz) + _e158.xyz);
            let _e162 = color0_[3u];
            let _e164 = color1_1[3u];
            let _e167 = color2_1[3u];
            base = vec4<f32>(_e160.x, _e160.y, _e160.z, ((_e162 * _e164) * _e167));
        } else {
            param_17 = 1u;
            let _e173 = frag_tex_coord1_1;
            param_18 = _e173;
            param_19 = 1i;
            let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e174;
            param_20 = 2u;
            let _e175 = frag_tex_coord2_1;
            param_21 = _e175;
            param_22 = 2i;
            let _e176 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e176;
            let _e177 = color0_;
            let _e179 = color1_2;
            let _e182 = color2_2;
            let _e184 = ((_e177.xyz * _e179.xyz) * _e182.xyz);
            base[0u] = _e184.x;
            base[1u] = _e184.y;
            base[2u] = _e184.z;
            let _e192 = color0_[3u];
            let _e194 = color1_2[3u];
            let _e197 = color2_2[3u];
            base[3u] = ((_e192 * _e194) * _e197);
        }
    }
    if override_type_11_2 {
        let _e203 = unnamed.packed_indices[2i][0u];
        let _e209 = unnamed.packed_indices[2i][0u];
        let _e214 = frag_tex_coord0_1;
        let _e215 = textureSample(wired_bindless_images[(_e203 & 4095u)], wired_bindless_samplers[((_e209 >> bitcast<u32>(12i)) & 255u)], _e214);
        orm = _e215.xyz;
        let _e218 = orm[0u];
        ao = _e218;
        let _e220 = orm[1u];
        roughness_1 = clamp(_e220, 0.04f, 1f);
        let _e223 = orm[2u];
        metalness = _e223;
        let _e224 = ibl_N_1;
        ibl_n = normalize(_e224);
        let _e226 = ibl_V_1;
        ibl_v = normalize(_e226);
        let _e228 = base;
        let _e230 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e228.xyz, vec3(_e230));
        let _e233 = ibl_n;
        let _e234 = ibl_v;
        NdotV = max(dot(_e233, _e234), 0f);
        let _e237 = NdotV;
        param_23 = _e237;
        let _e238 = F0_1;
        param_24 = _e238;
        let _e239 = roughness_1;
        param_25 = _e239;
        let _e240 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_23), (&param_24), (&param_25));
        F_amb = _e240;
        let _e241 = ibl_v;
        let _e243 = ibl_n;
        R = reflect(-(_e241), _e243);
        let _e245 = R;
        let _e246 = roughness_1;
        let _e248 = textureSampleLevel(radianceCube, radianceCube_sampler, _e245, (_e246 * 5f));
        prefiltered = _e248.xyz;
        let _e250 = NdotV;
        let _e251 = roughness_1;
        let _e253 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e250, _e251), 0f);
        envBRDF = _e253.xy;
        let _e255 = prefiltered;
        let _e256 = F_amb;
        let _e258 = envBRDF[0u];
        let _e261 = envBRDF[1u];
        let _e265 = ao;
        specularIBL = ((_e255 * ((_e256 * _e258) + vec3(_e261))) * _e265);
        let _e267 = gl_FragCoord_1;
        let _e269 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e267.xy / vec2<f32>(vec2<i32>(_e269)));
        let _e273 = gtaoUV;
        let _e274 = textureSample(gtaoMap, gtaoMap_sampler, _e273);
        gtao_vis = _e274.x;
        let _e276 = gtao_vis;
        let _e277 = specularIBL;
        specularIBL = (_e277 * _e276);
        let _e279 = specularIBL;
        let _e280 = base;
        let _e282 = (_e280.xyz + _e279);
        base[0u] = _e282.x;
        base[1u] = _e282.y;
        base[2u] = _e282.z;
    }
    if override_type_11_3 {
        let _e290 = base[3u];
        if (_e290 == 0f) {
            discard;
        }
    } else {
        if override_type_11_4 {
            let _e292 = base;
            let _e294 = base;
            if (dot(_e292.xyz, _e294.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e298 = base;
    out_color = _e298;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e15 = out_color;
    return _e15;
}
