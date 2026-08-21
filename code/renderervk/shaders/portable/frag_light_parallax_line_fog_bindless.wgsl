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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e74 = unnamed.packed_indices[0i][0u];
    let _e80 = unnamed.packed_indices[0i][0u];
    let _e85 = textureDimensions(wired_bindless_images[(_e74 & 4095u)], 0i);
    ts = vec2<i32>(_e85);
    let _e88 = (*tc)[0u];
    let _e90 = ts[0u];
    let _e93 = dpdx((_e88 * f32(_e90)));
    dx = max(abs(_e93), 0.001f);
    let _e97 = (*tc)[1u];
    let _e99 = ts[1u];
    let _e102 = dpdy((_e97 * f32(_e99)));
    dy = max(abs(_e102), 0.001f);
    let _e105 = dx;
    let _e106 = dy;
    dxy = max(_e105, _e106);
    let _e108 = dxy;
    scale = max((1f / _e108), 1f);
    let _e111 = (*threshold);
    let _e112 = (*alpha);
    let _e113 = (*threshold);
    let _e115 = scale;
    ac = (_e111 + ((_e112 - _e113) * _e115));
    let _e118 = ac;
    return _e118;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e66 = (*c);
    (*c) = max(_e66, vec3<f32>(0f, 0f, 0f));
    let _e68 = (*c);
    cutoff = (_e68 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e70 = (*c);
    lo = (_e70 / vec3(12.92f));
    let _e73 = (*c);
    hi = pow(((_e73 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e78 = hi;
    let _e79 = lo;
    let _e80 = cutoff;
    return mix(_e78, _e79, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e80));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e89 = (*uv);
    let _e90 = textureSample(wired_bindless_images[(_e74 & 4095u)], wired_bindless_samplers[((_e84 >> bitcast<u32>(12i)) & 255u)], _e89);
    c_1 = _e90;
    let _e91 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e91))) != 0i) {
        let _e96 = c_1;
        return _e96;
    }
    let _e97 = c_1;
    param = _e97.xyz;
    let _e99 = sRGBToLinear_u0028_vf3_u003b((&param));
    c_1[0u] = _e99.x;
    c_1[1u] = _e99.y;
    c_1[2u] = _e99.z;
    let _e106 = c_1;
    return _e106;
}

