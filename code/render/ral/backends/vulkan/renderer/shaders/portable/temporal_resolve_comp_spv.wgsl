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
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(5)
var motionValidity: texture_2d<f32>;
@group(0) @binding(4)
var motionVelocity: texture_2d<f32>;
@group(0) @binding(1)
var currentDeviceDepth: texture_2d<f32>;
@group(0) @binding(7)
var resolvedColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(8)
var historyOutColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(9)
var historyOutLinearDepth: texture_storage_2d<r32float,write>;

fn storeLinearDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    let _e64 = pc.zNear;
    let _e66 = pc.zFar;
    let _e69 = pc.zNear;
    let _e70 = (*z);
    let _e72 = pc.zFar;
    let _e74 = pc.zNear;
    return ((_e64 * _e66) / (_e69 + (_e70 * (_e72 - _e74))));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e64 = (*pixel);
    let _e66 = pc.extent;
    bounded = clamp(_e64, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e66) - vec2<i32>(1i, 1i)));
    let _e70 = bounded;
    let _e71 = textureLoad(currentColor, _e70, 0i);
    return _e71;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_103_: bool;

    let _e63 = (*v);
    let _e64 = (*v);
    let _e65 = (_e63 == _e64);
    phi_103_ = _e65;
    if _e65 {
        let _e66 = (*v);
        phi_103_ = (abs(_e66) <= 340282350000000000000000000000000000000f);
    }
    let _e70 = phi_103_;
    return _e70;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e65 = (*c);
    l = dot(_e65, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e67 = l;
    param = _e67;
    let _e68 = finite1_u0028_f1_u003b((&param));
    if !(_e68) {
        return 0f;
    }
    let _e70 = l;
    l = clamp(_e70, 0f, 10000f);
    let _e72 = l;
    let _e73 = l;
    let _e74 = l;
    return (_e72 * inverseSqrt((1f + (_e73 * _e74))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e65 = (*p);
    param_1 = _e65;
    let _e66 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e66.xyz;
    let _e68 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e68;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e66 = (*base);
    param_3 = _e66;
    let _e67 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e68 = (*base);
    let _e69 = (*across);
    param_4 = (_e68 + _e69);
    let _e71 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e67 + _e71));
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

    let _e135 = (*centerColor);
    param_5 = _e135.xyz;
    let _e137 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e137;
    let _e138 = (*px);
    param_6 = (_e138 + vec2<i32>(0i, -1i));
    let _e140 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e140;
    let _e141 = (*px);
    param_7 = (_e141 + vec2<i32>(0i, 1i));
    let _e143 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e143;
    let _e144 = (*px);
    param_8 = (_e144 + vec2<i32>(-1i, 0i));
    let _e146 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e146;
    let _e147 = (*px);
    param_9 = (_e147 + vec2<i32>(1i, 0i));
    let _e149 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e149;
    let _e150 = fxN;
    let _e151 = fxS;
    let _e153 = fxW;
    let _e154 = fxE;
    let _e156 = fxM;
    fxHi = max(max(_e150, _e151), max(max(_e153, _e154), _e156));
    let _e159 = fxN;
    let _e160 = fxS;
    let _e162 = fxW;
    let _e163 = fxE;
    let _e165 = fxM;
    fxLo = min(min(_e159, _e160), min(min(_e162, _e163), _e165));
    let _e168 = fxHi;
    let _e169 = fxLo;
    fxSpan = (_e168 - _e169);
    let _e171 = fxSpan;
    let _e172 = fxHi;
    if (_e171 < max(0.0312f, (_e172 * 0.125f))) {
        let _e176 = (*centerColor);
        return _e176;
    }
    let _e177 = (*px);
    param_10 = (_e177 + vec2<i32>(-1i, -1i));
    let _e179 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e179;
    let _e180 = (*px);
    param_11 = (_e180 + vec2<i32>(1i, -1i));
    let _e182 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e182;
    let _e183 = (*px);
    param_12 = (_e183 + vec2<i32>(-1i, 1i));
    let _e185 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e185;
    let _e186 = (*px);
    param_13 = (_e186 + vec2<i32>(1i, 1i));
    let _e188 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e188;
    let _e189 = fxN;
    let _e190 = fxS;
    fxNS = (_e189 + _e190);
    let _e192 = fxW;
    let _e193 = fxE;
    fxWE = (_e192 + _e193);
    let _e195 = fxNW;
    let _e196 = fxSW;
    fxNWSW = (_e195 + _e196);
    let _e198 = fxNE;
    let _e199 = fxSE;
    fxNESE = (_e198 + _e199);
    let _e201 = fxW;
    let _e203 = fxNWSW;
    let _e206 = fxM;
    let _e208 = fxNS;
    let _e212 = fxE;
    let _e214 = fxNESE;
    fxHorz = (abs(((-2f * _e201) + _e203)) + ((abs(((-2f * _e206) + _e208)) * 2f) + abs(((-2f * _e212) + _e214))));
    let _e219 = fxS;
    let _e221 = fxSW;
    let _e222 = fxSE;
    let _e226 = fxM;
    let _e228 = fxWE;
    let _e232 = fxN;
    let _e234 = fxNW;
    let _e235 = fxNE;
    fxVert = (abs(((-2f * _e219) + (_e221 + _e222))) + ((abs(((-2f * _e226) + _e228)) * 2f) + abs(((-2f * _e232) + (_e234 + _e235)))));
    let _e241 = fxHorz;
    let _e242 = fxVert;
    fxHorzSpan = (_e241 >= _e242);
    let _e244 = fxNS;
    let _e245 = fxWE;
    let _e248 = fxNWSW;
    let _e249 = fxNESE;
    fxAround = (((_e244 + _e245) * 2f) + (_e248 + _e249));
    let _e252 = fxHorzSpan;
    let _e253 = fxN;
    let _e254 = fxW;
    fxLoSide = select(_e254, _e253, _e252);
    let _e256 = fxHorzSpan;
    let _e257 = fxS;
    let _e258 = fxE;
    fxHiSide = select(_e258, _e257, _e256);
    let _e260 = fxLoSide;
    let _e261 = fxM;
    fxGradLo = (_e260 - _e261);
    let _e263 = fxHiSide;
    let _e264 = fxM;
    fxGradHi = (_e263 - _e264);
    let _e266 = fxGradLo;
    let _e268 = fxGradHi;
    fxLoSteeper = (abs(_e266) >= abs(_e268));
    let _e271 = fxGradLo;
    let _e273 = fxGradHi;
    fxGradient = max(abs(_e271), abs(_e273));
    let _e276 = fxLoSteeper;
    if _e276 {
        let _e277 = fxLoSide;
        let _e278 = fxM;
        local = (_e277 + _e278);
    } else {
        let _e280 = fxHiSide;
        let _e281 = fxM;
        local = (_e280 + _e281);
    }
    let _e283 = local;
    fxPair = _e283;
    let _e284 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e284));
    let _e287 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e287));
    let _e290 = fxLoSteeper;
    if _e290 {
        let _e291 = fxAcross;
        fxAcross = -(_e291);
    }
    let _e293 = fxSpan;
    let _e294 = fxSpan;
    fxInvSpan = inverseSqrt((_e293 * _e294));
    let _e297 = fxAround;
    let _e299 = fxM;
    let _e302 = fxInvSpan;
    fxBlendC = clamp((abs(((_e297 * 0.083333336f) - _e299)) * _e302), 0f, 1f);
    let _e305 = fxBlendC;
    let _e308 = fxBlendC;
    let _e309 = fxBlendC;
    fxBlendF = (((-2f * _e305) + 3f) * (_e308 * _e309));
    let _e312 = fxBlendF;
    let _e313 = fxBlendF;
    fxSubpixel = ((_e312 * _e313) * 0.75f);
    let _e316 = fxPair;
    fxHalfPair = (_e316 * 0.5f);
    let _e318 = fxGradient;
    fxScaled = (_e318 * 0.25f);
    let _e320 = fxM;
    let _e321 = fxHalfPair;
    fxMidNegative = ((_e320 - _e321) < 0f);
    let _e324 = (*px);
    let _e325 = fxAlong;
    param_14 = (_e324 - _e325);
    let _e327 = fxAcross;
    param_15 = _e327;
    let _e328 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e329 = fxHalfPair;
    fxEndN = (_e328 - _e329);
    let _e331 = (*px);
    let _e332 = fxAlong;
    param_16 = (_e331 + _e332);
    let _e334 = fxAcross;
    param_17 = _e334;
    let _e335 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e336 = fxHalfPair;
    fxEndP = (_e335 - _e336);
    let _e338 = fxEndN;
    let _e340 = fxScaled;
    fxDoneN = (abs(_e338) >= _e340);
    let _e342 = fxEndP;
    let _e344 = fxScaled;
    fxDoneP = (abs(_e342) >= _e344);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e346 = i;
        if (_e346 <= 12i) {
            let _e348 = fxDoneN;
            if !(_e348) {
                let _e350 = (*px);
                let _e351 = fxAlong;
                let _e352 = i;
                param_18 = (_e350 - (_e351 * vec2(_e352)));
                let _e356 = fxAcross;
                param_19 = _e356;
                let _e357 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e358 = fxHalfPair;
                fxEndN = (_e357 - _e358);
                let _e360 = fxEndN;
                let _e362 = fxScaled;
                fxDoneN = (abs(_e360) >= _e362);
                let _e364 = i;
                fxStepsN = _e364;
            }
            let _e365 = fxDoneP;
            if !(_e365) {
                let _e367 = (*px);
                let _e368 = fxAlong;
                let _e369 = i;
                param_20 = (_e367 + (_e368 * vec2(_e369)));
                let _e373 = fxAcross;
                param_21 = _e373;
                let _e374 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e375 = fxHalfPair;
                fxEndP = (_e374 - _e375);
                let _e377 = fxEndP;
                let _e379 = fxScaled;
                fxDoneP = (abs(_e377) >= _e379);
                let _e381 = i;
                fxStepsP = _e381;
            }
            let _e382 = fxDoneN;
            let _e383 = fxDoneP;
            if (_e382 && _e383) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e385 = i;
            i = (_e385 + 1i);
        }
    }
    let _e387 = fxStepsN;
    let _e388 = fxStepsP;
    fxTowardN = (_e387 < _e388);
    let _e390 = fxTowardN;
    let _e391 = fxEndN;
    let _e392 = fxEndP;
    fxEndG = select(_e392, _e391, _e390);
    let _e394 = fxEndG;
    let _e396 = fxMidNegative;
    fxGoodSpan = ((_e394 < 0f) != _e396);
    let _e398 = fxStepsN;
    let _e399 = fxStepsP;
    fxSpanLen = f32((_e398 + _e399));
    let _e402 = fxTowardN;
    let _e403 = fxStepsN;
    let _e404 = fxStepsP;
    fxNear = f32(select(_e404, _e403, _e402));
    let _e407 = fxGoodSpan;
    if _e407 {
        let _e408 = fxNear;
        let _e409 = fxSpanLen;
        let _e410 = fxSpanLen;
        local_1 = (0.5f - (_e408 * inverseSqrt((_e409 * _e410))));
    } else {
        local_1 = 0f;
    }
    let _e415 = local_1;
    fxEdgeBlend = _e415;
    let _e416 = fxEdgeBlend;
    let _e417 = fxSubpixel;
    fxBlend = clamp(max(_e416, _e417), 0f, 1f);
    let _e420 = (*px);
    let _e421 = fxAcross;
    param_22 = (_e420 + _e421);
    let _e423 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e423.xyz;
    let _e425 = (*centerColor);
    let _e427 = fxBlend;
    let _e430 = fxAcrossColor;
    let _e431 = fxBlend;
    let _e433 = ((_e425.xyz * (1f - _e427)) + (_e430 * _e431));
    let _e435 = (*centerColor)[3u];
    return vec4<f32>(_e433.x, _e433.y, _e433.z, _e435);
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e69 = (*a);
    oneMinus = (1f - _e69);
    let _e71 = (*x);
    let _e72 = oneMinus;
    left = (_e71 * _e72);
    let _e74 = (*y);
    let _e75 = (*a);
    right = (_e74 * _e75);
    let _e77 = left;
    let _e78 = right;
    result = (_e77 + _e78);
    let _e80 = result;
    return _e80;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e65 = (*weight);
    let _e66 = (*confidence);
    scaled = (_e65 * _e66);
    let _e68 = scaled;
    return _e68;
}

