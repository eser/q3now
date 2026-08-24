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

@id(15) override normal_format: i32 = 0i;
override override_type_8_: bool = (normal_format == 0i);
override override_type_8_1: bool = (normal_format == 1i);
@id(4) override tex_domain: i32 = 0i;
@id(13) override parallax_steps: i32 = 8i;
@id(12) override parallax_scale: f32 = 0.03f;
@id(3) override alpha_to_coverage: i32 = 0i;
override override_type_8_2: bool = (alpha_to_coverage != 0i);
@id(0) override alpha_test_func: i32 = 0i;
override override_type_8_3: bool = (alpha_test_func == 1i);
override override_type_8_4: bool = (alpha_test_func == 2i);
@id(1) override alpha_test_value: f32 = 0f;
override override_type_8_5: bool = (alpha_test_func == 3i);
override override_type_8_6: bool = (alpha_test_func == 1i);
override override_type_8_7: bool = (alpha_test_func == 2i);
override override_type_8_8: bool = (alpha_test_func == 3i);
@id(5) override abs_light: i32 = 0i;
override override_type_8_9: bool = (abs_light != 0i);

@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> frag_position_1: vec3<f32>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> N_1: vec3<f32>;
var<private> V_1: vec4<f32>;
var<private> L_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e68 + 0.5f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e75 = fogType;
    let _e78 = fogType;
    return (((_e73 > 0.5f) && (_e75 >= 1i)) && (_e78 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e68 = wired_advanced_fog_enabled_u0028_();
    if !(_e68) {
        return 0f;
    }
    let _e71 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e71, 0.000001f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e76 + 0.5f));
    let _e79 = fogType_1;
    if (_e79 == 1i) {
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e83 <= 0f) {
            return 0f;
        }
        let _e85 = viewDepth;
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e85 / _e88), 0f, 1f);
    }
    let _e93 = unnamed.advancedFogColorDensity[3u];
    let _e95 = viewDepth;
    opticalDepth = (max(_e93, 0f) * _e95);
    let _e97 = fogType_1;
    if (_e97 == 2i) {
        let _e99 = opticalDepth;
        return clamp((1f - exp(-(_e99))), 0f, 1f);
    }
    let _e104 = opticalDepth;
    let _e105 = opticalDepth;
    return clamp((1f - exp(-((_e104 * _e105)))), 0f, 1f);
}

fn CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b(threshold: ptr<function, f32>, alpha: ptr<function, f32>, tc: ptr<function, vec2<f32>>) -> f32 {
    var ts: vec2<i32>;
    var dx: f32;
    var dy: f32;
    var dxy: f32;
    var scale: f32;
    var ac: f32;

    let _e77 = unnamed.packed_indices[0i][0u];
    let _e83 = unnamed.packed_indices[0i][0u];
    let _e88 = textureDimensions(wired_bindless_images[(_e77 & 4095u)], 0i);
    ts = vec2<i32>(_e88);
    let _e91 = (*tc)[0u];
    let _e93 = ts[0u];
    let _e96 = dpdx((_e91 * f32(_e93)));
    dx = max(abs(_e96), 0.001f);
    let _e100 = (*tc)[1u];
    let _e102 = ts[1u];
    let _e105 = dpdy((_e100 * f32(_e102)));
    dy = max(abs(_e105), 0.001f);
    let _e108 = dx;
    let _e109 = dy;
    dxy = max(_e108, _e109);
    let _e111 = dxy;
    scale = max((1f / _e111), 1f);
    let _e114 = (*threshold);
    let _e115 = (*alpha);
    let _e116 = (*threshold);
    let _e118 = scale;
    ac = (_e114 + ((_e115 - _e116) * _e118));
    let _e121 = ac;
    return _e121;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c);
    (*c) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e92 = (*uv);
    let _e93 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    c_1 = _e93;
    let _e94 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e94))) != 0i) {
        let _e99 = c_1;
        return _e99;
    }
    let _e100 = c_1;
    param = _e100.xyz;
    let _e102 = sRGBToLinear_u0028_vf3_u003b((&param));
    c_1[0u] = _e102.x;
    c_1[1u] = _e102.y;
    c_1[2u] = _e102.z;
    let _e109 = c_1;
    return _e109;
}

