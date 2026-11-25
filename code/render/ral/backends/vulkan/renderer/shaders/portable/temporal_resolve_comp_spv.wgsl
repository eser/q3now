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

    let _e94 = pc.zNear;
    let _e95 = (*z);
    let _e97 = pc.zFar;
    let _e99 = pc.zNear;
    denominator = (_e94 + (_e95 * (_e97 - _e99)));
    let _e104 = pc.zNear;
    let _e106 = pc.zFar;
    let _e108 = denominator;
    let _e109 = denominator;
    return ((_e104 * _e106) * inverseSqrt((_e108 * _e109)));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e93 = (*pixel);
    let _e95 = pc.extent;
    bounded = clamp(_e93, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e95) - vec2<i32>(1i, 1i)));
    let _e99 = bounded;
    let _e100 = textureLoad(currentColor, _e99, 0i);
    return _e100;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_142_: bool;

    let _e92 = (*v);
    let _e93 = (*v);
    let _e94 = (_e92 == _e93);
    phi_142_ = _e94;
    if _e94 {
        let _e95 = (*v);
        phi_142_ = (abs(_e95) <= 0x1.fffffep+127f);
    }
    let _e99 = phi_142_;
    return _e99;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e94 = (*c);
    l = dot(_e94, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e96 = l;
    param = _e96;
    let _e97 = finite1_u0028_f1_u003b((&param));
    if !(_e97) {
        return 0f;
    }
    let _e99 = l;
    l = clamp(_e99, 0f, 10000f);
    let _e101 = l;
    let _e102 = l;
    let _e103 = l;
    return (_e101 * inverseSqrt((1f + (_e102 * _e103))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e94 = (*p);
    param_1 = _e94;
    let _e95 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e95.xyz;
    let _e97 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e97;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e95 = (*base);
    param_3 = _e95;
    let _e96 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e97 = (*base);
    let _e98 = (*across);
    param_4 = (_e97 + _e98);
    let _e100 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e96 + _e100));
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

    let _e164 = (*centerColor);
    param_5 = _e164.xyz;
    let _e166 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e166;
    let _e167 = (*px);
    param_6 = (_e167 + vec2<i32>(0i, -1i));
    let _e169 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e169;
    let _e170 = (*px);
    param_7 = (_e170 + vec2<i32>(0i, 1i));
    let _e172 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e172;
    let _e173 = (*px);
    param_8 = (_e173 + vec2<i32>(-1i, 0i));
    let _e175 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e175;
    let _e176 = (*px);
    param_9 = (_e176 + vec2<i32>(1i, 0i));
    let _e178 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e178;
    let _e179 = fxN;
    let _e180 = fxS;
    let _e182 = fxW;
    let _e183 = fxE;
    let _e185 = fxM;
    fxHi = max(max(_e179, _e180), max(max(_e182, _e183), _e185));
    let _e188 = fxN;
    let _e189 = fxS;
    let _e191 = fxW;
    let _e192 = fxE;
    let _e194 = fxM;
    fxLo = min(min(_e188, _e189), min(min(_e191, _e192), _e194));
    let _e197 = fxHi;
    let _e198 = fxLo;
    fxSpan = (_e197 - _e198);
    let _e200 = fxSpan;
    let _e201 = fxHi;
    if (_e200 < max(0.0312f, (_e201 * 0.125f))) {
        let _e205 = (*centerColor);
        return _e205;
    }
    let _e206 = (*px);
    param_10 = (_e206 + vec2<i32>(-1i, -1i));
    let _e208 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e208;
    let _e209 = (*px);
    param_11 = (_e209 + vec2<i32>(1i, -1i));
    let _e211 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e211;
    let _e212 = (*px);
    param_12 = (_e212 + vec2<i32>(-1i, 1i));
    let _e214 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e214;
    let _e215 = (*px);
    param_13 = (_e215 + vec2<i32>(1i, 1i));
    let _e217 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e217;
    let _e218 = fxN;
    let _e219 = fxS;
    fxNS = (_e218 + _e219);
    let _e221 = fxW;
    let _e222 = fxE;
    fxWE = (_e221 + _e222);
    let _e224 = fxNW;
    let _e225 = fxSW;
    fxNWSW = (_e224 + _e225);
    let _e227 = fxNE;
    let _e228 = fxSE;
    fxNESE = (_e227 + _e228);
    let _e230 = fxW;
    let _e232 = fxNWSW;
    let _e235 = fxM;
    let _e237 = fxNS;
    let _e241 = fxE;
    let _e243 = fxNESE;
    fxHorz = (abs(((-2f * _e230) + _e232)) + ((abs(((-2f * _e235) + _e237)) * 2f) + abs(((-2f * _e241) + _e243))));
    let _e248 = fxS;
    let _e250 = fxSW;
    let _e251 = fxSE;
    let _e255 = fxM;
    let _e257 = fxWE;
    let _e261 = fxN;
    let _e263 = fxNW;
    let _e264 = fxNE;
    fxVert = (abs(((-2f * _e248) + (_e250 + _e251))) + ((abs(((-2f * _e255) + _e257)) * 2f) + abs(((-2f * _e261) + (_e263 + _e264)))));
    let _e270 = fxHorz;
    let _e271 = fxVert;
    fxHorzSpan = (_e270 >= _e271);
    let _e273 = fxNS;
    let _e274 = fxWE;
    let _e277 = fxNWSW;
    let _e278 = fxNESE;
    fxAround = (((_e273 + _e274) * 2f) + (_e277 + _e278));
    let _e281 = fxHorzSpan;
    let _e282 = fxN;
    let _e283 = fxW;
    fxLoSide = select(_e283, _e282, _e281);
    let _e285 = fxHorzSpan;
    let _e286 = fxS;
    let _e287 = fxE;
    fxHiSide = select(_e287, _e286, _e285);
    let _e289 = fxLoSide;
    let _e290 = fxM;
    fxGradLo = (_e289 - _e290);
    let _e292 = fxHiSide;
    let _e293 = fxM;
    fxGradHi = (_e292 - _e293);
    let _e295 = fxGradLo;
    let _e297 = fxGradHi;
    fxLoSteeper = (abs(_e295) >= abs(_e297));
    let _e300 = fxGradLo;
    let _e302 = fxGradHi;
    fxGradient = max(abs(_e300), abs(_e302));
    let _e305 = fxLoSteeper;
    if _e305 {
        let _e306 = fxLoSide;
        let _e307 = fxM;
        local = (_e306 + _e307);
    } else {
        let _e309 = fxHiSide;
        let _e310 = fxM;
        local = (_e309 + _e310);
    }
    let _e312 = local;
    fxPair = _e312;
    let _e313 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e313));
    let _e316 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e316));
    let _e319 = fxLoSteeper;
    if _e319 {
        let _e320 = fxAcross;
        fxAcross = -(_e320);
    }
    let _e322 = fxSpan;
    let _e323 = fxSpan;
    fxInvSpan = inverseSqrt((_e322 * _e323));
    let _e326 = fxAround;
    let _e328 = fxM;
    let _e331 = fxInvSpan;
    fxBlendC = clamp((abs(((_e326 * 0.083333336f) - _e328)) * _e331), 0f, 1f);
    let _e334 = fxBlendC;
    let _e337 = fxBlendC;
    let _e338 = fxBlendC;
    fxBlendF = (((-2f * _e334) + 3f) * (_e337 * _e338));
    let _e341 = fxBlendF;
    let _e342 = fxBlendF;
    fxSubpixel = ((_e341 * _e342) * 0.75f);
    let _e345 = fxPair;
    fxHalfPair = (_e345 * 0.5f);
    let _e347 = fxGradient;
    fxScaled = (_e347 * 0.25f);
    let _e349 = fxM;
    let _e350 = fxHalfPair;
    fxMidNegative = ((_e349 - _e350) < 0f);
    let _e353 = (*px);
    let _e354 = fxAlong;
    param_14 = (_e353 - _e354);
    let _e356 = fxAcross;
    param_15 = _e356;
    let _e357 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e358 = fxHalfPair;
    fxEndN = (_e357 - _e358);
    let _e360 = (*px);
    let _e361 = fxAlong;
    param_16 = (_e360 + _e361);
    let _e363 = fxAcross;
    param_17 = _e363;
    let _e364 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e365 = fxHalfPair;
    fxEndP = (_e364 - _e365);
    let _e367 = fxEndN;
    let _e369 = fxScaled;
    fxDoneN = (abs(_e367) >= _e369);
    let _e371 = fxEndP;
    let _e373 = fxScaled;
    fxDoneP = (abs(_e371) >= _e373);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e375 = i;
        if (_e375 <= 12i) {
            let _e377 = fxDoneN;
            if !(_e377) {
                let _e379 = (*px);
                let _e380 = fxAlong;
                let _e381 = i;
                param_18 = (_e379 - (_e380 * vec2(_e381)));
                let _e385 = fxAcross;
                param_19 = _e385;
                let _e386 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e387 = fxHalfPair;
                fxEndN = (_e386 - _e387);
                let _e389 = fxEndN;
                let _e391 = fxScaled;
                fxDoneN = (abs(_e389) >= _e391);
                let _e393 = i;
                fxStepsN = _e393;
            }
            let _e394 = fxDoneP;
            if !(_e394) {
                let _e396 = (*px);
                let _e397 = fxAlong;
                let _e398 = i;
                param_20 = (_e396 + (_e397 * vec2(_e398)));
                let _e402 = fxAcross;
                param_21 = _e402;
                let _e403 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e404 = fxHalfPair;
                fxEndP = (_e403 - _e404);
                let _e406 = fxEndP;
                let _e408 = fxScaled;
                fxDoneP = (abs(_e406) >= _e408);
                let _e410 = i;
                fxStepsP = _e410;
            }
            let _e411 = fxDoneN;
            let _e412 = fxDoneP;
            if (_e411 && _e412) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e414 = i;
            i = (_e414 + 1i);
        }
    }
    let _e416 = fxStepsN;
    let _e417 = fxStepsP;
    fxTowardN = (_e416 < _e417);
    let _e419 = fxTowardN;
    let _e420 = fxEndN;
    let _e421 = fxEndP;
    fxEndG = select(_e421, _e420, _e419);
    let _e423 = fxEndG;
    let _e425 = fxMidNegative;
    fxGoodSpan = ((_e423 < 0f) != _e425);
    let _e427 = fxStepsN;
    let _e428 = fxStepsP;
    fxSpanLen = f32((_e427 + _e428));
    let _e431 = fxTowardN;
    let _e432 = fxStepsN;
    let _e433 = fxStepsP;
    fxNear = f32(select(_e433, _e432, _e431));
    let _e436 = fxGoodSpan;
    if _e436 {
        let _e437 = fxNear;
        let _e438 = fxSpanLen;
        let _e439 = fxSpanLen;
        local_1 = (0.5f - (_e437 * inverseSqrt((_e438 * _e439))));
    } else {
        local_1 = 0f;
    }
    let _e444 = local_1;
    fxEdgeBlend = _e444;
    let _e445 = fxEdgeBlend;
    let _e446 = fxSubpixel;
    fxBlend = clamp(max(_e445, _e446), 0f, 1f);
    let _e449 = (*px);
    let _e450 = fxAcross;
    param_22 = (_e449 + _e450);
    let _e452 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e452.xyz;
    let _e454 = (*centerColor);
    let _e456 = fxBlend;
    let _e459 = fxAcrossColor;
    let _e460 = fxBlend;
    let _e462 = ((_e454.xyz * (1f - _e456)) + (_e459 * _e460));
    let _e464 = (*centerColor)[3u];
    return vec4<f32>(_e462.x, _e462.y, _e462.z, _e464);
}

