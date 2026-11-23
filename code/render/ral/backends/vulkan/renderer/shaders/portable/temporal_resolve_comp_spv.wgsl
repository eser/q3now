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
    var phi_124_: bool;

    let _e87 = (*v);
    let _e88 = (*v);
    let _e89 = (_e87 == _e88);
    phi_124_ = _e89;
    if _e89 {
        let _e90 = (*v);
        phi_124_ = (abs(_e90) <= 0x1.fffffep+127f);
    }
    let _e94 = phi_124_;
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

fn samplePreviousBlur_u0028_vf2_u003b(texel: ptr<function, vec2<f32>>) -> vec3<f32> {
    var extent: vec2<f32>;
    var invExtent: vec2<f32>;
    var sum: vec3<f32>;
    var i_1: i32;
    var o: vec2<f32>;

    let _e93 = pc.extent;
    extent = vec2<f32>(_e93);
    let _e95 = extent;
    let _e96 = extent;
    invExtent = inverseSqrt((_e95 * _e96));
    sum = vec3<f32>(0f, 0f, 0f);
    i_1 = 0i;
    loop {
        let _e99 = i_1;
        if (_e99 < 4i) {
            let _e101 = i_1;
            let _e105 = i_1;
            o = vec2<f32>(select(-0.5f, 0.5f, ((_e101 & 1i) != 0i)), select(-0.5f, 0.5f, ((_e105 & 2i) != 0i)));
            let _e110 = (*texel);
            let _e113 = o;
            let _e115 = invExtent;
            let _e117 = textureSampleLevel(previousColor, nearestSampler, (((_e110 + vec2(0.5f)) + _e113) * _e115), 0f);
            let _e119 = sum;
            sum = (_e119 + _e117.xyz);
            continue;
        } else {
            break;
        }
        continuing {
            let _e121 = i_1;
            i_1 = (_e121 + 1i);
        }
    }
    let _e123 = sum;
    return (_e123 * 0.25f);
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

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_154_: bool;

    let _e87 = (*v_1);
    let _e88 = (*v_1);
    let _e90 = all((_e87 == _e88));
    phi_154_ = _e90;
    if _e90 {
        let _e91 = (*v_1);
        phi_154_ = all((abs(_e91) <= vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e96 = phi_154_;
    return _e96;
}

fn samplePreviousColor_u0028_vf2_u003b(texel_1: ptr<function, vec2<f32>>) -> vec4<f32> {
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

    let _e110 = (*texel_1);
    baseFloat = floor(_e110);
    let _e112 = (*texel_1);
    let _e113 = baseFloat;
    f = (_e112 - _e113);
    let _e115 = f;
    d0_ = (vec2(1f) + _e115);
    let _e118 = f;
    d1_ = _e118;
    let _e119 = f;
    d2_ = (vec2(1f) - _e119);
    let _e122 = f;
    d3_ = (vec2(2f) - _e122);
    let _e125 = d0_;
    let _e129 = d0_;
    let _e133 = d0_;
    w0_ = ((((((_e125 * -0.75f) - vec2(-3.75f)) * _e129) + vec2(-6f)) * _e133) - vec2(-3f));
    let _e137 = d1_;
    let _e141 = d1_;
    let _e143 = d1_;
    w1_ = (((((_e137 * 1.25f) - vec2(2.25f)) * _e141) * _e143) + vec2(1f));
    let _e147 = d2_;
    let _e151 = d2_;
    let _e153 = d2_;
    w2_ = (((((_e147 * 1.25f) - vec2(2.25f)) * _e151) * _e153) + vec2(1f));
    let _e157 = d3_;
    let _e161 = d3_;
    let _e165 = d3_;
    w3_ = ((((((_e157 * -0.75f) - vec2(-3.75f)) * _e161) + vec2(-6f)) * _e165) - vec2(-3f));
    let _e169 = w1_;
    let _e170 = w2_;
    w12_ = (_e169 + _e170);
    let _e172 = w2_;
    let _e173 = w12_;
    let _e174 = w12_;
    offset12_ = (_e172 * inverseSqrt((_e173 * _e174)));
    let _e179 = pc.extent;
    extent_1 = vec2<f32>(_e179);
    let _e181 = extent_1;
    let _e182 = extent_1;
    invExtent_1 = inverseSqrt((_e181 * _e182));
    let _e185 = baseFloat;
    let _e190 = invExtent_1;
    uv0_ = (((_e185 - vec2(1f)) + vec2(0.5f)) * _e190);
    let _e192 = baseFloat;
    let _e193 = offset12_;
    let _e197 = invExtent_1;
    uv12_ = (((_e192 + _e193) + vec2(0.5f)) * _e197);
    let _e199 = baseFloat;
    let _e204 = invExtent_1;
    uv3_ = (((_e199 + vec2(2f)) + vec2(0.5f)) * _e204);
    let _e207 = uv12_[0u];
    let _e209 = uv0_[0u];
    let _e211 = uv3_[0u];
    let _e213 = uv12_[0u];
    let _e215 = uv12_[0u];
    ux = array<f32, 5>(_e207, _e209, _e211, _e213, _e215);
    let _e218 = uv12_[1u];
    let _e220 = uv12_[1u];
    let _e222 = uv12_[1u];
    let _e224 = uv0_[1u];
    let _e226 = uv3_[1u];
    uy = array<f32, 5>(_e218, _e220, _e222, _e224, _e226);
    let _e229 = w12_[0u];
    let _e231 = w12_[1u];
    let _e234 = w0_[0u];
    let _e236 = w12_[1u];
    let _e239 = w3_[0u];
    let _e241 = w12_[1u];
    let _e244 = w12_[0u];
    let _e246 = w0_[1u];
    let _e249 = w12_[0u];
    let _e251 = w3_[1u];
    w = array<f32, 5>((_e229 * _e231), (_e234 * _e236), (_e239 * _e241), (_e244 * _e246), (_e249 * _e251));
    sum_1 = vec4<f32>(0f, 0f, 0f, 0f);
    total = 0f;
    i_2 = 0i;
    loop {
        let _e254 = i_2;
        if (_e254 < 5i) {
            let _e256 = i_2;
            let _e258 = w[_e256];
            let _e259 = i_2;
            let _e261 = ux[_e259];
            let _e262 = i_2;
            let _e264 = uy[_e262];
            let _e266 = textureSampleLevel(previousColor, nearestSampler, vec2<f32>(_e261, _e264), 0f);
            let _e268 = sum_1;
            sum_1 = (_e268 + (_e266 * _e258));
            let _e270 = i_2;
            let _e272 = w[_e270];
            let _e273 = total;
            total = (_e273 + _e272);
            continue;
        } else {
            break;
        }
        continuing {
            let _e275 = i_2;
            i_2 = (_e275 + 1i);
        }
    }
    let _e277 = sum_1;
    let _e278 = total;
    let _e279 = total;
    return (_e277 * inverseSqrt((_e278 * _e279)));
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_1: ptr<function, f32>, y_1: ptr<function, f32>, a_1: ptr<function, f32>) -> f32 {
    let _e89 = (*x_1);
    let _e90 = (*a_1);
    let _e93 = (*y_1);
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
    var param_23: f32;
    var param_24: f32;
    var param_25: f32;
    var bottom: f32;
    var param_26: f32;
    var param_27: f32;
    var param_28: f32;
    var param_29: f32;
    var param_30: f32;
    var param_31: f32;

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
    param_23 = _e140;
    let _e141 = d10_;
    param_24 = _e141;
    let _e143 = f_1[0u];
    param_25 = _e143;
    let _e144 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_23), (&param_24), (&param_25));
    top = _e144;
    let _e145 = d01_;
    param_26 = _e145;
    let _e146 = d11_;
    param_27 = _e146;
    let _e148 = f_1[0u];
    param_28 = _e148;
    let _e149 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_26), (&param_27), (&param_28));
    bottom = _e149;
    let _e150 = top;
    param_29 = _e150;
    let _e151 = bottom;
    param_30 = _e151;
    let _e153 = f_1[1u];
    param_31 = _e153;
    let _e154 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_29), (&param_30), (&param_31));
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
    var phi_139_: bool;

    let _e87 = (*v_2);
    let _e88 = (*v_2);
    let _e90 = all((_e87 == _e88));
    phi_139_ = _e90;
    if _e90 {
        let _e91 = (*v_2);
        phi_139_ = all((abs(_e91) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e96 = phi_139_;
    return _e96;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_169_: bool;

    let _e87 = (*v_3);
    let _e88 = (*v_3);
    let _e90 = all((_e87 == _e88));
    phi_169_ = _e90;
    if _e90 {
        let _e91 = (*v_3);
        phi_169_ = all((abs(_e91) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e96 = phi_169_;
    return _e96;
}

fn dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b(px_1: ptr<function, vec2<i32>>, tileIndex: ptr<function, i32>, centerValid: ptr<function, f32>) -> vec2<f32> {
    var centerVelocity: vec2<f32>;
    var param_32: vec2<f32>;
    var hi_1: vec2<i32>;
    var best: vec2<i32>;
    var bestDepth: f32;
    var y_2: i32;
    var x_2: i32;
    var d: f32;
    var v_4: f32;
    var dv: vec2<f32>;
    var param_33: vec2<f32>;
    var phi_1142_: bool;
    var phi_1150_: bool;

    let _e100 = (*px_1);
    let _e101 = textureLoad(motionVelocity, _e100, 0i);
    centerVelocity = _e101.xy;
    let _e104 = pc.qualityFlags;
    let _e106 = ((_e104 & 2u) == 0u);
    phi_1142_ = _e106;
    if !(_e106) {
        let _e108 = (*centerValid);
        phi_1142_ = !((_e108 > 0f));
    }
    let _e112 = phi_1142_;
    phi_1150_ = _e112;
    if !(_e112) {
        let _e114 = centerVelocity;
        param_32 = _e114;
        let _e115 = finite2_u0028_vf2_u003b((&param_32));
        phi_1150_ = !(_e115);
    }
    let _e118 = phi_1150_;
    if _e118 {
        let _e119 = centerVelocity;
        return _e119;
    }
    let _e121 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e121) - vec2<i32>(1i, 1i));
    let _e124 = (*px_1);
    best = _e124;
    let _e125 = (*tileIndex);
    let _e127 = tileDepth[_e125];
    bestDepth = _e127;
    y_2 = -1i;
    loop {
        let _e128 = y_2;
        if (_e128 <= 1i) {
            x_2 = -1i;
            loop {
                let _e130 = x_2;
                if (_e130 <= 1i) {
                    let _e132 = (*tileIndex);
                    let _e133 = y_2;
                    let _e136 = x_2;
                    let _e139 = tileDepth[((_e132 + (_e133 * 10i)) + _e136)];
                    d = _e139;
                    let _e140 = d;
                    let _e141 = bestDepth;
                    if (_e140 > _e141) {
                        let _e143 = d;
                        bestDepth = _e143;
                        let _e144 = (*px_1);
                        let _e145 = x_2;
                        let _e146 = y_2;
                        let _e149 = hi_1;
                        best = clamp((_e144 + vec2<i32>(_e145, _e146)), vec2<i32>(0i, 0i), _e149);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e151 = x_2;
                    x_2 = (_e151 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e153 = y_2;
            y_2 = (_e153 + 1i);
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
    param_33 = _e170;
    let _e171 = finite2_u0028_vf2_u003b((&param_33));
    let _e172 = dv;
    let _e173 = centerVelocity;
    return select(_e173, _e172, vec2(_e171));
}

fn accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b(index: ptr<function, i32>, weight_1: ptr<function, f32>, sum_2: ptr<function, vec3<f32>>, lo: ptr<function, vec3<f32>>, hi_2: ptr<function, vec3<f32>>, moments: ptr<function, vec3<f32>>, squares: ptr<function, vec3<f32>>) {
    var c_1: vec3<f32>;

    let _e94 = (*index);
    let _e96 = tileColor[_e94];
    c_1 = _e96.xyz;
    let _e98 = c_1;
    let _e99 = (*weight_1);
    let _e101 = (*sum_2);
    (*sum_2) = (_e101 + (_e98 * _e99));
    let _e103 = (*lo);
    let _e104 = c_1;
    (*lo) = min(_e103, _e104);
    let _e106 = (*hi_2);
    let _e107 = c_1;
    (*hi_2) = max(_e106, _e107);
    let _e109 = c_1;
    let _e110 = (*moments);
    (*moments) = (_e110 + _e109);
    let _e112 = c_1;
    let _e113 = c_1;
    let _e115 = (*squares);
    (*squares) = (_e115 + (_e112 * _e113));
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
    var param_34: i32;
    var param_35: f32;
    var param_36: vec3<f32>;
    var param_37: vec3<f32>;
    var param_38: vec3<f32>;
    var param_39: vec3<f32>;
    var param_40: vec3<f32>;
    var param_41: i32;
    var param_42: f32;
    var param_43: vec3<f32>;
    var param_44: vec3<f32>;
    var param_45: vec3<f32>;
    var param_46: vec3<f32>;
    var param_47: vec3<f32>;
    var param_48: i32;
    var param_49: f32;
    var param_50: vec3<f32>;
    var param_51: vec3<f32>;
    var param_52: vec3<f32>;
    var param_53: vec3<f32>;
    var param_54: vec3<f32>;
    var param_55: i32;
    var param_56: f32;
    var param_57: vec3<f32>;
    var param_58: vec3<f32>;
    var param_59: vec3<f32>;
    var param_60: vec3<f32>;
    var param_61: vec3<f32>;
    var param_62: i32;
    var param_63: f32;
    var param_64: vec3<f32>;
    var param_65: vec3<f32>;
    var param_66: vec3<f32>;
    var param_67: vec3<f32>;
    var param_68: vec3<f32>;
    var param_69: i32;
    var param_70: f32;
    var param_71: vec3<f32>;
    var param_72: vec3<f32>;
    var param_73: vec3<f32>;
    var param_74: vec3<f32>;
    var param_75: vec3<f32>;
    var param_76: i32;
    var param_77: f32;
    var param_78: vec3<f32>;
    var param_79: vec3<f32>;
    var param_80: vec3<f32>;
    var param_81: vec3<f32>;
    var param_82: vec3<f32>;
    var param_83: i32;
    var param_84: f32;
    var param_85: vec3<f32>;
    var param_86: vec3<f32>;
    var param_87: vec3<f32>;
    var param_88: vec3<f32>;
    var param_89: vec3<f32>;
    var param_90: i32;
    var param_91: f32;
    var param_92: vec3<f32>;
    var param_93: vec3<f32>;
    var param_94: vec3<f32>;
    var param_95: vec3<f32>;
    var param_96: vec3<f32>;
    var momentMean: vec3<f32>;
    var momentSigma: vec3<f32>;

    let _e163 = pc.currentEffectiveJitterUv;
    let _e165 = pc.extent;
    t = (_e163 * vec2<f32>(_e165));
    let _e169 = t[0u];
    let _e172 = t[0u];
    let _e176 = t[0u];
    let _e178 = t[0u];
    let _e182 = t[0u];
    let _e185 = t[0u];
    wx = vec3<f32>(((0.5f * _e169) * (_e172 - 1f)), (1f - (_e176 * _e178)), ((0.5f * _e182) * (_e185 + 1f)));
    let _e190 = t[1u];
    let _e193 = t[1u];
    let _e197 = t[1u];
    let _e199 = t[1u];
    let _e203 = t[1u];
    let _e206 = t[1u];
    wy = vec3<f32>(((0.5f * _e190) * (_e193 - 1f)), (1f - (_e197 * _e199)), ((0.5f * _e203) * (_e206 + 1f)));
    sum_3 = vec3<f32>(0f, 0f, 0f);
    neighborhoodMin = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
    neighborhoodMax = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
    momentSum = vec3<f32>(0f, 0f, 0f);
    momentSquares = vec3<f32>(0f, 0f, 0f);
    let _e210 = (*tileIndex_1);
    let _e214 = wx[0u];
    let _e216 = wy[0u];
    param_34 = ((_e210 - 10i) - 1i);
    param_35 = (_e214 * _e216);
    let _e218 = sum_3;
    param_36 = _e218;
    let _e219 = neighborhoodMin;
    param_37 = _e219;
    let _e220 = neighborhoodMax;
    param_38 = _e220;
    let _e221 = momentSum;
    param_39 = _e221;
    let _e222 = momentSquares;
    param_40 = _e222;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_34), (&param_35), (&param_36), (&param_37), (&param_38), (&param_39), (&param_40));
    let _e223 = param_36;
    sum_3 = _e223;
    let _e224 = param_37;
    neighborhoodMin = _e224;
    let _e225 = param_38;
    neighborhoodMax = _e225;
    let _e226 = param_39;
    momentSum = _e226;
    let _e227 = param_40;
    momentSquares = _e227;
    let _e228 = (*tileIndex_1);
    let _e231 = wx[1u];
    let _e233 = wy[0u];
    param_41 = (_e228 - 10i);
    param_42 = (_e231 * _e233);
    let _e235 = sum_3;
    param_43 = _e235;
    let _e236 = neighborhoodMin;
    param_44 = _e236;
    let _e237 = neighborhoodMax;
    param_45 = _e237;
    let _e238 = momentSum;
    param_46 = _e238;
    let _e239 = momentSquares;
    param_47 = _e239;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_41), (&param_42), (&param_43), (&param_44), (&param_45), (&param_46), (&param_47));
    let _e240 = param_43;
    sum_3 = _e240;
    let _e241 = param_44;
    neighborhoodMin = _e241;
    let _e242 = param_45;
    neighborhoodMax = _e242;
    let _e243 = param_46;
    momentSum = _e243;
    let _e244 = param_47;
    momentSquares = _e244;
    let _e245 = (*tileIndex_1);
    let _e249 = wx[2u];
    let _e251 = wy[0u];
    param_48 = ((_e245 - 10i) + 1i);
    param_49 = (_e249 * _e251);
    let _e253 = sum_3;
    param_50 = _e253;
    let _e254 = neighborhoodMin;
    param_51 = _e254;
    let _e255 = neighborhoodMax;
    param_52 = _e255;
    let _e256 = momentSum;
    param_53 = _e256;
    let _e257 = momentSquares;
    param_54 = _e257;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_48), (&param_49), (&param_50), (&param_51), (&param_52), (&param_53), (&param_54));
    let _e258 = param_50;
    sum_3 = _e258;
    let _e259 = param_51;
    neighborhoodMin = _e259;
    let _e260 = param_52;
    neighborhoodMax = _e260;
    let _e261 = param_53;
    momentSum = _e261;
    let _e262 = param_54;
    momentSquares = _e262;
    let _e263 = (*tileIndex_1);
    let _e266 = wx[0u];
    let _e268 = wy[1u];
    param_55 = (_e263 - 1i);
    param_56 = (_e266 * _e268);
    let _e270 = sum_3;
    param_57 = _e270;
    let _e271 = neighborhoodMin;
    param_58 = _e271;
    let _e272 = neighborhoodMax;
    param_59 = _e272;
    let _e273 = momentSum;
    param_60 = _e273;
    let _e274 = momentSquares;
    param_61 = _e274;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_55), (&param_56), (&param_57), (&param_58), (&param_59), (&param_60), (&param_61));
    let _e275 = param_57;
    sum_3 = _e275;
    let _e276 = param_58;
    neighborhoodMin = _e276;
    let _e277 = param_59;
    neighborhoodMax = _e277;
    let _e278 = param_60;
    momentSum = _e278;
    let _e279 = param_61;
    momentSquares = _e279;
    let _e281 = wx[1u];
    let _e283 = wy[1u];
    let _e285 = (*tileIndex_1);
    param_62 = _e285;
    param_63 = (_e281 * _e283);
    let _e286 = sum_3;
    param_64 = _e286;
    let _e287 = neighborhoodMin;
    param_65 = _e287;
    let _e288 = neighborhoodMax;
    param_66 = _e288;
    let _e289 = momentSum;
    param_67 = _e289;
    let _e290 = momentSquares;
    param_68 = _e290;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_62), (&param_63), (&param_64), (&param_65), (&param_66), (&param_67), (&param_68));
    let _e291 = param_64;
    sum_3 = _e291;
    let _e292 = param_65;
    neighborhoodMin = _e292;
    let _e293 = param_66;
    neighborhoodMax = _e293;
    let _e294 = param_67;
    momentSum = _e294;
    let _e295 = param_68;
    momentSquares = _e295;
    let _e296 = (*tileIndex_1);
    let _e299 = wx[2u];
    let _e301 = wy[1u];
    param_69 = (_e296 + 1i);
    param_70 = (_e299 * _e301);
    let _e303 = sum_3;
    param_71 = _e303;
    let _e304 = neighborhoodMin;
    param_72 = _e304;
    let _e305 = neighborhoodMax;
    param_73 = _e305;
    let _e306 = momentSum;
    param_74 = _e306;
    let _e307 = momentSquares;
    param_75 = _e307;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_69), (&param_70), (&param_71), (&param_72), (&param_73), (&param_74), (&param_75));
    let _e308 = param_71;
    sum_3 = _e308;
    let _e309 = param_72;
    neighborhoodMin = _e309;
    let _e310 = param_73;
    neighborhoodMax = _e310;
    let _e311 = param_74;
    momentSum = _e311;
    let _e312 = param_75;
    momentSquares = _e312;
    let _e313 = (*tileIndex_1);
    let _e317 = wx[0u];
    let _e319 = wy[2u];
    param_76 = ((_e313 + 10i) - 1i);
    param_77 = (_e317 * _e319);
    let _e321 = sum_3;
    param_78 = _e321;
    let _e322 = neighborhoodMin;
    param_79 = _e322;
    let _e323 = neighborhoodMax;
    param_80 = _e323;
    let _e324 = momentSum;
    param_81 = _e324;
    let _e325 = momentSquares;
    param_82 = _e325;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_76), (&param_77), (&param_78), (&param_79), (&param_80), (&param_81), (&param_82));
    let _e326 = param_78;
    sum_3 = _e326;
    let _e327 = param_79;
    neighborhoodMin = _e327;
    let _e328 = param_80;
    neighborhoodMax = _e328;
    let _e329 = param_81;
    momentSum = _e329;
    let _e330 = param_82;
    momentSquares = _e330;
    let _e331 = (*tileIndex_1);
    let _e334 = wx[1u];
    let _e336 = wy[2u];
    param_83 = (_e331 + 10i);
    param_84 = (_e334 * _e336);
    let _e338 = sum_3;
    param_85 = _e338;
    let _e339 = neighborhoodMin;
    param_86 = _e339;
    let _e340 = neighborhoodMax;
    param_87 = _e340;
    let _e341 = momentSum;
    param_88 = _e341;
    let _e342 = momentSquares;
    param_89 = _e342;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_83), (&param_84), (&param_85), (&param_86), (&param_87), (&param_88), (&param_89));
    let _e343 = param_85;
    sum_3 = _e343;
    let _e344 = param_86;
    neighborhoodMin = _e344;
    let _e345 = param_87;
    neighborhoodMax = _e345;
    let _e346 = param_88;
    momentSum = _e346;
    let _e347 = param_89;
    momentSquares = _e347;
    let _e348 = (*tileIndex_1);
    let _e352 = wx[2u];
    let _e354 = wy[2u];
    param_90 = ((_e348 + 10i) + 1i);
    param_91 = (_e352 * _e354);
    let _e356 = sum_3;
    param_92 = _e356;
    let _e357 = neighborhoodMin;
    param_93 = _e357;
    let _e358 = neighborhoodMax;
    param_94 = _e358;
    let _e359 = momentSum;
    param_95 = _e359;
    let _e360 = momentSquares;
    param_96 = _e360;
    accumulateTap_u0028_i1_u003b_f1_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_90), (&param_91), (&param_92), (&param_93), (&param_94), (&param_95), (&param_96));
    let _e361 = param_92;
    sum_3 = _e361;
    let _e362 = param_93;
    neighborhoodMin = _e362;
    let _e363 = param_94;
    neighborhoodMax = _e363;
    let _e364 = param_95;
    momentSum = _e364;
    let _e365 = param_96;
    momentSquares = _e365;
    let _e366 = neighborhoodMin;
    (*boxMin) = _e366;
    let _e367 = neighborhoodMax;
    (*boxMax) = _e367;
    let _e369 = pc.qualityFlags;
    if ((_e369 & 1u) != 0u) {
        let _e372 = momentSum;
        momentMean = (_e372 * 0.11111111f);
        let _e374 = momentSquares;
        let _e376 = momentMean;
        let _e377 = momentMean;
        momentSigma = sqrt(max(((_e374 * 0.11111111f) - (_e376 * _e377)), vec3<f32>(0f, 0f, 0f)));
        let _e382 = neighborhoodMin;
        let _e383 = momentMean;
        let _e384 = momentSigma;
        (*boxMin) = max(_e382, (_e383 - (_e384 * 1.25f)));
        let _e388 = neighborhoodMax;
        let _e389 = momentMean;
        let _e390 = momentSigma;
        (*boxMax) = min(_e388, (_e389 + (_e390 * 1.25f)));
    }
    let _e394 = sum_3;
    let _e395 = neighborhoodMin;
    let _e396 = neighborhoodMax;
    let _e397 = clamp(_e394, _e395, _e396);
    let _e398 = (*tileIndex_1);
    let _e401 = tileColor[_e398][3u];
    return vec4<f32>(_e397.x, _e397.y, _e397.z, _e401);
}

