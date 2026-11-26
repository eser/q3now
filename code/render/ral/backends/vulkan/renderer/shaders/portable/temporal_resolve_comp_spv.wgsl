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
    var phi_145_: bool;

    let _e92 = (*v);
    let _e93 = (*v);
    let _e94 = (_e92 == _e93);
    phi_145_ = _e94;
    if _e94 {
        let _e95 = (*v);
        phi_145_ = (abs(_e95) <= 0x1.fffffep+127f);
    }
    let _e99 = phi_145_;
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
    let _e110 = movingSpeedScale_u0028_f1_u003b((&param_24));
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
    var phi_175_: bool;

    let _e92 = (*v_1);
    let _e93 = (*v_1);
    let _e95 = all((_e92 == _e93));
    phi_175_ = _e95;
    if _e95 {
        let _e96 = (*v_1);
        phi_175_ = all((abs(_e96) <= vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_175_;
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
    var param_25: vec3<f32>;
    var param_26: vec3<f32>;
    var momentMean: vec3<f32>;
    var momentSigma: vec3<f32>;
    var phi_1484_: bool;

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
    param_25 = _e136;
    let _e137 = finite3_u0028_vf3_u003b((&param_25));
    phi_1484_ = _e137;
    if _e137 {
        let _e138 = neighborhoodMax;
        param_26 = _e138;
        let _e139 = finite3_u0028_vf3_u003b((&param_26));
        phi_1484_ = _e139;
    }
    let _e141 = phi_1484_;
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
    var param_27: f32;
    var param_28: f32;
    var param_29: f32;
    var bottom: f32;
    var param_30: f32;
    var param_31: f32;
    var param_32: f32;
    var param_33: f32;
    var param_34: f32;
    var param_35: f32;

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
    param_27 = _e145;
    let _e146 = d10_;
    param_28 = _e146;
    let _e148 = f_1[0u];
    param_29 = _e148;
    let _e149 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_27), (&param_28), (&param_29));
    top = _e149;
    let _e150 = d01_;
    param_30 = _e150;
    let _e151 = d11_;
    param_31 = _e151;
    let _e153 = f_1[0u];
    param_32 = _e153;
    let _e154 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_30), (&param_31), (&param_32));
    bottom = _e154;
    let _e155 = top;
    param_33 = _e155;
    let _e156 = bottom;
    param_34 = _e156;
    let _e158 = f_1[1u];
    param_35 = _e158;
    let _e159 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_33), (&param_34), (&param_35));
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
    var phi_160_: bool;

    let _e92 = (*v_2);
    let _e93 = (*v_2);
    let _e95 = all((_e92 == _e93));
    phi_160_ = _e95;
    if _e95 {
        let _e96 = (*v_2);
        phi_160_ = all((abs(_e96) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_160_;
    return _e101;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_190_: bool;

    let _e92 = (*v_3);
    let _e93 = (*v_3);
    let _e95 = all((_e92 == _e93));
    phi_190_ = _e95;
    if _e95 {
        let _e96 = (*v_3);
        phi_190_ = all((abs(_e96) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e101 = phi_190_;
    return _e101;
}

fn dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b(px_1: ptr<function, vec2<i32>>, tileIndex_1: ptr<function, i32>, centerValid: ptr<function, f32>) -> vec2<f32> {
    var centerVelocity: vec2<f32>;
    var param_36: vec2<f32>;
    var hi_1: vec2<i32>;
    var best: vec2<i32>;
    var bestDepth: f32;
    var y_3: i32;
    var x_3: i32;
    var d: f32;
    var v_4: f32;
    var dv: vec2<f32>;
    var param_37: vec2<f32>;
    var phi_1276_: bool;
    var phi_1284_: bool;

    let _e105 = (*px_1);
    let _e106 = textureLoad(motionVelocity, _e105, 0i);
    centerVelocity = _e106.xy;
    let _e109 = pc.qualityFlags;
    let _e111 = ((_e109 & 2u) == 0u);
    phi_1276_ = _e111;
    if !(_e111) {
        let _e113 = (*centerValid);
        phi_1276_ = !((_e113 > 0f));
    }
    let _e117 = phi_1276_;
    phi_1284_ = _e117;
    if !(_e117) {
        let _e119 = centerVelocity;
        param_36 = _e119;
        let _e120 = finite2_u0028_vf2_u003b((&param_36));
        phi_1284_ = !(_e120);
    }
    let _e123 = phi_1284_;
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
    param_37 = _e175;
    let _e176 = finite2_u0028_vf2_u003b((&param_37));
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
    var param_38: vec2<i32>;
    var param_39: i32;
    var result_2: vec4<f32>;
    var displayed: vec4<f32>;
    var usedHistory: bool;
    var historyCount: f32;
    var valid: f32;
    var velocity: vec2<f32>;
    var param_40: vec2<i32>;
    var param_41: i32;
    var param_42: f32;
    var motionTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_43: vec4<f32>;
    var param_44: f32;
    var param_45: vec2<f32>;
    var param_46: vec2<f32>;
    var currentDevice: f32;
    var deviceLo: f32;
    var deviceHi: f32;
    var y_4: i32;
    var x_4: i32;
    var d_1: f32;
    var currentLinear: f32;
    var param_47: f32;
    var rangeA: f32;
    var param_48: f32;
    var rangeB: f32;
    var param_49: f32;
    var priorLinear: f32;
    var param_50: vec2<f32>;
    var depthLimit: f32;
    var param_51: f32;
    var param_52: f32;
    var param_53: f32;
    var param_54: f32;
    var boxValid: bool;
    var param_55: i32;
    var param_56: vec3<f32>;
    var param_57: vec3<f32>;
    var tapMin_2: vec3<f32>;
    var tapMax_2: vec3<f32>;
    var centreCount_1: f32;
    var prior: vec4<f32>;
    var param_58: vec2<f32>;
    var param_59: vec3<f32>;
    var param_60: vec3<f32>;
    var param_61: f32;
    var param_62: vec4<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var priorCount: f32;
    var param_63: f32;
    var param_64: f32;
    var param_65: vec2<f32>;
    var param_66: f32;
    var param_67: vec3<f32>;
    var param_68: vec3<f32>;
    var baseWeight: f32;
    var param_69: f32;
    var weight_1: f32;
    var param_70: f32;
    var param_71: f32;
    var resolvedRgb: vec3<f32>;
    var param_72: vec3<f32>;
    var param_73: vec3<f32>;
    var param_74: f32;
    var detail: vec3<f32>;
    var param_75: vec2<f32>;
    var param_76: vec3<f32>;
    var param_77: vec3<f32>;
    var param_78: f32;
    var param_79: vec4<f32>;
    var param_80: vec2<i32>;
    var param_81: vec4<f32>;
    var param_82: vec4<f32>;
    var param_83: vec4<f32>;
    var param_84: f32;
    var phi_1673_: bool;
    var phi_1682_: bool;
    var phi_1691_: bool;
    var phi_1697_: bool;
    var phi_1780_: bool;
    var phi_1786_: bool;
    var phi_1792_: bool;
    var phi_1808_: bool;
    var phi_1818_: bool;
    var phi_1851_: bool;
    var phi_1970_: bool;
    var phi_1978_: bool;
    var phi_2001_: bool;

    let _e180 = gl_GlobalInvocationID_1;
    pixel_2 = bitcast<vec2<i32>>(_e180.xy);
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
    let _e218 = pixel_2;
    let _e220 = pc.extent;
    if any((_e218 >= bitcast<vec2<i32>>(_e220))) {
        return;
    }
    let _e225 = gl_LocalInvocationID_1[1u];
    let _e230 = gl_LocalInvocationID_1[0u];
    tileIndex_3 = ((((bitcast<i32>(_e225) + 1i) * 10i) + bitcast<i32>(_e230)) + 1i);
    clipMin = vec3<f32>(0f, 0f, 0f);
    clipMax = vec3<f32>(0f, 0f, 0f);
    let _e234 = pixel_2;
    param_38 = _e234;
    let _e235 = tileIndex_3;
    param_39 = _e235;
    let _e236 = reconstructCurrent_u0028_vi2_u003b_i1_u003b((&param_38), (&param_39));
    current = _e236;
    let _e237 = current;
    result_2 = _e237;
    let _e238 = current;
    displayed = _e238;
    usedHistory = false;
    historyCount = 1f;
    let _e239 = pixel_2;
    let _e240 = textureLoad(motionValidity, _e239, 0i);
    valid = _e240.x;
    let _e242 = pixel_2;
    param_40 = _e242;
    let _e243 = tileIndex_3;
    param_41 = _e243;
    let _e244 = valid;
    param_42 = _e244;
    let _e245 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_40), (&param_41), (&param_42));
    velocity = _e245;
    let _e246 = velocity;
    let _e248 = pc.extent;
    motionTexel = (_e246 * vec2<f32>(_e248));
    let _e251 = pixel_2;
    let _e253 = motionTexel;
    previousTexel = (vec2<f32>(_e251) - _e253);
    let _e255 = previousTexel;
    previousCenter = (_e255 + vec2(0.5f));
    let _e258 = previousCenter;
    let _e260 = all((_e258 >= vec2<f32>(0f, 0f)));
    phi_1673_ = _e260;
    if _e260 {
        let _e261 = previousCenter;
        let _e263 = pc.extent;
        phi_1673_ = all((_e261 < vec2<f32>(_e263)));
    }
    let _e268 = phi_1673_;
    inside = _e268;
    let _e269 = current;
    param_43 = _e269;
    let _e270 = finite4_u0028_vf4_u003b((&param_43));
    phi_1682_ = _e270;
    if _e270 {
        let _e271 = valid;
        param_44 = _e271;
        let _e272 = finite1_u0028_f1_u003b((&param_44));
        phi_1682_ = _e272;
    }
    let _e274 = phi_1682_;
    let _e275 = valid;
    let _e277 = (_e274 && (_e275 > 0f));
    phi_1691_ = _e277;
    if _e277 {
        let _e278 = velocity;
        param_45 = _e278;
        let _e279 = finite2_u0028_vf2_u003b((&param_45));
        phi_1691_ = _e279;
    }
    let _e281 = phi_1691_;
    phi_1697_ = _e281;
    if _e281 {
        let _e282 = previousTexel;
        param_46 = _e282;
        let _e283 = finite2_u0028_vf2_u003b((&param_46));
        phi_1697_ = _e283;
    }
    let _e285 = phi_1697_;
    let _e286 = inside;
    if (_e285 && _e286) {
        let _e288 = tileIndex_3;
        let _e290 = tileDepth[_e288];
        currentDevice = _e290;
        let _e291 = currentDevice;
        deviceLo = _e291;
        let _e292 = currentDevice;
        deviceHi = _e292;
        y_4 = -1i;
        loop {
            let _e293 = y_4;
            if (_e293 <= 1i) {
                x_4 = -1i;
                loop {
                    let _e295 = x_4;
                    if (_e295 <= 1i) {
                        let _e297 = tileIndex_3;
                        let _e298 = y_4;
                        let _e301 = x_4;
                        let _e304 = tileDepth[((_e297 + (_e298 * 10i)) + _e301)];
                        d_1 = _e304;
                        let _e305 = deviceLo;
                        let _e306 = d_1;
                        deviceLo = min(_e305, _e306);
                        let _e308 = deviceHi;
                        let _e309 = d_1;
                        deviceHi = max(_e308, _e309);
                        continue;
                    } else {
                        break;
                    }
                    continuing {
                        let _e311 = x_4;
                        x_4 = (_e311 + 1i);
                    }
                }
                continue;
            } else {
                break;
            }
            continuing {
                let _e313 = y_4;
                y_4 = (_e313 + 1i);
            }
        }
        let _e315 = currentDevice;
        param_47 = _e315;
        let _e316 = linearizeDepth_u0028_f1_u003b((&param_47));
        currentLinear = _e316;
        let _e317 = deviceLo;
        param_48 = _e317;
        let _e318 = linearizeDepth_u0028_f1_u003b((&param_48));
        rangeA = _e318;
        let _e319 = deviceHi;
        param_49 = _e319;
        let _e320 = linearizeDepth_u0028_f1_u003b((&param_49));
        rangeB = _e320;
        let _e321 = previousTexel;
        param_50 = _e321;
        let _e322 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_50));
        priorLinear = _e322;
        let _e324 = pc.depthThresholdAbsolute;
        let _e326 = pc.depthThresholdRelative;
        let _e327 = currentLinear;
        let _e328 = priorLinear;
        depthLimit = max(_e324, (_e326 * max(_e327, _e328)));
        let _e332 = currentLinear;
        param_51 = _e332;
        let _e333 = finite1_u0028_f1_u003b((&param_51));
        phi_1780_ = _e333;
        if _e333 {
            let _e334 = priorLinear;
            param_52 = _e334;
            let _e335 = finite1_u0028_f1_u003b((&param_52));
            phi_1780_ = _e335;
        }
        let _e337 = phi_1780_;
        phi_1786_ = _e337;
        if _e337 {
            let _e338 = rangeA;
            param_53 = _e338;
            let _e339 = finite1_u0028_f1_u003b((&param_53));
            phi_1786_ = _e339;
        }
        let _e341 = phi_1786_;
        phi_1792_ = _e341;
        if _e341 {
            let _e342 = rangeB;
            param_54 = _e342;
            let _e343 = finite1_u0028_f1_u003b((&param_54));
            phi_1792_ = _e343;
        }
        let _e345 = phi_1792_;
        let _e346 = currentLinear;
        let _e349 = priorLinear;
        let _e351 = ((_e345 && (_e346 > 0f)) && (_e349 > 0f));
        phi_1808_ = _e351;
        if _e351 {
            let _e352 = priorLinear;
            let _e353 = rangeA;
            let _e354 = rangeB;
            let _e356 = depthLimit;
            phi_1808_ = (_e352 >= (min(_e353, _e354) - _e356));
        }
        let _e360 = phi_1808_;
        phi_1818_ = _e360;
        if _e360 {
            let _e361 = priorLinear;
            let _e362 = rangeA;
            let _e363 = rangeB;
            let _e365 = depthLimit;
            phi_1818_ = (_e361 <= (max(_e362, _e363) + _e365));
        }
        let _e369 = phi_1818_;
        if _e369 {
            let _e370 = tileIndex_3;
            param_55 = _e370;
            let _e371 = neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b((&param_55), (&param_56), (&param_57));
            let _e372 = param_56;
            clipMin = _e372;
            let _e373 = param_57;
            clipMax = _e373;
            boxValid = _e371;
            tapMin_2 = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
            tapMax_2 = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
            centreCount_1 = 0x1.fffffep+127f;
            let _e374 = previousTexel;
            param_58 = _e374;
            let _e375 = tapMin_2;
            param_59 = _e375;
            let _e376 = tapMax_2;
            param_60 = _e376;
            let _e377 = centreCount_1;
            param_61 = _e377;
            let _e378 = samplePreviousColor_u0028_vf2_u003b_vf3_u003b_vf3_u003b_f1_u003b((&param_58), (&param_59), (&param_60), (&param_61));
            let _e379 = param_59;
            tapMin_2 = _e379;
            let _e380 = param_60;
            tapMax_2 = _e380;
            let _e381 = param_61;
            centreCount_1 = _e381;
            prior = _e378;
            let _e382 = boxValid;
            phi_1851_ = _e382;
            if _e382 {
                let _e383 = prior;
                param_62 = _e383;
                let _e384 = finite4_u0028_vf4_u003b((&param_62));
                phi_1851_ = _e384;
            }
            let _e386 = phi_1851_;
            if _e386 {
                let _e387 = prior;
                let _e389 = clipMin;
                let _e390 = clipMax;
                clampedPrior = clamp(_e387.xyz, _e389, _e390);
                let _e392 = valid;
                confidence_1 = clamp(_e392, 0f, 1f);
                let _e395 = pc.historyWeight;
                let _e397 = centreCount_1;
                param_63 = _e397;
                param_64 = clamp(_e395, 0f, 1f);
                let _e398 = motionTexel;
                param_65 = _e398;
                let _e399 = accumulatedHistoryCount_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_63), (&param_64), (&param_65));
                priorCount = _e399;
                let _e400 = prior;
                let _e402 = clampedPrior;
                let _e404 = clipMax;
                let _e405 = clipMin;
                let _e407 = priorCount;
                param_66 = _e407;
                param_67 = (_e400.xyz - _e402);
                param_68 = (_e404 - _e405);
                let _e408 = rectifiedHistoryCount_u0028_f1_u003b_vf3_u003b_vf3_u003b((&param_66), (&param_67), (&param_68));
                priorCount = _e408;
                let _e409 = priorCount;
                param_69 = _e409;
                let _e410 = historyWeightForCount_u0028_f1_u003b((&param_69));
                baseWeight = _e410;
                let _e411 = baseWeight;
                param_70 = _e411;
                let _e412 = confidence_1;
                param_71 = _e412;
                let _e413 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_70), (&param_71));
                weight_1 = _e413;
                let _e414 = current;
                param_72 = _e414.xyz;
                let _e416 = clampedPrior;
                param_73 = _e416;
                let _e417 = weight_1;
                param_74 = _e417;
                let _e418 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_72), (&param_73), (&param_74));
                resolvedRgb = _e418;
                let _e419 = resolvedRgb;
                let _e421 = current[3u];
                result_2 = vec4<f32>(_e419.x, _e419.y, _e419.z, _e421);
                let _e426 = result_2;
                displayed = _e426;
                let _e427 = priorCount;
                let _e428 = confidence_1;
                historyCount = min(((_e427 * _e428) + 1f), 48f);
                let _e433 = pc.qualityFlags;
                if ((_e433 & 8u) == 0u) {
                    let _e436 = prior;
                    let _e438 = previousTexel;
                    param_75 = _e438;
                    let _e439 = tapMin_2;
                    param_76 = _e439;
                    let _e440 = tapMax_2;
                    param_77 = _e440;
                    let _e441 = samplePreviousBlur_u0028_vf2_u003b_vf3_u003b_vf3_u003b((&param_75), (&param_76), (&param_77));
                    let _e442 = param_76;
                    tapMin_2 = _e442;
                    let _e443 = param_77;
                    tapMax_2 = _e443;
                    detail = (_e436.xyz - _e441);
                    let _e445 = resolvedRgb;
                    let _e446 = confidence_1;
                    let _e448 = motionTexel;
                    param_78 = length(_e448);
                    let _e450 = movingSpeedScale_u0028_f1_u003b((&param_78));
                    let _e452 = detail;
                    let _e455 = resolvedRgb;
                    let _e456 = tapMin_2;
                    let _e458 = resolvedRgb;
                    let _e459 = tapMax_2;
                    let _e461 = clamp((_e445 + (_e452 * ((2.6f * _e446) * _e450))), min(_e455, _e456), max(_e458, _e459));
                    let _e463 = current[3u];
                    displayed = vec4<f32>(_e461.x, _e461.y, _e461.z, _e463);
                }
                usedHistory = true;
            }
        }
    }
    let _e468 = usedHistory;
    let _e469 = !(_e468);
    phi_1970_ = _e469;
    if _e469 {
        let _e470 = current;
        param_79 = _e470;
        let _e471 = finite4_u0028_vf4_u003b((&param_79));
        phi_1970_ = _e471;
    }
    let _e473 = phi_1970_;
    phi_1978_ = _e473;
    if _e473 {
        let _e475 = pc.qualityFlags;
        phi_1978_ = ((_e475 & 4u) != 0u);
    }
    let _e479 = phi_1978_;
    if _e479 {
        let _e480 = pixel_2;
        param_80 = _e480;
        let _e481 = current;
        param_81 = _e481;
        let _e482 = spatialEdgeFilter_u0028_vi2_u003b_vf4_u003b((&param_80), (&param_81));
        result_2 = _e482;
    }
    let _e483 = result_2;
    param_82 = _e483;
    let _e484 = finite4_u0028_vf4_u003b((&param_82));
    if !(_e484) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e486 = usedHistory;
    let _e487 = !(_e486);
    phi_2001_ = _e487;
    if !(_e487) {
        let _e489 = displayed;
        param_83 = _e489;
        let _e490 = finite4_u0028_vf4_u003b((&param_83));
        phi_2001_ = !(_e490);
    }
    let _e493 = phi_2001_;
    if _e493 {
        let _e494 = result_2;
        displayed = _e494;
    }
    let _e495 = pixel_2;
    let _e496 = displayed;
    textureStore(resolvedColor, _e495, _e496);
    let _e497 = pixel_2;
    let _e498 = result_2;
    let _e499 = _e498.xyz;
    let _e500 = historyCount;
    textureStore(historyOutColor, _e497, vec4<f32>(_e499.x, _e499.y, _e499.z, _e500));
    let _e505 = pixel_2;
    let _e506 = tileIndex_3;
    let _e508 = tileDepth[_e506];
    param_84 = _e508;
    let _e509 = storeLinearDepth_u0028_f1_u003b((&param_84));
    textureStore(historyOutLinearDepth, _e505, vec4(_e509));
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
