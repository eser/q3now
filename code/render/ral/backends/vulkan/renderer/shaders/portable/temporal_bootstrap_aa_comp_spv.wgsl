struct TemporalBootstrapPush {
    extent: vec2<u32>,
    zNear: f32,
    zFar: f32,
}

@group(0) @binding(6)
var<uniform> pc: TemporalBootstrapPush;
@group(0) @binding(0)
var currentColor: texture_2d<f32>;
@group(0) @binding(2)
var nearestSampler: sampler;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var bootstrapColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(4)
var historyOutColor: texture_storage_2d<rgba16float,write>;
@group(0) @binding(5)
var historyOutLinearDepth: texture_storage_2d<r32float,write>;
@group(0) @binding(1)
var currentDeviceDepth: texture_2d<f32>;

fn storeLinearDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    var denominator: f32;

    let _e49 = pc.zNear;
    let _e50 = (*z);
    let _e52 = pc.zFar;
    let _e54 = pc.zNear;
    denominator = (_e49 + (_e50 * (_e52 - _e54)));
    let _e59 = pc.zNear;
    let _e61 = pc.zFar;
    let _e63 = denominator;
    let _e64 = denominator;
    return ((_e59 * _e61) * inverseSqrt((_e63 * _e64)));
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e48 = (*pixel);
    let _e50 = pc.extent;
    bounded = clamp(_e48, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e50) - vec2<i32>(1i, 1i)));
    let _e54 = bounded;
    let _e55 = textureLoad(currentColor, _e54, 0i);
    return _e55;
}

fn finite1_u0028_f1_u003b(v: ptr<function, f32>) -> bool {
    var phi_59_: bool;

    let _e47 = (*v);
    let _e48 = (*v);
    let _e49 = (_e47 == _e48);
    phi_59_ = _e49;
    if _e49 {
        let _e50 = (*v);
        phi_59_ = (abs(_e50) <= 340282350000000000000000000000000000000f);
    }
    let _e54 = phi_59_;
    return _e54;
}

fn fxLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
    var l: f32;
    var param: f32;

    let _e49 = (*c);
    l = dot(_e49, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e51 = l;
    param = _e51;
    let _e52 = finite1_u0028_f1_u003b((&param));
    if !(_e52) {
        return 0f;
    }
    let _e54 = l;
    l = clamp(_e54, 0f, 10000f);
    let _e56 = l;
    let _e57 = l;
    let _e58 = l;
    return (_e56 * inverseSqrt((1f + (_e57 * _e58))));
}

fn fxLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e49 = (*p);
    param_1 = _e49;
    let _e50 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e50.xyz;
    let _e52 = fxLuma_u0028_vf3_u003b((&param_2));
    return _e52;
}

