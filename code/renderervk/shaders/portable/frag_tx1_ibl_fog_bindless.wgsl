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

    let _e71 = (*cosTheta);
    t = (1f - _e71);
    let _e73 = t;
    let _e74 = t;
    t2_ = (_e73 * _e74);
    let _e76 = (*roughness);
    let _e79 = (*F0_);
    Fmax = max(vec3((1f - _e76)), _e79);
    let _e81 = (*F0_);
    let _e82 = Fmax;
    let _e83 = (*F0_);
    let _e85 = t2_;
    let _e86 = t2_;
    let _e88 = t;
    return (_e81 + ((_e82 - _e83) * ((_e85 * _e86) * _e88)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c);
    (*c) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e92 = (*uv);
    let _e93 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    c_1 = _e93;
    let _e94 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e94))) == 0i) {
        let _e99 = c_1;
        param = _e99.xyz;
        let _e101 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e101.x;
        c_1[1u] = _e101.y;
        c_1[2u] = _e101.z;
    }
    let _e108 = (*slot);
    if (lightmap_slot == (_e108 + 1i)) {
        let _e113 = unnamed.worldLightParams[0u];
        let _e114 = c_1;
        let _e116 = (_e114.xyz * _e113);
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = c_1;
    return _e123;
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

    let _e106 = unnamed.packed_indices[0i][3u];
    let _e112 = unnamed.packed_indices[0i][3u];
    let _e117 = fog_tex_coord_1;
    let _e118 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    fog = _e118;
    let _e119 = frag_color0In_1;
    param_1 = _e119.xyz;
    let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e123 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e121.x, _e121.y, _e121.z, _e123);
    param_2 = 0u;
    let _e128 = frag_tex_coord0_1;
    param_3 = _e128;
    param_4 = 0i;
    let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e130 = frag_color0_;
    color0_ = (_e129 * _e130);
    if override_type_11_ {
        param_5 = 1u;
        let _e132 = frag_tex_coord1_1;
        param_6 = _e132;
        param_7 = 1i;
        let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e133;
        let _e134 = color0_;
        let _e136 = color1_;
        let _e138 = (_e134.xyz + _e136.xyz);
        let _e140 = color0_[3u];
        let _e142 = color1_[3u];
        base = vec4<f32>(_e138.x, _e138.y, _e138.z, (_e140 * _e142));
    } else {
        if override_type_11_1 {
            param_8 = 1u;
            let _e148 = frag_tex_coord1_1;
            param_9 = _e148;
            param_10 = 1i;
            let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e150 = frag_color0_;
            color1_1 = (_e149 * _e150);
            let _e152 = color0_;
            let _e154 = color1_1;
            let _e156 = (_e152.xyz + _e154.xyz);
            let _e158 = color0_[3u];
            let _e160 = color1_1[3u];
            base = vec4<f32>(_e156.x, _e156.y, _e156.z, (_e158 * _e160));
        } else {
            param_11 = 1u;
            let _e166 = frag_tex_coord1_1;
            param_12 = _e166;
            param_13 = 1i;
            let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e167;
            let _e168 = color0_;
            let _e170 = color1_2;
            let _e172 = (_e168.xyz * _e170.xyz);
            base[0u] = _e172.x;
            base[1u] = _e172.y;
            base[2u] = _e172.z;
            let _e180 = color0_[3u];
            let _e182 = color1_2[3u];
            base[3u] = (_e180 * _e182);
        }
    }
    if override_type_11_2 {
        let _e188 = unnamed.packed_indices[2i][0u];
        let _e194 = unnamed.packed_indices[2i][0u];
        let _e199 = frag_tex_coord0_1;
        let _e200 = textureSample(wired_bindless_images[(_e188 & 4095u)], wired_bindless_samplers[((_e194 >> bitcast<u32>(12i)) & 255u)], _e199);
        orm = _e200.xyz;
        let _e203 = orm[0u];
        ao = _e203;
        let _e205 = orm[1u];
        roughness_1 = clamp(_e205, 0.04f, 1f);
        let _e208 = orm[2u];
        metalness = _e208;
        let _e209 = ibl_N_1;
        ibl_n = normalize(_e209);
        let _e211 = ibl_V_1;
        ibl_v = normalize(_e211);
        let _e213 = base;
        let _e215 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e213.xyz, vec3(_e215));
        let _e218 = ibl_n;
        let _e219 = ibl_v;
        NdotV = max(dot(_e218, _e219), 0f);
        let _e222 = NdotV;
        param_14 = _e222;
        let _e223 = F0_1;
        param_15 = _e223;
        let _e224 = roughness_1;
        param_16 = _e224;
        let _e225 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_14), (&param_15), (&param_16));
        F_amb = _e225;
        let _e226 = ibl_v;
        let _e228 = ibl_n;
        R = reflect(-(_e226), _e228);
        let _e230 = R;
        let _e231 = roughness_1;
        let _e233 = textureSampleLevel(radianceCube, radianceCube_sampler, _e230, (_e231 * 5f));
        prefiltered = _e233.xyz;
        let _e235 = NdotV;
        let _e236 = roughness_1;
        let _e238 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e235, _e236), 0f);
        envBRDF = _e238.xy;
        let _e240 = prefiltered;
        let _e241 = F_amb;
        let _e243 = envBRDF[0u];
        let _e246 = envBRDF[1u];
        let _e250 = ao;
        specularIBL = ((_e240 * ((_e241 * _e243) + vec3(_e246))) * _e250);
        let _e252 = gl_FragCoord_1;
        let _e254 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e252.xy / vec2<f32>(vec2<i32>(_e254)));
        let _e258 = gtaoUV;
        let _e259 = textureSample(gtaoMap, gtaoMap_sampler, _e258);
        gtao_vis = _e259.x;
        let _e261 = gtao_vis;
        let _e262 = specularIBL;
        specularIBL = (_e262 * _e261);
        let _e264 = specularIBL;
        let _e265 = base;
        let _e267 = (_e265.xyz + _e264);
        base[0u] = _e267.x;
        base[1u] = _e267.y;
        base[2u] = _e267.z;
    }
    if override_type_11_3 {
        let _e274 = base;
        let _e277 = fog[3u];
        let _e279 = (_e274.xyz * (1f - _e277));
        base[0u] = _e279.x;
        base[1u] = _e279.y;
        base[2u] = _e279.z;
    } else {
        if override_type_11_4 {
            let _e286 = base;
            let _e288 = fog[3u];
            base = (_e286 * (1f - _e288));
        } else {
            if override_type_11_5 {
                let _e292 = base[3u];
                let _e294 = fog[3u];
                base[3u] = (_e292 * (1f - _e294));
            } else {
                let _e298 = base;
                let _e299 = fog;
                let _e301 = unnamed.fogColor;
                let _e304 = fog[3u];
                base = mix(_e298, (_e299 * _e301), vec4(_e304));
            }
        }
    }
    if override_type_11_6 {
        let _e308 = base[3u];
        if (_e308 == 0f) {
            discard;
        }
    } else {
        if override_type_11_7 {
            let _e310 = base;
            let _e312 = base;
            if (dot(_e310.xyz, _e312.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e316 = base;
    out_color = _e316;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e15 = out_color;
    return _e15;
}
