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

    let _e84 = pc.zNear;
    let _e85 = (*z);
    let _e87 = pc.zFar;
    let _e89 = pc.zNear;
    denominator = (_e84 + (_e85 * (_e87 - _e89)));
    let _e94 = pc.zNear;
    let _e96 = pc.zFar;
    let _e98 = denominator;
    let _e99 = denominator;
    return ((_e94 * _e96) * inverseSqrt((_e98 * _e99)));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e83 = (*pixel);
    let _e85 = pc.extent;
    bounded = clamp(_e83, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e85) - vec2<i32>(1i, 1i)));
    let _e89 = bounded;
    let _e90 = textureLoad(currentColor, _e89, 0i);
    return _e90;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_117_: bool;

    let _e82 = (*v);
    let _e83 = (*v);
    let _e84 = (_e82 == _e83);
    phi_117_ = _e84;
    if _e84 {
        let _e85 = (*v);
        phi_117_ = (abs(_e85) <= 340282350000000000000000000000000000000f);
    }
    let _e89 = phi_117_;
    return _e89;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e84 = (*c);
    l = dot(_e84, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e86 = l;
    param = _e86;
    let _e87 = finite1_u0028_f1_u003b((&param));
    if !(_e87) {
        return 0f;
    }
    let _e89 = l;
    l = clamp(_e89, 0f, 10000f);
    let _e91 = l;
    let _e92 = l;
    let _e93 = l;
    return (_e91 * inverseSqrt((1f + (_e92 * _e93))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e84 = (*p);
    param_1 = _e84;
    let _e85 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e85.xyz;
    let _e87 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e87;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e85 = (*base);
    param_3 = _e85;
    let _e86 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e87 = (*base);
    let _e88 = (*across);
    param_4 = (_e87 + _e88);
    let _e90 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e86 + _e90));
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

    let _e154 = (*centerColor);
    param_5 = _e154.xyz;
    let _e156 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e156;
    let _e157 = (*px);
    param_6 = (_e157 + vec2<i32>(0i, -1i));
    let _e159 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e159;
    let _e160 = (*px);
    param_7 = (_e160 + vec2<i32>(0i, 1i));
    let _e162 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e162;
    let _e163 = (*px);
    param_8 = (_e163 + vec2<i32>(-1i, 0i));
    let _e165 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e165;
    let _e166 = (*px);
    param_9 = (_e166 + vec2<i32>(1i, 0i));
    let _e168 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e168;
    let _e169 = fxN;
    let _e170 = fxS;
    let _e172 = fxW;
    let _e173 = fxE;
    let _e175 = fxM;
    fxHi = max(max(_e169, _e170), max(max(_e172, _e173), _e175));
    let _e178 = fxN;
    let _e179 = fxS;
    let _e181 = fxW;
    let _e182 = fxE;
    let _e184 = fxM;
    fxLo = min(min(_e178, _e179), min(min(_e181, _e182), _e184));
    let _e187 = fxHi;
    let _e188 = fxLo;
    fxSpan = (_e187 - _e188);
    let _e190 = fxSpan;
    let _e191 = fxHi;
    if (_e190 < max(0.0312f, (_e191 * 0.125f))) {
        let _e195 = (*centerColor);
        return _e195;
    }
    let _e196 = (*px);
    param_10 = (_e196 + vec2<i32>(-1i, -1i));
    let _e198 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e198;
    let _e199 = (*px);
    param_11 = (_e199 + vec2<i32>(1i, -1i));
    let _e201 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e201;
    let _e202 = (*px);
    param_12 = (_e202 + vec2<i32>(-1i, 1i));
    let _e204 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e204;
    let _e205 = (*px);
    param_13 = (_e205 + vec2<i32>(1i, 1i));
    let _e207 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e207;
    let _e208 = fxN;
    let _e209 = fxS;
    fxNS = (_e208 + _e209);
    let _e211 = fxW;
    let _e212 = fxE;
    fxWE = (_e211 + _e212);
    let _e214 = fxNW;
    let _e215 = fxSW;
    fxNWSW = (_e214 + _e215);
    let _e217 = fxNE;
    let _e218 = fxSE;
    fxNESE = (_e217 + _e218);
    let _e220 = fxW;
    let _e222 = fxNWSW;
    let _e225 = fxM;
    let _e227 = fxNS;
    let _e231 = fxE;
    let _e233 = fxNESE;
    fxHorz = (abs(((-2f * _e220) + _e222)) + ((abs(((-2f * _e225) + _e227)) * 2f) + abs(((-2f * _e231) + _e233))));
    let _e238 = fxS;
    let _e240 = fxSW;
    let _e241 = fxSE;
    let _e245 = fxM;
    let _e247 = fxWE;
    let _e251 = fxN;
    let _e253 = fxNW;
    let _e254 = fxNE;
    fxVert = (abs(((-2f * _e238) + (_e240 + _e241))) + ((abs(((-2f * _e245) + _e247)) * 2f) + abs(((-2f * _e251) + (_e253 + _e254)))));
    let _e260 = fxHorz;
    let _e261 = fxVert;
    fxHorzSpan = (_e260 >= _e261);
    let _e263 = fxNS;
    let _e264 = fxWE;
    let _e267 = fxNWSW;
    let _e268 = fxNESE;
    fxAround = (((_e263 + _e264) * 2f) + (_e267 + _e268));
    let _e271 = fxHorzSpan;
    let _e272 = fxN;
    let _e273 = fxW;
    fxLoSide = select(_e273, _e272, _e271);
    let _e275 = fxHorzSpan;
    let _e276 = fxS;
    let _e277 = fxE;
    fxHiSide = select(_e277, _e276, _e275);
    let _e279 = fxLoSide;
    let _e280 = fxM;
    fxGradLo = (_e279 - _e280);
    let _e282 = fxHiSide;
    let _e283 = fxM;
    fxGradHi = (_e282 - _e283);
    let _e285 = fxGradLo;
    let _e287 = fxGradHi;
    fxLoSteeper = (abs(_e285) >= abs(_e287));
    let _e290 = fxGradLo;
    let _e292 = fxGradHi;
    fxGradient = max(abs(_e290), abs(_e292));
    let _e295 = fxLoSteeper;
    if _e295 {
        let _e296 = fxLoSide;
        let _e297 = fxM;
        local = (_e296 + _e297);
    } else {
        let _e299 = fxHiSide;
        let _e300 = fxM;
        local = (_e299 + _e300);
    }
    let _e302 = local;
    fxPair = _e302;
    let _e303 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e303));
    let _e306 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e306));
    let _e309 = fxLoSteeper;
    if _e309 {
        let _e310 = fxAcross;
        fxAcross = -(_e310);
    }
    let _e312 = fxSpan;
    let _e313 = fxSpan;
    fxInvSpan = inverseSqrt((_e312 * _e313));
    let _e316 = fxAround;
    let _e318 = fxM;
    let _e321 = fxInvSpan;
    fxBlendC = clamp((abs(((_e316 * 0.083333336f) - _e318)) * _e321), 0f, 1f);
    let _e324 = fxBlendC;
    let _e327 = fxBlendC;
    let _e328 = fxBlendC;
    fxBlendF = (((-2f * _e324) + 3f) * (_e327 * _e328));
    let _e331 = fxBlendF;
    let _e332 = fxBlendF;
    fxSubpixel = ((_e331 * _e332) * 0.75f);
    let _e335 = fxPair;
    fxHalfPair = (_e335 * 0.5f);
    let _e337 = fxGradient;
    fxScaled = (_e337 * 0.25f);
    let _e339 = fxM;
    let _e340 = fxHalfPair;
    fxMidNegative = ((_e339 - _e340) < 0f);
    let _e343 = (*px);
    let _e344 = fxAlong;
    param_14 = (_e343 - _e344);
    let _e346 = fxAcross;
    param_15 = _e346;
    let _e347 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e348 = fxHalfPair;
    fxEndN = (_e347 - _e348);
    let _e350 = (*px);
    let _e351 = fxAlong;
    param_16 = (_e350 + _e351);
    let _e353 = fxAcross;
    param_17 = _e353;
    let _e354 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e355 = fxHalfPair;
    fxEndP = (_e354 - _e355);
    let _e357 = fxEndN;
    let _e359 = fxScaled;
    fxDoneN = (abs(_e357) >= _e359);
    let _e361 = fxEndP;
    let _e363 = fxScaled;
    fxDoneP = (abs(_e361) >= _e363);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e365 = i;
        if (_e365 <= 12i) {
            let _e367 = fxDoneN;
            if !(_e367) {
                let _e369 = (*px);
                let _e370 = fxAlong;
                let _e371 = i;
                param_18 = (_e369 - (_e370 * vec2(_e371)));
                let _e375 = fxAcross;
                param_19 = _e375;
                let _e376 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e377 = fxHalfPair;
                fxEndN = (_e376 - _e377);
                let _e379 = fxEndN;
                let _e381 = fxScaled;
                fxDoneN = (abs(_e379) >= _e381);
                let _e383 = i;
                fxStepsN = _e383;
            }
            let _e384 = fxDoneP;
            if !(_e384) {
                let _e386 = (*px);
                let _e387 = fxAlong;
                let _e388 = i;
                param_20 = (_e386 + (_e387 * vec2(_e388)));
                let _e392 = fxAcross;
                param_21 = _e392;
                let _e393 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e394 = fxHalfPair;
                fxEndP = (_e393 - _e394);
                let _e396 = fxEndP;
                let _e398 = fxScaled;
                fxDoneP = (abs(_e396) >= _e398);
                let _e400 = i;
                fxStepsP = _e400;
            }
            let _e401 = fxDoneN;
            let _e402 = fxDoneP;
            if (_e401 && _e402) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e404 = i;
            i = (_e404 + 1i);
        }
    }
    let _e406 = fxStepsN;
    let _e407 = fxStepsP;
    fxTowardN = (_e406 < _e407);
    let _e409 = fxTowardN;
    let _e410 = fxEndN;
    let _e411 = fxEndP;
    fxEndG = select(_e411, _e410, _e409);
    let _e413 = fxEndG;
    let _e415 = fxMidNegative;
    fxGoodSpan = ((_e413 < 0f) != _e415);
    let _e417 = fxStepsN;
    let _e418 = fxStepsP;
    fxSpanLen = f32((_e417 + _e418));
    let _e421 = fxTowardN;
    let _e422 = fxStepsN;
    let _e423 = fxStepsP;
    fxNear = f32(select(_e423, _e422, _e421));
    let _e426 = fxGoodSpan;
    if _e426 {
        let _e427 = fxNear;
        let _e428 = fxSpanLen;
        let _e429 = fxSpanLen;
        local_1 = (0.5f - (_e427 * inverseSqrt((_e428 * _e429))));
    } else {
        local_1 = 0f;
    }
    let _e434 = local_1;
    fxEdgeBlend = _e434;
    let _e435 = fxEdgeBlend;
    let _e436 = fxSubpixel;
    fxBlend = clamp(max(_e435, _e436), 0f, 1f);
    let _e439 = (*px);
    let _e440 = fxAcross;
    param_22 = (_e439 + _e440);
    let _e442 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e442.xyz;
    let _e444 = (*centerColor);
    let _e446 = fxBlend;
    let _e449 = fxAcrossColor;
    let _e450 = fxBlend;
    let _e452 = ((_e444.xyz * (1f - _e446)) + (_e449 * _e450));
    let _e454 = (*centerColor)[3u];
    return vec4<f32>(_e452.x, _e452.y, _e452.z, _e454);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e88 = (*a);
    oneMinus = (1f - _e88);
    let _e90 = (*x);
    let _e91 = oneMinus;
    left = (_e90 * _e91);
    let _e93 = (*y);
    let _e94 = (*a);
    right = (_e93 * _e94);
    let _e96 = left;
    let _e97 = right;
    result = (_e96 + _e97);
    let _e99 = result;
    return _e99;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e84 = (*weight);
    let _e85 = (*confidence);
    scaled = (_e84 * _e85);
    let _e87 = scaled;
    return _e87;
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_147_: bool;

    let _e82 = (*v_1);
    let _e83 = (*v_1);
    let _e85 = all((_e82 == _e83));
    phi_147_ = _e85;
    if _e85 {
        let _e86 = (*v_1);
        phi_147_ = all((abs(_e86) <= vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e91 = phi_147_;
    return _e91;
}

fn neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b_vf3_u003b(tileIndex: ptr<function, i32>, boxMin: ptr<function, vec3<f32>>, boxMax: ptr<function, vec3<f32>>, boxMean: ptr<function, vec3<f32>>) -> bool {
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
    var phi_1136_: bool;

    neighborhoodMin = vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f);
    neighborhoodMax = vec3<f32>(-340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f);
    momentSum = vec3<f32>(0f, 0f, 0f);
    momentSquares = vec3<f32>(0f, 0f, 0f);
    y_1 = -1i;
    loop {
        let _e98 = y_1;
        if (_e98 <= 1i) {
            x_1 = -1i;
            loop {
                let _e100 = x_1;
                if (_e100 <= 1i) {
                    let _e102 = (*tileIndex);
                    let _e103 = y_1;
                    let _e106 = x_1;
                    let _e109 = tileColor[((_e102 + (_e103 * 10i)) + _e106)];
                    c_1 = _e109.xyz;
                    let _e111 = neighborhoodMin;
                    let _e112 = c_1;
                    neighborhoodMin = min(_e111, _e112);
                    let _e114 = neighborhoodMax;
                    let _e115 = c_1;
                    neighborhoodMax = max(_e114, _e115);
                    let _e117 = c_1;
                    let _e118 = momentSum;
                    momentSum = (_e118 + _e117);
                    let _e120 = c_1;
                    let _e121 = c_1;
                    let _e123 = momentSquares;
                    momentSquares = (_e123 + (_e120 * _e121));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e125 = x_1;
                    x_1 = (_e125 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e127 = y_1;
            y_1 = (_e127 + 1i);
        }
    }
    let _e129 = neighborhoodMin;
    param_23 = _e129;
    let _e130 = finite3_u0028_vf3_u003b((&param_23));
    phi_1136_ = _e130;
    if _e130 {
        let _e131 = neighborhoodMax;
        param_24 = _e131;
        let _e132 = finite3_u0028_vf3_u003b((&param_24));
        phi_1136_ = _e132;
    }
    let _e134 = phi_1136_;
    if !(_e134) {
        return false;
    }
    let _e136 = neighborhoodMin;
    clipMin = _e136;
    let _e137 = neighborhoodMax;
    clipMax = _e137;
    let _e139 = pc.qualityFlags;
    if ((_e139 & 1u) != 0u) {
        let _e142 = momentSum;
        momentMean = (_e142 * 0.11111111f);
        let _e144 = momentSquares;
        let _e146 = momentMean;
        let _e147 = momentMean;
        momentSigma = sqrt(max(((_e144 * 0.11111111f) - (_e146 * _e147)), vec3<f32>(0f, 0f, 0f)));
        let _e152 = neighborhoodMin;
        let _e153 = momentMean;
        let _e154 = momentSigma;
        clipMin = max(_e152, (_e153 - (_e154 * 1.25f)));
        let _e158 = neighborhoodMax;
        let _e159 = momentMean;
        let _e160 = momentSigma;
        clipMax = min(_e158, (_e159 + (_e160 * 1.25f)));
    }
    let _e164 = clipMin;
    (*boxMin) = _e164;
    let _e165 = clipMax;
    (*boxMax) = _e165;
    let _e166 = momentSum;
    (*boxMean) = (_e166 * 0.11111111f);
    return true;
}

fn mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b(x_2: ptr<function, vec4<f32>>, y_2: ptr<function, vec4<f32>>, a_1: ptr<function, f32>) -> vec4<f32> {
    let _e84 = (*x_2);
    let _e85 = (*a_1);
    let _e88 = (*y_2);
    let _e89 = (*a_1);
    return ((_e84 * (1f - _e85)) + (_e88 * _e89));
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

    let _e101 = (*texel);
    baseFloat = floor(_e101);
    let _e103 = baseFloat;
    base_1 = vec2<i32>(_e103);
    let _e105 = (*texel);
    let _e106 = baseFloat;
    f = (_e105 - _e106);
    let _e109 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e109) - vec2<i32>(1i, 1i));
    let _e112 = base_1;
    let _e113 = hi;
    let _e115 = textureLoad(previousColor, clamp(_e112, vec2<i32>(0i, 0i), _e113), 0i);
    c00_ = _e115;
    let _e116 = base_1;
    let _e118 = hi;
    let _e120 = textureLoad(previousColor, clamp((_e116 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e118), 0i);
    c10_ = _e120;
    let _e121 = base_1;
    let _e123 = hi;
    let _e125 = textureLoad(previousColor, clamp((_e121 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e123), 0i);
    c01_ = _e125;
    let _e126 = base_1;
    let _e128 = hi;
    let _e130 = textureLoad(previousColor, clamp((_e126 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e128), 0i);
    c11_ = _e130;
    let _e131 = c00_;
    param_25 = _e131;
    let _e132 = c10_;
    param_26 = _e132;
    let _e134 = f[0u];
    param_27 = _e134;
    let _e135 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_25), (&param_26), (&param_27));
    top = _e135;
    let _e136 = c01_;
    param_28 = _e136;
    let _e137 = c11_;
    param_29 = _e137;
    let _e139 = f[0u];
    param_30 = _e139;
    let _e140 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_28), (&param_29), (&param_30));
    bottom = _e140;
    let _e141 = top;
    param_31 = _e141;
    let _e142 = bottom;
    param_32 = _e142;
    let _e144 = f[1u];
    param_33 = _e144;
    let _e145 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_31), (&param_32), (&param_33));
    return _e145;
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_3: ptr<function, f32>, y_3: ptr<function, f32>, a_2: ptr<function, f32>) -> f32 {
    let _e84 = (*x_3);
    let _e85 = (*a_2);
    let _e88 = (*y_3);
    let _e89 = (*a_2);
    return ((_e84 * (1f - _e85)) + (_e88 * _e89));
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

    let _e101 = (*texel_1);
    baseFloat_1 = floor(_e101);
    let _e103 = baseFloat_1;
    base_2 = vec2<i32>(_e103);
    let _e105 = (*texel_1);
    let _e106 = baseFloat_1;
    f_1 = (_e105 - _e106);
    let _e109 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e109) - vec2<i32>(1i, 1i));
    let _e112 = base_2;
    let _e113 = hi_1;
    let _e115 = textureLoad(previousLinearDepth, clamp(_e112, vec2<i32>(0i, 0i), _e113), 0i);
    d00_ = _e115.x;
    let _e117 = base_2;
    let _e119 = hi_1;
    let _e121 = textureLoad(previousLinearDepth, clamp((_e117 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e119), 0i);
    d10_ = _e121.x;
    let _e123 = base_2;
    let _e125 = hi_1;
    let _e127 = textureLoad(previousLinearDepth, clamp((_e123 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e125), 0i);
    d01_ = _e127.x;
    let _e129 = base_2;
    let _e131 = hi_1;
    let _e133 = textureLoad(previousLinearDepth, clamp((_e129 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e131), 0i);
    d11_ = _e133.x;
    let _e135 = d00_;
    param_34 = _e135;
    let _e136 = d10_;
    param_35 = _e136;
    let _e138 = f_1[0u];
    param_36 = _e138;
    let _e139 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_34), (&param_35), (&param_36));
    top_1 = _e139;
    let _e140 = d01_;
    param_37 = _e140;
    let _e141 = d11_;
    param_38 = _e141;
    let _e143 = f_1[0u];
    param_39 = _e143;
    let _e144 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_37), (&param_38), (&param_39));
    bottom_1 = _e144;
    let _e145 = top_1;
    param_40 = _e145;
    let _e146 = bottom_1;
    param_41 = _e146;
    let _e148 = f_1[1u];
    param_42 = _e148;
    let _e149 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_40), (&param_41), (&param_42));
    return _e149;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator_1: f32;
    var numerator: f32;
    var result_1: f32;

    let _e88 = pc.zFar;
    let _e90 = pc.zNear;
    range = (_e88 - _e90);
    let _e92 = (*z_1);
    let _e93 = range;
    scaled_1 = (_e92 * _e93);
    let _e96 = pc.zNear;
    let _e97 = scaled_1;
    denominator_1 = (_e96 + _e97);
    let _e100 = pc.zNear;
    let _e102 = pc.zFar;
    numerator = (_e100 * _e102);
    let _e104 = numerator;
    let _e105 = denominator_1;
    result_1 = (_e104 / _e105);
    let _e107 = result_1;
    return _e107;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_132_: bool;

    let _e82 = (*v_2);
    let _e83 = (*v_2);
    let _e85 = all((_e82 == _e83));
    phi_132_ = _e85;
    if _e85 {
        let _e86 = (*v_2);
        phi_132_ = all((abs(_e86) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e91 = phi_132_;
    return _e91;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_162_: bool;

    let _e82 = (*v_3);
    let _e83 = (*v_3);
    let _e85 = all((_e82 == _e83));
    phi_162_ = _e85;
    if _e85 {
        let _e86 = (*v_3);
        phi_162_ = all((abs(_e86) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e91 = phi_162_;
    return _e91;
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
    var phi_960_: bool;
    var phi_968_: bool;

    let _e95 = (*px_1);
    let _e96 = textureLoad(motionVelocity, _e95, 0i);
    centerVelocity = _e96.xy;
    let _e99 = pc.qualityFlags;
    let _e101 = ((_e99 & 2u) == 0u);
    phi_960_ = _e101;
    if !(_e101) {
        let _e103 = (*centerValid);
        phi_960_ = !((_e103 > 0f));
    }
    let _e107 = phi_960_;
    phi_968_ = _e107;
    if !(_e107) {
        let _e109 = centerVelocity;
        param_43 = _e109;
        let _e110 = finite2_u0028_vf2_u003b((&param_43));
        phi_968_ = !(_e110);
    }
    let _e113 = phi_968_;
    if _e113 {
        let _e114 = centerVelocity;
        return _e114;
    }
    let _e116 = pc.extent;
    hi_2 = (bitcast<vec2<i32>>(_e116) - vec2<i32>(1i, 1i));
    let _e119 = (*px_1);
    best = _e119;
    let _e120 = (*tileIndex_1);
    let _e122 = tileDepth[_e120];
    bestDepth = _e122;
    y_4 = -1i;
    loop {
        let _e123 = y_4;
        if (_e123 <= 1i) {
            x_4 = -1i;
            loop {
                let _e125 = x_4;
                if (_e125 <= 1i) {
                    let _e127 = (*tileIndex_1);
                    let _e128 = y_4;
                    let _e131 = x_4;
                    let _e134 = tileDepth[((_e127 + (_e128 * 10i)) + _e131)];
                    d = _e134;
                    let _e135 = d;
                    let _e136 = bestDepth;
                    if (_e135 > _e136) {
                        let _e138 = d;
                        bestDepth = _e138;
                        let _e139 = (*px_1);
                        let _e140 = x_4;
                        let _e141 = y_4;
                        let _e144 = hi_2;
                        best = clamp((_e139 + vec2<i32>(_e140, _e141)), vec2<i32>(0i, 0i), _e144);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e146 = x_4;
                    x_4 = (_e146 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e148 = y_4;
            y_4 = (_e148 + 1i);
        }
    }
    let _e150 = best;
    let _e151 = (*px_1);
    if all((_e150 == _e151)) {
        let _e154 = centerVelocity;
        return _e154;
    }
    let _e155 = best;
    let _e156 = textureLoad(motionValidity, _e155, 0i);
    v_4 = _e156.x;
    let _e158 = v_4;
    if !((_e158 > 0f)) {
        let _e161 = centerVelocity;
        return _e161;
    }
    let _e162 = best;
    let _e163 = textureLoad(motionVelocity, _e162, 0i);
    dv = _e163.xy;
    let _e165 = dv;
    param_44 = _e165;
    let _e166 = finite2_u0028_vf2_u003b((&param_44));
    let _e167 = dv;
    let _e168 = centerVelocity;
    return select(_e168, _e167, vec2(_e166));
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
    var clipMean: vec3<f32>;
    var param_58: i32;
    var param_59: vec3<f32>;
    var param_60: vec3<f32>;
    var param_61: vec3<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var baseWeight: f32;
    var weight_1: f32;
    var param_62: f32;
    var param_63: f32;
    var resolvedRgb: vec3<f32>;
    var param_64: vec3<f32>;
    var param_65: vec3<f32>;
    var param_66: f32;
    var param_67: vec4<f32>;
    var param_68: vec2<i32>;
    var param_69: vec4<f32>;
    var fallbackMin: vec3<f32>;
    var fallbackMax: vec3<f32>;
    var fallbackMean: vec3<f32>;
    var param_70: i32;
    var param_71: vec3<f32>;
    var param_72: vec3<f32>;
    var param_73: vec3<f32>;
    var param_74: vec4<f32>;
    var param_75: f32;
    var phi_1342_: bool;
    var phi_1351_: bool;
    var phi_1360_: bool;
    var phi_1366_: bool;
    var phi_1403_: bool;
    var phi_1418_: bool;
    var phi_1442_: bool;
    var phi_1498_: bool;
    var phi_1506_: bool;

    let _e146 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e146.xy);
    let _e149 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e149.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e156 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e156) - vec2<i32>(1i, 1i));
    let _e159 = gl_LocalInvocationIndex_1;
    i_1 = _e159;
    loop {
        let _e160 = i_1;
        if (_e160 < 100u) {
            let _e162 = tileOrigin;
            let _e163 = i_1;
            let _e166 = i_1;
            let _e171 = imageHi;
            q = clamp((_e162 + vec2<i32>(bitcast<i32>((_e163 % 10u)), bitcast<i32>((_e166 / 10u)))), vec2<i32>(0i, 0i), _e171);
            let _e173 = i_1;
            let _e174 = q;
            let _e175 = textureLoad(currentColor, _e174, 0i);
            tileColor[_e173] = _e175;
            let _e177 = i_1;
            let _e178 = q;
            let _e179 = textureLoad(currentDeviceDepth, _e178, 0i);
            tileDepth[_e177] = _e179.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e182 = i_1;
            i_1 = (_e182 + 64u);
        }
    }
    workgroupBarrier();
    let _e184 = pixel_1;
    let _e186 = pc.extent;
    if any((_e184 >= bitcast<vec2<i32>>(_e186))) {
        return;
    }
    let _e191 = gl_LocalInvocationID_1[1u];
    let _e196 = gl_LocalInvocationID_1[0u];
    tileIndex_2 = ((((bitcast<i32>(_e191) + 1i) * 10i) + bitcast<i32>(_e196)) + 1i);
    let _e200 = tileIndex_2;
    let _e202 = tileColor[_e200];
    current = _e202;
    let _e203 = current;
    result_2 = _e203;
    usedHistory = false;
    let _e204 = pixel_1;
    let _e205 = textureLoad(motionValidity, _e204, 0i);
    valid = _e205.x;
    let _e207 = pixel_1;
    param_45 = _e207;
    let _e208 = tileIndex_2;
    param_46 = _e208;
    let _e209 = valid;
    param_47 = _e209;
    let _e210 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_45), (&param_46), (&param_47));
    velocity = _e210;
    let _e212 = pc.previousEffectiveJitterUv;
    let _e214 = pc.currentEffectiveJitterUv;
    jitterDelta = (_e212 - _e214);
    let _e216 = jitterDelta;
    let _e218 = pc.extent;
    jitterTexel = (_e216 * vec2<f32>(_e218));
    let _e221 = velocity;
    let _e223 = pc.extent;
    motionTexel = (_e221 * vec2<f32>(_e223));
    let _e226 = pixel_1;
    let _e228 = motionTexel;
    unjitteredPreviousTexel = (vec2<f32>(_e226) - _e228);
    let _e230 = unjitteredPreviousTexel;
    let _e231 = jitterTexel;
    previousTexel = (_e230 + _e231);
    let _e233 = previousTexel;
    previousCenter = (_e233 + vec2(0.5f));
    let _e236 = previousCenter;
    let _e238 = all((_e236 >= vec2<f32>(0f, 0f)));
    phi_1342_ = _e238;
    if _e238 {
        let _e239 = previousCenter;
        let _e241 = pc.extent;
        phi_1342_ = all((_e239 < vec2<f32>(_e241)));
    }
    let _e246 = phi_1342_;
    inside = _e246;
    let _e247 = current;
    param_48 = _e247;
    let _e248 = finite4_u0028_vf4_u003b((&param_48));
    phi_1351_ = _e248;
    if _e248 {
        let _e249 = valid;
        param_49 = _e249;
        let _e250 = finite1_u0028_f1_u003b((&param_49));
        phi_1351_ = _e250;
    }
    let _e252 = phi_1351_;
    let _e253 = valid;
    let _e255 = (_e252 && (_e253 > 0f));
    phi_1360_ = _e255;
    if _e255 {
        let _e256 = velocity;
        param_50 = _e256;
        let _e257 = finite2_u0028_vf2_u003b((&param_50));
        phi_1360_ = _e257;
    }
    let _e259 = phi_1360_;
    phi_1366_ = _e259;
    if _e259 {
        let _e260 = previousTexel;
        param_51 = _e260;
        let _e261 = finite2_u0028_vf2_u003b((&param_51));
        phi_1366_ = _e261;
    }
    let _e263 = phi_1366_;
    let _e264 = inside;
    if (_e263 && _e264) {
        let _e266 = tileIndex_2;
        let _e268 = tileDepth[_e266];
        currentDevice = _e268;
        let _e269 = currentDevice;
        param_52 = _e269;
        let _e270 = linearizeDepth_u0028_f1_u003b((&param_52));
        currentLinear = _e270;
        let _e271 = previousTexel;
        param_53 = _e271;
        let _e272 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_53));
        priorLinear = _e272;
        let _e274 = pc.depthThresholdAbsolute;
        let _e276 = pc.depthThresholdRelative;
        let _e277 = currentLinear;
        let _e278 = priorLinear;
        depthLimit = max(_e274, (_e276 * max(_e277, _e278)));
        let _e282 = currentLinear;
        param_54 = _e282;
        let _e283 = finite1_u0028_f1_u003b((&param_54));
        phi_1403_ = _e283;
        if _e283 {
            let _e284 = priorLinear;
            param_55 = _e284;
            let _e285 = finite1_u0028_f1_u003b((&param_55));
            phi_1403_ = _e285;
        }
        let _e287 = phi_1403_;
        let _e288 = currentLinear;
        let _e291 = priorLinear;
        let _e293 = ((_e287 && (_e288 > 0f)) && (_e291 > 0f));
        phi_1418_ = _e293;
        if _e293 {
            let _e294 = currentLinear;
            let _e295 = priorLinear;
            let _e298 = depthLimit;
            phi_1418_ = (abs((_e294 - _e295)) <= _e298);
        }
        let _e301 = phi_1418_;
        if _e301 {
            let _e302 = previousTexel;
            param_56 = _e302;
            let _e303 = samplePreviousColorLinear_u0028_vf2_u003b((&param_56));
            prior = _e303;
            let _e304 = prior;
            param_57 = _e304;
            let _e305 = finite4_u0028_vf4_u003b((&param_57));
            phi_1442_ = _e305;
            if _e305 {
                let _e306 = tileIndex_2;
                param_58 = _e306;
                let _e307 = neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_58), (&param_59), (&param_60), (&param_61));
                let _e308 = param_59;
                clipMin_1 = _e308;
                let _e309 = param_60;
                clipMax_1 = _e309;
                let _e310 = param_61;
                clipMean = _e310;
                phi_1442_ = _e307;
            }
            let _e312 = phi_1442_;
            if _e312 {
                let _e313 = prior;
                let _e315 = clipMin_1;
                let _e316 = clipMax_1;
                clampedPrior = clamp(_e313.xyz, _e315, _e316);
                let _e318 = valid;
                confidence_1 = clamp(_e318, 0f, 1f);
                let _e321 = pc.historyWeight;
                baseWeight = clamp(_e321, 0f, 1f);
                let _e323 = baseWeight;
                param_62 = _e323;
                let _e324 = confidence_1;
                param_63 = _e324;
                let _e325 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_62), (&param_63));
                weight_1 = _e325;
                let _e326 = current;
                param_64 = _e326.xyz;
                let _e328 = clampedPrior;
                param_65 = _e328;
                let _e329 = weight_1;
                param_66 = _e329;
                let _e330 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_64), (&param_65), (&param_66));
                resolvedRgb = _e330;
                let _e331 = resolvedRgb;
                let _e332 = current;
                let _e334 = clipMean;
                let _e338 = clipMin_1;
                let _e339 = clipMax_1;
                let _e340 = clamp((_e331 + ((_e332.xyz - _e334) * 0.6f)), _e338, _e339);
                let _e342 = current[3u];
                result_2 = vec4<f32>(_e340.x, _e340.y, _e340.z, _e342);
                usedHistory = true;
            }
        }
    }
    let _e347 = usedHistory;
    let _e348 = !(_e347);
    phi_1498_ = _e348;
    if _e348 {
        let _e349 = current;
        param_67 = _e349;
        let _e350 = finite4_u0028_vf4_u003b((&param_67));
        phi_1498_ = _e350;
    }
    let _e352 = phi_1498_;
    phi_1506_ = _e352;
    if _e352 {
        let _e354 = pc.qualityFlags;
        phi_1506_ = ((_e354 & 4u) != 0u);
    }
    let _e358 = phi_1506_;
    if _e358 {
        let _e359 = pixel_1;
        param_68 = _e359;
        let _e360 = current;
        param_69 = _e360;
        let _e361 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_68), (&param_69));
        result_2 = _e361;
        let _e362 = tileIndex_2;
        param_70 = _e362;
        let _e363 = neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_70), (&param_71), (&param_72), (&param_73));
        let _e364 = param_71;
        fallbackMin = _e364;
        let _e365 = param_72;
        fallbackMax = _e365;
        let _e366 = param_73;
        fallbackMean = _e366;
        if _e363 {
            let _e367 = result_2;
            let _e369 = current;
            let _e371 = fallbackMean;
            let _e375 = fallbackMin;
            let _e376 = fallbackMax;
            let _e377 = clamp((_e367.xyz + ((_e369.xyz - _e371) * 0.4f)), _e375, _e376);
            let _e379 = result_2[3u];
            result_2 = vec4<f32>(_e377.x, _e377.y, _e377.z, _e379);
        }
    }
    let _e384 = result_2;
    param_74 = _e384;
    let _e385 = finite4_u0028_vf4_u003b((&param_74));
    if !(_e385) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e387 = pixel_1;
    let _e388 = result_2;
    textureStore(resolvedColor, _e387, _e388);
    let _e389 = pixel_1;
    let _e390 = result_2;
    textureStore(historyOutColor, _e389, _e390);
    let _e391 = pixel_1;
    let _e392 = tileIndex_2;
    let _e394 = tileDepth[_e392];
    param_75 = _e394;
    let _e395 = storeLinearDepth_u0028_f1_u003b((&param_75));
    textureStore(historyOutLinearDepth, _e391, vec4(_e395));
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