fn finite3_u0028_vf3_u003b(v_1: ptr<function, vec3<f32>>) -> bool {
    var phi_133_: bool;

    let _e63 = (*v_1);
    let _e64 = (*v_1);
    let _e66 = all((_e63 == _e64));
    phi_133_ = _e66;
    if _e66 {
        let _e67 = (*v_1);
        phi_133_ = all((abs(_e67) <= vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e72 = phi_133_;
    return _e72;
}

fn mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b(x_1: ptr<function, vec4<f32>>, y_1: ptr<function, vec4<f32>>, a_1: ptr<function, f32>) -> vec4<f32> {
    var oneMinus_1: f32;
    var left_1: vec4<f32>;
    var right_1: vec4<f32>;
    var result_1: vec4<f32>;

    let _e69 = (*a_1);
    oneMinus_1 = (1f - _e69);
    let _e71 = (*x_1);
    let _e72 = oneMinus_1;
    left_1 = (_e71 * _e72);
    let _e74 = (*y_1);
    let _e75 = (*a_1);
    right_1 = (_e74 * _e75);
    let _e77 = left_1;
    let _e78 = right_1;
    result_1 = (_e77 + _e78);
    let _e80 = result_1;
    return _e80;
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
    var param_23: vec4<f32>;
    var param_24: vec4<f32>;
    var param_25: f32;
    var bottom: vec4<f32>;
    var param_26: vec4<f32>;
    var param_27: vec4<f32>;
    var param_28: f32;
    var param_29: vec4<f32>;
    var param_30: vec4<f32>;
    var param_31: f32;

    let _e82 = (*texel);
    baseFloat = floor(_e82);
    let _e84 = baseFloat;
    base_1 = vec2<i32>(_e84);
    let _e86 = (*texel);
    let _e87 = baseFloat;
    f = (_e86 - _e87);
    let _e90 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e90) - vec2<i32>(1i, 1i));
    let _e93 = base_1;
    let _e94 = hi;
    let _e96 = textureLoad(previousColor, clamp(_e93, vec2<i32>(0i, 0i), _e94), 0i);
    c00_ = _e96;
    let _e97 = base_1;
    let _e99 = hi;
    let _e101 = textureLoad(previousColor, clamp((_e97 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e99), 0i);
    c10_ = _e101;
    let _e102 = base_1;
    let _e104 = hi;
    let _e106 = textureLoad(previousColor, clamp((_e102 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e104), 0i);
    c01_ = _e106;
    let _e107 = base_1;
    let _e109 = hi;
    let _e111 = textureLoad(previousColor, clamp((_e107 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e109), 0i);
    c11_ = _e111;
    let _e112 = c00_;
    param_23 = _e112;
    let _e113 = c10_;
    param_24 = _e113;
    let _e115 = f[0u];
    param_25 = _e115;
    let _e116 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_23), (&param_24), (&param_25));
    top = _e116;
    let _e117 = c01_;
    param_26 = _e117;
    let _e118 = c11_;
    param_27 = _e118;
    let _e120 = f[0u];
    param_28 = _e120;
    let _e121 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_26), (&param_27), (&param_28));
    bottom = _e121;
    let _e122 = top;
    param_29 = _e122;
    let _e123 = bottom;
    param_30 = _e123;
    let _e125 = f[1u];
    param_31 = _e125;
    let _e126 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_29), (&param_30), (&param_31));
    return _e126;
}

