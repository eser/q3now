struct TemporalResolvePush {
    extent: vec2<u32>,
    currentEffectiveJitterUv: vec2<f32>,
    previousEffectiveJitterUv: vec2<f32>,
    zNear: f32,
    zFar: f32,
    depthThresholdAbsolute: f32,
    depthThresholdRelative: f32,
    historyWeight: f32,
    historyReadIndex: u32,
    qualityFlags: u32,
}

@group(0) @binding(10)
var<uniform> pc: TemporalResolvePush;
@group(0) @binding(0)
var currentColor: texture_2d<f32>;
@group(0) @binding(6)
var nearestSampler: sampler;
@group(0) @binding(2)
var previousColor: texture_2d<f32>;
@group(0) @binding(3)
var previousLinearDepth: texture_2d<f32>;
@group(0) @binding(4)
var motionVelocity: texture_2d<f32>;
@group(0) @binding(1)
var currentDeviceDepth: texture_2d<f32>;
@group(0) @binding(5)
var motionValidity: texture_2d<f32>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(7)
var resolvedColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(8)
var historyOutColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(9)
var historyOutLinearDepth: texture_storage_2d<r32float,write>;

fn storeLinearDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    let _e70 = pc.zNear;
    let _e72 = pc.zFar;
    let _e75 = pc.zNear;
    let _e76 = (*z);
    let _e78 = pc.zFar;
    let _e80 = pc.zNear;
    return ((_e70 * _e72) / (_e75 + (_e76 * (_e78 - _e80))));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e70 = (*pixel);
    let _e72 = pc.extent;
    bounded = clamp(_e70, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e72) - vec2<i32>(1i, 1i)));
    let _e76 = bounded;
    let _e77 = textureLoad(currentColor, _e76, 0i);
    return _e77;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_108_: bool;

    let _e69 = (*v);
    let _e70 = (*v);
    let _e71 = (_e69 == _e70);
    phi_108_ = _e71;
    if _e71 {
        let _e72 = (*v);
        phi_108_ = (abs(_e72) <= 340282350000000000000000000000000000000f);
    }
    let _e76 = phi_108_;
    return _e76;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e71 = (*c);
    l = dot(_e71, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e73 = l;
    param = _e73;
    let _e74 = finite1_u0028_f1_u003b((&param));
    if !(_e74) {
        return 0f;
    }
    let _e76 = l;
    l = clamp(_e76, 0f, 10000f);
    let _e78 = l;
    let _e79 = l;
    let _e80 = l;
    return (_e78 * inverseSqrt((1f + (_e79 * _e80))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e71 = (*p);
    param_1 = _e71;
    let _e72 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e72.xyz;
    let _e74 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e74;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e72 = (*base);
    param_3 = _e72;
    let _e73 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e74 = (*base);
    let _e75 = (*across);
    param_4 = (_e74 + _e75);
    let _e77 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e73 + _e77));
}

