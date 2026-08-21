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

fn FresnelSchlick_u0028_f1_u003b_vf3_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;

    let _e57 = (*cosTheta);
    t = (1f - _e57);
    let _e59 = t;
    let _e60 = t;
    t2_ = (_e59 * _e60);
    let _e62 = (*F0_);
    let _e63 = (*F0_);
    let _e66 = t2_;
    let _e67 = t2_;
    let _e69 = t;
    return (_e62 + ((vec3(1f) - _e63) * ((_e66 * _e67) * _e69)));
}

fn GeometrySchlickGGX_u0028_f1_u003b_f1_u003b(NdotV: ptr<function, f32>, roughness: ptr<function, f32>) -> f32 {
    var r: f32;
    var k: f32;

    let _e57 = (*roughness);
    r = (_e57 + 1f);
    let _e59 = r;
    let _e60 = r;
    k = ((_e59 * _e60) / 8f);
    let _e63 = (*NdotV);
    let _e64 = (*NdotV);
    let _e65 = k;
    let _e68 = k;
    return (_e63 / ((_e64 * (1f - _e65)) + _e68));
}

fn GeometrySmith_u0028_f1_u003b_f1_u003b_f1_u003b(NdotV_1: ptr<function, f32>, NdotL: ptr<function, f32>, roughness_1: ptr<function, f32>) -> f32 {
    var param: f32;
    var param_1: f32;
    var param_2: f32;
    var param_3: f32;

    let _e60 = (*NdotV_1);
    param = _e60;
    let _e61 = (*roughness_1);
    param_1 = _e61;
    let _e62 = GeometrySchlickGGX_u0028_f1_u003b_f1_u003b((&param), (&param_1));
    let _e63 = (*NdotL);
    param_2 = _e63;
    let _e64 = (*roughness_1);
    param_3 = _e64;
    let _e65 = GeometrySchlickGGX_u0028_f1_u003b_f1_u003b((&param_2), (&param_3));
    return (_e62 * _e65);
}

fn DistributionGGX_u0028_f1_u003b_f1_u003b(NdotH: ptr<function, f32>, roughness_2: ptr<function, f32>) -> f32 {
    var a: f32;
    var a2_: f32;
    var denom: f32;

    let _e58 = (*roughness_2);
    let _e59 = (*roughness_2);
    a = (_e58 * _e59);
    let _e61 = a;
    let _e62 = a;
    a2_ = (_e61 * _e62);
    let _e64 = (*NdotH);
    let _e65 = (*NdotH);
    let _e67 = a2_;
    denom = (((_e64 * _e65) * (_e67 - 1f)) + 1f);
    let _e71 = a2_;
    let _e72 = denom;
    let _e74 = denom;
    return (_e71 / ((3.1415927f * _e72) * _e74));
}

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e65 = unnamed.packed_indices[0i][0u];
    let _e71 = unnamed.packed_indices[0i][0u];
    let _e76 = textureDimensions(wired_bindless_images[(_e65 & 4095u)], 0i);
    ts = vec2<i32>(_e76);
    let _e79 = (*tc)[0u];
    let _e81 = ts[0u];
    let _e84 = dpdx((_e79 * f32(_e81)));
    dx = max(abs(_e84), 0.001f);
    let _e88 = (*tc)[1u];
    let _e90 = ts[1u];
    let _e93 = dpdy((_e88 * f32(_e90)));
    dy = max(abs(_e93), 0.001f);
    let _e96 = dx;
    let _e97 = dy;
    dxy = max(_e96, _e97);
    let _e99 = dxy;
    scale = max((1f / _e99), 1f);
    let _e102 = (*threshold);
    let _e103 = (*alpha);
    let _e104 = (*threshold);
    let _e106 = scale;
    ac = (_e102 + ((_e103 - _e104) * _e106));
    let _e109 = ac;
    return _e109;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e57 = (*c);
    (*c) = max(_e57, vec3<f32>(0f, 0f, 0f));
    let _e59 = (*c);
    cutoff = (_e59 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e61 = (*c);
    lo = (_e61 / vec3(12.92f));
    let _e64 = (*c);
    hi = pow(((_e64 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e69 = hi;
    let _e70 = lo;
    let _e71 = cutoff;
    return mix(_e69, _e70, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e71));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_4: vec3<f32>;

    let _e58 = (*role);
    let _e60 = (*role);
    let _e65 = unnamed.packed_indices[(_e58 / 4u)][(_e60 % 4u)];
    let _e68 = (*role);
    let _e70 = (*role);
    let _e75 = unnamed.packed_indices[(_e68 / 4u)][(_e70 % 4u)];
    let _e80 = (*uv);
    let _e81 = textureSample(wired_bindless_images[(_e65 & 4095u)], wired_bindless_samplers[((_e75 >> bitcast<u32>(12i)) & 255u)], _e80);
    c_1 = _e81;
    let _e82 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e82))) != 0i) {
        let _e87 = c_1;
        return _e87;
    }
    let _e88 = c_1;
    param_4 = _e88.xyz;
    let _e90 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    c_1[0u] = _e90.x;
    c_1[1u] = _e90.y;
    c_1[2u] = _e90.z;
    let _e97 = c_1;
    return _e97;
}

