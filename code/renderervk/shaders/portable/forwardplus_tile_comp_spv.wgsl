struct Push {
    invViewProj: mat4x4<f32>,
    params0_: vec4<f32>,
    params1_: vec4<f32>,
}

struct TileDepth {
    tileDepth: array<f32>,
}

struct Light {
    posRadius: vec4<f32>,
    color: vec4<f32>,
    posRadius2_: vec4<f32>,
}

struct Lights {
    lights: array<Light>,
}

struct TileLights {
    tileLights: array<u32>,
}

@group(0) @binding(3)
var<uniform> unnamed: Push;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(1) 
var<storage> unnamed_1: TileDepth;
@group(0) @binding(0) 
var<storage> unnamed_2: Lights;
@group(0) @binding(2) 
var<storage, read_write> unnamed_3: TileLights;

fn planeDist_u0028_vf4_u003b_vf3_u003b(pl: ptr<function, vec4<f32>>, p: ptr<function, vec3<f32>>) -> f32 {
    let _e36 = (*pl);
    let _e38 = (*p);
    let _e41 = (*pl)[3u];
    return (dot(_e36.xyz, _e38) + _e41);
}

fn planeFromPoints_u0028_vf3_u003b_vf3_u003b_vf3_u003b(a: ptr<function, vec3<f32>>, b: ptr<function, vec3<f32>>, c: ptr<function, vec3<f32>>) -> vec4<f32> {
    var n: vec3<f32>;

    let _e38 = (*b);
    let _e39 = (*a);
    let _e41 = (*c);
    let _e42 = (*a);
    n = normalize(cross((_e38 - _e39), (_e41 - _e42)));
    let _e46 = n;
    let _e47 = n;
    let _e48 = (*a);
    return vec4<f32>(_e46.x, _e46.y, _e46.z, -(dot(_e47, _e48)));
}

fn unproject_u0028_vf3_u003b(ndc: ptr<function, vec3<f32>>) -> vec3<f32> {
    var w: vec4<f32>;

    let _e37 = unnamed.invViewProj;
    let _e38 = (*ndc);
    w = (_e37 * vec4<f32>(_e38.x, _e38.y, _e38.z, 1f));
    let _e44 = w;
    let _e47 = w[3u];
    return (_e44.xyz / vec3(_e47));
}