fn fxSpatialAa_u0028_vi2_u003b_vf4_u003b(px: ptr<function, vec2<i32>>, centerColor: ptr<function, vec4<f32>>) -> vec4<f32> {
    var fxM: f32;
    var param_5: vec3<f32>;
    var fxN: f32;
    var param_6: vec2<i32>;
    var fxS: f32;
    var param_7: vec2<i32>;
    var fxW: f32;
    var param_8: vec2<i32>;
    var fxE: f32;
    var param_9: vec2<i32>;
    var fxHi: f32;
    var fxLo: f32;
    var fxSpan: f32;
    var fxNW: f32;
    var param_10: vec2<i32>;
    var fxNE: f32;
    var param_11: vec2<i32>;
    var fxSW: f32;
    var param_12: vec2<i32>;
    var fxSE: f32;
    var param_13: vec2<i32>;
    var fxNS: f32;
    var fxWE: f32;
    var fxNWSW: f32;
    var fxNESE: f32;
    var fxHorz: f32;
    var fxVert: f32;
    var fxHorzSpan: bool;
    var fxAround: f32;
    var fxLoSide: f32;
    var fxHiSide: f32;
    var fxGradLo: f32;
    var fxGradHi: f32;
    var fxLoSteeper: bool;
    var fxGradient: f32;
    var fxPair: f32;
    var local: f32;
    var fxAcross: vec2<i32>;
    var fxAlong: vec2<i32>;
    var fxInvSpan: f32;
    var fxBlendC: f32;
    var fxBlendF: f32;
    var fxSubpixel: f32;
    var fxHalfPair: f32;
    var fxScaled: f32;
    var fxMidNegative: bool;
    var fxEndN: f32;
    var param_14: vec2<i32>;
    var param_15: vec2<i32>;
    var fxEndP: f32;
    var param_16: vec2<i32>;
    var param_17: vec2<i32>;
    var fxDoneN: bool;
    var fxDoneP: bool;
    var fxStepsN: i32;
    var fxStepsP: i32;
    var i: i32;
    var param_18: vec2<i32>;
    var param_19: vec2<i32>;
    var param_20: vec2<i32>;
    var param_21: vec2<i32>;
    var fxTowardN: bool;
    var fxEndG: f32;
    var fxGoodSpan: bool;
    var fxSpanLen: f32;
    var fxNear: f32;
    var fxEdgeBlend: f32;
    var local_1: f32;
    var fxBlend: f32;
    var fxAcrossColor: vec3<f32>;
    var param_22: vec2<i32>;

    let _e141 = (*centerColor);
    param_5 = _e141.xyz;
    let _e143 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e143;
    let _e144 = (*px);
    param_6 = (_e144 + vec2<i32>(0i, -1i));
    let _e146 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e146;
    let _e147 = (*px);
    param_7 = (_e147 + vec2<i32>(0i, 1i));
    let _e149 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e149;
    let _e150 = (*px);
    param_8 = (_e150 + vec2<i32>(-1i, 0i));
    let _e152 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e152;
    let _e153 = (*px);
    param_9 = (_e153 + vec2<i32>(1i, 0i));
    let _e155 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e155;
    let _e156 = fxN;
    let _e157 = fxS;
    let _e159 = fxW;
    let _e160 = fxE;
    let _e162 = fxM;
    fxHi = max(max(_e156, _e157), max(max(_e159, _e160), _e162));
    let _e165 = fxN;
    let _e166 = fxS;
    let _e168 = fxW;
    let _e169 = fxE;
    let _e171 = fxM;
    fxLo = min(min(_e165, _e166), min(min(_e168, _e169), _e171));
    let _e174 = fxHi;
    let _e175 = fxLo;
    fxSpan = (_e174 - _e175);
    let _e177 = fxSpan;
    let _e178 = fxHi;
    if (_e177 < max(0.0312f, (_e178 * 0.125f))) {
        let _e182 = (*centerColor);
        return _e182;
    }
    let _e183 = (*px);
    param_10 = (_e183 + vec2<i32>(-1i, -1i));
    let _e185 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e185;
    let _e186 = (*px);
    param_11 = (_e186 + vec2<i32>(1i, -1i));
    let _e188 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e188;
    let _e189 = (*px);
    param_12 = (_e189 + vec2<i32>(-1i, 1i));
    let _e191 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e191;
    let _e192 = (*px);
    param_13 = (_e192 + vec2<i32>(1i, 1i));
    let _e194 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e194;
    let _e195 = fxN;
    let _e196 = fxS;
    fxNS = (_e195 + _e196);
    let _e198 = fxW;
    let _e199 = fxE;
    fxWE = (_e198 + _e199);
    let _e201 = fxNW;
    let _e202 = fxSW;
    fxNWSW = (_e201 + _e202);
    let _e204 = fxNE;
    let _e205 = fxSE;
    fxNESE = (_e204 + _e205);
    let _e207 = fxW;
    let _e209 = fxNWSW;
    let _e212 = fxM;
    let _e214 = fxNS;
    let _e218 = fxE;
    let _e220 = fxNESE;
    fxHorz = (abs(((-2f * _e207) + _e209)) + ((abs(((-2f * _e212) + _e214)) * 2f) + abs(((-2f * _e218) + _e220))));
    let _e225 = fxS;
    let _e227 = fxSW;
    let _e228 = fxSE;
    let _e232 = fxM;
    let _e234 = fxWE;
    let _e238 = fxN;
    let _e240 = fxNW;
    let _e241 = fxNE;
    fxVert = (abs(((-2f * _e225) + (_e227 + _e228))) + ((abs(((-2f * _e232) + _e234)) * 2f) + abs(((-2f * _e238) + (_e240 + _e241)))));
    let _e247 = fxHorz;
    let _e248 = fxVert;
    fxHorzSpan = (_e247 >= _e248);
    let _e250 = fxNS;
    let _e251 = fxWE;
    let _e254 = fxNWSW;
    let _e255 = fxNESE;
    fxAround = (((_e250 + _e251) * 2f) + (_e254 + _e255));
    let _e258 = fxHorzSpan;
    let _e259 = fxN;
    let _e260 = fxW;
    fxLoSide = select(_e260, _e259, _e258);
    let _e262 = fxHorzSpan;
    let _e263 = fxS;
    let _e264 = fxE;
    fxHiSide = select(_e264, _e263, _e262);
    let _e266 = fxLoSide;
    let _e267 = fxM;
    fxGradLo = (_e266 - _e267);
    let _e269 = fxHiSide;
    let _e270 = fxM;
    fxGradHi = (_e269 - _e270);
    let _e272 = fxGradLo;
    let _e274 = fxGradHi;
    fxLoSteeper = (abs(_e272) >= abs(_e274));
    let _e277 = fxGradLo;
    let _e279 = fxGradHi;
    fxGradient = max(abs(_e277), abs(_e279));
    let _e282 = fxLoSteeper;
    if _e282 {
        let _e283 = fxLoSide;
        let _e284 = fxM;
        local = (_e283 + _e284);
    } else {
        let _e286 = fxHiSide;
        let _e287 = fxM;
        local = (_e286 + _e287);
    }
    let _e289 = local;
    fxPair = _e289;
    let _e290 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e290));
    let _e293 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e293));
    let _e296 = fxLoSteeper;
    if _e296 {
        let _e297 = fxAcross;
        fxAcross = -(_e297);
    }
    let _e299 = fxSpan;
    let _e300 = fxSpan;
    fxInvSpan = inverseSqrt((_e299 * _e300));
    let _e303 = fxAround;
    let _e305 = fxM;
    let _e308 = fxInvSpan;
    fxBlendC = clamp((abs(((_e303 * 0.083333336f) - _e305)) * _e308), 0f, 1f);
    let _e311 = fxBlendC;
    let _e314 = fxBlendC;
    let _e315 = fxBlendC;
    fxBlendF = (((-2f * _e311) + 3f) * (_e314 * _e315));
    let _e318 = fxBlendF;
    let _e319 = fxBlendF;
    fxSubpixel = ((_e318 * _e319) * 0.75f);
    let _e322 = fxPair;
    fxHalfPair = (_e322 * 0.5f);
    let _e324 = fxGradient;
    fxScaled = (_e324 * 0.25f);
    let _e326 = fxM;
    let _e327 = fxHalfPair;
    fxMidNegative = ((_e326 - _e327) < 0f);
    let _e330 = (*px);
    let _e331 = fxAlong;
    param_14 = (_e330 - _e331);
    let _e333 = fxAcross;
    param_15 = _e333;
    let _e334 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e335 = fxHalfPair;
    fxEndN = (_e334 - _e335);
    let _e337 = (*px);
    let _e338 = fxAlong;
    param_16 = (_e337 + _e338);
    let _e340 = fxAcross;
    param_17 = _e340;
    let _e341 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e342 = fxHalfPair;
    fxEndP = (_e341 - _e342);
    let _e344 = fxEndN;
    let _e346 = fxScaled;
    fxDoneN = (abs(_e344) >= _e346);
    let _e348 = fxEndP;
    let _e350 = fxScaled;
    fxDoneP = (abs(_e348) >= _e350);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e352 = i;
        if (_e352 <= 12i) {
            let _e354 = fxDoneN;
            if !(_e354) {
                let _e356 = (*px);
                let _e357 = fxAlong;
                let _e358 = i;
                param_18 = (_e356 - (_e357 * vec2(_e358)));
                let _e362 = fxAcross;
                param_19 = _e362;
                let _e363 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e364 = fxHalfPair;
                fxEndN = (_e363 - _e364);
                let _e366 = fxEndN;
                let _e368 = fxScaled;
                fxDoneN = (abs(_e366) >= _e368);
                let _e370 = i;
                fxStepsN = _e370;
            }
            let _e371 = fxDoneP;
            if !(_e371) {
                let _e373 = (*px);
                let _e374 = fxAlong;
                let _e375 = i;
                param_20 = (_e373 + (_e374 * vec2(_e375)));
                let _e379 = fxAcross;
                param_21 = _e379;
                let _e380 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e381 = fxHalfPair;
                fxEndP = (_e380 - _e381);
                let _e383 = fxEndP;
                let _e385 = fxScaled;
                fxDoneP = (abs(_e383) >= _e385);
                let _e387 = i;
                fxStepsP = _e387;
            }
            let _e388 = fxDoneN;
            let _e389 = fxDoneP;
            if (_e388 && _e389) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e391 = i;
            i = (_e391 + 1i);
        }
    }
    let _e393 = fxStepsN;
    let _e394 = fxStepsP;
    fxTowardN = (_e393 < _e394);
    let _e396 = fxTowardN;
    let _e397 = fxEndN;
    let _e398 = fxEndP;
    fxEndG = select(_e398, _e397, _e396);
    let _e400 = fxEndG;
    let _e402 = fxMidNegative;
    fxGoodSpan = ((_e400 < 0f) != _e402);
    let _e404 = fxStepsN;
    let _e405 = fxStepsP;
    fxSpanLen = f32((_e404 + _e405));
    let _e408 = fxTowardN;
    let _e409 = fxStepsN;
    let _e410 = fxStepsP;
    fxNear = f32(select(_e410, _e409, _e408));
    let _e413 = fxGoodSpan;
    if _e413 {
        let _e414 = fxNear;
        let _e415 = fxSpanLen;
        let _e416 = fxSpanLen;
        local_1 = (0.5f - (_e414 * inverseSqrt((_e415 * _e416))));
    } else {
        local_1 = 0f;
    }
    let _e421 = local_1;
    fxEdgeBlend = _e421;
    let _e422 = fxEdgeBlend;
    let _e423 = fxSubpixel;
    fxBlend = clamp(max(_e422, _e423), 0f, 1f);
    let _e426 = (*px);
    let _e427 = fxAcross;
    param_22 = (_e426 + _e427);
    let _e429 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e429.xyz;
    let _e431 = (*centerColor);
    let _e433 = fxBlend;
    let _e436 = fxAcrossColor;
    let _e437 = fxBlend;
    let _e439 = ((_e431.xyz * (1f - _e433)) + (_e436 * _e437));
    let _e441 = (*centerColor)[3u];
    return vec4<f32>(_e439.x, _e439.y, _e439.z, _e441);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e75 = (*a);
    oneMinus = (1f - _e75);
    let _e77 = (*x);
    let _e78 = oneMinus;
    left = (_e77 * _e78);
    let _e80 = (*y);
    let _e81 = (*a);
    right = (_e80 * _e81);
    let _e83 = left;
    let _e84 = right;
    result = (_e83 + _e84);
    let _e86 = result;
    return _e86;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e71 = (*weight);
    let _e72 = (*confidence);
    scaled = (_e71 * _e72);
    let _e74 = scaled;
    return _e74;
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_138_: bool;

    let _e69 = (*v_1);
    let _e70 = (*v_1);
    let _e72 = all((_e69 == _e70));
    phi_138_ = _e72;
    if _e72 {
        let _e73 = (*v_1);
        phi_138_ = all((abs(_e73) <= vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_138_;
    return _e78;
}

fn mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b(x_1: ptr<function, vec4<f32>>, y_1: ptr<function, vec4<f32>>, a_1: ptr<function, f32>) -> vec4<f32> {
    var oneMinus_1: f32;
    var left_1: vec4<f32>;
    var right_1: vec4<f32>;
    var result_1: vec4<f32>;

    let _e75 = (*a_1);
    oneMinus_1 = (1f - _e75);
    let _e77 = (*x_1);
    let _e78 = oneMinus_1;
    left_1 = (_e77 * _e78);
    let _e80 = (*y_1);
    let _e81 = (*a_1);
    right_1 = (_e80 * _e81);
    let _e83 = left_1;
    let _e84 = right_1;
    result_1 = (_e83 + _e84);
    let _e86 = result_1;
    return _e86;
}

fn samplePreviousColorLinear_u0028_vf2_u003b(texel: ptr<function, vec2<f32>>) -> vec4<f32> {
    var baseFloat: vec2<f32>;
    var base_1: vec2<i32>;
    var f: vec2<f32>;
    var hi: vec2<i32>;
    var c00_: vec4<f32>;
    var c10_: vec4<f32>;
    var c01_: vec4<f32>;
    var c11_: vec4<f32>;
    var top: vec4<f32>;
    var param_23: vec4<f32>;
    var param_24: vec4<f32>;
    var param_25: f32;
    var bottom: vec4<f32>;
    var param_26: vec4<f32>;
    var param_27: vec4<f32>;
    var param_28: f32;
    var param_29: vec4<f32>;
    var param_30: vec4<f32>;
    var param_31: f32;

    let _e88 = (*texel);
    baseFloat = floor(_e88);
    let _e90 = baseFloat;
    base_1 = vec2<i32>(_e90);
    let _e92 = (*texel);
    let _e93 = baseFloat;
    f = (_e92 - _e93);
    let _e96 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e96) - vec2<i32>(1i, 1i));
    let _e99 = base_1;
    let _e100 = hi;
    let _e102 = textureLoad(previousColor, clamp(_e99, vec2<i32>(0i, 0i), _e100), 0i);
    c00_ = _e102;
    let _e103 = base_1;
    let _e105 = hi;
    let _e107 = textureLoad(previousColor, clamp((_e103 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e105), 0i);
    c10_ = _e107;
    let _e108 = base_1;
    let _e110 = hi;
    let _e112 = textureLoad(previousColor, clamp((_e108 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e110), 0i);
    c01_ = _e112;
    let _e113 = base_1;
    let _e115 = hi;
    let _e117 = textureLoad(previousColor, clamp((_e113 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e115), 0i);
    c11_ = _e117;
    let _e118 = c00_;
    param_23 = _e118;
    let _e119 = c10_;
    param_24 = _e119;
    let _e121 = f[0u];
    param_25 = _e121;
    let _e122 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_23), (&param_24), (&param_25));
    top = _e122;
    let _e123 = c01_;
    param_26 = _e123;
    let _e124 = c11_;
    param_27 = _e124;
    let _e126 = f[0u];
    param_28 = _e126;
    let _e127 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_26), (&param_27), (&param_28));
    bottom = _e127;
    let _e128 = top;
    param_29 = _e128;
    let _e129 = bottom;
    param_30 = _e129;
    let _e131 = f[1u];
    param_31 = _e131;
    let _e132 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_29), (&param_30), (&param_31));
    return _e132;
}

