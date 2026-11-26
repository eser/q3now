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
    var phi_153_: bool;

    let _e92 = (*v);
    let _e93 = (*v);
    let _e94 = (_e92 == _e93);
    phi_153_ = _e94;
    if _e94 {
        let _e95 = (*v);
        phi_153_ = (abs(_e95) <= 0x1.fffffep+127f);
    }
    let _e99 = phi_153_;
    return _e99;
}

fn edgeLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
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

fn edgeLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e94 = (*p);
    param_1 = _e94;
    let _e95 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e95.xyz;
    let _e97 = edgeLuma_u0028_vf3_u003b((&param_2));
    return _e97;
}

fn edgeMidLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e95 = (*base);
    param_3 = _e95;
    let _e96 = edgeLumaAt_u0028_vi2_u003b((&param_3));
    let _e97 = (*base);
    let _e98 = (*across);
    param_4 = (_e97 + _e98);
    let _e100 = edgeLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e96 + _e100));
}

fn spatialEdgeFilter_u0028_vi2_u003b_vf4_u003b(px: ptr<function, vec2<i32>>, centerColor: ptr<function, vec4<f32>>) -> vec4<f32> {
    var edgeM: f32;
    var param_5: vec3<f32>;
    var edgeN: f32;
    var param_6: vec2<i32>;
    var edgeS: f32;
    var param_7: vec2<i32>;
    var edgeW: f32;
    var param_8: vec2<i32>;
    var edgeE: f32;
    var param_9: vec2<i32>;
    var edgeHi: f32;
    var edgeLo: f32;
    var edgeSpan: f32;
    var edgeNW: f32;
    var param_10: vec2<i32>;
    var edgeNE: f32;
    var param_11: vec2<i32>;
    var edgeSW: f32;
    var param_12: vec2<i32>;
    var edgeSE: f32;
    var param_13: vec2<i32>;
    var edgeNS: f32;
    var edgeWE: f32;
    var edgeNWSW: f32;
    var edgeNESE: f32;
    var edgeHorz: f32;
    var edgeVert: f32;
    var edgeHorzSpan: bool;
    var edgeAround: f32;
    var edgeLoSide: f32;
    var edgeHiSide: f32;
    var edgeGradLo: f32;
    var edgeGradHi: f32;
    var edgeLoSteeper: bool;
    var edgeGradient: f32;
    var edgePair: f32;
    var local: f32;
    var edgeAcross: vec2<i32>;
    var edgeAlong: vec2<i32>;
    var edgeInvSpan: f32;
    var edgeBlendC: f32;
    var edgeBlendF: f32;
    var edgeSubpixel: f32;
    var edgeHalfPair: f32;
    var edgeScaled: f32;
    var edgeMidNegative: bool;
    var edgeEndN: f32;
    var param_14: vec2<i32>;
    var param_15: vec2<i32>;
    var edgeEndP: f32;
    var param_16: vec2<i32>;
    var param_17: vec2<i32>;
    var edgeDoneN: bool;
    var edgeDoneP: bool;
    var edgeStepsN: i32;
    var edgeStepsP: i32;
    var i: i32;
    var param_18: vec2<i32>;
    var param_19: vec2<i32>;
    var param_20: vec2<i32>;
    var param_21: vec2<i32>;
    var edgeTowardN: bool;
    var edgeEndG: f32;
    var edgeGoodSpan: bool;
    var edgeSpanLen: f32;
    var edgeNear: f32;
    var edgeStepBlend: f32;
    var local_1: f32;
    var edgeBlend: f32;
    var edgeAcrossColor: vec3<f32>;
    var param_22: vec2<i32>;

    let _e164 = (*centerColor);
    param_5 = _e164.xyz;
    let _e166 = edgeLuma_u0028_vf3_u003b((&param_5));
    edgeM = _e166;
    let _e167 = (*px);
    param_6 = (_e167 + vec2<i32>(0i, -1i));
    let _e169 = edgeLumaAt_u0028_vi2_u003b((&param_6));
    edgeN = _e169;
    let _e170 = (*px);
    param_7 = (_e170 + vec2<i32>(0i, 1i));
    let _e172 = edgeLumaAt_u0028_vi2_u003b((&param_7));
    edgeS = _e172;
    let _e173 = (*px);
    param_8 = (_e173 + vec2<i32>(-1i, 0i));
    let _e175 = edgeLumaAt_u0028_vi2_u003b((&param_8));
    edgeW = _e175;
    let _e176 = (*px);
    param_9 = (_e176 + vec2<i32>(1i, 0i));
    let _e178 = edgeLumaAt_u0028_vi2_u003b((&param_9));
    edgeE = _e178;
    let _e179 = edgeN;
    let _e180 = edgeS;
    let _e182 = edgeW;
    let _e183 = edgeE;
    let _e185 = edgeM;
    edgeHi = max(max(_e179, _e180), max(max(_e182, _e183), _e185));
    let _e188 = edgeN;
    let _e189 = edgeS;
    let _e191 = edgeW;
    let _e192 = edgeE;
    let _e194 = edgeM;
    edgeLo = min(min(_e188, _e189), min(min(_e191, _e192), _e194));
    let _e197 = edgeHi;
    let _e198 = edgeLo;
    edgeSpan = (_e197 - _e198);
    let _e200 = edgeSpan;
    let _e201 = edgeHi;
    if (_e200 < max(0.0312f, (_e201 * 0.125f))) {
        let _e205 = (*centerColor);
        return _e205;
    }
    let _e206 = (*px);
    param_10 = (_e206 + vec2<i32>(-1i, -1i));
    let _e208 = edgeLumaAt_u0028_vi2_u003b((&param_10));
    edgeNW = _e208;
    let _e209 = (*px);
    param_11 = (_e209 + vec2<i32>(1i, -1i));
    let _e211 = edgeLumaAt_u0028_vi2_u003b((&param_11));
    edgeNE = _e211;
    let _e212 = (*px);
    param_12 = (_e212 + vec2<i32>(-1i, 1i));
    let _e214 = edgeLumaAt_u0028_vi2_u003b((&param_12));
    edgeSW = _e214;
    let _e215 = (*px);
    param_13 = (_e215 + vec2<i32>(1i, 1i));
    let _e217 = edgeLumaAt_u0028_vi2_u003b((&param_13));
    edgeSE = _e217;
    let _e218 = edgeN;
    let _e219 = edgeS;
    edgeNS = (_e218 + _e219);
    let _e221 = edgeW;
    let _e222 = edgeE;
    edgeWE = (_e221 + _e222);
    let _e224 = edgeNW;
    let _e225 = edgeSW;
    edgeNWSW = (_e224 + _e225);
    let _e227 = edgeNE;
    let _e228 = edgeSE;
    edgeNESE = (_e227 + _e228);
    let _e230 = edgeW;
    let _e232 = edgeNWSW;
    let _e235 = edgeM;
    let _e237 = edgeNS;
    let _e241 = edgeE;
    let _e243 = edgeNESE;
    edgeHorz = (abs(((-2f * _e230) + _e232)) + ((abs(((-2f * _e235) + _e237)) * 2f) + abs(((-2f * _e241) + _e243))));
    let _e248 = edgeS;
    let _e250 = edgeSW;
    let _e251 = edgeSE;
    let _e255 = edgeM;
    let _e257 = edgeWE;
    let _e261 = edgeN;
    let _e263 = edgeNW;
    let _e264 = edgeNE;
    edgeVert = (abs(((-2f * _e248) + (_e250 + _e251))) + ((abs(((-2f * _e255) + _e257)) * 2f) + abs(((-2f * _e261) + (_e263 + _e264)))));
    let _e270 = edgeHorz;
    let _e271 = edgeVert;
    edgeHorzSpan = (_e270 >= _e271);
    let _e273 = edgeNS;
    let _e274 = edgeWE;
    let _e277 = edgeNWSW;
    let _e278 = edgeNESE;
    edgeAround = (((_e273 + _e274) * 2f) + (_e277 + _e278));
    let _e281 = edgeHorzSpan;
    let _e282 = edgeN;
    let _e283 = edgeW;
    edgeLoSide = select(_e283, _e282, _e281);
    let _e285 = edgeHorzSpan;
    let _e286 = edgeS;
    let _e287 = edgeE;
    edgeHiSide = select(_e287, _e286, _e285);
    let _e289 = edgeLoSide;
    let _e290 = edgeM;
    edgeGradLo = (_e289 - _e290);
    let _e292 = edgeHiSide;
    let _e293 = edgeM;
    edgeGradHi = (_e292 - _e293);
    let _e295 = edgeGradLo;
    let _e297 = edgeGradHi;
    edgeLoSteeper = (abs(_e295) >= abs(_e297));
    let _e300 = edgeGradLo;
    let _e302 = edgeGradHi;
    edgeGradient = max(abs(_e300), abs(_e302));
    let _e305 = edgeLoSteeper;
    if _e305 {
        let _e306 = edgeLoSide;
        let _e307 = edgeM;
        local = (_e306 + _e307);
    } else {
        let _e309 = edgeHiSide;
        let _e310 = edgeM;
        local = (_e309 + _e310);
    }
    let _e312 = local;
    edgePair = _e312;
    let _e313 = edgeHorzSpan;
    edgeAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e313));
    let _e316 = edgeHorzSpan;
    edgeAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e316));
    let _e319 = edgeLoSteeper;
    if _e319 {
        let _e320 = edgeAcross;
        edgeAcross = -(_e320);
    }
    let _e322 = edgeSpan;
    let _e323 = edgeSpan;
    edgeInvSpan = inverseSqrt((_e322 * _e323));
    let _e326 = edgeAround;
    let _e328 = edgeM;
    let _e331 = edgeInvSpan;
    edgeBlendC = clamp((abs(((_e326 * 0.083333336f) - _e328)) * _e331), 0f, 1f);
    let _e334 = edgeBlendC;
    let _e337 = edgeBlendC;
    let _e338 = edgeBlendC;
    edgeBlendF = (((-2f * _e334) + 3f) * (_e337 * _e338));
    let _e341 = edgeBlendF;
    let _e342 = edgeBlendF;
    edgeSubpixel = ((_e341 * _e342) * 0.75f);
    let _e345 = edgePair;
    edgeHalfPair = (_e345 * 0.5f);
    let _e347 = edgeGradient;
    edgeScaled = (_e347 * 0.25f);
    let _e349 = edgeM;
    let _e350 = edgeHalfPair;
    edgeMidNegative = ((_e349 - _e350) < 0f);
    let _e353 = (*px);
    let _e354 = edgeAlong;
    param_14 = (_e353 - _e354);
    let _e356 = edgeAcross;
    param_15 = _e356;
    let _e357 = edgeMidLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e358 = edgeHalfPair;
    edgeEndN = (_e357 - _e358);
    let _e360 = (*px);
    let _e361 = edgeAlong;
    param_16 = (_e360 + _e361);
    let _e363 = edgeAcross;
    param_17 = _e363;
    let _e364 = edgeMidLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e365 = edgeHalfPair;
    edgeEndP = (_e364 - _e365);
    let _e367 = edgeEndN;
    let _e369 = edgeScaled;
    edgeDoneN = (abs(_e367) >= _e369);
    let _e371 = edgeEndP;
    let _e373 = edgeScaled;
    edgeDoneP = (abs(_e371) >= _e373);
    edgeStepsN = 1i;
    edgeStepsP = 1i;
    i = 2i;
    loop {
        let _e375 = i;
        if (_e375 <= 12i) {
            let _e377 = edgeDoneN;
            if !(_e377) {
                let _e379 = (*px);
                let _e380 = edgeAlong;
                let _e381 = i;
                param_18 = (_e379 - (_e380 * vec2(_e381)));
                let _e385 = edgeAcross;
                param_19 = _e385;
                let _e386 = edgeMidLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e387 = edgeHalfPair;
                edgeEndN = (_e386 - _e387);
                let _e389 = edgeEndN;
                let _e391 = edgeScaled;
                edgeDoneN = (abs(_e389) >= _e391);
                let _e393 = i;
                edgeStepsN = _e393;
            }
            let _e394 = edgeDoneP;
            if !(_e394) {
                let _e396 = (*px);
                let _e397 = edgeAlong;
                let _e398 = i;
                param_20 = (_e396 + (_e397 * vec2(_e398)));
                let _e402 = edgeAcross;
                param_21 = _e402;
                let _e403 = edgeMidLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e404 = edgeHalfPair;
                edgeEndP = (_e403 - _e404);
                let _e406 = edgeEndP;
                let _e408 = edgeScaled;
                edgeDoneP = (abs(_e406) >= _e408);
                let _e410 = i;
                edgeStepsP = _e410;
            }
            let _e411 = edgeDoneN;
            let _e412 = edgeDoneP;
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
    let _e416 = edgeStepsN;
    let _e417 = edgeStepsP;
    edgeTowardN = (_e416 < _e417);
    let _e419 = edgeTowardN;
    let _e420 = edgeEndN;
    let _e421 = edgeEndP;
    edgeEndG = select(_e421, _e420, _e419);
    let _e423 = edgeEndG;
    let _e425 = edgeMidNegative;
    edgeGoodSpan = ((_e423 < 0f) != _e425);
    let _e427 = edgeStepsN;
    let _e428 = edgeStepsP;
    edgeSpanLen = f32((_e427 + _e428));
    let _e431 = edgeTowardN;
    let _e432 = edgeStepsN;
    let _e433 = edgeStepsP;
    edgeNear = f32(select(_e433, _e432, _e431));
    let _e436 = edgeGoodSpan;
    if _e436 {
        let _e437 = edgeNear;
        let _e438 = edgeSpanLen;
        let _e439 = edgeSpanLen;
        local_1 = (0.5f - (_e437 * inverseSqrt((_e438 * _e439))));
    } else {
        local_1 = 0f;
    }
    let _e444 = local_1;
    edgeStepBlend = _e444;
    let _e445 = edgeStepBlend;
    let _e446 = edgeSubpixel;
    edgeBlend = clamp(max(_e445, _e446), 0f, 1f);
    let _e449 = (*px);
    let _e450 = edgeAcross;
    param_22 = (_e449 + _e450);
    let _e452 = sampleCurrent_u0028_vi2_u003b((&param_22));
    edgeAcrossColor = _e452.xyz;
    let _e454 = (*centerColor);
    let _e456 = edgeBlend;
    let _e459 = edgeAcrossColor;
    let _e460 = edgeBlend;
    let _e462 = ((_e454.xyz * (1f - _e456)) + (_e459 * _e460));
    let _e464 = (*centerColor)[3u];
    return vec4<f32>(_e462.x, _e462.y, _e462.z, _e464);
}

fn movingSpeedScale_u0028_f1_u003b(pixelsPerFrame: ptr<function, f32>) -> f32 {
    var s: f32;

    let _e93 = (*pixelsPerFrame);
    s = (1f + (0.25f * _e93));
    let _e96 = s;
    let _e97 = s;
    return inverseSqrt((_e96 * _e97));
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

fn movingHistoryScale_u0028_f1_u003b(pixelsPerFrame_1: ptr<function, f32>) -> f32 {
    var s_1: f32;

    let _e93 = (*pixelsPerFrame_1);
    s_1 = (1f + (0.5f * _e93));
    let _e96 = s_1;
    let _e97 = s_1;
    return inverseSqrt((_e96 * _e97));
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
    var speed: f32;
    var moving: f32;
    var movingCount: f32;
    var param_23: f32;
    var param_24: f32;

    let _e100 = (*storedCount);
    count_2 = clamp(_e100, 1f, 48f);
    let _e102 = (*motionPixels);
    speed = length(_e102);
    let _e104 = speed;
    moving = clamp((_e104 * 8f), 0f, 1f);
    let _e107 = (*movingWeight_1);
    param_23 = _e107;
    let _e108 = movingHistoryCount_u0028_f1_u003b((&param_23));
    let _e109 = speed;
    param_24 = _e109;
    let _e110 = movingHistoryScale_u0028_f1_u003b((&param_24));
    movingCount = max(1f, (_e108 * _e110));
    let _e113 = count_2;
    let _e114 = count_2;
    let _e115 = movingCount;
    let _e117 = count_2;
    let _e119 = moving;
    return (_e113 + ((min(_e114, _e115) - _e117) * _e119));
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
    var phi_183_: bool;

    let _e92 = (*v_1);
    let _e93 = (*v_1);
    let _e95 = all((_e92 == _e93));
    phi_183_ = _e95;
    if _e95 {
        let _e96 = (*v_1);
        phi_183_ = all((abs(_e96) <= vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_183_;
    return _e101;
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_1: ptr<function, f32>, y_1: ptr<function, f32>, a_1: ptr<function, f32>) -> f32 {
    let _e94 = (*x_1);
    let _e95 = (*a_1);
    let _e98 = (*y_1);
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
    var param_25: f32;
    var param_26: f32;
    var param_27: f32;
    var bottom: f32;
    var param_28: f32;
    var param_29: f32;
    var param_30: f32;
    var param_31: f32;
    var param_32: f32;
    var param_33: f32;

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
    param_25 = _e145;
    let _e146 = d10_;
    param_26 = _e146;
    let _e148 = f_1[0u];
    param_27 = _e148;
    let _e149 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_25), (&param_26), (&param_27));
    top = _e149;
    let _e150 = d01_;
    param_28 = _e150;
    let _e151 = d11_;
    param_29 = _e151;
    let _e153 = f_1[0u];
    param_30 = _e153;
    let _e154 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_28), (&param_29), (&param_30));
    bottom = _e154;
    let _e155 = top;
    param_31 = _e155;
    let _e156 = bottom;
    param_32 = _e156;
    let _e158 = f_1[1u];
    param_33 = _e158;
    let _e159 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_31), (&param_32), (&param_33));
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
    var phi_168_: bool;

    let _e92 = (*v_2);
    let _e93 = (*v_2);
    let _e95 = all((_e92 == _e93));
    phi_168_ = _e95;
    if _e95 {
        let _e96 = (*v_2);
        phi_168_ = all((abs(_e96) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_168_;
    return _e101;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_198_: bool;

    let _e92 = (*v_3);
    let _e93 = (*v_3);
    let _e95 = all((_e92 == _e93));
    phi_198_ = _e95;
    if _e95 {
        let _e96 = (*v_3);
        phi_198_ = all((abs(_e96) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_198_;
    return _e101;
}

fn dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b(px_1: ptr<function, vec2<i32>>, tileIndex: ptr<function, i32>, centerValid: ptr<function, f32>) -> vec2<f32> {
    var centerVelocity: vec2<f32>;
    var param_34: vec2<f32>;
    var hi_1: vec2<i32>;
    var best: vec2<i32>;
    var bestDepth: f32;
    var y_2: i32;
    var x_2: i32;
    var d: f32;
    var v_4: f32;
    var dv: vec2<f32>;
    var param_35: vec2<f32>;
    var phi_1294_: bool;
    var phi_1302_: bool;

    let _e105 = (*px_1);
    let _e106 = textureLoad(motionVelocity, _e105, 0i);
    centerVelocity = _e106.xy;
    let _e109 = pc.qualityFlags;
    let _e111 = ((_e109 & 2u) == 0u);
    phi_1294_ = _e111;
    if !(_e111) {
        let _e113 = (*centerValid);
        phi_1294_ = !((_e113 > 0f));
    }
    let _e117 = phi_1294_;
    phi_1302_ = _e117;
    if !(_e117) {
        let _e119 = centerVelocity;
        param_34 = _e119;
        let _e120 = finite2_u0028_vf2_u003b((&param_34));
        phi_1302_ = !(_e120);
    }
    let _e123 = phi_1302_;
    if _e123 {
        let _e124 = centerVelocity;
        return _e124;
    }
    let _e126 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e126) - vec2<i32>(1i, 1i));
    let _e129 = (*px_1);
    best = _e129;
    let _e130 = (*tileIndex);
    let _e132 = tileDepth[_e130];
    bestDepth = _e132;
    y_2 = -1i;
    loop {
        let _e133 = y_2;
        if (_e133 <= 1i) {
            x_2 = -1i;
            loop {
                let _e135 = x_2;
                if (_e135 <= 1i) {
                    let _e137 = (*tileIndex);
                    let _e138 = y_2;
                    let _e141 = x_2;
                    let _e144 = tileDepth[((_e137 + (_e138 * 10i)) + _e141)];
                    d = _e144;
                    let _e145 = d;
                    let _e146 = bestDepth;
                    if (_e145 > _e146) {
                        let _e148 = d;
                        bestDepth = _e148;
                        let _e149 = (*px_1);
                        let _e150 = x_2;
                        let _e151 = y_2;
                        let _e154 = hi_1;
                        best = clamp((_e149 + vec2<i32>(_e150, _e151)), vec2<i32>(0i, 0i), _e154);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e156 = x_2;
                    x_2 = (_e156 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e158 = y_2;
            y_2 = (_e158 + 1i);
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
    param_35 = _e175;
    let _e176 = finite2_u0028_vf2_u003b((&param_35));
    let _e177 = dv;
    let _e178 = centerVelocity;
    return select(_e178, _e177, vec2(_e176));
}

fn accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b(index: ptr<function, i32>, weight_1: ptr<function, f32>, sum_2: ptr<function, vec3<f32>>, lo: ptr<function, vec3<f32>>, hi_2: ptr<function, vec3<f32>>, moments: ptr<function, vec3<f32>>, squares: ptr<function, vec3<f32>>) {
    var c_1: vec3<f32>;

    let _e99 = (*index);
    let _e101 = tileColor[_e99];
    c_1 = _e101.xyz;
    let _e103 = c_1;
    let _e104 = (*weight_1);
    let _e106 = (*sum_2);
    (*sum_2) = (_e106 + (_e103 * _e104));
    let _e108 = (*lo);
    let _e109 = c_1;
    (*lo) = min(_e108, _e109);
    let _e111 = (*hi_2);
    let _e112 = c_1;
    (*hi_2) = max(_e111, _e112);
    let _e114 = c_1;
    let _e115 = (*moments);
    (*moments) = (_e115 + _e114);
    let _e117 = c_1;
    let _e118 = c_1;
    let _e120 = (*squares);
    (*squares) = (_e120 + (_e117 * _e118));
    return;
}

fn reconstructCurrent_u0028_i1_u003b_vf3_u003b_vf3_u003b(tileIndex_1: ptr<function, i32>, boxMin: ptr<function, vec3<f32>>, boxMax: ptr<function, vec3<f32>>) -> vec4<f32> {
    var t: vec2<f32>;
    var wx: vec3<f32>;
    var wy: vec3<f32>;
    var sum_3: vec3<f32>;
    var neighborhoodMin: vec3<f32>;
    var neighborhoodMax: vec3<f32>;
    var momentSum: vec3<f32>;
    var momentSquares: vec3<f32>;
    var param_36: i32;
    var param_37: f32;
    var param_38: vec3<f32>;
    var param_39: vec3<f32>;
    var param_40: vec3<f32>;
    var param_41: vec3<f32>;
    var param_42: vec3<f32>;
    var param_43: i32;
    var param_44: f32;
    var param_45: vec3<f32>;
    var param_46: vec3<f32>;
    var param_47: vec3<f32>;
    var param_48: vec3<f32>;
    var param_49: vec3<f32>;
    var param_50: i32;
    var param_51: f32;
    var param_52: vec3<f32>;
    var param_53: vec3<f32>;
    var param_54: vec3<f32>;
    var param_55: vec3<f32>;
    var param_56: vec3<f32>;
    var param_57: i32;
    var param_58: f32;
    var param_59: vec3<f32>;
    var param_60: vec3<f32>;
    var param_61: vec3<f32>;
    var param_62: vec3<f32>;
    var param_63: vec3<f32>;
    var param_64: i32;
    var param_65: f32;
    var param_66: vec3<f32>;
    var param_67: vec3<f32>;
    var param_68: vec3<f32>;
    var param_69: vec3<f32>;
    var param_70: vec3<f32>;
    var param_71: i32;
    var param_72: f32;
    var param_73: vec3<f32>;
    var param_74: vec3<f32>;
    var param_75: vec3<f32>;
    var param_76: vec3<f32>;
    var param_77: vec3<f32>;
    var param_78: i32;
    var param_79: f32;
    var param_80: vec3<f32>;
    var param_81: vec3<f32>;
    var param_82: vec3<f32>;
    var param_83: vec3<f32>;
    var param_84: vec3<f32>;
    var param_85: i32;
    var param_86: f32;
    var param_87: vec3<f32>;
    var param_88: vec3<f32>;
    var param_89: vec3<f32>;
    var param_90: vec3<f32>;
    var param_91: vec3<f32>;
    var param_92: i32;
    var param_93: f32;
    var param_94: vec3<f32>;
    var param_95: vec3<f32>;
    var param_96: vec3<f32>;
    var param_97: vec3<f32>;
    var param_98: vec3<f32>;
    var momentMean: vec3<f32>;
    var momentSigma: vec3<f32>;

    let _e168 = pc.currentEffectiveJitterUv;
    let _e170 = pc.extent;
    t = (_e168 * vec2<f32>(_e170));
    let _e174 = t[0u];
    let _e177 = t[0u];
    let _e181 = t[0u];
    let _e183 = t[0u];
    let _e187 = t[0u];
    let _e190 = t[0u];
    wx = vec3<f32>(((0.5f * _e174) * (_e177 - 1f)), (1f - (_e181 * _e183)), ((0.5f * _e187) * (_e190 + 1f)));
    let _e195 = t[1u];
    let _e198 = t[1u];
    let _e202 = t[1u];
    let _e204 = t[1u];
    let _e208 = t[1u];
    let _e211 = t[1u];
    wy = vec3<f32>(((0.5f * _e195) * (_e198 - 1f)), (1f - (_e202 * _e204)), ((0.5f * _e208) * (_e211 + 1f)));
    sum_3 = vec3<f32>(0f, 0f, 0f);
    neighborhoodMin = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
    neighborhoodMax = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
    momentSum = vec3<f32>(0f, 0f, 0f);
    momentSquares = vec3<f32>(0f, 0f, 0f);
    let _e215 = (*tileIndex_1);
    let _e219 = wx[0u];
    let _e221 = wy[0u];
    param_36 = ((_e215 - 10i) - 1i);
    param_37 = (_e219 * _e221);
    let _e223 = sum_3;
    param_38 = _e223;
    let _e224 = neighborhoodMin;
    param_39 = _e224;
    let _e225 = neighborhoodMax;
    param_40 = _e225;
    let _e226 = momentSum;
    param_41 = _e226;
    let _e227 = momentSquares;
    param_42 = _e227;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_36), (&param_37), (&param_38), (&param_39), (&param_40), (&param_41), (&param_42));
    let _e228 = param_38;
    sum_3 = _e228;
    let _e229 = param_39;
    neighborhoodMin = _e229;
    let _e230 = param_40;
    neighborhoodMax = _e230;
    let _e231 = param_41;
    momentSum = _e231;
    let _e232 = param_42;
    momentSquares = _e232;
    let _e233 = (*tileIndex_1);
    let _e236 = wx[1u];
    let _e238 = wy[0u];
    param_43 = (_e233 - 10i);
    param_44 = (_e236 * _e238);
    let _e240 = sum_3;
    param_45 = _e240;
    let _e241 = neighborhoodMin;
    param_46 = _e241;
    let _e242 = neighborhoodMax;
    param_47 = _e242;
    let _e243 = momentSum;
    param_48 = _e243;
    let _e244 = momentSquares;
    param_49 = _e244;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_43), (&param_44), (&param_45), (&param_46), (&param_47), (&param_48), (&param_49));
    let _e245 = param_45;
    sum_3 = _e245;
    let _e246 = param_46;
    neighborhoodMin = _e246;
    let _e247 = param_47;
    neighborhoodMax = _e247;
    let _e248 = param_48;
    momentSum = _e248;
    let _e249 = param_49;
    momentSquares = _e249;
    let _e250 = (*tileIndex_1);
    let _e254 = wx[2u];
    let _e256 = wy[0u];
    param_50 = ((_e250 - 10i) + 1i);
    param_51 = (_e254 * _e256);
    let _e258 = sum_3;
    param_52 = _e258;
    let _e259 = neighborhoodMin;
    param_53 = _e259;
    let _e260 = neighborhoodMax;
    param_54 = _e260;
    let _e261 = momentSum;
    param_55 = _e261;
    let _e262 = momentSquares;
    param_56 = _e262;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_50), (&param_51), (&param_52), (&param_53), (&param_54), (&param_55), (&param_56));
    let _e263 = param_52;
    sum_3 = _e263;
    let _e264 = param_53;
    neighborhoodMin = _e264;
    let _e265 = param_54;
    neighborhoodMax = _e265;
    let _e266 = param_55;
    momentSum = _e266;
    let _e267 = param_56;
    momentSquares = _e267;
    let _e268 = (*tileIndex_1);
    let _e271 = wx[0u];
    let _e273 = wy[1u];
    param_57 = (_e268 - 1i);
    param_58 = (_e271 * _e273);
    let _e275 = sum_3;
    param_59 = _e275;
    let _e276 = neighborhoodMin;
    param_60 = _e276;
    let _e277 = neighborhoodMax;
    param_61 = _e277;
    let _e278 = momentSum;
    param_62 = _e278;
    let _e279 = momentSquares;
    param_63 = _e279;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_57), (&param_58), (&param_59), (&param_60), (&param_61), (&param_62), (&param_63));
    let _e280 = param_59;
    sum_3 = _e280;
    let _e281 = param_60;
    neighborhoodMin = _e281;
    let _e282 = param_61;
    neighborhoodMax = _e282;
    let _e283 = param_62;
    momentSum = _e283;
    let _e284 = param_63;
    momentSquares = _e284;
    let _e286 = wx[1u];
    let _e288 = wy[1u];
    let _e290 = (*tileIndex_1);
    param_64 = _e290;
    param_65 = (_e286 * _e288);
    let _e291 = sum_3;
    param_66 = _e291;
    let _e292 = neighborhoodMin;
    param_67 = _e292;
    let _e293 = neighborhoodMax;
    param_68 = _e293;
    let _e294 = momentSum;
    param_69 = _e294;
    let _e295 = momentSquares;
    param_70 = _e295;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_64), (&param_65), (&param_66), (&param_67), (&param_68), (&param_69), (&param_70));
    let _e296 = param_66;
    sum_3 = _e296;
    let _e297 = param_67;
    neighborhoodMin = _e297;
    let _e298 = param_68;
    neighborhoodMax = _e298;
    let _e299 = param_69;
    momentSum = _e299;
    let _e300 = param_70;
    momentSquares = _e300;
    let _e301 = (*tileIndex_1);
    let _e304 = wx[2u];
    let _e306 = wy[1u];
    param_71 = (_e301 + 1i);
    param_72 = (_e304 * _e306);
    let _e308 = sum_3;
    param_73 = _e308;
    let _e309 = neighborhoodMin;
    param_74 = _e309;
    let _e310 = neighborhoodMax;
    param_75 = _e310;
    let _e311 = momentSum;
    param_76 = _e311;
    let _e312 = momentSquares;
    param_77 = _e312;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_71), (&param_72), (&param_73), (&param_74), (&param_75), (&param_76), (&param_77));
    let _e313 = param_73;
    sum_3 = _e313;
    let _e314 = param_74;
    neighborhoodMin = _e314;
    let _e315 = param_75;
    neighborhoodMax = _e315;
    let _e316 = param_76;
    momentSum = _e316;
    let _e317 = param_77;
    momentSquares = _e317;
    let _e318 = (*tileIndex_1);
    let _e322 = wx[0u];
    let _e324 = wy[2u];
    param_78 = ((_e318 + 10i) - 1i);
    param_79 = (_e322 * _e324);
    let _e326 = sum_3;
    param_80 = _e326;
    let _e327 = neighborhoodMin;
    param_81 = _e327;
    let _e328 = neighborhoodMax;
    param_82 = _e328;
    let _e329 = momentSum;
    param_83 = _e329;
    let _e330 = momentSquares;
    param_84 = _e330;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_78), (&param_79), (&param_80), (&param_81), (&param_82), (&param_83), (&param_84));
    let _e331 = param_80;
    sum_3 = _e331;
    let _e332 = param_81;
    neighborhoodMin = _e332;
    let _e333 = param_82;
    neighborhoodMax = _e333;
    let _e334 = param_83;
    momentSum = _e334;
    let _e335 = param_84;
    momentSquares = _e335;
    let _e336 = (*tileIndex_1);
    let _e339 = wx[1u];
    let _e341 = wy[2u];
    param_85 = (_e336 + 10i);
    param_86 = (_e339 * _e341);
    let _e343 = sum_3;
    param_87 = _e343;
    let _e344 = neighborhoodMin;
    param_88 = _e344;
    let _e345 = neighborhoodMax;
    param_89 = _e345;
    let _e346 = momentSum;
    param_90 = _e346;
    let _e347 = momentSquares;
    param_91 = _e347;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_85), (&param_86), (&param_87), (&param_88), (&param_89), (&param_90), (&param_91));
    let _e348 = param_87;
    sum_3 = _e348;
    let _e349 = param_88;
    neighborhoodMin = _e349;
    let _e350 = param_89;
    neighborhoodMax = _e350;
    let _e351 = param_90;
    momentSum = _e351;
    let _e352 = param_91;
    momentSquares = _e352;
    let _e353 = (*tileIndex_1);
    let _e357 = wx[2u];
    let _e359 = wy[2u];
    param_92 = ((_e353 + 10i) + 1i);
    param_93 = (_e357 * _e359);
    let _e361 = sum_3;
    param_94 = _e361;
    let _e362 = neighborhoodMin;
    param_95 = _e362;
    let _e363 = neighborhoodMax;
    param_96 = _e363;
    let _e364 = momentSum;
    param_97 = _e364;
    let _e365 = momentSquares;
    param_98 = _e365;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_92), (&param_93), (&param_94), (&param_95), (&param_96), (&param_97), (&param_98));
    let _e366 = param_94;
    sum_3 = _e366;
    let _e367 = param_95;
    neighborhoodMin = _e367;
    let _e368 = param_96;
    neighborhoodMax = _e368;
    let _e369 = param_97;
    momentSum = _e369;
    let _e370 = param_98;
    momentSquares = _e370;
    let _e371 = neighborhoodMin;
    (*boxMin) = _e371;
    let _e372 = neighborhoodMax;
    (*boxMax) = _e372;
    let _e374 = pc.qualityFlags;
    if ((_e374 & 1u) != 0u) {
        let _e377 = momentSum;
        momentMean = (_e377 * 0.11111111f);
        let _e379 = momentSquares;
        let _e381 = momentMean;
        let _e382 = momentMean;
        momentSigma = sqrt(max(((_e379 * 0.11111111f) - (_e381 * _e382)), vec3<f32>(0f, 0f, 0f)));
        let _e387 = neighborhoodMin;
        let _e388 = momentMean;
        let _e389 = momentSigma;
        (*boxMin) = max(_e387, (_e388 - (_e389 * 1.25f)));
        let _e393 = neighborhoodMax;
        let _e394 = momentMean;
        let _e395 = momentSigma;
        (*boxMax) = min(_e393, (_e394 + (_e395 * 1.25f)));
    }
    let _e399 = sum_3;
    let _e400 = neighborhoodMin;
    let _e401 = neighborhoodMax;
    let _e402 = clamp(_e399, _e400, _e401);
    let _e403 = (*tileIndex_1);
    let _e406 = tileColor[_e403][3u];
    return vec4<f32>(_e402.x, _e402.y, _e402.z, _e406);
}

