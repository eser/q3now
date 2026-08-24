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
    _pad_worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(3) override alpha_to_coverage: i32 = 0i;
override override_type_6_: bool = (alpha_to_coverage != 0i);
@id(0) override alpha_test_func: i32 = 0i;
override override_type_6_1: bool = (alpha_test_func == 1i);
override override_type_6_2: bool = (alpha_test_func == 2i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_6_3: bool = (alpha_test_func == 3i);
override override_type_6_4: bool = (alpha_test_func == 1i);
override override_type_6_5: bool = (alpha_test_func == 2i);
override override_type_6_6: bool = (alpha_test_func == 3i);
@id(5) override abs_light: i32 = 0i;
override override_type_6_7: bool = (abs_light != 0i);

@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> N_1: vec3<f32>;
var<private> L_1: vec4<f32>;
var<private> V_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn FresnelSchlick_u0028_f1_u003b_vf3_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;

    let _e62 = (*cosTheta);
    t = (1f - _e62);
    let _e64 = t;
    let _e65 = t;
    t2_ = (_e64 * _e65);
    let _e67 = (*F0_);
    let _e68 = (*F0_);
    let _e71 = t2_;
    let _e72 = t2_;
    let _e74 = t;
    return (_e67 + ((vec3(1f) - _e68) * ((_e71 * _e72) * _e74)));
}

fn GeometrySchlickGGX_u0028_f1_u003b_f1_u003b(NdotV: ptr<function, f32>, roughness: ptr<function, f32>) -> f32 {
    var r: f32;
    var k: f32;

    let _e62 = (*roughness);
    r = (_e62 + 1f);
    let _e64 = r;
    let _e65 = r;
    k = ((_e64 * _e65) / 8f);
    let _e68 = (*NdotV);
    let _e69 = (*NdotV);
    let _e70 = k;
    let _e73 = k;
    return (_e68 / ((_e69 * (1f - _e70)) + _e73));
}

fn GeometrySmith_u0028_f1_u003b_f1_u003b_f1_u003b(NdotV_1: ptr<function, f32>, NdotL: ptr<function, f32>, roughness_1: ptr<function, f32>) -> f32 {
    var param: f32;
    var param_1: f32;
    var param_2: f32;
    var param_3: f32;

    let _e65 = (*NdotV_1);
    param = _e65;
    let _e66 = (*roughness_1);
    param_1 = _e66;
    let _e67 = GeometrySchlickGGX_u0028_f1_u003b_f1_u003b((&param), (&param_1));
    let _e68 = (*NdotL);
    param_2 = _e68;
    let _e69 = (*roughness_1);
    param_3 = _e69;
    let _e70 = GeometrySchlickGGX_u0028_f1_u003b_f1_u003b((&param_2), (&param_3));
    return (_e67 * _e70);
}

