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
}

@id(4) override tex_domain: i32 = 0i;
@id(3) override alpha_to_coverage: i32 = 0i;
override override_type_22_: bool = (alpha_to_coverage != 0i);
@id(0) override alpha_test_func: i32 = 0i;
override override_type_22_1: bool = (alpha_test_func == 1i);
override override_type_22_2: bool = (alpha_test_func == 2i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_22_3: bool = (alpha_test_func == 3i);
override override_type_22_4: bool = (alpha_test_func == 1i);
override override_type_22_5: bool = (alpha_test_func == 2i);
override override_type_22_6: bool = (alpha_test_func == 3i);
@id(5) override abs_light: i32 = 0i;
override override_type_22_7: bool = (abs_light != 0i);

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> N_1: vec3<f32>;
var<private> L_1: vec4<f32>;
var<private> V_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn FresnelSchlick_u0028_f1_u003b_vf3_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;

    let _e58 = (*cosTheta);
    t = (1f - _e58);
    let _e60 = t;
    let _e61 = t;
    t2_ = (_e60 * _e61);
    let _e63 = (*F0_);
    let _e64 = (*F0_);
    let _e67 = t2_;
    let _e68 = t2_;
    let _e70 = t;
    return (_e63 + ((vec3(1f) - _e64) * ((_e67 * _e68) * _e70)));
}

fn GeometrySchlickGGX_u0028_f1_u003b_f1_u003b(NdotV: ptr<function, f32>, roughness: ptr<function, f32>) -> f32 {
    var r: f32;
    var k: f32;

    let _e58 = (*roughness);
    r = (_e58 + 1f);
    let _e60 = r;
    let _e61 = r;
    k = ((_e60 * _e61) / 8f);
    let _e64 = (*NdotV);
    let _e65 = (*NdotV);
    let _e66 = k;
    let _e69 = k;
    return (_e64 / ((_e65 * (1f - _e66)) + _e69));
}

fn GeometrySmith_u0028_f1_u003b_f1_u003b_f1_u003b(NdotV_1: ptr<function, f32>, NdotL: ptr<function, f32>, roughness_1: ptr<function, f32>) -> f32 {
    var param: f32;
    var param_1: f32;
    var param_2: f32;
    var param_3: f32;

    let _e61 = (*NdotV_1);
    param = _e61;
    let _e62 = (*roughness_1);
    param_1 = _e62;
    let _e63 = GeometrySchlickGGX_u0028_f1_u003b_f1_u003b((&param), (&param_1));
    let _e64 = (*NdotL);
    param_2 = _e64;
    let _e65 = (*roughness_1);
    param_3 = _e65;
    let _e66 = GeometrySchlickGGX_u0028_f1_u003b_f1_u003b((&param_2), (&param_3));
    return (_e63 * _e66);
}

fn DistributionGGX_u0028_f1_u003b_f1_u003b(NdotH: ptr<function, f32>, roughness_2: ptr<function, f32>) -> f32 {
    var a: f32;
    var a2_: f32;
    var denom: f32;

    let _e59 = (*roughness_2);
    let _e60 = (*roughness_2);
    a = (_e59 * _e60);
    let _e62 = a;
    let _e63 = a;
    a2_ = (_e62 * _e63);
    let _e65 = (*NdotH);
    let _e66 = (*NdotH);
    let _e68 = a2_;
    denom = (((_e65 * _e66) * (_e68 - 1f)) + 1f);
    let _e72 = a2_;
    let _e73 = denom;
    let _e75 = denom;
    return (_e72 / ((3.1415927f * _e73) * _e75));
}

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e66 = unnamed.packed_indices[0i][0u];
    let _e72 = unnamed.packed_indices[0i][0u];
    let _e77 = textureDimensions(wired_bindless_images[(_e66 & 4095u)], 0i);
    ts = vec2<i32>(_e77);
    let _e80 = (*tc)[0u];
    let _e82 = ts[0u];
    let _e85 = dpdx((_e80 * f32(_e82)));
    dx = max(abs(_e85), 0.001f);
    let _e89 = (*tc)[1u];
    let _e91 = ts[1u];
    let _e94 = dpdy((_e89 * f32(_e91)));
    dy = max(abs(_e94), 0.001f);
    let _e97 = dx;
    let _e98 = dy;
    dxy = max(_e97, _e98);
    let _e100 = dxy;
    scale = max((1f / _e100), 1f);
    let _e103 = (*threshold);
    let _e104 = (*alpha);
    let _e105 = (*threshold);
    let _e107 = scale;
    ac = (_e103 + ((_e104 - _e105) * _e107));
    let _e110 = ac;
    return _e110;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e58 = (*c);
    (*c) = max(_e58, vec3<f32>(0f, 0f, 0f));
    let _e60 = (*c);
    cutoff = (_e60 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e62 = (*c);
    lo = (_e62 / vec3(12.92f));
    let _e65 = (*c);
    hi = pow(((_e65 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e70 = hi;
    let _e71 = lo;
    let _e72 = cutoff;
    return mix(_e70, _e71, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e72));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_4: vec3<f32>;

    let _e59 = (*role);
    let _e61 = (*role);
    let _e66 = unnamed.packed_indices[(_e59 / 4u)][(_e61 % 4u)];
    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e81 = (*uv);
    let _e82 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e76 >> bitcast<u32>(12i)) & 255u)], _e81);
    c_1 = _e82;
    let _e83 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e83))) != 0i) {
        let _e88 = c_1;
        return _e88;
    }
    let _e89 = c_1;
    param_4 = _e89.xyz;
    let _e91 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    c_1[0u] = _e91.x;
    c_1[1u] = _e91.y;
    c_1[2u] = _e91.z;
    let _e98 = c_1;
    return _e98;
}

