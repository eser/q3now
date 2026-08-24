enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 30>,
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

    let _e79 = frag_tex_coord_1;
    parallax_tc = _e79;
    let _e80 = N_1;
    Np = _e80;
    param_1 = 0u;
    let _e81 = parallax_tc;
    param_2 = _e81;
    param_3 = 0i;
    let _e82 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    base = _e82;
    if override_type_6_ {
        if override_type_6_1 {
            let _e84 = base[3u];
            base[3u] = select(0f, 1f, (_e84 > 0f));
        } else {
            if override_type_6_2 {
                let _e89 = base[3u];
                param_4 = alpha_test_value;
                param_5 = (1f - _e89);
                let _e91 = frag_tex_coord_1;
                param_6 = _e91;
                let _e92 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_4), (&param_5), (&param_6));
                base[3u] = _e92;
            } else {
                if override_type_6_3 {
                    param_7 = alpha_test_value;
                    let _e95 = base[3u];
                    param_8 = _e95;
                    let _e96 = frag_tex_coord_1;
                    param_9 = _e96;
                    let _e97 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_7), (&param_8), (&param_9));
                    base[3u] = _e97;
                }
            }
        }
    } else {
        if override_type_6_4 {
            let _e100 = base[3u];
            if (_e100 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_6_5 {
                let _e103 = base[3u];
                if (_e103 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_6_6 {
                    let _e106 = base[3u];
                    if (_e106 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e109 = unnamed.lightColor;
    lightColorRadius = _e109;
    let _e110 = L_1;
    let _e114 = unnamed.lightVector;
    let _e119 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e110.xyz), _e114.xyz) * _e119), 0f, 1f);
    let _e123 = unnamed.lightVector;
    let _e124 = scale_1;
    let _e126 = L_1;
    LL = ((_e123 * _e124) + _e126);
    let _e128 = LL;
    nL = normalize(_e128.xyz);
    let _e131 = V_1;
    nV = normalize(_e131.xyz);
    let _e134 = LL;
    let _e136 = LL;
    let _e140 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e134.xyz, _e136.xyz) * _e140));
    let _e143 = intensFactor;
    if (_e143 <= 0f) {
        discard;
    }
    let _e145 = lightColorRadius;
    let _e147 = intensFactor;
    intens = (_e145.xyz * _e147);
    let _e149 = wired_advanced_fog_amount_u0028_();
    fogAmount = _e149;
    let _e150 = fogAmount;
    let _e152 = base;
    let _e154 = (_e152.xyz * (1f - _e150));
    base[0u] = _e154.x;
    base[1u] = _e154.y;
    base[2u] = _e154.z;
    let _e161 = Np;
    let _e162 = nL;
    diffuse = dot(_e161, _e162);
    let _e164 = Np;
    let _e165 = nL;
    let _e166 = nV;
    specFactor = dot(_e164, normalize((_e165 + _e166)));
    if override_type_6_7 {
        let _e170 = diffuse;
        let _e171 = Np;
        let _e172 = nV;
        if ((_e170 * dot(_e171, _e172)) <= 0f) {
            discard;
        }
        let _e176 = diffuse;
        diffuse = abs(_e176);
        let _e178 = specFactor;
        specFactor = abs(_e178);
    } else {
        let _e180 = diffuse;
        diffuse = max(_e180, 0f);
        let _e182 = specFactor;
        specFactor = max(_e182, 0f);
    }
    let _e184 = specFactor;
    let _e188 = base;
    spec = ((vec4((pow(_e184, 10f) * 0.25f)) * _e188) * 0.8f);
    let _e191 = base;
    let _e192 = diffuse;
    let _e195 = spec;
    let _e197 = intens;
    out_color = (((_e191 * vec4(_e192)) + _e195) * vec4<f32>(_e197.x, _e197.y, _e197.z, 1f));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(2) L: vec4<f32>, @location(3) V: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    L_1 = L;
    V_1 = V;
    main_1();
    let _e11 = out_color;
    return _e11;
}
