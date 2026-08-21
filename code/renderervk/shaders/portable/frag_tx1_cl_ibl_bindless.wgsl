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

    let _e76 = (*cosTheta);
    t = (1f - _e76);
    let _e78 = t;
    let _e79 = t;
    t2_ = (_e78 * _e79);
    let _e81 = (*roughness);
    let _e84 = (*F0_);
    Fmax = max(vec3((1f - _e81)), _e84);
    let _e86 = (*F0_);
    let _e87 = Fmax;
    let _e88 = (*F0_);
    let _e90 = t2_;
    let _e91 = t2_;
    let _e93 = t;
    return (_e86 + ((_e87 - _e88) * ((_e90 * _e91) * _e93)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e118 = unnamed.worldLightParams[0u];
        let _e119 = c_1;
        let _e121 = (_e119.xyz * _e118);
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = c_1;
    return _e128;
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

    let _e125 = frag_color0In_1;
    param_1 = _e125.xyz;
    let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e129 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e127.x, _e127.y, _e127.z, _e129);
    let _e134 = frag_color1In_1;
    param_2 = _e134.xyz;
    let _e136 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e138 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e136.x, _e136.y, _e136.z, _e138);
    param_3 = 0u;
    let _e143 = frag_tex_coord0_1;
    param_4 = _e143;
    param_5 = 0i;
    let _e144 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e145 = frag_color0_;
    color0_ = (_e144 * _e145);
    if override_type_11_2 {
        param_6 = 1u;
        let _e147 = frag_tex_coord1_1;
        param_7 = _e147;
        param_8 = 1i;
        let _e148 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e149 = frag_color1_;
        color1_ = (_e148 * _e149);
        let _e151 = color0_;
        let _e153 = color1_;
        let _e155 = (_e151.xyz + _e153.xyz);
        let _e157 = color0_[3u];
        let _e159 = color1_[3u];
        base = vec4<f32>(_e155.x, _e155.y, _e155.z, (_e157 * _e159));
    } else {
        if override_type_11_3 {
            param_9 = 1u;
            let _e165 = frag_tex_coord1_1;
            param_10 = _e165;
            param_11 = 1i;
            let _e166 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e167 = frag_color1_;
            color1_1 = (_e166 * _e167);
            let _e170 = color0_[3u];
            let _e171 = color0_;
            color0_ = (_e171 * _e170);
            let _e174 = color1_1[3u];
            let _e175 = color1_1;
            color1_1 = (_e175 * _e174);
            let _e177 = color0_;
            let _e179 = color1_1;
            let _e181 = (_e177.xyz + _e179.xyz);
            let _e183 = color0_[3u];
            let _e185 = color1_1[3u];
            base = vec4<f32>(_e181.x, _e181.y, _e181.z, (_e183 * _e185));
        } else {
            if override_type_11_4 {
                param_12 = 1u;
                let _e191 = frag_tex_coord1_1;
                param_13 = _e191;
                param_14 = 1i;
                let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e193 = frag_color1_;
                color1_2 = (_e192 * _e193);
                let _e196 = color0_[3u];
                let _e198 = color0_;
                color0_ = (_e198 * (1f - _e196));
                let _e201 = color1_2[3u];
                let _e203 = color1_2;
                color1_2 = (_e203 * (1f - _e201));
                let _e205 = color0_;
                let _e207 = color1_2;
                let _e209 = (_e205.xyz + _e207.xyz);
                let _e211 = color0_[3u];
                let _e213 = color1_2[3u];
                base = vec4<f32>(_e209.x, _e209.y, _e209.z, (_e211 * _e213));
            } else {
                if override_type_11_5 {
                    param_15 = 1u;
                    let _e219 = frag_tex_coord1_1;
                    param_16 = _e219;
                    param_17 = 1i;
                    let _e220 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e221 = frag_color1_;
                    color1_3 = (_e220 * _e221);
                    let _e223 = color0_;
                    let _e224 = color1_3;
                    let _e226 = color1_3[3u];
                    base = mix(_e223, _e224, vec4(_e226));
                } else {
                    if override_type_11_6 {
                        param_18 = 1u;
                        let _e229 = frag_tex_coord1_1;
                        param_19 = _e229;
                        param_20 = 1i;
                        let _e230 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e231 = frag_color1_;
                        color1_4 = (_e230 * _e231);
                        let _e233 = color1_4;
                        let _e234 = color0_;
                        let _e236 = color1_4[3u];
                        base = mix(_e233, _e234, vec4(_e236));
                    } else {
                        if override_type_11_7 {
                            param_21 = 1u;
                            let _e239 = frag_tex_coord1_1;
                            param_22 = _e239;
                            param_23 = 1i;
                            let _e240 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e241 = frag_color1_;
                            color1_5 = (_e240 * _e241);
                            let _e243 = color1_5;
                            let _e245 = color1_5[3u];
                            let _e248 = color0_;
                            base = ((_e243 + vec4(_e245)) * _e248);
                        } else {
                            param_24 = 1u;
                            let _e250 = frag_tex_coord1_1;
                            param_25 = _e250;
                            param_26 = 1i;
                            let _e251 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e252 = frag_color1_;
                            color1_6 = (_e251 * _e252);
                            let _e254 = color0_;
                            let _e256 = color1_6;
                            let _e258 = (_e254.xyz * _e256.xyz);
                            base[0u] = _e258.x;
                            base[1u] = _e258.y;
                            base[2u] = _e258.z;
                            let _e266 = color0_[3u];
                            let _e268 = color1_6[3u];
                            base[3u] = (_e266 * _e268);
                        }
                    }
                }
            }
        }
    }
    if override_type_11_8 {
        let _e274 = unnamed.packed_indices[2i][0u];
        let _e280 = unnamed.packed_indices[2i][0u];
        let _e285 = frag_tex_coord0_1;
        let _e286 = textureSample(wired_bindless_images[(_e274 & 4095u)], wired_bindless_samplers[((_e280 >> bitcast<u32>(12i)) & 255u)], _e285);
        orm = _e286.xyz;
        let _e289 = orm[0u];
        ao = _e289;
        let _e291 = orm[1u];
        roughness_1 = clamp(_e291, 0.04f, 1f);
        let _e294 = orm[2u];
        metalness = _e294;
        let _e295 = ibl_N_1;
        ibl_n = normalize(_e295);
        let _e297 = ibl_V_1;
        ibl_v = normalize(_e297);
        let _e299 = base;
        let _e301 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e299.xyz, vec3(_e301));
        let _e304 = ibl_n;
        let _e305 = ibl_v;
        NdotV = max(dot(_e304, _e305), 0f);
        let _e308 = NdotV;
        param_27 = _e308;
        let _e309 = F0_1;
        param_28 = _e309;
        let _e310 = roughness_1;
        param_29 = _e310;
        let _e311 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_27), (&param_28), (&param_29));
        F_amb = _e311;
        let _e312 = ibl_v;
        let _e314 = ibl_n;
        R = reflect(-(_e312), _e314);
        let _e316 = R;
        let _e317 = roughness_1;
        let _e319 = textureSampleLevel(radianceCube, radianceCube_sampler, _e316, (_e317 * 5f));
        prefiltered = _e319.xyz;
        let _e321 = NdotV;
        let _e322 = roughness_1;
        let _e324 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e321, _e322), 0f);
        envBRDF = _e324.xy;
        let _e326 = prefiltered;
        let _e327 = F_amb;
        let _e329 = envBRDF[0u];
        let _e332 = envBRDF[1u];
        let _e336 = ao;
        specularIBL = ((_e326 * ((_e327 * _e329) + vec3(_e332))) * _e336);
        let _e338 = gl_FragCoord_1;
        let _e340 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e338.xy / vec2<f32>(vec2<i32>(_e340)));
        let _e344 = gtaoUV;
        let _e345 = textureSample(gtaoMap, gtaoMap_sampler, _e344);
        gtao_vis = _e345.x;
        let _e347 = gtao_vis;
        let _e348 = specularIBL;
        specularIBL = (_e348 * _e347);
        let _e350 = specularIBL;
        let _e351 = base;
        let _e353 = (_e351.xyz + _e350);
        base[0u] = _e353.x;
        base[1u] = _e353.y;
        base[2u] = _e353.z;
    }
    if override_type_11_9 {
        let _e361 = base[3u];
        if (_e361 == 0f) {
            discard;
        }
    } else {
        if override_type_11_10 {
            let _e363 = base;
            let _e365 = base;
            if (dot(_e363.xyz, _e365.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e369 = base;
    out_color = _e369;
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e15 = out_color;
    return _e15;
}