fn main_1() {
    var fog: vec4<f32>;
    var parallax_tc: vec2<f32>;
    var Np: vec3<f32>;
    var base: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var param_8: f32;
    var param_9: f32;
    var param_10: vec2<f32>;
    var param_11: f32;
    var param_12: f32;
    var param_13: vec2<f32>;
    var lightColorRadius: vec4<f32>;
    var scale_1: f32;
    var LL: vec4<f32>;
    var nL: vec3<f32>;
    var nV: vec3<f32>;
    var intensFactor: f32;
    var intens: vec3<f32>;
    var pbr: vec3<f32>;
    var ao: f32;
    var roughness_3: f32;
    var metalness: f32;
    var halfVec: vec3<f32>;
    var NdotL_1: f32;
    var NdotV_2: f32;
    var NdotH_1: f32;
    var HdotV: f32;
    var F0_1: vec3<f32>;
    var D: f32;
    var param_14: f32;
    var param_15: f32;
    var G: f32;
    var param_16: f32;
    var param_17: f32;
    var param_18: f32;
    var F: vec3<f32>;
    var param_19: f32;
    var param_20: vec3<f32>;
    var specular: vec3<f32>;
    var kD: vec3<f32>;
    var diffuseColor: vec3<f32>;

    let _e100 = unnamed.packed_indices[0i][1u];
    let _e106 = unnamed.packed_indices[0i][1u];
    let _e111 = fog_tex_coord_1;
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    fog = _e112;
    let _e113 = frag_tex_coord_1;
    parallax_tc = _e113;
    let _e114 = N_1;
    Np = _e114;
    param_5 = 0u;
    let _e115 = parallax_tc;
    param_6 = _e115;
    param_7 = 0i;
    let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    base = _e116;
    if override_type_22_ {
        if override_type_22_1 {
            let _e118 = base[3u];
            base[3u] = select(0f, 1f, (_e118 > 0f));
        } else {
            if override_type_22_2 {
                let _e123 = base[3u];
                param_8 = alpha_test_value;
                param_9 = (1f - _e123);
                let _e125 = frag_tex_coord_1;
                param_10 = _e125;
                let _e126 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_8), (&param_9), (&param_10));
                base[3u] = _e126;
            } else {
                if override_type_22_3 {
                    param_11 = alpha_test_value;
                    let _e129 = base[3u];
                    param_12 = _e129;
                    let _e130 = frag_tex_coord_1;
                    param_13 = _e130;
                    let _e131 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_11), (&param_12), (&param_13));
                    base[3u] = _e131;
                }
            }
        }
    } else {
        if override_type_22_4 {
            let _e134 = base[3u];
            if (_e134 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_22_5 {
                let _e137 = base[3u];
                if (_e137 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_22_6 {
                    let _e140 = base[3u];
                    if (_e140 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e143 = unnamed.lightColor;
    lightColorRadius = _e143;
    let _e144 = L_1;
    let _e148 = unnamed.lightVector;
    let _e153 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e144.xyz), _e148.xyz) * _e153), 0f, 1f);
    let _e157 = unnamed.lightVector;
    let _e158 = scale_1;
    let _e160 = L_1;
    LL = ((_e157 * _e158) + _e160);
    let _e162 = LL;
    nL = normalize(_e162.xyz);
    let _e165 = V_1;
    nV = normalize(_e165.xyz);
    let _e168 = LL;
    let _e170 = LL;
    let _e174 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e168.xyz, _e170.xyz) * _e174));
    let _e177 = intensFactor;
    if (_e177 <= 0f) {
        discard;
    }
    let _e179 = lightColorRadius;
    let _e181 = intensFactor;
    intens = (_e179.xyz * _e181);
    let _e183 = base;
    let _e186 = fog[3u];
    let _e188 = (_e183.xyz * (1f - _e186));
    base[0u] = _e188.x;
    base[1u] = _e188.y;
    base[2u] = _e188.z;
    let _e198 = unnamed.packed_indices[0i][3u];
    let _e204 = unnamed.packed_indices[0i][3u];
    let _e209 = parallax_tc;
    let _e210 = textureSample(wired_bindless_images[(_e198 & 4095u)], wired_bindless_samplers[((_e204 >> bitcast<u32>(12i)) & 255u)], _e209);
    pbr = _e210.xyz;
    let _e213 = pbr[0u];
    ao = _e213;
    let _e215 = pbr[1u];
    roughness_3 = clamp(_e215, 0.04f, 1f);
    let _e218 = pbr[2u];
    metalness = _e218;
    let _e219 = nL;
    let _e220 = nV;
    halfVec = normalize((_e219 + _e220));
    let _e223 = Np;
    let _e224 = nL;
    NdotL_1 = max(dot(_e223, _e224), 0f);
    let _e227 = Np;
    let _e228 = nV;
    NdotV_2 = max(dot(_e227, _e228), 0.001f);
    let _e231 = Np;
    let _e232 = halfVec;
    NdotH_1 = max(dot(_e231, _e232), 0f);
    let _e235 = halfVec;
    let _e236 = nV;
    HdotV = max(dot(_e235, _e236), 0f);
    if override_type_22_7 {
        let _e239 = NdotL_1;
        let _e240 = NdotV_2;
        if ((_e239 * _e240) <= 0f) {
            discard;
        }
        let _e243 = NdotL_1;
        NdotL_1 = abs(_e243);
        let _e245 = NdotV_2;
        NdotV_2 = abs(_e245);
    }
    let _e247 = base;
    let _e249 = metalness;
    F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e247.xyz, vec3(_e249));
    let _e252 = NdotH_1;
    param_14 = _e252;
    let _e253 = roughness_3;
    param_15 = _e253;
    let _e254 = DistributionGGX_u0028_f1_u003b_f1_u003b((&param_14), (&param_15));
    D = _e254;
    let _e255 = NdotV_2;
    param_16 = _e255;
    let _e256 = NdotL_1;
    param_17 = _e256;
    let _e257 = roughness_3;
    param_18 = _e257;
    let _e258 = GeometrySmith_u0028_f1_u003b_f1_u003b_f1_u003b((&param_16), (&param_17), (&param_18));
    G = _e258;
    let _e259 = HdotV;
    param_19 = _e259;
    let _e260 = F0_1;
    param_20 = _e260;
    let _e261 = FresnelSchlick_u0028_f1_u003b_vf3_u003b((&param_19), (&param_20));
    F = _e261;
    let _e262 = D;
    let _e263 = G;
    let _e265 = F;
    let _e267 = NdotV_2;
    let _e269 = NdotL_1;
    specular = ((_e265 * (_e262 * _e263)) / vec3(max(((4f * _e267) * _e269), 0.001f)));
    let _e274 = F;
    let _e276 = metalness;
    kD = ((vec3<f32>(1f, 1f, 1f) - _e274) * (1f - _e276));
    let _e279 = kD;
    let _e280 = base;
    diffuseColor = ((_e279 * _e280.xyz) / vec3(3.1415927f));
    let _e285 = diffuseColor;
    let _e286 = specular;
    let _e288 = NdotL_1;
    let _e290 = intens;
    let _e291 = (((_e285 + _e286) * _e288) * _e290);
    let _e293 = base[3u];
    out_color = vec4<f32>(_e291.x, _e291.y, _e291.z, _e293);
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(2) L: vec4<f32>, @location(3) V: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    L_1 = L;
    V_1 = V;
    main_1();
    let _e11 = out_color;
    return _e11;
}
