enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 30>,
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
var<private> frag_tex_coord_1: vec2<f32>;
var<private> N_1: vec3<f32>;
var<private> L_1: vec4<f32>;
var<private> V_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e63 = unnamed.packed_indices[0i][0u];
    let _e69 = unnamed.packed_indices[0i][0u];
    let _e74 = textureDimensions(wired_bindless_images[(_e63 & 4095u)], 0i);
    ts = vec2<i32>(_e74);
    let _e77 = (*tc)[0u];
    let _e79 = ts[0u];
    let _e82 = dpdx((_e77 * f32(_e79)));
    dx = max(abs(_e82), 0.001f);
    let _e86 = (*tc)[1u];
    let _e88 = ts[1u];
    let _e91 = dpdy((_e86 * f32(_e88)));
    dy = max(abs(_e91), 0.001f);
    let _e94 = dx;
    let _e95 = dy;
    dxy = max(_e94, _e95);
    let _e97 = dxy;
    scale = max((1f / _e97), 1f);
    let _e100 = (*threshold);
    let _e101 = (*alpha);
    let _e102 = (*threshold);
    let _e104 = scale;
    ac = (_e100 + ((_e101 - _e102) * _e104));
    let _e107 = ac;
    return _e107;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e55 = (*c);
    (*c) = max(_e55, vec3<f32>(0f, 0f, 0f));
    let _e57 = (*c);
    cutoff = (_e57 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e59 = (*c);
    lo = (_e59 / vec3(12.92f));
    let _e62 = (*c);
    hi = pow(((_e62 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e67 = hi;
    let _e68 = lo;
    let _e69 = cutoff;
    return mix(_e67, _e68, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e69));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e56 = (*role);
    let _e58 = (*role);
    let _e63 = unnamed.packed_indices[(_e56 / 4u)][(_e58 % 4u)];
    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e78 = (*uv);
    let _e79 = textureSample(wired_bindless_images[(_e63 & 4095u)], wired_bindless_samplers[((_e73 >> bitcast<u32>(12i)) & 255u)], _e78);
    c_1 = _e79;
    let _e80 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e80))) != 0i) {
        let _e85 = c_1;
        return _e85;
    }
    let _e86 = c_1;
    param = _e86.xyz;
    let _e88 = sRGBToLinear_u0028_vf3_u003b((&param));
    c_1[0u] = _e88.x;
    c_1[1u] = _e88.y;
    c_1[2u] = _e88.z;
    let _e95 = c_1;
    return _e95;
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
    var nL: vec3<f32>;
    var nV: vec3<f32>;
    var intensFactor: f32;
    var intens: vec3<f32>;
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;

    let _e71 = frag_tex_coord_1;
    parallax_tc = _e71;
    let _e72 = N_1;
    Np = _e72;
    param_1 = 0u;
    let _e73 = parallax_tc;
    param_2 = _e73;
    param_3 = 0i;
    let _e74 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    base = _e74;
    if override_type_22_ {
        if override_type_22_1 {
            let _e76 = base[3u];
            base[3u] = select(0f, 1f, (_e76 > 0f));
        } else {
            if override_type_22_2 {
                let _e81 = base[3u];
                param_4 = alpha_test_value;
                param_5 = (1f - _e81);
                let _e83 = frag_tex_coord_1;
                param_6 = _e83;
                let _e84 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_4), (&param_5), (&param_6));
                base[3u] = _e84;
            } else {
                if override_type_22_3 {
                    param_7 = alpha_test_value;
                    let _e87 = base[3u];
                    param_8 = _e87;
                    let _e88 = frag_tex_coord_1;
                    param_9 = _e88;
                    let _e89 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_7), (&param_8), (&param_9));
                    base[3u] = _e89;
                }
            }
        }
    } else {
        if override_type_22_4 {
            let _e92 = base[3u];
            if (_e92 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_22_5 {
                let _e95 = base[3u];
                if (_e95 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_22_6 {
                    let _e98 = base[3u];
                    if (_e98 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e101 = unnamed.lightColor;
    lightColorRadius = _e101;
    let _e102 = L_1;
    nL = normalize(_e102.xyz);
    let _e105 = V_1;
    nV = normalize(_e105.xyz);
    let _e108 = L_1;
    let _e110 = L_1;
    let _e114 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e108.xyz, _e110.xyz) * _e114));
    let _e117 = intensFactor;
    if (_e117 <= 0f) {
        discard;
    }
    let _e119 = lightColorRadius;
    let _e121 = intensFactor;
    intens = (_e119.xyz * _e121);
    let _e123 = Np;
    let _e124 = nL;
    diffuse = dot(_e123, _e124);
    let _e126 = Np;
    let _e127 = nL;
    let _e128 = nV;
    specFactor = dot(_e126, normalize((_e127 + _e128)));
    if override_type_22_7 {
        let _e132 = diffuse;
        let _e133 = Np;
        let _e134 = nV;
        if ((_e132 * dot(_e133, _e134)) <= 0f) {
            discard;
        }
        let _e138 = diffuse;
        diffuse = abs(_e138);
        let _e140 = specFactor;
        specFactor = abs(_e140);
    } else {
        let _e142 = diffuse;
        diffuse = max(_e142, 0f);
        let _e144 = specFactor;
        specFactor = max(_e144, 0f);
    }
    let _e146 = specFactor;
    let _e150 = base;
    spec = ((vec4((pow(_e146, 10f) * 0.25f)) * _e150) * 0.8f);
    let _e153 = base;
    let _e154 = diffuse;
    let _e157 = spec;
    let _e159 = intens;
    out_color = (((_e153 * vec4(_e154)) + _e157) * vec4<f32>(_e159.x, _e159.y, _e159.z, 1f));
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(2) L: vec4<f32>, @location(3) V: vec4<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    L_1 = L;
    V_1 = V;
    main_1();
    let _e9 = out_color;
    return _e9;
}