fn main_1() {
    var pixel_1: vec2<i32>;
    var tileOrigin: vec2<i32>;
    var imageHi: vec2<i32>;
    var i_3: u32;
    var q: vec2<i32>;
    var tileIndex_2: i32;
    var clipMin: vec3<f32>;
    var clipMax: vec3<f32>;
    var current: vec4<f32>;
    var param_99: i32;
    var param_100: vec3<f32>;
    var param_101: vec3<f32>;
    var result_2: vec4<f32>;
    var displayed: vec4<f32>;
    var usedHistory: bool;
    var historyCount: f32;
    var valid: f32;
    var velocity: vec2<f32>;
    var param_102: vec2<i32>;
    var param_103: i32;
    var param_104: f32;
    var motionTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_105: vec4<f32>;
    var param_106: f32;
    var param_107: vec2<f32>;
    var param_108: vec2<f32>;
    var currentDevice: f32;
    var deviceLo: f32;
    var deviceHi: f32;
    var y_3: i32;
    var x_3: i32;
    var d_1: f32;
    var currentLinear: f32;
    var param_109: f32;
    var rangeA: f32;
    var param_110: f32;
    var rangeB: f32;
    var param_111: f32;
    var priorLinear: f32;
    var param_112: vec2<f32>;
    var depthLimit: f32;
    var param_113: f32;
    var param_114: f32;
    var param_115: f32;
    var param_116: f32;
    var boxValid: bool;
    var param_117: vec3<f32>;
    var param_118: vec3<f32>;
    var tapMin_2: vec3<f32>;
    var tapMax_2: vec3<f32>;
    var centreCount_1: f32;
    var prior: vec4<f32>;
    var param_119: vec2<f32>;
    var param_120: vec3<f32>;
    var param_121: vec3<f32>;
    var param_122: f32;
    var param_123: vec4<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var priorCount: f32;
    var param_124: f32;
    var param_125: f32;
    var param_126: vec2<f32>;
    var param_127: f32;
    var param_128: vec3<f32>;
    var param_129: vec3<f32>;
    var baseWeight: f32;
    var param_130: f32;
    var weight_2: f32;
    var param_131: f32;
    var param_132: f32;
    var resolvedRgb: vec3<f32>;
    var param_133: vec3<f32>;
    var param_134: vec3<f32>;
    var param_135: f32;
    var detail: vec3<f32>;
    var param_136: vec2<f32>;
    var param_137: vec3<f32>;
    var param_138: vec3<f32>;
    var param_139: f32;
    var param_140: vec4<f32>;
    var param_141: vec2<i32>;
    var param_142: vec4<f32>;
    var param_143: vec4<f32>;
    var param_144: vec4<f32>;
    var param_145: f32;
    var phi_1918_: bool;
    var phi_1927_: bool;
    var phi_1936_: bool;
    var phi_1942_: bool;
    var phi_2025_: bool;
    var phi_2031_: bool;
    var phi_2037_: bool;
    var phi_2053_: bool;
    var phi_2063_: bool;
    var phi_2075_: bool;
    var phi_2098_: bool;
    var phi_2218_: bool;
    var phi_2226_: bool;
    var phi_2249_: bool;

    let _e180 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e180.xy);
    let _e183 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e183.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e190 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e190) - vec2<i32>(1i, 1i));
    let _e193 = gl_LocalInvocationIndex_1;
    i_3 = _e193;
    loop {
        let _e194 = i_3;
        if (_e194 < 100u) {
            let _e196 = tileOrigin;
            let _e197 = i_3;
            let _e200 = i_3;
            let _e205 = imageHi;
            q = clamp((_e196 + vec2<i32>(bitcast<i32>((_e197 % 10u)), bitcast<i32>((_e200 / 10u)))), vec2<i32>(0i, 0i), _e205);
            let _e207 = i_3;
            let _e208 = q;
            let _e209 = textureLoad(currentColor, _e208, 0i);
            tileColor[_e207] = _e209;
            let _e211 = i_3;
            let _e212 = q;
            let _e213 = textureLoad(currentDeviceDepth, _e212, 0i);
            tileDepth[_e211] = _e213.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e216 = i_3;
            i_3 = (_e216 + 64u);
        }
    }
    workgroupBarrier();
    let _e218 = pixel_1;
    let _e220 = pc.extent;
    if any((_e218 >= bitcast<vec2<i32>>(_e220))) {
        return;
    }
    let _e225 = gl_LocalInvocationID_1[1u];
    let _e230 = gl_LocalInvocationID_1[0u];
    tileIndex_2 = ((((bitcast<i32>(_e225) + 1i) * 10i) + bitcast<i32>(_e230)) + 1i);
    clipMin = vec3<f32>(0f, 0f, 0f);
    clipMax = vec3<f32>(0f, 0f, 0f);
    let _e234 = tileIndex_2;
    param_99 = _e234;
    let _e235 = reconstructCurrent_u0028_i1_u003b_vf3_u003b_vf3_u003b((&param_99), (&param_100), (&param_101));
    let _e236 = param_100;
    clipMin = _e236;
    let _e237 = param_101;
    clipMax = _e237;
    current = _e235;
    let _e238 = current;
    result_2 = _e238;
    let _e239 = current;
    displayed = _e239;
    usedHistory = false;
    historyCount = 1f;
    let _e240 = pixel_1;
    let _e241 = textureLoad(motionValidity, _e240, 0i);
    valid = _e241.x;
    let _e243 = pixel_1;
    param_102 = _e243;
    let _e244 = tileIndex_2;
    param_103 = _e244;
    let _e245 = valid;
    param_104 = _e245;
    let _e246 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_102), (&param_103), (&param_104));
    velocity = _e246;
    let _e247 = velocity;
    let _e249 = pc.extent;
    motionTexel = (_e247 * vec2<f32>(_e249));
    let _e252 = pixel_1;
    let _e254 = motionTexel;
    previousTexel = (vec2<f32>(_e252) - _e254);
    let _e256 = previousTexel;
    previousCenter = (_e256 + vec2(0.5f));
    let _e259 = previousCenter;
    let _e261 = all((_e259 >= vec2<f32>(0f, 0f)));
    phi_1918_ = _e261;
    if _e261 {
        let _e262 = previousCenter;
        let _e264 = pc.extent;
        phi_1918_ = all((_e262 < vec2<f32>(_e264)));
    }
    let _e269 = phi_1918_;
    inside = _e269;
    let _e270 = current;
    param_105 = _e270;
    let _e271 = finite4_u0028_vf4_u003b((&param_105));
    phi_1927_ = _e271;
    if _e271 {
        let _e272 = valid;
        param_106 = _e272;
        let _e273 = finite1_u0028_f1_u003b((&param_106));
        phi_1927_ = _e273;
    }
    let _e275 = phi_1927_;
    let _e276 = valid;
    let _e278 = (_e275 && (_e276 > 0f));
    phi_1936_ = _e278;
    if _e278 {
        let _e279 = velocity;
        param_107 = _e279;
        let _e280 = finite2_u0028_vf2_u003b((&param_107));
        phi_1936_ = _e280;
    }
    let _e282 = phi_1936_;
    phi_1942_ = _e282;
    if _e282 {
        let _e283 = previousTexel;
        param_108 = _e283;
        let _e284 = finite2_u0028_vf2_u003b((&param_108));
        phi_1942_ = _e284;
    }
    let _e286 = phi_1942_;
    let _e287 = inside;
    if (_e286 && _e287) {
        let _e289 = tileIndex_2;
        let _e291 = tileDepth[_e289];
        currentDevice = _e291;
        let _e292 = currentDevice;
        deviceLo = _e292;
        let _e293 = currentDevice;
        deviceHi = _e293;
        y_3 = -1i;
        loop {
            let _e294 = y_3;
            if (_e294 <= 1i) {
                x_3 = -1i;
                loop {
                    let _e296 = x_3;
                    if (_e296 <= 1i) {
                        let _e298 = tileIndex_2;
                        let _e299 = y_3;
                        let _e302 = x_3;
                        let _e305 = tileDepth[((_e298 + (_e299 * 10i)) + _e302)];
                        d_1 = _e305;
                        let _e306 = deviceLo;
                        let _e307 = d_1;
                        deviceLo = min(_e306, _e307);
                        let _e309 = deviceHi;
                        let _e310 = d_1;
                        deviceHi = max(_e309, _e310);
                        continue;
                    } else {
                        break;
                    }
                    continuing {
                        let _e312 = x_3;
                        x_3 = (_e312 + 1i);
                    }
                }
                continue;
            } else {
                break;
            }
            continuing {
                let _e314 = y_3;
                y_3 = (_e314 + 1i);
            }
        }
        let _e316 = currentDevice;
        param_109 = _e316;
        let _e317 = linearizeDepth_u0028_f1_u003b((&param_109));
        currentLinear = _e317;
        let _e318 = deviceLo;
        param_110 = _e318;
        let _e319 = linearizeDepth_u0028_f1_u003b((&param_110));
        rangeA = _e319;
        let _e320 = deviceHi;
        param_111 = _e320;
        let _e321 = linearizeDepth_u0028_f1_u003b((&param_111));
        rangeB = _e321;
        let _e322 = previousTexel;
        param_112 = _e322;
        let _e323 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_112));
        priorLinear = _e323;
        let _e325 = pc.depthThresholdAbsolute;
        let _e327 = pc.depthThresholdRelative;
        let _e328 = currentLinear;
        let _e329 = priorLinear;
        depthLimit = max(_e325, (_e327 * max(_e328, _e329)));
        let _e333 = currentLinear;
        param_113 = _e333;
        let _e334 = finite1_u0028_f1_u003b((&param_113));
        phi_2025_ = _e334;
        if _e334 {
            let _e335 = priorLinear;
            param_114 = _e335;
            let _e336 = finite1_u0028_f1_u003b((&param_114));
            phi_2025_ = _e336;
        }
        let _e338 = phi_2025_;
        phi_2031_ = _e338;
        if _e338 {
            let _e339 = rangeA;
            param_115 = _e339;
            let _e340 = finite1_u0028_f1_u003b((&param_115));
            phi_2031_ = _e340;
        }
        let _e342 = phi_2031_;
        phi_2037_ = _e342;
        if _e342 {
            let _e343 = rangeB;
            param_116 = _e343;
            let _e344 = finite1_u0028_f1_u003b((&param_116));
            phi_2037_ = _e344;
        }
        let _e346 = phi_2037_;
        let _e347 = currentLinear;
        let _e350 = priorLinear;
        let _e352 = ((_e346 && (_e347 > 0f)) && (_e350 > 0f));
        phi_2053_ = _e352;
        if _e352 {
            let _e353 = priorLinear;
            let _e354 = rangeA;
            let _e355 = rangeB;
            let _e357 = depthLimit;
            phi_2053_ = (_e353 >= (min(_e354, _e355) - _e357));
        }
        let _e361 = phi_2053_;
        phi_2063_ = _e361;
        if _e361 {
            let _e362 = priorLinear;
            let _e363 = rangeA;
            let _e364 = rangeB;
            let _e366 = depthLimit;
            phi_2063_ = (_e362 <= (max(_e363, _e364) + _e366));
        }
        let _e370 = phi_2063_;
        if _e370 {
            let _e371 = clipMin;
            param_117 = _e371;
            let _e372 = finite3_u0028_vf3_u003b((&param_117));
            phi_2075_ = _e372;
            if _e372 {
                let _e373 = clipMax;
                param_118 = _e373;
                let _e374 = finite3_u0028_vf3_u003b((&param_118));
                phi_2075_ = _e374;
            }
            let _e376 = phi_2075_;
            boxValid = _e376;
            tapMin_2 = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
            tapMax_2 = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
            centreCount_1 = 0x1.fffffep+127f;
            let _e377 = previousTexel;
            param_119 = _e377;
            let _e378 = tapMin_2;
            param_120 = _e378;
            let _e379 = tapMax_2;
            param_121 = _e379;
            let _e380 = centreCount_1;
            param_122 = _e380;
            let _e381 = samplePreviousColor_u0028_vf2_u003b_vf3_u003b_vf3_u003b_f1_u003b((&param_119), (&param_120), (&param_121), (&param_122));
            let _e382 = param_120;
            tapMin_2 = _e382;
            let _e383 = param_121;
            tapMax_2 = _e383;
            let _e384 = param_122;
            centreCount_1 = _e384;
            prior = _e381;
            let _e385 = boxValid;
            phi_2098_ = _e385;
            if _e385 {
                let _e386 = prior;
                param_123 = _e386;
                let _e387 = finite4_u0028_vf4_u003b((&param_123));
                phi_2098_ = _e387;
            }
            let _e389 = phi_2098_;
            if _e389 {
                let _e390 = prior;
                let _e392 = clipMin;
                let _e393 = clipMax;
                clampedPrior = clamp(_e390.xyz, _e392, _e393);
                let _e395 = valid;
                confidence_1 = clamp(_e395, 0f, 1f);
                let _e398 = pc.historyWeight;
                let _e400 = centreCount_1;
                param_124 = _e400;
                param_125 = clamp(_e398, 0f, 1f);
                let _e401 = motionTexel;
                param_126 = _e401;
                let _e402 = accumulatedHistoryCount_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_124), (&param_125), (&param_126));
                priorCount = _e402;
                let _e403 = prior;
                let _e405 = clampedPrior;
                let _e407 = clipMax;
                let _e408 = clipMin;
                let _e410 = priorCount;
                param_127 = _e410;
                param_128 = (_e403.xyz - _e405);
                param_129 = (_e407 - _e408);
                let _e411 = rectifiedHistoryCount_u0028_f1_u003b_vf3_u003b_vf3_u003b((&param_127), (&param_128), (&param_129));
                priorCount = _e411;
                let _e412 = priorCount;
                param_130 = _e412;
                let _e413 = historyWeightForCount_u0028_f1_u003b((&param_130));
                baseWeight = _e413;
                let _e414 = baseWeight;
                param_131 = _e414;
                let _e415 = confidence_1;
                param_132 = _e415;
                let _e416 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_131), (&param_132));
                weight_2 = _e416;
                let _e417 = current;
                param_133 = _e417.xyz;
                let _e419 = clampedPrior;
                param_134 = _e419;
                let _e420 = weight_2;
                param_135 = _e420;
                let _e421 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_133), (&param_134), (&param_135));
                resolvedRgb = _e421;
                let _e422 = resolvedRgb;
                let _e424 = current[3u];
                result_2 = vec4<f32>(_e422.x, _e422.y, _e422.z, _e424);
                let _e429 = result_2;
                displayed = _e429;
                let _e430 = priorCount;
                let _e431 = confidence_1;
                historyCount = min(((_e430 * _e431) + 1f), 48f);
                let _e436 = pc.qualityFlags;
                if ((_e436 & 8u) == 0u) {
                    let _e439 = prior;
                    let _e441 = previousTexel;
                    param_136 = _e441;
                    let _e442 = tapMin_2;
                    param_137 = _e442;
                    let _e443 = tapMax_2;
                    param_138 = _e443;
                    let _e444 = samplePreviousBlur_u0028_vf2_u003b_vf3_u003b_vf3_u003b((&param_136), (&param_137), (&param_138));
                    let _e445 = param_137;
                    tapMin_2 = _e445;
                    let _e446 = param_138;
                    tapMax_2 = _e446;
                    detail = (_e439.xyz - _e444);
                    let _e448 = resolvedRgb;
                    let _e449 = confidence_1;
                    let _e451 = motionTexel;
                    param_139 = length(_e451);
                    let _e453 = movingSpeedScale_u0028_f1_u003b((&param_139));
                    let _e455 = detail;
                    let _e458 = resolvedRgb;
                    let _e459 = tapMin_2;
                    let _e461 = resolvedRgb;
                    let _e462 = tapMax_2;
                    let _e464 = clamp((_e448 + (_e455 * ((1.1f * _e449) * _e453))), min(_e458, _e459), max(_e461, _e462));
                    let _e466 = current[3u];
                    displayed = vec4<f32>(_e464.x, _e464.y, _e464.z, _e466);
                }
                usedHistory = true;
            }
        }
    }
    let _e471 = usedHistory;
    let _e472 = !(_e471);
    phi_2218_ = _e472;
    if _e472 {
        let _e473 = current;
        param_140 = _e473;
        let _e474 = finite4_u0028_vf4_u003b((&param_140));
        phi_2218_ = _e474;
    }
    let _e476 = phi_2218_;
    phi_2226_ = _e476;
    if _e476 {
        let _e478 = pc.qualityFlags;
        phi_2226_ = ((_e478 & 4u) != 0u);
    }
    let _e482 = phi_2226_;
    if _e482 {
        let _e483 = pixel_1;
        param_141 = _e483;
        let _e484 = current;
        param_142 = _e484;
        let _e485 = spatialEdgeFilter_u0028_vi2_u003b_vf4_u003b((&param_141), (&param_142));
        result_2 = _e485;
    }
    let _e486 = result_2;
    param_143 = _e486;
    let _e487 = finite4_u0028_vf4_u003b((&param_143));
    if !(_e487) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e489 = usedHistory;
    let _e490 = !(_e489);
    phi_2249_ = _e490;
    if !(_e490) {
        let _e492 = displayed;
        param_144 = _e492;
        let _e493 = finite4_u0028_vf4_u003b((&param_144));
        phi_2249_ = !(_e493);
    }
    let _e496 = phi_2249_;
    if _e496 {
        let _e497 = result_2;
        displayed = _e497;
    }
    let _e498 = pixel_1;
    let _e499 = displayed;
    textureStore(resolvedColor, _e498, _e499);
    let _e500 = pixel_1;
    let _e501 = result_2;
    let _e502 = _e501.xyz;
    let _e503 = historyCount;
    textureStore(historyOutColor, _e500, vec4<f32>(_e502.x, _e502.y, _e502.z, _e503));
    let _e508 = pixel_1;
    let _e509 = tileIndex_2;
    let _e511 = tileDepth[_e509];
    param_145 = _e511;
    let _e512 = storeLinearDepth_u0028_f1_u003b((&param_145));
    textureStore(historyOutLinearDepth, _e508, vec4(_e512));
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