fn main_1() {
    var pixel_1: vec2<i32>;
    var tileOrigin: vec2<i32>;
    var imageHi: vec2<i32>;
    var i_3: u32;
    var q: vec2<i32>;
    var tileIndex_2: i32;
    var current: vec4<f32>;
    var clipMin: vec3<f32>;
    var clipMax: vec3<f32>;
    var param_97: i32;
    var param_98: vec3<f32>;
    var param_99: vec3<f32>;
    var result_2: vec4<f32>;
    var displayed: vec4<f32>;
    var usedHistory: bool;
    var valid: f32;
    var velocity: vec2<f32>;
    var param_100: vec2<i32>;
    var param_101: i32;
    var param_102: f32;
    var motionTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_103: vec4<f32>;
    var param_104: f32;
    var param_105: vec2<f32>;
    var param_106: vec2<f32>;
    var currentDevice: f32;
    var currentLinear: f32;
    var param_107: f32;
    var priorLinear: f32;
    var param_108: vec2<f32>;
    var depthLimit: f32;
    var param_109: f32;
    var param_110: f32;
    var prior: vec4<f32>;
    var param_111: vec2<f32>;
    var param_112: vec4<f32>;
    var param_113: vec3<f32>;
    var param_114: vec3<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var baseWeight: f32;
    var weight_2: f32;
    var param_115: f32;
    var param_116: f32;
    var resolvedRgb: vec3<f32>;
    var param_117: vec3<f32>;
    var param_118: vec3<f32>;
    var param_119: f32;
    var detail: vec3<f32>;
    var param_120: vec2<f32>;
    var param_121: vec4<f32>;
    var param_122: vec2<i32>;
    var param_123: vec4<f32>;
    var param_124: vec4<f32>;
    var param_125: vec4<f32>;
    var param_126: f32;
    var phi_1765_: bool;
    var phi_1774_: bool;
    var phi_1783_: bool;
    var phi_1789_: bool;
    var phi_1825_: bool;
    var phi_1840_: bool;
    var phi_1855_: bool;
    var phi_1861_: bool;
    var phi_1941_: bool;
    var phi_1949_: bool;
    var phi_1972_: bool;

    let _e145 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e145.xy);
    let _e148 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e148.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e155 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e155) - vec2<i32>(1i, 1i));
    let _e158 = gl_LocalInvocationIndex_1;
    i_3 = _e158;
    loop {
        let _e159 = i_3;
        if (_e159 < 100u) {
            let _e161 = tileOrigin;
            let _e162 = i_3;
            let _e165 = i_3;
            let _e170 = imageHi;
            q = clamp((_e161 + vec2<i32>(bitcast<i32>((_e162 % 10u)), bitcast<i32>((_e165 / 10u)))), vec2<i32>(0i, 0i), _e170);
            let _e172 = i_3;
            let _e173 = q;
            let _e174 = textureLoad(currentColor, _e173, 0i);
            tileColor[_e172] = _e174;
            let _e176 = i_3;
            let _e177 = q;
            let _e178 = textureLoad(currentDeviceDepth, _e177, 0i);
            tileDepth[_e176] = _e178.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e181 = i_3;
            i_3 = (_e181 + 64u);
        }
    }
    workgroupBarrier();
    let _e183 = pixel_1;
    let _e185 = pc.extent;
    if any((_e183 >= bitcast<vec2<i32>>(_e185))) {
        return;
    }
    let _e190 = gl_LocalInvocationID_1[1u];
    let _e195 = gl_LocalInvocationID_1[0u];
    tileIndex_2 = ((((bitcast<i32>(_e190) + 1i) * 10i) + bitcast<i32>(_e195)) + 1i);
    let _e199 = tileIndex_2;
    param_97 = _e199;
    let _e200 = reconstructCurrent_u0028_i1_u003b_vf3_u003b_vf3_u003b((&param_97), (&param_98), (&param_99));
    let _e201 = param_98;
    clipMin = _e201;
    let _e202 = param_99;
    clipMax = _e202;
    current = _e200;
    let _e203 = current;
    result_2 = _e203;
    let _e204 = current;
    displayed = _e204;
    usedHistory = false;
    let _e205 = pixel_1;
    let _e206 = textureLoad(motionValidity, _e205, 0i);
    valid = _e206.x;
    let _e208 = pixel_1;
    param_100 = _e208;
    let _e209 = tileIndex_2;
    param_101 = _e209;
    let _e210 = valid;
    param_102 = _e210;
    let _e211 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_100), (&param_101), (&param_102));
    velocity = _e211;
    let _e212 = velocity;
    let _e214 = pc.extent;
    motionTexel = (_e212 * vec2<f32>(_e214));
    let _e217 = pixel_1;
    let _e219 = motionTexel;
    previousTexel = (vec2<f32>(_e217) - _e219);
    let _e221 = previousTexel;
    previousCenter = (_e221 + vec2(0.5f));
    let _e224 = previousCenter;
    let _e226 = all((_e224 >= vec2<f32>(0f, 0f)));
    phi_1765_ = _e226;
    if _e226 {
        let _e227 = previousCenter;
        let _e229 = pc.extent;
        phi_1765_ = all((_e227 < vec2<f32>(_e229)));
    }
    let _e234 = phi_1765_;
    inside = _e234;
    let _e235 = current;
    param_103 = _e235;
    let _e236 = finite4_u0028_vf4_u003b((&param_103));
    phi_1774_ = _e236;
    if _e236 {
        let _e237 = valid;
        param_104 = _e237;
        let _e238 = finite1_u0028_f1_u003b((&param_104));
        phi_1774_ = _e238;
    }
    let _e240 = phi_1774_;
    let _e241 = valid;
    let _e243 = (_e240 && (_e241 > 0f));
    phi_1783_ = _e243;
    if _e243 {
        let _e244 = velocity;
        param_105 = _e244;
        let _e245 = finite2_u0028_vf2_u003b((&param_105));
        phi_1783_ = _e245;
    }
    let _e247 = phi_1783_;
    phi_1789_ = _e247;
    if _e247 {
        let _e248 = previousTexel;
        param_106 = _e248;
        let _e249 = finite2_u0028_vf2_u003b((&param_106));
        phi_1789_ = _e249;
    }
    let _e251 = phi_1789_;
    let _e252 = inside;
    if (_e251 && _e252) {
        let _e254 = tileIndex_2;
        let _e256 = tileDepth[_e254];
        currentDevice = _e256;
        let _e257 = currentDevice;
        param_107 = _e257;
        let _e258 = linearizeDepth_u0028_f1_u003b((&param_107));
        currentLinear = _e258;
        let _e259 = previousTexel;
        param_108 = _e259;
        let _e260 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_108));
        priorLinear = _e260;
        let _e262 = pc.depthThresholdAbsolute;
        let _e264 = pc.depthThresholdRelative;
        let _e265 = currentLinear;
        let _e266 = priorLinear;
        depthLimit = max(_e262, (_e264 * max(_e265, _e266)));
        let _e270 = currentLinear;
        param_109 = _e270;
        let _e271 = finite1_u0028_f1_u003b((&param_109));
        phi_1825_ = _e271;
        if _e271 {
            let _e272 = priorLinear;
            param_110 = _e272;
            let _e273 = finite1_u0028_f1_u003b((&param_110));
            phi_1825_ = _e273;
        }
        let _e275 = phi_1825_;
        let _e276 = currentLinear;
        let _e279 = priorLinear;
        let _e281 = ((_e275 && (_e276 > 0f)) && (_e279 > 0f));
        phi_1840_ = _e281;
        if _e281 {
            let _e282 = currentLinear;
            let _e283 = priorLinear;
            let _e286 = depthLimit;
            phi_1840_ = (abs((_e282 - _e283)) <= _e286);
        }
        let _e289 = phi_1840_;
        if _e289 {
            let _e290 = previousTexel;
            param_111 = _e290;
            let _e291 = samplePreviousColor_u0028_vf2_u003b((&param_111));
            prior = _e291;
            let _e292 = prior;
            param_112 = _e292;
            let _e293 = finite4_u0028_vf4_u003b((&param_112));
            phi_1855_ = _e293;
            if _e293 {
                let _e294 = clipMin;
                param_113 = _e294;
                let _e295 = finite3_u0028_vf3_u003b((&param_113));
                phi_1855_ = _e295;
            }
            let _e297 = phi_1855_;
            phi_1861_ = _e297;
            if _e297 {
                let _e298 = clipMax;
                param_114 = _e298;
                let _e299 = finite3_u0028_vf3_u003b((&param_114));
                phi_1861_ = _e299;
            }
            let _e301 = phi_1861_;
            if _e301 {
                let _e302 = prior;
                let _e304 = clipMin;
                let _e305 = clipMax;
                clampedPrior = clamp(_e302.xyz, _e304, _e305);
                let _e307 = valid;
                confidence_1 = clamp(_e307, 0f, 1f);
                let _e310 = pc.historyWeight;
                baseWeight = clamp(_e310, 0f, 1f);
                let _e312 = baseWeight;
                param_115 = _e312;
                let _e313 = confidence_1;
                param_116 = _e313;
                let _e314 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_115), (&param_116));
                weight_2 = _e314;
                let _e315 = current;
                param_117 = _e315.xyz;
                let _e317 = clampedPrior;
                param_118 = _e317;
                let _e318 = weight_2;
                param_119 = _e318;
                let _e319 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_117), (&param_118), (&param_119));
                resolvedRgb = _e319;
                let _e320 = resolvedRgb;
                let _e322 = current[3u];
                result_2 = vec4<f32>(_e320.x, _e320.y, _e320.z, _e322);
                let _e328 = pc.qualityFlags;
                if ((_e328 & 8u) == 0u) {
                    let _e331 = prior;
                    let _e333 = previousTexel;
                    param_120 = _e333;
                    let _e334 = samplePreviousBlur_u0028_vf2_u003b((&param_120));
                    detail = (_e331.xyz - _e334);
                    let _e336 = resolvedRgb;
                    let _e337 = confidence_1;
                    let _e339 = detail;
                    let _e342 = resolvedRgb;
                    let _e343 = clipMin;
                    let _e345 = resolvedRgb;
                    let _e346 = clipMax;
                    let _e348 = clamp((_e336 + (_e339 * (0.5f * _e337))), min(_e342, _e343), max(_e345, _e346));
                    let _e350 = current[3u];
                    displayed = vec4<f32>(_e348.x, _e348.y, _e348.z, _e350);
                }
                usedHistory = true;
            }
        }
    }
    let _e355 = usedHistory;
    let _e356 = !(_e355);
    phi_1941_ = _e356;
    if _e356 {
        let _e357 = current;
        param_121 = _e357;
        let _e358 = finite4_u0028_vf4_u003b((&param_121));
        phi_1941_ = _e358;
    }
    let _e360 = phi_1941_;
    phi_1949_ = _e360;
    if _e360 {
        let _e362 = pc.qualityFlags;
        phi_1949_ = ((_e362 & 4u) != 0u);
    }
    let _e366 = phi_1949_;
    if _e366 {
        let _e367 = pixel_1;
        param_122 = _e367;
        let _e368 = current;
        param_123 = _e368;
        let _e369 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_122), (&param_123));
        result_2 = _e369;
    }
    let _e370 = result_2;
    param_124 = _e370;
    let _e371 = finite4_u0028_vf4_u003b((&param_124));
    if !(_e371) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e373 = usedHistory;
    let _e374 = !(_e373);
    phi_1972_ = _e374;
    if !(_e374) {
        let _e376 = displayed;
        param_125 = _e376;
        let _e377 = finite4_u0028_vf4_u003b((&param_125));
        phi_1972_ = !(_e377);
    }
    let _e380 = phi_1972_;
    if _e380 {
        let _e381 = result_2;
        displayed = _e381;
    }
    let _e382 = pixel_1;
    let _e383 = displayed;
    textureStore(resolvedColor, _e382, _e383);
    let _e384 = pixel_1;
    let _e385 = result_2;
    textureStore(historyOutColor, _e384, _e385);
    let _e386 = pixel_1;
    let _e387 = tileIndex_2;
    let _e389 = tileDepth[_e387];
    param_126 = _e389;
    let _e390 = storeLinearDepth_u0028_f1_u003b((&param_126));
    textureStore(historyOutLinearDepth, _e386, vec4(_e390));
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