fn wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b(role_1: ptr<function, u32>, uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var xy: vec2<f32>;
    var local: vec2<f32>;

    if override_type_11_ {
        let _e66 = (*role_1);
        let _e68 = (*role_1);
        let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
        let _e76 = (*role_1);
        let _e78 = (*role_1);
        let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
        let _e88 = (*uv_1);
        let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
        return ((_e89.xyz * 2f) - vec3(1f));
    }
    if override_type_11_1 {
        let _e94 = (*role_1);
        let _e96 = (*role_1);
        let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
        let _e104 = (*role_1);
        let _e106 = (*role_1);
        let _e111 = unnamed.packed_indices[(_e104 / 4u)][(_e106 % 4u)];
        let _e116 = (*uv_1);
        let _e117 = textureSample(wired_bindless_images[(_e101 & 4095u)], wired_bindless_samplers[((_e111 >> bitcast<u32>(12i)) & 255u)], _e116);
        local = ((_e117.xy * 2f) - vec2(1f));
    } else {
        let _e122 = (*role_1);
        let _e124 = (*role_1);
        let _e129 = unnamed.packed_indices[(_e122 / 4u)][(_e124 % 4u)];
        let _e132 = (*role_1);
        let _e134 = (*role_1);
        let _e139 = unnamed.packed_indices[(_e132 / 4u)][(_e134 % 4u)];
        let _e144 = (*uv_1);
        let _e145 = textureSample(wired_bindless_images[(_e129 & 4095u)], wired_bindless_samplers[((_e139 >> bitcast<u32>(12i)) & 255u)], _e144);
        local = _e145.xy;
    }
    let _e147 = local;
    xy = _e147;
    let _e148 = xy;
    let _e149 = xy;
    let _e150 = xy;
    return vec3<f32>(_e148.x, _e148.y, sqrt(max(0f, (1f - dot(_e149, _e150)))));
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
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;

    let _e109 = unnamed.packed_indices[0i][1u];
    let _e115 = unnamed.packed_indices[0i][1u];
    let _e120 = fog_tex_coord_1;
    let _e121 = textureSample(wired_bindless_images[(_e109 & 4095u)], wired_bindless_samplers[((_e115 >> bitcast<u32>(12i)) & 255u)], _e120);
    fog = _e121;
    let _e122 = frag_position_1;
    let _e123 = dpdx(_e122);
    dp1_ = _e123;
    let _e124 = frag_position_1;
    let _e125 = dpdy(_e124);
    dp2_ = _e125;
    let _e126 = frag_tex_coord_1;
    let _e127 = dpdx(_e126);
    duv1_ = _e127;
    let _e128 = frag_tex_coord_1;
    let _e129 = dpdy(_e128);
    duv2_ = _e129;
    let _e130 = dp2_;
    let _e131 = N_1;
    dp2perp = cross(_e130, _e131);
    let _e133 = N_1;
    let _e134 = dp1_;
    dp1perp = cross(_e133, _e134);
    let _e136 = dp2perp;
    let _e138 = duv1_[0u];
    let _e140 = dp1perp;
    let _e142 = duv2_[0u];
    T = ((_e136 * _e138) + (_e140 * _e142));
    let _e145 = dp2perp;
    let _e147 = duv1_[1u];
    let _e149 = dp1perp;
    let _e151 = duv2_[1u];
    B = ((_e145 * _e147) + (_e149 * _e151));
    let _e154 = T;
    let _e155 = T;
    let _e157 = B;
    let _e158 = B;
    invmax = inverseSqrt(max(dot(_e154, _e155), dot(_e157, _e158)));
    let _e162 = T;
    let _e163 = invmax;
    let _e164 = (_e162 * _e163);
    let _e165 = B;
    let _e166 = invmax;
    let _e167 = (_e165 * _e166);
    let _e168 = N_1;
    TBN = mat3x3<f32>(vec3<f32>(_e164.x, _e164.y, _e164.z), vec3<f32>(_e167.x, _e167.y, _e167.z), vec3<f32>(_e168.x, _e168.y, _e168.z));
    let _e182 = TBN;
    let _e184 = V_1;
    viewTS = normalize((transpose(_e182) * normalize(_e184.xyz)));
    layerDepth = (1f / f32(parallax_steps));
    currentDepth = 0f;
    let _e191 = viewTS;
    let _e195 = viewTS[2u];
    deltaUV = ((_e191.xy * parallax_scale) / vec2((_e195 + 0.42f)));
    let _e200 = deltaUV;
    deltaUV = (_e200 / vec2(f32(parallax_steps)));
    let _e203 = frag_tex_coord_1;
    currentUV = _e203;
    let _e207 = unnamed.packed_indices[1i][1u];
    let _e213 = unnamed.packed_indices[1i][1u];
    let _e218 = currentUV;
    let _e219 = textureSample(wired_bindless_images[(_e207 & 4095u)], wired_bindless_samplers[((_e213 >> bitcast<u32>(12i)) & 255u)], _e218);
    currentMapDepth = (1f - _e219.w);
    i = 0i;
    loop {
        let _e222 = i;
        let _e224 = currentDepth;
        let _e225 = currentMapDepth;
        if ((_e222 < parallax_steps) && (_e224 < _e225)) {
            let _e228 = deltaUV;
            let _e229 = currentUV;
            currentUV = (_e229 - _e228);
            let _e234 = unnamed.packed_indices[1i][1u];
            let _e240 = unnamed.packed_indices[1i][1u];
            let _e245 = currentUV;
            let _e246 = textureSample(wired_bindless_images[(_e234 & 4095u)], wired_bindless_samplers[((_e240 >> bitcast<u32>(12i)) & 255u)], _e245);
            currentMapDepth = (1f - _e246.w);
            let _e249 = layerDepth;
            let _e250 = currentDepth;
            currentDepth = (_e250 + _e249);
            continue;
        } else {
            break;
        }
        continuing {
            let _e252 = i;
            i = (_e252 + 1i);
        }
    }
    i_1 = 0i;
    loop {
        let _e254 = i_1;
        if (_e254 < 2i) {
            let _e256 = deltaUV;
            deltaUV = (_e256 * 0.5f);
            let _e258 = layerDepth;
            layerDepth = (_e258 * 0.5f);
            let _e260 = currentDepth;
            let _e261 = currentMapDepth;
            if (_e260 > _e261) {
                let _e263 = deltaUV;
                let _e264 = currentUV;
                currentUV = (_e264 + _e263);
                let _e266 = layerDepth;
                let _e267 = currentDepth;
                currentDepth = (_e267 - _e266);
            } else {
                let _e269 = deltaUV;
                let _e270 = currentUV;
                currentUV = (_e270 - _e269);
                let _e272 = layerDepth;
                let _e273 = currentDepth;
                currentDepth = (_e273 + _e272);
            }
            let _e278 = unnamed.packed_indices[1i][1u];
            let _e284 = unnamed.packed_indices[1i][1u];
            let _e289 = currentUV;
            let _e290 = textureSample(wired_bindless_images[(_e278 & 4095u)], wired_bindless_samplers[((_e284 >> bitcast<u32>(12i)) & 255u)], _e289);
            currentMapDepth = (1f - _e290.w);
            continue;
        } else {
            break;
        }
        continuing {
            let _e293 = i_1;
            i_1 = (_e293 + 1i);
        }
    }
    let _e295 = currentUV;
    parallax_tc = _e295;
    param_1 = 5u;
    let _e296 = parallax_tc;
    param_2 = _e296;
    let _e297 = wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
    mapNormal = _e297;
    let _e298 = TBN;
    let _e299 = mapNormal;
    Np = normalize((_e298 * _e299));
    param_3 = 0u;
    let _e302 = parallax_tc;
    param_4 = _e302;
    param_5 = 0i;
    let _e303 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    base = _e303;
    if override_type_11_2 {
        if override_type_11_3 {
            let _e305 = base[3u];
            base[3u] = select(0f, 1f, (_e305 > 0f));
        } else {
            if override_type_11_4 {
                let _e310 = base[3u];
                param_6 = alpha_test_value;
                param_7 = (1f - _e310);
                let _e312 = frag_tex_coord_1;
                param_8 = _e312;
                let _e313 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_6), (&param_7), (&param_8));
                base[3u] = _e313;
            } else {
                if override_type_11_5 {
                    param_9 = alpha_test_value;
                    let _e316 = base[3u];
                    param_10 = _e316;
                    let _e317 = frag_tex_coord_1;
                    param_11 = _e317;
                    let _e318 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_9), (&param_10), (&param_11));
                    base[3u] = _e318;
                }
            }
        }
    } else {
        if override_type_11_6 {
            let _e321 = base[3u];
            if (_e321 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_11_7 {
                let _e324 = base[3u];
                if (_e324 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_11_8 {
                    let _e327 = base[3u];
                    if (_e327 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e330 = unnamed.lightColor;
    lightColorRadius = _e330;
    let _e331 = L_1;
    let _e335 = unnamed.lightVector;
    let _e340 = unnamed.lightVector[3u];
    scale_1 = clamp((dot(-(_e331.xyz), _e335.xyz) * _e340), 0f, 1f);
    let _e344 = unnamed.lightVector;
    let _e345 = scale_1;
    let _e347 = L_1;
    LL = ((_e344 * _e345) + _e347);
    let _e349 = LL;
    nL = normalize(_e349.xyz);
    let _e352 = V_1;
    nV = normalize(_e352.xyz);
    let _e355 = LL;
    let _e357 = LL;
    let _e361 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e355.xyz, _e357.xyz) * _e361));
    let _e364 = intensFactor;
    if (_e364 <= 0f) {
        discard;
    }
    let _e366 = lightColorRadius;
    let _e368 = intensFactor;
    intens = (_e366.xyz * _e368);
    let _e370 = base;
    let _e373 = fog[3u];
    let _e375 = (_e370.xyz * (1f - _e373));
    base[0u] = _e375.x;
    base[1u] = _e375.y;
    base[2u] = _e375.z;
    let _e382 = Np;
    let _e383 = nL;
    diffuse = dot(_e382, _e383);
    let _e385 = Np;
    let _e386 = nL;
    let _e387 = nV;
    specFactor = dot(_e385, normalize((_e386 + _e387)));
    if override_type_11_9 {
        let _e391 = diffuse;
        let _e392 = Np;
        let _e393 = nV;
        if ((_e391 * dot(_e392, _e393)) <= 0f) {
            discard;
        }
        let _e397 = diffuse;
        diffuse = abs(_e397);
        let _e399 = specFactor;
        specFactor = abs(_e399);
    } else {
        let _e401 = diffuse;
        diffuse = max(_e401, 0f);
        let _e403 = specFactor;
        specFactor = max(_e403, 0f);
    }
    let _e405 = specFactor;
    let _e409 = base;
    spec = ((vec4((pow(_e405, 10f) * 0.25f)) * _e409) * 0.8f);
    let _e412 = base;
    let _e413 = diffuse;
    let _e416 = spec;
    let _e418 = intens;
    out_color = (((_e412 * vec4(_e413)) + _e416) * vec4<f32>(_e418.x, _e418.y, _e418.z, 1f));
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(5) frag_position: vec3<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(1) N: vec3<f32>, @location(3) V: vec4<f32>, @location(2) L: vec4<f32>) -> @location(0) vec4<f32> {
    fog_tex_coord_1 = fog_tex_coord;
    frag_position_1 = frag_position;
    frag_tex_coord_1 = frag_tex_coord;
    N_1 = N;
    V_1 = V;
    L_1 = L;
    main_1();
    let _e13 = out_color;
    return _e13;
}
