enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 30>,
    packed_indices: array<vec4<u32>, 3>,
}

@id(15) override normal_format: i32 = 0i;
override override_type_11_: bool = (normal_format == 0i);
override override_type_11_1: bool = (normal_format == 1i);
@id(4) override tex_domain: i32 = 0i;
@id(13) override parallax_steps: i32 = 8i;
@id(12) override parallax_scale: f32 = 0.03f;
@id(3) override alpha_to_coverage: i32 = 0i;
override override_type_11_2: bool = (alpha_to_coverage != 0i);
@id(0) override alpha_test_func: i32 = 0i;
override override_type_11_3: bool = (alpha_test_func == 1i);
override override_type_11_4: bool = (alpha_test_func == 2i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_11_5: bool = (alpha_test_func == 3i);
override override_type_11_6: bool = (alpha_test_func == 1i);
override override_type_11_7: bool = (alpha_test_func == 2i);
override override_type_11_8: bool = (alpha_test_func == 3i);
@id(5) override abs_light: i32 = 0i;
override override_type_11_9: bool = (abs_light != 0i);

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_position_1: vec3<f32>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> N_1: vec3<f32>;
var<private> V_1: vec4<f32>;
var<private> L_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e73 = unnamed.packed_indices[0i][0u];
    let _e79 = unnamed.packed_indices[0i][0u];
    let _e84 = textureDimensions(wired_bindless_images[(_e73 & 4095u)], 0i);
    ts = vec2<i32>(_e84);
    let _e87 = (*tc)[0u];
    let _e89 = ts[0u];
    let _e92 = dpdx((_e87 * f32(_e89)));
    dx = max(abs(_e92), 0.001f);
    let _e96 = (*tc)[1u];
    let _e98 = ts[1u];
    let _e101 = dpdy((_e96 * f32(_e98)));
    dy = max(abs(_e101), 0.001f);
    let _e104 = dx;
    let _e105 = dy;
    dxy = max(_e104, _e105);
    let _e107 = dxy;
    scale = max((1f / _e107), 1f);
    let _e110 = (*threshold);
    let _e111 = (*alpha);
    let _e112 = (*threshold);
    let _e114 = scale;
    ac = (_e110 + ((_e111 - _e112) * _e114));
    let _e117 = ac;
    return _e117;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e65 = (*c);
    (*c) = max(_e65, vec3<f32>(0f, 0f, 0f));
    let _e67 = (*c);
    cutoff = (_e67 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e69 = (*c);
    lo = (_e69 / vec3(12.92f));
    let _e72 = (*c);
    hi = pow(((_e72 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e77 = hi;
    let _e78 = lo;
    let _e79 = cutoff;
    return mix(_e77, _e78, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e79));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e88 = (*uv);
    let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    c_1 = _e89;
    let _e90 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e90))) != 0i) {
        let _e95 = c_1;
        return _e95;
    }
    let _e96 = c_1;
    param = _e96.xyz;
    let _e98 = sRGBToLinear_u0028_vf3_u003b((&param));
    c_1[0u] = _e98.x;
    c_1[1u] = _e98.y;
    c_1[2u] = _e98.z;
    let _e105 = c_1;
    return _e105;
}

fn wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b(role_1: ptr<function, u32>, uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var xy: vec2<f32>;
    var local: vec2<f32>;

    if override_type_11_ {
        let _e65 = (*role_1);
        let _e67 = (*role_1);
        let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
        let _e75 = (*role_1);
        let _e77 = (*role_1);
        let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
        let _e87 = (*uv_1);
        let _e88 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
        return ((_e88.xyz * 2f) - vec3(1f));
    }
    if override_type_11_1 {
        let _e93 = (*role_1);
        let _e95 = (*role_1);
        let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
        let _e103 = (*role_1);
        let _e105 = (*role_1);
        let _e110 = unnamed.packed_indices[(_e103 / 4u)][(_e105 % 4u)];
        let _e115 = (*uv_1);
        let _e116 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
        local = ((_e116.xy * 2f) - vec2(1f));
    } else {
        let _e121 = (*role_1);
        let _e123 = (*role_1);
        let _e128 = unnamed.packed_indices[(_e121 / 4u)][(_e123 % 4u)];
        let _e131 = (*role_1);
        let _e133 = (*role_1);
        let _e138 = unnamed.packed_indices[(_e131 / 4u)][(_e133 % 4u)];
        let _e143 = (*uv_1);
        let _e144 = textureSample(wired_bindless_images[(_e128 & 4095u)], wired_bindless_samplers[((_e138 >> bitcast<u32>(12i)) & 255u)], _e143);
        local = _e144.xy;
    }
    let _e146 = local;
    xy = _e146;
    let _e147 = xy;
    let _e148 = xy;
    let _e149 = xy;
    return vec3<f32>(_e147.x, _e147.y, sqrt(max(0f, (1f - dot(_e148, _e149)))));
}

