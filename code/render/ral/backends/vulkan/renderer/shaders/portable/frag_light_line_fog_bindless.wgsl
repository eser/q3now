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

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e59 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e59 + 0.5f));
    let _e64 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e66 = fogType;
    let _e69 = fogType;
    return (((_e64 > 0.5f) && (_e66 >= 1i)) && (_e69 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e59 = wired_advanced_fog_enabled_u0028_();
    if !(_e59) {
        return 0f;
    }
    let _e62 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e62, 0.000001f));
    let _e67 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e67 + 0.5f));
    let _e70 = fogType_1;
    if (_e70 == 1i) {
        let _e74 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e74 <= 0f) {
            return 0f;
        }
        let _e76 = viewDepth;
        let _e79 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e76 / _e79), 0f, 1f);
    }
    let _e84 = unnamed.advancedFogColorDensity[3u];
    let _e86 = viewDepth;
    opticalDepth = (max(_e84, 0f) * _e86);
    let _e88 = fogType_1;
    if (_e88 == 2i) {
        let _e90 = opticalDepth;
        return clamp((1f - exp(-(_e90))), 0f, 1f);
    }
    let _e95 = opticalDepth;
    let _e96 = opticalDepth;
    return clamp((1f - exp(-((_e95 * _e96)))), 0f, 1f);
}

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e68 = unnamed.packed_indices[0i][0u];
    let _e74 = unnamed.packed_indices[0i][0u];
    let _e79 = textureDimensions(wired_bindless_images[(_e68 & 4095u)], 0i);
    ts = vec2<i32>(_e79);
    let _e82 = (*tc)[0u];
    let _e84 = ts[0u];
    let _e87 = dpdx((_e82 * f32(_e84)));
    dx = max(abs(_e87), 0.001f);
    let _e91 = (*tc)[1u];
    let _e93 = ts[1u];
    let _e96 = dpdy((_e91 * f32(_e93)));
    dy = max(abs(_e96), 0.001f);
    let _e99 = dx;
    let _e100 = dy;
    dxy = max(_e99, _e100);
    let _e102 = dxy;
    scale = max((1f / _e102), 1f);
    let _e105 = (*threshold);
    let _e106 = (*alpha);
    let _e107 = (*threshold);
    let _e109 = scale;
    ac = (_e105 + ((_e106 - _e107) * _e109));
    let _e112 = ac;
    return _e112;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e60 = (*c);
    (*c) = max(_e60, vec3<f32>(0f, 0f, 0f));
    let _e62 = (*c);
    cutoff = (_e62 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e64 = (*c);
    lo = (_e64 / vec3(12.92f));
    let _e67 = (*c);
    hi = pow(((_e67 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e72 = hi;
    let _e73 = lo;
    let _e74 = cutoff;
    return mix(_e72, _e73, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e74));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e61 = (*role);
    let _e63 = (*role);
    let _e68 = unnamed.packed_indices[(_e61 / 4u)][(_e63 % 4u)];
    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e83 = (*uv);
    let _e84 = textureSample(wired_bindless_images[(_e68 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    c_1 = _e84;
    let _e85 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e85))) != 0i) {
        let _e90 = c_1;
        return _e90;
    }
    let _e91 = c_1;
    param = _e91.xyz;
    let _e93 = sRGBToLinear_u0028_vf3_u003b((&param));
    c_1[0u] = _e93.x;
    c_1[1u] = _e93.y;
    c_1[2u] = _e93.z;
    let _e100 = c_1;
    return _e100;
}

fn main_1() {
    var fog: vec4<f32>;
    var parallax_tc: vec2<f32>;
    var Np: vec3<f32>;
    var base: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var param_4: f32;
    var param_5: f32;
    var param_6: vec2<f32>;
    var param_7: f32;
    var param_8: f32;
    var param_9: vec2<f32>;
    var lightColorRadius: vec4<f32>;
    var scale_1: f32;
    var LL: vec4<f32>;
    var nL: vec3<f32>;
    var nV: vec3<f32>;
    var intensFactor: f32;
    var intens: vec3<f32>;
    var fogAmount: f32;
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;

    let _e83 = unnamed.packed_indices[0i][1u];
    let _e89 = unnamed.packed_indices[0i][1u];
    let _e94 = fog_tex_coord_1;
    let _e95 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e89 >> bitcast<u32>(12i)) & 255u)], _e94);
    fog = _e95;
    let _e96 = frag_tex_coord_1;
    parallax_tc = _e96;
    let _e97 = N_1;
    Np = _e97;
    param_1 = 0u;
    let _e98 = parallax_tc;
    param_2 = _e98;
    param_3 = 0i;
    let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    base = _e99;
    if override_type_6_ {
        if override_type_6_1 {
            let _e101 = base[3u];
            base[3u] = select(0f, 1f, (_e101 > 0f));
        } else {
            if override_type_6_2 {
                let _e106 = base[3u];
                param_4 = alpha_test_value;
                param_5 = (1f - _e106);
                let _e108 = frag_tex_coord_1;
                param_6 = _e108;
                let _e109 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_4), (&param_5), (&param_6));
                base[3u] = _e109;
            } else {
                if override_type_6_3 {
                    param_7 = alpha_test_value;
                    let _e112 = base[3u];
                    param_8 = _e112;
                    let _e113 = frag_tex_coord_1;
                    param_9 = _e113;
                    let _e114 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_7), (&param_8), (&param_9));
                    base[3u] = _e114;
                }
            }
        }
    } else {
        if override_type_6_4 {
            let _e117 = base[3u];
            if (_e117 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_6_5 {
                let _e120 = base[3u];
                if (_e120 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_6_6 {
                    let _e123 = base[3u];
                    if (_e123 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e126 = unnamed.lightColor;
    lightColorRadius = _e126;
    let _e127 = L_1;
    let _e131 = unnamed.lightVector;
    let _e136 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e127.xyz), _e131.xyz) * _e136), 0f, 1f);
    let _e140 = unnamed.lightVector;
    let _e141 = scale_1;
    let _e143 = L_1;
    LL = ((_e140 * _e141) + _e143);
    let _e145 = LL;
    nL = normalize(_e145.xyz);
    let _e148 = V_1;
    nV = normalize(_e148.xyz);
    let _e151 = LL;
    let _e153 = LL;
    let _e157 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e151.xyz, _e153.xyz) * _e157));
    let _e160 = intensFactor;
    if (_e160 <= 0f) {
        discard;
    }
    let _e162 = lightColorRadius;
    let _e164 = intensFactor;
    intens = (_e162.xyz * _e164);
    let _e166 = wired_advanced_fog_amount_u0028_();
    fogAmount = _e166;
    let _e167 = wired_advanced_fog_enabled_u0028_();
    if !(_e167) {
        let _e170 = fog[3u];
        fogAmount = _e170;
    }
    let _e171 = fogAmount;
    let _e173 = base;
    let _e175 = (_e173.xyz * (1f - _e171));
    base[0u] = _e175.x;
    base[1u] = _e175.y;
    base[2u] = _e175.z;
    let _e182 = Np;
    let _e183 = nL;
    diffuse = dot(_e182, _e183);
    let _e185 = Np;
    let _e186 = nL;
    let _e187 = nV;
    specFactor = dot(_e185, normalize((_e186 + _e187)));
    if override_type_6_7 {
        let _e191 = diffuse;
        let _e192 = Np;
        let _e193 = nV;
        if ((_e191 * dot(_e192, _e193)) <= 0f) {
            discard;
        }
        let _e197 = diffuse;
        diffuse = abs(_e197);
        let _e199 = specFactor;
        specFactor = abs(_e199);
    } else {
        let _e201 = diffuse;
        diffuse = max(_e201, 0f);
        let _e203 = specFactor;
        specFactor = max(_e203, 0f);
    }
    let _e205 = specFactor;
    let _e209 = base;
    spec = ((vec4((pow(_e205, 10f) * 0.25f)) * _e209) * 0.8f);
    let _e212 = base;
    let _e213 = diffuse;
    let _e216 = spec;
    let _e218 = intens;
    out_color = (((_e212 * vec4(_e213)) + _e216) * vec4<f32>(_e218.x, _e218.y, _e218.z, 1f));
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
