struct MenuBgBlock {
    time: f32,
    mouseX: f32,
    mouseY: f32,
    transition: f32,
    resX: f32,
    resY: f32,
    pad0_: f32,
    pad1_: f32,
}

@group(2) @binding(0)
var<uniform> u: MenuBgBlock;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn hash11_u0028_f1_u003b(n: ptr<function, f32>) -> f32 {
    let _e98 = (*n);
    return fract((sin(_e98) * 43758.547f));
}

fn segDist_u0028_vf2_u003b_vf2_u003b_vf2_u003b(p: ptr<function, vec2<f32>>, a: ptr<function, vec2<f32>>, b: ptr<function, vec2<f32>>) -> f32 {
    var pa: vec2<f32>;
    var ba: vec2<f32>;
    var t: f32;

    let _e103 = (*p);
    let _e104 = (*a);
    pa = (_e103 - _e104);
    let _e106 = (*b);
    let _e107 = (*a);
    ba = (_e106 - _e107);
    let _e109 = pa;
    let _e110 = ba;
    let _e112 = ba;
    let _e113 = ba;
    t = clamp((dot(_e109, _e110) / max(dot(_e112, _e113), 0.000001f)), 0f, 1f);
    let _e118 = pa;
    let _e119 = ba;
    let _e120 = t;
    return length((_e118 - (_e119 * _e120)));
}

fn sdTriangle_u0028_vf2_u003b_f1_u003b(p_1: ptr<function, vec2<f32>>, r: ptr<function, f32>) -> f32 {
    let _e100 = (*p_1)[0u];
    let _e102 = (*r);
    (*p_1)[0u] = (abs(_e100) - _e102);
    let _e106 = (*p_1)[1u];
    let _e107 = (*r);
    (*p_1)[1u] = (_e106 + (_e107 / 1.7320508f));
    let _e112 = (*p_1)[0u];
    let _e114 = (*p_1)[1u];
    if ((_e112 + (1.7320508f * _e114)) > 0f) {
        let _e119 = (*p_1)[0u];
        let _e121 = (*p_1)[1u];
        let _e125 = (*p_1)[0u];
        let _e128 = (*p_1)[1u];
        (*p_1) = (vec2<f32>((_e119 - (1.7320508f * _e121)), ((-1.7320508f * _e125) - _e128)) / vec2(2f));
    }
    let _e134 = (*p_1)[0u];
    let _e135 = (*r);
    let _e139 = (*p_1)[0u];
    (*p_1)[0u] = (_e139 - clamp(_e134, (-2f * _e135), 0f));
    let _e142 = (*p_1);
    let _e146 = (*p_1)[1u];
    return (-(length(_e142)) * sign(_e146));
}

fn hash22_u0028_vf2_u003b(p_2: ptr<function, vec2<f32>>) -> vec2<f32> {
    let _e98 = (*p_2);
    let _e100 = (*p_2);
    (*p_2) = vec2<f32>(dot(_e98, vec2<f32>(127.1f, 311.7f)), dot(_e100, vec2<f32>(269.5f, 183.3f)));
    let _e103 = (*p_2);
    return fract((sin(_e103) * 43758.547f));
}

fn cellPoint_u0028_vf2_u003b(cell: ptr<function, vec2<f32>>) -> vec2<f32> {
    var h: vec2<f32>;
    var param: vec2<f32>;
    var sp: f32;
    var ph: vec2<f32>;

    let _e102 = (*cell);
    param = _e102;
    let _e103 = hash22_u0028_vf2_u003b((&param));
    h = _e103;
    let _e105 = h[0u];
    sp = (0.2f + (0.35f * _e105));
    let _e108 = h;
    ph = (_e108 * 6.2831855f);
    let _e111 = u.time;
    let _e112 = sp;
    let _e115 = ph[0u];
    let _e121 = u.time;
    let _e122 = sp;
    let _e126 = ph[1u];
    return vec2<f32>((0.5f + (0.34f * sin(((_e111 * _e112) + _e115)))), (0.5f + (0.34f * cos((((_e121 * _e122) * 0.87f) + _e126)))));
}

