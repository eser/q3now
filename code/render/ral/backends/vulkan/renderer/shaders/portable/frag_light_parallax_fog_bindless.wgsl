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
    var nL: vec3<f32>;
    var nV: vec3<f32>;
    var intensFactor: f32;
    var intens: vec3<f32>;
    var fogAmount: f32;
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;

    let _e111 = unnamed.packed_indices[0i][1u];
    let _e117 = unnamed.packed_indices[0i][1u];
    let _e122 = fog_tex_coord_1;
    let _e123 = textureSample(wired_bindless_images[(_e111 & 4095u)], wired_bindless_samplers[((_e117 >> bitcast<u32>(12i)) & 255u)], _e122);
    fog = _e123;
    let _e124 = frag_position_1;
    let _e125 = dpdx(_e124);
    dp1_ = _e125;
    let _e126 = frag_position_1;
    let _e127 = dpdy(_e126);
    dp2_ = _e127;
    let _e128 = frag_tex_coord_1;
    let _e129 = dpdx(_e128);
    duv1_ = _e129;
    let _e130 = frag_tex_coord_1;
    let _e131 = dpdy(_e130);
    duv2_ = _e131;
    let _e132 = dp2_;
    let _e133 = N_1;
    dp2perp = cross(_e132, _e133);
    let _e135 = N_1;
    let _e136 = dp1_;
    dp1perp = cross(_e135, _e136);
    let _e138 = dp2perp;
    let _e140 = duv1_[0u];
    let _e142 = dp1perp;
    let _e144 = duv2_[0u];
    T = ((_e138 * _e140) + (_e142 * _e144));
    let _e147 = dp2perp;
    let _e149 = duv1_[1u];
    let _e151 = dp1perp;
    let _e153 = duv2_[1u];
    B = ((_e147 * _e149) + (_e151 * _e153));
    let _e156 = T;
    let _e157 = T;
    let _e159 = B;
    let _e160 = B;
    invmax = inverseSqrt(max(dot(_e156, _e157), dot(_e159, _e160)));
    let _e164 = T;
    let _e165 = invmax;
    let _e166 = (_e164 * _e165);
    let _e167 = B;
    let _e168 = invmax;
    let _e169 = (_e167 * _e168);
    let _e170 = N_1;
    TBN = mat3x3<f32>(vec3<f32>(_e166.x, _e166.y, _e166.z), vec3<f32>(_e169.x, _e169.y, _e169.z), vec3<f32>(_e170.x, _e170.y, _e170.z));
    let _e184 = TBN;
    let _e186 = V_1;
    viewTS = normalize((transpose(_e184) * normalize(_e186.xyz)));
    layerDepth = (1f / f32(parallax_steps));
    currentDepth = 0f;
    let _e193 = viewTS;
    let _e197 = viewTS[2u];
    deltaUV = ((_e193.xy * parallax_scale) / vec2((_e197 + 0.42f)));
    let _e202 = deltaUV;
    deltaUV = (_e202 / vec2(f32(parallax_steps)));
    let _e205 = frag_tex_coord_1;
    currentUV = _e205;
    let _e209 = unnamed.packed_indices[1i][1u];
    let _e215 = unnamed.packed_indices[1i][1u];
    let _e220 = currentUV;
    let _e221 = textureSample(wired_bindless_images[(_e209 & 4095u)], wired_bindless_samplers[((_e215 >> bitcast<u32>(12i)) & 255u)], _e220);
    currentMapDepth = (1f - _e221.w);
    i = 0i;
    loop {
        let _e224 = i;
        let _e226 = currentDepth;
        let _e227 = currentMapDepth;
        if ((_e224 < parallax_steps) && (_e226 < _e227)) {
            let _e230 = deltaUV;
            let _e231 = currentUV;
            currentUV = (_e231 - _e230);
            let _e236 = unnamed.packed_indices[1i][1u];
            let _e242 = unnamed.packed_indices[1i][1u];
            let _e247 = currentUV;
            let _e248 = textureSample(wired_bindless_images[(_e236 & 4095u)], wired_bindless_samplers[((_e242 >> bitcast<u32>(12i)) & 255u)], _e247);
            currentMapDepth = (1f - _e248.w);
            let _e251 = layerDepth;
            let _e252 = currentDepth;
            currentDepth = (_e252 + _e251);
            continue;
        } else {
            break;
        }
        continuing {
            let _e254 = i;
            i = (_e254 + 1i);
        }
    }
    i_1 = 0i;
    loop {
        let _e256 = i_1;
        if (_e256 < 2i) {
            let _e258 = deltaUV;
            deltaUV = (_e258 * 0.5f);
            let _e260 = layerDepth;
            layerDepth = (_e260 * 0.5f);
            let _e262 = currentDepth;
            let _e263 = currentMapDepth;
            if (_e262 > _e263) {
                let _e265 = deltaUV;
                let _e266 = currentUV;
                currentUV = (_e266 + _e265);
                let _e268 = layerDepth;
                let _e269 = currentDepth;
                currentDepth = (_e269 - _e268);
            } else {
                let _e271 = deltaUV;
                let _e272 = currentUV;
                currentUV = (_e272 - _e271);
                let _e274 = layerDepth;
                let _e275 = currentDepth;
                currentDepth = (_e275 + _e274);
            }
            let _e280 = unnamed.packed_indices[1i][1u];
            let _e286 = unnamed.packed_indices[1i][1u];
            let _e291 = currentUV;
            let _e292 = textureSample(wired_bindless_images[(_e280 & 4095u)], wired_bindless_samplers[((_e286 >> bitcast<u32>(12i)) & 255u)], _e291);
            currentMapDepth = (1f - _e292.w);
            continue;
        } else {
            break;
        }
        continuing {
            let _e295 = i_1;
            i_1 = (_e295 + 1i);
        }
    }
    let _e297 = currentUV;
    parallax_tc = _e297;
    param_1 = 5u;
    let _e298 = parallax_tc;
    param_2 = _e298;
    let _e299 = wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
    mapNormal = _e299;
    let _e300 = TBN;
    let _e301 = mapNormal;
    Np = normalize((_e300 * _e301));
    param_3 = 0u;
    let _e304 = parallax_tc;
    param_4 = _e304;
    param_5 = 0i;
    let _e305 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    base = _e305;
    if override_type_8_2 {
        if override_type_8_3 {
            let _e307 = base[3u];
            base[3u] = select(0f, 1f, (_e307 > 0f));
        } else {
            if override_type_8_4 {
                let _e312 = base[3u];
                param_6 = alpha_test_value;
                param_7 = (1f - _e312);
                let _e314 = frag_tex_coord_1;
                param_8 = _e314;
                let _e315 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_6), (&param_7), (&param_8));
                base[3u] = _e315;
            } else {
                if override_type_8_5 {
                    param_9 = alpha_test_value;
                    let _e318 = base[3u];
                    param_10 = _e318;
                    let _e319 = frag_tex_coord_1;
                    param_11 = _e319;
                    let _e320 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_9), (&param_10), (&param_11));
                    base[3u] = _e320;
                }
            }
        }
    } else {
        if override_type_8_6 {
            let _e323 = base[3u];
            if (_e323 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_8_7 {
                let _e326 = base[3u];
                if (_e326 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_8_8 {
                    let _e329 = base[3u];
                    if (_e329 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e332 = unnamed.lightColor;
    lightColorRadius = _e332;
    let _e333 = L_1;
    nL = normalize(_e333.xyz);
    let _e336 = V_1;
    nV = normalize(_e336.xyz);
    let _e339 = L_1;
    let _e341 = L_1;
    let _e345 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e339.xyz, _e341.xyz) * _e345));
    let _e348 = intensFactor;
    if (_e348 <= 0f) {
        discard;
    }
    let _e350 = lightColorRadius;
    let _e352 = intensFactor;
    intens = (_e350.xyz * _e352);
    let _e354 = wired_advanced_fog_amount_u0028_();
    fogAmount = _e354;
    let _e355 = wired_advanced_fog_enabled_u0028_();
    if !(_e355) {
        let _e358 = fog[3u];
        fogAmount = _e358;
    }
    let _e359 = fogAmount;
    let _e361 = base;
    let _e363 = (_e361.xyz * (1f - _e359));
    base[0u] = _e363.x;
    base[1u] = _e363.y;
    base[2u] = _e363.z;
    let _e370 = Np;
    let _e371 = nL;
    diffuse = dot(_e370, _e371);
    let _e373 = Np;
    let _e374 = nL;
    let _e375 = nV;
    specFactor = dot(_e373, normalize((_e374 + _e375)));
    if override_type_8_9 {
        let _e379 = diffuse;
        let _e380 = Np;
        let _e381 = nV;
        if ((_e379 * dot(_e380, _e381)) <= 0f) {
            discard;
        }
        let _e385 = diffuse;
        diffuse = abs(_e385);
        let _e387 = specFactor;
        specFactor = abs(_e387);
    } else {
        let _e389 = diffuse;
        diffuse = max(_e389, 0f);
        let _e391 = specFactor;
        specFactor = max(_e391, 0f);
    }
    let _e393 = specFactor;
    let _e397 = base;
    spec = ((vec4((pow(_e393, 10f) * 0.25f)) * _e397) * 0.8f);
    let _e400 = base;
    let _e401 = diffuse;
    let _e404 = spec;
    let _e406 = intens;
    out_color = (((_e400 * vec4(_e401)) + _e404) * vec4<f32>(_e406.x, _e406.y, _e406.z, 1f));
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
