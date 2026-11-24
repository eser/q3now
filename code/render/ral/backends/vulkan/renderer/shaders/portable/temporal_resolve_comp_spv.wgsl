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

    let _e93 = pc.zNear;
    let _e94 = (*z);
    let _e96 = pc.zFar;
    let _e98 = pc.zNear;
    denominator = (_e93 + (_e94 * (_e96 - _e98)));
    let _e103 = pc.zNear;
    let _e105 = pc.zFar;
    let _e107 = denominator;
    let _e108 = denominator;
    return ((_e103 * _e105) * inverseSqrt((_e107 * _e108)));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e92 = (*pixel);
    let _e94 = pc.extent;
    bounded = clamp(_e92, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e94) - vec2<i32>(1i, 1i)));
    let _e98 = bounded;
    let _e99 = textureLoad(currentColor, _e98, 0i);
    return _e99;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_142_: bool;

    let _e91 = (*v);
    let _e92 = (*v);
    let _e93 = (_e91 == _e92);
    phi_142_ = _e93;
    if _e93 {
        let _e94 = (*v);
        phi_142_ = (abs(_e94) <= 0x1.fffffep+127f);
    }
    let _e98 = phi_142_;
    return _e98;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e93 = (*c);
    l = dot(_e93, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e95 = l;
    param = _e95;
    let _e96 = finite1_u0028_f1_u003b((&param));
    if !(_e96) {
        return 0f;
    }
    let _e98 = l;
    l = clamp(_e98, 0f, 10000f);
    let _e100 = l;
    let _e101 = l;
    let _e102 = l;
    return (_e100 * inverseSqrt((1f + (_e101 * _e102))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e93 = (*p);
    param_1 = _e93;
    let _e94 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e94.xyz;
    let _e96 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e96;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e94 = (*base);
    param_3 = _e94;
    let _e95 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e96 = (*base);
    let _e97 = (*across);
    param_4 = (_e96 + _e97);
    let _e99 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e95 + _e99));
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

    let _e163 = (*centerColor);
    param_5 = _e163.xyz;
    let _e165 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e165;
    let _e166 = (*px);
    param_6 = (_e166 + vec2<i32>(0i, -1i));
    let _e168 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e168;
    let _e169 = (*px);
    param_7 = (_e169 + vec2<i32>(0i, 1i));
    let _e171 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e171;
    let _e172 = (*px);
    param_8 = (_e172 + vec2<i32>(-1i, 0i));
    let _e174 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e174;
    let _e175 = (*px);
    param_9 = (_e175 + vec2<i32>(1i, 0i));
    let _e177 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e177;
    let _e178 = fxN;
    let _e179 = fxS;
    let _e181 = fxW;
    let _e182 = fxE;
    let _e184 = fxM;
    fxHi = max(max(_e178, _e179), max(max(_e181, _e182), _e184));
    let _e187 = fxN;
    let _e188 = fxS;
    let _e190 = fxW;
    let _e191 = fxE;
    let _e193 = fxM;
    fxLo = min(min(_e187, _e188), min(min(_e190, _e191), _e193));
    let _e196 = fxHi;
    let _e197 = fxLo;
    fxSpan = (_e196 - _e197);
    let _e199 = fxSpan;
    let _e200 = fxHi;
    if (_e199 < max(0.0312f, (_e200 * 0.125f))) {
        let _e204 = (*centerColor);
        return _e204;
    }
    let _e205 = (*px);
    param_10 = (_e205 + vec2<i32>(-1i, -1i));
    let _e207 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e207;
    let _e208 = (*px);
    param_11 = (_e208 + vec2<i32>(1i, -1i));
    let _e210 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e210;
    let _e211 = (*px);
    param_12 = (_e211 + vec2<i32>(-1i, 1i));
    let _e213 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e213;
    let _e214 = (*px);
    param_13 = (_e214 + vec2<i32>(1i, 1i));
    let _e216 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e216;
    let _e217 = fxN;
    let _e218 = fxS;
    fxNS = (_e217 + _e218);
    let _e220 = fxW;
    let _e221 = fxE;
    fxWE = (_e220 + _e221);
    let _e223 = fxNW;
    let _e224 = fxSW;
    fxNWSW = (_e223 + _e224);
    let _e226 = fxNE;
    let _e227 = fxSE;
    fxNESE = (_e226 + _e227);
    let _e229 = fxW;
    let _e231 = fxNWSW;
    let _e234 = fxM;
    let _e236 = fxNS;
    let _e240 = fxE;
    let _e242 = fxNESE;
    fxHorz = (abs(((-2f * _e229) + _e231)) + ((abs(((-2f * _e234) + _e236)) * 2f) + abs(((-2f * _e240) + _e242))));
    let _e247 = fxS;
    let _e249 = fxSW;
    let _e250 = fxSE;
    let _e254 = fxM;
    let _e256 = fxWE;
    let _e260 = fxN;
    let _e262 = fxNW;
    let _e263 = fxNE;
    fxVert = (abs(((-2f * _e247) + (_e249 + _e250))) + ((abs(((-2f * _e254) + _e256)) * 2f) + abs(((-2f * _e260) + (_e262 + _e263)))));
    let _e269 = fxHorz;
    let _e270 = fxVert;
    fxHorzSpan = (_e269 >= _e270);
    let _e272 = fxNS;
    let _e273 = fxWE;
    let _e276 = fxNWSW;
    let _e277 = fxNESE;
    fxAround = (((_e272 + _e273) * 2f) + (_e276 + _e277));
    let _e280 = fxHorzSpan;
    let _e281 = fxN;
    let _e282 = fxW;
    fxLoSide = select(_e282, _e281, _e280);
    let _e284 = fxHorzSpan;
    let _e285 = fxS;
    let _e286 = fxE;
    fxHiSide = select(_e286, _e285, _e284);
    let _e288 = fxLoSide;
    let _e289 = fxM;
    fxGradLo = (_e288 - _e289);
    let _e291 = fxHiSide;
    let _e292 = fxM;
    fxGradHi = (_e291 - _e292);
    let _e294 = fxGradLo;
    let _e296 = fxGradHi;
    fxLoSteeper = (abs(_e294) >= abs(_e296));
    let _e299 = fxGradLo;
    let _e301 = fxGradHi;
    fxGradient = max(abs(_e299), abs(_e301));
    let _e304 = fxLoSteeper;
    if _e304 {
        let _e305 = fxLoSide;
        let _e306 = fxM;
        local = (_e305 + _e306);
    } else {
        let _e308 = fxHiSide;
        let _e309 = fxM;
        local = (_e308 + _e309);
    }
    let _e311 = local;
    fxPair = _e311;
    let _e312 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e312));
    let _e315 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e315));
    let _e318 = fxLoSteeper;
    if _e318 {
        let _e319 = fxAcross;
        fxAcross = -(_e319);
    }
    let _e321 = fxSpan;
    let _e322 = fxSpan;
    fxInvSpan = inverseSqrt((_e321 * _e322));
    let _e325 = fxAround;
    let _e327 = fxM;
    let _e330 = fxInvSpan;
    fxBlendC = clamp((abs(((_e325 * 0.083333336f) - _e327)) * _e330), 0f, 1f);
    let _e333 = fxBlendC;
    let _e336 = fxBlendC;
    let _e337 = fxBlendC;
    fxBlendF = (((-2f * _e333) + 3f) * (_e336 * _e337));
    let _e340 = fxBlendF;
    let _e341 = fxBlendF;
    fxSubpixel = ((_e340 * _e341) * 0.75f);
    let _e344 = fxPair;
    fxHalfPair = (_e344 * 0.5f);
    let _e346 = fxGradient;
    fxScaled = (_e346 * 0.25f);
    let _e348 = fxM;
    let _e349 = fxHalfPair;
    fxMidNegative = ((_e348 - _e349) < 0f);
    let _e352 = (*px);
    let _e353 = fxAlong;
    param_14 = (_e352 - _e353);
    let _e355 = fxAcross;
    param_15 = _e355;
    let _e356 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e357 = fxHalfPair;
    fxEndN = (_e356 - _e357);
    let _e359 = (*px);
    let _e360 = fxAlong;
    param_16 = (_e359 + _e360);
    let _e362 = fxAcross;
    param_17 = _e362;
    let _e363 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e364 = fxHalfPair;
    fxEndP = (_e363 - _e364);
    let _e366 = fxEndN;
    let _e368 = fxScaled;
    fxDoneN = (abs(_e366) >= _e368);
    let _e370 = fxEndP;
    let _e372 = fxScaled;
    fxDoneP = (abs(_e370) >= _e372);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e374 = i;
        if (_e374 <= 12i) {
            let _e376 = fxDoneN;
            if !(_e376) {
                let _e378 = (*px);
                let _e379 = fxAlong;
                let _e380 = i;
                param_18 = (_e378 - (_e379 * vec2(_e380)));
                let _e384 = fxAcross;
                param_19 = _e384;
                let _e385 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e386 = fxHalfPair;
                fxEndN = (_e385 - _e386);
                let _e388 = fxEndN;
                let _e390 = fxScaled;
                fxDoneN = (abs(_e388) >= _e390);
                let _e392 = i;
                fxStepsN = _e392;
            }
            let _e393 = fxDoneP;
            if !(_e393) {
                let _e395 = (*px);
                let _e396 = fxAlong;
                let _e397 = i;
                param_20 = (_e395 + (_e396 * vec2(_e397)));
                let _e401 = fxAcross;
                param_21 = _e401;
                let _e402 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e403 = fxHalfPair;
                fxEndP = (_e402 - _e403);
                let _e405 = fxEndP;
                let _e407 = fxScaled;
                fxDoneP = (abs(_e405) >= _e407);
                let _e409 = i;
                fxStepsP = _e409;
            }
            let _e410 = fxDoneN;
            let _e411 = fxDoneP;
            if (_e410 && _e411) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e413 = i;
            i = (_e413 + 1i);
        }
    }
    let _e415 = fxStepsN;
    let _e416 = fxStepsP;
    fxTowardN = (_e415 < _e416);
    let _e418 = fxTowardN;
    let _e419 = fxEndN;
    let _e420 = fxEndP;
    fxEndG = select(_e420, _e419, _e418);
    let _e422 = fxEndG;
    let _e424 = fxMidNegative;
    fxGoodSpan = ((_e422 < 0f) != _e424);
    let _e426 = fxStepsN;
    let _e427 = fxStepsP;
    fxSpanLen = f32((_e426 + _e427));
    let _e430 = fxTowardN;
    let _e431 = fxStepsN;
    let _e432 = fxStepsP;
    fxNear = f32(select(_e432, _e431, _e430));
    let _e435 = fxGoodSpan;
    if _e435 {
        let _e436 = fxNear;
        let _e437 = fxSpanLen;
        let _e438 = fxSpanLen;
        local_1 = (0.5f - (_e436 * inverseSqrt((_e437 * _e438))));
    } else {
        local_1 = 0f;
    }
    let _e443 = local_1;
    fxEdgeBlend = _e443;
    let _e444 = fxEdgeBlend;
    let _e445 = fxSubpixel;
    fxBlend = clamp(max(_e444, _e445), 0f, 1f);
    let _e448 = (*px);
    let _e449 = fxAcross;
    param_22 = (_e448 + _e449);
    let _e451 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e451.xyz;
    let _e453 = (*centerColor);
    let _e455 = fxBlend;
    let _e458 = fxAcrossColor;
    let _e459 = fxBlend;
    let _e461 = ((_e453.xyz * (1f - _e455)) + (_e458 * _e459));
    let _e463 = (*centerColor)[3u];
    return vec4<f32>(_e461.x, _e461.y, _e461.z, _e463);
}

