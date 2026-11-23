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

    let _e83 = pc.zNear;
    let _e84 = (*z);
    let _e86 = pc.zFar;
    let _e88 = pc.zNear;
    denominator = (_e83 + (_e84 * (_e86 - _e88)));
    let _e93 = pc.zNear;
    let _e95 = pc.zFar;
    let _e97 = denominator;
    let _e98 = denominator;
    return ((_e93 * _e95) * inverseSqrt((_e97 * _e98)));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e82 = (*pixel);
    let _e84 = pc.extent;
    bounded = clamp(_e82, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e84) - vec2<i32>(1i, 1i)));
    let _e88 = bounded;
    let _e89 = textureLoad(currentColor, _e88, 0i);
    return _e89;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_117_: bool;

    let _e81 = (*v);
    let _e82 = (*v);
    let _e83 = (_e81 == _e82);
    phi_117_ = _e83;
    if _e83 {
        let _e84 = (*v);
        phi_117_ = (abs(_e84) <= 0x1.fffffep+127f);
    }
    let _e88 = phi_117_;
    return _e88;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e83 = (*c);
    l = dot(_e83, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e85 = l;
    param = _e85;
    let _e86 = finite1_u0028_f1_u003b((&param));
    if !(_e86) {
        return 0f;
    }
    let _e88 = l;
    l = clamp(_e88, 0f, 10000f);
    let _e90 = l;
    let _e91 = l;
    let _e92 = l;
    return (_e90 * inverseSqrt((1f + (_e91 * _e92))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e83 = (*p);
    param_1 = _e83;
    let _e84 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e84.xyz;
    let _e86 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e86;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e84 = (*base);
    param_3 = _e84;
    let _e85 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e86 = (*base);
    let _e87 = (*across);
    param_4 = (_e86 + _e87);
    let _e89 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e85 + _e89));
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

    let _e153 = (*centerColor);
    param_5 = _e153.xyz;
    let _e155 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e155;
    let _e156 = (*px);
    param_6 = (_e156 + vec2<i32>(0i, -1i));
    let _e158 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e158;
    let _e159 = (*px);
    param_7 = (_e159 + vec2<i32>(0i, 1i));
    let _e161 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e161;
    let _e162 = (*px);
    param_8 = (_e162 + vec2<i32>(-1i, 0i));
    let _e164 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e164;
    let _e165 = (*px);
    param_9 = (_e165 + vec2<i32>(1i, 0i));
    let _e167 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e167;
    let _e168 = fxN;
    let _e169 = fxS;
    let _e171 = fxW;
    let _e172 = fxE;
    let _e174 = fxM;
    fxHi = max(max(_e168, _e169), max(max(_e171, _e172), _e174));
    let _e177 = fxN;
    let _e178 = fxS;
    let _e180 = fxW;
    let _e181 = fxE;
    let _e183 = fxM;
    fxLo = min(min(_e177, _e178), min(min(_e180, _e181), _e183));
    let _e186 = fxHi;
    let _e187 = fxLo;
    fxSpan = (_e186 - _e187);
    let _e189 = fxSpan;
    let _e190 = fxHi;
    if (_e189 < max(0.0312f, (_e190 * 0.125f))) {
        let _e194 = (*centerColor);
        return _e194;
    }
    let _e195 = (*px);
    param_10 = (_e195 + vec2<i32>(-1i, -1i));
    let _e197 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e197;
    let _e198 = (*px);
    param_11 = (_e198 + vec2<i32>(1i, -1i));
    let _e200 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e200;
    let _e201 = (*px);
    param_12 = (_e201 + vec2<i32>(-1i, 1i));
    let _e203 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e203;
    let _e204 = (*px);
    param_13 = (_e204 + vec2<i32>(1i, 1i));
    let _e206 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e206;
    let _e207 = fxN;
    let _e208 = fxS;
    fxNS = (_e207 + _e208);
    let _e210 = fxW;
    let _e211 = fxE;
    fxWE = (_e210 + _e211);
    let _e213 = fxNW;
    let _e214 = fxSW;
    fxNWSW = (_e213 + _e214);
    let _e216 = fxNE;
    let _e217 = fxSE;
    fxNESE = (_e216 + _e217);
    let _e219 = fxW;
    let _e221 = fxNWSW;
    let _e224 = fxM;
    let _e226 = fxNS;
    let _e230 = fxE;
    let _e232 = fxNESE;
    fxHorz = (abs(((-2f * _e219) + _e221)) + ((abs(((-2f * _e224) + _e226)) * 2f) + abs(((-2f * _e230) + _e232))));
    let _e237 = fxS;
    let _e239 = fxSW;
    let _e240 = fxSE;
    let _e244 = fxM;
    let _e246 = fxWE;
    let _e250 = fxN;
    let _e252 = fxNW;
    let _e253 = fxNE;
    fxVert = (abs(((-2f * _e237) + (_e239 + _e240))) + ((abs(((-2f * _e244) + _e246)) * 2f) + abs(((-2f * _e250) + (_e252 + _e253)))));
    let _e259 = fxHorz;
    let _e260 = fxVert;
    fxHorzSpan = (_e259 >= _e260);
    let _e262 = fxNS;
    let _e263 = fxWE;
    let _e266 = fxNWSW;
    let _e267 = fxNESE;
    fxAround = (((_e262 + _e263) * 2f) + (_e266 + _e267));
    let _e270 = fxHorzSpan;
    let _e271 = fxN;
    let _e272 = fxW;
    fxLoSide = select(_e272, _e271, _e270);
    let _e274 = fxHorzSpan;
    let _e275 = fxS;
    let _e276 = fxE;
    fxHiSide = select(_e276, _e275, _e274);
    let _e278 = fxLoSide;
    let _e279 = fxM;
    fxGradLo = (_e278 - _e279);
    let _e281 = fxHiSide;
    let _e282 = fxM;
    fxGradHi = (_e281 - _e282);
    let _e284 = fxGradLo;
    let _e286 = fxGradHi;
    fxLoSteeper = (abs(_e284) >= abs(_e286));
    let _e289 = fxGradLo;
    let _e291 = fxGradHi;
    fxGradient = max(abs(_e289), abs(_e291));
    let _e294 = fxLoSteeper;
    if _e294 {
        let _e295 = fxLoSide;
        let _e296 = fxM;
        local = (_e295 + _e296);
    } else {
        let _e298 = fxHiSide;
        let _e299 = fxM;
        local = (_e298 + _e299);
    }
    let _e301 = local;
    fxPair = _e301;
    let _e302 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e302));
    let _e305 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e305));
    let _e308 = fxLoSteeper;
    if _e308 {
        let _e309 = fxAcross;
        fxAcross = -(_e309);
    }
    let _e311 = fxSpan;
    let _e312 = fxSpan;
    fxInvSpan = inverseSqrt((_e311 * _e312));
    let _e315 = fxAround;
    let _e317 = fxM;
    let _e320 = fxInvSpan;
    fxBlendC = clamp((abs(((_e315 * 0.083333336f) - _e317)) * _e320), 0f, 1f);
    let _e323 = fxBlendC;
    let _e326 = fxBlendC;
    let _e327 = fxBlendC;
    fxBlendF = (((-2f * _e323) + 3f) * (_e326 * _e327));
    let _e330 = fxBlendF;
    let _e331 = fxBlendF;
    fxSubpixel = ((_e330 * _e331) * 0.75f);
    let _e334 = fxPair;
    fxHalfPair = (_e334 * 0.5f);
    let _e336 = fxGradient;
    fxScaled = (_e336 * 0.25f);
    let _e338 = fxM;
    let _e339 = fxHalfPair;
    fxMidNegative = ((_e338 - _e339) < 0f);
    let _e342 = (*px);
    let _e343 = fxAlong;
    param_14 = (_e342 - _e343);
    let _e345 = fxAcross;
    param_15 = _e345;
    let _e346 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e347 = fxHalfPair;
    fxEndN = (_e346 - _e347);
    let _e349 = (*px);
    let _e350 = fxAlong;
    param_16 = (_e349 + _e350);
    let _e352 = fxAcross;
    param_17 = _e352;
    let _e353 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e354 = fxHalfPair;
    fxEndP = (_e353 - _e354);
    let _e356 = fxEndN;
    let _e358 = fxScaled;
    fxDoneN = (abs(_e356) >= _e358);
    let _e360 = fxEndP;
    let _e362 = fxScaled;
    fxDoneP = (abs(_e360) >= _e362);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e364 = i;
        if (_e364 <= 12i) {
            let _e366 = fxDoneN;
            if !(_e366) {
                let _e368 = (*px);
                let _e369 = fxAlong;
                let _e370 = i;
                param_18 = (_e368 - (_e369 * vec2(_e370)));
                let _e374 = fxAcross;
                param_19 = _e374;
                let _e375 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e376 = fxHalfPair;
                fxEndN = (_e375 - _e376);
                let _e378 = fxEndN;
                let _e380 = fxScaled;
                fxDoneN = (abs(_e378) >= _e380);
                let _e382 = i;
                fxStepsN = _e382;
            }
            let _e383 = fxDoneP;
            if !(_e383) {
                let _e385 = (*px);
                let _e386 = fxAlong;
                let _e387 = i;
                param_20 = (_e385 + (_e386 * vec2(_e387)));
                let _e391 = fxAcross;
                param_21 = _e391;
                let _e392 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e393 = fxHalfPair;
                fxEndP = (_e392 - _e393);
                let _e395 = fxEndP;
                let _e397 = fxScaled;
                fxDoneP = (abs(_e395) >= _e397);
                let _e399 = i;
                fxStepsP = _e399;
            }
            let _e400 = fxDoneN;
            let _e401 = fxDoneP;
            if (_e400 && _e401) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e403 = i;
            i = (_e403 + 1i);
        }
    }
    let _e405 = fxStepsN;
    let _e406 = fxStepsP;
    fxTowardN = (_e405 < _e406);
    let _e408 = fxTowardN;
    let _e409 = fxEndN;
    let _e410 = fxEndP;
    fxEndG = select(_e410, _e409, _e408);
    let _e412 = fxEndG;
    let _e414 = fxMidNegative;
    fxGoodSpan = ((_e412 < 0f) != _e414);
    let _e416 = fxStepsN;
    let _e417 = fxStepsP;
    fxSpanLen = f32((_e416 + _e417));
    let _e420 = fxTowardN;
    let _e421 = fxStepsN;
    let _e422 = fxStepsP;
    fxNear = f32(select(_e422, _e421, _e420));
    let _e425 = fxGoodSpan;
    if _e425 {
        let _e426 = fxNear;
        let _e427 = fxSpanLen;
        let _e428 = fxSpanLen;
        local_1 = (0.5f - (_e426 * inverseSqrt((_e427 * _e428))));
    } else {
        local_1 = 0f;
    }
    let _e433 = local_1;
    fxEdgeBlend = _e433;
    let _e434 = fxEdgeBlend;
    let _e435 = fxSubpixel;
    fxBlend = clamp(max(_e434, _e435), 0f, 1f);
    let _e438 = (*px);
    let _e439 = fxAcross;
    param_22 = (_e438 + _e439);
    let _e441 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e441.xyz;
    let _e443 = (*centerColor);
    let _e445 = fxBlend;
    let _e448 = fxAcrossColor;
    let _e449 = fxBlend;
    let _e451 = ((_e443.xyz * (1f - _e445)) + (_e448 * _e449));
    let _e453 = (*centerColor)[3u];
    return vec4<f32>(_e451.x, _e451.y, _e451.z, _e453);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e87 = (*a);
    oneMinus = (1f - _e87);
    let _e89 = (*x);
    let _e90 = oneMinus;
    left = (_e89 * _e90);
    let _e92 = (*y);
    let _e93 = (*a);
    right = (_e92 * _e93);
    let _e95 = left;
    let _e96 = right;
    result = (_e95 + _e96);
    let _e98 = result;
    return _e98;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e83 = (*weight);
    let _e84 = (*confidence);
    scaled = (_e83 * _e84);
    let _e86 = scaled;
    return _e86;
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_147_: bool;

    let _e81 = (*v_1);
    let _e82 = (*v_1);
    let _e84 = all((_e81 == _e82));
    phi_147_ = _e84;
    if _e84 {
        let _e85 = (*v_1);
        phi_147_ = all((abs(_e85) <= vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e90 = phi_147_;
    return _e90;
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

    neighborhoodMin = vec3<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f);
    neighborhoodMax = vec3<f32>(-0x1.fffffep+127f, -0x1.fffffep+127f, -0x1.fffffep+127f);
    momentSum = vec3<f32>(0f, 0f, 0f);
    momentSquares = vec3<f32>(0f, 0f, 0f);
    y_1 = -1i;
    loop {
        let _e97 = y_1;
        if (_e97 <= 1i) {
            x_1 = -1i;
            loop {
                let _e99 = x_1;
                if (_e99 <= 1i) {
                    let _e101 = (*tileIndex);
                    let _e102 = y_1;
                    let _e105 = x_1;
                    let _e108 = tileColor[((_e101 + (_e102 * 10i)) + _e105)];
                    c_1 = _e108.xyz;
                    let _e110 = neighborhoodMin;
                    let _e111 = c_1;
                    neighborhoodMin = min(_e110, _e111);
                    let _e113 = neighborhoodMax;
                    let _e114 = c_1;
                    neighborhoodMax = max(_e113, _e114);
                    let _e116 = c_1;
                    let _e117 = momentSum;
                    momentSum = (_e117 + _e116);
                    let _e119 = c_1;
                    let _e120 = c_1;
                    let _e122 = momentSquares;
                    momentSquares = (_e122 + (_e119 * _e120));
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e124 = x_1;
                    x_1 = (_e124 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e126 = y_1;
            y_1 = (_e126 + 1i);
        }
    }
    let _e128 = neighborhoodMin;
    param_23 = _e128;
    let _e129 = finite3_u0028_vf3_u003b((&param_23));
    phi_1136_ = _e129;
    if _e129 {
        let _e130 = neighborhoodMax;
        param_24 = _e130;
        let _e131 = finite3_u0028_vf3_u003b((&param_24));
        phi_1136_ = _e131;
    }
    let _e133 = phi_1136_;
    if !(_e133) {
        return false;
    }
    let _e135 = neighborhoodMin;
    clipMin = _e135;
    let _e136 = neighborhoodMax;
    clipMax = _e136;
    let _e138 = pc.qualityFlags;
    if ((_e138 & 1u) != 0u) {
        let _e141 = momentSum;
        momentMean = (_e141 * 0.11111111f);
        let _e143 = momentSquares;
        let _e145 = momentMean;
        let _e146 = momentMean;
        momentSigma = sqrt(max(((_e143 * 0.11111111f) - (_e145 * _e146)), vec3<f32>(0f, 0f, 0f)));
        let _e151 = neighborhoodMin;
        let _e152 = momentMean;
        let _e153 = momentSigma;
        clipMin = max(_e151, (_e152 - (_e153 * 1.25f)));
        let _e157 = neighborhoodMax;
        let _e158 = momentMean;
        let _e159 = momentSigma;
        clipMax = min(_e157, (_e158 + (_e159 * 1.25f)));
    }
    let _e163 = clipMin;
    (*boxMin) = _e163;
    let _e164 = clipMax;
    (*boxMax) = _e164;
    let _e165 = momentSum;
    (*boxMean) = (_e165 * 0.11111111f);
    return true;
}

fn mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b(x_2: ptr<function, vec4<f32>>, y_2: ptr<function, vec4<f32>>, a_1: ptr<function, f32>) -> vec4<f32> {
    let _e83 = (*x_2);
    let _e84 = (*a_1);
    let _e87 = (*y_2);
    let _e88 = (*a_1);
    return ((_e83 * (1f - _e84)) + (_e87 * _e88));
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

    let _e100 = (*texel);
    baseFloat = floor(_e100);
    let _e102 = baseFloat;
    base_1 = vec2<i32>(_e102);
    let _e104 = (*texel);
    let _e105 = baseFloat;
    f = (_e104 - _e105);
    let _e108 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e108) - vec2<i32>(1i, 1i));
    let _e111 = base_1;
    let _e112 = hi;
    let _e114 = textureLoad(previousColor, clamp(_e111, vec2<i32>(0i, 0i), _e112), 0i);
    c00_ = _e114;
    let _e115 = base_1;
    let _e117 = hi;
    let _e119 = textureLoad(previousColor, clamp((_e115 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e117), 0i);
    c10_ = _e119;
    let _e120 = base_1;
    let _e122 = hi;
    let _e124 = textureLoad(previousColor, clamp((_e120 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e122), 0i);
    c01_ = _e124;
    let _e125 = base_1;
    let _e127 = hi;
    let _e129 = textureLoad(previousColor, clamp((_e125 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e127), 0i);
    c11_ = _e129;
    let _e130 = c00_;
    param_25 = _e130;
    let _e131 = c10_;
    param_26 = _e131;
    let _e133 = f[0u];
    param_27 = _e133;
    let _e134 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_25), (&param_26), (&param_27));
    top = _e134;
    let _e135 = c01_;
    param_28 = _e135;
    let _e136 = c11_;
    param_29 = _e136;
    let _e138 = f[0u];
    param_30 = _e138;
    let _e139 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_28), (&param_29), (&param_30));
    bottom = _e139;
    let _e140 = top;
    param_31 = _e140;
    let _e141 = bottom;
    param_32 = _e141;
    let _e143 = f[1u];
    param_33 = _e143;
    let _e144 = mixFast_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_31), (&param_32), (&param_33));
    return _e144;
}

