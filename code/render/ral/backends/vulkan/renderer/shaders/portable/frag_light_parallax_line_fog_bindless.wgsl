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
var<private> fog_tex_coord_1: vec2<f32>;
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
    var fog: vec4<f32>;
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

    let _e113 = unnamed.packed_indices[0i][1u];
    let _e119 = unnamed.packed_indices[0i][1u];
    let _e124 = fog_tex_coord_1;
    let _e125 = textureSample(wired_bindless_images[(_e113 & 4095u)], wired_bindless_samplers[((_e119 >> bitcast<u32>(12i)) & 255u)], _e124);
    fog = _e125;
    let _e126 = frag_position_1;
    let _e127 = dpdx(_e126);
    dp1_ = _e127;
    let _e128 = frag_position_1;
    let _e129 = dpdy(_e128);
    dp2_ = _e129;
    let _e130 = frag_tex_coord_1;
    let _e131 = dpdx(_e130);
    duv1_ = _e131;
    let _e132 = frag_tex_coord_1;
    let _e133 = dpdy(_e132);
    duv2_ = _e133;
    let _e134 = dp2_;
    let _e135 = N_1;
    dp2perp = cross(_e134, _e135);
    let _e137 = N_1;
    let _e138 = dp1_;
    dp1perp = cross(_e137, _e138);
    let _e140 = dp2perp;
    let _e142 = duv1_[0u];
    let _e144 = dp1perp;
    let _e146 = duv2_[0u];
    T = ((_e140 * _e142) + (_e144 * _e146));
    let _e149 = dp2perp;
    let _e151 = duv1_[1u];
    let _e153 = dp1perp;
    let _e155 = duv2_[1u];
    B = ((_e149 * _e151) + (_e153 * _e155));
    let _e158 = T;
    let _e159 = T;
    let _e161 = B;
    let _e162 = B;
    invmax = inverseSqrt(max(dot(_e158, _e159), dot(_e161, _e162)));
    let _e166 = T;
    let _e167 = invmax;
    let _e168 = (_e166 * _e167);
    let _e169 = B;
    let _e170 = invmax;
    let _e171 = (_e169 * _e170);
    let _e172 = N_1;
    TBN = mat3x3<f32>(vec3<f32>(_e168.x, _e168.y, _e168.z), vec3<f32>(_e171.x, _e171.y, _e171.z), vec3<f32>(_e172.x, _e172.y, _e172.z));
    let _e186 = TBN;
    let _e188 = V_1;
    viewTS = normalize((transpose(_e186) * normalize(_e188.xyz)));
    layerDepth = (1f / f32(parallax_steps));
    currentDepth = 0f;
    let _e195 = viewTS;
    let _e199 = viewTS[2u];
    deltaUV = ((_e195.xy * parallax_scale) / vec2((_e199 + 0.42f)));
    let _e204 = deltaUV;
    deltaUV = (_e204 / vec2(f32(parallax_steps)));
    let _e207 = frag_tex_coord_1;
    currentUV = _e207;
    let _e211 = unnamed.packed_indices[1i][1u];
    let _e217 = unnamed.packed_indices[1i][1u];
    let _e222 = currentUV;
    let _e223 = textureSample(wired_bindless_images[(_e211 & 4095u)], wired_bindless_samplers[((_e217 >> bitcast<u32>(12i)) & 255u)], _e222);
    currentMapDepth = (1f - _e223.w);
    i = 0i;
    loop {
        let _e226 = i;
        let _e228 = currentDepth;
        let _e229 = currentMapDepth;
        if ((_e226 < parallax_steps) && (_e228 < _e229)) {
            let _e232 = deltaUV;
            let _e233 = currentUV;
            currentUV = (_e233 - _e232);
            let _e238 = unnamed.packed_indices[1i][1u];
            let _e244 = unnamed.packed_indices[1i][1u];
            let _e249 = currentUV;
            let _e250 = textureSample(wired_bindless_images[(_e238 & 4095u)], wired_bindless_samplers[((_e244 >> bitcast<u32>(12i)) & 255u)], _e249);
            currentMapDepth = (1f - _e250.w);
            let _e253 = layerDepth;
            let _e254 = currentDepth;
            currentDepth = (_e254 + _e253);
            continue;
        } else {
            break;
        }
        continuing {
            let _e256 = i;
            i = (_e256 + 1i);
        }
    }
    i_1 = 0i;
    loop {
        let _e258 = i_1;
        if (_e258 < 2i) {
            let _e260 = deltaUV;
            deltaUV = (_e260 * 0.5f);
            let _e262 = layerDepth;
            layerDepth = (_e262 * 0.5f);
            let _e264 = currentDepth;
            let _e265 = currentMapDepth;
            if (_e264 > _e265) {
                let _e267 = deltaUV;
                let _e268 = currentUV;
                currentUV = (_e268 + _e267);
                let _e270 = layerDepth;
                let _e271 = currentDepth;
                currentDepth = (_e271 - _e270);
            } else {
                let _e273 = deltaUV;
                let _e274 = currentUV;
                currentUV = (_e274 - _e273);
                let _e276 = layerDepth;
                let _e277 = currentDepth;
                currentDepth = (_e277 + _e276);
            }
            let _e282 = unnamed.packed_indices[1i][1u];
            let _e288 = unnamed.packed_indices[1i][1u];
            let _e293 = currentUV;
            let _e294 = textureSample(wired_bindless_images[(_e282 & 4095u)], wired_bindless_samplers[((_e288 >> bitcast<u32>(12i)) & 255u)], _e293);
            currentMapDepth = (1f - _e294.w);
            continue;
        } else {
            break;
        }
        continuing {
            let _e297 = i_1;
            i_1 = (_e297 + 1i);
        }
    }
    let _e299 = currentUV;
    parallax_tc = _e299;
    param_1 = 5u;
    let _e300 = parallax_tc;
    param_2 = _e300;
    let _e301 = wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
    mapNormal = _e301;
    let _e302 = TBN;
    let _e303 = mapNormal;
    Np = normalize((_e302 * _e303));
    param_3 = 0u;
    let _e306 = parallax_tc;
    param_4 = _e306;
    param_5 = 0i;
    let _e307 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    base = _e307;
    if override_type_8_2 {
        if override_type_8_3 {
            let _e309 = base[3u];
            base[3u] = select(0f, 1f, (_e309 > 0f));
        } else {
            if override_type_8_4 {
                let _e314 = base[3u];
                param_6 = alpha_test_value;
                param_7 = (1f - _e314);
                let _e316 = frag_tex_coord_1;
                param_8 = _e316;
                let _e317 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_6), (&param_7), (&param_8));
                base[3u] = _e317;
            } else {
                if override_type_8_5 {
                    param_9 = alpha_test_value;
                    let _e320 = base[3u];
                    param_10 = _e320;
                    let _e321 = frag_tex_coord_1;
                    param_11 = _e321;
                    let _e322 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_9), (&param_10), (&param_11));
                    base[3u] = _e322;
                }
            }
        }
    } else {
        if override_type_8_6 {
            let _e325 = base[3u];
            if (_e325 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_8_7 {
                let _e328 = base[3u];
                if (_e328 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_8_8 {
                    let _e331 = base[3u];
                    if (_e331 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e334 = unnamed.lightColor;
    lightColorRadius = _e334;
    let _e335 = L_1;
    let _e339 = unnamed.lightVector;
    let _e344 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e335.xyz), _e339.xyz) * _e344), 0f, 1f);
    let _e348 = unnamed.lightVector;
    let _e349 = scale_1;
    let _e351 = L_1;
    LL = ((_e348 * _e349) + _e351);
    let _e353 = LL;
    nL = normalize(_e353.xyz);
    let _e356 = V_1;
    nV = normalize(_e356.xyz);
    let _e359 = LL;
    let _e361 = LL;
    let _e365 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e359.xyz, _e361.xyz) * _e365));
    let _e368 = intensFactor;
    if (_e368 <= 0f) {
        discard;
    }
    let _e370 = lightColorRadius;
    let _e372 = intensFactor;
    intens = (_e370.xyz * _e372);
    let _e374 = wired_advanced_fog_amount_u0028_();
    fogAmount = _e374;
    let _e375 = wired_advanced_fog_enabled_u0028_();
    if !(_e375) {
        let _e378 = fog[3u];
        fogAmount = _e378;
    }
    let _e379 = fogAmount;
    let _e381 = base;
    let _e383 = (_e381.xyz * (1f - _e379));
    base[0u] = _e383.x;
    base[1u] = _e383.y;
    base[2u] = _e383.z;
    let _e390 = Np;
    let _e391 = nL;
    diffuse = dot(_e390, _e391);
    let _e393 = Np;
    let _e394 = nL;
    let _e395 = nV;
    specFactor = dot(_e393, normalize((_e394 + _e395)));
    if override_type_8_9 {
        let _e399 = diffuse;
        let _e400 = Np;
        let _e401 = nV;
        if ((_e399 * dot(_e400, _e401)) <= 0f) {
            discard;
        }
        let _e405 = diffuse;
        diffuse = abs(_e405);
        let _e407 = specFactor;
        specFactor = abs(_e407);
    } else {
        let _e409 = diffuse;
        diffuse = max(_e409, 0f);
        let _e411 = specFactor;
        specFactor = max(_e411, 0f);
    }
    let _e413 = specFactor;
    let _e417 = base;
    spec = ((vec4((pow(_e413, 10f) * 0.25f)) * _e417) * 0.8f);
    let _e420 = base;
    let _e421 = diffuse;
    let _e424 = spec;
    let _e426 = intens;
    out_color = (((_e420 * vec4(_e421)) + _e424) * vec4<f32>(_e426.x, _e426.y, _e426.z, 1f));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(5) frag_position: vec3<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(3) V: vec4<f32>, @location(2) L: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_position_1 = frag_position;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    V_1 = V;
    L_1 = L;
    main_1();
    let _e15 = out_color;
    return _e15;
}