fn wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b(role_1: ptr<function, u32>, uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var xy: vec2<f32>;
    var local: vec2<f32>;

    if override_type_8_ {
        let _e69 = (*role_1);
        let _e71 = (*role_1);
        let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
        let _e79 = (*role_1);
        let _e81 = (*role_1);
        let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
        let _e91 = (*uv_1);
        let _e92 = textureSample(wired_bindless_images[(_e76 & 4095u)], wired_bindless_samplers[((_e86 >> bitcast<u32>(12i)) & 255u)], _e91);
        return ((_e92.xyz * 2f) - vec3(1f));
    }
    if override_type_8_1 {
        let _e97 = (*role_1);
        let _e99 = (*role_1);
        let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
        let _e107 = (*role_1);
        let _e109 = (*role_1);
        let _e114 = unnamed.packed_indices[(_e107 / 4u)][(_e109 % 4u)];
        let _e119 = (*uv_1);
        let _e120 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
        local = ((_e120.xy * 2f) - vec2(1f));
    } else {
        let _e125 = (*role_1);
        let _e127 = (*role_1);
        let _e132 = unnamed.packed_indices[(_e125 / 4u)][(_e127 % 4u)];
        let _e135 = (*role_1);
        let _e137 = (*role_1);
        let _e142 = unnamed.packed_indices[(_e135 / 4u)][(_e137 % 4u)];
        let _e147 = (*uv_1);
        let _e148 = textureSample(wired_bindless_images[(_e132 & 4095u)], wired_bindless_samplers[((_e142 >> bitcast<u32>(12i)) & 255u)], _e147);
        local = _e148.xy;
    }
    let _e150 = local;
    xy = _e150;
    let _e151 = xy;
    let _e152 = xy;
    let _e153 = xy;
    return vec3<f32>(_e151.x, _e151.y, sqrt(max(0f, (1f - dot(_e152, _e153)))));
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

    let _e109 = frag_position_1;
    let _e110 = dpdx(_e109);
    dp1_ = _e110;
    let _e111 = frag_position_1;
    let _e112 = dpdy(_e111);
    dp2_ = _e112;
    let _e113 = frag_tex_coord_1;
    let _e114 = dpdx(_e113);
    duv1_ = _e114;
    let _e115 = frag_tex_coord_1;
    let _e116 = dpdy(_e115);
    duv2_ = _e116;
    let _e117 = dp2_;
    let _e118 = N_1;
    dp2perp = cross(_e117, _e118);
    let _e120 = N_1;
    let _e121 = dp1_;
    dp1perp = cross(_e120, _e121);
    let _e123 = dp2perp;
    let _e125 = duv1_[0u];
    let _e127 = dp1perp;
    let _e129 = duv2_[0u];
    T = ((_e123 * _e125) + (_e127 * _e129));
    let _e132 = dp2perp;
    let _e134 = duv1_[1u];
    let _e136 = dp1perp;
    let _e138 = duv2_[1u];
    B = ((_e132 * _e134) + (_e136 * _e138));
    let _e141 = T;
    let _e142 = T;
    let _e144 = B;
    let _e145 = B;
    invmax = inverseSqrt(max(dot(_e141, _e142), dot(_e144, _e145)));
    let _e149 = T;
    let _e150 = invmax;
    let _e151 = (_e149 * _e150);
    let _e152 = B;
    let _e153 = invmax;
    let _e154 = (_e152 * _e153);
    let _e155 = N_1;
    TBN = mat3x3<f32>(vec3<f32>(_e151.x, _e151.y, _e151.z), vec3<f32>(_e154.x, _e154.y, _e154.z), vec3<f32>(_e155.x, _e155.y, _e155.z));
    let _e169 = TBN;
    let _e171 = V_1;
    viewTS = normalize((transpose(_e169) * normalize(_e171.xyz)));
    layerDepth = (1f / f32(parallax_steps));
    currentDepth = 0f;
    let _e178 = viewTS;
    let _e182 = viewTS[2u];
    deltaUV = ((_e178.xy * parallax_scale) / vec2((_e182 + 0.42f)));
    let _e187 = deltaUV;
    deltaUV = (_e187 / vec2(f32(parallax_steps)));
    let _e190 = frag_tex_coord_1;
    currentUV = _e190;
    let _e194 = unnamed.packed_indices[1i][1u];
    let _e200 = unnamed.packed_indices[1i][1u];
    let _e205 = currentUV;
    let _e206 = textureSample(wired_bindless_images[(_e194 & 4095u)], wired_bindless_samplers[((_e200 >> bitcast<u32>(12i)) & 255u)], _e205);
    currentMapDepth = (1f - _e206.w);
    i = 0i;
    loop {
        let _e209 = i;
        let _e211 = currentDepth;
        let _e212 = currentMapDepth;
        if ((_e209 < parallax_steps) && (_e211 < _e212)) {
            let _e215 = deltaUV;
            let _e216 = currentUV;
            currentUV = (_e216 - _e215);
            let _e221 = unnamed.packed_indices[1i][1u];
            let _e227 = unnamed.packed_indices[1i][1u];
            let _e232 = currentUV;
            let _e233 = textureSample(wired_bindless_images[(_e221 & 4095u)], wired_bindless_samplers[((_e227 >> bitcast<u32>(12i)) & 255u)], _e232);
            currentMapDepth = (1f - _e233.w);
            let _e236 = layerDepth;
            let _e237 = currentDepth;
            currentDepth = (_e237 + _e236);
            continue;
        } else {
            break;
        }
        continuing {
            let _e239 = i;
            i = (_e239 + 1i);
        }
    }
    i_1 = 0i;
    loop {
        let _e241 = i_1;
        if (_e241 < 2i) {
            let _e243 = deltaUV;
            deltaUV = (_e243 * 0.5f);
            let _e245 = layerDepth;
            layerDepth = (_e245 * 0.5f);
            let _e247 = currentDepth;
            let _e248 = currentMapDepth;
            if (_e247 > _e248) {
                let _e250 = deltaUV;
                let _e251 = currentUV;
                currentUV = (_e251 + _e250);
                let _e253 = layerDepth;
                let _e254 = currentDepth;
                currentDepth = (_e254 - _e253);
            } else {
                let _e256 = deltaUV;
                let _e257 = currentUV;
                currentUV = (_e257 - _e256);
                let _e259 = layerDepth;
                let _e260 = currentDepth;
                currentDepth = (_e260 + _e259);
            }
            let _e265 = unnamed.packed_indices[1i][1u];
            let _e271 = unnamed.packed_indices[1i][1u];
            let _e276 = currentUV;
            let _e277 = textureSample(wired_bindless_images[(_e265 & 4095u)], wired_bindless_samplers[((_e271 >> bitcast<u32>(12i)) & 255u)], _e276);
            currentMapDepth = (1f - _e277.w);
            continue;
        } else {
            break;
        }
        continuing {
            let _e280 = i_1;
            i_1 = (_e280 + 1i);
        }
    }
    let _e282 = currentUV;
    parallax_tc = _e282;
    param_1 = 5u;
    let _e283 = parallax_tc;
    param_2 = _e283;
    let _e284 = wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
    mapNormal = _e284;
    let _e285 = TBN;
    let _e286 = mapNormal;
    Np = normalize((_e285 * _e286));
    param_3 = 0u;
    let _e289 = parallax_tc;
    param_4 = _e289;
    param_5 = 0i;
    let _e290 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    base = _e290;
    if override_type_8_2 {
        if override_type_8_3 {
            let _e292 = base[3u];
            base[3u] = select(0f, 1f, (_e292 > 0f));
        } else {
            if override_type_8_4 {
                let _e297 = base[3u];
                param_6 = alpha_test_value;
                param_7 = (1f - _e297);
                let _e299 = frag_tex_coord_1;
                param_8 = _e299;
                let _e300 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_6), (&param_7), (&param_8));
                base[3u] = _e300;
            } else {
                if override_type_8_5 {
                    param_9 = alpha_test_value;
                    let _e303 = base[3u];
                    param_10 = _e303;
                    let _e304 = frag_tex_coord_1;
                    param_11 = _e304;
                    let _e305 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_9), (&param_10), (&param_11));
                    base[3u] = _e305;
                }
            }
        }
    } else {
        if override_type_8_6 {
            let _e308 = base[3u];
            if (_e308 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_8_7 {
                let _e311 = base[3u];
                if (_e311 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_8_8 {
                    let _e314 = base[3u];
                    if (_e314 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e317 = unnamed.lightColor;
    lightColorRadius = _e317;
    let _e318 = L_1;
    let _e322 = unnamed.lightVector;
    let _e327 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e318.xyz), _e322.xyz) * _e327), 0f, 1f);
    let _e331 = unnamed.lightVector;
    let _e332 = scale_1;
    let _e334 = L_1;
    LL = ((_e331 * _e332) + _e334);
    let _e336 = LL;
    nL = normalize(_e336.xyz);
    let _e339 = V_1;
    nV = normalize(_e339.xyz);
    let _e342 = LL;
    let _e344 = LL;
    let _e348 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e342.xyz, _e344.xyz) * _e348));
    let _e351 = intensFactor;
    if (_e351 <= 0f) {
        discard;
    }
    let _e353 = lightColorRadius;
    let _e355 = intensFactor;
    intens = (_e353.xyz * _e355);
    let _e357 = wired_advanced_fog_amount_u0028_();
    fogAmount = _e357;
    let _e358 = fogAmount;
    let _e360 = base;
    let _e362 = (_e360.xyz * (1f - _e358));
    base[0u] = _e362.x;
    base[1u] = _e362.y;
    base[2u] = _e362.z;
    let _e369 = Np;
    let _e370 = nL;
    diffuse = dot(_e369, _e370);
    let _e372 = Np;
    let _e373 = nL;
    let _e374 = nV;
    specFactor = dot(_e372, normalize((_e373 + _e374)));
    if override_type_8_9 {
        let _e378 = diffuse;
        let _e379 = Np;
        let _e380 = nV;
        if ((_e378 * dot(_e379, _e380)) <= 0f) {
            discard;
        }
        let _e384 = diffuse;
        diffuse = abs(_e384);
        let _e386 = specFactor;
        specFactor = abs(_e386);
    } else {
        let _e388 = diffuse;
        diffuse = max(_e388, 0f);
        let _e390 = specFactor;
        specFactor = max(_e390, 0f);
    }
    let _e392 = specFactor;
    let _e396 = base;
    spec = ((vec4((pow(_e392, 10f) * 0.25f)) * _e396) * 0.8f);
    let _e399 = base;
    let _e400 = diffuse;
    let _e403 = spec;
    let _e405 = intens;
    out_color = (((_e399 * vec4(_e400)) + _e403) * vec4<f32>(_e405.x, _e405.y, _e405.z, 1f));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(5) frag_position: vec3<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(3) V: vec4<f32>, @location(2) L: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_position_1 = frag_position;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    V_1 = V;
    L_1 = L;
    main_1();
    let _e13 = out_color;
    return _e13;
}