fn srgbToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e98 = (*c);
    let _e100 = (*c);
    let _e105 = (*c);
    return mix((_e98 * 0.07739938f), pow(((_e100 + vec3(0.055f)) * 0.9478673f), vec3<f32>(2.4f, 2.4f, 2.4f)), step(vec3<f32>(0.04045f, 0.04045f, 0.04045f), _e105));
}

fn main_1() {
    var SKY_TOP: vec3<f32>;
    var param_1: vec3<f32>;
    var SKY_LOW: vec3<f32>;
    var param_2: vec3<f32>;
    var FLOOR_COL: vec3<f32>;
    var param_3: vec3<f32>;
    var EMBER: vec3<f32>;
    var param_4: vec3<f32>;
    var STAR_COL: vec3<f32>;
    var param_5: vec3<f32>;
    var res: vec2<f32>;
    var aspect: f32;
    var uv: vec2<f32>;
    var asp: vec2<f32>;
    var par: vec2<f32>;
    var skyY: f32;
    var g: f32;
    var col: vec3<f32>;
    var floorMix: f32;
    var guv: vec2<f32>;
    var gp: vec2<f32>;
    var glow: f32;
    var suv: vec2<f32>;
    var cell_1: vec2<f32>;
    var f: vec2<f32>;
    var lineAcc: f32;
    var dotAcc: f32;
    var oy: i32;
    var ox: i32;
    var nCell: vec2<f32>;
    var pN: vec2<f32>;
    var param_6: vec2<f32>;
    var d: vec2<f32>;
    var dist: f32;
    var hh: vec2<f32>;
    var param_7: vec2<f32>;
    var ptype: f32;
    var palpha: f32;
    var aa: f32;
    var r_1: f32;
    var td: f32;
    var param_8: vec2<f32>;
    var param_9: f32;
    var bar: f32;
    var pCenter: vec2<f32>;
    var param_10: vec2<f32>;
    var pNbr: vec2<f32>;
    var param_11: vec2<f32>;
    var sep: f32;
    var fade: f32;
    var ld: f32;
    var param_12: vec2<f32>;
    var param_13: vec2<f32>;
    var param_14: vec2<f32>;
    var law: f32;
    var line: f32;
    var vp: vec2<f32>;
    var vig: f32;
    var dnoise: f32;
    var param_15: f32;
    var phi_270_: bool;

    param_1 = vec3<f32>(0.227f, 0.094f, 0.031f);
    let _e157 = srgbToLinear_u0028_vf3_u003b((&param_1));
    SKY_TOP = _e157;
    param_2 = vec3<f32>(0.102f, 0.035f, 0.02f);
    let _e158 = srgbToLinear_u0028_vf3_u003b((&param_2));
    SKY_LOW = _e158;
    param_3 = vec3<f32>(0.055f, 0.026f, 0.016f);
    let _e159 = srgbToLinear_u0028_vf3_u003b((&param_3));
    FLOOR_COL = _e159;
    param_4 = vec3<f32>(0.957f, 0.627f, 0.227f);
    let _e160 = srgbToLinear_u0028_vf3_u003b((&param_4));
    EMBER = _e160;
    param_5 = vec3<f32>(0.784f, 0.251f, 0.188f);
    let _e161 = srgbToLinear_u0028_vf3_u003b((&param_5));
    STAR_COL = _e161;
    let _e163 = u.resX;
    let _e165 = u.resY;
    res = vec2<f32>(_e163, _e165);
    let _e168 = res[0u];
    let _e169 = (_e168 < 1f);
    phi_270_ = _e169;
    if !(_e169) {
        let _e172 = res[1u];
        phi_270_ = (_e172 < 1f);
    }
    let _e175 = phi_270_;
    if _e175 {
        res = vec2<f32>(1280f, 720f);
    }
    let _e177 = res[0u];
    let _e179 = res[1u];
    aspect = (_e177 / _e179);
    let _e181 = frag_tex_coord_1;
    uv = _e181;
    let _e182 = aspect;
    asp = vec2<f32>(_e182, 1f);
    let _e185 = u.mouseX;
    let _e188 = u.transition;
    let _e192 = u.mouseY;
    par = vec2<f32>(((_e185 * 0.02f) + (_e188 * 0.03f)), (_e192 * 0.014f));
    let _e196 = uv[1u];
    let _e198 = par[1u];
    skyY = (_e196 + (_e198 * 0.1f));
    let _e201 = skyY;
    g = smoothstep(0f, 0.62f, _e201);
    let _e203 = SKY_TOP;
    let _e204 = SKY_LOW;
    let _e205 = g;
    col = mix(_e203, _e204, vec3(_e205));
    let _e208 = skyY;
    floorMix = smoothstep(0.6f, 1f, _e208);
    let _e210 = col;
    let _e211 = FLOOR_COL;
    let _e212 = floorMix;
    col = mix(_e210, _e211, vec3(_e212));
    let _e215 = uv;
    let _e216 = par;
    guv = (_e215 + (_e216 * 0.42f));
    let _e219 = guv;
    let _e221 = asp;
    gp = ((_e219 - vec2<f32>(0.5f, 0.66f)) * _e221);
    let _e223 = gp;
    let _e224 = gp;
    glow = exp((-(dot(_e223, _e224)) * 6.5f));
    let _e229 = guv;
    let _e231 = asp;
    let _e233 = guv;
    let _e235 = asp;
    let _e242 = glow;
    glow = (_e242 + (0.35f * exp((-(dot(((_e229 - vec2<f32>(0.5f, 0.3f)) * _e231), ((_e233 - vec2<f32>(0.5f, 0.3f)) * _e235))) * 9f))));
    let _e245 = u.time;
    let _e250 = glow;
    glow = (_e250 * (0.85f + (0.15f * sin((_e245 * 0.6f)))));
    let _e252 = EMBER;
    let _e253 = glow;
    let _e256 = col;
    col = (_e256 + ((_e252 * _e253) * 0.11f));
    let _e258 = uv;
    let _e259 = par;
    let _e262 = asp;
    suv = (((_e258 + (_e259 * 1f)) * _e262) * 7f);
    let _e265 = suv;
    cell_1 = floor(_e265);
    let _e267 = suv;
    f = fract(_e267);
    lineAcc = 0f;
    dotAcc = 0f;
    oy = -1i;
    loop {
        let _e269 = oy;
        if (_e269 <= 1i) {
            ox = -1i;
            loop {
                let _e271 = ox;
                if (_e271 <= 1i) {
                    let _e273 = cell_1;
                    let _e274 = ox;
                    let _e276 = oy;
                    nCell = (_e273 + vec2<f32>(f32(_e274), f32(_e276)));
                    let _e280 = ox;
                    let _e282 = oy;
                    let _e285 = nCell;
                    param_6 = _e285;
                    let _e286 = cellPoint_u0028_vf2_u003b((&param_6));
                    pN = (vec2<f32>(f32(_e280), f32(_e282)) + _e286);
                    let _e288 = pN;
                    let _e289 = f;
                    d = (_e288 - _e289);
                    let _e291 = d;
                    dist = length(_e291);
                    let _e293 = nCell;
                    param_7 = (_e293 + vec2(3.17f));
                    let _e296 = hash22_u0028_vf2_u003b((&param_7));
                    hh = _e296;
                    let _e298 = hh[0u];
                    ptype = floor((_e298 * 3f));
                    let _e302 = hh[1u];
                    palpha = (0.35f + (0.45f * _e302));
                    let _e305 = dist;
                    let _e306 = fwidth(_e305);
                    aa = (_e306 + 0.0001f);
                    let _e308 = ptype;
                    if (_e308 < 0.5f) {
                        r_1 = 0.045f;
                        let _e310 = palpha;
                        let _e311 = r_1;
                        let _e312 = aa;
                        let _e314 = r_1;
                        let _e315 = aa;
                        let _e317 = dist;
                        let _e321 = dotAcc;
                        dotAcc = (_e321 + (_e310 * (1f - smoothstep((_e311 - _e312), (_e314 + _e315), _e317))));
                    } else {
                        let _e323 = ptype;
                        if (_e323 < 1.5f) {
                            let _e325 = d;
                            param_8 = _e325;
                            param_9 = 0.075f;
                            let _e326 = sdTriangle_u0028_vf2_u003b_f1_u003b((&param_8), (&param_9));
                            td = (abs(_e326) - 0.006f);
                            let _e329 = palpha;
                            let _e330 = aa;
                            let _e332 = aa;
                            let _e333 = td;
                            let _e337 = dotAcc;
                            dotAcc = (_e337 + (_e329 * (1f - smoothstep(-(_e330), _e332, _e333))));
                        } else {
                            let _e340 = d[0u];
                            let _e344 = d[1u];
                            let _e349 = d[1u];
                            let _e353 = d[0u];
                            bar = min(max((abs(_e340) - 0.01f), (abs(_e344) - 0.06f)), max((abs(_e349) - 0.01f), (abs(_e353) - 0.06f)));
                            let _e358 = palpha;
                            let _e359 = aa;
                            let _e361 = aa;
                            let _e362 = bar;
                            let _e366 = dotAcc;
                            dotAcc = (_e366 + (_e358 * (1f - smoothstep(-(_e359), _e361, _e362))));
                        }
                    }
                    let _e368 = ox;
                    let _e370 = oy;
                    if !(((_e368 == 0i) && (_e370 == 0i))) {
                        let _e374 = cell_1;
                        param_10 = _e374;
                        let _e375 = cellPoint_u0028_vf2_u003b((&param_10));
                        pCenter = _e375;
                        let _e376 = ox;
                        let _e378 = oy;
                        let _e381 = nCell;
                        param_11 = _e381;
                        let _e382 = cellPoint_u0028_vf2_u003b((&param_11));
                        pNbr = (vec2<f32>(f32(_e376), f32(_e378)) + _e382);
                        let _e384 = pNbr;
                        let _e385 = pCenter;
                        sep = length((_e384 - _e385));
                        let _e388 = sep;
                        fade = (1f - smoothstep(0.55f, 1.15f, _e388));
                        let _e391 = fade;
                        if (_e391 > 0.001f) {
                            let _e393 = f;
                            param_12 = _e393;
                            let _e394 = pCenter;
                            param_13 = _e394;
                            let _e395 = pNbr;
                            param_14 = _e395;
                            let _e396 = segDist_u0028_vf2_u003b_vf2_u003b_vf2_u003b((&param_12), (&param_13), (&param_14));
                            ld = _e396;
                            let _e397 = ld;
                            let _e398 = fwidth(_e397);
                            law = (_e398 + 0.0001f);
                            let _e400 = law;
                            let _e402 = ld;
                            line = (1f - smoothstep(0f, (0.006f + _e400), _e402));
                            let _e405 = line;
                            let _e406 = fade;
                            let _e409 = lineAcc;
                            lineAcc = (_e409 + ((_e405 * _e406) * 0.5f));
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e411 = ox;
                    ox = (_e411 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e413 = oy;
            oy = (_e413 + 1i);
        }
    }
    let _e415 = STAR_COL;
    let _e416 = lineAcc;
    let _e420 = col;
    col = (_e420 + ((_e415 * clamp(_e416, 0f, 1f)) * 0.22f));
    let _e422 = STAR_COL;
    let _e423 = dotAcc;
    let _e427 = col;
    col = (_e427 + ((_e422 * clamp(_e423, 0f, 1f)) * 0.55f));
    let _e429 = uv;
    let _e432 = asp;
    vp = ((_e429 - vec2(0.5f)) * _e432);
    let _e434 = vp;
    vig = smoothstep(1.05f, 0.35f, length(_e434));
    let _e437 = vig;
    let _e439 = col;
    col = (_e439 * mix(0.55f, 1f, _e437));
    let _e441 = gl_FragCoord_1;
    param_15 = dot(_e441.xy, vec2<f32>(12.9898f, 78.233f));
    let _e444 = hash11_u0028_f1_u003b((&param_15));
    dnoise = _e444;
    let _e445 = dnoise;
    let _e448 = col;
    col = (_e448 + vec3(((_e445 - 0.5f) * 0.003921569f)));
    let _e451 = col;
    let _e452 = max(_e451, vec3<f32>(0f, 0f, 0f));
    out_color = vec4<f32>(_e452.x, _e452.y, _e452.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>, @builtin(position) gl_FragCoord: vec4<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    gl_FragCoord_1 = gl_FragCoord;
    main_1();
    let _e5 = out_color;
    return _e5;
}