fn mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b(x_2: ptr<function, f32>, y_2: ptr<function, f32>, a_2: ptr<function, f32>) -> f32 {
    var oneMinus_2: f32;
    var left_2: f32;
    var right_2: f32;
    var result_2: f32;

    let _e69 = (*a_2);
    oneMinus_2 = (1f - _e69);
    let _e71 = (*x_2);
    let _e72 = oneMinus_2;
    left_2 = (_e71 * _e72);
    let _e74 = (*y_2);
    let _e75 = (*a_2);
    right_2 = (_e74 * _e75);
    let _e77 = left_2;
    let _e78 = right_2;
    result_2 = (_e77 + _e78);
    let _e80 = result_2;
    return _e80;
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
    var param_32: f32;
    var param_33: f32;
    var param_34: f32;
    var bottom_1: f32;
    var param_35: f32;
    var param_36: f32;
    var param_37: f32;
    var param_38: f32;
    var param_39: f32;
    var param_40: f32;

    let _e82 = (*texel_1);
    baseFloat_1 = floor(_e82);
    let _e84 = baseFloat_1;
    base_2 = vec2<i32>(_e84);
    let _e86 = (*texel_1);
    let _e87 = baseFloat_1;
    f_1 = (_e86 - _e87);
    let _e90 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e90) - vec2<i32>(1i, 1i));
    let _e93 = base_2;
    let _e94 = hi_1;
    let _e96 = textureLoad(previousLinearDepth, clamp(_e93, vec2<i32>(0i, 0i), _e94), 0i);
    d00_ = _e96.x;
    let _e98 = base_2;
    let _e100 = hi_1;
    let _e102 = textureLoad(previousLinearDepth, clamp((_e98 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e100), 0i);
    d10_ = _e102.x;
    let _e104 = base_2;
    let _e106 = hi_1;
    let _e108 = textureLoad(previousLinearDepth, clamp((_e104 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e106), 0i);
    d01_ = _e108.x;
    let _e110 = base_2;
    let _e112 = hi_1;
    let _e114 = textureLoad(previousLinearDepth, clamp((_e110 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e112), 0i);
    d11_ = _e114.x;
    let _e116 = d00_;
    param_32 = _e116;
    let _e117 = d10_;
    param_33 = _e117;
    let _e119 = f_1[0u];
    param_34 = _e119;
    let _e120 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_32), (&param_33), (&param_34));
    top_1 = _e120;
    let _e121 = d01_;
    param_35 = _e121;
    let _e122 = d11_;
    param_36 = _e122;
    let _e124 = f_1[0u];
    param_37 = _e124;
    let _e125 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_35), (&param_36), (&param_37));
    bottom_1 = _e125;
    let _e126 = top_1;
    param_38 = _e126;
    let _e127 = bottom_1;
    param_39 = _e127;
    let _e129 = f_1[1u];
    param_40 = _e129;
    let _e130 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_38), (&param_39), (&param_40));
    return _e130;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator: f32;
    var numerator: f32;
    var result_3: f32;

    let _e69 = pc.zFar;
    let _e71 = pc.zNear;
    range = (_e69 - _e71);
    let _e73 = (*z_1);
    let _e74 = range;
    scaled_1 = (_e73 * _e74);
    let _e77 = pc.zNear;
    let _e78 = scaled_1;
    denominator = (_e77 + _e78);
    let _e81 = pc.zNear;
    let _e83 = pc.zFar;
    numerator = (_e81 * _e83);
    let _e85 = numerator;
    let _e86 = denominator;
    result_3 = (_e85 / _e86);
    let _e88 = result_3;
    return _e88;
}

