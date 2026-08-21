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
    var nL: vec3<f32>;
    var nV: vec3<f32>;
    var intensFactor: f32;
    var intens: vec3<f32>;
    var diffuse: f32;
    var specFactor: f32;
    var spec: vec4<f32>;

    let _e107 = unnamed.packed_indices[0i][1u];
    let _e113 = unnamed.packed_indices[0i][1u];
    let _e118 = fog_tex_coord_1;
    let _e119 = textureSample(wired_bindless_images[(_e107 & 4095u)], wired_bindless_samplers[((_e113 >> bitcast<u32>(12i)) & 255u)], _e118);
    fog = _e119;
    let _e120 = frag_position_1;
    let _e121 = dpdx(_e120);
    dp1_ = _e121;
    let _e122 = frag_position_1;
    let _e123 = dpdy(_e122);
    dp2_ = _e123;
    let _e124 = frag_tex_coord_1;
    let _e125 = dpdx(_e124);
    duv1_ = _e125;
    let _e126 = frag_tex_coord_1;
    let _e127 = dpdy(_e126);
    duv2_ = _e127;
    let _e128 = dp2_;
    let _e129 = N_1;
    dp2perp = cross(_e128, _e129);
    let _e131 = N_1;
    let _e132 = dp1_;
    dp1perp = cross(_e131, _e132);
    let _e134 = dp2perp;
    let _e136 = duv1_[0u];
    let _e138 = dp1perp;
    let _e140 = duv2_[0u];
    T = ((_e134 * _e136) + (_e138 * _e140));
    let _e143 = dp2perp;
    let _e145 = duv1_[1u];
    let _e147 = dp1perp;
    let _e149 = duv2_[1u];
    B = ((_e143 * _e145) + (_e147 * _e149));
    let _e152 = T;
    let _e153 = T;
    let _e155 = B;
    let _e156 = B;
    invmax = inverseSqrt(max(dot(_e152, _e153), dot(_e155, _e156)));
    let _e160 = T;
    let _e161 = invmax;
    let _e162 = (_e160 * _e161);
    let _e163 = B;
    let _e164 = invmax;
    let _e165 = (_e163 * _e164);
    let _e166 = N_1;
    TBN = mat3x3<f32>(vec3<f32>(_e162.x, _e162.y, _e162.z), vec3<f32>(_e165.x, _e165.y, _e165.z), vec3<f32>(_e166.x, _e166.y, _e166.z));
    let _e180 = TBN;
    let _e182 = V_1;
    viewTS = normalize((transpose(_e180) * normalize(_e182.xyz)));
    layerDepth = (1f / f32(parallax_steps));
    currentDepth = 0f;
    let _e189 = viewTS;
    let _e193 = viewTS[2u];
    deltaUV = ((_e189.xy * parallax_scale) / vec2((_e193 + 0.42f)));
    let _e198 = deltaUV;
    deltaUV = (_e198 / vec2(f32(parallax_steps)));
    let _e201 = frag_tex_coord_1;
    currentUV = _e201;
    let _e205 = unnamed.packed_indices[1i][1u];
    let _e211 = unnamed.packed_indices[1i][1u];
    let _e216 = currentUV;
    let _e217 = textureSample(wired_bindless_images[(_e205 & 4095u)], wired_bindless_samplers[((_e211 >> bitcast<u32>(12i)) & 255u)], _e216);
    currentMapDepth = (1f - _e217.w);
    i = 0i;
    loop {
        let _e220 = i;
        let _e222 = currentDepth;
        let _e223 = currentMapDepth;
        if ((_e220 < parallax_steps) && (_e222 < _e223)) {
            let _e226 = deltaUV;
            let _e227 = currentUV;
            currentUV = (_e227 - _e226);
            let _e232 = unnamed.packed_indices[1i][1u];
            let _e238 = unnamed.packed_indices[1i][1u];
            let _e243 = currentUV;
            let _e244 = textureSample(wired_bindless_images[(_e232 & 4095u)], wired_bindless_samplers[((_e238 >> bitcast<u32>(12i)) & 255u)], _e243);
            currentMapDepth = (1f - _e244.w);
            let _e247 = layerDepth;
            let _e248 = currentDepth;
            currentDepth = (_e248 + _e247);
            continue;
        } else {
            break;
        }
        continuing {
            let _e250 = i;
            i = (_e250 + 1i);
        }
    }
    i_1 = 0i;
    loop {
        let _e252 = i_1;
        if (_e252 < 2i) {
            let _e254 = deltaUV;
            deltaUV = (_e254 * 0.5f);
            let _e256 = layerDepth;
            layerDepth = (_e256 * 0.5f);
            let _e258 = currentDepth;
            let _e259 = currentMapDepth;
            if (_e258 > _e259) {
                let _e261 = deltaUV;
                let _e262 = currentUV;
                currentUV = (_e262 + _e261);
                let _e264 = layerDepth;
                let _e265 = currentDepth;
                currentDepth = (_e265 - _e264);
            } else {
                let _e267 = deltaUV;
                let _e268 = currentUV;
                currentUV = (_e268 - _e267);
                let _e270 = layerDepth;
                let _e271 = currentDepth;
                currentDepth = (_e271 + _e270);
            }
            let _e276 = unnamed.packed_indices[1i][1u];
            let _e282 = unnamed.packed_indices[1i][1u];
            let _e287 = currentUV;
            let _e288 = textureSample(wired_bindless_images[(_e276 & 4095u)], wired_bindless_samplers[((_e282 >> bitcast<u32>(12i)) & 255u)], _e287);
            currentMapDepth = (1f - _e288.w);
            continue;
        } else {
            break;
        }
        continuing {
            let _e291 = i_1;
            i_1 = (_e291 + 1i);
        }
    }
    let _e293 = currentUV;
    parallax_tc = _e293;
    param_1 = 5u;
    let _e294 = parallax_tc;
    param_2 = _e294;
    let _e295 = wired_bl_sample_normal_u0028_u1_u003b_vf2_u003b((&param_1), (&param_2));
    mapNormal = _e295;
    let _e296 = TBN;
    let _e297 = mapNormal;
    Np = normalize((_e296 * _e297));
    param_3 = 0u;
    let _e300 = parallax_tc;
    param_4 = _e300;
    param_5 = 0i;
    let _e301 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    base = _e301;
    if override_type_11_2 {
        if override_type_11_3 {
            let _e303 = base[3u];
            base[3u] = select(0f, 1f, (_e303 > 0f));
        } else {
            if override_type_11_4 {
                let _e308 = base[3u];
                param_6 = alpha_test_value;
                param_7 = (1f - _e308);
                let _e310 = frag_tex_coord_1;
                param_8 = _e310;
                let _e311 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_6), (&param_7), (&param_8));
                base[3u] = _e311;
            } else {
                if override_type_11_5 {
                    param_9 = alpha_test_value;
                    let _e314 = base[3u];
                    param_10 = _e314;
                    let _e315 = frag_tex_coord_1;
                    param_11 = _e315;
                    let _e316 = CorrectAlpha_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_9), (&param_10), (&param_11));
                    base[3u] = _e316;
                }
            }
        }
    } else {
        if override_type_11_6 {
            let _e319 = base[3u];
            if (_e319 == alpha_test_value) {
                discard;
            }
        } else {
            if override_type_11_7 {
                let _e322 = base[3u];
                if (_e322 >= alpha_test_value) {
                    discard;
                }
            } else {
                if override_type_11_8 {
                    let _e325 = base[3u];
                    if (_e325 < alpha_test_value) {
                        discard;
                    }
                }
            }
        }
    }
    let _e328 = unnamed.lightColor;
    lightColorRadius = _e328;
    let _e329 = L_1;
    nL = normalize(_e329.xyz);
    let _e332 = V_1;
    nV = normalize(_e332.xyz);
    let _e335 = L_1;
    let _e337 = L_1;
    let _e341 = lightColorRadius[3u];
    intensFactor = (1f - (dot(_e335.xyz, _e337.xyz) * _e341));
    let _e344 = intensFactor;
    if (_e344 <= 0f) {
        discard;
    }
    let _e346 = lightColorRadius;
    let _e348 = intensFactor;
    intens = (_e346.xyz * _e348);
    let _e350 = base;
    let _e353 = fog[3u];
    let _e355 = (_e350.xyz * (1f - _e353));
    base[0u] = _e355.x;
    base[1u] = _e355.y;
    base[2u] = _e355.z;
    let _e362 = Np;
    let _e363 = nL;
    diffuse = dot(_e362, _e363);
    let _e365 = Np;
    let _e366 = nL;
    let _e367 = nV;
    specFactor = dot(_e365, normalize((_e366 + _e367)));
    if override_type_11_9 {
        let _e371 = diffuse;
        let _e372 = Np;
        let _e373 = nV;
        if ((_e371 * dot(_e372, _e373)) <= 0f) {
            discard;
        }
        let _e377 = diffuse;
        diffuse = abs(_e377);
        let _e379 = specFactor;
        specFactor = abs(_e379);
    } else {
        let _e381 = diffuse;
        diffuse = max(_e381, 0f);
        let _e383 = specFactor;
        specFactor = max(_e383, 0f);
    }
    let _e385 = specFactor;
    let _e389 = base;
    spec = ((vec4((pow(_e385, 10f) * 0.25f)) * _e389) * 0.8f);
    let _e392 = base;
    let _e393 = diffuse;
    let _e396 = spec;
    let _e398 = intens;
    out_color = (((_e392 * vec4(_e393)) + _e396) * vec4<f32>(_e398.x, _e398.y, _e398.z, 1f));
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