fn samplePreviousBlur_u0028_vf2_u003b_vf3_u003b_vf3_u003b(texel: ptr<function, vec2<f32>>, tapMin: ptr<function, vec3<f32>>, tapMax: ptr<function, vec3<f32>>) -> vec3<f32> {
    var extent: vec2<f32>;
    var invExtent: vec2<f32>;
    var sum: vec3<f32>;
    var i_1: i32;
    var o: vec2<f32>;
    var tap: vec3<f32>;

    let _e101 = pc.extent;
    extent = vec2<f32>(_e101);
    let _e103 = extent;
    let _e104 = extent;
    invExtent = inverseSqrt((_e103 * _e104));
    sum = vec3<f32>(0f, 0f, 0f);
    i_1 = 0i;
    loop {
        let _e107 = i_1;
        if (_e107 < 4i) {
            let _e109 = i_1;
            let _e113 = i_1;
            o = vec2<f32>(select(-0.5f, 0.5f, ((_e109 & 1i) != 0i)), select(-0.5f, 0.5f, ((_e113 & 2i) != 0i)));
            let _e118 = (*texel);
            let _e121 = o;
            let _e123 = invExtent;
            let _e125 = textureSampleLevel(previousColor, nearestSampler, (((_e118 + vec2(0.5f)) + _e121) * _e123), 0f);
            tap = _e125.xyz;
            let _e127 = tap;
            let _e128 = sum;
            sum = (_e128 + _e127);
            let _e130 = (*tapMin);
            let _e131 = tap;
            (*tapMin) = min(_e130, _e131);
            let _e133 = (*tapMax);
            let _e134 = tap;
            (*tapMax) = max(_e133, _e134);
            continue;
        } else {
            break;
        }
        continuing {
            let _e136 = i_1;
            i_1 = (_e136 + 1i);
        }
    }
    let _e138 = sum;
    return (_e138 * 0.25f);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e98 = (*a);
    oneMinus = (1f - _e98);
    let _e100 = (*x);
    let _e101 = oneMinus;
    left = (_e100 * _e101);
    let _e103 = (*y);
    let _e104 = (*a);
    right = (_e103 * _e104);
    let _e106 = left;
    let _e107 = right;
    result = (_e106 + _e107);
    let _e109 = result;
    return _e109;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e94 = (*weight);
    let _e95 = (*confidence);
    scaled = (_e94 * _e95);
    let _e97 = scaled;
    return _e97;
}