fn finite2_u0028_vf2_u003b(v_2: ptr<function, vec2<f32>>) -> bool {
    var phi_118_: bool;

    let _e63 = (*v_2);
    let _e64 = (*v_2);
    let _e66 = all((_e63 == _e64));
    phi_118_ = _e66;
    if _e66 {
        let _e67 = (*v_2);
        phi_118_ = all((abs(_e67) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e72 = phi_118_;
    return _e72;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_148_: bool;

    let _e63 = (*v_3);
    let _e64 = (*v_3);
    let _e66 = all((_e63 == _e64));
    phi_148_ = _e66;
    if _e66 {
        let _e67 = (*v_3);
        phi_148_ = all((abs(_e67) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e72 = phi_148_;
    return _e72;
}

fn main_1() {
    var pixel_1: vec2<i32>;
    var current: vec4<f32>;
    var result_4: vec4<f32>;
    var usedHistory: bool;
    var valid: f32;
    var velocity: vec2<f32>;
    var jitterDelta: vec2<f32>;
    var jitterTexel: vec2<f32>;
    var motionTexel: vec2<f32>;
    var unjitteredPreviousTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_41: vec4<f32>;
    var param_42: f32;
    var param_43: vec2<f32>;
    var param_44: vec2<f32>;
    var currentDevice: f32;
    var currentLinear: f32;
    var param_45: f32;
    var priorLinear: f32;
    var param_46: vec2<f32>;
    var depthLimit: f32;
    var param_47: f32;
    var param_48: f32;
    var prior: vec4<f32>;
    var param_49: vec2<f32>;
    var neighborhoodMin: vec3<f32>;
    var neighborhoodMax: vec3<f32>;
    var y_3: i32;
    var x_3: i32;
    var c_1: vec3<f32>;
    var param_50: vec2<i32>;
    var param_51: vec4<f32>;
    var param_52: vec3<f32>;
    var param_53: vec3<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var baseWeight: f32;
    var weight_1: f32;
    var param_54: f32;
    var param_55: f32;
    var resolvedRgb: vec3<f32>;
    var param_56: vec3<f32>;
    var param_57: vec3<f32>;
    var param_58: f32;
    var param_59: vec4<f32>;
    var param_60: vec2<i32>;
    var param_61: vec4<f32>;
    var param_62: vec4<f32>;
    var param_63: f32;
    var phi_1026_: bool;
    var phi_1035_: bool;
    var phi_1044_: bool;
    var phi_1050_: bool;
    var phi_1092_: bool;
    var phi_1107_: bool;
    var phi_1161_: bool;
    var phi_1167_: bool;
    var phi_1214_: bool;

    let _e113 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e113.xy);
    let _e116 = pixel_1;
    let _e118 = pc.extent;
    if any((_e116 >= bitcast<vec2<i32>>(_e118))) {
        return;
    }
    let _e122 = pixel_1;
    let _e123 = textureLoad(currentColor, _e122, 0i);
    current = _e123;
    let _e124 = current;
    result_4 = _e124;
    usedHistory = false;
    let _e125 = pixel_1;
    let _e126 = textureLoad(motionValidity, _e125, 0i);
    valid = _e126.x;
    let _e128 = pixel_1;
    let _e129 = textureLoad(motionVelocity, _e128, 0i);
    velocity = _e129.xy;
    let _e132 = pc.previousEffectiveJitterUv;
    let _e134 = pc.currentEffectiveJitterUv;
    jitterDelta = (_e132 - _e134);
    let _e136 = jitterDelta;
    let _e138 = pc.extent;
    jitterTexel = (_e136 * vec2<f32>(_e138));
    let _e141 = velocity;
    let _e143 = pc.extent;
    motionTexel = (_e141 * vec2<f32>(_e143));
    let _e146 = pixel_1;
    let _e148 = motionTexel;
    unjitteredPreviousTexel = (vec2<f32>(_e146) - _e148);
    let _e150 = unjitteredPreviousTexel;
    let _e151 = jitterTexel;
    previousTexel = (_e150 + _e151);
    let _e153 = previousTexel;
    previousCenter = (_e153 + vec2(0.5f));
    let _e156 = previousCenter;
    let _e158 = all((_e156 >= vec2<f32>(0f, 0f)));
    phi_1026_ = _e158;
    if _e158 {
        let _e159 = previousCenter;
        let _e161 = pc.extent;
        phi_1026_ = all((_e159 < vec2<f32>(_e161)));
    }
    let _e166 = phi_1026_;
    inside = _e166;
    let _e167 = current;
    param_41 = _e167;
    let _e168 = finite4_u0028_vf4_u003b((&param_41));
    phi_1035_ = _e168;
    if _e168 {
        let _e169 = valid;
        param_42 = _e169;
        let _e170 = finite1_u0028_f1_u003b((&param_42));
        phi_1035_ = _e170;
    }
    let _e172 = phi_1035_;
    let _e173 = valid;
    let _e175 = (_e172 && (_e173 > 0f));
    phi_1044_ = _e175;
    if _e175 {
        let _e176 = velocity;
        param_43 = _e176;
        let _e177 = finite2_u0028_vf2_u003b((&param_43));
        phi_1044_ = _e177;
    }
    let _e179 = phi_1044_;
    phi_1050_ = _e179;
    if _e179 {
        let _e180 = previousTexel;
        param_44 = _e180;
        let _e181 = finite2_u0028_vf2_u003b((&param_44));
        phi_1050_ = _e181;
    }
    let _e183 = phi_1050_;
    let _e184 = inside;
    if (_e183 && _e184) {
        let _e186 = pixel_1;
        let _e187 = textureLoad(currentDeviceDepth, _e186, 0i);
        currentDevice = _e187.x;
        let _e189 = currentDevice;
        param_45 = _e189;
        let _e190 = linearizeDepth_u0028_f1_u003b((&param_45));
        currentLinear = _e190;
        let _e191 = previousTexel;
        param_46 = _e191;
        let _e192 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_46));
        priorLinear = _e192;
        let _e194 = pc.depthThresholdAbsolute;
        let _e196 = pc.depthThresholdRelative;
        let _e197 = currentLinear;
        let _e198 = priorLinear;
        depthLimit = max(_e194, (_e196 * max(_e197, _e198)));
        let _e202 = currentLinear;
        param_47 = _e202;
        let _e203 = finite1_u0028_f1_u003b((&param_47));
        phi_1092_ = _e203;
        if _e203 {
            let _e204 = priorLinear;
            param_48 = _e204;
            let _e205 = finite1_u0028_f1_u003b((&param_48));
            phi_1092_ = _e205;
        }
        let _e207 = phi_1092_;
        let _e208 = currentLinear;
        let _e211 = priorLinear;
        let _e213 = ((_e207 && (_e208 > 0f)) && (_e211 > 0f));
        phi_1107_ = _e213;
        if _e213 {
            let _e214 = currentLinear;
            let _e215 = priorLinear;
            let _e218 = depthLimit;
            phi_1107_ = (abs((_e214 - _e215)) <= _e218);
        }
        let _e221 = phi_1107_;
        if _e221 {
            let _e222 = previousTexel;
            param_49 = _e222;
            let _e223 = samplePreviousColorLinear_u0028_vf2_u003b((&param_49));
            prior = _e223;
            neighborhoodMin = vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f);
            neighborhoodMax = vec3<f32>(-340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f);
            y_3 = -1i;
            loop {
                let _e224 = y_3;
                if (_e224 <= 1i) {
                    x_3 = -1i;
                    loop {
                        let _e226 = x_3;
                        if (_e226 <= 1i) {
                            let _e228 = pixel_1;
                            let _e229 = x_3;
                            let _e230 = y_3;
                            param_50 = (_e228 + vec2<i32>(_e229, _e230));
                            let _e233 = sampleCurrent_u0028_vi2_u003b((&param_50));
                            c_1 = _e233.xyz;
                            let _e235 = neighborhoodMin;
                            let _e236 = c_1;
                            neighborhoodMin = min(_e235, _e236);
                            let _e238 = neighborhoodMax;
                            let _e239 = c_1;
                            neighborhoodMax = max(_e238, _e239);
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e241 = x_3;
                            x_3 = (_e241 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e243 = y_3;
                    y_3 = (_e243 + 1i);
                }
            }
            let _e245 = prior;
            param_51 = _e245;
            let _e246 = finite4_u0028_vf4_u003b((&param_51));
            phi_1161_ = _e246;
            if _e246 {
                let _e247 = neighborhoodMin;
                param_52 = _e247;
                let _e248 = finite3_u0028_vf3_u003b((&param_52));
                phi_1161_ = _e248;
            }
            let _e250 = phi_1161_;
            phi_1167_ = _e250;
            if _e250 {
                let _e251 = neighborhoodMax;
                param_53 = _e251;
                let _e252 = finite3_u0028_vf3_u003b((&param_53));
                phi_1167_ = _e252;
            }
            let _e254 = phi_1167_;
            if _e254 {
                let _e255 = prior;
                let _e257 = neighborhoodMin;
                let _e258 = neighborhoodMax;
                clampedPrior = clamp(_e255.xyz, _e257, _e258);
                let _e260 = valid;
                confidence_1 = clamp(_e260, 0f, 1f);
                let _e263 = pc.historyWeight;
                baseWeight = clamp(_e263, 0f, 1f);
                let _e265 = baseWeight;
                param_54 = _e265;
                let _e266 = confidence_1;
                param_55 = _e266;
                let _e267 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_54), (&param_55));
                weight_1 = _e267;
                let _e268 = current;
                param_56 = _e268.xyz;
                let _e270 = clampedPrior;
                param_57 = _e270;
                let _e271 = weight_1;
                param_58 = _e271;
                let _e272 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_56), (&param_57), (&param_58));
                resolvedRgb = _e272;
                let _e273 = resolvedRgb;
                let _e275 = current[3u];
                result_4 = vec4<f32>(_e273.x, _e273.y, _e273.z, _e275);
                usedHistory = true;
            }
        }
    }
    let _e280 = usedHistory;
    let _e281 = !(_e280);
    phi_1214_ = _e281;
    if _e281 {
        let _e282 = current;
        param_59 = _e282;
        let _e283 = finite4_u0028_vf4_u003b((&param_59));
        phi_1214_ = _e283;
    }
    let _e285 = phi_1214_;
    if _e285 {
        let _e286 = pixel_1;
        param_60 = _e286;
        let _e287 = current;
        param_61 = _e287;
        let _e288 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_60), (&param_61));
        result_4 = _e288;
    }
    let _e289 = result_4;
    param_62 = _e289;
    let _e290 = finite4_u0028_vf4_u003b((&param_62));
    if !(_e290) {
        result_4 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e292 = pixel_1;
    let _e293 = result_4;
    textureStore(resolvedColor, _e292, _e293);
    let _e294 = pixel_1;
    let _e295 = result_4;
    textureStore(historyOutColor, _e294, _e295);
    let _e296 = pixel_1;
    let _e297 = pixel_1;
    let _e298 = textureLoad(currentDeviceDepth, _e297, 0i);
    param_63 = _e298.x;
    let _e300 = storeLinearDepth_u0028_f1_u003b((&param_63));
    textureStore(historyOutLinearDepth, _e296, vec4(_e300));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