fn samplePreviousBlur_u0028_vf2_u003b_vf3_u003b_vf3_u003b(texel: ptr<function, vec2<f32>>, tapMin: ptr<function, vec3<f32>>, tapMax: ptr<function, vec3<f32>>) -> vec3<f32> {
    var extent: vec2<f32>;
    var invExtent: vec2<f32>;
    var sum: vec3<f32>;
    var i_1: i32;
    var o: vec2<f32>;
    var tap: vec3<f32>;

    let _e100 = pc.extent;
    extent = vec2<f32>(_e100);
    let _e102 = extent;
    let _e103 = extent;
    invExtent = inverseSqrt((_e102 * _e103));
    sum = vec3<f32>(0f, 0f, 0f);
    i_1 = 0i;
    loop {
        let _e106 = i_1;
        if (_e106 < 4i) {
            let _e108 = i_1;
            let _e112 = i_1;
            o = vec2<f32>(select(-0.5f, 0.5f, ((_e108 & 1i) != 0i)), select(-0.5f, 0.5f, ((_e112 & 2i) != 0i)));
            let _e117 = (*texel);
            let _e120 = o;
            let _e122 = invExtent;
            let _e124 = textureSampleLevel(previousColor, nearestSampler, (((_e117 + vec2(0.5f)) + _e120) * _e122), 0f);
            tap = _e124.xyz;
            let _e126 = tap;
            let _e127 = sum;
            sum = (_e127 + _e126);
            let _e129 = (*tapMin);
            let _e130 = tap;
            (*tapMin) = min(_e129, _e130);
            let _e132 = (*tapMax);
            let _e133 = tap;
            (*tapMax) = max(_e132, _e133);
            continue;
        } else {
            break;
        }
        continuing {
            let _e135 = i_1;
            i_1 = (_e135 + 1i);
        }
    }
    let _e137 = sum;
    return (_e137 * 0.25f);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e97 = (*a);
    oneMinus = (1f - _e97);
    let _e99 = (*x);
    let _e100 = oneMinus;
    left = (_e99 * _e100);
    let _e102 = (*y);
    let _e103 = (*a);
    right = (_e102 * _e103);
    let _e105 = left;
    let _e106 = right;
    result = (_e105 + _e106);
    let _e108 = result;
    return _e108;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e93 = (*weight);
    let _e94 = (*confidence);
    scaled = (_e93 * _e94);
    let _e96 = scaled;
    return _e96;
}