fn historyWeightForCount_u0028_f1_u003b(count: ptr<function, f32>) -> f32 {
    let _e92 = (*count);
    let _e93 = (*count);
    let _e95 = (*count);
    return (_e92 * inverseSqrt(((_e93 + 1f) * (_e95 + 1f))));
}

fn rectifiedHistoryCount_u0028_f1_u003b_vf3_u003b_vf3_u003b(count_1: ptr<function, f32>, clampMove: ptr<function, vec3<f32>>, boxExtent: ptr<function, vec3<f32>>) -> f32 {
    var boxSize: f32;
    var rectified: f32;

    let _e96 = (*boxExtent);
    boxSize = max(length(_e96), 0.0001f);
    let _e99 = (*clampMove);
    let _e101 = boxSize;
    let _e102 = boxSize;
    rectified = clamp(((length(_e99) * inverseSqrt((_e101 * _e102))) - 0.5f), 0f, 1f);
    let _e108 = (*count_1);
    let _e109 = (*count_1);
    let _e111 = rectified;
    return max(1f, (_e108 - ((_e109 - 1f) * _e111)));
}

fn movingHistoryCount_u0028_f1_u003b(movingWeight: ptr<function, f32>) -> f32 {
    var rest: f32;

    let _e93 = (*movingWeight);
    rest = max((1f - _e93), 0.020833334f);
    let _e96 = (*movingWeight);
    let _e97 = rest;
    let _e98 = rest;
    return (_e96 * inverseSqrt((_e97 * _e98)));
}

fn accumulatedHistoryCount_u0028_f1_u003b_f1_u003b_vf2_u003b(storedCount: ptr<function, f32>, movingWeight_1: ptr<function, f32>, motionPixels: ptr<function, vec2<f32>>) -> f32 {
    var count_2: f32;
    var moving: f32;
    var param_23: f32;

    let _e97 = (*storedCount);
    count_2 = clamp(_e97, 1f, 48f);
    let _e99 = (*motionPixels);
    moving = clamp((length(_e99) * 8f), 0f, 1f);
    let _e103 = count_2;
    let _e104 = count_2;
    let _e105 = (*movingWeight_1);
    param_23 = _e105;
    let _e106 = movingHistoryCount_u0028_f1_u003b((&param_23));
    let _e108 = count_2;
    let _e110 = moving;
    return (_e103 + ((min(_e104, _e106) - _e108) * _e110));
}