fn main_1() {
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

    let _e95 = frag_tex_coord_1;
    parallax_tc = _e95;
    let _e96 = N_1;
    Np = _e96;
    param_5 = 0u;
    let _e97 = parallax_tc;
    param_6 = _e97;
    param_7 = 0i;
    let _e98 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    base = _e98;
    if override_type_22_ {
        if override_type_22_1 {
            let _e100 = base[3u];
            base[3u] = select(0f, 1f, (_e100 > 0f));
        } else {
            if override_type_22_2 {
                let _e105 = base[3u];
                param_8 = alpha_test_value;
                param_9 = (1f - _e105);
                let _e107 = frag_tex_coord_1;
                param_10 = _e107;
                let _e108 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_8), (&param_9), (&param_10));
                base[3u] = _e108;
            } else {
                if override_type_22_3 {
                    param_11 = alpha_test_value;
                    let _e111 = base[3u];
                    param_12 = _e111;
                    let _e112 = frag_tex_coord_1;
                    param_13 = _e112;
                    let _e113 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_11), (&param_12), (&param_13));
                    base[3u] = _e113;
                }
            }
        }
    } else {
        if override_type_22_4 {
            let _e116 = base[3u];
            if (_e116 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_22_5 {
                let _e119 = base[3u];
                if (_e119 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_22_6 {
                    let _e122 = base[3u];
                    if (_e122 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e125 = unnamed.lightColor;
    lightColorRadius = _e125;
    let _e126 = L_1;
    let _e130 = unnamed.lightVector;
    let _e135 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e126.xyz), _e130.xyz) * _e135), 0f, 1f);
    let _e139 = unnamed.lightVector;
    let _e140 = scale_1;
    let _e142 = L_1;
    LL = ((_e139 * _e140) + _e142);
    let _e144 = LL;
    nL = normalize(_e144.xyz);
    let _e147 = V_1;
    nV = normalize(_e147.xyz);
    let _e150 = LL;
    let _e152 = LL;
    let _e156 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e150.xyz, _e152.xyz) * _e156));
    let _e159 = intensFactor;
    if (_e159 <= 0f) {
        discard;
    }
    let _e161 = lightColorRadius;
    let _e163 = intensFactor;
    intens = (_e161.xyz * _e163);
    let _e168 = unnamed.packed_indices[0i][3u];
    let _e174 = unnamed.packed_indices[0i][3u];
    let _e179 = parallax_tc;
    let _e180 = textureSample(wired_bindless_images[(_e168 & 4095u)], wired_bindless_samplers[((_e174 >> bitcast<u32>(12i)) & 255u)], _e179);
    pbr = _e180.xyz;
    let _e183 = pbr[0u];
    ao = _e183;
    let _e185 = pbr[1u];
    roughness_3 = clamp(_e185, 0.04f, 1f);
    let _e188 = pbr[2u];
    metalness = _e188;
    let _e189 = nL;
    let _e190 = nV;
    halfVec = normalize((_e189 + _e190));
    let _e193 = Np;
    let _e194 = nL;
    NdotL_1 = max(dot(_e193, _e194), 0f);
    let _e197 = Np;
    let _e198 = nV;
    NdotV_2 = max(dot(_e197, _e198), 0.001f);
    let _e201 = Np;
    let _e202 = halfVec;
    NdotH_1 = max(dot(_e201, _e202), 0f);
    let _e205 = halfVec;
    let _e206 = nV;
    HdotV = max(dot(_e205, _e206), 0f);
    if override_type_22_7 {
        let _e209 = NdotL_1;
        let _e210 = NdotV_2;
        if ((_e209 * _e210) <= 0f) {
            discard;
        }
        let _e213 = NdotL_1;
        NdotL_1 = abs(_e213);
        let _e215 = NdotV_2;
        NdotV_2 = abs(_e215);
    }
    let _e217 = base;
    let _e219 = metalness;
    F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e217.xyz, vec3(_e219));
    let _e222 = NdotH_1;
    param_14 = _e222;
    let _e223 = roughness_3;
    param_15 = _e223;
    let _e224 = DistributionGGX_u0028_f1_u003b_f1_u003b((&param_14), (&param_15));
    D = _e224;
    let _e225 = NdotV_2;
    param_16 = _e225;
    let _e226 = NdotL_1;
    param_17 = _e226;
    let _e227 = roughness_3;
    param_18 = _e227;
    let _e228 = GeometrySmith_u0028_f1_u003b_f1_u003b_f1_u003b((&param_16), (&param_17), (&param_18));
    G = _e228;
    let _e229 = HdotV;
    param_19 = _e229;
    let _e230 = F0_1;
    param_20 = _e230;
    let _e231 = FresnelSchlick_u0028_f1_u003b_vf3_u003b((&param_19), (&param_20));
    F = _e231;
    let _e232 = D;
    let _e233 = G;
    let _e235 = F;
    let _e237 = NdotV_2;
    let _e239 = NdotL_1;
    specular = ((_e235 * (_e232 * _e233)) / vec3(max(((4f * _e237) * _e239), 0.001f)));
    let _e244 = F;
    let _e246 = metalness;
    kD = ((vec3<f32>(1f, 1f, 1f) - _e244) * (1f - _e246));
    let _e249 = kD;
    let _e250 = base;
    diffuseColor = ((_e249 * _e250.xyz) / vec3(3.1415927f));
    let _e255 = diffuseColor;
    let _e256 = specular;
    let _e258 = NdotL_1;
    let _e260 = intens;
    let _e261 = (((_e255 + _e256) * _e258) * _e260);
    let _e263 = base[3u];
    out_color = vec4<f32>(_e261.x, _e261.y, _e261.z, _e263);
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