fn mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b(x_2: ptr<function, f32>, y_2: ptr<function, f32>, a_2: ptr<function, f32>) -> f32 {
    var oneMinus_2: f32;
    var left_2: f32;
    var right_2: f32;
    var result_2: f32;

    let _e75 = (*a_2);
    oneMinus_2 = (1f - _e75);
    let _e77 = (*x_2);
    let _e78 = oneMinus_2;
    left_2 = (_e77 * _e78);
    let _e80 = (*y_2);
    let _e81 = (*a_2);
    right_2 = (_e80 * _e81);
    let _e83 = left_2;
    let _e84 = right_2;
    result_2 = (_e83 + _e84);
    let _e86 = result_2;
    return _e86;
}

fn samplePreviousDepthLinear_u0028_vf2_u003b(texel_1: ptr<function, vec2<f32>>) -> f32 {
    var baseFloat_1: vec2<f32>;
    var base_2: vec2<i32>;
    var f_1: vec2<f32>;
    var hi_1: vec2<i32>;
    var d00_: f32;
    var d10_: f32;
    var d01_: f32;
    var d11_: f32;
    var top_1: f32;
    var param_32: f32;
    var param_33: f32;
    var param_34: f32;
    var bottom_1: f32;
    var param_35: f32;
    var param_36: f32;
    var param_37: f32;
    var param_38: f32;
    var param_39: f32;
    var param_40: f32;

    let _e88 = (*texel_1);
    baseFloat_1 = floor(_e88);
    let _e90 = baseFloat_1;
    base_2 = vec2<i32>(_e90);
    let _e92 = (*texel_1);
    let _e93 = baseFloat_1;
    f_1 = (_e92 - _e93);
    let _e96 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e96) - vec2<i32>(1i, 1i));
    let _e99 = base_2;
    let _e100 = hi_1;
    let _e102 = textureLoad(previousLinearDepth, clamp(_e99, vec2<i32>(0i, 0i), _e100), 0i);
    d00_ = _e102.x;
    let _e104 = base_2;
    let _e106 = hi_1;
    let _e108 = textureLoad(previousLinearDepth, clamp((_e104 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e106), 0i);
    d10_ = _e108.x;
    let _e110 = base_2;
    let _e112 = hi_1;
    let _e114 = textureLoad(previousLinearDepth, clamp((_e110 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e112), 0i);
    d01_ = _e114.x;
    let _e116 = base_2;
    let _e118 = hi_1;
    let _e120 = textureLoad(previousLinearDepth, clamp((_e116 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e118), 0i);
    d11_ = _e120.x;
    let _e122 = d00_;
    param_32 = _e122;
    let _e123 = d10_;
    param_33 = _e123;
    let _e125 = f_1[0u];
    param_34 = _e125;
    let _e126 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_32), (&param_33), (&param_34));
    top_1 = _e126;
    let _e127 = d01_;
    param_35 = _e127;
    let _e128 = d11_;
    param_36 = _e128;
    let _e130 = f_1[0u];
    param_37 = _e130;
    let _e131 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_35), (&param_36), (&param_37));
    bottom_1 = _e131;
    let _e132 = top_1;
    param_38 = _e132;
    let _e133 = bottom_1;
    param_39 = _e133;
    let _e135 = f_1[1u];
    param_40 = _e135;
    let _e136 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_38), (&param_39), (&param_40));
    return _e136;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator: f32;
    var numerator: f32;
    var result_3: f32;

    let _e75 = pc.zFar;
    let _e77 = pc.zNear;
    range = (_e75 - _e77);
    let _e79 = (*z_1);
    let _e80 = range;
    scaled_1 = (_e79 * _e80);
    let _e83 = pc.zNear;
    let _e84 = scaled_1;
    denominator = (_e83 + _e84);
    let _e87 = pc.zNear;
    let _e89 = pc.zFar;
    numerator = (_e87 * _e89);
    let _e91 = numerator;
    let _e92 = denominator;
    result_3 = (_e91 / _e92);
    let _e94 = result_3;
    return _e94;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_123_: bool;

    let _e69 = (*v_2);
    let _e70 = (*v_2);
    let _e72 = all((_e69 == _e70));
    phi_123_ = _e72;
    if _e72 {
        let _e73 = (*v_2);
        phi_123_ = all((abs(_e73) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_123_;
    return _e78;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_153_: bool;

    let _e69 = (*v_3);
    let _e70 = (*v_3);
    let _e72 = all((_e69 == _e70));
    phi_153_ = _e72;
    if _e72 {
        let _e73 = (*v_3);
        phi_153_ = all((abs(_e73) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e78 = phi_153_;
    return _e78;
}

fn dlMotionVelocity_u0028_vi2_u003b_f1_u003b(px_1: ptr<function, vec2<i32>>, centerValid: ptr<function, f32>) -> vec2<f32> {
    var centerVelocity: vec2<f32>;
    var param_41: vec2<f32>;
    var hi_2: vec2<i32>;
    var best: vec2<i32>;
    var bestDepth: f32;
    var y_3: i32;
    var x_3: i32;
    var q: vec2<i32>;
    var d: f32;
    var v_4: f32;
    var dv: vec2<f32>;
    var param_42: vec2<f32>;
    var phi_963_: bool;
    var phi_971_: bool;

    let _e82 = (*px_1);
    let _e83 = textureLoad(motionVelocity, _e82, 0i);
    centerVelocity = _e83.xy;
    let _e86 = pc.qualityFlags;
    let _e88 = ((_e86 & 2u) == 0u);
    phi_963_ = _e88;
    if !(_e88) {
        let _e90 = (*centerValid);
        phi_963_ = !((_e90 > 0f));
    }
    let _e94 = phi_963_;
    phi_971_ = _e94;
    if !(_e94) {
        let _e96 = centerVelocity;
        param_41 = _e96;
        let _e97 = finite2_u0028_vf2_u003b((&param_41));
        phi_971_ = !(_e97);
    }
    let _e100 = phi_971_;
    if _e100 {
        let _e101 = centerVelocity;
        return _e101;
    }
    let _e103 = pc.extent;
    hi_2 = (bitcast<vec2<i32>>(_e103) - vec2<i32>(1i, 1i));
    let _e106 = (*px_1);
    best = _e106;
    let _e107 = (*px_1);
    let _e108 = textureLoad(currentDeviceDepth, _e107, 0i);
    bestDepth = _e108.x;
    y_3 = -1i;
    loop {
        let _e110 = y_3;
        if (_e110 <= 1i) {
            x_3 = -1i;
            loop {
                let _e112 = x_3;
                if (_e112 <= 1i) {
                    let _e114 = (*px_1);
                    let _e115 = x_3;
                    let _e116 = y_3;
                    let _e119 = hi_2;
                    q = clamp((_e114 + vec2<i32>(_e115, _e116)), vec2<i32>(0i, 0i), _e119);
                    let _e121 = q;
                    let _e122 = textureLoad(currentDeviceDepth, _e121, 0i);
                    d = _e122.x;
                    let _e124 = d;
                    let _e125 = bestDepth;
                    if (_e124 > _e125) {
                        let _e127 = d;
                        bestDepth = _e127;
                        let _e128 = q;
                        best = _e128;
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e129 = x_3;
                    x_3 = (_e129 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e131 = y_3;
            y_3 = (_e131 + 1i);
        }
    }
    let _e133 = best;
    let _e134 = (*px_1);
    if all((_e133 == _e134)) {
        let _e137 = centerVelocity;
        return _e137;
    }
    let _e138 = best;
    let _e139 = textureLoad(motionValidity, _e138, 0i);
    v_4 = _e139.x;
    let _e141 = v_4;
    if !((_e141 > 0f)) {
        let _e144 = centerVelocity;
        return _e144;
    }
    let _e145 = best;
    let _e146 = textureLoad(motionVelocity, _e145, 0i);
    dv = _e146.xy;
    let _e148 = dv;
    param_42 = _e148;
    let _e149 = finite2_u0028_vf2_u003b((&param_42));
    let _e150 = dv;
    let _e151 = centerVelocity;
    return select(_e151, _e150, vec2(_e149));
}

fn main_1() {
    var pixel_1: vec2<i32>;
    var current: vec4<f32>;
    var result_4: vec4<f32>;
    var usedHistory: bool;
    var valid: f32;
    var velocity: vec2<f32>;
    var param_43: vec2<i32>;
    var param_44: f32;
    var jitterDelta: vec2<f32>;
    var jitterTexel: vec2<f32>;
    var motionTexel: vec2<f32>;
    var unjitteredPreviousTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_45: vec4<f32>;
    var param_46: f32;
    var param_47: vec2<f32>;
    var param_48: vec2<f32>;
    var currentDevice: f32;
    var currentLinear: f32;
    var param_49: f32;
    var priorLinear: f32;
    var param_50: vec2<f32>;
    var depthLimit: f32;
    var param_51: f32;
    var param_52: f32;
    var prior: vec4<f32>;
    var param_53: vec2<f32>;
    var neighborhoodMin: vec3<f32>;
    var neighborhoodMax: vec3<f32>;
    var momentSum: vec3<f32>;
    var momentSquares: vec3<f32>;
    var y_4: i32;
    var x_4: i32;
    var c_1: vec3<f32>;
    var param_54: vec2<i32>;
    var param_55: vec4<f32>;
    var param_56: vec3<f32>;
    var param_57: vec3<f32>;
    var clipMin: vec3<f32>;
    var clipMax: vec3<f32>;
    var momentMean: vec3<f32>;
    var momentSigma: vec3<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var baseWeight: f32;
    var weight_1: f32;
    var param_58: f32;
    var param_59: f32;
    var resolvedRgb: vec3<f32>;
    var param_60: vec3<f32>;
    var param_61: vec3<f32>;
    var param_62: f32;
    var param_63: vec4<f32>;
    var param_64: vec2<i32>;
    var param_65: vec4<f32>;
    var param_66: vec4<f32>;
    var param_67: f32;
    var phi_1162_: bool;
    var phi_1171_: bool;
    var phi_1180_: bool;
    var phi_1186_: bool;
    var phi_1227_: bool;
    var phi_1242_: bool;
    var phi_1307_: bool;
    var phi_1313_: bool;
    var phi_1396_: bool;
    var phi_1404_: bool;

    let _e127 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e127.xy);
    let _e130 = pixel_1;
    let _e132 = pc.extent;
    if any((_e130 >= bitcast<vec2<i32>>(_e132))) {
        return;
    }
    let _e136 = pixel_1;
    let _e137 = textureLoad(currentColor, _e136, 0i);
    current = _e137;
    let _e138 = current;
    result_4 = _e138;
    usedHistory = false;
    let _e139 = pixel_1;
    let _e140 = textureLoad(motionValidity, _e139, 0i);
    valid = _e140.x;
    let _e142 = pixel_1;
    param_43 = _e142;
    let _e143 = valid;
    param_44 = _e143;
    let _e144 = dlMotionVelocity_u0028_vi2_u003b_f1_u003b((&param_43), (&param_44));
    velocity = _e144;
    let _e146 = pc.previousEffectiveJitterUv;
    let _e148 = pc.currentEffectiveJitterUv;
    jitterDelta = (_e146 - _e148);
    let _e150 = jitterDelta;
    let _e152 = pc.extent;
    jitterTexel = (_e150 * vec2<f32>(_e152));
    let _e155 = velocity;
    let _e157 = pc.extent;
    motionTexel = (_e155 * vec2<f32>(_e157));
    let _e160 = pixel_1;
    let _e162 = motionTexel;
    unjitteredPreviousTexel = (vec2<f32>(_e160) - _e162);
    let _e164 = unjitteredPreviousTexel;
    let _e165 = jitterTexel;
    previousTexel = (_e164 + _e165);
    let _e167 = previousTexel;
    previousCenter = (_e167 + vec2(0.5f));
    let _e170 = previousCenter;
    let _e172 = all((_e170 >= vec2<f32>(0f, 0f)));
    phi_1162_ = _e172;
    if _e172 {
        let _e173 = previousCenter;
        let _e175 = pc.extent;
        phi_1162_ = all((_e173 < vec2<f32>(_e175)));
    }
    let _e180 = phi_1162_;
    inside = _e180;
    let _e181 = current;
    param_45 = _e181;
    let _e182 = finite4_u0028_vf4_u003b((&param_45));
    phi_1171_ = _e182;
    if _e182 {
        let _e183 = valid;
        param_46 = _e183;
        let _e184 = finite1_u0028_f1_u003b((&param_46));
        phi_1171_ = _e184;
    }
    let _e186 = phi_1171_;
    let _e187 = valid;
    let _e189 = (_e186 && (_e187 > 0f));
    phi_1180_ = _e189;
    if _e189 {
        let _e190 = velocity;
        param_47 = _e190;
        let _e191 = finite2_u0028_vf2_u003b((&param_47));
        phi_1180_ = _e191;
    }
    let _e193 = phi_1180_;
    phi_1186_ = _e193;
    if _e193 {
        let _e194 = previousTexel;
        param_48 = _e194;
        let _e195 = finite2_u0028_vf2_u003b((&param_48));
        phi_1186_ = _e195;
    }
    let _e197 = phi_1186_;
    let _e198 = inside;
    if (_e197 && _e198) {
        let _e200 = pixel_1;
        let _e201 = textureLoad(currentDeviceDepth, _e200, 0i);
        currentDevice = _e201.x;
        let _e203 = currentDevice;
        param_49 = _e203;
        let _e204 = linearizeDepth_u0028_f1_u003b((&param_49));
        currentLinear = _e204;
        let _e205 = previousTexel;
        param_50 = _e205;
        let _e206 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_50));
        priorLinear = _e206;
        let _e208 = pc.depthThresholdAbsolute;
        let _e210 = pc.depthThresholdRelative;
        let _e211 = currentLinear;
        let _e212 = priorLinear;
        depthLimit = max(_e208, (_e210 * max(_e211, _e212)));
        let _e216 = currentLinear;
        param_51 = _e216;
        let _e217 = finite1_u0028_f1_u003b((&param_51));
        phi_1227_ = _e217;
        if _e217 {
            let _e218 = priorLinear;
            param_52 = _e218;
            let _e219 = finite1_u0028_f1_u003b((&param_52));
            phi_1227_ = _e219;
        }
        let _e221 = phi_1227_;
        let _e222 = currentLinear;
        let _e225 = priorLinear;
        let _e227 = ((_e221 && (_e222 > 0f)) && (_e225 > 0f));
        phi_1242_ = _e227;
        if _e227 {
            let _e228 = currentLinear;
            let _e229 = priorLinear;
            let _e232 = depthLimit;
            phi_1242_ = (abs((_e228 - _e229)) <= _e232);
        }
        let _e235 = phi_1242_;
        if _e235 {
            let _e236 = previousTexel;
            param_53 = _e236;
            let _e237 = samplePreviousColorLinear_u0028_vf2_u003b((&param_53));
            prior = _e237;
            neighborhoodMin = vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f);
            neighborhoodMax = vec3<f32>(-340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f);
            momentSum = vec3<f32>(0f, 0f, 0f);
            momentSquares = vec3<f32>(0f, 0f, 0f);
            y_4 = -1i;
            loop {
                let _e238 = y_4;
                if (_e238 <= 1i) {
                    x_4 = -1i;
                    loop {
                        let _e240 = x_4;
                        if (_e240 <= 1i) {
                            let _e242 = pixel_1;
                            let _e243 = x_4;
                            let _e244 = y_4;
                            param_54 = (_e242 + vec2<i32>(_e243, _e244));
                            let _e247 = sampleCurrent_u0028_vi2_u003b((&param_54));
                            c_1 = _e247.xyz;
                            let _e249 = neighborhoodMin;
                            let _e250 = c_1;
                            neighborhoodMin = min(_e249, _e250);
                            let _e252 = neighborhoodMax;
                            let _e253 = c_1;
                            neighborhoodMax = max(_e252, _e253);
                            let _e255 = c_1;
                            let _e256 = momentSum;
                            momentSum = (_e256 + _e255);
                            let _e258 = c_1;
                            let _e259 = c_1;
                            let _e261 = momentSquares;
                            momentSquares = (_e261 + (_e258 * _e259));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e263 = x_4;
                            x_4 = (_e263 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e265 = y_4;
                    y_4 = (_e265 + 1i);
                }
            }
            let _e267 = prior;
            param_55 = _e267;
            let _e268 = finite4_u0028_vf4_u003b((&param_55));
            phi_1307_ = _e268;
            if _e268 {
                let _e269 = neighborhoodMin;
                param_56 = _e269;
                let _e270 = finite3_u0028_vf3_u003b((&param_56));
                phi_1307_ = _e270;
            }
            let _e272 = phi_1307_;
            phi_1313_ = _e272;
            if _e272 {
                let _e273 = neighborhoodMax;
                param_57 = _e273;
                let _e274 = finite3_u0028_vf3_u003b((&param_57));
                phi_1313_ = _e274;
            }
            let _e276 = phi_1313_;
            if _e276 {
                let _e277 = neighborhoodMin;
                clipMin = _e277;
                let _e278 = neighborhoodMax;
                clipMax = _e278;
                let _e280 = pc.qualityFlags;
                if ((_e280 & 1u) != 0u) {
                    let _e283 = momentSum;
                    momentMean = (_e283 * 0.11111111f);
                    let _e285 = momentSquares;
                    let _e287 = momentMean;
                    let _e288 = momentMean;
                    momentSigma = sqrt(max(((_e285 * 0.11111111f) - (_e287 * _e288)), vec3<f32>(0f, 0f, 0f)));
                    let _e293 = neighborhoodMin;
                    let _e294 = momentMean;
                    let _e295 = momentSigma;
                    clipMin = max(_e293, (_e294 - (_e295 * 1.25f)));
                    let _e299 = neighborhoodMax;
                    let _e300 = momentMean;
                    let _e301 = momentSigma;
                    clipMax = min(_e299, (_e300 + (_e301 * 1.25f)));
                }
                let _e305 = prior;
                let _e307 = clipMin;
                let _e308 = clipMax;
                clampedPrior = clamp(_e305.xyz, _e307, _e308);
                let _e310 = valid;
                confidence_1 = clamp(_e310, 0f, 1f);
                let _e313 = pc.historyWeight;
                baseWeight = clamp(_e313, 0f, 1f);
                let _e315 = baseWeight;
                param_58 = _e315;
                let _e316 = confidence_1;
                param_59 = _e316;
                let _e317 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_58), (&param_59));
                weight_1 = _e317;
                let _e318 = current;
                param_60 = _e318.xyz;
                let _e320 = clampedPrior;
                param_61 = _e320;
                let _e321 = weight_1;
                param_62 = _e321;
                let _e322 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_60), (&param_61), (&param_62));
                resolvedRgb = _e322;
                let _e323 = resolvedRgb;
                let _e325 = current[3u];
                result_4 = vec4<f32>(_e323.x, _e323.y, _e323.z, _e325);
                usedHistory = true;
            }
        }
    }
    let _e330 = usedHistory;
    let _e331 = !(_e330);
    phi_1396_ = _e331;
    if _e331 {
        let _e332 = current;
        param_63 = _e332;
        let _e333 = finite4_u0028_vf4_u003b((&param_63));
        phi_1396_ = _e333;
    }
    let _e335 = phi_1396_;
    phi_1404_ = _e335;
    if _e335 {
        let _e337 = pc.qualityFlags;
        phi_1404_ = ((_e337 & 4u) != 0u);
    }
    let _e341 = phi_1404_;
    if _e341 {
        let _e342 = pixel_1;
        param_64 = _e342;
        let _e343 = current;
        param_65 = _e343;
        let _e344 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_64), (&param_65));
        result_4 = _e344;
    }
    let _e345 = result_4;
    param_66 = _e345;
    let _e346 = finite4_u0028_vf4_u003b((&param_66));
    if !(_e346) {
        result_4 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e348 = pixel_1;
    let _e349 = result_4;
    textureStore(resolvedColor, _e348, _e349);
    let _e350 = pixel_1;
    let _e351 = result_4;
    textureStore(historyOutColor, _e350, _e351);
    let _e352 = pixel_1;
    let _e353 = pixel_1;
    let _e354 = textureLoad(currentDeviceDepth, _e353, 0i);
    param_67 = _e354.x;
    let _e356 = storeLinearDepth_u0028_f1_u003b((&param_67));
    textureStore(historyOutLinearDepth, _e352, vec4(_e356));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