fn samplePreviousColor_u0028_vf2_u003b_vf3_u003b_vf3_u003b_f1_u003b(texel_1: ptr<function, vec2<f32>>, tapMin_1: ptr<function, vec3<f32>>, tapMax_1: ptr<function, vec3<f32>>, centreCount: ptr<function, f32>) -> vec4<f32> {
    var baseFloat: vec2<f32>;
    var f: vec2<f32>;
    var d0_: vec2<f32>;
    var d1_: vec2<f32>;
    var d2_: vec2<f32>;
    var d3_: vec2<f32>;
    var w0_: vec2<f32>;
    var w1_: vec2<f32>;
    var w2_: vec2<f32>;
    var w3_: vec2<f32>;
    var w12_: vec2<f32>;
    var offset12_: vec2<f32>;
    var extent_1: vec2<f32>;
    var invExtent_1: vec2<f32>;
    var uv0_: vec2<f32>;
    var uv12_: vec2<f32>;
    var uv3_: vec2<f32>;
    var ux: array<f32, 5>;
    var uy: array<f32, 5>;
    var w: array<f32, 5>;
    var sum_1: vec4<f32>;
    var total: f32;
    var i_2: i32;
    var tap_1: vec4<f32>;

    let _e119 = (*texel_1);
    baseFloat = floor(_e119);
    let _e121 = (*texel_1);
    let _e122 = baseFloat;
    f = (_e121 - _e122);
    let _e124 = f;
    d0_ = (vec2(1f) + _e124);
    let _e127 = f;
    d1_ = _e127;
    let _e128 = f;
    d2_ = (vec2(1f) - _e128);
    let _e131 = f;
    d3_ = (vec2(2f) - _e131);
    let _e134 = d0_;
    let _e138 = d0_;
    let _e142 = d0_;
    w0_ = ((((((_e134 * -0.75f) - vec2(-3.75f)) * _e138) + vec2(-6f)) * _e142) - vec2(-3f));
    let _e146 = d1_;
    let _e150 = d1_;
    let _e152 = d1_;
    w1_ = (((((_e146 * 1.25f) - vec2(2.25f)) * _e150) * _e152) + vec2(1f));
    let _e156 = d2_;
    let _e160 = d2_;
    let _e162 = d2_;
    w2_ = (((((_e156 * 1.25f) - vec2(2.25f)) * _e160) * _e162) + vec2(1f));
    let _e166 = d3_;
    let _e170 = d3_;
    let _e174 = d3_;
    w3_ = ((((((_e166 * -0.75f) - vec2(-3.75f)) * _e170) + vec2(-6f)) * _e174) - vec2(-3f));
    let _e178 = w1_;
    let _e179 = w2_;
    w12_ = (_e178 + _e179);
    let _e181 = w2_;
    let _e182 = w12_;
    let _e183 = w12_;
    offset12_ = (_e181 * inverseSqrt((_e182 * _e183)));
    let _e188 = pc.extent;
    extent_1 = vec2<f32>(_e188);
    let _e190 = extent_1;
    let _e191 = extent_1;
    invExtent_1 = inverseSqrt((_e190 * _e191));
    let _e194 = baseFloat;
    let _e199 = invExtent_1;
    uv0_ = (((_e194 - vec2(1f)) + vec2(0.5f)) * _e199);
    let _e201 = baseFloat;
    let _e202 = offset12_;
    let _e206 = invExtent_1;
    uv12_ = (((_e201 + _e202) + vec2(0.5f)) * _e206);
    let _e208 = baseFloat;
    let _e213 = invExtent_1;
    uv3_ = (((_e208 + vec2(2f)) + vec2(0.5f)) * _e213);
    let _e216 = uv12_[0u];
    let _e218 = uv0_[0u];
    let _e220 = uv3_[0u];
    let _e222 = uv12_[0u];
    let _e224 = uv12_[0u];
    ux = array<f32, 5>(_e216, _e218, _e220, _e222, _e224);
    let _e227 = uv12_[1u];
    let _e229 = uv12_[1u];
    let _e231 = uv12_[1u];
    let _e233 = uv0_[1u];
    let _e235 = uv3_[1u];
    uy = array<f32, 5>(_e227, _e229, _e231, _e233, _e235);
    let _e238 = w12_[0u];
    let _e240 = w12_[1u];
    let _e243 = w0_[0u];
    let _e245 = w12_[1u];
    let _e248 = w3_[0u];
    let _e250 = w12_[1u];
    let _e253 = w12_[0u];
    let _e255 = w0_[1u];
    let _e258 = w12_[0u];
    let _e260 = w3_[1u];
    w = array<f32, 5>((_e238 * _e240), (_e243 * _e245), (_e248 * _e250), (_e253 * _e255), (_e258 * _e260));
    sum_1 = vec4<f32>(0f, 0f, 0f, 0f);
    total = 0f;
    i_2 = 0i;
    loop {
        let _e263 = i_2;
        if (_e263 < 5i) {
            let _e265 = i_2;
            let _e267 = ux[_e265];
            let _e268 = i_2;
            let _e270 = uy[_e268];
            let _e272 = textureSampleLevel(previousColor, nearestSampler, vec2<f32>(_e267, _e270), 0f);
            tap_1 = _e272;
            let _e273 = i_2;
            let _e275 = w[_e273];
            let _e276 = tap_1;
            let _e278 = sum_1;
            sum_1 = (_e278 + (_e276 * _e275));
            let _e280 = i_2;
            let _e282 = w[_e280];
            let _e283 = total;
            total = (_e283 + _e282);
            let _e285 = (*tapMin_1);
            let _e286 = tap_1;
            (*tapMin_1) = min(_e285, _e286.xyz);
            let _e289 = (*tapMax_1);
            let _e290 = tap_1;
            (*tapMax_1) = max(_e289, _e290.xyz);
            let _e293 = i_2;
            if (_e293 == 0i) {
                let _e296 = tap_1[3u];
                (*centreCount) = _e296;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e297 = i_2;
            i_2 = (_e297 + 1i);
        }
    }
    let _e299 = sum_1;
    let _e300 = total;
    let _e301 = total;
    return (_e299 * inverseSqrt((_e300 * _e301)));
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_172_: bool;

    let _e92 = (*v_1);
    let _e93 = (*v_1);
    let _e95 = all((_e92 == _e93));
    phi_172_ = _e95;
    if _e95 {
        let _e96 = (*v_1);
        phi_172_ = all((abs(_e96) <= vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_172_;
    return _e101;
}

fn neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b(tileIndex: ptr<function, i32>, boxMin: ptr<function, vec3<f32>>, boxMax: ptr<function, vec3<f32>>) -> bool {
    var neighborhoodMin: vec3<f32>;
    var neighborhoodMax: vec3<f32>;
    var momentSum: vec3<f32>;
    var momentSquares: vec3<f32>;
    var y_1: i32;
    var x_1: i32;
    var c_1: vec3<f32>;
    var param_24: vec3<f32>;
    var param_25: vec3<f32>;
    var momentMean: vec3<f32>;
    var momentSigma: vec3<f32>;
    var phi_1462_: bool;

    neighborhoodMin = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
    neighborhoodMax = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
    momentSum = vec3<f32>(0f, 0f, 0f);
    momentSquares = vec3<f32>(0f, 0f, 0f);
    y_1 = -1i;
    loop {
        let _e105 = y_1;
        if (_e105 <= 1i) {
            x_1 = -1i;
            loop {
                let _e107 = x_1;
                if (_e107 <= 1i) {
                    let _e109 = (*tileIndex);
                    let _e110 = y_1;
                    let _e113 = x_1;
                    let _e116 = tileColor[((_e109 + (_e110 * 10i)) + _e113)];
                    c_1 = _e116.xyz;
                    let _e118 = neighborhoodMin;
                    let _e119 = c_1;
                    neighborhoodMin = min(_e118, _e119);
                    let _e121 = neighborhoodMax;
                    let _e122 = c_1;
                    neighborhoodMax = max(_e121, _e122);
                    let _e124 = c_1;
                    let _e125 = momentSum;
                    momentSum = (_e125 + _e124);
                    let _e127 = c_1;
                    let _e128 = c_1;
                    let _e130 = momentSquares;
                    momentSquares = (_e130 + (_e127 * _e128));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e132 = x_1;
                    x_1 = (_e132 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e134 = y_1;
            y_1 = (_e134 + 1i);
        }
    }
    let _e136 = neighborhoodMin;
    param_24 = _e136;
    let _e137 = finite3_u0028_vf3_u003b((&param_24));
    phi_1462_ = _e137;
    if _e137 {
        let _e138 = neighborhoodMax;
        param_25 = _e138;
        let _e139 = finite3_u0028_vf3_u003b((&param_25));
        phi_1462_ = _e139;
    }
    let _e141 = phi_1462_;
    if !(_e141) {
        return false;
    }
    let _e143 = neighborhoodMin;
    (*boxMin) = _e143;
    let _e144 = neighborhoodMax;
    (*boxMax) = _e144;
    let _e146 = pc.qualityFlags;
    if ((_e146 & 1u) != 0u) {
        let _e149 = momentSum;
        momentMean = (_e149 * 0.11111111f);
        let _e151 = momentSquares;
        let _e153 = momentMean;
        let _e154 = momentMean;
        momentSigma = sqrt(max(((_e151 * 0.11111111f) - (_e153 * _e154)), vec3<f32>(0f, 0f, 0f)));
        let _e159 = neighborhoodMin;
        let _e160 = momentMean;
        let _e161 = momentSigma;
        (*boxMin) = max(_e159, (_e160 - (_e161 * 1.25f)));
        let _e165 = neighborhoodMax;
        let _e166 = momentMean;
        let _e167 = momentSigma;
        (*boxMax) = min(_e165, (_e166 + (_e167 * 1.25f)));
    }
    return true;
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_2: ptr<function, f32>, y_2: ptr<function, f32>, a_1: ptr<function, f32>) -> f32 {
    let _e94 = (*x_2);
    let _e95 = (*a_1);
    let _e98 = (*y_2);
    let _e99 = (*a_1);
    return ((_e94 * (1f - _e95)) + (_e98 * _e99));
}

fn samplePreviousDepthLinear_u0028_vf2_u003b(texel_2: ptr<function, vec2<f32>>) -> f32 {
    var baseFloat_1: vec2<f32>;
    var base_1: vec2<i32>;
    var f_1: vec2<f32>;
    var hi: vec2<i32>;
    var d00_: f32;
    var d10_: f32;
    var d01_: f32;
    var d11_: f32;
    var top: f32;
    var param_26: f32;
    var param_27: f32;
    var param_28: f32;
    var bottom: f32;
    var param_29: f32;
    var param_30: f32;
    var param_31: f32;
    var param_32: f32;
    var param_33: f32;
    var param_34: f32;

    let _e111 = (*texel_2);
    baseFloat_1 = floor(_e111);
    let _e113 = baseFloat_1;
    base_1 = vec2<i32>(_e113);
    let _e115 = (*texel_2);
    let _e116 = baseFloat_1;
    f_1 = (_e115 - _e116);
    let _e119 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e119) - vec2<i32>(1i, 1i));
    let _e122 = base_1;
    let _e123 = hi;
    let _e125 = textureLoad(previousLinearDepth, clamp(_e122, vec2<i32>(0i, 0i), _e123), 0i);
    d00_ = _e125.x;
    let _e127 = base_1;
    let _e129 = hi;
    let _e131 = textureLoad(previousLinearDepth, clamp((_e127 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e129), 0i);
    d10_ = _e131.x;
    let _e133 = base_1;
    let _e135 = hi;
    let _e137 = textureLoad(previousLinearDepth, clamp((_e133 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e135), 0i);
    d01_ = _e137.x;
    let _e139 = base_1;
    let _e141 = hi;
    let _e143 = textureLoad(previousLinearDepth, clamp((_e139 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e141), 0i);
    d11_ = _e143.x;
    let _e145 = d00_;
    param_26 = _e145;
    let _e146 = d10_;
    param_27 = _e146;
    let _e148 = f_1[0u];
    param_28 = _e148;
    let _e149 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_26), (&param_27), (&param_28));
    top = _e149;
    let _e150 = d01_;
    param_29 = _e150;
    let _e151 = d11_;
    param_30 = _e151;
    let _e153 = f_1[0u];
    param_31 = _e153;
    let _e154 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_29), (&param_30), (&param_31));
    bottom = _e154;
    let _e155 = top;
    param_32 = _e155;
    let _e156 = bottom;
    param_33 = _e156;
    let _e158 = f_1[1u];
    param_34 = _e158;
    let _e159 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_32), (&param_33), (&param_34));
    return _e159;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator_1: f32;
    var numerator: f32;
    var result_1: f32;

    let _e98 = pc.zFar;
    let _e100 = pc.zNear;
    range = (_e98 - _e100);
    let _e102 = (*z_1);
    let _e103 = range;
    scaled_1 = (_e102 * _e103);
    let _e106 = pc.zNear;
    let _e107 = scaled_1;
    denominator_1 = (_e106 + _e107);
    let _e110 = pc.zNear;
    let _e112 = pc.zFar;
    numerator = (_e110 * _e112);
    let _e114 = numerator;
    let _e115 = denominator_1;
    result_1 = (_e114 / _e115);
    let _e117 = result_1;
    return _e117;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_157_: bool;

    let _e92 = (*v_2);
    let _e93 = (*v_2);
    let _e95 = all((_e92 == _e93));
    phi_157_ = _e95;
    if _e95 {
        let _e96 = (*v_2);
        phi_157_ = all((abs(_e96) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_157_;
    return _e101;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_187_: bool;

    let _e92 = (*v_3);
    let _e93 = (*v_3);
    let _e95 = all((_e92 == _e93));
    phi_187_ = _e95;
    if _e95 {
        let _e96 = (*v_3);
        phi_187_ = all((abs(_e96) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_187_;
    return _e101;
}

fn dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b(px_1: ptr<function, vec2<i32>>, tileIndex_1: ptr<function, i32>, centerValid: ptr<function, f32>) -> vec2<f32> {
    var centerVelocity: vec2<f32>;
    var param_35: vec2<f32>;
    var hi_1: vec2<i32>;
    var best: vec2<i32>;
    var bestDepth: f32;
    var y_3: i32;
    var x_3: i32;
    var d: f32;
    var v_4: f32;
    var dv: vec2<f32>;
    var param_36: vec2<f32>;
    var phi_1254_: bool;
    var phi_1262_: bool;

    let _e105 = (*px_1);
    let _e106 = textureLoad(motionVelocity, _e105, 0i);
    centerVelocity = _e106.xy;
    let _e109 = pc.qualityFlags;
    let _e111 = ((_e109 & 2u) == 0u);
    phi_1254_ = _e111;
    if !(_e111) {
        let _e113 = (*centerValid);
        phi_1254_ = !((_e113 > 0f));
    }
    let _e117 = phi_1254_;
    phi_1262_ = _e117;
    if !(_e117) {
        let _e119 = centerVelocity;
        param_35 = _e119;
        let _e120 = finite2_u0028_vf2_u003b((&param_35));
        phi_1262_ = !(_e120);
    }
    let _e123 = phi_1262_;
    if _e123 {
        let _e124 = centerVelocity;
        return _e124;
    }
    let _e126 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e126) - vec2<i32>(1i, 1i));
    let _e129 = (*px_1);
    best = _e129;
    let _e130 = (*tileIndex_1);
    let _e132 = tileDepth[_e130];
    bestDepth = _e132;
    y_3 = -1i;
    loop {
        let _e133 = y_3;
        if (_e133 <= 1i) {
            x_3 = -1i;
            loop {
                let _e135 = x_3;
                if (_e135 <= 1i) {
                    let _e137 = (*tileIndex_1);
                    let _e138 = y_3;
                    let _e141 = x_3;
                    let _e144 = tileDepth[((_e137 + (_e138 * 10i)) + _e141)];
                    d = _e144;
                    let _e145 = d;
                    let _e146 = bestDepth;
                    if (_e145 > _e146) {
                        let _e148 = d;
                        bestDepth = _e148;
                        let _e149 = (*px_1);
                        let _e150 = x_3;
                        let _e151 = y_3;
                        let _e154 = hi_1;
                        best = clamp((_e149 + vec2<i32>(_e150, _e151)), vec2<i32>(0i, 0i), _e154);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e156 = x_3;
                    x_3 = (_e156 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e158 = y_3;
            y_3 = (_e158 + 1i);
        }
    }
    let _e160 = best;
    let _e161 = (*px_1);
    if all((_e160 == _e161)) {
        let _e164 = centerVelocity;
        return _e164;
    }
    let _e165 = best;
    let _e166 = textureLoad(motionValidity, _e165, 0i);
    v_4 = _e166.x;
    let _e168 = v_4;
    if !((_e168 > 0f)) {
        let _e171 = centerVelocity;
        return _e171;
    }
    let _e172 = best;
    let _e173 = textureLoad(motionVelocity, _e172, 0i);
    dv = _e173.xy;
    let _e175 = dv;
    param_36 = _e175;
    let _e176 = finite2_u0028_vf2_u003b((&param_36));
    let _e177 = dv;
    let _e178 = centerVelocity;
    return select(_e178, _e177, vec2(_e176));
}

fn reconstructCurrent_u0028_vi2_u003b_i1_u003b(pixel_1: ptr<function, vec2<i32>>, tileIndex_2: ptr<function, i32>) -> vec4<f32> {
    var extent_2: vec2<f32>;
    var sampleAt: vec2<f32>;

    let _e96 = pc.extent;
    extent_2 = vec2<f32>(_e96);
    let _e98 = (*pixel_1);
    let _e102 = extent_2;
    let _e103 = extent_2;
    let _e108 = pc.currentEffectiveJitterUv;
    sampleAt = (((vec2<f32>(_e98) + vec2(0.5f)) * inverseSqrt((_e102 * _e103))) + _e108);
    let _e110 = sampleAt;
    let _e111 = textureSampleLevel(currentColor, nearestSampler, _e110, 0f);
    let _e112 = _e111.xyz;
    let _e113 = (*tileIndex_2);
    let _e116 = tileColor[_e113][3u];
    return vec4<f32>(_e112.x, _e112.y, _e112.z, _e116);
}

fn main_1() {
    var pixel_2: vec2<i32>;
    var tileOrigin: vec2<i32>;
    var imageHi: vec2<i32>;
    var i_3: u32;
    var q: vec2<i32>;
    var tileIndex_3: i32;
    var clipMin: vec3<f32>;
    var clipMax: vec3<f32>;
    var current: vec4<f32>;
    var param_37: vec2<i32>;
    var param_38: i32;
    var result_2: vec4<f32>;
    var displayed: vec4<f32>;
    var usedHistory: bool;
    var historyCount: f32;
    var valid: f32;
    var velocity: vec2<f32>;
    var param_39: vec2<i32>;
    var param_40: i32;
    var param_41: f32;
    var motionTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_42: vec4<f32>;
    var param_43: f32;
    var param_44: vec2<f32>;
    var param_45: vec2<f32>;
    var currentDevice: f32;
    var deviceLo: f32;
    var deviceHi: f32;
    var y_4: i32;
    var x_4: i32;
    var d_1: f32;
    var currentLinear: f32;
    var param_46: f32;
    var rangeA: f32;
    var param_47: f32;
    var rangeB: f32;
    var param_48: f32;
    var priorLinear: f32;
    var param_49: vec2<f32>;
    var depthLimit: f32;
    var param_50: f32;
    var param_51: f32;
    var param_52: f32;
    var param_53: f32;
    var boxValid: bool;
    var param_54: i32;
    var param_55: vec3<f32>;
    var param_56: vec3<f32>;
    var tapMin_2: vec3<f32>;
    var tapMax_2: vec3<f32>;
    var centreCount_1: f32;
    var prior: vec4<f32>;
    var param_57: vec2<f32>;
    var param_58: vec3<f32>;
    var param_59: vec3<f32>;
    var param_60: f32;
    var param_61: vec4<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var priorCount: f32;
    var param_62: f32;
    var param_63: f32;
    var param_64: vec2<f32>;
    var param_65: f32;
    var param_66: vec3<f32>;
    var param_67: vec3<f32>;
    var baseWeight: f32;
    var param_68: f32;
    var weight_1: f32;
    var param_69: f32;
    var param_70: f32;
    var resolvedRgb: vec3<f32>;
    var param_71: vec3<f32>;
    var param_72: vec3<f32>;
    var param_73: f32;
    var detail: vec3<f32>;
    var param_74: vec2<f32>;
    var param_75: vec3<f32>;
    var param_76: vec3<f32>;
    var param_77: vec4<f32>;
    var param_78: vec2<i32>;
    var param_79: vec4<f32>;
    var param_80: vec4<f32>;
    var param_81: vec4<f32>;
    var param_82: f32;
    var phi_1651_: bool;
    var phi_1660_: bool;
    var phi_1669_: bool;
    var phi_1675_: bool;
    var phi_1758_: bool;
    var phi_1764_: bool;
    var phi_1770_: bool;
    var phi_1786_: bool;
    var phi_1796_: bool;
    var phi_1829_: bool;
    var phi_1943_: bool;
    var phi_1951_: bool;
    var phi_1974_: bool;

    let _e179 = gl_GlobalInvocationID_1;
    pixel_2 = bitcast<vec2<i32>>(_e179.xy);
    let _e182 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e182.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e189 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e189) - vec2<i32>(1i, 1i));
    let _e192 = gl_LocalInvocationIndex_1;
    i_3 = _e192;
    loop {
        let _e193 = i_3;
        if (_e193 < 100u) {
            let _e195 = tileOrigin;
            let _e196 = i_3;
            let _e199 = i_3;
            let _e204 = imageHi;
            q = clamp((_e195 + vec2<i32>(bitcast<i32>((_e196 % 10u)), bitcast<i32>((_e199 / 10u)))), vec2<i32>(0i, 0i), _e204);
            let _e206 = i_3;
            let _e207 = q;
            let _e208 = textureLoad(currentColor, _e207, 0i);
            tileColor[_e206] = _e208;
            let _e210 = i_3;
            let _e211 = q;
            let _e212 = textureLoad(currentDeviceDepth, _e211, 0i);
            tileDepth[_e210] = _e212.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e215 = i_3;
            i_3 = (_e215 + 64u);
        }
    }
    workgroupBarrier();
    let _e217 = pixel_2;
    let _e219 = pc.extent;
    if any((_e217 >= bitcast<vec2<i32>>(_e219))) {
        return;
    }
    let _e224 = gl_LocalInvocationID_1[1u];
    let _e229 = gl_LocalInvocationID_1[0u];
    tileIndex_3 = ((((bitcast<i32>(_e224) + 1i) * 10i) + bitcast<i32>(_e229)) + 1i);
    clipMin = vec3<f32>(0f, 0f, 0f);
    clipMax = vec3<f32>(0f, 0f, 0f);
    let _e233 = pixel_2;
    param_37 = _e233;
    let _e234 = tileIndex_3;
    param_38 = _e234;
    let _e235 = reconstructCurrent_u0028_vi2_u003b_i1_u003b((&param_37), (&param_38));
    current = _e235;
    let _e236 = current;
    result_2 = _e236;
    let _e237 = current;
    displayed = _e237;
    usedHistory = false;
    historyCount = 1f;
    let _e238 = pixel_2;
    let _e239 = textureLoad(motionValidity, _e238, 0i);
    valid = _e239.x;
    let _e241 = pixel_2;
    param_39 = _e241;
    let _e242 = tileIndex_3;
    param_40 = _e242;
    let _e243 = valid;
    param_41 = _e243;
    let _e244 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_39), (&param_40), (&param_41));
    velocity = _e244;
    let _e245 = velocity;
    let _e247 = pc.extent;
    motionTexel = (_e245 * vec2<f32>(_e247));
    let _e250 = pixel_2;
    let _e252 = motionTexel;
    previousTexel = (vec2<f32>(_e250) - _e252);
    let _e254 = previousTexel;
    previousCenter = (_e254 + vec2(0.5f));
    let _e257 = previousCenter;
    let _e259 = all((_e257 >= vec2<f32>(0f, 0f)));
    phi_1651_ = _e259;
    if _e259 {
        let _e260 = previousCenter;
        let _e262 = pc.extent;
        phi_1651_ = all((_e260 < vec2<f32>(_e262)));
    }
    let _e267 = phi_1651_;
    inside = _e267;
    let _e268 = current;
    param_42 = _e268;
    let _e269 = finite4_u0028_vf4_u003b((&param_42));
    phi_1660_ = _e269;
    if _e269 {
        let _e270 = valid;
        param_43 = _e270;
        let _e271 = finite1_u0028_f1_u003b((&param_43));
        phi_1660_ = _e271;
    }
    let _e273 = phi_1660_;
    let _e274 = valid;
    let _e276 = (_e273 && (_e274 > 0f));
    phi_1669_ = _e276;
    if _e276 {
        let _e277 = velocity;
        param_44 = _e277;
        let _e278 = finite2_u0028_vf2_u003b((&param_44));
        phi_1669_ = _e278;
    }
    let _e280 = phi_1669_;
    phi_1675_ = _e280;
    if _e280 {
        let _e281 = previousTexel;
        param_45 = _e281;
        let _e282 = finite2_u0028_vf2_u003b((&param_45));
        phi_1675_ = _e282;
    }
    let _e284 = phi_1675_;
    let _e285 = inside;
    if (_e284 && _e285) {
        let _e287 = tileIndex_3;
        let _e289 = tileDepth[_e287];
        currentDevice = _e289;
        let _e290 = currentDevice;
        deviceLo = _e290;
        let _e291 = currentDevice;
        deviceHi = _e291;
        y_4 = -1i;
        loop {
            let _e292 = y_4;
            if (_e292 <= 1i) {
                x_4 = -1i;
                loop {
                    let _e294 = x_4;
                    if (_e294 <= 1i) {
                        let _e296 = tileIndex_3;
                        let _e297 = y_4;
                        let _e300 = x_4;
                        let _e303 = tileDepth[((_e296 + (_e297 * 10i)) + _e300)];
                        d_1 = _e303;
                        let _e304 = deviceLo;
                        let _e305 = d_1;
                        deviceLo = min(_e304, _e305);
                        let _e307 = deviceHi;
                        let _e308 = d_1;
                        deviceHi = max(_e307, _e308);
                        continue;
                    } else {
                        break;
                    }
                    continuing {
                        let _e310 = x_4;
                        x_4 = (_e310 + 1i);
                    }
                }
                continue;
            } else {
                break;
            }
            continuing {
                let _e312 = y_4;
                y_4 = (_e312 + 1i);
            }
        }
        let _e314 = currentDevice;
        param_46 = _e314;
        let _e315 = linearizeDepth_u0028_f1_u003b((&param_46));
        currentLinear = _e315;
        let _e316 = deviceLo;
        param_47 = _e316;
        let _e317 = linearizeDepth_u0028_f1_u003b((&param_47));
        rangeA = _e317;
        let _e318 = deviceHi;
        param_48 = _e318;
        let _e319 = linearizeDepth_u0028_f1_u003b((&param_48));
        rangeB = _e319;
        let _e320 = previousTexel;
        param_49 = _e320;
        let _e321 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_49));
        priorLinear = _e321;
        let _e323 = pc.depthThresholdAbsolute;
        let _e325 = pc.depthThresholdRelative;
        let _e326 = currentLinear;
        let _e327 = priorLinear;
        depthLimit = max(_e323, (_e325 * max(_e326, _e327)));
        let _e331 = currentLinear;
        param_50 = _e331;
        let _e332 = finite1_u0028_f1_u003b((&param_50));
        phi_1758_ = _e332;
        if _e332 {
            let _e333 = priorLinear;
            param_51 = _e333;
            let _e334 = finite1_u0028_f1_u003b((&param_51));
            phi_1758_ = _e334;
        }
        let _e336 = phi_1758_;
        phi_1764_ = _e336;
        if _e336 {
            let _e337 = rangeA;
            param_52 = _e337;
            let _e338 = finite1_u0028_f1_u003b((&param_52));
            phi_1764_ = _e338;
        }
        let _e340 = phi_1764_;
        phi_1770_ = _e340;
        if _e340 {
            let _e341 = rangeB;
            param_53 = _e341;
            let _e342 = finite1_u0028_f1_u003b((&param_53));
            phi_1770_ = _e342;
        }
        let _e344 = phi_1770_;
        let _e345 = currentLinear;
        let _e348 = priorLinear;
        let _e350 = ((_e344 && (_e345 > 0f)) && (_e348 > 0f));
        phi_1786_ = _e350;
        if _e350 {
            let _e351 = priorLinear;
            let _e352 = rangeA;
            let _e353 = rangeB;
            let _e355 = depthLimit;
            phi_1786_ = (_e351 >= (min(_e352, _e353) - _e355));
        }
        let _e359 = phi_1786_;
        phi_1796_ = _e359;
        if _e359 {
            let _e360 = priorLinear;
            let _e361 = rangeA;
            let _e362 = rangeB;
            let _e364 = depthLimit;
            phi_1796_ = (_e360 <= (max(_e361, _e362) + _e364));
        }
        let _e368 = phi_1796_;
        if _e368 {
            let _e369 = tileIndex_3;
            param_54 = _e369;
            let _e370 = neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b((&param_54), (&param_55), (&param_56));
            let _e371 = param_55;
            clipMin = _e371;
            let _e372 = param_56;
            clipMax = _e372;
            boxValid = _e370;
            tapMin_2 = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
            tapMax_2 = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
            centreCount_1 = 0x1.fffffep+127f;
            let _e373 = previousTexel;
            param_57 = _e373;
            let _e374 = tapMin_2;
            param_58 = _e374;
            let _e375 = tapMax_2;
            param_59 = _e375;
            let _e376 = centreCount_1;
            param_60 = _e376;
            let _e377 = samplePreviousColor_u0028_vf2_u003b_vf3_u003b_vf3_u003b_f1_u003b((&param_57), (&param_58), (&param_59), (&param_60));
            let _e378 = param_58;
            tapMin_2 = _e378;
            let _e379 = param_59;
            tapMax_2 = _e379;
            let _e380 = param_60;
            centreCount_1 = _e380;
            prior = _e377;
            let _e381 = boxValid;
            phi_1829_ = _e381;
            if _e381 {
                let _e382 = prior;
                param_61 = _e382;
                let _e383 = finite4_u0028_vf4_u003b((&param_61));
                phi_1829_ = _e383;
            }
            let _e385 = phi_1829_;
            if _e385 {
                let _e386 = prior;
                let _e388 = clipMin;
                let _e389 = clipMax;
                clampedPrior = clamp(_e386.xyz, _e388, _e389);
                let _e391 = valid;
                confidence_1 = clamp(_e391, 0f, 1f);
                let _e394 = pc.historyWeight;
                let _e396 = centreCount_1;
                param_62 = _e396;
                param_63 = clamp(_e394, 0f, 1f);
                let _e397 = motionTexel;
                param_64 = _e397;
                let _e398 = accumulatedHistoryCount_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_62), (&param_63), (&param_64));
                priorCount = _e398;
                let _e399 = prior;
                let _e401 = clampedPrior;
                let _e403 = clipMax;
                let _e404 = clipMin;
                let _e406 = priorCount;
                param_65 = _e406;
                param_66 = (_e399.xyz - _e401);
                param_67 = (_e403 - _e404);
                let _e407 = rectifiedHistoryCount_u0028_f1_u003b_vf3_u003b_vf3_u003b((&param_65), (&param_66), (&param_67));
                priorCount = _e407;
                let _e408 = priorCount;
                param_68 = _e408;
                let _e409 = historyWeightForCount_u0028_f1_u003b((&param_68));
                baseWeight = _e409;
                let _e410 = baseWeight;
                param_69 = _e410;
                let _e411 = confidence_1;
                param_70 = _e411;
                let _e412 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_69), (&param_70));
                weight_1 = _e412;
                let _e413 = current;
                param_71 = _e413.xyz;
                let _e415 = clampedPrior;
                param_72 = _e415;
                let _e416 = weight_1;
                param_73 = _e416;
                let _e417 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_71), (&param_72), (&param_73));
                resolvedRgb = _e417;
                let _e418 = resolvedRgb;
                let _e420 = current[3u];
                result_2 = vec4<f32>(_e418.x, _e418.y, _e418.z, _e420);
                let _e425 = result_2;
                displayed = _e425;
                let _e426 = priorCount;
                let _e427 = confidence_1;
                historyCount = min(((_e426 * _e427) + 1f), 48f);
                let _e432 = pc.qualityFlags;
                if ((_e432 & 8u) == 0u) {
                    let _e435 = prior;
                    let _e437 = previousTexel;
                    param_74 = _e437;
                    let _e438 = tapMin_2;
                    param_75 = _e438;
                    let _e439 = tapMax_2;
                    param_76 = _e439;
                    let _e440 = samplePreviousBlur_u0028_vf2_u003b_vf3_u003b_vf3_u003b((&param_74), (&param_75), (&param_76));
                    let _e441 = param_75;
                    tapMin_2 = _e441;
                    let _e442 = param_76;
                    tapMax_2 = _e442;
                    detail = (_e435.xyz - _e440);
                    let _e444 = resolvedRgb;
                    let _e445 = confidence_1;
                    let _e447 = detail;
                    let _e450 = resolvedRgb;
                    let _e451 = tapMin_2;
                    let _e453 = resolvedRgb;
                    let _e454 = tapMax_2;
                    let _e456 = clamp((_e444 + (_e447 * (2.4f * _e445))), min(_e450, _e451), max(_e453, _e454));
                    let _e458 = current[3u];
                    displayed = vec4<f32>(_e456.x, _e456.y, _e456.z, _e458);
                }
                usedHistory = true;
            }
        }
    }
    let _e463 = usedHistory;
    let _e464 = !(_e463);
    phi_1943_ = _e464;
    if _e464 {
        let _e465 = current;
        param_77 = _e465;
        let _e466 = finite4_u0028_vf4_u003b((&param_77));
        phi_1943_ = _e466;
    }
    let _e468 = phi_1943_;
    phi_1951_ = _e468;
    if _e468 {
        let _e470 = pc.qualityFlags;
        phi_1951_ = ((_e470 & 4u) != 0u);
    }
    let _e474 = phi_1951_;
    if _e474 {
        let _e475 = pixel_2;
        param_78 = _e475;
        let _e476 = current;
        param_79 = _e476;
        let _e477 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_78), (&param_79));
        result_2 = _e477;
    }
    let _e478 = result_2;
    param_80 = _e478;
    let _e479 = finite4_u0028_vf4_u003b((&param_80));
    if !(_e479) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e481 = usedHistory;
    let _e482 = !(_e481);
    phi_1974_ = _e482;
    if !(_e482) {
        let _e484 = displayed;
        param_81 = _e484;
        let _e485 = finite4_u0028_vf4_u003b((&param_81));
        phi_1974_ = !(_e485);
    }
    let _e488 = phi_1974_;
    if _e488 {
        let _e489 = result_2;
        displayed = _e489;
    }
    let _e490 = pixel_2;
    let _e491 = displayed;
    textureStore(resolvedColor, _e490, _e491);
    let _e492 = pixel_2;
    let _e493 = result_2;
    let _e494 = _e493.xyz;
    let _e495 = historyCount;
    textureStore(historyOutColor, _e492, vec4<f32>(_e494.x, _e494.y, _e494.z, _e495));
    let _e500 = pixel_2;
    let _e501 = tileIndex_3;
    let _e503 = tileDepth[_e501];
    param_82 = _e503;
    let _e504 = storeLinearDepth_u0028_f1_u003b((&param_82));
    textureStore(historyOutLinearDepth, _e500, vec4(_e504));
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