fn main_1() {
    var dp1_: vec3<f32>;
    var dp2_: vec3<f32>;
    var duv1_: vec2<f32>;
    var duv2_: vec2<f32>;
    var dp2perp: vec3<f32>;
    var dp1perp: vec3<f32>;
    var T: vec3<f32>;
    var B: vec3<f32>;
    var invmax: f32;
    var TBN: mat3x3<f32>;
    var viewTS: vec3<f32>;
    var layerDepth: f32;
    var currentDepth: f32;
    var deltaUV: vec2<f32>;
    var currentUV: vec2<f32>;
    var currentMapDepth: f32;
    var i: i32;
    var i_1: i32;
    var parallax_tc: vec2<f32>;
    var mapNormal: vec3<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var Np: vec3<f32>;
    var base: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var param_6: f32;
    var param_7: f32;
    var param_8: vec2<f32>;
    var param_9: f32;
    var param_10: f32;
    var param_11: vec2<f32>;
    var lightColorRadius: vec4<f32>;
    var nL: vec3<f32>;
    var nV: vec3<f32>;
    var intensFactor: f32;
    var intens: vec3<f32>;
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;

    let _e102 = frag_position_1;
    let _e103 = dpdx(_e102);
    dp1_ = _e103;
    let _e104 = frag_position_1;
    let _e105 = dpdy(_e104);
    dp2_ = _e105;
    let _e106 = frag_tex_coord_1;
    let _e107 = dpdx(_e106);
    duv1_ = _e107;
    let _e108 = frag_tex_coord_1;
    let _e109 = dpdy(_e108);
    duv2_ = _e109;
    let _e110 = dp2_;
    let _e111 = N_1;
    dp2perp = cross(_e110, _e111);
    let _e113 = N_1;
    let _e114 = dp1_;
    dp1perp = cross(_e113, _e114);
    let _e116 = dp2perp;
    let _e118 = duv1_[0u];
    let _e120 = dp1perp;
    let _e122 = duv2_[0u];
    T = ((_e116 * _e118) + (_e120 * _e122));
    let _e125 = dp2perp;
    let _e127 = duv1_[1u];
    let _e129 = dp1perp;
    let _e131 = duv2_[1u];
    B = ((_e125 * _e127) + (_e129 * _e131));
    let _e134 = T;
    let _e135 = T;
    let _e137 = B;
    let _e138 = B;
    invmax = inverseSqrt(max(dot(_e134, _e135), dot(_e137, _e138)));
    let _e142 = T;
    let _e143 = invmax;
    let _e144 = (_e142 * _e143);
    let _e145 = B;
    let _e146 = invmax;
    let _e147 = (_e145 * _e146);
    let _e148 = N_1;
    TBN = mat3x3<f32>(vec3<f32>(_e144.x, _e144.y, _e144.z), vec3<f32>(_e147.x, _e147.y, _e147.z), vec3<f32>(_e148.x, _e148.y, _e148.z));
    let _e162 = TBN;
    let _e164 = V_1;
    viewTS = normalize((transpose(_e162) * normalize(_e164.xyz)));
    layerDepth = (1f / f32(parallax_steps));
    currentDepth = 0f;
    let _e171 = viewTS;
    let _e175 = viewTS[2u];
    deltaUV = ((_e171.xy * parallax_scale) / vec2((_e175 + 0.42f)));
    let _e180 = deltaUV;
    deltaUV = (_e180 / vec2(f32(parallax_steps)));
    let _e183 = frag_tex_coord_1;
    currentUV = _e183;
    let _e187 = unnamed.packed_indices[1i][1u];
    let _e193 = unnamed.packed_indices[1i][1u];
    let _e198 = currentUV;
    let _e199 = textureSample(wired_bindless_images[(_e187 & 4095u)], wired_bindless_samplers[((_e193 >> bitcast<u32>(12i)) & 255u)], _e198);
    currentMapDepth = (1f - _e199.w);
    i = 0i;
    loop {
        let _e202 = i;
        let _e204 = currentDepth;
        let _e205 = currentMapDepth;
        if ((_e202 < parallax_steps) && (_e204 < _e205)) {
            let _e208 = deltaUV;
            let _e209 = currentUV;
            currentUV = (_e209 - _e208);
            let _e214 = unnamed.packed_indices[1i][1u];
            let _e220 = unnamed.packed_indices[1i][1u];
            let _e225 = currentUV;
            let _e226 = textureSample(wired_bindless_images[(_e214 & 4095u)], wired_bindless_samplers[((_e220 >> bitcast<u32>(12i)) & 255u)], _e225);
            currentMapDepth = (1f - _e226.w);
            let _e229 = layerDepth;
            let _e230 = currentDepth;
            currentDepth = (_e230 + _e229);
            continue;
        } else {
            break;
        }
        continuing {
            let _e232 = i;
            i = (_e232 + 1i);
        }
    }
    i_1 = 0i;
    loop {
        let _e234 = i_1;
        if (_e234 < 2i) {
            let _e236 = deltaUV;
            deltaUV = (_e236 * 0.5f);
            let _e238 = layerDepth;
            layerDepth = (_e238 * 0.5f);
            let _e240 = currentDepth;
            let _e241 = currentMapDepth;
            if (_e240 > _e241) {
                let _e243 = deltaUV;
                let _e244 = currentUV;
                currentUV = (_e244 + _e243);
                let _e246 = layerDepth;
                let _e247 = currentDepth;
                currentDepth = (_e247 - _e246);
            } else {
                let _e249 = deltaUV;
                let _e250 = currentUV;
                currentUV = (_e250 - _e249);
                let _e252 = layerDepth;
                let _e253 = currentDepth;
                currentDepth = (_e253 + _e252);
            }
            let _e258 = unnamed.packed_indices[1i][1u];
            let _e264 = unnamed.packed_indices[1i][1u];
            let _e269 = currentUV;
            let _e270 = textureSample(wired_bindless_images[(_e258 & 4095u)], wired_bindless_samplers[((_e264 >> bitcast<u32>(12i)) & 255u)], _e269);
            currentMapDepth = (1f - _e270.w);
            continue;
        } else {
            break;
        }
        continuing {
            let _e273 = i_1;
            i_1 = (_e273 + 1i);
        }
    }
    let _e275 = currentUV;
    parallax_tc = _e275;
    param_1 = 5u;
    let _e276 = parallax_tc;
    param_2 = _e276;
    let _e277 = wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
    mapNormal = _e277;
    let _e278 = TBN;
    let _e279 = mapNormal;
    Np = normalize((_e278 * _e279));
    param_3 = 0u;
    let _e282 = parallax_tc;
    param_4 = _e282;
    param_5 = 0i;
    let _e283 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    base = _e283;
    if override_type_11_2 {
        if override_type_11_3 {
            let _e285 = base[3u];
            base[3u] = select(0f, 1f, (_e285 > 0f));
        } else {
            if override_type_11_4 {
                let _e290 = base[3u];
                param_6 = alpha_test_value;
                param_7 = (1f - _e290);
                let _e292 = frag_tex_coord_1;
                param_8 = _e292;
                let _e293 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_6), (&param_7), (&param_8));
                base[3u] = _e293;
            } else {
                if override_type_11_5 {
                    param_9 = alpha_test_value;
                    let _e296 = base[3u];
                    param_10 = _e296;
                    let _e297 = frag_tex_coord_1;
                    param_11 = _e297;
                    let _e298 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_9), (&param_10), (&param_11));
                    base[3u] = _e298;
                }
            }
        }
    } else {
        if override_type_11_6 {
            let _e301 = base[3u];
            if (_e301 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_11_7 {
                let _e304 = base[3u];
                if (_e304 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_11_8 {
                    let _e307 = base[3u];
                    if (_e307 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e310 = unnamed.lightColor;
    lightColorRadius = _e310;
    let _e311 = L_1;
    nL = normalize(_e311.xyz);
    let _e314 = V_1;
    nV = normalize(_e314.xyz);
    let _e317 = L_1;
    let _e319 = L_1;
    let _e323 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e317.xyz, _e319.xyz) * _e323));
    let _e326 = intensFactor;
    if (_e326 <= 0f) {
        discard;
    }
    let _e328 = lightColorRadius;
    let _e330 = intensFactor;
    intens = (_e328.xyz * _e330);
    let _e332 = Np;
    let _e333 = nL;
    diffuse = dot(_e332, _e333);
    let _e335 = Np;
    let _e336 = nL;
    let _e337 = nV;
    specFactor = dot(_e335, normalize((_e336 + _e337)));
    if override_type_11_9 {
        let _e341 = diffuse;
        let _e342 = Np;
        let _e343 = nV;
        if ((_e341 * dot(_e342, _e343)) <= 0f) {
            discard;
        }
        let _e347 = diffuse;
        diffuse = abs(_e347);
        let _e349 = specFactor;
        specFactor = abs(_e349);
    } else {
        let _e351 = diffuse;
        diffuse = max(_e351, 0f);
        let _e353 = specFactor;
        specFactor = max(_e353, 0f);
    }
    let _e355 = specFactor;
    let _e359 = base;
    spec = ((vec4((pow(_e355, 10f) * 0.25f)) * _e359) * 0.8f);
    let _e362 = base;
    let _e363 = diffuse;
    let _e366 = spec;
    let _e368 = intens;
    out_color = (((_e362 * vec4(_e363)) + _e366) * vec4<f32>(_e368.x, _e368.y, _e368.z, 1f));
    return;
}

@fragment 
fn main(@location(5) frag_position: vec3<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(3) V: vec4<f32>, @location(2) L: vec4<f32>) -> @location(0) vec4<f32> {
    frag_position_1 = frag_position;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    V_1 = V;
    L_1 = L;
    main_1();
    let _e11 = out_color;
    return _e11;
}
