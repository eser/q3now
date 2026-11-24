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

    let _e89 = pc.zNear;
    let _e90 = (*z);
    let _e92 = pc.zFar;
    let _e94 = pc.zNear;
    denominator = (_e89 + (_e90 * (_e92 - _e94)));
    let _e99 = pc.zNear;
    let _e101 = pc.zFar;
    let _e103 = denominator;
    let _e104 = denominator;
    return ((_e99 * _e101) * inverseSqrt((_e103 * _e104)));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e88 = (*pixel);
    let _e90 = pc.extent;
    bounded = clamp(_e88, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e90) - vec2<i32>(1i, 1i)));
    let _e94 = bounded;
    let _e95 = textureLoad(currentColor, _e94, 0i);
    return _e95;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_123_: bool;

    let _e87 = (*v);
    let _e88 = (*v);
    let _e89 = (_e87 == _e88);
    phi_123_ = _e89;
    if _e89 {
        let _e90 = (*v);
        phi_123_ = (abs(_e90) <= 0x1.fffffep+127f);
    }
    let _e94 = phi_123_;
    return _e94;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e89 = (*c);
    l = dot(_e89, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e91 = l;
    param = _e91;
    let _e92 = finite1_u0028_f1_u003b((&param));
    if !(_e92) {
        return 0f;
    }
    let _e94 = l;
    l = clamp(_e94, 0f, 10000f);
    let _e96 = l;
    let _e97 = l;
    let _e98 = l;
    return (_e96 * inverseSqrt((1f + (_e97 * _e98))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e89 = (*p);
    param_1 = _e89;
    let _e90 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e90.xyz;
    let _e92 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e92;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e90 = (*base);
    param_3 = _e90;
    let _e91 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e92 = (*base);
    let _e93 = (*across);
    param_4 = (_e92 + _e93);
    let _e95 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e91 + _e95));
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

    let _e159 = (*centerColor);
    param_5 = _e159.xyz;
    let _e161 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e161;
    let _e162 = (*px);
    param_6 = (_e162 + vec2<i32>(0i, -1i));
    let _e164 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e164;
    let _e165 = (*px);
    param_7 = (_e165 + vec2<i32>(0i, 1i));
    let _e167 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e167;
    let _e168 = (*px);
    param_8 = (_e168 + vec2<i32>(-1i, 0i));
    let _e170 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e170;
    let _e171 = (*px);
    param_9 = (_e171 + vec2<i32>(1i, 0i));
    let _e173 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e173;
    let _e174 = fxN;
    let _e175 = fxS;
    let _e177 = fxW;
    let _e178 = fxE;
    let _e180 = fxM;
    fxHi = max(max(_e174, _e175), max(max(_e177, _e178), _e180));
    let _e183 = fxN;
    let _e184 = fxS;
    let _e186 = fxW;
    let _e187 = fxE;
    let _e189 = fxM;
    fxLo = min(min(_e183, _e184), min(min(_e186, _e187), _e189));
    let _e192 = fxHi;
    let _e193 = fxLo;
    fxSpan = (_e192 - _e193);
    let _e195 = fxSpan;
    let _e196 = fxHi;
    if (_e195 < max(0.0312f, (_e196 * 0.125f))) {
        let _e200 = (*centerColor);
        return _e200;
    }
    let _e201 = (*px);
    param_10 = (_e201 + vec2<i32>(-1i, -1i));
    let _e203 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e203;
    let _e204 = (*px);
    param_11 = (_e204 + vec2<i32>(1i, -1i));
    let _e206 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e206;
    let _e207 = (*px);
    param_12 = (_e207 + vec2<i32>(-1i, 1i));
    let _e209 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e209;
    let _e210 = (*px);
    param_13 = (_e210 + vec2<i32>(1i, 1i));
    let _e212 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e212;
    let _e213 = fxN;
    let _e214 = fxS;
    fxNS = (_e213 + _e214);
    let _e216 = fxW;
    let _e217 = fxE;
    fxWE = (_e216 + _e217);
    let _e219 = fxNW;
    let _e220 = fxSW;
    fxNWSW = (_e219 + _e220);
    let _e222 = fxNE;
    let _e223 = fxSE;
    fxNESE = (_e222 + _e223);
    let _e225 = fxW;
    let _e227 = fxNWSW;
    let _e230 = fxM;
    let _e232 = fxNS;
    let _e236 = fxE;
    let _e238 = fxNESE;
    fxHorz = (abs(((-2f * _e225) + _e227)) + ((abs(((-2f * _e230) + _e232)) * 2f) + abs(((-2f * _e236) + _e238))));
    let _e243 = fxS;
    let _e245 = fxSW;
    let _e246 = fxSE;
    let _e250 = fxM;
    let _e252 = fxWE;
    let _e256 = fxN;
    let _e258 = fxNW;
    let _e259 = fxNE;
    fxVert = (abs(((-2f * _e243) + (_e245 + _e246))) + ((abs(((-2f * _e250) + _e252)) * 2f) + abs(((-2f * _e256) + (_e258 + _e259)))));
    let _e265 = fxHorz;
    let _e266 = fxVert;
    fxHorzSpan = (_e265 >= _e266);
    let _e268 = fxNS;
    let _e269 = fxWE;
    let _e272 = fxNWSW;
    let _e273 = fxNESE;
    fxAround = (((_e268 + _e269) * 2f) + (_e272 + _e273));
    let _e276 = fxHorzSpan;
    let _e277 = fxN;
    let _e278 = fxW;
    fxLoSide = select(_e278, _e277, _e276);
    let _e280 = fxHorzSpan;
    let _e281 = fxS;
    let _e282 = fxE;
    fxHiSide = select(_e282, _e281, _e280);
    let _e284 = fxLoSide;
    let _e285 = fxM;
    fxGradLo = (_e284 - _e285);
    let _e287 = fxHiSide;
    let _e288 = fxM;
    fxGradHi = (_e287 - _e288);
    let _e290 = fxGradLo;
    let _e292 = fxGradHi;
    fxLoSteeper = (abs(_e290) >= abs(_e292));
    let _e295 = fxGradLo;
    let _e297 = fxGradHi;
    fxGradient = max(abs(_e295), abs(_e297));
    let _e300 = fxLoSteeper;
    if _e300 {
        let _e301 = fxLoSide;
        let _e302 = fxM;
        local = (_e301 + _e302);
    } else {
        let _e304 = fxHiSide;
        let _e305 = fxM;
        local = (_e304 + _e305);
    }
    let _e307 = local;
    fxPair = _e307;
    let _e308 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e308));
    let _e311 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e311));
    let _e314 = fxLoSteeper;
    if _e314 {
        let _e315 = fxAcross;
        fxAcross = -(_e315);
    }
    let _e317 = fxSpan;
    let _e318 = fxSpan;
    fxInvSpan = inverseSqrt((_e317 * _e318));
    let _e321 = fxAround;
    let _e323 = fxM;
    let _e326 = fxInvSpan;
    fxBlendC = clamp((abs(((_e321 * 0.083333336f) - _e323)) * _e326), 0f, 1f);
    let _e329 = fxBlendC;
    let _e332 = fxBlendC;
    let _e333 = fxBlendC;
    fxBlendF = (((-2f * _e329) + 3f) * (_e332 * _e333));
    let _e336 = fxBlendF;
    let _e337 = fxBlendF;
    fxSubpixel = ((_e336 * _e337) * 0.75f);
    let _e340 = fxPair;
    fxHalfPair = (_e340 * 0.5f);
    let _e342 = fxGradient;
    fxScaled = (_e342 * 0.25f);
    let _e344 = fxM;
    let _e345 = fxHalfPair;
    fxMidNegative = ((_e344 - _e345) < 0f);
    let _e348 = (*px);
    let _e349 = fxAlong;
    param_14 = (_e348 - _e349);
    let _e351 = fxAcross;
    param_15 = _e351;
    let _e352 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e353 = fxHalfPair;
    fxEndN = (_e352 - _e353);
    let _e355 = (*px);
    let _e356 = fxAlong;
    param_16 = (_e355 + _e356);
    let _e358 = fxAcross;
    param_17 = _e358;
    let _e359 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e360 = fxHalfPair;
    fxEndP = (_e359 - _e360);
    let _e362 = fxEndN;
    let _e364 = fxScaled;
    fxDoneN = (abs(_e362) >= _e364);
    let _e366 = fxEndP;
    let _e368 = fxScaled;
    fxDoneP = (abs(_e366) >= _e368);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e370 = i;
        if (_e370 <= 12i) {
            let _e372 = fxDoneN;
            if !(_e372) {
                let _e374 = (*px);
                let _e375 = fxAlong;
                let _e376 = i;
                param_18 = (_e374 - (_e375 * vec2(_e376)));
                let _e380 = fxAcross;
                param_19 = _e380;
                let _e381 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e382 = fxHalfPair;
                fxEndN = (_e381 - _e382);
                let _e384 = fxEndN;
                let _e386 = fxScaled;
                fxDoneN = (abs(_e384) >= _e386);
                let _e388 = i;
                fxStepsN = _e388;
            }
            let _e389 = fxDoneP;
            if !(_e389) {
                let _e391 = (*px);
                let _e392 = fxAlong;
                let _e393 = i;
                param_20 = (_e391 + (_e392 * vec2(_e393)));
                let _e397 = fxAcross;
                param_21 = _e397;
                let _e398 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e399 = fxHalfPair;
                fxEndP = (_e398 - _e399);
                let _e401 = fxEndP;
                let _e403 = fxScaled;
                fxDoneP = (abs(_e401) >= _e403);
                let _e405 = i;
                fxStepsP = _e405;
            }
            let _e406 = fxDoneN;
            let _e407 = fxDoneP;
            if (_e406 && _e407) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e409 = i;
            i = (_e409 + 1i);
        }
    }
    let _e411 = fxStepsN;
    let _e412 = fxStepsP;
    fxTowardN = (_e411 < _e412);
    let _e414 = fxTowardN;
    let _e415 = fxEndN;
    let _e416 = fxEndP;
    fxEndG = select(_e416, _e415, _e414);
    let _e418 = fxEndG;
    let _e420 = fxMidNegative;
    fxGoodSpan = ((_e418 < 0f) != _e420);
    let _e422 = fxStepsN;
    let _e423 = fxStepsP;
    fxSpanLen = f32((_e422 + _e423));
    let _e426 = fxTowardN;
    let _e427 = fxStepsN;
    let _e428 = fxStepsP;
    fxNear = f32(select(_e428, _e427, _e426));
    let _e431 = fxGoodSpan;
    if _e431 {
        let _e432 = fxNear;
        let _e433 = fxSpanLen;
        let _e434 = fxSpanLen;
        local_1 = (0.5f - (_e432 * inverseSqrt((_e433 * _e434))));
    } else {
        local_1 = 0f;
    }
    let _e439 = local_1;
    fxEdgeBlend = _e439;
    let _e440 = fxEdgeBlend;
    let _e441 = fxSubpixel;
    fxBlend = clamp(max(_e440, _e441), 0f, 1f);
    let _e444 = (*px);
    let _e445 = fxAcross;
    param_22 = (_e444 + _e445);
    let _e447 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e447.xyz;
    let _e449 = (*centerColor);
    let _e451 = fxBlend;
    let _e454 = fxAcrossColor;
    let _e455 = fxBlend;
    let _e457 = ((_e449.xyz * (1f - _e451)) + (_e454 * _e455));
    let _e459 = (*centerColor)[3u];
    return vec4<f32>(_e457.x, _e457.y, _e457.z, _e459);
}