fn historyWeightForCount_u0028_f1_u003b(count: ptr<function, f32>) -> f32 {
    let _e91 = (*count);
    let _e92 = (*count);
    let _e94 = (*count);
    return (_e91 * inverseSqrt(((_e92 + 1f) * (_e94 + 1f))));
}

fn rectifiedHistoryCount_u0028_f1_u003b_vf3_u003b_vf3_u003b(count_1: ptr<function, f32>, clampMove: ptr<function, vec3<f32>>, boxExtent: ptr<function, vec3<f32>>) -> f32 {
    var boxSize: f32;
    var rectified: f32;

    let _e95 = (*boxExtent);
    boxSize = max(length(_e95), 0.0001f);
    let _e98 = (*clampMove);
    let _e100 = boxSize;
    let _e101 = boxSize;
    rectified = clamp(((length(_e98) * inverseSqrt((_e100 * _e101))) - 0.5f), 0f, 1f);
    let _e107 = (*count_1);
    let _e108 = (*count_1);
    let _e110 = rectified;
    return max(1f, (_e107 - ((_e108 - 1f) * _e110)));
}

fn movingHistoryCount_u0028_f1_u003b(movingWeight: ptr<function, f32>) -> f32 {
    var rest: f32;

    let _e92 = (*movingWeight);
    rest = max((1f - _e92), 0.041666668f);
    let _e95 = (*movingWeight);
    let _e96 = rest;
    let _e97 = rest;
    return (_e95 * inverseSqrt((_e96 * _e97)));
}

