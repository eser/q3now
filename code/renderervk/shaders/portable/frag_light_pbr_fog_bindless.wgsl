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

    let _e98 = unnamed.packed_indices[0i][1u];
    let _e104 = unnamed.packed_indices[0i][1u];
    let _e109 = fog_tex_coord_1;
    let _e110 = textureSample(wired_bindless_images[(_e98 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    fog = _e110;
    let _e111 = frag_tex_coord_1;
    parallax_tc = _e111;
    let _e112 = N_1;
    Np = _e112;
    param_5 = 0u;
    let _e113 = parallax_tc;
    param_6 = _e113;
    param_7 = 0i;
    let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    base = _e114;
    if override_type_22_ {
        if override_type_22_1 {
            let _e116 = base[3u];
            base[3u] = select(0f, 1f, (_e116 > 0f));
        } else {
            if override_type_22_2 {
                let _e121 = base[3u];
                param_8 = alpha_test_value;
                param_9 = (1f - _e121);
                let _e123 = frag_tex_coord_1;
                param_10 = _e123;
                let _e124 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_8), (&param_9), (&param_10));
                base[3u] = _e124;
            } else {
                if override_type_22_3 {
                    param_11 = alpha_test_value;
                    let _e127 = base[3u];
                    param_12 = _e127;
                    let _e128 = frag_tex_coord_1;
                    param_13 = _e128;
                    let _e129 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_11), (&param_12), (&param_13));
                    base[3u] = _e129;
                }
            }
        }
    } else {
        if override_type_22_4 {
            let _e132 = base[3u];
            if (_e132 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_22_5 {
                let _e135 = base[3u];
                if (_e135 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_22_6 {
                    let _e138 = base[3u];
                    if (_e138 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e141 = unnamed.lightColor;
    lightColorRadius = _e141;
    let _e142 = L_1;
    nL = normalize(_e142.xyz);
    let _e145 = V_1;
    nV = normalize(_e145.xyz);
    let _e148 = L_1;
    let _e150 = L_1;
    let _e154 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e148.xyz, _e150.xyz) * _e154));
    let _e157 = intensFactor;
    if (_e157 <= 0f) {
        discard;
    }
    let _e159 = lightColorRadius;
    let _e161 = intensFactor;
    intens = (_e159.xyz * _e161);
    let _e163 = base;
    let _e166 = fog[3u];
    let _e168 = (_e163.xyz * (1f - _e166));
    base[0u] = _e168.x;
    base[1u] = _e168.y;
    base[2u] = _e168.z;
    let _e178 = unnamed.packed_indices[0i][3u];
    let _e184 = unnamed.packed_indices[0i][3u];
    let _e189 = parallax_tc;
    let _e190 = textureSample(wired_bindless_images[(_e178 & 4095u)], wired_bindless_samplers[((_e184 >> bitcast<u32>(12i)) & 255u)], _e189);
    pbr = _e190.xyz;
    let _e193 = pbr[0u];
    ao = _e193;
    let _e195 = pbr[1u];
    roughness_3 = clamp(_e195, 0.04f, 1f);
    let _e198 = pbr[2u];
    metalness = _e198;
    let _e199 = nL;
    let _e200 = nV;
    halfVec = normalize((_e199 + _e200));
    let _e203 = Np;
    let _e204 = nL;
    NdotL_1 = max(dot(_e203, _e204), 0f);
    let _e207 = Np;
    let _e208 = nV;
    NdotV_2 = max(dot(_e207, _e208), 0.001f);
    let _e211 = Np;
    let _e212 = halfVec;
    NdotH_1 = max(dot(_e211, _e212), 0f);
    let _e215 = halfVec;
    let _e216 = nV;
    HdotV = max(dot(_e215, _e216), 0f);
    if override_type_22_7 {
        let _e219 = NdotL_1;
        let _e220 = NdotV_2;
        if ((_e219 * _e220) <= 0f) {
            discard;
        }
        let _e223 = NdotL_1;
        NdotL_1 = abs(_e223);
        let _e225 = NdotV_2;
        NdotV_2 = abs(_e225);
    }
    let _e227 = base;
    let _e229 = metalness;
    F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e227.xyz, vec3(_e229));
    let _e232 = NdotH_1;
    param_14 = _e232;
    let _e233 = roughness_3;
    param_15 = _e233;
    let _e234 = DistributionGGX_u0028_f1_u003b_f1_u003b((&param_14), (&param_15));
    D = _e234;
    let _e235 = NdotV_2;
    param_16 = _e235;
    let _e236 = NdotL_1;
    param_17 = _e236;
    let _e237 = roughness_3;
    param_18 = _e237;
    let _e238 = GeometrySmith_u0028_f1_u003b_f1_u003b_f1_u003b((&param_16), (&param_17), (&param_18));
    G = _e238;
    let _e239 = HdotV;
    param_19 = _e239;
    let _e240 = F0_1;
    param_20 = _e240;
    let _e241 = FresnelSchlick_u0028_f1_u003b_vf3_u003b((&param_19), (&param_20));
    F = _e241;
    let _e242 = D;
    let _e243 = G;
    let _e245 = F;
    let _e247 = NdotV_2;
    let _e249 = NdotL_1;
    specular = ((_e245 * (_e242 * _e243)) / vec3(max(((4f * _e247) * _e249), 0.001f)));
    let _e254 = F;
    let _e256 = metalness;
    kD = ((vec3<f32>(1f, 1f, 1f) - _e254) * (1f - _e256));
    let _e259 = kD;
    let _e260 = base;
    diffuseColor = ((_e259 * _e260.xyz) / vec3(3.1415927f));
    let _e265 = diffuseColor;
    let _e266 = specular;
    let _e268 = NdotL_1;
    let _e270 = intens;
    let _e271 = (((_e265 + _e266) * _e268) * _e270);
    let _e273 = base[3u];
    out_color = vec4<f32>(_e271.x, _e271.y, _e271.z, _e273);
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
