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
        phi_59_ = (abs(_e50) <= 0x1.fffffep+127f);
    }
    let _e54 = phi_59_;
    return _e54;
}

fn edgeLuma_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> f32 {
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

fn edgeLumaAt_u0028_vi2_u003b(p: ptr<function, vec2<i32>>) -> f32 {
    var param_1: vec2<i32>;
    var param_2: vec3<f32>;

    let _e49 = (*p);
    param_1 = _e49;
    let _e50 = sampleCurrent_u0028_vi2_u003b((&param_1));
    param_2 = _e50.xyz;
    let _e52 = edgeLuma_u0028_vf3_u003b((&param_2));
    return _e52;
}

fn edgeMidLuma_u0028_vi2_u003b_vi2_u003b(base: ptr<function, vec2<i32>>, across: ptr<function, vec2<i32>>) -> f32 {
    var param_3: vec2<i32>;
    var param_4: vec2<i32>;

    let _e50 = (*base);
    param_3 = _e50;
    let _e51 = edgeLumaAt_u0028_vi2_u003b((&param_3));
    let _e52 = (*base);
    let _e53 = (*across);
    param_4 = (_e52 + _e53);
    let _e55 = edgeLumaAt_u0028_vi2_u003b((&param_4));
    return (0.5f * (_e51 + _e55));
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

    let _e119 = (*centerColor);
    param_5 = _e119.xyz;
    let _e121 = edgeLuma_u0028_vf3_u003b((&param_5));
    edgeM = _e121;
    let _e122 = (*px);
    param_6 = (_e122 + vec2<i32>(0i, -1i));
    let _e124 = edgeLumaAt_u0028_vi2_u003b((&param_6));
    edgeN = _e124;
    let _e125 = (*px);
    param_7 = (_e125 + vec2<i32>(0i, 1i));
    let _e127 = edgeLumaAt_u0028_vi2_u003b((&param_7));
    edgeS = _e127;
    let _e128 = (*px);
    param_8 = (_e128 + vec2<i32>(-1i, 0i));
    let _e130 = edgeLumaAt_u0028_vi2_u003b((&param_8));
    edgeW = _e130;
    let _e131 = (*px);
    param_9 = (_e131 + vec2<i32>(1i, 0i));
    let _e133 = edgeLumaAt_u0028_vi2_u003b((&param_9));
    edgeE = _e133;
    let _e134 = edgeN;
    let _e135 = edgeS;
    let _e137 = edgeW;
    let _e138 = edgeE;
    let _e140 = edgeM;
    edgeHi = max(max(_e134, _e135), max(max(_e137, _e138), _e140));
    let _e143 = edgeN;
    let _e144 = edgeS;
    let _e146 = edgeW;
    let _e147 = edgeE;
    let _e149 = edgeM;
    edgeLo = min(min(_e143, _e144), min(min(_e146, _e147), _e149));
    let _e152 = edgeHi;
    let _e153 = edgeLo;
    edgeSpan = (_e152 - _e153);
    let _e155 = edgeSpan;
    let _e156 = edgeHi;
    if (_e155 < max(0.0312f, (_e156 * 0.125f))) {
        let _e160 = (*centerColor);
        return _e160;
    }
    let _e161 = (*px);
    param_10 = (_e161 + vec2<i32>(-1i, -1i));
    let _e163 = edgeLumaAt_u0028_vi2_u003b((&param_10));
    edgeNW = _e163;
    let _e164 = (*px);
    param_11 = (_e164 + vec2<i32>(1i, -1i));
    let _e166 = edgeLumaAt_u0028_vi2_u003b((&param_11));
    edgeNE = _e166;
    let _e167 = (*px);
    param_12 = (_e167 + vec2<i32>(-1i, 1i));
    let _e169 = edgeLumaAt_u0028_vi2_u003b((&param_12));
    edgeSW = _e169;
    let _e170 = (*px);
    param_13 = (_e170 + vec2<i32>(1i, 1i));
    let _e172 = edgeLumaAt_u0028_vi2_u003b((&param_13));
    edgeSE = _e172;
    let _e173 = edgeN;
    let _e174 = edgeS;
    edgeNS = (_e173 + _e174);
    let _e176 = edgeW;
    let _e177 = edgeE;
    edgeWE = (_e176 + _e177);
    let _e179 = edgeNW;
    let _e180 = edgeSW;
    edgeNWSW = (_e179 + _e180);
    let _e182 = edgeNE;
    let _e183 = edgeSE;
    edgeNESE = (_e182 + _e183);
    let _e185 = edgeW;
    let _e187 = edgeNWSW;
    let _e190 = edgeM;
    let _e192 = edgeNS;
    let _e196 = edgeE;
    let _e198 = edgeNESE;
    edgeHorz = (abs(((-2f * _e185) + _e187)) + ((abs(((-2f * _e190) + _e192)) * 2f) + abs(((-2f * _e196) + _e198))));
    let _e203 = edgeS;
    let _e205 = edgeSW;
    let _e206 = edgeSE;
    let _e210 = edgeM;
    let _e212 = edgeWE;
    let _e216 = edgeN;
    let _e218 = edgeNW;
    let _e219 = edgeNE;
    edgeVert = (abs(((-2f * _e203) + (_e205 + _e206))) + ((abs(((-2f * _e210) + _e212)) * 2f) + abs(((-2f * _e216) + (_e218 + _e219)))));
    let _e225 = edgeHorz;
    let _e226 = edgeVert;
    edgeHorzSpan = (_e225 >= _e226);
    let _e228 = edgeNS;
    let _e229 = edgeWE;
    let _e232 = edgeNWSW;
    let _e233 = edgeNESE;
    edgeAround = (((_e228 + _e229) * 2f) + (_e232 + _e233));
    let _e236 = edgeHorzSpan;
    let _e237 = edgeN;
    let _e238 = edgeW;
    edgeLoSide = select(_e238, _e237, _e236);
    let _e240 = edgeHorzSpan;
    let _e241 = edgeS;
    let _e242 = edgeE;
    edgeHiSide = select(_e242, _e241, _e240);
    let _e244 = edgeLoSide;
    let _e245 = edgeM;
    edgeGradLo = (_e244 - _e245);
    let _e247 = edgeHiSide;
    let _e248 = edgeM;
    edgeGradHi = (_e247 - _e248);
    let _e250 = edgeGradLo;
    let _e252 = edgeGradHi;
    edgeLoSteeper = (abs(_e250) >= abs(_e252));
    let _e255 = edgeGradLo;
    let _e257 = edgeGradHi;
    edgeGradient = max(abs(_e255), abs(_e257));
    let _e260 = edgeLoSteeper;
    if _e260 {
        let _e261 = edgeLoSide;
        let _e262 = edgeM;
        local = (_e261 + _e262);
    } else {
        let _e264 = edgeHiSide;
        let _e265 = edgeM;
        local = (_e264 + _e265);
    }
    let _e267 = local;
    edgePair = _e267;
    let _e268 = edgeHorzSpan;
    edgeAcross = select(vec2<i32>(1i, 0i), vec2<i32>(0i, 1i), vec2(_e268));
    let _e271 = edgeHorzSpan;
    edgeAlong = select(vec2<i32>(0i, 1i), vec2<i32>(1i, 0i), vec2(_e271));
    let _e274 = edgeLoSteeper;
    if _e274 {
        let _e275 = edgeAcross;
        edgeAcross = -(_e275);
    }
    let _e277 = edgeSpan;
    let _e278 = edgeSpan;
    edgeInvSpan = inverseSqrt((_e277 * _e278));
    let _e281 = edgeAround;
    let _e283 = edgeM;
    let _e286 = edgeInvSpan;
    edgeBlendC = clamp((abs(((_e281 * 0.083333336f) - _e283)) * _e286), 0f, 1f);
    let _e289 = edgeBlendC;
    let _e292 = edgeBlendC;
    let _e293 = edgeBlendC;
    edgeBlendF = (((-2f * _e289) + 3f) * (_e292 * _e293));
    let _e296 = edgeBlendF;
    let _e297 = edgeBlendF;
    edgeSubpixel = ((_e296 * _e297) * 0.75f);
    let _e300 = edgePair;
    edgeHalfPair = (_e300 * 0.5f);
    let _e302 = edgeGradient;
    edgeScaled = (_e302 * 0.25f);
    let _e304 = edgeM;
    let _e305 = edgeHalfPair;
    edgeMidNegative = ((_e304 - _e305) < 0f);
    let _e308 = (*px);
    let _e309 = edgeAlong;
    param_14 = (_e308 - _e309);
    let _e311 = edgeAcross;
    param_15 = _e311;
    let _e312 = edgeMidLuma_u0028_vi2_u003b_vi2_u003b((&param_14), (&param_15));
    let _e313 = edgeHalfPair;
    edgeEndN = (_e312 - _e313);
    let _e315 = (*px);
    let _e316 = edgeAlong;
    param_16 = (_e315 + _e316);
    let _e318 = edgeAcross;
    param_17 = _e318;
    let _e319 = edgeMidLuma_u0028_vi2_u003b_vi2_u003b((&param_16), (&param_17));
    let _e320 = edgeHalfPair;
    edgeEndP = (_e319 - _e320);
    let _e322 = edgeEndN;
    let _e324 = edgeScaled;
    edgeDoneN = (abs(_e322) >= _e324);
    let _e326 = edgeEndP;
    let _e328 = edgeScaled;
    edgeDoneP = (abs(_e326) >= _e328);
    edgeStepsN = 1i;
    edgeStepsP = 1i;
    i = 2i;
    loop {
        let _e330 = i;
        if (_e330 <= 12i) {
            let _e332 = edgeDoneN;
            if !(_e332) {
                let _e334 = (*px);
                let _e335 = edgeAlong;
                let _e336 = i;
                param_18 = (_e334 - (_e335 * vec2(_e336)));
                let _e340 = edgeAcross;
                param_19 = _e340;
                let _e341 = edgeMidLuma_u0028_vi2_u003b_vi2_u003b((&param_18), (&param_19));
                let _e342 = edgeHalfPair;
                edgeEndN = (_e341 - _e342);
                let _e344 = edgeEndN;
                let _e346 = edgeScaled;
                edgeDoneN = (abs(_e344) >= _e346);
                let _e348 = i;
                edgeStepsN = _e348;
            }
            let _e349 = edgeDoneP;
            if !(_e349) {
                let _e351 = (*px);
                let _e352 = edgeAlong;
                let _e353 = i;
                param_20 = (_e351 + (_e352 * vec2(_e353)));
                let _e357 = edgeAcross;
                param_21 = _e357;
                let _e358 = edgeMidLuma_u0028_vi2_u003b_vi2_u003b((&param_20), (&param_21));
                let _e359 = edgeHalfPair;
                edgeEndP = (_e358 - _e359);
                let _e361 = edgeEndP;
                let _e363 = edgeScaled;
                edgeDoneP = (abs(_e361) >= _e363);
                let _e365 = i;
                edgeStepsP = _e365;
            }
            let _e366 = edgeDoneN;
            let _e367 = edgeDoneP;
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
    let _e371 = edgeStepsN;
    let _e372 = edgeStepsP;
    edgeTowardN = (_e371 < _e372);
    let _e374 = edgeTowardN;
    let _e375 = edgeEndN;
    let _e376 = edgeEndP;
    edgeEndG = select(_e376, _e375, _e374);
    let _e378 = edgeEndG;
    let _e380 = edgeMidNegative;
    edgeGoodSpan = ((_e378 < 0f) != _e380);
    let _e382 = edgeStepsN;
    let _e383 = edgeStepsP;
    edgeSpanLen = f32((_e382 + _e383));
    let _e386 = edgeTowardN;
    let _e387 = edgeStepsN;
    let _e388 = edgeStepsP;
    edgeNear = f32(select(_e388, _e387, _e386));
    let _e391 = edgeGoodSpan;
    if _e391 {
        let _e392 = edgeNear;
        let _e393 = edgeSpanLen;
        let _e394 = edgeSpanLen;
        local_1 = (0.5f - (_e392 * inverseSqrt((_e393 * _e394))));
    } else {
        local_1 = 0f;
    }
    let _e399 = local_1;
    edgeStepBlend = _e399;
    let _e400 = edgeStepBlend;
    let _e401 = edgeSubpixel;
    edgeBlend = clamp(max(_e400, _e401), 0f, 1f);
    let _e404 = (*px);
    let _e405 = edgeAcross;
    param_22 = (_e404 + _e405);
    let _e407 = sampleCurrent_u0028_vi2_u003b((&param_22));
    edgeAcrossColor = _e407.xyz;
    let _e409 = (*centerColor);
    let _e411 = edgeBlend;
    let _e414 = edgeAcrossColor;
    let _e415 = edgeBlend;
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
        phi_74_ = all((abs(_e51) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
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
        let _e70 = spatialEdgeFilter_u0028_vi2_u003b_vf4_u003b((&param_24), (&param_25));
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
    let _e79 = _e78.xyz;
    textureStore(historyOutColor, _e77, vec4<f32>(_e79.x, _e79.y, _e79.z, 1f));
    let _e84 = pixel_1;
    let _e85 = pixel_1;
    let _e86 = textureLoad(currentDeviceDepth, _e85, 0i);
    param_27 = _e86.x;
    let _e88 = storeLinearDepth_u0028_f1_u003b((&param_27));
    textureStore(historyOutLinearDepth, _e84, vec4(_e88));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