fn samplePreviousBlur_u0028_vf2_u003b_vf3_u003b_vf3_u003b(texel: ptr<function, vec2<f32>>, tapMin: ptr<function, vec3<f32>>, tapMax: ptr<function, vec3<f32>>) -> vec3<f32> {
    var extent: vec2<f32>;
    var invExtent: vec2<f32>;
    var sum: vec3<f32>;
    var i_1: i32;
    var o: vec2<f32>;
    var tap: vec3<f32>;

    let _e96 = pc.extent;
    extent = vec2<f32>(_e96);
    let _e98 = extent;
    let _e99 = extent;
    invExtent = inverseSqrt((_e98 * _e99));
    sum = vec3<f32>(0f, 0f, 0f);
    i_1 = 0i;
    loop {
        let _e102 = i_1;
        if (_e102 < 4i) {
            let _e104 = i_1;
            let _e108 = i_1;
            o = vec2<f32>(select(-0.5f, 0.5f, ((_e104 & 1i) != 0i)), select(-0.5f, 0.5f, ((_e108 & 2i) != 0i)));
            let _e113 = (*texel);
            let _e116 = o;
            let _e118 = invExtent;
            let _e120 = textureSampleLevel(previousColor, nearestSampler, (((_e113 + vec2(0.5f)) + _e116) * _e118), 0f);
            tap = _e120.xyz;
            let _e122 = tap;
            let _e123 = sum;
            sum = (_e123 + _e122);
            let _e125 = (*tapMin);
            let _e126 = tap;
            (*tapMin) = min(_e125, _e126);
            let _e128 = (*tapMax);
            let _e129 = tap;
            (*tapMax) = max(_e128, _e129);
            continue;
        } else {
            break;
        }
        continuing {
            let _e131 = i_1;
            i_1 = (_e131 + 1i);
        }
    }
    let _e133 = sum;
    return (_e133 * 0.25f);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e93 = (*a);
    oneMinus = (1f - _e93);
    let _e95 = (*x);
    let _e96 = oneMinus;
    left = (_e95 * _e96);
    let _e98 = (*y);
    let _e99 = (*a);
    right = (_e98 * _e99);
    let _e101 = left;
    let _e102 = right;
    result = (_e101 + _e102);
    let _e104 = result;
    return _e104;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e89 = (*weight);
    let _e90 = (*confidence);
    scaled = (_e89 * _e90);
    let _e92 = scaled;
    return _e92;
}