fn mixFast_u0028_f1_u003b_f1_u003b_f1_u003b(x_3: ptr<function, f32>, y_3: ptr<function, f32>, a_2: ptr<function, f32>) -> f32 {
    let _e83 = (*x_3);
    let _e84 = (*a_2);
    let _e87 = (*y_3);
    let _e88 = (*a_2);
    return ((_e83 * (1f - _e84)) + (_e87 * _e88));
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

    let _e100 = (*texel_1);
    baseFloat_1 = floor(_e100);
    let _e102 = baseFloat_1;
    base_2 = vec2<i32>(_e102);
    let _e104 = (*texel_1);
    let _e105 = baseFloat_1;
    f_1 = (_e104 - _e105);
    let _e108 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e108) - vec2<i32>(1i, 1i));
    let _e111 = base_2;
    let _e112 = hi_1;
    let _e114 = textureLoad(previousLinearDepth, clamp(_e111, vec2<i32>(0i, 0i), _e112), 0i);
    d00_ = _e114.x;
    let _e116 = base_2;
    let _e118 = hi_1;
    let _e120 = textureLoad(previousLinearDepth, clamp((_e116 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e118), 0i);
    d10_ = _e120.x;
    let _e122 = base_2;
    let _e124 = hi_1;
    let _e126 = textureLoad(previousLinearDepth, clamp((_e122 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e124), 0i);
    d01_ = _e126.x;
    let _e128 = base_2;
    let _e130 = hi_1;
    let _e132 = textureLoad(previousLinearDepth, clamp((_e128 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e130), 0i);
    d11_ = _e132.x;
    let _e134 = d00_;
    param_34 = _e134;
    let _e135 = d10_;
    param_35 = _e135;
    let _e137 = f_1[0u];
    param_36 = _e137;
    let _e138 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_34), (&param_35), (&param_36));
    top_1 = _e138;
    let _e139 = d01_;
    param_37 = _e139;
    let _e140 = d11_;
    param_38 = _e140;
    let _e142 = f_1[0u];
    param_39 = _e142;
    let _e143 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_37), (&param_38), (&param_39));
    bottom_1 = _e143;
    let _e144 = top_1;
    param_40 = _e144;
    let _e145 = bottom_1;
    param_41 = _e145;
    let _e147 = f_1[1u];
    param_42 = _e147;
    let _e148 = mixFast_u0028_f1_u003b_f1_u003b_f1_u003b((&param_40), (&param_41), (&param_42));
    return _e148;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator_1: f32;
    var numerator: f32;
    var result_1: f32;

    let _e87 = pc.zFar;
    let _e89 = pc.zNear;
    range = (_e87 - _e89);
    let _e91 = (*z_1);
    let _e92 = range;
    scaled_1 = (_e91 * _e92);
    let _e95 = pc.zNear;
    let _e96 = scaled_1;
    denominator_1 = (_e95 + _e96);
    let _e99 = pc.zNear;
    let _e101 = pc.zFar;
    numerator = (_e99 * _e101);
    let _e103 = numerator;
    let _e104 = denominator_1;
    result_1 = (_e103 / _e104);
    let _e106 = result_1;
    return _e106;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_132_: bool;

    let _e81 = (*v_2);
    let _e82 = (*v_2);
    let _e84 = all((_e81 == _e82));
    phi_132_ = _e84;
    if _e84 {
        let _e85 = (*v_2);
        phi_132_ = all((abs(_e85) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e90 = phi_132_;
    return _e90;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_162_: bool;

    let _e81 = (*v_3);
    let _e82 = (*v_3);
    let _e84 = all((_e81 == _e82));
    phi_162_ = _e84;
    if _e84 {
        let _e85 = (*v_3);
        phi_162_ = all((abs(_e85) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e90 = phi_162_;
    return _e90;
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

    let _e94 = (*px_1);
    let _e95 = textureLoad(motionVelocity, _e94, 0i);
    centerVelocity = _e95.xy;
    let _e98 = pc.qualityFlags;
    let _e100 = ((_e98 & 2u) == 0u);
    phi_960_ = _e100;
    if !(_e100) {
        let _e102 = (*centerValid);
        phi_960_ = !((_e102 > 0f));
    }
    let _e106 = phi_960_;
    phi_968_ = _e106;
    if !(_e106) {
        let _e108 = centerVelocity;
        param_43 = _e108;
        let _e109 = finite2_u0028_vf2_u003b((&param_43));
        phi_968_ = !(_e109);
    }
    let _e112 = phi_968_;
    if _e112 {
        let _e113 = centerVelocity;
        return _e113;
    }
    let _e115 = pc.extent;
    hi_2 = (bitcast<vec2<i32>>(_e115) - vec2<i32>(1i, 1i));
    let _e118 = (*px_1);
    best = _e118;
    let _e119 = (*tileIndex_1);
    let _e121 = tileDepth[_e119];
    bestDepth = _e121;
    y_4 = -1i;
    loop {
        let _e122 = y_4;
        if (_e122 <= 1i) {
            x_4 = -1i;
            loop {
                let _e124 = x_4;
                if (_e124 <= 1i) {
                    let _e126 = (*tileIndex_1);
                    let _e127 = y_4;
                    let _e130 = x_4;
                    let _e133 = tileDepth[((_e126 + (_e127 * 10i)) + _e130)];
                    d = _e133;
                    let _e134 = d;
                    let _e135 = bestDepth;
                    if (_e134 > _e135) {
                        let _e137 = d;
                        bestDepth = _e137;
                        let _e138 = (*px_1);
                        let _e139 = x_4;
                        let _e140 = y_4;
                        let _e143 = hi_2;
                        best = clamp((_e138 + vec2<i32>(_e139, _e140)), vec2<i32>(0i, 0i), _e143);
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e145 = x_4;
                    x_4 = (_e145 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e147 = y_4;
            y_4 = (_e147 + 1i);
        }
    }
    let _e149 = best;
    let _e150 = (*px_1);
    if all((_e149 == _e150)) {
        let _e153 = centerVelocity;
        return _e153;
    }
    let _e154 = best;
    let _e155 = textureLoad(motionValidity, _e154, 0i);
    v_4 = _e155.x;
    let _e157 = v_4;
    if !((_e157 > 0f)) {
        let _e160 = centerVelocity;
        return _e160;
    }
    let _e161 = best;
    let _e162 = textureLoad(motionVelocity, _e161, 0i);
    dv = _e162.xy;
    let _e164 = dv;
    param_44 = _e164;
    let _e165 = finite2_u0028_vf2_u003b((&param_44));
    let _e166 = dv;
    let _e167 = centerVelocity;
    return select(_e167, _e166, vec2(_e165));
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
    var param_70: vec4<f32>;
    var param_71: f32;
    var phi_1342_: bool;
    var phi_1351_: bool;
    var phi_1360_: bool;
    var phi_1366_: bool;
    var phi_1403_: bool;
    var phi_1418_: bool;
    var phi_1442_: bool;
    var phi_1498_: bool;
    var phi_1506_: bool;

    let _e138 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e138.xy);
    let _e141 = gl_WorkGroupID_1;
    tileOrigin = ((bitcast<vec2<i32>>(_e141.xy) * vec2(8i)) - vec2<i32>(1i, 1i));
    let _e148 = pc.extent;
    imageHi = (bitcast<vec2<i32>>(_e148) - vec2<i32>(1i, 1i));
    let _e151 = gl_LocalInvocationIndex_1;
    i_1 = _e151;
    loop {
        let _e152 = i_1;
        if (_e152 < 100u) {
            let _e154 = tileOrigin;
            let _e155 = i_1;
            let _e158 = i_1;
            let _e163 = imageHi;
            q = clamp((_e154 + vec2<i32>(bitcast<i32>((_e155 % 10u)), bitcast<i32>((_e158 / 10u)))), vec2<i32>(0i, 0i), _e163);
            let _e165 = i_1;
            let _e166 = q;
            let _e167 = textureLoad(currentColor, _e166, 0i);
            tileColor[_e165] = _e167;
            let _e169 = i_1;
            let _e170 = q;
            let _e171 = textureLoad(currentDeviceDepth, _e170, 0i);
            tileDepth[_e169] = _e171.x;
            continue;
        } else {
            break;
        }
        continuing {
            let _e174 = i_1;
            i_1 = (_e174 + 64u);
        }
    }
    workgroupBarrier();
    let _e176 = pixel_1;
    let _e178 = pc.extent;
    if any((_e176 >= bitcast<vec2<i32>>(_e178))) {
        return;
    }
    let _e183 = gl_LocalInvocationID_1[1u];
    let _e188 = gl_LocalInvocationID_1[0u];
    tileIndex_2 = ((((bitcast<i32>(_e183) + 1i) * 10i) + bitcast<i32>(_e188)) + 1i);
    let _e192 = tileIndex_2;
    let _e194 = tileColor[_e192];
    current = _e194;
    let _e195 = current;
    result_2 = _e195;
    usedHistory = false;
    let _e196 = pixel_1;
    let _e197 = textureLoad(motionValidity, _e196, 0i);
    valid = _e197.x;
    let _e199 = pixel_1;
    param_45 = _e199;
    let _e200 = tileIndex_2;
    param_46 = _e200;
    let _e201 = valid;
    param_47 = _e201;
    let _e202 = dlMotionVelocity_u0028_vi2_u003b_i1_u003b_f1_u003b((&param_45), (&param_46), (&param_47));
    velocity = _e202;
    let _e204 = pc.previousEffectiveJitterUv;
    let _e206 = pc.currentEffectiveJitterUv;
    jitterDelta = (_e204 - _e206);
    let _e208 = jitterDelta;
    let _e210 = pc.extent;
    jitterTexel = (_e208 * vec2<f32>(_e210));
    let _e213 = velocity;
    let _e215 = pc.extent;
    motionTexel = (_e213 * vec2<f32>(_e215));
    let _e218 = pixel_1;
    let _e220 = motionTexel;
    unjitteredPreviousTexel = (vec2<f32>(_e218) - _e220);
    let _e222 = unjitteredPreviousTexel;
    let _e223 = jitterTexel;
    previousTexel = (_e222 + _e223);
    let _e225 = previousTexel;
    previousCenter = (_e225 + vec2(0.5f));
    let _e228 = previousCenter;
    let _e230 = all((_e228 >= vec2<f32>(0f, 0f)));
    phi_1342_ = _e230;
    if _e230 {
        let _e231 = previousCenter;
        let _e233 = pc.extent;
        phi_1342_ = all((_e231 < vec2<f32>(_e233)));
    }
    let _e238 = phi_1342_;
    inside = _e238;
    let _e239 = current;
    param_48 = _e239;
    let _e240 = finite4_u0028_vf4_u003b((&param_48));
    phi_1351_ = _e240;
    if _e240 {
        let _e241 = valid;
        param_49 = _e241;
        let _e242 = finite1_u0028_f1_u003b((&param_49));
        phi_1351_ = _e242;
    }
    let _e244 = phi_1351_;
    let _e245 = valid;
    let _e247 = (_e244 && (_e245 > 0f));
    phi_1360_ = _e247;
    if _e247 {
        let _e248 = velocity;
        param_50 = _e248;
        let _e249 = finite2_u0028_vf2_u003b((&param_50));
        phi_1360_ = _e249;
    }
    let _e251 = phi_1360_;
    phi_1366_ = _e251;
    if _e251 {
        let _e252 = previousTexel;
        param_51 = _e252;
        let _e253 = finite2_u0028_vf2_u003b((&param_51));
        phi_1366_ = _e253;
    }
    let _e255 = phi_1366_;
    let _e256 = inside;
    if (_e255 && _e256) {
        let _e258 = tileIndex_2;
        let _e260 = tileDepth[_e258];
        currentDevice = _e260;
        let _e261 = currentDevice;
        param_52 = _e261;
        let _e262 = linearizeDepth_u0028_f1_u003b((&param_52));
        currentLinear = _e262;
        let _e263 = previousTexel;
        param_53 = _e263;
        let _e264 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_53));
        priorLinear = _e264;
        let _e266 = pc.depthThresholdAbsolute;
        let _e268 = pc.depthThresholdRelative;
        let _e269 = currentLinear;
        let _e270 = priorLinear;
        depthLimit = max(_e266, (_e268 * max(_e269, _e270)));
        let _e274 = currentLinear;
        param_54 = _e274;
        let _e275 = finite1_u0028_f1_u003b((&param_54));
        phi_1403_ = _e275;
        if _e275 {
            let _e276 = priorLinear;
            param_55 = _e276;
            let _e277 = finite1_u0028_f1_u003b((&param_55));
            phi_1403_ = _e277;
        }
        let _e279 = phi_1403_;
        let _e280 = currentLinear;
        let _e283 = priorLinear;
        let _e285 = ((_e279 && (_e280 > 0f)) && (_e283 > 0f));
        phi_1418_ = _e285;
        if _e285 {
            let _e286 = currentLinear;
            let _e287 = priorLinear;
            let _e290 = depthLimit;
            phi_1418_ = (abs((_e286 - _e287)) <= _e290);
        }
        let _e293 = phi_1418_;
        if _e293 {
            let _e294 = previousTexel;
            param_56 = _e294;
            let _e295 = samplePreviousColorLinear_u0028_vf2_u003b((&param_56));
            prior = _e295;
            let _e296 = prior;
            param_57 = _e296;
            let _e297 = finite4_u0028_vf4_u003b((&param_57));
            phi_1442_ = _e297;
            if _e297 {
                let _e298 = tileIndex_2;
                param_58 = _e298;
                let _e299 = neighborhoodBox_u0028_i1_u003b_vf3_u003b_vf3_u003b_vf3_u003b((&param_58), (&param_59), (&param_60), (&param_61));
                let _e300 = param_59;
                clipMin_1 = _e300;
                let _e301 = param_60;
                clipMax_1 = _e301;
                let _e302 = param_61;
                clipMean = _e302;
                phi_1442_ = _e299;
            }
            let _e304 = phi_1442_;
            if _e304 {
                let _e305 = prior;
                let _e307 = clipMin_1;
                let _e308 = clipMax_1;
                clampedPrior = clamp(_e305.xyz, _e307, _e308);
                let _e310 = valid;
                confidence_1 = clamp(_e310, 0f, 1f);
                let _e313 = pc.historyWeight;
                baseWeight = clamp(_e313, 0f, 1f);
                let _e315 = baseWeight;
                param_62 = _e315;
                let _e316 = confidence_1;
                param_63 = _e316;
                let _e317 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_62), (&param_63));
                weight_1 = _e317;
                let _e318 = current;
                param_64 = _e318.xyz;
                let _e320 = clampedPrior;
                param_65 = _e320;
                let _e321 = weight_1;
                param_66 = _e321;
                let _e322 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_64), (&param_65), (&param_66));
                resolvedRgb = _e322;
                let _e323 = resolvedRgb;
                let _e324 = current;
                let _e326 = clipMean;
                let _e330 = clipMin_1;
                let _e331 = clipMax_1;
                let _e332 = clamp((_e323 + ((_e324.xyz - _e326) * 0.6f)), _e330, _e331);
                let _e334 = current[3u];
                result_2 = vec4<f32>(_e332.x, _e332.y, _e332.z, _e334);
                usedHistory = true;
            }
        }
    }
    let _e339 = usedHistory;
    let _e340 = !(_e339);
    phi_1498_ = _e340;
    if _e340 {
        let _e341 = current;
        param_67 = _e341;
        let _e342 = finite4_u0028_vf4_u003b((&param_67));
        phi_1498_ = _e342;
    }
    let _e344 = phi_1498_;
    phi_1506_ = _e344;
    if _e344 {
        let _e346 = pc.qualityFlags;
        phi_1506_ = ((_e346 & 4u) != 0u);
    }
    let _e350 = phi_1506_;
    if _e350 {
        let _e351 = pixel_1;
        param_68 = _e351;
        let _e352 = current;
        param_69 = _e352;
        let _e353 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_68), (&param_69));
        result_2 = _e353;
    }
    let _e354 = result_2;
    param_70 = _e354;
    let _e355 = finite4_u0028_vf4_u003b((&param_70));
    if !(_e355) {
        result_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e357 = pixel_1;
    let _e358 = result_2;
    textureStore(resolvedColor, _e357, _e358);
    let _e359 = pixel_1;
    let _e360 = result_2;
    textureStore(historyOutColor, _e359, _e360);
    let _e361 = pixel_1;
    let _e362 = tileIndex_2;
    let _e364 = tileDepth[_e362];
    param_71 = _e364;
    let _e365 = storeLinearDepth_u0028_f1_u003b((&param_71));
    textureStore(historyOutLinearDepth, _e361, vec4(_e365));
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
