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
@id(10) override acff: i32 = 0i;
override override_type_11_9: bool = (acff == 1i);
override override_type_11_10: bool = (acff == 2i);
override override_type_11_11: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_12: bool = (discard_mode == 1i);
override override_type_11_13: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
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
    var fog: vec4<f32>;
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

    let _e134 = unnamed.packed_indices[0i][3u];
    let _e140 = unnamed.packed_indices[0i][3u];
    let _e145 = fog_tex_coord_1;
    let _e146 = textureSample(wired_bindless_images[(_e134 & 4095u)], wired_bindless_samplers[((_e140 >> bitcast<u32>(12i)) & 255u)], _e145);
    fog = _e146;
    let _e147 = frag_color0In_1;
    param_1 = _e147.xyz;
    let _e149 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e151 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e149.x, _e149.y, _e149.z, _e151);
    let _e156 = frag_color1In_1;
    param_2 = _e156.xyz;
    let _e158 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e160 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e158.x, _e158.y, _e158.z, _e160);
    param_3 = 0u;
    let _e165 = frag_tex_coord0_1;
    param_4 = _e165;
    param_5 = 0i;
    let _e166 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e167 = frag_color0_;
    color0_ = (_e166 * _e167);
    if override_type_11_2 {
        param_6 = 1u;
        let _e169 = frag_tex_coord1_1;
        param_7 = _e169;
        param_8 = 1i;
        let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e171 = frag_color1_;
        color1_ = (_e170 * _e171);
        let _e173 = color0_;
        let _e175 = color1_;
        let _e177 = (_e173.xyz + _e175.xyz);
        let _e179 = color0_[3u];
        let _e181 = color1_[3u];
        base = vec4<f32>(_e177.x, _e177.y, _e177.z, (_e179 * _e181));
    } else {
        if override_type_11_3 {
            param_9 = 1u;
            let _e187 = frag_tex_coord1_1;
            param_10 = _e187;
            param_11 = 1i;
            let _e188 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e189 = frag_color1_;
            color1_1 = (_e188 * _e189);
            let _e192 = color0_[3u];
            let _e193 = color0_;
            color0_ = (_e193 * _e192);
            let _e196 = color1_1[3u];
            let _e197 = color1_1;
            color1_1 = (_e197 * _e196);
            let _e199 = color0_;
            let _e201 = color1_1;
            let _e203 = (_e199.xyz + _e201.xyz);
            let _e205 = color0_[3u];
            let _e207 = color1_1[3u];
            base = vec4<f32>(_e203.x, _e203.y, _e203.z, (_e205 * _e207));
        } else {
            if override_type_11_4 {
                param_12 = 1u;
                let _e213 = frag_tex_coord1_1;
                param_13 = _e213;
                param_14 = 1i;
                let _e214 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e215 = frag_color1_;
                color1_2 = (_e214 * _e215);
                let _e218 = color0_[3u];
                let _e220 = color0_;
                color0_ = (_e220 * (1f - _e218));
                let _e223 = color1_2[3u];
                let _e225 = color1_2;
                color1_2 = (_e225 * (1f - _e223));
                let _e227 = color0_;
                let _e229 = color1_2;
                let _e231 = (_e227.xyz + _e229.xyz);
                let _e233 = color0_[3u];
                let _e235 = color1_2[3u];
                base = vec4<f32>(_e231.x, _e231.y, _e231.z, (_e233 * _e235));
            } else {
                if override_type_11_5 {
                    param_15 = 1u;
                    let _e241 = frag_tex_coord1_1;
                    param_16 = _e241;
                    param_17 = 1i;
                    let _e242 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e243 = frag_color1_;
                    color1_3 = (_e242 * _e243);
                    let _e245 = color0_;
                    let _e246 = color1_3;
                    let _e248 = color1_3[3u];
                    base = mix(_e245, _e246, vec4(_e248));
                } else {
                    if override_type_11_6 {
                        param_18 = 1u;
                        let _e251 = frag_tex_coord1_1;
                        param_19 = _e251;
                        param_20 = 1i;
                        let _e252 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e253 = frag_color1_;
                        color1_4 = (_e252 * _e253);
                        let _e255 = color1_4;
                        let _e256 = color0_;
                        let _e258 = color1_4[3u];
                        base = mix(_e255, _e256, vec4(_e258));
                    } else {
                        if override_type_11_7 {
                            param_21 = 1u;
                            let _e261 = frag_tex_coord1_1;
                            param_22 = _e261;
                            param_23 = 1i;
                            let _e262 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e263 = frag_color1_;
                            color1_5 = (_e262 * _e263);
                            let _e265 = color1_5;
                            let _e267 = color1_5[3u];
                            let _e270 = color0_;
                            base = ((_e265 + vec4(_e267)) * _e270);
                        } else {
                            param_24 = 1u;
                            let _e272 = frag_tex_coord1_1;
                            param_25 = _e272;
                            param_26 = 1i;
                            let _e273 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e274 = frag_color1_;
                            color1_6 = (_e273 * _e274);
                            let _e276 = color0_;
                            let _e278 = color1_6;
                            let _e280 = (_e276.xyz * _e278.xyz);
                            base[0u] = _e280.x;
                            base[1u] = _e280.y;
                            base[2u] = _e280.z;
                            let _e288 = color0_[3u];
                            let _e290 = color1_6[3u];
                            base[3u] = (_e288 * _e290);
                        }
                    }
                }
            }
        }
    }
    if override_type_11_8 {
        let _e296 = unnamed.packed_indices[2i][0u];
        let _e302 = unnamed.packed_indices[2i][0u];
        let _e307 = frag_tex_coord0_1;
        let _e308 = textureSample(wired_bindless_images[(_e296 & 4095u)], wired_bindless_samplers[((_e302 >> bitcast<u32>(12i)) & 255u)], _e307);
        orm = _e308.xyz;
        let _e311 = orm[0u];
        ao = _e311;
        let _e313 = orm[1u];
        roughness_1 = clamp(_e313, 0.04f, 1f);
        let _e316 = orm[2u];
        metalness = _e316;
        let _e317 = ibl_N_1;
        ibl_n = normalize(_e317);
        let _e319 = ibl_V_1;
        ibl_v = normalize(_e319);
        let _e321 = base;
        let _e323 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e321.xyz, vec3(_e323));
        let _e326 = ibl_n;
        let _e327 = ibl_v;
        NdotV = max(dot(_e326, _e327), 0f);
        let _e330 = NdotV;
        param_27 = _e330;
        let _e331 = F0_1;
        param_28 = _e331;
        let _e332 = roughness_1;
        param_29 = _e332;
        let _e333 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_27), (&param_28), (&param_29));
        F_amb = _e333;
        let _e334 = ibl_v;
        let _e336 = ibl_n;
        R = reflect(-(_e334), _e336);
        let _e338 = R;
        let _e339 = roughness_1;
        let _e341 = textureSampleLevel(radianceCube, radianceCube_sampler, _e338, (_e339 * 5f));
        prefiltered = _e341.xyz;
        let _e343 = NdotV;
        let _e344 = roughness_1;
        let _e346 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e343, _e344), 0f);
        envBRDF = _e346.xy;
        let _e348 = prefiltered;
        let _e349 = F_amb;
        let _e351 = envBRDF[0u];
        let _e354 = envBRDF[1u];
        let _e358 = ao;
        specularIBL = ((_e348 * ((_e349 * _e351) + vec3(_e354))) * _e358);
        let _e360 = gl_FragCoord_1;
        let _e362 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e360.xy / vec2<f32>(vec2<i32>(_e362)));
        let _e366 = gtaoUV;
        let _e367 = textureSample(gtaoMap, gtaoMap_sampler, _e366);
        gtao_vis = _e367.x;
        let _e369 = gtao_vis;
        let _e370 = specularIBL;
        specularIBL = (_e370 * _e369);
        let _e372 = specularIBL;
        let _e373 = base;
        let _e375 = (_e373.xyz + _e372);
        base[0u] = _e375.x;
        base[1u] = _e375.y;
        base[2u] = _e375.z;
    }
    if override_type_11_9 {
        let _e382 = base;
        let _e385 = fog[3u];
        let _e387 = (_e382.xyz * (1f - _e385));
        base[0u] = _e387.x;
        base[1u] = _e387.y;
        base[2u] = _e387.z;
    } else {
        if override_type_11_10 {
            let _e394 = base;
            let _e396 = fog[3u];
            base = (_e394 * (1f - _e396));
        } else {
            if override_type_11_11 {
                let _e400 = base[3u];
                let _e402 = fog[3u];
                base[3u] = (_e400 * (1f - _e402));
            } else {
                let _e406 = base;
                let _e407 = fog;
                let _e409 = unnamed.fogColor;
                let _e412 = fog[3u];
                base = mix(_e406, (_e407 * _e409), vec4(_e412));
            }
        }
    }
    if override_type_11_12 {
        let _e416 = base[3u];
        if (_e416 == 0f) {
            discard;
        }
    } else {
        if override_type_11_13 {
            let _e418 = base;
            let _e420 = base;
            if (dot(_e418.xyz, _e420.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e424 = base;
    out_color = _e424;
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e17 = out_color;
    return _e17;
}