fn fxEdgeLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e50 = (*base);
    param_3 = _e50;
    let _e51 = fxLumaAt_u0028_vi2_u003b((&param_3));
    let _e52 = (*base);
    let _e53 = (*across);
    param_4 = (_e52 + _e53);
    let _e55 = fxLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e51 + _e55));
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

    let _e119 = (*centerColor);
    param_5 = _e119.xyz;
    let _e121 = fxLuma_u0028_vf3_u003b((&param_5));
    fxM = _e121;
    let _e122 = (*px);
    param_6 = (_e122 + vec2<i32>(0i, -1i));
    let _e124 = fxLumaAt_u0028_vi2_u003b((&param_6));
    fxN = _e124;
    let _e125 = (*px);
    param_7 = (_e125 + vec2<i32>(0i, 1i));
    let _e127 = fxLumaAt_u0028_vi2_u003b((&param_7));
    fxS = _e127;
    let _e128 = (*px);
    param_8 = (_e128 + vec2<i32>(-1i, 0i));
    let _e130 = fxLumaAt_u0028_vi2_u003b((&param_8));
    fxW = _e130;
    let _e131 = (*px);
    param_9 = (_e131 + vec2<i32>(1i, 0i));
    let _e133 = fxLumaAt_u0028_vi2_u003b((&param_9));
    fxE = _e133;
    let _e134 = fxN;
    let _e135 = fxS;
    let _e137 = fxW;
    let _e138 = fxE;
    let _e140 = fxM;
    fxHi = max(max(_e134, _e135), max(max(_e137, _e138), _e140));
    let _e143 = fxN;
    let _e144 = fxS;
    let _e146 = fxW;
    let _e147 = fxE;
    let _e149 = fxM;
    fxLo = min(min(_e143, _e144), min(min(_e146, _e147), _e149));
    let _e152 = fxHi;
    let _e153 = fxLo;
    fxSpan = (_e152 - _e153);
    let _e155 = fxSpan;
    let _e156 = fxHi;
    if (_e155 < max(0.0312f, (_e156 * 0.125f))) {
        let _e160 = (*centerColor);
        return _e160;
    }
    let _e161 = (*px);
    param_10 = (_e161 + vec2<i32>(-1i, -1i));
    let _e163 = fxLumaAt_u0028_vi2_u003b((&param_10));
    fxNW = _e163;
    let _e164 = (*px);
    param_11 = (_e164 + vec2<i32>(1i, -1i));
    let _e166 = fxLumaAt_u0028_vi2_u003b((&param_11));
    fxNE = _e166;
    let _e167 = (*px);
    param_12 = (_e167 + vec2<i32>(-1i, 1i));
    let _e169 = fxLumaAt_u0028_vi2_u003b((&param_12));
    fxSW = _e169;
    let _e170 = (*px);
    param_13 = (_e170 + vec2<i32>(1i, 1i));
    let _e172 = fxLumaAt_u0028_vi2_u003b((&param_13));
    fxSE = _e172;
    let _e173 = fxN;
    let _e174 = fxS;
    fxNS = (_e173 + _e174);
    let _e176 = fxW;
    let _e177 = fxE;
    fxWE = (_e176 + _e177);
    let _e179 = fxNW;
    let _e180 = fxSW;
    fxNWSW = (_e179 + _e180);
    let _e182 = fxNE;
    let _e183 = fxSE;
    fxNESE = (_e182 + _e183);
    let _e185 = fxW;
    let _e187 = fxNWSW;
    let _e190 = fxM;
    let _e192 = fxNS;
    let _e196 = fxE;
    let _e198 = fxNESE;
    fxHorz = (abs(((-2f * _e185) + _e187)) + ((abs(((-2f * _e190) + _e192)) * 2f) + abs(((-2f * _e196) + _e198))));
    let _e203 = fxS;
    let _e205 = fxSW;
    let _e206 = fxSE;
    let _e210 = fxM;
    let _e212 = fxWE;
    let _e216 = fxN;
    let _e218 = fxNW;
    let _e219 = fxNE;
    fxVert = (abs(((-2f * _e203) + (_e205 + _e206))) + ((abs(((-2f * _e210) + _e212)) * 2f) + abs(((-2f * _e216) + (_e218 + _e219)))));
    let _e225 = fxHorz;
    let _e226 = fxVert;
    fxHorzSpan = (_e225 >= _e226);
    let _e228 = fxNS;
    let _e229 = fxWE;
    let _e232 = fxNWSW;
    let _e233 = fxNESE;
    fxAround = (((_e228 + _e229) * 2f) + (_e232 + _e233));
    let _e236 = fxHorzSpan;
    let _e237 = fxN;
    let _e238 = fxW;
    fxLoSide = select(_e238, _e237, _e236);
    let _e240 = fxHorzSpan;
    let _e241 = fxS;
    let _e242 = fxE;
    fxHiSide = select(_e242, _e241, _e240);
    let _e244 = fxLoSide;
    let _e245 = fxM;
    fxGradLo = (_e244 - _e245);
    let _e247 = fxHiSide;
    let _e248 = fxM;
    fxGradHi = (_e247 - _e248);
    let _e250 = fxGradLo;
    let _e252 = fxGradHi;
    fxLoSteeper = (abs(_e250) >= abs(_e252));
    let _e255 = fxGradLo;
    let _e257 = fxGradHi;
    fxGradient = max(abs(_e255), abs(_e257));
    let _e260 = fxLoSteeper;
    if _e260 {
        let _e261 = fxLoSide;
        let _e262 = fxM;
        local = (_e261 + _e262);
    } else {
        let _e264 = fxHiSide;
        let _e265 = fxM;
        local = (_e264 + _e265);
    }
    let _e267 = local;
    fxPair = _e267;
    let _e268 = fxHorzSpan;
    fxAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e268));
    let _e271 = fxHorzSpan;
    fxAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e271));
    let _e274 = fxLoSteeper;
    if _e274 {
        let _e275 = fxAcross;
        fxAcross = -(_e275);
    }
    let _e277 = fxSpan;
    let _e278 = fxSpan;
    fxInvSpan = inverseSqrt((_e277 * _e278));
    let _e281 = fxAround;
    let _e283 = fxM;
    let _e286 = fxInvSpan;
    fxBlendC = clamp((abs(((_e281 * 0.083333336f) - _e283)) * _e286), 0f, 1f);
    let _e289 = fxBlendC;
    let _e292 = fxBlendC;
    let _e293 = fxBlendC;
    fxBlendF = (((-2f * _e289) + 3f) * (_e292 * _e293));
    let _e296 = fxBlendF;
    let _e297 = fxBlendF;
    fxSubpixel = ((_e296 * _e297) * 0.75f);
    let _e300 = fxPair;
    fxHalfPair = (_e300 * 0.5f);
    let _e302 = fxGradient;
    fxScaled = (_e302 * 0.25f);
    let _e304 = fxM;
    let _e305 = fxHalfPair;
    fxMidNegative = ((_e304 - _e305) < 0f);
    let _e308 = (*px);
    let _e309 = fxAlong;
    param_14 = (_e308 - _e309);
    let _e311 = fxAcross;
    param_15 = _e311;
    let _e312 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e313 = fxHalfPair;
    fxEndN = (_e312 - _e313);
    let _e315 = (*px);
    let _e316 = fxAlong;
    param_16 = (_e315 + _e316);
    let _e318 = fxAcross;
    param_17 = _e318;
    let _e319 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e320 = fxHalfPair;
    fxEndP = (_e319 - _e320);
    let _e322 = fxEndN;
    let _e324 = fxScaled;
    fxDoneN = (abs(_e322) >= _e324);
    let _e326 = fxEndP;
    let _e328 = fxScaled;
    fxDoneP = (abs(_e326) >= _e328);
    fxStepsN = 1i;
    fxStepsP = 1i;
    i = 2i;
    loop {
        let _e330 = i;
        if (_e330 <= 12i) {
            let _e332 = fxDoneN;
            if !(_e332) {
                let _e334 = (*px);
                let _e335 = fxAlong;
                let _e336 = i;
                param_18 = (_e334 - (_e335 * vec2(_e336)));
                let _e340 = fxAcross;
                param_19 = _e340;
                let _e341 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e342 = fxHalfPair;
                fxEndN = (_e341 - _e342);
                let _e344 = fxEndN;
                let _e346 = fxScaled;
                fxDoneN = (abs(_e344) >= _e346);
                let _e348 = i;
                fxStepsN = _e348;
            }
            let _e349 = fxDoneP;
            if !(_e349) {
                let _e351 = (*px);
                let _e352 = fxAlong;
                let _e353 = i;
                param_20 = (_e351 + (_e352 * vec2(_e353)));
                let _e357 = fxAcross;
                param_21 = _e357;
                let _e358 = fxEdgeLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e359 = fxHalfPair;
                fxEndP = (_e358 - _e359);
                let _e361 = fxEndP;
                let _e363 = fxScaled;
                fxDoneP = (abs(_e361) >= _e363);
                let _e365 = i;
                fxStepsP = _e365;
            }
            let _e366 = fxDoneN;
            let _e367 = fxDoneP;
            if (_e366 && _e367) {
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e369 = i;
            i = (_e369 + 1i);
        }
    }
    let _e371 = fxStepsN;
    let _e372 = fxStepsP;
    fxTowardN = (_e371 < _e372);
    let _e374 = fxTowardN;
    let _e375 = fxEndN;
    let _e376 = fxEndP;
    fxEndG = select(_e376, _e375, _e374);
    let _e378 = fxEndG;
    let _e380 = fxMidNegative;
    fxGoodSpan = ((_e378 < 0f) != _e380);
    let _e382 = fxStepsN;
    let _e383 = fxStepsP;
    fxSpanLen = f32((_e382 + _e383));
    let _e386 = fxTowardN;
    let _e387 = fxStepsN;
    let _e388 = fxStepsP;
    fxNear = f32(select(_e388, _e387, _e386));
    let _e391 = fxGoodSpan;
    if _e391 {
        let _e392 = fxNear;
        let _e393 = fxSpanLen;
        let _e394 = fxSpanLen;
        local_1 = (0.5f - (_e392 * inverseSqrt((_e393 * _e394))));
    } else {
        local_1 = 0f;
    }
    let _e399 = local_1;
    fxEdgeBlend = _e399;
    let _e400 = fxEdgeBlend;
    let _e401 = fxSubpixel;
    fxBlend = clamp(max(_e400, _e401), 0f, 1f);
    let _e404 = (*px);
    let _e405 = fxAcross;
    param_22 = (_e404 + _e405);
    let _e407 = sampleCurrent_u0028_vi2_u003b((&param_22));
    fxAcrossColor = _e407.xyz;
    let _e409 = (*centerColor);
    let _e411 = fxBlend;
    let _e414 = fxAcrossColor;
    let _e415 = fxBlend;
    let _e417 = ((_e409.xyz * (1f - _e411)) + (_e414 * _e415));
    let _e419 = (*centerColor)[3u];
    return vec4<f32>(_e417.x, _e417.y, _e417.z, _e419);
}