fn main_1() {
    var tx: u32;
    var ty: u32;
    var tilesX: u32;
    var tilesY: u32;
    var tileIndex: u32;
    var outBase: u32;
    var screenW: f32;
    var screenH: f32;
    var numLights: u32;
    var depthValid: bool;
    var x0_: f32;
    var y0_: f32;
    var x1_: f32;
    var y1_: f32;
    var ndcX0_: f32;
    var ndcX1_: f32;
    var ndcY0_: f32;
    var ndcY1_: f32;
    var dMin: f32;
    var dMax: f32;
    var c_1: array<vec3<f32>, 8>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: vec3<f32>;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;
    var param_7: vec3<f32>;
    var tileCentroid: vec3<f32>;
    var i: i32;
    var planes: array<vec4<f32>, 6>;
    var param_8: vec3<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;
    var param_15: vec3<f32>;
    var param_16: vec3<f32>;
    var param_17: vec3<f32>;
    var param_18: vec3<f32>;
    var param_19: vec3<f32>;
    var param_20: vec3<f32>;
    var param_21: vec3<f32>;
    var param_22: vec3<f32>;
    var param_23: vec3<f32>;
    var param_24: vec3<f32>;
    var param_25: vec3<f32>;
    var i_1: i32;
    var param_26: vec4<f32>;
    var param_27: vec3<f32>;
    var count: u32;
    var cap: u32;
    var li: u32;
    var lp: vec3<f32>;
    var lr: f32;
    var center: vec3<f32>;
    var radius: f32;
    var lp2_: vec3<f32>;
    var inside: bool;
    var p_1: i32;
    var param_28: vec4<f32>;
    var param_29: vec3<f32>;

    let _e100 = gl_GlobalInvocationID_1[0u];
    tx = _e100;
    let _e102 = gl_GlobalInvocationID_1[1u];
    ty = _e102;
    let _e105 = unnamed.params0_[2u];
    tilesX = u32(_e105);
    let _e109 = unnamed.params0_[3u];
    tilesY = u32(_e109);
    let _e111 = tx;
    let _e112 = tilesX;
    let _e114 = ty;
    let _e115 = tilesY;
    if ((_e111 >= _e112) || (_e114 >= _e115)) {
        return;
    }
    let _e118 = ty;
    let _e119 = tilesX;
    let _e121 = tx;
    tileIndex = ((_e118 * _e119) + _e121);
    let _e123 = tileIndex;
    outBase = (_e123 * 33u);
    let _e127 = unnamed.params0_[0u];
    screenW = _e127;
    let _e130 = unnamed.params0_[1u];
    screenH = _e130;
    let _e133 = unnamed.params1_[0u];
    numLights = u32(_e133);
    let _e137 = unnamed.params1_[1u];
    depthValid = (_e137 != 0f);
    let _e139 = tx;
    x0_ = f32((_e139 * 16u));
    let _e142 = ty;
    y0_ = f32((_e142 * 16u));
    let _e145 = x0_;
    let _e147 = screenW;
    x1_ = min((_e145 + 16f), _e147);
    let _e149 = y0_;
    let _e151 = screenH;
    y1_ = min((_e149 + 16f), _e151);
    let _e153 = x0_;
    let _e154 = screenW;
    ndcX0_ = (((_e153 / _e154) * 2f) - 1f);
    let _e158 = x1_;
    let _e159 = screenW;
    ndcX1_ = (((_e158 / _e159) * 2f) - 1f);
    let _e163 = y0_;
    let _e164 = screenH;
    ndcY0_ = (((_e163 / _e164) * 2f) - 1f);
    let _e168 = y1_;
    let _e169 = screenH;
    ndcY1_ = (((_e168 / _e169) * 2f) - 1f);
    dMin = 0f;
    dMax = 1f;
    let _e173 = depthValid;
    if _e173 {
        let _e174 = tileIndex;
        let _e179 = unnamed_1.tileDepth[((2u * _e174) + 0u)];
        dMin = _e179;
        let _e180 = tileIndex;
        let _e185 = unnamed_1.tileDepth[((2u * _e180) + 1u)];
        dMax = _e185;
        let _e186 = dMax;
        let _e187 = dMin;
        if (_e186 < _e187) {
            dMin = 0f;
            dMax = 1f;
        }
    }
    let _e189 = ndcX0_;
    let _e190 = ndcY0_;
    let _e191 = dMax;
    param = vec3<f32>(_e189, _e190, _e191);
    let _e193 = unproject_u0028_vf3_u003b((&param));
    c_1[0i] = _e193;
    let _e195 = ndcX1_;
    let _e196 = ndcY0_;
    let _e197 = dMax;
    param_1 = vec3<f32>(_e195, _e196, _e197);
    let _e199 = unproject_u0028_vf3_u003b((&param_1));
    c_1[1i] = _e199;
    let _e201 = ndcX1_;
    let _e202 = ndcY1_;
    let _e203 = dMax;
    param_2 = vec3<f32>(_e201, _e202, _e203);
    let _e205 = unproject_u0028_vf3_u003b((&param_2));
    c_1[2i] = _e205;
    let _e207 = ndcX0_;
    let _e208 = ndcY1_;
    let _e209 = dMax;
    param_3 = vec3<f32>(_e207, _e208, _e209);
    let _e211 = unproject_u0028_vf3_u003b((&param_3));
    c_1[3i] = _e211;
    let _e213 = ndcX0_;
    let _e214 = ndcY0_;
    let _e215 = dMin;
    param_4 = vec3<f32>(_e213, _e214, _e215);
    let _e217 = unproject_u0028_vf3_u003b((&param_4));
    c_1[4i] = _e217;
    let _e219 = ndcX1_;
    let _e220 = ndcY0_;
    let _e221 = dMin;
    param_5 = vec3<f32>(_e219, _e220, _e221);
    let _e223 = unproject_u0028_vf3_u003b((&param_5));
    c_1[5i] = _e223;
    let _e225 = ndcX1_;
    let _e226 = ndcY1_;
    let _e227 = dMin;
    param_6 = vec3<f32>(_e225, _e226, _e227);
    let _e229 = unproject_u0028_vf3_u003b((&param_6));
    c_1[6i] = _e229;
    let _e231 = ndcX0_;
    let _e232 = ndcY1_;
    let _e233 = dMin;
    param_7 = vec3<f32>(_e231, _e232, _e233);
    let _e235 = unproject_u0028_vf3_u003b((&param_7));
    c_1[7i] = _e235;
    tileCentroid = vec3<f32>(0f, 0f, 0f);
    i = 0i;
    loop {
        let _e237 = i;
        if (_e237 < 8i) {
            let _e239 = i;
            let _e241 = c_1[_e239];
            let _e242 = tileCentroid;
            tileCentroid = (_e242 + _e241);
            continue;
        } else {
            break;
        }
        continuing {
            let _e244 = i;
            i = (_e244 + 1i);
        }
    }
    let _e246 = tileCentroid;
    tileCentroid = (_e246 * 0.125f);
    let _e249 = c_1[0i];
    param_8 = _e249;
    let _e251 = c_1[3i];
    param_9 = _e251;
    let _e253 = c_1[4i];
    param_10 = _e253;
    let _e254 = planeFromPoints_u0028_vf3_u003b_vf3_u003b_vf3_u003b((&param_8), (&param_9), (&param_10));
    planes[0i] = _e254;
    let _e257 = c_1[1i];
    param_11 = _e257;
    let _e259 = c_1[5i];
    param_12 = _e259;
    let _e261 = c_1[2i];
    param_13 = _e261;
    let _e262 = planeFromPoints_u0028_vf3_u003b_vf3_u003b_vf3_u003b((&param_11), (&param_12), (&param_13));
    planes[1i] = _e262;
    let _e265 = c_1[0i];
    param_14 = _e265;
    let _e267 = c_1[4i];
    param_15 = _e267;
    let _e269 = c_1[1i];
    param_16 = _e269;
    let _e270 = planeFromPoints_u0028_vf3_u003b_vf3_u003b_vf3_u003b((&param_14), (&param_15), (&param_16));
    planes[2i] = _e270;
    let _e273 = c_1[3i];
    param_17 = _e273;
    let _e275 = c_1[2i];
    param_18 = _e275;
    let _e277 = c_1[7i];
    param_19 = _e277;
    let _e278 = planeFromPoints_u0028_vf3_u003b_vf3_u003b_vf3_u003b((&param_17), (&param_18), (&param_19));
    planes[3i] = _e278;
    let _e281 = c_1[0i];
    param_20 = _e281;
    let _e283 = c_1[1i];
    param_21 = _e283;
    let _e285 = c_1[3i];
    param_22 = _e285;
    let _e286 = planeFromPoints_u0028_vf3_u003b_vf3_u003b_vf3_u003b((&param_20), (&param_21), (&param_22));
    planes[4i] = _e286;
    let _e289 = c_1[4i];
    param_23 = _e289;
    let _e291 = c_1[7i];
    param_24 = _e291;
    let _e293 = c_1[5i];
    param_25 = _e293;
    let _e294 = planeFromPoints_u0028_vf3_u003b_vf3_u003b_vf3_u003b((&param_23), (&param_24), (&param_25));
    planes[5i] = _e294;
    i_1 = 0i;
    loop {
        let _e296 = i_1;
        if (_e296 < 6i) {
            let _e298 = i_1;
            let _e300 = planes[_e298];
            param_26 = _e300;
            let _e301 = tileCentroid;
            param_27 = _e301;
            let _e302 = planeDist_u0028_vf4_u003b_vf3_u003b((&param_26), (&param_27));
            if (_e302 < 0f) {
                let _e304 = i_1;
                let _e305 = i_1;
                let _e307 = planes[_e305];
                planes[_e304] = -(_e307);
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e310 = i_1;
            i_1 = (_e310 + 1i);
        }
    }
    count = 0u;
    cap = 32u;
    li = 0u;
    loop {
        let _e312 = li;
        let _e313 = numLights;
        let _e315 = li;
        if ((_e312 < _e313) && (_e315 < 256u)) {
            let _e318 = li;
            let _e322 = unnamed_2.lights[_e318].posRadius;
            lp = _e322.xyz;
            let _e324 = li;
            let _e329 = unnamed_2.lights[_e324].posRadius[3u];
            lr = _e329;
            let _e330 = lr;
            if (_e330 <= 0f) {
                continue;
            }
            let _e332 = lp;
            center = _e332;
            let _e333 = lr;
            radius = _e333;
            let _e334 = li;
            let _e339 = unnamed_2.lights[_e334].posRadius2_[3u];
            if (_e339 != 0f) {
                let _e341 = li;
                let _e345 = unnamed_2.lights[_e341].posRadius2_;
                lp2_ = _e345.xyz;
                let _e347 = lp;
                let _e348 = lp2_;
                center = ((_e347 + _e348) * 0.5f);
                let _e351 = lr;
                let _e352 = lp2_;
                let _e353 = lp;
                radius = (_e351 + (0.5f * length((_e352 - _e353))));
            }
            inside = true;
            p_1 = 0i;
            loop {
                let _e358 = p_1;
                if (_e358 < 6i) {
                    let _e360 = p_1;
                    let _e362 = planes[_e360];
                    param_28 = _e362;
                    let _e363 = center;
                    param_29 = _e363;
                    let _e364 = planeDist_u0028_vf4_u003b_vf3_u003b((&param_28), (&param_29));
                    let _e365 = radius;
                    if (_e364 < -(_e365)) {
                        inside = false;
                        break;
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e368 = p_1;
                    p_1 = (_e368 + 1i);
                }
            }
            let _e370 = inside;
            let _e371 = count;
            let _e372 = cap;
            if (_e370 && (_e371 < _e372)) {
                let _e375 = outBase;
                let _e377 = count;
                let _e379 = li;
                unnamed_3.tileLights[((_e375 + 1u) + _e377)] = _e379;
                let _e382 = count;
                count = (_e382 + bitcast<u32>(1i));
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e385 = li;
            li = (_e385 + bitcast<u32>(1i));
        }
    }
    let _e388 = outBase;
    let _e389 = count;
    unnamed_3.tileLights[_e388] = _e389;
    return;
}

@compute @workgroup_size(8, 8, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
