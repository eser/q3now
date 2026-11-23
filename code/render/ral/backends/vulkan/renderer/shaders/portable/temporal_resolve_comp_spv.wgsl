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

    let _e88 = pc.zNear;
    let _e89 = (*z);
    let _e91 = pc.zFar;
    let _e93 = pc.zNear;
    denominator = (_e88 + (_e89 * (_e91 - _e93)));
    let _e98 = pc.zNear;
    let _e100 = pc.zFar;
    let _e102 = denominator;
    let _e103 = denominator;
    return ((_e98 * _e100) * inverseSqrt((_e102 * _e103)));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e87 = (*pixel);
    let _e89 = pc.extent;
    bounded = clamp(_e87, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e89) - vec2<i32>(1i, 1i)));
    let _e93 = bounded;
    let _e94 = textureLoad(currentColor, _e93, 0i);
    return _e94;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_110_: bool;

    let _e86 = (*v);
    let _e87 = (*v);
    let _e88 = (_e86 == _e87);
    phi_110_ = _e88;
    if _e88 {
        let _e89 = (*v);
        phi_110_ = (abs(_e89) <= 0x1.fffffep+127f);
    }
    let _e93 = phi_110_;
    return _e93;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e88 = (*c);
    l = dot(_e88, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e90 = l;
    param = _e90;
    let _e91 = finite1_u0028_f1_u003b((&param));
    if !(_e91) {
        return 0f;
    }
    let _e93 = l;
    l = clamp(_e93, 0f, 10000f);
    let _e95 = l;
    let _e96 = l;
    let _e97 = l;
    return (_e95 * inverseSqrt((1f + (_e96 * _e97))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e88 = (*p);
    param_1 = _e88;
    let _e89 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e89.xyz;
    let _e91 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e91;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e89 = (*base);
    param_3 = _e89;
    let _e90 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e91 = (*base);
    let _e92 = (*across);
    param_4 = (_e91 + _e92);
    let _e94 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e90 + _e94));
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

    let _e158 = (*centerColor);
    param_5 = _e158.xyz;
    let _e160 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e160;
    let _e161 = (*px);
    param_6 = (_e161 + vec2<i32>(0i, -1i));
    let _e163 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e163;
    let _e164 = (*px);
    param_7 = (_e164 + vec2<i32>(0i, 1i));
    let _e166 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e166;
    let _e167 = (*px);
    param_8 = (_e167 + vec2<i32>(-1i, 0i));
    let _e169 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e169;
    let _e170 = (*px);
    param_9 = (_e170 + vec2<i32>(1i, 0i));
    let _e172 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e172;
    let _e173 = fxN;
    let _e174 = fxS;
    let _e176 = fxW;
    let _e177 = fxE;
    let _e179 = fxM;
    fxHi = max(max(_e173, _e174), max(max(_e176, _e177), _e179));
    let _e182 = fxN;
    let _e183 = fxS;
    let _e185 = fxW;
    let _e186 = fxE;
    let _e188 = fxM;
    fxLo = min(min(_e182, _e183), min(min(_e185, _e186), _e188));
    let _e191 = fxHi;
    let _e192 = fxLo;
    fxSpan = (_e191 - _e192);
    let _e194 = fxSpan;
    let _e195 = fxHi;
    if (_e194 < max(0.0312f, (_e195 * 0.125f))) {
        let _e199 = (*centerColor);
        return _e199;
    }
    let _e200 = (*px);
    param_10 = (_e200 + vec2<i32>(-1i, -1i));
    let _e202 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e202;
    let _e203 = (*px);
    param_11 = (_e203 + vec2<i32>(1i, -1i));
    let _e205 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e205;
    let _e206 = (*px);
    param_12 = (_e206 + vec2<i32>(-1i, 1i));
    let _e208 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e208;
    let _e209 = (*px);
    param_13 = (_e209 + vec2<i32>(1i, 1i));
    let _e211 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e211;
    let _e212 = fxN;
    let _e213 = fxS;
    fxNS = (_e212 + _e213);
    let _e215 = fxW;
    let _e216 = fxE;
    fxWE = (_e215 + _e216);
    let _e218 = fxNW;
    let _e219 = fxSW;
    fxNWSW = (_e218 + _e219);
    let _e221 = fxNE;
    let _e222 = fxSE;
    fxNESE = (_e221 + _e222);
    let _e224 = fxW;
    let _e226 = fxNWSW;
    let _e229 = fxM;
    let _e231 = fxNS;
    let _e235 = fxE;
    let _e237 = fxNESE;
    fxHorz = (abs(((-2f * _e224) + _e226)) + ((abs(((-2f * _e229) + _e231)) * 2f) + abs(((-2f * _e235) + _e237))));
    let _e242 = fxS;
    let _e244 = fxSW;
    let _e245 = fxSE;
    let _e249 = fxM;
    let _e251 = fxWE;
    let _e255 = fxN;
    let _e257 = fxNW;
    let _e258 = fxNE;
    fxVert = (abs(((-2f * _e242) + (_e244 + _e245))) + ((abs(((-2f * _e249) + _e251)) * 2f) + abs(((-2f * _e255) + (_e257 + _e258)))));
    let _e264 = fxHorz;
    let _e265 = fxVert;
    fxHorzSpan = (_e264 >= _e265);
    let _e267 = fxNS;
    let _e268 = fxWE;
    let _e271 = fxNWSW;
    let _e272 = fxNESE;
    fxAround = (((_e267 + _e268) * 2f) + (_e271 + _e272));
    let _e275 = fxHorzSpan;
    let _e276 = fxN;
    let _e277 = fxW;
    fxLoSide = select(_e277, _e276, _e275);
    let _e279 = fxHorzSpan;
    let _e280 = fxS;
    let _e281 = fxE;
    fxHiSide = select(_e281, _e280, _e279);
    let _e283 = fxLoSide;
    let _e284 = fxM;
    fxGradLo = (_e283 - _e284);
    let _e286 = fxHiSide;
    let _e287 = fxM;
    fxGradHi = (_e286 - _e287);
    let _e289 = fxGradLo;
    let _e291 = fxGradHi;
    fxLoSteeper = (abs(_e289) >= abs(_e291));
    let _e294 = fxGradLo;
    let _e296 = fxGradHi;
    fxGradient = max(abs(_e294), abs(_e296));
    let _e299 = fxLoSteeper;
    if _e299 {
        let _e300 = fxLoSide;
        let _e301 = fxM;
        local = (_e300 + _e301);
    } else {
        let _e303 = fxHiSide;
        let _e304 = fxM;
        local = (_e303 + _e304);
    }
    let _e306 = local;
    fxPair = _e306;
    let _e307 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e307));
    let _e310 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e310));
    let _e313 = fxLoSteeper;
    if _e313 {
        let _e314 = fxAcross;
        fxAcross = -(_e314);
    }
    let _e316 = fxSpan;
    let _e317 = fxSpan;
    fxInvSpan = inverseSqrt((_e316 * _e317));
    let _e320 = fxAround;
    let _e322 = fxM;
    let _e325 = fxInvSpan;
    fxBlendC = clamp((abs(((_e320 * 0.083333336f) - _e322)) * _e325), 0f, 1f);
    let _e328 = fxBlendC;
    let _e331 = fxBlendC;
    let _e332 = fxBlendC;
    fxBlendF = (((-2f * _e328) + 3f) * (_e331 * _e332));
    let _e335 = fxBlendF;
    let _e336 = fxBlendF;
    fxSubpixel = ((_e335 * _e336) * 0.75f);
    let _e339 = fxPair;
    fxHalfPair = (_e339 * 0.5f);
    let _e341 = fxGradient;
    fxScaled = (_e341 * 0.25f);
    let _e343 = fxM;
    let _e344 = fxHalfPair;
    fxMidNegative = ((_e343 - _e344) < 0f);
    let _e347 = (*px);
    let _e348 = fxAlong;
    param_14 = (_e347 - _e348);
    let _e350 = fxAcross;
    param_15 = _e350;
    let _e351 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e352 = fxHalfPair;
    fxEndN = (_e351 - _e352);
    let _e354 = (*px);
    let _e355 = fxAlong;
    param_16 = (_e354 + _e355);
    let _e357 = fxAcross;
    param_17 = _e357;
    let _e358 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e359 = fxHalfPair;
    fxEndP = (_e358 - _e359);
    let _e361 = fxEndN;
    let _e363 = fxScaled;
    fxDoneN = (abs(_e361) >= _e363);
    let _e365 = fxEndP;
    let _e367 = fxScaled;
    fxDoneP = (abs(_e365) >= _e367);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e369 = i;
        if (_e369 <= 12i) {
            let _e371 = fxDoneN;
            if !(_e371) {
                let _e373 = (*px);
                let _e374 = fxAlong;
                let _e375 = i;
                param_18 = (_e373 - (_e374 * vec2(_e375)));
                let _e379 = fxAcross;
                param_19 = _e379;
                let _e380 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e381 = fxHalfPair;
                fxEndN = (_e380 - _e381);
                let _e383 = fxEndN;
                let _e385 = fxScaled;
                fxDoneN = (abs(_e383) >= _e385);
                let _e387 = i;
                fxStepsN = _e387;
            }
            let _e388 = fxDoneP;
            if !(_e388) {
                let _e390 = (*px);
                let _e391 = fxAlong;
                let _e392 = i;
                param_20 = (_e390 + (_e391 * vec2(_e392)));
                let _e396 = fxAcross;
                param_21 = _e396;
                let _e397 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e398 = fxHalfPair;
                fxEndP = (_e397 - _e398);
                let _e400 = fxEndP;
                let _e402 = fxScaled;
                fxDoneP = (abs(_e400) >= _e402);
                let _e404 = i;
                fxStepsP = _e404;
            }
            let _e405 = fxDoneN;
            let _e406 = fxDoneP;
            if (_e405 && _e406) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e408 = i;
            i = (_e408 + 1i);
        }
    }
    let _e410 = fxStepsN;
    let _e411 = fxStepsP;
    fxTowardN = (_e410 < _e411);
    let _e413 = fxTowardN;
    let _e414 = fxEndN;
    let _e415 = fxEndP;
    fxEndG = select(_e415, _e414, _e413);
    let _e417 = fxEndG;
    let _e419 = fxMidNegative;
    fxGoodSpan = ((_e417 < 0f) != _e419);
    let _e421 = fxStepsN;
    let _e422 = fxStepsP;
    fxSpanLen = f32((_e421 + _e422));
    let _e425 = fxTowardN;
    let _e426 = fxStepsN;
    let _e427 = fxStepsP;
    fxNear = f32(select(_e427, _e426, _e425));
    let _e430 = fxGoodSpan;
    if _e430 {
        let _e431 = fxNear;
        let _e432 = fxSpanLen;
        let _e433 = fxSpanLen;
        local_1 = (0.5f - (_e431 * inverseSqrt((_e432 * _e433))));
    } else {
        local_1 = 0f;
    }
    let _e438 = local_1;
    fxEdgeBlend = _e438;
    let _e439 = fxEdgeBlend;
    let _e440 = fxSubpixel;
    fxBlend = clamp(max(_e439, _e440), 0f, 1f);
    let _e443 = (*px);
    let _e444 = fxAcross;
    param_22 = (_e443 + _e444);
    let _e446 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e446.xyz;
    let _e448 = (*centerColor);
    let _e450 = fxBlend;
    let _e453 = fxAcrossColor;
    let _e454 = fxBlend;
    let _e456 = ((_e448.xyz * (1f - _e450)) + (_e453 * _e454));
    let _e458 = (*centerColor)[3u];
    return vec4<f32>(_e456.x, _e456.y, _e456.z, _e458);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e92 = (*a);
    oneMinus = (1f - _e92);
    let _e94 = (*x);
    let _e95 = oneMinus;
    left = (_e94 * _e95);
    let _e97 = (*y);
    let _e98 = (*a);
    right = (_e97 * _e98);
    let _e100 = left;
    let _e101 = right;
    result = (_e100 + _e101);
    let _e103 = result;
    return _e103;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e88 = (*weight);
    let _e89 = (*confidence);
    scaled = (_e88 * _e89);
    let _e91 = scaled;
    return _e91;
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_140_: bool;

    let _e86 = (*v_1);
    let _e87 = (*v_1);
    let _e89 = all((_e86 == _e87));
    phi_140_ = _e89;
    if _e89 {
        let _e90 = (*v_1);
        phi_140_ = all((abs(_e90) <= vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e95 = phi_140_;
    return _e95;
}

fn samplePreviousColor_u0028_vf2_u003b(texel: ptr<function, vec2<f32>>) -> vec4<f32> {
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
    var extent: vec2<f32>;
    var invExtent: vec2<f32>;
    var uv0_: vec2<f32>;
    var uv12_: vec2<f32>;
    var uv3_: vec2<f32>;
    var ux: array<f32, 5>;
    var uy: array<f32, 5>;
    var w: array<f32, 5>;
    var sum: vec4<f32>;
    var total: f32;
    var i_1: i32;

    let _e109 = (*texel);
    baseFloat = floor(_e109);
    let _e111 = (*texel);
    let _e112 = baseFloat;
    f = (_e111 - _e112);
    let _e114 = f;
    d0_ = (vec2(1f) + _e114);
    let _e117 = f;
    d1_ = _e117;
    let _e118 = f;
    d2_ = (vec2(1f) - _e118);
    let _e121 = f;
    d3_ = (vec2(2f) - _e121);
    let _e124 = d0_;
    let _e128 = d0_;
    let _e132 = d0_;
    w0_ = ((((((_e124 * -0.75f) - vec2(-3.75f)) * _e128) + vec2(-6f)) * _e132) - vec2(-3f));
    let _e136 = d1_;
    let _e140 = d1_;
    let _e142 = d1_;
    w1_ = (((((_e136 * 1.25f) - vec2(2.25f)) * _e140) * _e142) + vec2(1f));
    let _e146 = d2_;
    let _e150 = d2_;
    let _e152 = d2_;
    w2_ = (((((_e146 * 1.25f) - vec2(2.25f)) * _e150) * _e152) + vec2(1f));
    let _e156 = d3_;
    let _e160 = d3_;
    let _e164 = d3_;
    w3_ = ((((((_e156 * -0.75f) - vec2(-3.75f)) * _e160) + vec2(-6f)) * _e164) - vec2(-3f));
    let _e168 = w1_;
    let _e169 = w2_;
    w12_ = (_e168 + _e169);
    let _e171 = w2_;
    let _e172 = w12_;
    let _e173 = w12_;
    offset12_ = (_e171 * inverseSqrt((_e172 * _e173)));
    let _e178 = pc.extent;
    extent = vec2<f32>(_e178);
    let _e180 = extent;
    let _e181 = extent;
    invExtent = inverseSqrt((_e180 * _e181));
    let _e184 = baseFloat;
    let _e189 = invExtent;
    uv0_ = (((_e184 - vec2(1f)) + vec2(0.5f)) * _e189);
    let _e191 = baseFloat;
    let _e192 = offset12_;
    let _e196 = invExtent;
    uv12_ = (((_e191 + _e192) + vec2(0.5f)) * _e196);
    let _e198 = baseFloat;
    let _e203 = invExtent;
    uv3_ = (((_e198 + vec2(2f)) + vec2(0.5f)) * _e203);
    let _e206 = uv12_[0u];
    let _e208 = uv0_[0u];
    let _e210 = uv3_[0u];
    let _e212 = uv12_[0u];
    let _e214 = uv12_[0u];
    ux = array<f32, 5>(_e206, _e208, _e210, _e212, _e214);
    let _e217 = uv12_[1u];
    let _e219 = uv12_[1u];
    let _e221 = uv12_[1u];
    let _e223 = uv0_[1u];
    let _e225 = uv3_[1u];
    uy = array<f32, 5>(_e217, _e219, _e221, _e223, _e225);
    let _e228 = w12_[0u];
    let _e230 = w12_[1u];
    let _e233 = w0_[0u];
    let _e235 = w12_[1u];
    let _e238 = w3_[0u];
    let _e240 = w12_[1u];
    let _e243 = w12_[0u];
    let _e245 = w0_[1u];
    let _e248 = w12_[0u];
    let _e250 = w3_[1u];
    w = array<f32, 5>((_e228 * _e230), (_e233 * _e235), (_e238 * _e240), (_e243 * _e245), (_e248 * _e250));
    sum = vec4<f32>(0f, 0f, 0f, 0f);
    total = 0f;
    i_1 = 0i;
    loop {
        let _e253 = i_1;
        if (_e253 < 5i) {
            let _e255 = i_1;
            let _e257 = w[_e255];
            let _e258 = i_1;
            let _e260 = ux[_e258];
            let _e261 = i_1;
            let _e263 = uy[_e261];
            let _e265 = textureSampleLevel(previousColor, nearestSampler, vec2<f32>(_e260, _e263), 0f);
            let _e267 = sum;
            sum = (_e267 + (_e265 * _e257));
            let _e269 = i_1;
            let _e271 = w[_e269];
            let _e272 = total;
            total = (_e272 + _e271);
            continue;
        } else {
            break;
        }
        continuing {
            let _e274 = i_1;
            i_1 = (_e274 + 1i);
        }
    }
    let _e276 = sum;
    let _e277 = total;
    let _e278 = total;
    return (_e276 * inverseSqrt((_e277 * _e278)));
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_1: ptr<function, f32>, y_1: ptr<function, f32>, a_1: ptr<function, f32>) -> f32 {
    let _e88 = (*x_1);
    let _e89 = (*a_1);
    let _e92 = (*y_1);
    let _e93 = (*a_1);
    return ((_e88 * (1f - _e89)) + (_e92 * _e93));
}

fn samplePreviousDepthLinear_u0028_vf2_u003b(texel_1: ptr<function, vec2<f32>>) -> f32 {
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

    let _e105 = (*texel_1);
    baseFloat_1 = floor(_e105);
    let _e107 = baseFloat_1;
    base_1 = vec2<i32>(_e107);
    let _e109 = (*texel_1);
    let _e110 = baseFloat_1;
    f_1 = (_e109 - _e110);
    let _e113 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e113) - vec2<i32>(1i, 1i));
    let _e116 = base_1;
    let _e117 = hi;
    let _e119 = textureLoad(previousLinearDepth, clamp(_e116, vec2<i32>(0i, 0i), _e117), 0i);
    d00_ = _e119.x;
    let _e121 = base_1;
    let _e123 = hi;
    let _e125 = textureLoad(previousLinearDepth, clamp((_e121 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e123), 0i);
    d10_ = _e125.x;
    let _e127 = base_1;
    let _e129 = hi;
    let _e131 = textureLoad(previousLinearDepth, clamp((_e127 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e129), 0i);
    d01_ = _e131.x;
    let _e133 = base_1;
    let _e135 = hi;
    let _e137 = textureLoad(previousLinearDepth, clamp((_e133 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e135), 0i);
    d11_ = _e137.x;
    let _e139 = d00_;
    param_23 = _e139;
    let _e140 = d10_;
    param_24 = _e140;
    let _e142 = f_1[0u];
    param_25 = _e142;
    let _e143 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_23), (&param_24), (&param_25));
    top = _e143;
    let _e144 = d01_;
    param_26 = _e144;
    let _e145 = d11_;
    param_27 = _e145;
    let _e147 = f_1[0u];
    param_28 = _e147;
    let _e148 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_26), (&param_27), (&param_28));
    bottom = _e148;
    let _e149 = top;
    param_29 = _e149;
    let _e150 = bottom;
    param_30 = _e150;
    let _e152 = f_1[1u];
    param_31 = _e152;
    let _e153 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_29), (&param_30), (&param_31));
    return _e153;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator_1: f32;
    var numerator: f32;
    var result_1: f32;

    let _e92 = pc.zFar;
    let _e94 = pc.zNear;
    range = (_e92 - _e94);
    let _e96 = (*z_1);
    let _e97 = range;
    scaled_1 = (_e96 * _e97);
    let _e100 = pc.zNear;
    let _e101 = scaled_1;
    denominator_1 = (_e100 + _e101);
    let _e104 = pc.zNear;
    let _e106 = pc.zFar;
    numerator = (_e104 * _e106);
    let _e108 = numerator;
    let _e109 = denominator_1;
    result_1 = (_e108 / _e109);
    let _e111 = result_1;
    return _e111;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_125_: bool;

    let _e86 = (*v_2);
    let _e87 = (*v_2);
    let _e89 = all((_e86 == _e87));
    phi_125_ = _e89;
    if _e89 {
        let _e90 = (*v_2);
        phi_125_ = all((abs(_e90) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e95 = phi_125_;
    return _e95;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_155_: bool;

    let _e86 = (*v_3);
    let _e87 = (*v_3);
    let _e89 = all((_e86 == _e87));
    phi_155_ = _e89;
    if _e89 {
        let _e90 = (*v_3);
        phi_155_ = all((abs(_e90) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e95 = phi_155_;
    return _e95;
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
    var phi_1078_: bool;
    var phi_1086_: bool;

    let _e99 = (*px_1);
    let _e100 = textureLoad(motionVelocity, _e99, 0i);
    centerVelocity = _e100.xy;
    let _e103 = pc.qualityFlags;
    let _e105 = ((_e103 & 2u) == 0u);
    phi_1078_ = _e105;
    if !(_e105) {
        let _e107 = (*centerValid);
        phi_1078_ = !((_e107 > 0f));
    }
    let _e111 = phi_1078_;
    phi_1086_ = _e111;
    if !(_e111) {
        let _e113 = centerVelocity;
        param_32 = _e113;
        let _e114 = finite2_u0028_vf2_u003b((&param_32));
        phi_1086_ = !(_e114);
    }
    let _e117 = phi_1086_;
    if _e117 {
        let _e118 = centerVelocity;
        return _e118;
    }
    let _e120 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e120) - vec2<i32>(1i, 1i));
    let _e123 = (*px_1);
    best = _e123;
    let _e124 = (*tileIndex);
    let _e126 = tileDepth[_e124];
    bestDepth = _e126;
    y_2 = -1i;
    loop {
        let _e127 = y_2;
        if (_e127 <= 1i) {
            x_2 = -1i;
            loop {
                let _e129 = x_2;
                if (_e129 <= 1i) {
                    let _e131 = (*tileIndex);
                    let _e132 = y_2;
                    let _e135 = x_2;
                    let _e138 = tileDepth[((_e131 + (_e132 * 10i)) + _e135)];
                    d = _e138;
                    let _e139 = d;
                    let _e140 = bestDepth;
                    if (_e139 > _e140) {
                        let _e142 = d;
                        bestDepth = _e142;
                        let _e143 = (*px_1);
                        let _e144 = x_2;
                        let _e145 = y_2;
                        let _e148 = hi_1;
                        best = clamp((_e143 + vec2<i32>(_e144, _e145)), vec2<i32>(0i, 0i), _e148);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e150 = x_2;
                    x_2 = (_e150 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e152 = y_2;
            y_2 = (_e152 + 1i);
        }
    }
    let _e154 = best;
    let _e155 = (*px_1);
    if all((_e154 == _e155)) {
        let _e158 = centerVelocity;
        return _e158;
    }
    let _e159 = best;
    let _e160 = textureLoad(motionValidity, _e159, 0i);
    v_4 = _e160.x;
    let _e162 = v_4;
    if !((_e162 > 0f)) {
        let _e165 = centerVelocity;
        return _e165;
    }
    let _e166 = best;
    let _e167 = textureLoad(motionVelocity, _e166, 0i);
    dv = _e167.xy;
    let _e169 = dv;
    param_33 = _e169;
    let _e170 = finite2_u0028_vf2_u003b((&param_33));
    let _e171 = dv;
    let _e172 = centerVelocity;
    return select(_e172, _e171, vec2(_e170));
}

fn reconstructCurrent_u0028_i1_u003b_vf3_u003b_vf3_u003b(tileIndex_1: ptr<function, i32>, boxMin: ptr<function, vec3<f32>>, boxMax: ptr<function, vec3<f32>>) -> vec4<f32> {
    var t: vec2<f32>;
    var wx: vec3<f32>;
    var wy: vec3<f32>;
    var sum_1: vec3<f32>;
    var neighborhoodMin: vec3<f32>;
    var neighborhoodMax: vec3<f32>;
    var momentSum: vec3<f32>;
    var momentSquares: vec3<f32>;
    var y_3: i32;
    var x_3: i32;
    var c_1: vec3<f32>;
    var momentMean: vec3<f32>;
    var momentSigma: vec3<f32>;

    let _e102 = pc.currentEffectiveJitterUv;
    let _e104 = pc.extent;
    t = (_e102 * vec2<f32>(_e104));
    let _e108 = t[0u];
    let _e111 = t[0u];
    let _e115 = t[0u];
    let _e117 = t[0u];
    let _e121 = t[0u];
    let _e124 = t[0u];
    wx = vec3<f32>(((0.5f * _e108) * (_e111 - 1f)), (1f - (_e115 * _e117)), ((0.5f * _e121) * (_e124 + 1f)));
    let _e129 = t[1u];
    let _e132 = t[1u];
    let _e136 = t[1u];
    let _e138 = t[1u];
    let _e142 = t[1u];
    let _e145 = t[1u];
    wy = vec3<f32>(((0.5f * _e129) * (_e132 - 1f)), (1f - (_e136 * _e138)), ((0.5f * _e142) * (_e145 + 1f)));
    sum_1 = vec3<f32>(0f, 0f, 0f);
    neighborhoodMin = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
    neighborhoodMax = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
    momentSum = vec3<f32>(0f, 0f, 0f);
    momentSquares = vec3<f32>(0f, 0f, 0f);
    y_3 = -1i;
    loop {
        let _e149 = y_3;
        if (_e149 <= 1i) {
            x_3 = -1i;
            loop {
                let _e151 = x_3;
                if (_e151 <= 1i) {
                    let _e153 = (*tileIndex_1);
                    let _e154 = y_3;
                    let _e157 = x_3;
                    let _e160 = tileColor[((_e153 + (_e154 * 10i)) + _e157)];
                    c_1 = _e160.xyz;
                    let _e162 = c_1;
                    let _e163 = x_3;
                    let _e166 = wx[(_e163 + 1i)];
                    let _e167 = y_3;
                    let _e170 = wy[(_e167 + 1i)];
                    let _e173 = sum_1;
                    sum_1 = (_e173 + (_e162 * (_e166 * _e170)));
                    let _e175 = neighborhoodMin;
                    let _e176 = c_1;
                    neighborhoodMin = min(_e175, _e176);
                    let _e178 = neighborhoodMax;
                    let _e179 = c_1;
                    neighborhoodMax = max(_e178, _e179);
                    let _e181 = c_1;
                    let _e182 = momentSum;
                    momentSum = (_e182 + _e181);
                    let _e184 = c_1;
                    let _e185 = c_1;
                    let _e187 = momentSquares;
                    momentSquares = (_e187 + (_e184 * _e185));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e189 = x_3;
                    x_3 = (_e189 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e191 = y_3;
            y_3 = (_e191 + 1i);
        }
    }
    let _e193 = neighborhoodMin;
    (*boxMin) = _e193;
    let _e194 = neighborhoodMax;
    (*boxMax) = _e194;
    let _e196 = pc.qualityFlags;
    if ((_e196 & 1u) != 0u) {
        let _e199 = momentSum;
        momentMean = (_e199 * 0.11111111f);
        let _e201 = momentSquares;
        let _e203 = momentMean;
        let _e204 = momentMean;
        momentSigma = sqrt(max(((_e201 * 0.11111111f) - (_e203 * _e204)), vec3<f32>(0f, 0f, 0f)));
        let _e209 = neighborhoodMin;
        let _e210 = momentMean;
        let _e211 = momentSigma;
        (*boxMin) = max(_e209, (_e210 - (_e211 * 1.25f)));
        let _e215 = neighborhoodMax;
        let _e216 = momentMean;
        let _e217 = momentSigma;
        (*boxMax) = min(_e215, (_e216 + (_e217 * 1.25f)));
    }
    let _e221 = sum_1;
    let _e222 = neighborhoodMin;
    let _e223 = neighborhoodMax;
    let _e224 = clamp(_e221, _e222, _e223);
    let _e225 = (*tileIndex_1);
    let _e228 = tileColor[_e225][3u];
    return vec4<f32>(_e224.x, _e224.y, _e224.z, _e228);
}

fn main_1() {
    var pixel_1: vec2<i32>;
    var tileOrigin: vec2<i32>;
    var imageHi: vec2<i32>;
    var i_2: u32;
    var q: vec2<i32>;
    var tileIndex_2: i32;
    var current: vec4<f32>;
    var clipMin: vec3<f32>;
    var clipMax: vec3<f32>;
    var param_34: i32;
    var param_35: vec3<f32>;
    var param_36: vec3<f32>;
    var result_2: vec4<f32>;
    var usedHistory: bool;
    var valid: f32;
    var velocity: vec2<f32>;
    var param_37: vec2<i32>;
    var param_38: i32;
    var param_39: f32;
    var motionTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_40: vec4<f32>;
    var param_41: f32;
    var param_42: vec2<f32>;
    var param_43: vec2<f32>;
    var currentDevice: f32;
    var currentLinear: f32;
    var param_44: f32;
    var priorLinear: f32;
    var param_45: vec2<f32>;
    var depthLimit: f32;
    var param_46: f32;
    var param_47: f32;
    var prior: vec4<f32>;
    var param_48: vec2<f32>;
    var param_49: vec4<f32>;
    var param_50: vec3<f32>;
    var param_51: vec3<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var baseWeight: f32;
    var weight_1: f32;
    var param_52: f32;
    var param_53: f32;
    var resolvedRgb: vec3<f32>;
    var param_54: vec3<f32>;
    var param_55: vec3<f32>;
    var param_56: f32;
    var param_57: vec4<f32>;
    var param_58: vec2<i32>;
    var param_59: vec4<f32>;
    var param_60: vec4<f32>;
    var param_61: f32;
    var phi_1505_: bool;
    var phi_1514_: bool;
    var phi_1523_: bool;
    var phi_1529_: bool;
    var phi_1565_: bool;
    var phi_1580_: bool;
    var phi_1595_: bool;
    var phi_1601_: bool;
    var phi_1648_: bool;
    var phi_1656_: bool;

    let _e140 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e140.xy);
    let _e143 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e143.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e150 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e150) - vec2<i32>(1i, 1i));
    let _e153 = gl_LocalInvocationIndex_1;
    i_2 = _e153;
    loop {
        let _e154 = i_2;
        if (_e154 < 100u) {
            let _e156 = tileOrigin;
            let _e157 = i_2;
            let _e160 = i_2;
            let _e165 = imageHi;
            q = clamp((_e156 + vec2<i32>(bitcast<i32>((_e157 % 10u)), bitcast<i32>((_e160 / 10u)))), vec2<i32>(0i, 0i), _e165);
            let _e167 = i_2;
            let _e168 = q;
            let _e169 = textureLoad(currentColor, _e168, 0i);
            tileColor[_e167] = _e169;
            let _e171 = i_2;
            let _e172 = q;
            let _e173 = textureLoad(currentDeviceDepth, _e172, 0i);
            tileDepth[_e171] = _e173.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e176 = i_2;
            i_2 = (_e176 + 64u);
        }
    }
    workgroupBarrier();
    let _e178 = pixel_1;
    let _e180 = pc.extent;
    if any((_e178 >= bitcast<vec2<i32>>(_e180))) {
        return;
    }
    let _e185 = gl_LocalInvocationID_1[1u];
    let _e190 = gl_LocalInvocationID_1[0u];
    tileIndex_2 = ((((bitcast<i32>(_e185) + 1i) * 10i) + bitcast<i32>(_e190)) + 1i);
    let _e194 = tileIndex_2;
    param_34 = _e194;
    let _e195 = reconstructCurrent_u0028_i1_u003b_vf3_u003b_vf3_u003b((&param_34), (&param_35), (&param_36));
    let _e196 = param_35;
    clipMin = _e196;
    let _e197 = param_36;
    clipMax = _e197;
    current = _e195;
    let _e198 = current;
    result_2 = _e198;
    usedHistory = false;
    let _e199 = pixel_1;
    let _e200 = textureLoad(motionValidity, _e199, 0i);
    valid = _e200.x;
    let _e202 = pixel_1;
    param_37 = _e202;
    let _e203 = tileIndex_2;
    param_38 = _e203;
    let _e204 = valid;
    param_39 = _e204;
    let _e205 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_37), (&param_38), (&param_39));
    velocity = _e205;
    let _e206 = velocity;
    let _e208 = pc.extent;
    motionTexel = (_e206 * vec2<f32>(_e208));
    let _e211 = pixel_1;
    let _e213 = motionTexel;
    previousTexel = (vec2<f32>(_e211) - _e213);
    let _e215 = previousTexel;
    previousCenter = (_e215 + vec2(0.5f));
    let _e218 = previousCenter;
    let _e220 = all((_e218 >= vec2<f32>(0f, 0f)));
    phi_1505_ = _e220;
    if _e220 {
        let _e221 = previousCenter;
        let _e223 = pc.extent;
        phi_1505_ = all((_e221 < vec2<f32>(_e223)));
    }
    let _e228 = phi_1505_;
    inside = _e228;
    let _e229 = current;
    param_40 = _e229;
    let _e230 = finite4_u0028_vf4_u003b((&param_40));
    phi_1514_ = _e230;
    if _e230 {
        let _e231 = valid;
        param_41 = _e231;
        let _e232 = finite1_u0028_f1_u003b((&param_41));
        phi_1514_ = _e232;
    }
    let _e234 = phi_1514_;
    let _e235 = valid;
    let _e237 = (_e234 && (_e235 > 0f));
    phi_1523_ = _e237;
    if _e237 {
        let _e238 = velocity;
        param_42 = _e238;
        let _e239 = finite2_u0028_vf2_u003b((&param_42));
        phi_1523_ = _e239;
    }
    let _e241 = phi_1523_;
    phi_1529_ = _e241;
    if _e241 {
        let _e242 = previousTexel;
        param_43 = _e242;
        let _e243 = finite2_u0028_vf2_u003b((&param_43));
        phi_1529_ = _e243;
    }
    let _e245 = phi_1529_;
    let _e246 = inside;
    if (_e245 && _e246) {
        let _e248 = tileIndex_2;
        let _e250 = tileDepth[_e248];
        currentDevice = _e250;
        let _e251 = currentDevice;
        param_44 = _e251;
        let _e252 = linearizeDepth_u0028_f1_u003b((&param_44));
        currentLinear = _e252;
        let _e253 = previousTexel;
        param_45 = _e253;
        let _e254 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_45));
        priorLinear = _e254;
        let _e256 = pc.depthThresholdAbsolute;
        let _e258 = pc.depthThresholdRelative;
        let _e259 = currentLinear;
        let _e260 = priorLinear;
        depthLimit = max(_e256, (_e258 * max(_e259, _e260)));
        let _e264 = currentLinear;
        param_46 = _e264;
        let _e265 = finite1_u0028_f1_u003b((&param_46));
        phi_1565_ = _e265;
        if _e265 {
            let _e266 = priorLinear;
            param_47 = _e266;
            let _e267 = finite1_u0028_f1_u003b((&param_47));
            phi_1565_ = _e267;
        }
        let _e269 = phi_1565_;
        let _e270 = currentLinear;
        let _e273 = priorLinear;
        let _e275 = ((_e269 && (_e270 > 0f)) && (_e273 > 0f));
        phi_1580_ = _e275;
        if _e275 {
            let _e276 = currentLinear;
            let _e277 = priorLinear;
            let _e280 = depthLimit;
            phi_1580_ = (abs((_e276 - _e277)) <= _e280);
        }
        let _e283 = phi_1580_;
        if _e283 {
            let _e284 = previousTexel;
            param_48 = _e284;
            let _e285 = samplePreviousColor_u0028_vf2_u003b((&param_48));
            prior = _e285;
            let _e286 = prior;
            param_49 = _e286;
            let _e287 = finite4_u0028_vf4_u003b((&param_49));
            phi_1595_ = _e287;
            if _e287 {
                let _e288 = clipMin;
                param_50 = _e288;
                let _e289 = finite3_u0028_vf3_u003b((&param_50));
                phi_1595_ = _e289;
            }
            let _e291 = phi_1595_;
            phi_1601_ = _e291;
            if _e291 {
                let _e292 = clipMax;
                param_51 = _e292;
                let _e293 = finite3_u0028_vf3_u003b((&param_51));
                phi_1601_ = _e293;
            }
            let _e295 = phi_1601_;
            if _e295 {
                let _e296 = prior;
                let _e298 = clipMin;
                let _e299 = clipMax;
                clampedPrior = clamp(_e296.xyz, _e298, _e299);
                let _e301 = valid;
                confidence_1 = clamp(_e301, 0f, 1f);
                let _e304 = pc.historyWeight;
                baseWeight = clamp(_e304, 0f, 1f);
                let _e306 = baseWeight;
                param_52 = _e306;
                let _e307 = confidence_1;
                param_53 = _e307;
                let _e308 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_52), (&param_53));
                weight_1 = _e308;
                let _e309 = current;
                param_54 = _e309.xyz;
                let _e311 = clampedPrior;
                param_55 = _e311;
                let _e312 = weight_1;
                param_56 = _e312;
                let _e313 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_54), (&param_55), (&param_56));
                resolvedRgb = _e313;
                let _e314 = resolvedRgb;
                let _e316 = current[3u];
                result_2 = vec4<f32>(_e314.x, _e314.y, _e314.z, _e316);
                usedHistory = true;
            }
        }
    }
    let _e321 = usedHistory;
    let _e322 = !(_e321);
    phi_1648_ = _e322;
    if _e322 {
        let _e323 = current;
        param_57 = _e323;
        let _e324 = finite4_u0028_vf4_u003b((&param_57));
        phi_1648_ = _e324;
    }
    let _e326 = phi_1648_;
    phi_1656_ = _e326;
    if _e326 {
        let _e328 = pc.qualityFlags;
        phi_1656_ = ((_e328 & 4u) != 0u);
    }
    let _e332 = phi_1656_;
    if _e332 {
        let _e333 = pixel_1;
        param_58 = _e333;
        let _e334 = current;
        param_59 = _e334;
        let _e335 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_58), (&param_59));
        result_2 = _e335;
    }
    let _e336 = result_2;
    param_60 = _e336;
    let _e337 = finite4_u0028_vf4_u003b((&param_60));
    if !(_e337) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e339 = pixel_1;
    let _e340 = result_2;
    textureStore(resolvedColor, _e339, _e340);
    let _e341 = pixel_1;
    let _e342 = result_2;
    textureStore(historyOutColor, _e341, _e342);
    let _e343 = pixel_1;
    let _e344 = tileIndex_2;
    let _e346 = tileDepth[_e344];
    param_61 = _e346;
    let _e347 = storeLinearDepth_u0028_f1_u003b((&param_61));
    textureStore(historyOutLinearDepth, _e343, vec4(_e347));
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
