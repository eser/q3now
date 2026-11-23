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
var<workgroup> tileDepth: array<f32, 100>;
@group(0) @binding(5)
var motionValidity: texture_2d<f32>;
var<workgroup> tileColor: array<vec4<f32>, 100>;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
var<private> gl_WorkGroupID_1: vec3<u32>;
var<private> gl_LocalInvocationIndex_1: u32;
@group(0) @binding(1)
var currentDeviceDepth: texture_2d<f32>;
var<private> gl_LocalInvocationID_1: vec3<u32>;
@group(0) @binding(7)
var resolvedColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(8)
var historyOutColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(9)
var historyOutLinearDepth: texture_storage_2d<r32float,write>;

fn storeLinearDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    var denominator: f32;

    let _e82 = pc.zNear;
    let _e83 = (*z);
    let _e85 = pc.zFar;
    let _e87 = pc.zNear;
    denominator = (_e82 + (_e83 * (_e85 - _e87)));
    let _e92 = pc.zNear;
    let _e94 = pc.zFar;
    let _e96 = denominator;
    let _e97 = denominator;
    return ((_e92 * _e94) * inverseSqrt((_e96 * _e97)));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e81 = (*pixel);
    let _e83 = pc.extent;
    bounded = clamp(_e81, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e83) - vec2<i32>(1i, 1i)));
    let _e87 = bounded;
    let _e88 = textureLoad(currentColor, _e87, 0i);
    return _e88;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_116_: bool;

    let _e80 = (*v);
    let _e81 = (*v);
    let _e82 = (_e80 == _e81);
    phi_116_ = _e82;
    if _e82 {
        let _e83 = (*v);
        phi_116_ = (abs(_e83) <= 340282350000000000000000000000000000000f);
    }
    let _e87 = phi_116_;
    return _e87;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e82 = (*c);
    l = dot(_e82, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e84 = l;
    param = _e84;
    let _e85 = finite1_u0028_f1_u003b((&param));
    if !(_e85) {
        return 0f;
    }
    let _e87 = l;
    l = clamp(_e87, 0f, 10000f);
    let _e89 = l;
    let _e90 = l;
    let _e91 = l;
    return (_e89 * inverseSqrt((1f + (_e90 * _e91))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e82 = (*p);
    param_1 = _e82;
    let _e83 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e83.xyz;
    let _e85 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e85;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e83 = (*base);
    param_3 = _e83;
    let _e84 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e85 = (*base);
    let _e86 = (*across);
    param_4 = (_e85 + _e86);
    let _e88 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e84 + _e88));
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

    let _e152 = (*centerColor);
    param_5 = _e152.xyz;
    let _e154 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e154;
    let _e155 = (*px);
    param_6 = (_e155 + vec2<i32>(0i, -1i));
    let _e157 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e157;
    let _e158 = (*px);
    param_7 = (_e158 + vec2<i32>(0i, 1i));
    let _e160 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e160;
    let _e161 = (*px);
    param_8 = (_e161 + vec2<i32>(-1i, 0i));
    let _e163 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e163;
    let _e164 = (*px);
    param_9 = (_e164 + vec2<i32>(1i, 0i));
    let _e166 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e166;
    let _e167 = fxN;
    let _e168 = fxS;
    let _e170 = fxW;
    let _e171 = fxE;
    let _e173 = fxM;
    fxHi = max(max(_e167, _e168), max(max(_e170, _e171), _e173));
    let _e176 = fxN;
    let _e177 = fxS;
    let _e179 = fxW;
    let _e180 = fxE;
    let _e182 = fxM;
    fxLo = min(min(_e176, _e177), min(min(_e179, _e180), _e182));
    let _e185 = fxHi;
    let _e186 = fxLo;
    fxSpan = (_e185 - _e186);
    let _e188 = fxSpan;
    let _e189 = fxHi;
    if (_e188 < max(0.0312f, (_e189 * 0.125f))) {
        let _e193 = (*centerColor);
        return _e193;
    }
    let _e194 = (*px);
    param_10 = (_e194 + vec2<i32>(-1i, -1i));
    let _e196 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e196;
    let _e197 = (*px);
    param_11 = (_e197 + vec2<i32>(1i, -1i));
    let _e199 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e199;
    let _e200 = (*px);
    param_12 = (_e200 + vec2<i32>(-1i, 1i));
    let _e202 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e202;
    let _e203 = (*px);
    param_13 = (_e203 + vec2<i32>(1i, 1i));
    let _e205 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e205;
    let _e206 = fxN;
    let _e207 = fxS;
    fxNS = (_e206 + _e207);
    let _e209 = fxW;
    let _e210 = fxE;
    fxWE = (_e209 + _e210);
    let _e212 = fxNW;
    let _e213 = fxSW;
    fxNWSW = (_e212 + _e213);
    let _e215 = fxNE;
    let _e216 = fxSE;
    fxNESE = (_e215 + _e216);
    let _e218 = fxW;
    let _e220 = fxNWSW;
    let _e223 = fxM;
    let _e225 = fxNS;
    let _e229 = fxE;
    let _e231 = fxNESE;
    fxHorz = (abs(((-2f * _e218) + _e220)) + ((abs(((-2f * _e223) + _e225)) * 2f) + abs(((-2f * _e229) + _e231))));
    let _e236 = fxS;
    let _e238 = fxSW;
    let _e239 = fxSE;
    let _e243 = fxM;
    let _e245 = fxWE;
    let _e249 = fxN;
    let _e251 = fxNW;
    let _e252 = fxNE;
    fxVert = (abs(((-2f * _e236) + (_e238 + _e239))) + ((abs(((-2f * _e243) + _e245)) * 2f) + abs(((-2f * _e249) + (_e251 + _e252)))));
    let _e258 = fxHorz;
    let _e259 = fxVert;
    fxHorzSpan = (_e258 >= _e259);
    let _e261 = fxNS;
    let _e262 = fxWE;
    let _e265 = fxNWSW;
    let _e266 = fxNESE;
    fxAround = (((_e261 + _e262) * 2f) + (_e265 + _e266));
    let _e269 = fxHorzSpan;
    let _e270 = fxN;
    let _e271 = fxW;
    fxLoSide = select(_e271, _e270, _e269);
    let _e273 = fxHorzSpan;
    let _e274 = fxS;
    let _e275 = fxE;
    fxHiSide = select(_e275, _e274, _e273);
    let _e277 = fxLoSide;
    let _e278 = fxM;
    fxGradLo = (_e277 - _e278);
    let _e280 = fxHiSide;
    let _e281 = fxM;
    fxGradHi = (_e280 - _e281);
    let _e283 = fxGradLo;
    let _e285 = fxGradHi;
    fxLoSteeper = (abs(_e283) >= abs(_e285));
    let _e288 = fxGradLo;
    let _e290 = fxGradHi;
    fxGradient = max(abs(_e288), abs(_e290));
    let _e293 = fxLoSteeper;
    if _e293 {
        let _e294 = fxLoSide;
        let _e295 = fxM;
        local = (_e294 + _e295);
    } else {
        let _e297 = fxHiSide;
        let _e298 = fxM;
        local = (_e297 + _e298);
    }
    let _e300 = local;
    fxPair = _e300;
    let _e301 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e301));
    let _e304 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e304));
    let _e307 = fxLoSteeper;
    if _e307 {
        let _e308 = fxAcross;
        fxAcross = -(_e308);
    }
    let _e310 = fxSpan;
    let _e311 = fxSpan;
    fxInvSpan = inverseSqrt((_e310 * _e311));
    let _e314 = fxAround;
    let _e316 = fxM;
    let _e319 = fxInvSpan;
    fxBlendC = clamp((abs(((_e314 * 0.083333336f) - _e316)) * _e319), 0f, 1f);
    let _e322 = fxBlendC;
    let _e325 = fxBlendC;
    let _e326 = fxBlendC;
    fxBlendF = (((-2f * _e322) + 3f) * (_e325 * _e326));
    let _e329 = fxBlendF;
    let _e330 = fxBlendF;
    fxSubpixel = ((_e329 * _e330) * 0.75f);
    let _e333 = fxPair;
    fxHalfPair = (_e333 * 0.5f);
    let _e335 = fxGradient;
    fxScaled = (_e335 * 0.25f);
    let _e337 = fxM;
    let _e338 = fxHalfPair;
    fxMidNegative = ((_e337 - _e338) < 0f);
    let _e341 = (*px);
    let _e342 = fxAlong;
    param_14 = (_e341 - _e342);
    let _e344 = fxAcross;
    param_15 = _e344;
    let _e345 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e346 = fxHalfPair;
    fxEndN = (_e345 - _e346);
    let _e348 = (*px);
    let _e349 = fxAlong;
    param_16 = (_e348 + _e349);
    let _e351 = fxAcross;
    param_17 = _e351;
    let _e352 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e353 = fxHalfPair;
    fxEndP = (_e352 - _e353);
    let _e355 = fxEndN;
    let _e357 = fxScaled;
    fxDoneN = (abs(_e355) >= _e357);
    let _e359 = fxEndP;
    let _e361 = fxScaled;
    fxDoneP = (abs(_e359) >= _e361);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e363 = i;
        if (_e363 <= 12i) {
            let _e365 = fxDoneN;
            if !(_e365) {
                let _e367 = (*px);
                let _e368 = fxAlong;
                let _e369 = i;
                param_18 = (_e367 - (_e368 * vec2(_e369)));
                let _e373 = fxAcross;
                param_19 = _e373;
                let _e374 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e375 = fxHalfPair;
                fxEndN = (_e374 - _e375);
                let _e377 = fxEndN;
                let _e379 = fxScaled;
                fxDoneN = (abs(_e377) >= _e379);
                let _e381 = i;
                fxStepsN = _e381;
            }
            let _e382 = fxDoneP;
            if !(_e382) {
                let _e384 = (*px);
                let _e385 = fxAlong;
                let _e386 = i;
                param_20 = (_e384 + (_e385 * vec2(_e386)));
                let _e390 = fxAcross;
                param_21 = _e390;
                let _e391 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e392 = fxHalfPair;
                fxEndP = (_e391 - _e392);
                let _e394 = fxEndP;
                let _e396 = fxScaled;
                fxDoneP = (abs(_e394) >= _e396);
                let _e398 = i;
                fxStepsP = _e398;
            }
            let _e399 = fxDoneN;
            let _e400 = fxDoneP;
            if (_e399 && _e400) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e402 = i;
            i = (_e402 + 1i);
        }
    }
    let _e404 = fxStepsN;
    let _e405 = fxStepsP;
    fxTowardN = (_e404 < _e405);
    let _e407 = fxTowardN;
    let _e408 = fxEndN;
    let _e409 = fxEndP;
    fxEndG = select(_e409, _e408, _e407);
    let _e411 = fxEndG;
    let _e413 = fxMidNegative;
    fxGoodSpan = ((_e411 < 0f) != _e413);
    let _e415 = fxStepsN;
    let _e416 = fxStepsP;
    fxSpanLen = f32((_e415 + _e416));
    let _e419 = fxTowardN;
    let _e420 = fxStepsN;
    let _e421 = fxStepsP;
    fxNear = f32(select(_e421, _e420, _e419));
    let _e424 = fxGoodSpan;
    if _e424 {
        let _e425 = fxNear;
        let _e426 = fxSpanLen;
        let _e427 = fxSpanLen;
        local_1 = (0.5f - (_e425 * inverseSqrt((_e426 * _e427))));
    } else {
        local_1 = 0f;
    }
    let _e432 = local_1;
    fxEdgeBlend = _e432;
    let _e433 = fxEdgeBlend;
    let _e434 = fxSubpixel;
    fxBlend = clamp(max(_e433, _e434), 0f, 1f);
    let _e437 = (*px);
    let _e438 = fxAcross;
    param_22 = (_e437 + _e438);
    let _e440 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e440.xyz;
    let _e442 = (*centerColor);
    let _e444 = fxBlend;
    let _e447 = fxAcrossColor;
    let _e448 = fxBlend;
    let _e450 = ((_e442.xyz * (1f - _e444)) + (_e447 * _e448));
    let _e452 = (*centerColor)[3u];
    return vec4<f32>(_e450.x, _e450.y, _e450.z, _e452);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e86 = (*a);
    oneMinus = (1f - _e86);
    let _e88 = (*x);
    let _e89 = oneMinus;
    left = (_e88 * _e89);
    let _e91 = (*y);
    let _e92 = (*a);
    right = (_e91 * _e92);
    let _e94 = left;
    let _e95 = right;
    result = (_e94 + _e95);
    let _e97 = result;
    return _e97;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e82 = (*weight);
    let _e83 = (*confidence);
    scaled = (_e82 * _e83);
    let _e85 = scaled;
    return _e85;
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_146_: bool;

    let _e80 = (*v_1);
    let _e81 = (*v_1);
    let _e83 = all((_e80 == _e81));
    phi_146_ = _e83;
    if _e83 {
        let _e84 = (*v_1);
        phi_146_ = all((abs(_e84) <= vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e89 = phi_146_;
    return _e89;
}

fn neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b(tileIndex: ptr<function, i32>, boxMin: ptr<function, vec3<f32>>, boxMax: ptr<function, vec3<f32>>) -> bool {
    var neighborhoodMin: vec3<f32>;
    var neighborhoodMax: vec3<f32>;
    var momentSum: vec3<f32>;
    var momentSquares: vec3<f32>;
    var y_1: i32;
    var x_1: i32;
    var c_1: vec3<f32>;
    var param_23: vec3<f32>;
    var param_24: vec3<f32>;
    var clipMin: vec3<f32>;
    var clipMax: vec3<f32>;
    var momentMean: vec3<f32>;
    var momentSigma: vec3<f32>;
    var phi_1135_: bool;

    neighborhoodMin = vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f);
    neighborhoodMax = vec3<f32>(-340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f);
    momentSum = vec3<f32>(0f, 0f, 0f);
    momentSquares = vec3<f32>(0f, 0f, 0f);
    y_1 = -1i;
    loop {
        let _e95 = y_1;
        if (_e95 <= 1i) {
            x_1 = -1i;
            loop {
                let _e97 = x_1;
                if (_e97 <= 1i) {
                    let _e99 = (*tileIndex);
                    let _e100 = y_1;
                    let _e103 = x_1;
                    let _e106 = tileColor[((_e99 + (_e100 * 10i)) + _e103)];
                    c_1 = _e106.xyz;
                    let _e108 = neighborhoodMin;
                    let _e109 = c_1;
                    neighborhoodMin = min(_e108, _e109);
                    let _e111 = neighborhoodMax;
                    let _e112 = c_1;
                    neighborhoodMax = max(_e111, _e112);
                    let _e114 = c_1;
                    let _e115 = momentSum;
                    momentSum = (_e115 + _e114);
                    let _e117 = c_1;
                    let _e118 = c_1;
                    let _e120 = momentSquares;
                    momentSquares = (_e120 + (_e117 * _e118));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e122 = x_1;
                    x_1 = (_e122 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e124 = y_1;
            y_1 = (_e124 + 1i);
        }
    }
    let _e126 = neighborhoodMin;
    param_23 = _e126;
    let _e127 = finite3_u0028_vf3_u003b((&param_23));
    phi_1135_ = _e127;
    if _e127 {
        let _e128 = neighborhoodMax;
        param_24 = _e128;
        let _e129 = finite3_u0028_vf3_u003b((&param_24));
        phi_1135_ = _e129;
    }
    let _e131 = phi_1135_;
    if !(_e131) {
        return false;
    }
    let _e133 = neighborhoodMin;
    clipMin = _e133;
    let _e134 = neighborhoodMax;
    clipMax = _e134;
    let _e136 = pc.qualityFlags;
    if ((_e136 & 1u) != 0u) {
        let _e139 = momentSum;
        momentMean = (_e139 * 0.11111111f);
        let _e141 = momentSquares;
        let _e143 = momentMean;
        let _e144 = momentMean;
        momentSigma = sqrt(max(((_e141 * 0.11111111f) - (_e143 * _e144)), vec3<f32>(0f, 0f, 0f)));
        let _e149 = neighborhoodMin;
        let _e150 = momentMean;
        let _e151 = momentSigma;
        clipMin = max(_e149, (_e150 - (_e151 * 1.25f)));
        let _e155 = neighborhoodMax;
        let _e156 = momentMean;
        let _e157 = momentSigma;
        clipMax = min(_e155, (_e156 + (_e157 * 1.25f)));
    }
    let _e161 = clipMin;
    (*boxMin) = _e161;
    let _e162 = clipMax;
    (*boxMax) = _e162;
    return true;
}

fn mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b(x_2: ptr<function, vec4<f32>>, y_2: ptr<function, vec4<f32>>, a_1: ptr<function, f32>) -> vec4<f32> {
    let _e82 = (*x_2);
    let _e83 = (*a_1);
    let _e86 = (*y_2);
    let _e87 = (*a_1);
    return ((_e82 * (1f - _e83)) + (_e86 * _e87));
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
    var param_25: vec4<f32>;
    var param_26: vec4<f32>;
    var param_27: f32;
    var bottom: vec4<f32>;
    var param_28: vec4<f32>;
    var param_29: vec4<f32>;
    var param_30: f32;
    var param_31: vec4<f32>;
    var param_32: vec4<f32>;
    var param_33: f32;

    let _e99 = (*texel);
    baseFloat = floor(_e99);
    let _e101 = baseFloat;
    base_1 = vec2<i32>(_e101);
    let _e103 = (*texel);
    let _e104 = baseFloat;
    f = (_e103 - _e104);
    let _e107 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e107) - vec2<i32>(1i, 1i));
    let _e110 = base_1;
    let _e111 = hi;
    let _e113 = textureLoad(previousColor, clamp(_e110, vec2<i32>(0i, 0i), _e111), 0i);
    c00_ = _e113;
    let _e114 = base_1;
    let _e116 = hi;
    let _e118 = textureLoad(previousColor, clamp((_e114 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e116), 0i);
    c10_ = _e118;
    let _e119 = base_1;
    let _e121 = hi;
    let _e123 = textureLoad(previousColor, clamp((_e119 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e121), 0i);
    c01_ = _e123;
    let _e124 = base_1;
    let _e126 = hi;
    let _e128 = textureLoad(previousColor, clamp((_e124 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e126), 0i);
    c11_ = _e128;
    let _e129 = c00_;
    param_25 = _e129;
    let _e130 = c10_;
    param_26 = _e130;
    let _e132 = f[0u];
    param_27 = _e132;
    let _e133 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_25), (&param_26), (&param_27));
    top = _e133;
    let _e134 = c01_;
    param_28 = _e134;
    let _e135 = c11_;
    param_29 = _e135;
    let _e137 = f[0u];
    param_30 = _e137;
    let _e138 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_28), (&param_29), (&param_30));
    bottom = _e138;
    let _e139 = top;
    param_31 = _e139;
    let _e140 = bottom;
    param_32 = _e140;
    let _e142 = f[1u];
    param_33 = _e142;
    let _e143 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_31), (&param_32), (&param_33));
    return _e143;
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_3: ptr<function, f32>, y_3: ptr<function, f32>, a_2: ptr<function, f32>) -> f32 {
    let _e82 = (*x_3);
    let _e83 = (*a_2);
    let _e86 = (*y_3);
    let _e87 = (*a_2);
    return ((_e82 * (1f - _e83)) + (_e86 * _e87));
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
    var param_34: f32;
    var param_35: f32;
    var param_36: f32;
    var bottom_1: f32;
    var param_37: f32;
    var param_38: f32;
    var param_39: f32;
    var param_40: f32;
    var param_41: f32;
    var param_42: f32;

    let _e99 = (*texel_1);
    baseFloat_1 = floor(_e99);
    let _e101 = baseFloat_1;
    base_2 = vec2<i32>(_e101);
    let _e103 = (*texel_1);
    let _e104 = baseFloat_1;
    f_1 = (_e103 - _e104);
    let _e107 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e107) - vec2<i32>(1i, 1i));
    let _e110 = base_2;
    let _e111 = hi_1;
    let _e113 = textureLoad(previousLinearDepth, clamp(_e110, vec2<i32>(0i, 0i), _e111), 0i);
    d00_ = _e113.x;
    let _e115 = base_2;
    let _e117 = hi_1;
    let _e119 = textureLoad(previousLinearDepth, clamp((_e115 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e117), 0i);
    d10_ = _e119.x;
    let _e121 = base_2;
    let _e123 = hi_1;
    let _e125 = textureLoad(previousLinearDepth, clamp((_e121 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e123), 0i);
    d01_ = _e125.x;
    let _e127 = base_2;
    let _e129 = hi_1;
    let _e131 = textureLoad(previousLinearDepth, clamp((_e127 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e129), 0i);
    d11_ = _e131.x;
    let _e133 = d00_;
    param_34 = _e133;
    let _e134 = d10_;
    param_35 = _e134;
    let _e136 = f_1[0u];
    param_36 = _e136;
    let _e137 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_34), (&param_35), (&param_36));
    top_1 = _e137;
    let _e138 = d01_;
    param_37 = _e138;
    let _e139 = d11_;
    param_38 = _e139;
    let _e141 = f_1[0u];
    param_39 = _e141;
    let _e142 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_37), (&param_38), (&param_39));
    bottom_1 = _e142;
    let _e143 = top_1;
    param_40 = _e143;
    let _e144 = bottom_1;
    param_41 = _e144;
    let _e146 = f_1[1u];
    param_42 = _e146;
    let _e147 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_40), (&param_41), (&param_42));
    return _e147;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator_1: f32;
    var numerator: f32;
    var result_1: f32;

    let _e86 = pc.zFar;
    let _e88 = pc.zNear;
    range = (_e86 - _e88);
    let _e90 = (*z_1);
    let _e91 = range;
    scaled_1 = (_e90 * _e91);
    let _e94 = pc.zNear;
    let _e95 = scaled_1;
    denominator_1 = (_e94 + _e95);
    let _e98 = pc.zNear;
    let _e100 = pc.zFar;
    numerator = (_e98 * _e100);
    let _e102 = numerator;
    let _e103 = denominator_1;
    result_1 = (_e102 / _e103);
    let _e105 = result_1;
    return _e105;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_131_: bool;

    let _e80 = (*v_2);
    let _e81 = (*v_2);
    let _e83 = all((_e80 == _e81));
    phi_131_ = _e83;
    if _e83 {
        let _e84 = (*v_2);
        phi_131_ = all((abs(_e84) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e89 = phi_131_;
    return _e89;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_161_: bool;

    let _e80 = (*v_3);
    let _e81 = (*v_3);
    let _e83 = all((_e80 == _e81));
    phi_161_ = _e83;
    if _e83 {
        let _e84 = (*v_3);
        phi_161_ = all((abs(_e84) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e89 = phi_161_;
    return _e89;
}

fn dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b(px_1: ptr<function, vec2<i32>>, tileIndex_1: ptr<function, i32>, centerValid: ptr<function, f32>) -> vec2<f32> {
    var centerVelocity: vec2<f32>;
    var param_43: vec2<f32>;
    var hi_2: vec2<i32>;
    var best: vec2<i32>;
    var bestDepth: f32;
    var y_4: i32;
    var x_4: i32;
    var d: f32;
    var v_4: f32;
    var dv: vec2<f32>;
    var param_44: vec2<f32>;
    var phi_959_: bool;
    var phi_967_: bool;

    let _e93 = (*px_1);
    let _e94 = textureLoad(motionVelocity, _e93, 0i);
    centerVelocity = _e94.xy;
    let _e97 = pc.qualityFlags;
    let _e99 = ((_e97 & 2u) == 0u);
    phi_959_ = _e99;
    if !(_e99) {
        let _e101 = (*centerValid);
        phi_959_ = !((_e101 > 0f));
    }
    let _e105 = phi_959_;
    phi_967_ = _e105;
    if !(_e105) {
        let _e107 = centerVelocity;
        param_43 = _e107;
        let _e108 = finite2_u0028_vf2_u003b((&param_43));
        phi_967_ = !(_e108);
    }
    let _e111 = phi_967_;
    if _e111 {
        let _e112 = centerVelocity;
        return _e112;
    }
    let _e114 = pc.extent;
    hi_2 = (bitcast<vec2<i32>>(_e114) - vec2<i32>(1i, 1i));
    let _e117 = (*px_1);
    best = _e117;
    let _e118 = (*tileIndex_1);
    let _e120 = tileDepth[_e118];
    bestDepth = _e120;
    y_4 = -1i;
    loop {
        let _e121 = y_4;
        if (_e121 <= 1i) {
            x_4 = -1i;
            loop {
                let _e123 = x_4;
                if (_e123 <= 1i) {
                    let _e125 = (*tileIndex_1);
                    let _e126 = y_4;
                    let _e129 = x_4;
                    let _e132 = tileDepth[((_e125 + (_e126 * 10i)) + _e129)];
                    d = _e132;
                    let _e133 = d;
                    let _e134 = bestDepth;
                    if (_e133 > _e134) {
                        let _e136 = d;
                        bestDepth = _e136;
                        let _e137 = (*px_1);
                        let _e138 = x_4;
                        let _e139 = y_4;
                        let _e142 = hi_2;
                        best = clamp((_e137 + vec2<i32>(_e138, _e139)), vec2<i32>(0i, 0i), _e142);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e144 = x_4;
                    x_4 = (_e144 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e146 = y_4;
            y_4 = (_e146 + 1i);
        }
    }
    let _e148 = best;
    let _e149 = (*px_1);
    if all((_e148 == _e149)) {
        let _e152 = centerVelocity;
        return _e152;
    }
    let _e153 = best;
    let _e154 = textureLoad(motionValidity, _e153, 0i);
    v_4 = _e154.x;
    let _e156 = v_4;
    if !((_e156 > 0f)) {
        let _e159 = centerVelocity;
        return _e159;
    }
    let _e160 = best;
    let _e161 = textureLoad(motionVelocity, _e160, 0i);
    dv = _e161.xy;
    let _e163 = dv;
    param_44 = _e163;
    let _e164 = finite2_u0028_vf2_u003b((&param_44));
    let _e165 = dv;
    let _e166 = centerVelocity;
    return select(_e166, _e165, vec2(_e164));
}

fn main_1() {
    var pixel_1: vec2<i32>;
    var tileOrigin: vec2<i32>;
    var imageHi: vec2<i32>;
    var i_1: u32;
    var q: vec2<i32>;
    var tileIndex_2: i32;
    var current: vec4<f32>;
    var result_2: vec4<f32>;
    var usedHistory: bool;
    var valid: f32;
    var velocity: vec2<f32>;
    var param_45: vec2<i32>;
    var param_46: i32;
    var param_47: f32;
    var jitterDelta: vec2<f32>;
    var jitterTexel: vec2<f32>;
    var motionTexel: vec2<f32>;
    var unjitteredPreviousTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_48: vec4<f32>;
    var param_49: f32;
    var param_50: vec2<f32>;
    var param_51: vec2<f32>;
    var currentDevice: f32;
    var currentLinear: f32;
    var param_52: f32;
    var priorLinear: f32;
    var param_53: vec2<f32>;
    var depthLimit: f32;
    var param_54: f32;
    var param_55: f32;
    var prior: vec4<f32>;
    var param_56: vec2<f32>;
    var param_57: vec4<f32>;
    var clipMin_1: vec3<f32>;
    var clipMax_1: vec3<f32>;
    var param_58: i32;
    var param_59: vec3<f32>;
    var param_60: vec3<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var baseWeight: f32;
    var weight_1: f32;
    var param_61: f32;
    var param_62: f32;
    var resolvedRgb: vec3<f32>;
    var param_63: vec3<f32>;
    var param_64: vec3<f32>;
    var param_65: f32;
    var param_66: vec4<f32>;
    var param_67: vec2<i32>;
    var param_68: vec4<f32>;
    var param_69: vec4<f32>;
    var param_70: f32;
    var phi_1339_: bool;
    var phi_1348_: bool;
    var phi_1357_: bool;
    var phi_1363_: bool;
    var phi_1400_: bool;
    var phi_1415_: bool;
    var phi_1436_: bool;
    var phi_1482_: bool;
    var phi_1490_: bool;

    let _e135 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e135.xy);
    let _e138 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e138.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e145 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e145) - vec2<i32>(1i, 1i));
    let _e148 = gl_LocalInvocationIndex_1;
    i_1 = _e148;
    loop {
        let _e149 = i_1;
        if (_e149 < 100u) {
            let _e151 = tileOrigin;
            let _e152 = i_1;
            let _e155 = i_1;
            let _e160 = imageHi;
            q = clamp((_e151 + vec2<i32>(bitcast<i32>((_e152 % 10u)), bitcast<i32>((_e155 / 10u)))), vec2<i32>(0i, 0i), _e160);
            let _e162 = i_1;
            let _e163 = q;
            let _e164 = textureLoad(currentColor, _e163, 0i);
            tileColor[_e162] = _e164;
            let _e166 = i_1;
            let _e167 = q;
            let _e168 = textureLoad(currentDeviceDepth, _e167, 0i);
            tileDepth[_e166] = _e168.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e171 = i_1;
            i_1 = (_e171 + 64u);
        }
    }
    workgroupBarrier();
    let _e173 = pixel_1;
    let _e175 = pc.extent;
    if any((_e173 >= bitcast<vec2<i32>>(_e175))) {
        return;
    }
    let _e180 = gl_LocalInvocationID_1[1u];
    let _e185 = gl_LocalInvocationID_1[0u];
    tileIndex_2 = ((((bitcast<i32>(_e180) + 1i) * 10i) + bitcast<i32>(_e185)) + 1i);
    let _e189 = tileIndex_2;
    let _e191 = tileColor[_e189];
    current = _e191;
    let _e192 = current;
    result_2 = _e192;
    usedHistory = false;
    let _e193 = pixel_1;
    let _e194 = textureLoad(motionValidity, _e193, 0i);
    valid = _e194.x;
    let _e196 = pixel_1;
    param_45 = _e196;
    let _e197 = tileIndex_2;
    param_46 = _e197;
    let _e198 = valid;
    param_47 = _e198;
    let _e199 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_45), (&param_46), (&param_47));
    velocity = _e199;
    let _e201 = pc.previousEffectiveJitterUv;
    let _e203 = pc.currentEffectiveJitterUv;
    jitterDelta = (_e201 - _e203);
    let _e205 = jitterDelta;
    let _e207 = pc.extent;
    jitterTexel = (_e205 * vec2<f32>(_e207));
    let _e210 = velocity;
    let _e212 = pc.extent;
    motionTexel = (_e210 * vec2<f32>(_e212));
    let _e215 = pixel_1;
    let _e217 = motionTexel;
    unjitteredPreviousTexel = (vec2<f32>(_e215) - _e217);
    let _e219 = unjitteredPreviousTexel;
    let _e220 = jitterTexel;
    previousTexel = (_e219 + _e220);
    let _e222 = previousTexel;
    previousCenter = (_e222 + vec2(0.5f));
    let _e225 = previousCenter;
    let _e227 = all((_e225 >= vec2<f32>(0f, 0f)));
    phi_1339_ = _e227;
    if _e227 {
        let _e228 = previousCenter;
        let _e230 = pc.extent;
        phi_1339_ = all((_e228 < vec2<f32>(_e230)));
    }
    let _e235 = phi_1339_;
    inside = _e235;
    let _e236 = current;
    param_48 = _e236;
    let _e237 = finite4_u0028_vf4_u003b((&param_48));
    phi_1348_ = _e237;
    if _e237 {
        let _e238 = valid;
        param_49 = _e238;
        let _e239 = finite1_u0028_f1_u003b((&param_49));
        phi_1348_ = _e239;
    }
    let _e241 = phi_1348_;
    let _e242 = valid;
    let _e244 = (_e241 && (_e242 > 0f));
    phi_1357_ = _e244;
    if _e244 {
        let _e245 = velocity;
        param_50 = _e245;
        let _e246 = finite2_u0028_vf2_u003b((&param_50));
        phi_1357_ = _e246;
    }
    let _e248 = phi_1357_;
    phi_1363_ = _e248;
    if _e248 {
        let _e249 = previousTexel;
        param_51 = _e249;
        let _e250 = finite2_u0028_vf2_u003b((&param_51));
        phi_1363_ = _e250;
    }
    let _e252 = phi_1363_;
    let _e253 = inside;
    if (_e252 && _e253) {
        let _e255 = tileIndex_2;
        let _e257 = tileDepth[_e255];
        currentDevice = _e257;
        let _e258 = currentDevice;
        param_52 = _e258;
        let _e259 = linearizeDepth_u0028_f1_u003b((&param_52));
        currentLinear = _e259;
        let _e260 = previousTexel;
        param_53 = _e260;
        let _e261 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_53));
        priorLinear = _e261;
        let _e263 = pc.depthThresholdAbsolute;
        let _e265 = pc.depthThresholdRelative;
        let _e266 = currentLinear;
        let _e267 = priorLinear;
        depthLimit = max(_e263, (_e265 * max(_e266, _e267)));
        let _e271 = currentLinear;
        param_54 = _e271;
        let _e272 = finite1_u0028_f1_u003b((&param_54));
        phi_1400_ = _e272;
        if _e272 {
            let _e273 = priorLinear;
            param_55 = _e273;
            let _e274 = finite1_u0028_f1_u003b((&param_55));
            phi_1400_ = _e274;
        }
        let _e276 = phi_1400_;
        let _e277 = currentLinear;
        let _e280 = priorLinear;
        let _e282 = ((_e276 && (_e277 > 0f)) && (_e280 > 0f));
        phi_1415_ = _e282;
        if _e282 {
            let _e283 = currentLinear;
            let _e284 = priorLinear;
            let _e287 = depthLimit;
            phi_1415_ = (abs((_e283 - _e284)) <= _e287);
        }
        let _e290 = phi_1415_;
        if _e290 {
            let _e291 = previousTexel;
            param_56 = _e291;
            let _e292 = samplePreviousColorLinear_u0028_vf2_u003b((&param_56));
            prior = _e292;
            let _e293 = prior;
            param_57 = _e293;
            let _e294 = finite4_u0028_vf4_u003b((&param_57));
            phi_1436_ = _e294;
            if _e294 {
                let _e295 = tileIndex_2;
                param_58 = _e295;
                let _e296 = neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b((&param_58), (&param_59), (&param_60));
                let _e297 = param_59;
                clipMin_1 = _e297;
                let _e298 = param_60;
                clipMax_1 = _e298;
                phi_1436_ = _e296;
            }
            let _e300 = phi_1436_;
            if _e300 {
                let _e301 = prior;
                let _e303 = clipMin_1;
                let _e304 = clipMax_1;
                clampedPrior = clamp(_e301.xyz, _e303, _e304);
                let _e306 = valid;
                confidence_1 = clamp(_e306, 0f, 1f);
                let _e309 = pc.historyWeight;
                baseWeight = clamp(_e309, 0f, 1f);
                let _e311 = baseWeight;
                param_61 = _e311;
                let _e312 = confidence_1;
                param_62 = _e312;
                let _e313 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_61), (&param_62));
                weight_1 = _e313;
                let _e314 = current;
                param_63 = _e314.xyz;
                let _e316 = clampedPrior;
                param_64 = _e316;
                let _e317 = weight_1;
                param_65 = _e317;
                let _e318 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_63), (&param_64), (&param_65));
                resolvedRgb = _e318;
                let _e319 = resolvedRgb;
                let _e321 = current[3u];
                result_2 = vec4<f32>(_e319.x, _e319.y, _e319.z, _e321);
                usedHistory = true;
            }
        }
    }
    let _e326 = usedHistory;
    let _e327 = !(_e326);
    phi_1482_ = _e327;
    if _e327 {
        let _e328 = current;
        param_66 = _e328;
        let _e329 = finite4_u0028_vf4_u003b((&param_66));
        phi_1482_ = _e329;
    }
    let _e331 = phi_1482_;
    phi_1490_ = _e331;
    if _e331 {
        let _e333 = pc.qualityFlags;
        phi_1490_ = ((_e333 & 4u) != 0u);
    }
    let _e337 = phi_1490_;
    if _e337 {
        let _e338 = pixel_1;
        param_67 = _e338;
        let _e339 = current;
        param_68 = _e339;
        let _e340 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_67), (&param_68));
        result_2 = _e340;
    }
    let _e341 = result_2;
    param_69 = _e341;
    let _e342 = finite4_u0028_vf4_u003b((&param_69));
    if !(_e342) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e344 = pixel_1;
    let _e345 = result_2;
    textureStore(resolvedColor, _e344, _e345);
    let _e346 = pixel_1;
    let _e347 = result_2;
    textureStore(historyOutColor, _e346, _e347);
    let _e348 = pixel_1;
    let _e349 = tileIndex_2;
    let _e351 = tileDepth[_e349];
    param_70 = _e351;
    let _e352 = storeLinearDepth_u0028_f1_u003b((&param_70));
    textureStore(historyOutLinearDepth, _e348, vec4(_e352));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>, @builtin(workgroup_id) gl_WorkGroupID: vec3<u32>, @builtin(local_invocation_index) gl_LocalInvocationIndex: u32, @builtin(local_invocation_id) gl_LocalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    gl_WorkGroupID_1 = gl_WorkGroupID;
    gl_LocalInvocationIndex_1 = gl_LocalInvocationIndex;
    gl_LocalInvocationID_1 = gl_LocalInvocationID;
    main_1();
}