fn samplePreviousColor_u0028_vf2_u003b_vf3_u003b_vf3_u003b(texel_1: ptr<function, vec2<f32>>, tapMin_1: ptr<function, vec3<f32>>, tapMax_1: ptr<function, vec3<f32>>) -> vec4<f32> {
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

    let _e113 = (*texel_1);
    baseFloat = floor(_e113);
    let _e115 = (*texel_1);
    let _e116 = baseFloat;
    f = (_e115 - _e116);
    let _e118 = f;
    d0_ = (vec2(1f) + _e118);
    let _e121 = f;
    d1_ = _e121;
    let _e122 = f;
    d2_ = (vec2(1f) - _e122);
    let _e125 = f;
    d3_ = (vec2(2f) - _e125);
    let _e128 = d0_;
    let _e132 = d0_;
    let _e136 = d0_;
    w0_ = ((((((_e128 * -0.75f) - vec2(-3.75f)) * _e132) + vec2(-6f)) * _e136) - vec2(-3f));
    let _e140 = d1_;
    let _e144 = d1_;
    let _e146 = d1_;
    w1_ = (((((_e140 * 1.25f) - vec2(2.25f)) * _e144) * _e146) + vec2(1f));
    let _e150 = d2_;
    let _e154 = d2_;
    let _e156 = d2_;
    w2_ = (((((_e150 * 1.25f) - vec2(2.25f)) * _e154) * _e156) + vec2(1f));
    let _e160 = d3_;
    let _e164 = d3_;
    let _e168 = d3_;
    w3_ = ((((((_e160 * -0.75f) - vec2(-3.75f)) * _e164) + vec2(-6f)) * _e168) - vec2(-3f));
    let _e172 = w1_;
    let _e173 = w2_;
    w12_ = (_e172 + _e173);
    let _e175 = w2_;
    let _e176 = w12_;
    let _e177 = w12_;
    offset12_ = (_e175 * inverseSqrt((_e176 * _e177)));
    let _e182 = pc.extent;
    extent_1 = vec2<f32>(_e182);
    let _e184 = extent_1;
    let _e185 = extent_1;
    invExtent_1 = inverseSqrt((_e184 * _e185));
    let _e188 = baseFloat;
    let _e193 = invExtent_1;
    uv0_ = (((_e188 - vec2(1f)) + vec2(0.5f)) * _e193);
    let _e195 = baseFloat;
    let _e196 = offset12_;
    let _e200 = invExtent_1;
    uv12_ = (((_e195 + _e196) + vec2(0.5f)) * _e200);
    let _e202 = baseFloat;
    let _e207 = invExtent_1;
    uv3_ = (((_e202 + vec2(2f)) + vec2(0.5f)) * _e207);
    let _e210 = uv12_[0u];
    let _e212 = uv0_[0u];
    let _e214 = uv3_[0u];
    let _e216 = uv12_[0u];
    let _e218 = uv12_[0u];
    ux = array<f32, 5>(_e210, _e212, _e214, _e216, _e218);
    let _e221 = uv12_[1u];
    let _e223 = uv12_[1u];
    let _e225 = uv12_[1u];
    let _e227 = uv0_[1u];
    let _e229 = uv3_[1u];
    uy = array<f32, 5>(_e221, _e223, _e225, _e227, _e229);
    let _e232 = w12_[0u];
    let _e234 = w12_[1u];
    let _e237 = w0_[0u];
    let _e239 = w12_[1u];
    let _e242 = w3_[0u];
    let _e244 = w12_[1u];
    let _e247 = w12_[0u];
    let _e249 = w0_[1u];
    let _e252 = w12_[0u];
    let _e254 = w3_[1u];
    w = array<f32, 5>((_e232 * _e234), (_e237 * _e239), (_e242 * _e244), (_e247 * _e249), (_e252 * _e254));
    sum_1 = vec4<f32>(0f, 0f, 0f, 0f);
    total = 0f;
    i_2 = 0i;
    loop {
        let _e257 = i_2;
        if (_e257 < 5i) {
            let _e259 = i_2;
            let _e261 = ux[_e259];
            let _e262 = i_2;
            let _e264 = uy[_e262];
            let _e266 = textureSampleLevel(previousColor, nearestSampler, vec2<f32>(_e261, _e264), 0f);
            tap_1 = _e266;
            let _e267 = i_2;
            let _e269 = w[_e267];
            let _e270 = tap_1;
            let _e272 = sum_1;
            sum_1 = (_e272 + (_e270 * _e269));
            let _e274 = i_2;
            let _e276 = w[_e274];
            let _e277 = total;
            total = (_e277 + _e276);
            let _e279 = (*tapMin_1);
            let _e280 = tap_1;
            (*tapMin_1) = min(_e279, _e280.xyz);
            let _e283 = (*tapMax_1);
            let _e284 = tap_1;
            (*tapMax_1) = max(_e283, _e284.xyz);
            continue;
        } else {
            break;
        }
        continuing {
            let _e287 = i_2;
            i_2 = (_e287 + 1i);
        }
    }
    let _e289 = sum_1;
    let _e290 = total;
    let _e291 = total;
    return (_e289 * inverseSqrt((_e290 * _e291)));
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_153_: bool;

    let _e87 = (*v_1);
    let _e88 = (*v_1);
    let _e90 = all((_e87 == _e88));
    phi_153_ = _e90;
    if _e90 {
        let _e91 = (*v_1);
        phi_153_ = all((abs(_e91) <= vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e96 = phi_153_;
    return _e96;
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
    var momentMean: vec3<f32>;
    var momentSigma: vec3<f32>;
    var phi_1367_: bool;

    neighborhoodMin = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
    neighborhoodMax = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
    momentSum = vec3<f32>(0f, 0f, 0f);
    momentSquares = vec3<f32>(0f, 0f, 0f);
    y_1 = -1i;
    loop {
        let _e100 = y_1;
        if (_e100 <= 1i) {
            x_1 = -1i;
            loop {
                let _e102 = x_1;
                if (_e102 <= 1i) {
                    let _e104 = (*tileIndex);
                    let _e105 = y_1;
                    let _e108 = x_1;
                    let _e111 = tileColor[((_e104 + (_e105 * 10i)) + _e108)];
                    c_1 = _e111.xyz;
                    let _e113 = neighborhoodMin;
                    let _e114 = c_1;
                    neighborhoodMin = min(_e113, _e114);
                    let _e116 = neighborhoodMax;
                    let _e117 = c_1;
                    neighborhoodMax = max(_e116, _e117);
                    let _e119 = c_1;
                    let _e120 = momentSum;
                    momentSum = (_e120 + _e119);
                    let _e122 = c_1;
                    let _e123 = c_1;
                    let _e125 = momentSquares;
                    momentSquares = (_e125 + (_e122 * _e123));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e127 = x_1;
                    x_1 = (_e127 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e129 = y_1;
            y_1 = (_e129 + 1i);
        }
    }
    let _e131 = neighborhoodMin;
    param_23 = _e131;
    let _e132 = finite3_u0028_vf3_u003b((&param_23));
    phi_1367_ = _e132;
    if _e132 {
        let _e133 = neighborhoodMax;
        param_24 = _e133;
        let _e134 = finite3_u0028_vf3_u003b((&param_24));
        phi_1367_ = _e134;
    }
    let _e136 = phi_1367_;
    if !(_e136) {
        return false;
    }
    let _e138 = neighborhoodMin;
    (*boxMin) = _e138;
    let _e139 = neighborhoodMax;
    (*boxMax) = _e139;
    let _e141 = pc.qualityFlags;
    if ((_e141 & 1u) != 0u) {
        let _e144 = momentSum;
        momentMean = (_e144 * 0.11111111f);
        let _e146 = momentSquares;
        let _e148 = momentMean;
        let _e149 = momentMean;
        momentSigma = sqrt(max(((_e146 * 0.11111111f) - (_e148 * _e149)), vec3<f32>(0f, 0f, 0f)));
        let _e154 = neighborhoodMin;
        let _e155 = momentMean;
        let _e156 = momentSigma;
        (*boxMin) = max(_e154, (_e155 - (_e156 * 1.25f)));
        let _e160 = neighborhoodMax;
        let _e161 = momentMean;
        let _e162 = momentSigma;
        (*boxMax) = min(_e160, (_e161 + (_e162 * 1.25f)));
    }
    return true;
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_2: ptr<function, f32>, y_2: ptr<function, f32>, a_1: ptr<function, f32>) -> f32 {
    let _e89 = (*x_2);
    let _e90 = (*a_1);
    let _e93 = (*y_2);
    let _e94 = (*a_1);
    return ((_e89 * (1f - _e90)) + (_e93 * _e94));
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

    let _e106 = (*texel_2);
    baseFloat_1 = floor(_e106);
    let _e108 = baseFloat_1;
    base_1 = vec2<i32>(_e108);
    let _e110 = (*texel_2);
    let _e111 = baseFloat_1;
    f_1 = (_e110 - _e111);
    let _e114 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e114) - vec2<i32>(1i, 1i));
    let _e117 = base_1;
    let _e118 = hi;
    let _e120 = textureLoad(previousLinearDepth, clamp(_e117, vec2<i32>(0i, 0i), _e118), 0i);
    d00_ = _e120.x;
    let _e122 = base_1;
    let _e124 = hi;
    let _e126 = textureLoad(previousLinearDepth, clamp((_e122 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e124), 0i);
    d10_ = _e126.x;
    let _e128 = base_1;
    let _e130 = hi;
    let _e132 = textureLoad(previousLinearDepth, clamp((_e128 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e130), 0i);
    d01_ = _e132.x;
    let _e134 = base_1;
    let _e136 = hi;
    let _e138 = textureLoad(previousLinearDepth, clamp((_e134 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e136), 0i);
    d11_ = _e138.x;
    let _e140 = d00_;
    param_25 = _e140;
    let _e141 = d10_;
    param_26 = _e141;
    let _e143 = f_1[0u];
    param_27 = _e143;
    let _e144 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_25), (&param_26), (&param_27));
    top = _e144;
    let _e145 = d01_;
    param_28 = _e145;
    let _e146 = d11_;
    param_29 = _e146;
    let _e148 = f_1[0u];
    param_30 = _e148;
    let _e149 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_28), (&param_29), (&param_30));
    bottom = _e149;
    let _e150 = top;
    param_31 = _e150;
    let _e151 = bottom;
    param_32 = _e151;
    let _e153 = f_1[1u];
    param_33 = _e153;
    let _e154 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_31), (&param_32), (&param_33));
    return _e154;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator_1: f32;
    var numerator: f32;
    var result_1: f32;

    let _e93 = pc.zFar;
    let _e95 = pc.zNear;
    range = (_e93 - _e95);
    let _e97 = (*z_1);
    let _e98 = range;
    scaled_1 = (_e97 * _e98);
    let _e101 = pc.zNear;
    let _e102 = scaled_1;
    denominator_1 = (_e101 + _e102);
    let _e105 = pc.zNear;
    let _e107 = pc.zFar;
    numerator = (_e105 * _e107);
    let _e109 = numerator;
    let _e110 = denominator_1;
    result_1 = (_e109 / _e110);
    let _e112 = result_1;
    return _e112;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_138_: bool;

    let _e87 = (*v_2);
    let _e88 = (*v_2);
    let _e90 = all((_e87 == _e88));
    phi_138_ = _e90;
    if _e90 {
        let _e91 = (*v_2);
        phi_138_ = all((abs(_e91) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e96 = phi_138_;
    return _e96;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_168_: bool;

    let _e87 = (*v_3);
    let _e88 = (*v_3);
    let _e90 = all((_e87 == _e88));
    phi_168_ = _e90;
    if _e90 {
        let _e91 = (*v_3);
        phi_168_ = all((abs(_e91) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e96 = phi_168_;
    return _e96;
}

fn dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b(px_1: ptr<function, vec2<i32>>, tileIndex_1: ptr<function, i32>, centerValid: ptr<function, f32>) -> vec2<f32> {
    var centerVelocity: vec2<f32>;
    var param_34: vec2<f32>;
    var hi_1: vec2<i32>;
    var best: vec2<i32>;
    var bestDepth: f32;
    var y_3: i32;
    var x_3: i32;
    var d: f32;
    var v_4: f32;
    var dv: vec2<f32>;
    var param_35: vec2<f32>;
    var phi_1159_: bool;
    var phi_1167_: bool;

    let _e100 = (*px_1);
    let _e101 = textureLoad(motionVelocity, _e100, 0i);
    centerVelocity = _e101.xy;
    let _e104 = pc.qualityFlags;
    let _e106 = ((_e104 & 2u) == 0u);
    phi_1159_ = _e106;
    if !(_e106) {
        let _e108 = (*centerValid);
        phi_1159_ = !((_e108 > 0f));
    }
    let _e112 = phi_1159_;
    phi_1167_ = _e112;
    if !(_e112) {
        let _e114 = centerVelocity;
        param_34 = _e114;
        let _e115 = finite2_u0028_vf2_u003b((&param_34));
        phi_1167_ = !(_e115);
    }
    let _e118 = phi_1167_;
    if _e118 {
        let _e119 = centerVelocity;
        return _e119;
    }
    let _e121 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e121) - vec2<i32>(1i, 1i));
    let _e124 = (*px_1);
    best = _e124;
    let _e125 = (*tileIndex_1);
    let _e127 = tileDepth[_e125];
    bestDepth = _e127;
    y_3 = -1i;
    loop {
        let _e128 = y_3;
        if (_e128 <= 1i) {
            x_3 = -1i;
            loop {
                let _e130 = x_3;
                if (_e130 <= 1i) {
                    let _e132 = (*tileIndex_1);
                    let _e133 = y_3;
                    let _e136 = x_3;
                    let _e139 = tileDepth[((_e132 + (_e133 * 10i)) + _e136)];
                    d = _e139;
                    let _e140 = d;
                    let _e141 = bestDepth;
                    if (_e140 > _e141) {
                        let _e143 = d;
                        bestDepth = _e143;
                        let _e144 = (*px_1);
                        let _e145 = x_3;
                        let _e146 = y_3;
                        let _e149 = hi_1;
                        best = clamp((_e144 + vec2<i32>(_e145, _e146)), vec2<i32>(0i, 0i), _e149);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e151 = x_3;
                    x_3 = (_e151 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e153 = y_3;
            y_3 = (_e153 + 1i);
        }
    }
    let _e155 = best;
    let _e156 = (*px_1);
    if all((_e155 == _e156)) {
        let _e159 = centerVelocity;
        return _e159;
    }
    let _e160 = best;
    let _e161 = textureLoad(motionValidity, _e160, 0i);
    v_4 = _e161.x;
    let _e163 = v_4;
    if !((_e163 > 0f)) {
        let _e166 = centerVelocity;
        return _e166;
    }
    let _e167 = best;
    let _e168 = textureLoad(motionVelocity, _e167, 0i);
    dv = _e168.xy;
    let _e170 = dv;
    param_35 = _e170;
    let _e171 = finite2_u0028_vf2_u003b((&param_35));
    let _e172 = dv;
    let _e173 = centerVelocity;
    return select(_e173, _e172, vec2(_e171));
}

fn reconstructCurrent_u0028_vi2_u003b_i1_u003b(pixel_1: ptr<function, vec2<i32>>, tileIndex_2: ptr<function, i32>) -> vec4<f32> {
    var extent_2: vec2<f32>;
    var sampleAt: vec2<f32>;

    let _e91 = pc.extent;
    extent_2 = vec2<f32>(_e91);
    let _e93 = (*pixel_1);
    let _e97 = extent_2;
    let _e98 = extent_2;
    let _e103 = pc.currentEffectiveJitterUv;
    sampleAt = (((vec2<f32>(_e93) + vec2(0.5f)) * inverseSqrt((_e97 * _e98))) + _e103);
    let _e105 = sampleAt;
    let _e106 = textureSampleLevel(currentColor, nearestSampler, _e105, 0f);
    let _e107 = _e106.xyz;
    let _e108 = (*tileIndex_2);
    let _e111 = tileColor[_e108][3u];
    return vec4<f32>(_e107.x, _e107.y, _e107.z, _e111);
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
    var param_36: vec2<i32>;
    var param_37: i32;
    var result_2: vec4<f32>;
    var displayed: vec4<f32>;
    var usedHistory: bool;
    var valid: f32;
    var velocity: vec2<f32>;
    var param_38: vec2<i32>;
    var param_39: i32;
    var param_40: f32;
    var motionTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_41: vec4<f32>;
    var param_42: f32;
    var param_43: vec2<f32>;
    var param_44: vec2<f32>;
    var currentDevice: f32;
    var deviceLo: f32;
    var deviceHi: f32;
    var y_4: i32;
    var x_4: i32;
    var d_1: f32;
    var currentLinear: f32;
    var param_45: f32;
    var rangeA: f32;
    var param_46: f32;
    var rangeB: f32;
    var param_47: f32;
    var priorLinear: f32;
    var param_48: vec2<f32>;
    var depthLimit: f32;
    var param_49: f32;
    var param_50: f32;
    var param_51: f32;
    var param_52: f32;
    var boxValid: bool;
    var param_53: i32;
    var param_54: vec3<f32>;
    var param_55: vec3<f32>;
    var tapMin_2: vec3<f32>;
    var tapMax_2: vec3<f32>;
    var prior: vec4<f32>;
    var param_56: vec2<f32>;
    var param_57: vec3<f32>;
    var param_58: vec3<f32>;
    var param_59: vec4<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var baseWeight: f32;
    var weight_1: f32;
    var param_60: f32;
    var param_61: f32;
    var resolvedRgb: vec3<f32>;
    var param_62: vec3<f32>;
    var param_63: vec3<f32>;
    var param_64: f32;
    var detail: vec3<f32>;
    var param_65: vec2<f32>;
    var param_66: vec3<f32>;
    var param_67: vec3<f32>;
    var param_68: vec4<f32>;
    var param_69: vec2<i32>;
    var param_70: vec4<f32>;
    var param_71: vec4<f32>;
    var param_72: vec4<f32>;
    var param_73: f32;
    var phi_1555_: bool;
    var phi_1564_: bool;
    var phi_1573_: bool;
    var phi_1579_: bool;
    var phi_1662_: bool;
    var phi_1668_: bool;
    var phi_1674_: bool;
    var phi_1690_: bool;
    var phi_1700_: bool;
    var phi_1729_: bool;
    var phi_1815_: bool;
    var phi_1823_: bool;
    var phi_1846_: bool;

    let _e163 = gl_GlobalInvocationID_1;
    pixel_2 = bitcast<vec2<i32>>(_e163.xy);
    let _e166 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e166.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e173 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e173) - vec2<i32>(1i, 1i));
    let _e176 = gl_LocalInvocationIndex_1;
    i_3 = _e176;
    loop {
        let _e177 = i_3;
        if (_e177 < 100u) {
            let _e179 = tileOrigin;
            let _e180 = i_3;
            let _e183 = i_3;
            let _e188 = imageHi;
            q = clamp((_e179 + vec2<i32>(bitcast<i32>((_e180 % 10u)), bitcast<i32>((_e183 / 10u)))), vec2<i32>(0i, 0i), _e188);
            let _e190 = i_3;
            let _e191 = q;
            let _e192 = textureLoad(currentColor, _e191, 0i);
            tileColor[_e190] = _e192;
            let _e194 = i_3;
            let _e195 = q;
            let _e196 = textureLoad(currentDeviceDepth, _e195, 0i);
            tileDepth[_e194] = _e196.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e199 = i_3;
            i_3 = (_e199 + 64u);
        }
    }
    workgroupBarrier();
    let _e201 = pixel_2;
    let _e203 = pc.extent;
    if any((_e201 >= bitcast<vec2<i32>>(_e203))) {
        return;
    }
    let _e208 = gl_LocalInvocationID_1[1u];
    let _e213 = gl_LocalInvocationID_1[0u];
    tileIndex_3 = ((((bitcast<i32>(_e208) + 1i) * 10i) + bitcast<i32>(_e213)) + 1i);
    clipMin = vec3<f32>(0f, 0f, 0f);
    clipMax = vec3<f32>(0f, 0f, 0f);
    let _e217 = pixel_2;
    param_36 = _e217;
    let _e218 = tileIndex_3;
    param_37 = _e218;
    let _e219 = reconstructCurrent_u0028_vi2_u003b_i1_u003b((&param_36), (&param_37));
    current = _e219;
    let _e220 = current;
    result_2 = _e220;
    let _e221 = current;
    displayed = _e221;
    usedHistory = false;
    let _e222 = pixel_2;
    let _e223 = textureLoad(motionValidity, _e222, 0i);
    valid = _e223.x;
    let _e225 = pixel_2;
    param_38 = _e225;
    let _e226 = tileIndex_3;
    param_39 = _e226;
    let _e227 = valid;
    param_40 = _e227;
    let _e228 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_38), (&param_39), (&param_40));
    velocity = _e228;
    let _e229 = velocity;
    let _e231 = pc.extent;
    motionTexel = (_e229 * vec2<f32>(_e231));
    let _e234 = pixel_2;
    let _e236 = motionTexel;
    previousTexel = (vec2<f32>(_e234) - _e236);
    let _e238 = previousTexel;
    previousCenter = (_e238 + vec2(0.5f));
    let _e241 = previousCenter;
    let _e243 = all((_e241 >= vec2<f32>(0f, 0f)));
    phi_1555_ = _e243;
    if _e243 {
        let _e244 = previousCenter;
        let _e246 = pc.extent;
        phi_1555_ = all((_e244 < vec2<f32>(_e246)));
    }
    let _e251 = phi_1555_;
    inside = _e251;
    let _e252 = current;
    param_41 = _e252;
    let _e253 = finite4_u0028_vf4_u003b((&param_41));
    phi_1564_ = _e253;
    if _e253 {
        let _e254 = valid;
        param_42 = _e254;
        let _e255 = finite1_u0028_f1_u003b((&param_42));
        phi_1564_ = _e255;
    }
    let _e257 = phi_1564_;
    let _e258 = valid;
    let _e260 = (_e257 && (_e258 > 0f));
    phi_1573_ = _e260;
    if _e260 {
        let _e261 = velocity;
        param_43 = _e261;
        let _e262 = finite2_u0028_vf2_u003b((&param_43));
        phi_1573_ = _e262;
    }
    let _e264 = phi_1573_;
    phi_1579_ = _e264;
    if _e264 {
        let _e265 = previousTexel;
        param_44 = _e265;
        let _e266 = finite2_u0028_vf2_u003b((&param_44));
        phi_1579_ = _e266;
    }
    let _e268 = phi_1579_;
    let _e269 = inside;
    if (_e268 && _e269) {
        let _e271 = tileIndex_3;
        let _e273 = tileDepth[_e271];
        currentDevice = _e273;
        let _e274 = currentDevice;
        deviceLo = _e274;
        let _e275 = currentDevice;
        deviceHi = _e275;
        y_4 = -1i;
        loop {
            let _e276 = y_4;
            if (_e276 <= 1i) {
                x_4 = -1i;
                loop {
                    let _e278 = x_4;
                    if (_e278 <= 1i) {
                        let _e280 = tileIndex_3;
                        let _e281 = y_4;
                        let _e284 = x_4;
                        let _e287 = tileDepth[((_e280 + (_e281 * 10i)) + _e284)];
                        d_1 = _e287;
                        let _e288 = deviceLo;
                        let _e289 = d_1;
                        deviceLo = min(_e288, _e289);
                        let _e291 = deviceHi;
                        let _e292 = d_1;
                        deviceHi = max(_e291, _e292);
                        continue;
                    } else {
                        break;
                    }
                    continuing {
                        let _e294 = x_4;
                        x_4 = (_e294 + 1i);
                    }
                }
                continue;
            } else {
                break;
            }
            continuing {
                let _e296 = y_4;
                y_4 = (_e296 + 1i);
            }
        }
        let _e298 = currentDevice;
        param_45 = _e298;
        let _e299 = linearizeDepth_u0028_f1_u003b((&param_45));
        currentLinear = _e299;
        let _e300 = deviceLo;
        param_46 = _e300;
        let _e301 = linearizeDepth_u0028_f1_u003b((&param_46));
        rangeA = _e301;
        let _e302 = deviceHi;
        param_47 = _e302;
        let _e303 = linearizeDepth_u0028_f1_u003b((&param_47));
        rangeB = _e303;
        let _e304 = previousTexel;
        param_48 = _e304;
        let _e305 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_48));
        priorLinear = _e305;
        let _e307 = pc.depthThresholdAbsolute;
        let _e309 = pc.depthThresholdRelative;
        let _e310 = currentLinear;
        let _e311 = priorLinear;
        depthLimit = max(_e307, (_e309 * max(_e310, _e311)));
        let _e315 = currentLinear;
        param_49 = _e315;
        let _e316 = finite1_u0028_f1_u003b((&param_49));
        phi_1662_ = _e316;
        if _e316 {
            let _e317 = priorLinear;
            param_50 = _e317;
            let _e318 = finite1_u0028_f1_u003b((&param_50));
            phi_1662_ = _e318;
        }
        let _e320 = phi_1662_;
        phi_1668_ = _e320;
        if _e320 {
            let _e321 = rangeA;
            param_51 = _e321;
            let _e322 = finite1_u0028_f1_u003b((&param_51));
            phi_1668_ = _e322;
        }
        let _e324 = phi_1668_;
        phi_1674_ = _e324;
        if _e324 {
            let _e325 = rangeB;
            param_52 = _e325;
            let _e326 = finite1_u0028_f1_u003b((&param_52));
            phi_1674_ = _e326;
        }
        let _e328 = phi_1674_;
        let _e329 = currentLinear;
        let _e332 = priorLinear;
        let _e334 = ((_e328 && (_e329 > 0f)) && (_e332 > 0f));
        phi_1690_ = _e334;
        if _e334 {
            let _e335 = priorLinear;
            let _e336 = rangeA;
            let _e337 = rangeB;
            let _e339 = depthLimit;
            phi_1690_ = (_e335 >= (min(_e336, _e337) - _e339));
        }
        let _e343 = phi_1690_;
        phi_1700_ = _e343;
        if _e343 {
            let _e344 = priorLinear;
            let _e345 = rangeA;
            let _e346 = rangeB;
            let _e348 = depthLimit;
            phi_1700_ = (_e344 <= (max(_e345, _e346) + _e348));
        }
        let _e352 = phi_1700_;
        if _e352 {
            let _e353 = tileIndex_3;
            param_53 = _e353;
            let _e354 = neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b((&param_53), (&param_54), (&param_55));
            let _e355 = param_54;
            clipMin = _e355;
            let _e356 = param_55;
            clipMax = _e356;
            boxValid = _e354;
            tapMin_2 = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
            tapMax_2 = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
            let _e357 = previousTexel;
            param_56 = _e357;
            let _e358 = tapMin_2;
            param_57 = _e358;
            let _e359 = tapMax_2;
            param_58 = _e359;
            let _e360 = samplePreviousColor_u0028_vf2_u003b_vf3_u003b_vf3_u003b((&param_56), (&param_57), (&param_58));
            let _e361 = param_57;
            tapMin_2 = _e361;
            let _e362 = param_58;
            tapMax_2 = _e362;
            prior = _e360;
            let _e363 = boxValid;
            phi_1729_ = _e363;
            if _e363 {
                let _e364 = prior;
                param_59 = _e364;
                let _e365 = finite4_u0028_vf4_u003b((&param_59));
                phi_1729_ = _e365;
            }
            let _e367 = phi_1729_;
            if _e367 {
                let _e368 = prior;
                let _e370 = clipMin;
                let _e371 = clipMax;
                clampedPrior = clamp(_e368.xyz, _e370, _e371);
                let _e373 = valid;
                confidence_1 = clamp(_e373, 0f, 1f);
                let _e376 = pc.historyWeight;
                baseWeight = clamp(_e376, 0f, 1f);
                let _e378 = baseWeight;
                param_60 = _e378;
                let _e379 = confidence_1;
                param_61 = _e379;
                let _e380 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_60), (&param_61));
                weight_1 = _e380;
                let _e381 = current;
                param_62 = _e381.xyz;
                let _e383 = clampedPrior;
                param_63 = _e383;
                let _e384 = weight_1;
                param_64 = _e384;
                let _e385 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_62), (&param_63), (&param_64));
                resolvedRgb = _e385;
                let _e386 = resolvedRgb;
                let _e388 = current[3u];
                result_2 = vec4<f32>(_e386.x, _e386.y, _e386.z, _e388);
                let _e393 = result_2;
                displayed = _e393;
                let _e395 = pc.qualityFlags;
                if ((_e395 & 8u) == 0u) {
                    let _e398 = prior;
                    let _e400 = previousTexel;
                    param_65 = _e400;
                    let _e401 = tapMin_2;
                    param_66 = _e401;
                    let _e402 = tapMax_2;
                    param_67 = _e402;
                    let _e403 = samplePreviousBlur_u0028_vf2_u003b_vf3_u003b_vf3_u003b((&param_65), (&param_66), (&param_67));
                    let _e404 = param_66;
                    tapMin_2 = _e404;
                    let _e405 = param_67;
                    tapMax_2 = _e405;
                    detail = (_e398.xyz - _e403);
                    let _e407 = resolvedRgb;
                    let _e408 = confidence_1;
                    let _e410 = detail;
                    let _e413 = resolvedRgb;
                    let _e414 = tapMin_2;
                    let _e416 = resolvedRgb;
                    let _e417 = tapMax_2;
                    let _e419 = clamp((_e407 + (_e410 * (2f * _e408))), min(_e413, _e414), max(_e416, _e417));
                    let _e421 = current[3u];
                    displayed = vec4<f32>(_e419.x, _e419.y, _e419.z, _e421);
                }
                usedHistory = true;
            }
        }
    }
    let _e426 = usedHistory;
    let _e427 = !(_e426);
    phi_1815_ = _e427;
    if _e427 {
        let _e428 = current;
        param_68 = _e428;
        let _e429 = finite4_u0028_vf4_u003b((&param_68));
        phi_1815_ = _e429;
    }
    let _e431 = phi_1815_;
    phi_1823_ = _e431;
    if _e431 {
        let _e433 = pc.qualityFlags;
        phi_1823_ = ((_e433 & 4u) != 0u);
    }
    let _e437 = phi_1823_;
    if _e437 {
        let _e438 = pixel_2;
        param_69 = _e438;
        let _e439 = current;
        param_70 = _e439;
        let _e440 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_69), (&param_70));
        result_2 = _e440;
    }
    let _e441 = result_2;
    param_71 = _e441;
    let _e442 = finite4_u0028_vf4_u003b((&param_71));
    if !(_e442) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e444 = usedHistory;
    let _e445 = !(_e444);
    phi_1846_ = _e445;
    if !(_e445) {
        let _e447 = displayed;
        param_72 = _e447;
        let _e448 = finite4_u0028_vf4_u003b((&param_72));
        phi_1846_ = !(_e448);
    }
    let _e451 = phi_1846_;
    if _e451 {
        let _e452 = result_2;
        displayed = _e452;
    }
    let _e453 = pixel_2;
    let _e454 = displayed;
    textureStore(resolvedColor, _e453, _e454);
    let _e455 = pixel_2;
    let _e456 = result_2;
    textureStore(historyOutColor, _e455, _e456);
    let _e457 = pixel_2;
    let _e458 = tileIndex_3;
    let _e460 = tileDepth[_e458];
    param_73 = _e460;
    let _e461 = storeLinearDepth_u0028_f1_u003b((&param_73));
    textureStore(historyOutLinearDepth, _e457, vec4(_e461));
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