fn accumulatedHistoryCount_u0028_f1_u003b_f1_u003b_vf2_u003b(storedCount: ptr<function, f32>, movingWeight_1: ptr<function, f32>, motionPixels: ptr<function, vec2<f32>>) -> f32 {
    var count_2: f32;
    var moving: f32;
    var param_23: f32;

    let _e96 = (*storedCount);
    count_2 = clamp(_e96, 1f, 24f);
    let _e98 = (*motionPixels);
    moving = clamp((length(_e98) * 8f), 0f, 1f);
    let _e102 = count_2;
    let _e103 = count_2;
    let _e104 = (*movingWeight_1);
    param_23 = _e104;
    let _e105 = movingHistoryCount_u0028_f1_u003b((&param_23));
    let _e107 = count_2;
    let _e109 = moving;
    return (_e102 + ((min(_e103, _e105) - _e107) * _e109));
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

    let _e118 = (*texel_1);
    baseFloat = floor(_e118);
    let _e120 = (*texel_1);
    let _e121 = baseFloat;
    f = (_e120 - _e121);
    let _e123 = f;
    d0_ = (vec2(1f) + _e123);
    let _e126 = f;
    d1_ = _e126;
    let _e127 = f;
    d2_ = (vec2(1f) - _e127);
    let _e130 = f;
    d3_ = (vec2(2f) - _e130);
    let _e133 = d0_;
    let _e137 = d0_;
    let _e141 = d0_;
    w0_ = ((((((_e133 * -0.75f) - vec2(-3.75f)) * _e137) + vec2(-6f)) * _e141) - vec2(-3f));
    let _e145 = d1_;
    let _e149 = d1_;
    let _e151 = d1_;
    w1_ = (((((_e145 * 1.25f) - vec2(2.25f)) * _e149) * _e151) + vec2(1f));
    let _e155 = d2_;
    let _e159 = d2_;
    let _e161 = d2_;
    w2_ = (((((_e155 * 1.25f) - vec2(2.25f)) * _e159) * _e161) + vec2(1f));
    let _e165 = d3_;
    let _e169 = d3_;
    let _e173 = d3_;
    w3_ = ((((((_e165 * -0.75f) - vec2(-3.75f)) * _e169) + vec2(-6f)) * _e173) - vec2(-3f));
    let _e177 = w1_;
    let _e178 = w2_;
    w12_ = (_e177 + _e178);
    let _e180 = w2_;
    let _e181 = w12_;
    let _e182 = w12_;
    offset12_ = (_e180 * inverseSqrt((_e181 * _e182)));
    let _e187 = pc.extent;
    extent_1 = vec2<f32>(_e187);
    let _e189 = extent_1;
    let _e190 = extent_1;
    invExtent_1 = inverseSqrt((_e189 * _e190));
    let _e193 = baseFloat;
    let _e198 = invExtent_1;
    uv0_ = (((_e193 - vec2(1f)) + vec2(0.5f)) * _e198);
    let _e200 = baseFloat;
    let _e201 = offset12_;
    let _e205 = invExtent_1;
    uv12_ = (((_e200 + _e201) + vec2(0.5f)) * _e205);
    let _e207 = baseFloat;
    let _e212 = invExtent_1;
    uv3_ = (((_e207 + vec2(2f)) + vec2(0.5f)) * _e212);
    let _e215 = uv12_[0u];
    let _e217 = uv0_[0u];
    let _e219 = uv3_[0u];
    let _e221 = uv12_[0u];
    let _e223 = uv12_[0u];
    ux = array<f32, 5>(_e215, _e217, _e219, _e221, _e223);
    let _e226 = uv12_[1u];
    let _e228 = uv12_[1u];
    let _e230 = uv12_[1u];
    let _e232 = uv0_[1u];
    let _e234 = uv3_[1u];
    uy = array<f32, 5>(_e226, _e228, _e230, _e232, _e234);
    let _e237 = w12_[0u];
    let _e239 = w12_[1u];
    let _e242 = w0_[0u];
    let _e244 = w12_[1u];
    let _e247 = w3_[0u];
    let _e249 = w12_[1u];
    let _e252 = w12_[0u];
    let _e254 = w0_[1u];
    let _e257 = w12_[0u];
    let _e259 = w3_[1u];
    w = array<f32, 5>((_e237 * _e239), (_e242 * _e244), (_e247 * _e249), (_e252 * _e254), (_e257 * _e259));
    sum_1 = vec4<f32>(0f, 0f, 0f, 0f);
    total = 0f;
    i_2 = 0i;
    loop {
        let _e262 = i_2;
        if (_e262 < 5i) {
            let _e264 = i_2;
            let _e266 = ux[_e264];
            let _e267 = i_2;
            let _e269 = uy[_e267];
            let _e271 = textureSampleLevel(previousColor, nearestSampler, vec2<f32>(_e266, _e269), 0f);
            tap_1 = _e271;
            let _e272 = i_2;
            let _e274 = w[_e272];
            let _e275 = tap_1;
            let _e277 = sum_1;
            sum_1 = (_e277 + (_e275 * _e274));
            let _e279 = i_2;
            let _e281 = w[_e279];
            let _e282 = total;
            total = (_e282 + _e281);
            let _e284 = (*tapMin_1);
            let _e285 = tap_1;
            (*tapMin_1) = min(_e284, _e285.xyz);
            let _e288 = (*tapMax_1);
            let _e289 = tap_1;
            (*tapMax_1) = max(_e288, _e289.xyz);
            let _e292 = i_2;
            if (_e292 == 0i) {
                let _e295 = tap_1[3u];
                (*centreCount) = _e295;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e296 = i_2;
            i_2 = (_e296 + 1i);
        }
    }
    let _e298 = sum_1;
    let _e299 = total;
    let _e300 = total;
    return (_e298 * inverseSqrt((_e299 * _e300)));
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_172_: bool;

    let _e91 = (*v_1);
    let _e92 = (*v_1);
    let _e94 = all((_e91 == _e92));
    phi_172_ = _e94;
    if _e94 {
        let _e95 = (*v_1);
        phi_172_ = all((abs(_e95) <= vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e100 = phi_172_;
    return _e100;
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
        let _e104 = y_1;
        if (_e104 <= 1i) {
            x_1 = -1i;
            loop {
                let _e106 = x_1;
                if (_e106 <= 1i) {
                    let _e108 = (*tileIndex);
                    let _e109 = y_1;
                    let _e112 = x_1;
                    let _e115 = tileColor[((_e108 + (_e109 * 10i)) + _e112)];
                    c_1 = _e115.xyz;
                    let _e117 = neighborhoodMin;
                    let _e118 = c_1;
                    neighborhoodMin = min(_e117, _e118);
                    let _e120 = neighborhoodMax;
                    let _e121 = c_1;
                    neighborhoodMax = max(_e120, _e121);
                    let _e123 = c_1;
                    let _e124 = momentSum;
                    momentSum = (_e124 + _e123);
                    let _e126 = c_1;
                    let _e127 = c_1;
                    let _e129 = momentSquares;
                    momentSquares = (_e129 + (_e126 * _e127));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e131 = x_1;
                    x_1 = (_e131 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e133 = y_1;
            y_1 = (_e133 + 1i);
        }
    }
    let _e135 = neighborhoodMin;
    param_24 = _e135;
    let _e136 = finite3_u0028_vf3_u003b((&param_24));
    phi_1462_ = _e136;
    if _e136 {
        let _e137 = neighborhoodMax;
        param_25 = _e137;
        let _e138 = finite3_u0028_vf3_u003b((&param_25));
        phi_1462_ = _e138;
    }
    let _e140 = phi_1462_;
    if !(_e140) {
        return false;
    }
    let _e142 = neighborhoodMin;
    (*boxMin) = _e142;
    let _e143 = neighborhoodMax;
    (*boxMax) = _e143;
    let _e145 = pc.qualityFlags;
    if ((_e145 & 1u) != 0u) {
        let _e148 = momentSum;
        momentMean = (_e148 * 0.11111111f);
        let _e150 = momentSquares;
        let _e152 = momentMean;
        let _e153 = momentMean;
        momentSigma = sqrt(max(((_e150 * 0.11111111f) - (_e152 * _e153)), vec3<f32>(0f, 0f, 0f)));
        let _e158 = neighborhoodMin;
        let _e159 = momentMean;
        let _e160 = momentSigma;
        (*boxMin) = max(_e158, (_e159 - (_e160 * 1.25f)));
        let _e164 = neighborhoodMax;
        let _e165 = momentMean;
        let _e166 = momentSigma;
        (*boxMax) = min(_e164, (_e165 + (_e166 * 1.25f)));
    }
    return true;
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_2: ptr<function, f32>, y_2: ptr<function, f32>, a_1: ptr<function, f32>) -> f32 {
    let _e93 = (*x_2);
    let _e94 = (*a_1);
    let _e97 = (*y_2);
    let _e98 = (*a_1);
    return ((_e93 * (1f - _e94)) + (_e97 * _e98));
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

    let _e110 = (*texel_2);
    baseFloat_1 = floor(_e110);
    let _e112 = baseFloat_1;
    base_1 = vec2<i32>(_e112);
    let _e114 = (*texel_2);
    let _e115 = baseFloat_1;
    f_1 = (_e114 - _e115);
    let _e118 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e118) - vec2<i32>(1i, 1i));
    let _e121 = base_1;
    let _e122 = hi;
    let _e124 = textureLoad(previousLinearDepth, clamp(_e121, vec2<i32>(0i, 0i), _e122), 0i);
    d00_ = _e124.x;
    let _e126 = base_1;
    let _e128 = hi;
    let _e130 = textureLoad(previousLinearDepth, clamp((_e126 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e128), 0i);
    d10_ = _e130.x;
    let _e132 = base_1;
    let _e134 = hi;
    let _e136 = textureLoad(previousLinearDepth, clamp((_e132 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e134), 0i);
    d01_ = _e136.x;
    let _e138 = base_1;
    let _e140 = hi;
    let _e142 = textureLoad(previousLinearDepth, clamp((_e138 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e140), 0i);
    d11_ = _e142.x;
    let _e144 = d00_;
    param_26 = _e144;
    let _e145 = d10_;
    param_27 = _e145;
    let _e147 = f_1[0u];
    param_28 = _e147;
    let _e148 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_26), (&param_27), (&param_28));
    top = _e148;
    let _e149 = d01_;
    param_29 = _e149;
    let _e150 = d11_;
    param_30 = _e150;
    let _e152 = f_1[0u];
    param_31 = _e152;
    let _e153 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_29), (&param_30), (&param_31));
    bottom = _e153;
    let _e154 = top;
    param_32 = _e154;
    let _e155 = bottom;
    param_33 = _e155;
    let _e157 = f_1[1u];
    param_34 = _e157;
    let _e158 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_32), (&param_33), (&param_34));
    return _e158;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator_1: f32;
    var numerator: f32;
    var result_1: f32;

    let _e97 = pc.zFar;
    let _e99 = pc.zNear;
    range = (_e97 - _e99);
    let _e101 = (*z_1);
    let _e102 = range;
    scaled_1 = (_e101 * _e102);
    let _e105 = pc.zNear;
    let _e106 = scaled_1;
    denominator_1 = (_e105 + _e106);
    let _e109 = pc.zNear;
    let _e111 = pc.zFar;
    numerator = (_e109 * _e111);
    let _e113 = numerator;
    let _e114 = denominator_1;
    result_1 = (_e113 / _e114);
    let _e116 = result_1;
    return _e116;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_157_: bool;

    let _e91 = (*v_2);
    let _e92 = (*v_2);
    let _e94 = all((_e91 == _e92));
    phi_157_ = _e94;
    if _e94 {
        let _e95 = (*v_2);
        phi_157_ = all((abs(_e95) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e100 = phi_157_;
    return _e100;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_187_: bool;

    let _e91 = (*v_3);
    let _e92 = (*v_3);
    let _e94 = all((_e91 == _e92));
    phi_187_ = _e94;
    if _e94 {
        let _e95 = (*v_3);
        phi_187_ = all((abs(_e95) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e100 = phi_187_;
    return _e100;
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

    let _e104 = (*px_1);
    let _e105 = textureLoad(motionVelocity, _e104, 0i);
    centerVelocity = _e105.xy;
    let _e108 = pc.qualityFlags;
    let _e110 = ((_e108 & 2u) == 0u);
    phi_1254_ = _e110;
    if !(_e110) {
        let _e112 = (*centerValid);
        phi_1254_ = !((_e112 > 0f));
    }
    let _e116 = phi_1254_;
    phi_1262_ = _e116;
    if !(_e116) {
        let _e118 = centerVelocity;
        param_35 = _e118;
        let _e119 = finite2_u0028_vf2_u003b((&param_35));
        phi_1262_ = !(_e119);
    }
    let _e122 = phi_1262_;
    if _e122 {
        let _e123 = centerVelocity;
        return _e123;
    }
    let _e125 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e125) - vec2<i32>(1i, 1i));
    let _e128 = (*px_1);
    best = _e128;
    let _e129 = (*tileIndex_1);
    let _e131 = tileDepth[_e129];
    bestDepth = _e131;
    y_3 = -1i;
    loop {
        let _e132 = y_3;
        if (_e132 <= 1i) {
            x_3 = -1i;
            loop {
                let _e134 = x_3;
                if (_e134 <= 1i) {
                    let _e136 = (*tileIndex_1);
                    let _e137 = y_3;
                    let _e140 = x_3;
                    let _e143 = tileDepth[((_e136 + (_e137 * 10i)) + _e140)];
                    d = _e143;
                    let _e144 = d;
                    let _e145 = bestDepth;
                    if (_e144 > _e145) {
                        let _e147 = d;
                        bestDepth = _e147;
                        let _e148 = (*px_1);
                        let _e149 = x_3;
                        let _e150 = y_3;
                        let _e153 = hi_1;
                        best = clamp((_e148 + vec2<i32>(_e149, _e150)), vec2<i32>(0i, 0i), _e153);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e155 = x_3;
                    x_3 = (_e155 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e157 = y_3;
            y_3 = (_e157 + 1i);
        }
    }
    let _e159 = best;
    let _e160 = (*px_1);
    if all((_e159 == _e160)) {
        let _e163 = centerVelocity;
        return _e163;
    }
    let _e164 = best;
    let _e165 = textureLoad(motionValidity, _e164, 0i);
    v_4 = _e165.x;
    let _e167 = v_4;
    if !((_e167 > 0f)) {
        let _e170 = centerVelocity;
        return _e170;
    }
    let _e171 = best;
    let _e172 = textureLoad(motionVelocity, _e171, 0i);
    dv = _e172.xy;
    let _e174 = dv;
    param_36 = _e174;
    let _e175 = finite2_u0028_vf2_u003b((&param_36));
    let _e176 = dv;
    let _e177 = centerVelocity;
    return select(_e177, _e176, vec2(_e175));
}

fn reconstructCurrent_u0028_vi2_u003b_i1_u003b(pixel_1: ptr<function, vec2<i32>>, tileIndex_2: ptr<function, i32>) -> vec4<f32> {
    var extent_2: vec2<f32>;
    var sampleAt: vec2<f32>;

    let _e95 = pc.extent;
    extent_2 = vec2<f32>(_e95);
    let _e97 = (*pixel_1);
    let _e101 = extent_2;
    let _e102 = extent_2;
    let _e107 = pc.currentEffectiveJitterUv;
    sampleAt = (((vec2<f32>(_e97) + vec2(0.5f)) * inverseSqrt((_e101 * _e102))) + _e107);
    let _e109 = sampleAt;
    let _e110 = textureSampleLevel(currentColor, nearestSampler, _e109, 0f);
    let _e111 = _e110.xyz;
    let _e112 = (*tileIndex_2);
    let _e115 = tileColor[_e112][3u];
    return vec4<f32>(_e111.x, _e111.y, _e111.z, _e115);
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
    var phi_1942_: bool;
    var phi_1950_: bool;
    var phi_1973_: bool;

    let _e178 = gl_GlobalInvocationID_1;
    pixel_2 = bitcast<vec2<i32>>(_e178.xy);
    let _e181 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e181.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e188 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e188) - vec2<i32>(1i, 1i));
    let _e191 = gl_LocalInvocationIndex_1;
    i_3 = _e191;
    loop {
        let _e192 = i_3;
        if (_e192 < 100u) {
            let _e194 = tileOrigin;
            let _e195 = i_3;
            let _e198 = i_3;
            let _e203 = imageHi;
            q = clamp((_e194 + vec2<i32>(bitcast<i32>((_e195 % 10u)), bitcast<i32>((_e198 / 10u)))), vec2<i32>(0i, 0i), _e203);
            let _e205 = i_3;
            let _e206 = q;
            let _e207 = textureLoad(currentColor, _e206, 0i);
            tileColor[_e205] = _e207;
            let _e209 = i_3;
            let _e210 = q;
            let _e211 = textureLoad(currentDeviceDepth, _e210, 0i);
            tileDepth[_e209] = _e211.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e214 = i_3;
            i_3 = (_e214 + 64u);
        }
    }
    workgroupBarrier();
    let _e216 = pixel_2;
    let _e218 = pc.extent;
    if any((_e216 >= bitcast<vec2<i32>>(_e218))) {
        return;
    }
    let _e223 = gl_LocalInvocationID_1[1u];
    let _e228 = gl_LocalInvocationID_1[0u];
    tileIndex_3 = ((((bitcast<i32>(_e223) + 1i) * 10i) + bitcast<i32>(_e228)) + 1i);
    clipMin = vec3<f32>(0f, 0f, 0f);
    clipMax = vec3<f32>(0f, 0f, 0f);
    let _e232 = pixel_2;
    param_37 = _e232;
    let _e233 = tileIndex_3;
    param_38 = _e233;
    let _e234 = reconstructCurrent_u0028_vi2_u003b_i1_u003b((&param_37), (&param_38));
    current = _e234;
    let _e235 = current;
    result_2 = _e235;
    let _e236 = current;
    displayed = _e236;
    usedHistory = false;
    historyCount = 1f;
    let _e237 = pixel_2;
    let _e238 = textureLoad(motionValidity, _e237, 0i);
    valid = _e238.x;
    let _e240 = pixel_2;
    param_39 = _e240;
    let _e241 = tileIndex_3;
    param_40 = _e241;
    let _e242 = valid;
    param_41 = _e242;
    let _e243 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_39), (&param_40), (&param_41));
    velocity = _e243;
    let _e244 = velocity;
    let _e246 = pc.extent;
    motionTexel = (_e244 * vec2<f32>(_e246));
    let _e249 = pixel_2;
    let _e251 = motionTexel;
    previousTexel = (vec2<f32>(_e249) - _e251);
    let _e253 = previousTexel;
    previousCenter = (_e253 + vec2(0.5f));
    let _e256 = previousCenter;
    let _e258 = all((_e256 >= vec2<f32>(0f, 0f)));
    phi_1651_ = _e258;
    if _e258 {
        let _e259 = previousCenter;
        let _e261 = pc.extent;
        phi_1651_ = all((_e259 < vec2<f32>(_e261)));
    }
    let _e266 = phi_1651_;
    inside = _e266;
    let _e267 = current;
    param_42 = _e267;
    let _e268 = finite4_u0028_vf4_u003b((&param_42));
    phi_1660_ = _e268;
    if _e268 {
        let _e269 = valid;
        param_43 = _e269;
        let _e270 = finite1_u0028_f1_u003b((&param_43));
        phi_1660_ = _e270;
    }
    let _e272 = phi_1660_;
    let _e273 = valid;
    let _e275 = (_e272 && (_e273 > 0f));
    phi_1669_ = _e275;
    if _e275 {
        let _e276 = velocity;
        param_44 = _e276;
        let _e277 = finite2_u0028_vf2_u003b((&param_44));
        phi_1669_ = _e277;
    }
    let _e279 = phi_1669_;
    phi_1675_ = _e279;
    if _e279 {
        let _e280 = previousTexel;
        param_45 = _e280;
        let _e281 = finite2_u0028_vf2_u003b((&param_45));
        phi_1675_ = _e281;
    }
    let _e283 = phi_1675_;
    let _e284 = inside;
    if (_e283 && _e284) {
        let _e286 = tileIndex_3;
        let _e288 = tileDepth[_e286];
        currentDevice = _e288;
        let _e289 = currentDevice;
        deviceLo = _e289;
        let _e290 = currentDevice;
        deviceHi = _e290;
        y_4 = -1i;
        loop {
            let _e291 = y_4;
            if (_e291 <= 1i) {
                x_4 = -1i;
                loop {
                    let _e293 = x_4;
                    if (_e293 <= 1i) {
                        let _e295 = tileIndex_3;
                        let _e296 = y_4;
                        let _e299 = x_4;
                        let _e302 = tileDepth[((_e295 + (_e296 * 10i)) + _e299)];
                        d_1 = _e302;
                        let _e303 = deviceLo;
                        let _e304 = d_1;
                        deviceLo = min(_e303, _e304);
                        let _e306 = deviceHi;
                        let _e307 = d_1;
                        deviceHi = max(_e306, _e307);
                        continue;
                    } else {
                        break;
                    }
                    continuing {
                        let _e309 = x_4;
                        x_4 = (_e309 + 1i);
                    }
                }
                continue;
            } else {
                break;
            }
            continuing {
                let _e311 = y_4;
                y_4 = (_e311 + 1i);
            }
        }
        let _e313 = currentDevice;
        param_46 = _e313;
        let _e314 = linearizeDepth_u0028_f1_u003b((&param_46));
        currentLinear = _e314;
        let _e315 = deviceLo;
        param_47 = _e315;
        let _e316 = linearizeDepth_u0028_f1_u003b((&param_47));
        rangeA = _e316;
        let _e317 = deviceHi;
        param_48 = _e317;
        let _e318 = linearizeDepth_u0028_f1_u003b((&param_48));
        rangeB = _e318;
        let _e319 = previousTexel;
        param_49 = _e319;
        let _e320 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_49));
        priorLinear = _e320;
        let _e322 = pc.depthThresholdAbsolute;
        let _e324 = pc.depthThresholdRelative;
        let _e325 = currentLinear;
        let _e326 = priorLinear;
        depthLimit = max(_e322, (_e324 * max(_e325, _e326)));
        let _e330 = currentLinear;
        param_50 = _e330;
        let _e331 = finite1_u0028_f1_u003b((&param_50));
        phi_1758_ = _e331;
        if _e331 {
            let _e332 = priorLinear;
            param_51 = _e332;
            let _e333 = finite1_u0028_f1_u003b((&param_51));
            phi_1758_ = _e333;
        }
        let _e335 = phi_1758_;
        phi_1764_ = _e335;
        if _e335 {
            let _e336 = rangeA;
            param_52 = _e336;
            let _e337 = finite1_u0028_f1_u003b((&param_52));
            phi_1764_ = _e337;
        }
        let _e339 = phi_1764_;
        phi_1770_ = _e339;
        if _e339 {
            let _e340 = rangeB;
            param_53 = _e340;
            let _e341 = finite1_u0028_f1_u003b((&param_53));
            phi_1770_ = _e341;
        }
        let _e343 = phi_1770_;
        let _e344 = currentLinear;
        let _e347 = priorLinear;
        let _e349 = ((_e343 && (_e344 > 0f)) && (_e347 > 0f));
        phi_1786_ = _e349;
        if _e349 {
            let _e350 = priorLinear;
            let _e351 = rangeA;
            let _e352 = rangeB;
            let _e354 = depthLimit;
            phi_1786_ = (_e350 >= (min(_e351, _e352) - _e354));
        }
        let _e358 = phi_1786_;
        phi_1796_ = _e358;
        if _e358 {
            let _e359 = priorLinear;
            let _e360 = rangeA;
            let _e361 = rangeB;
            let _e363 = depthLimit;
            phi_1796_ = (_e359 <= (max(_e360, _e361) + _e363));
        }
        let _e367 = phi_1796_;
        if _e367 {
            let _e368 = tileIndex_3;
            param_54 = _e368;
            let _e369 = neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b((&param_54), (&param_55), (&param_56));
            let _e370 = param_55;
            clipMin = _e370;
            let _e371 = param_56;
            clipMax = _e371;
            boxValid = _e369;
            tapMin_2 = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
            tapMax_2 = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
            centreCount_1 = 0x1.fffffep+127f;
            let _e372 = previousTexel;
            param_57 = _e372;
            let _e373 = tapMin_2;
            param_58 = _e373;
            let _e374 = tapMax_2;
            param_59 = _e374;
            let _e375 = centreCount_1;
            param_60 = _e375;
            let _e376 = samplePreviousColor_u0028_vf2_u003b_vf3_u003b_vf3_u003b_f1_u003b((&param_57), (&param_58), (&param_59), (&param_60));
            let _e377 = param_58;
            tapMin_2 = _e377;
            let _e378 = param_59;
            tapMax_2 = _e378;
            let _e379 = param_60;
            centreCount_1 = _e379;
            prior = _e376;
            let _e380 = boxValid;
            phi_1829_ = _e380;
            if _e380 {
                let _e381 = prior;
                param_61 = _e381;
                let _e382 = finite4_u0028_vf4_u003b((&param_61));
                phi_1829_ = _e382;
            }
            let _e384 = phi_1829_;
            if _e384 {
                let _e385 = prior;
                let _e387 = clipMin;
                let _e388 = clipMax;
                clampedPrior = clamp(_e385.xyz, _e387, _e388);
                let _e390 = valid;
                confidence_1 = clamp(_e390, 0f, 1f);
                let _e393 = pc.historyWeight;
                let _e395 = centreCount_1;
                param_62 = _e395;
                param_63 = clamp(_e393, 0f, 1f);
                let _e396 = motionTexel;
                param_64 = _e396;
                let _e397 = accumulatedHistoryCount_u0028_f1_u003b_f1_u003b_vf2_u003b((&param_62), (&param_63), (&param_64));
                priorCount = _e397;
                let _e398 = prior;
                let _e400 = clampedPrior;
                let _e402 = clipMax;
                let _e403 = clipMin;
                let _e405 = priorCount;
                param_65 = _e405;
                param_66 = (_e398.xyz - _e400);
                param_67 = (_e402 - _e403);
                let _e406 = rectifiedHistoryCount_u0028_f1_u003b_vf3_u003b_vf3_u003b((&param_65), (&param_66), (&param_67));
                priorCount = _e406;
                let _e407 = priorCount;
                param_68 = _e407;
                let _e408 = historyWeightForCount_u0028_f1_u003b((&param_68));
                baseWeight = _e408;
                let _e409 = baseWeight;
                param_69 = _e409;
                let _e410 = confidence_1;
                param_70 = _e410;
                let _e411 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_69), (&param_70));
                weight_1 = _e411;
                let _e412 = current;
                param_71 = _e412.xyz;
                let _e414 = clampedPrior;
                param_72 = _e414;
                let _e415 = weight_1;
                param_73 = _e415;
                let _e416 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_71), (&param_72), (&param_73));
                resolvedRgb = _e416;
                let _e417 = resolvedRgb;
                let _e419 = current[3u];
                result_2 = vec4<f32>(_e417.x, _e417.y, _e417.z, _e419);
                let _e424 = result_2;
                displayed = _e424;
                let _e425 = priorCount;
                let _e426 = confidence_1;
                historyCount = min(((_e425 * _e426) + 1f), 24f);
                let _e431 = pc.qualityFlags;
                if ((_e431 & 8u) == 0u) {
                    let _e434 = prior;
                    let _e436 = previousTexel;
                    param_74 = _e436;
                    let _e437 = tapMin_2;
                    param_75 = _e437;
                    let _e438 = tapMax_2;
                    param_76 = _e438;
                    let _e439 = samplePreviousBlur_u0028_vf2_u003b_vf3_u003b_vf3_u003b((&param_74), (&param_75), (&param_76));
                    let _e440 = param_75;
                    tapMin_2 = _e440;
                    let _e441 = param_76;
                    tapMax_2 = _e441;
                    detail = (_e434.xyz - _e439);
                    let _e443 = resolvedRgb;
                    let _e444 = confidence_1;
                    let _e446 = detail;
                    let _e449 = resolvedRgb;
                    let _e450 = tapMin_2;
                    let _e452 = resolvedRgb;
                    let _e453 = tapMax_2;
                    let _e455 = clamp((_e443 + (_e446 * (2f * _e444))), min(_e449, _e450), max(_e452, _e453));
                    let _e457 = current[3u];
                    displayed = vec4<f32>(_e455.x, _e455.y, _e455.z, _e457);
                }
                usedHistory = true;
            }
        }
    }
    let _e462 = usedHistory;
    let _e463 = !(_e462);
    phi_1942_ = _e463;
    if _e463 {
        let _e464 = current;
        param_77 = _e464;
        let _e465 = finite4_u0028_vf4_u003b((&param_77));
        phi_1942_ = _e465;
    }
    let _e467 = phi_1942_;
    phi_1950_ = _e467;
    if _e467 {
        let _e469 = pc.qualityFlags;
        phi_1950_ = ((_e469 & 4u) != 0u);
    }
    let _e473 = phi_1950_;
    if _e473 {
        let _e474 = pixel_2;
        param_78 = _e474;
        let _e475 = current;
        param_79 = _e475;
        let _e476 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_78), (&param_79));
        result_2 = _e476;
    }
    let _e477 = result_2;
    param_80 = _e477;
    let _e478 = finite4_u0028_vf4_u003b((&param_80));
    if !(_e478) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e480 = usedHistory;
    let _e481 = !(_e480);
    phi_1973_ = _e481;
    if !(_e481) {
        let _e483 = displayed;
        param_81 = _e483;
        let _e484 = finite4_u0028_vf4_u003b((&param_81));
        phi_1973_ = !(_e484);
    }
    let _e487 = phi_1973_;
    if _e487 {
        let _e488 = result_2;
        displayed = _e488;
    }
    let _e489 = pixel_2;
    let _e490 = displayed;
    textureStore(resolvedColor, _e489, _e490);
    let _e491 = pixel_2;
    let _e492 = result_2;
    let _e493 = _e492.xyz;
    let _e494 = historyCount;
    textureStore(historyOutColor, _e491, vec4<f32>(_e493.x, _e493.y, _e493.z, _e494));
    let _e499 = pixel_2;
    let _e500 = tileIndex_3;
    let _e502 = tileDepth[_e500];
    param_82 = _e502;
    let _e503 = storeLinearDepth_u0028_f1_u003b((&param_82));
    textureStore(historyOutLinearDepth, _e499, vec4(_e503));
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