fn DistributionGGX_u0028_f1_u003b_f1_u003b(NdotH: ptr<function, f32>, roughness_2: ptr<function, f32>) -> f32 {
    var a: f32;
    var a2_: f32;
    var denom: f32;

    let _e63 = (*roughness_2);
    let _e64 = (*roughness_2);
    a = (_e63 * _e64);
    let _e66 = a;
    let _e67 = a;
    a2_ = (_e66 * _e67);
    let _e69 = (*NdotH);
    let _e70 = (*NdotH);
    let _e72 = a2_;
    denom = (((_e69 * _e70) * (_e72 - 1f)) + 1f);
    let _e76 = a2_;
    let _e77 = denom;
    let _e79 = denom;
    return (_e76 / ((3.1415927f * _e77) * _e79));
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e61 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e61 + 0.5f));
    let _e66 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e68 = fogType;
    let _e71 = fogType;
    return (((_e66 > 0.5f) && (_e68 >= 1i)) && (_e71 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e61 = wired_advanced_fog_enabled_u0028_();
    if !(_e61) {
        return 0f;
    }
    let _e64 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e64, 0.000001f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e69 + 0.5f));
    let _e72 = fogType_1;
    if (_e72 == 1i) {
        let _e76 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e76 <= 0f) {
            return 0f;
        }
        let _e78 = viewDepth;
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e78 / _e81), 0f, 1f);
    }
    let _e86 = unnamed.advancedFogColorDensity[3u];
    let _e88 = viewDepth;
    opticalDepth = (max(_e86, 0f) * _e88);
    let _e90 = fogType_1;
    if (_e90 == 2i) {
        let _e92 = opticalDepth;
        return clamp((1f - exp(-(_e92))), 0f, 1f);
    }
    let _e97 = opticalDepth;
    let _e98 = opticalDepth;
    return clamp((1f - exp(-((_e97 * _e98)))), 0f, 1f);
}

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e70 = unnamed.packed_indices[0i][0u];
    let _e76 = unnamed.packed_indices[0i][0u];
    let _e81 = textureDimensions(wired_bindless_images[(_e70 & 4095u)], 0i);
    ts = vec2<i32>(_e81);
    let _e84 = (*tc)[0u];
    let _e86 = ts[0u];
    let _e89 = dpdx((_e84 * f32(_e86)));
    dx = max(abs(_e89), 0.001f);
    let _e93 = (*tc)[1u];
    let _e95 = ts[1u];
    let _e98 = dpdy((_e93 * f32(_e95)));
    dy = max(abs(_e98), 0.001f);
    let _e101 = dx;
    let _e102 = dy;
    dxy = max(_e101, _e102);
    let _e104 = dxy;
    scale = max((1f / _e104), 1f);
    let _e107 = (*threshold);
    let _e108 = (*alpha);
    let _e109 = (*threshold);
    let _e111 = scale;
    ac = (_e107 + ((_e108 - _e109) * _e111));
    let _e114 = ac;
    return _e114;
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
    var param_4: vec3<f32>;

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
    if ((tex_domain & (1i << bitcast<u32>(_e87))) != 0i) {
        let _e92 = c_1;
        return _e92;
    }
    let _e93 = c_1;
    param_4 = _e93.xyz;
    let _e95 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    c_1[0u] = _e95.x;
    c_1[1u] = _e95.y;
    c_1[2u] = _e95.z;
    let _e102 = c_1;
    return _e102;
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
    var fogAmount: f32;
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

    let _e105 = unnamed.packed_indices[0i][1u];
    let _e111 = unnamed.packed_indices[0i][1u];
    let _e116 = fog_tex_coord_1;
    let _e117 = textureSample(wired_bindless_images[(_e105 & 4095u)], wired_bindless_samplers[((_e111 >> bitcast<u32>(12i)) & 255u)], _e116);
    fog = _e117;
    let _e118 = frag_tex_coord_1;
    parallax_tc = _e118;
    let _e119 = N_1;
    Np = _e119;
    param_5 = 0u;
    let _e120 = parallax_tc;
    param_6 = _e120;
    param_7 = 0i;
    let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    base = _e121;
    if override_type_6_ {
        if override_type_6_1 {
            let _e123 = base[3u];
            base[3u] = select(0f, 1f, (_e123 > 0f));
        } else {
            if override_type_6_2 {
                let _e128 = base[3u];
                param_8 = alpha_test_value;
                param_9 = (1f - _e128);
                let _e130 = frag_tex_coord_1;
                param_10 = _e130;
                let _e131 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_8), (&param_9), (&param_10));
                base[3u] = _e131;
            } else {
                if override_type_6_3 {
                    param_11 = alpha_test_value;
                    let _e134 = base[3u];
                    param_12 = _e134;
                    let _e135 = frag_tex_coord_1;
                    param_13 = _e135;
                    let _e136 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_11), (&param_12), (&param_13));
                    base[3u] = _e136;
                }
            }
        }
    } else {
        if override_type_6_4 {
            let _e139 = base[3u];
            if (_e139 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_6_5 {
                let _e142 = base[3u];
                if (_e142 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_6_6 {
                    let _e145 = base[3u];
                    if (_e145 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e148 = unnamed.lightColor;
    lightColorRadius = _e148;
    let _e149 = L_1;
    let _e153 = unnamed.lightVector;
    let _e158 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e149.xyz), _e153.xyz) * _e158), 0f, 1f);
    let _e162 = unnamed.lightVector;
    let _e163 = scale_1;
    let _e165 = L_1;
    LL = ((_e162 * _e163) + _e165);
    let _e167 = LL;
    nL = normalize(_e167.xyz);
    let _e170 = V_1;
    nV = normalize(_e170.xyz);
    let _e173 = LL;
    let _e175 = LL;
    let _e179 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e173.xyz, _e175.xyz) * _e179));
    let _e182 = intensFactor;
    if (_e182 <= 0f) {
        discard;
    }
    let _e184 = lightColorRadius;
    let _e186 = intensFactor;
    intens = (_e184.xyz * _e186);
    let _e188 = wired_advanced_fog_amount_u0028_();
    fogAmount = _e188;
    let _e189 = wired_advanced_fog_enabled_u0028_();
    if !(_e189) {
        let _e192 = fog[3u];
        fogAmount = _e192;
    }
    let _e193 = fogAmount;
    let _e195 = base;
    let _e197 = (_e195.xyz * (1f - _e193));
    base[0u] = _e197.x;
    base[1u] = _e197.y;
    base[2u] = _e197.z;
    let _e207 = unnamed.packed_indices[0i][3u];
    let _e213 = unnamed.packed_indices[0i][3u];
    let _e218 = parallax_tc;
    let _e219 = textureSample(wired_bindless_images[(_e207 & 4095u)], wired_bindless_samplers[((_e213 >> bitcast<u32>(12i)) & 255u)], _e218);
    pbr = _e219.xyz;
    let _e222 = pbr[0u];
    ao = _e222;
    let _e224 = pbr[1u];
    roughness_3 = clamp(_e224, 0.04f, 1f);
    let _e227 = pbr[2u];
    metalness = _e227;
    let _e228 = nL;
    let _e229 = nV;
    halfVec = normalize((_e228 + _e229));
    let _e232 = Np;
    let _e233 = nL;
    NdotL_1 = max(dot(_e232, _e233), 0f);
    let _e236 = Np;
    let _e237 = nV;
    NdotV_2 = max(dot(_e236, _e237), 0.001f);
    let _e240 = Np;
    let _e241 = halfVec;
    NdotH_1 = max(dot(_e240, _e241), 0f);
    let _e244 = halfVec;
    let _e245 = nV;
    HdotV = max(dot(_e244, _e245), 0f);
    if override_type_6_7 {
        let _e248 = NdotL_1;
        let _e249 = NdotV_2;
        if ((_e248 * _e249) <= 0f) {
            discard;
        }
        let _e252 = NdotL_1;
        NdotL_1 = abs(_e252);
        let _e254 = NdotV_2;
        NdotV_2 = abs(_e254);
    }
    let _e256 = base;
    let _e258 = metalness;
    F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e256.xyz, vec3(_e258));
    let _e261 = NdotH_1;
    param_14 = _e261;
    let _e262 = roughness_3;
    param_15 = _e262;
    let _e263 = DistributionGGX_u0028_f1_u003b_f1_u003b((&param_14), (&param_15));
    D = _e263;
    let _e264 = NdotV_2;
    param_16 = _e264;
    let _e265 = NdotL_1;
    param_17 = _e265;
    let _e266 = roughness_3;
    param_18 = _e266;
    let _e267 = GeometrySmith_u0028_f1_u003b_f1_u003b_f1_u003b((&param_16), (&param_17), (&param_18));
    G = _e267;
    let _e268 = HdotV;
    param_19 = _e268;
    let _e269 = F0_1;
    param_20 = _e269;
    let _e270 = FresnelSchlick_u0028_f1_u003b_vf3_u003b((&param_19), (&param_20));
    F = _e270;
    let _e271 = D;
    let _e272 = G;
    let _e274 = F;
    let _e276 = NdotV_2;
    let _e278 = NdotL_1;
    specular = ((_e274 * (_e271 * _e272)) / vec3(max(((4f * _e276) * _e278), 0.001f)));
    let _e283 = F;
    let _e285 = metalness;
    kD = ((vec3<f32>(1f, 1f, 1f) - _e283) * (1f - _e285));
    let _e288 = kD;
    let _e289 = base;
    diffuseColor = ((_e288 * _e289.xyz) / vec3(3.1415927f));
    let _e294 = diffuseColor;
    let _e295 = specular;
    let _e297 = NdotL_1;
    let _e299 = intens;
    let _e300 = (((_e294 + _e295) * _e297) * _e299);
    let _e302 = base[3u];
    out_color = vec4<f32>(_e300.x, _e300.y, _e300.z, _e302);
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(2) L: vec4<f32>, @location(3) V: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    L_1 = L;
    V_1 = V;
    main_1();
    let _e13 = out_color;
    return _e13;
}
