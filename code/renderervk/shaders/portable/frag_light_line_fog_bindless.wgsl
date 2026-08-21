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

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e64 = unnamed.packed_indices[0i][0u];
    let _e70 = unnamed.packed_indices[0i][0u];
    let _e75 = textureDimensions(wired_bindless_images[(_e64 & 4095u)], 0i);
    ts = vec2<i32>(_e75);
    let _e78 = (*tc)[0u];
    let _e80 = ts[0u];
    let _e83 = dpdx((_e78 * f32(_e80)));
    dx = max(abs(_e83), 0.001f);
    let _e87 = (*tc)[1u];
    let _e89 = ts[1u];
    let _e92 = dpdy((_e87 * f32(_e89)));
    dy = max(abs(_e92), 0.001f);
    let _e95 = dx;
    let _e96 = dy;
    dxy = max(_e95, _e96);
    let _e98 = dxy;
    scale = max((1f / _e98), 1f);
    let _e101 = (*threshold);
    let _e102 = (*alpha);
    let _e103 = (*threshold);
    let _e105 = scale;
    ac = (_e101 + ((_e102 - _e103) * _e105));
    let _e108 = ac;
    return _e108;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e56 = (*c);
    (*c) = max(_e56, vec3<f32>(0f, 0f, 0f));
    let _e58 = (*c);
    cutoff = (_e58 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e60 = (*c);
    lo = (_e60 / vec3(12.92f));
    let _e63 = (*c);
    hi = pow(((_e63 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e68 = hi;
    let _e69 = lo;
    let _e70 = cutoff;
    return mix(_e68, _e69, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e70));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e57 = (*role);
    let _e59 = (*role);
    let _e64 = unnamed.packed_indices[(_e57 / 4u)][(_e59 % 4u)];
    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e79 = (*uv);
    let _e80 = textureSample(wired_bindless_images[(_e64 & 4095u)], wired_bindless_samplers[((_e74 >> bitcast<u32>(12i)) & 255u)], _e79);
    c_1 = _e80;
    let _e81 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e81))) != 0i) {
        let _e86 = c_1;
        return _e86;
    }
    let _e87 = c_1;
    param = _e87.xyz;
    let _e89 = sRGBToLinear_u0028_vf3_u003b((&param));
    c_1[0u] = _e89.x;
    c_1[1u] = _e89.y;
    c_1[2u] = _e89.z;
    let _e96 = c_1;
    return _e96;
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
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;

    let _e78 = unnamed.packed_indices[0i][1u];
    let _e84 = unnamed.packed_indices[0i][1u];
    let _e89 = fog_tex_coord_1;
    let _e90 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    fog = _e90;
    let _e91 = frag_tex_coord_1;
    parallax_tc = _e91;
    let _e92 = N_1;
    Np = _e92;
    param_1 = 0u;
    let _e93 = parallax_tc;
    param_2 = _e93;
    param_3 = 0i;
    let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    base = _e94;
    if override_type_22_ {
        if override_type_22_1 {
            let _e96 = base[3u];
            base[3u] = select(0f, 1f, (_e96 > 0f));
        } else {
            if override_type_22_2 {
                let _e101 = base[3u];
                param_4 = alpha_test_value;
                param_5 = (1f - _e101);
                let _e103 = frag_tex_coord_1;
                param_6 = _e103;
                let _e104 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_4), (&param_5), (&param_6));
                base[3u] = _e104;
            } else {
                if override_type_22_3 {
                    param_7 = alpha_test_value;
                    let _e107 = base[3u];
                    param_8 = _e107;
                    let _e108 = frag_tex_coord_1;
                    param_9 = _e108;
                    let _e109 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_7), (&param_8), (&param_9));
                    base[3u] = _e109;
                }
            }
        }
    } else {
        if override_type_22_4 {
            let _e112 = base[3u];
            if (_e112 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_22_5 {
                let _e115 = base[3u];
                if (_e115 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_22_6 {
                    let _e118 = base[3u];
                    if (_e118 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e121 = unnamed.lightColor;
    lightColorRadius = _e121;
    let _e122 = L_1;
    let _e126 = unnamed.lightVector;
    let _e131 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e122.xyz), _e126.xyz) * _e131), 0f, 1f);
    let _e135 = unnamed.lightVector;
    let _e136 = scale_1;
    let _e138 = L_1;
    LL = ((_e135 * _e136) + _e138);
    let _e140 = LL;
    nL = normalize(_e140.xyz);
    let _e143 = V_1;
    nV = normalize(_e143.xyz);
    let _e146 = LL;
    let _e148 = LL;
    let _e152 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e146.xyz, _e148.xyz) * _e152));
    let _e155 = intensFactor;
    if (_e155 <= 0f) {
        discard;
    }
    let _e157 = lightColorRadius;
    let _e159 = intensFactor;
    intens = (_e157.xyz * _e159);
    let _e161 = base;
    let _e164 = fog[3u];
    let _e166 = (_e161.xyz * (1f - _e164));
    base[0u] = _e166.x;
    base[1u] = _e166.y;
    base[2u] = _e166.z;
    let _e173 = Np;
    let _e174 = nL;
    diffuse = dot(_e173, _e174);
    let _e176 = Np;
    let _e177 = nL;
    let _e178 = nV;
    specFactor = dot(_e176, normalize((_e177 + _e178)));
    if override_type_22_7 {
        let _e182 = diffuse;
        let _e183 = Np;
        let _e184 = nV;
        if ((_e182 * dot(_e183, _e184)) <= 0f) {
            discard;
        }
        let _e188 = diffuse;
        diffuse = abs(_e188);
        let _e190 = specFactor;
        specFactor = abs(_e190);
    } else {
        let _e192 = diffuse;
        diffuse = max(_e192, 0f);
        let _e194 = specFactor;
        specFactor = max(_e194, 0f);
    }
    let _e196 = specFactor;
    let _e200 = base;
    spec = ((vec4((pow(_e196, 10f) * 0.25f)) * _e200) * 0.8f);
    let _e203 = base;
    let _e204 = diffuse;
    let _e207 = spec;
    let _e209 = intens;
    out_color = (((_e203 * vec4(_e204)) + _e207) * vec4<f32>(_e209.x, _e209.y, _e209.z, 1f));
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