fn finite4_u0028_vf4_u003b(v_1: ptr<function, vec4<f32>>) -> bool {
    var phi_74_: bool;

    let _e47 = (*v_1);
    let _e48 = (*v_1);
    let _e50 = all((_e47 == _e48));
    phi_74_ = _e50;
    if _e50 {
        let _e51 = (*v_1);
        phi_74_ = all((abs(_e51) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e56 = phi_74_;
    return _e56;
}

fn main_1() {
    var pixel_1: vec2<i32>;
    var current: vec4<f32>;
    var result: vec4<f32>;
    var param_23: vec4<f32>;
    var local_2: vec4<f32>;
    var param_24: vec2<i32>;
    var param_25: vec4<f32>;
    var param_26: vec4<f32>;
    var param_27: f32;

    let _e55 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e55.xy);
    let _e58 = pixel_1;
    let _e60 = pc.extent;
    if any((_e58 >= bitcast<vec2<i32>>(_e60))) {
        return;
    }
    let _e64 = pixel_1;
    let _e65 = textureLoad(currentColor, _e64, 0i);
    current = _e65;
    let _e66 = current;
    param_23 = _e66;
    let _e67 = finite4_u0028_vf4_u003b((&param_23));
    if _e67 {
        let _e68 = pixel_1;
        param_24 = _e68;
        let _e69 = current;
        param_25 = _e69;
        let _e70 = fxSpatialAa_u0028_vi2_u003b_vf4_u003b((&param_24), (&param_25));
        local_2 = _e70;
    } else {
        local_2 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e71 = local_2;
    result = _e71;
    let _e72 = result;
    param_26 = _e72;
    let _e73 = finite4_u0028_vf4_u003b((&param_26));
    if !(_e73) {
        result = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e75 = pixel_1;
    let _e76 = result;
    textureStore(bootstrapColor, _e75, _e76);
    let _e77 = pixel_1;
    let _e78 = result;
    textureStore(historyOutColor, _e77, _e78);
    let _e79 = pixel_1;
    let _e80 = pixel_1;
    let _e81 = textureLoad(currentDeviceDepth, _e80, 0i);
    param_27 = _e81.x;
    let _e83 = storeLinearDepth_u0028_f1_u003b((&param_27));
    textureStore(historyOutLinearDepth, _e79, vec4(_e83));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
