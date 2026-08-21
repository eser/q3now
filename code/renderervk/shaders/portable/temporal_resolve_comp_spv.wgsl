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

@group(0) @binding(8)
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

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e46 = (*a);
    oneMinus = (1f - _e46);
    let _e48 = (*x);
    let _e49 = oneMinus;
    left = (_e48 * _e49);
    let _e51 = (*y);
    let _e52 = (*a);
    right = (_e51 * _e52);
    let _e54 = left;
    let _e55 = right;
    result = (_e54 + _e55);
    let _e57 = result;
    return _e57;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e42 = (*weight);
    let _e43 = (*confidence);
    scaled = (_e42 * _e43);
    let _e45 = scaled;
    return _e45;
}

fn finite3_u0028_vf3_u003b(v: ptr<function, vec3<f32>>) -> bool {
    var phi_112_: bool;

    let _e40 = (*v);
    let _e41 = (*v);
    let _e43 = all((_e40 == _e41));
    phi_112_ = _e43;
    if _e43 {
        let _e44 = (*v);
        phi_112_ = all((abs(_e44) <= vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e49 = phi_112_;
    return _e49;
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e41 = (*pixel);
    let _e43 = pc.extent;
    bounded = clamp(_e41, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e43) - vec2<i32>(1i, 1i)));
    let _e47 = bounded;
    let _e48 = textureLoad(currentColor, _e47, 0i);
    return _e48;
}

fn mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b(x_1: ptr<function, vec4<f32>>, y_1: ptr<function, vec4<f32>>, a_1: ptr<function, f32>) -> vec4<f32> {
    var oneMinus_1: f32;
    var left_1: vec4<f32>;
    var right_1: vec4<f32>;
    var result_1: vec4<f32>;

    let _e46 = (*a_1);
    oneMinus_1 = (1f - _e46);
    let _e48 = (*x_1);
    let _e49 = oneMinus_1;
    left_1 = (_e48 * _e49);
    let _e51 = (*y_1);
    let _e52 = (*a_1);
    right_1 = (_e51 * _e52);
    let _e54 = left_1;
    let _e55 = right_1;
    result_1 = (_e54 + _e55);
    let _e57 = result_1;
    return _e57;
}

fn samplePreviousColorLinear_u0028_vf2_u003b(texel: ptr<function, vec2<f32>>) -> vec4<f32> {
    var baseFloat: vec2<f32>;
    var base: vec2<i32>;
    var f: vec2<f32>;
    var hi: vec2<i32>;
    var c00_: vec4<f32>;
    var c10_: vec4<f32>;
    var c01_: vec4<f32>;
    var c11_: vec4<f32>;
    var top: vec4<f32>;
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var param_2: f32;
    var bottom: vec4<f32>;
    var param_3: vec4<f32>;
    var param_4: vec4<f32>;
    var param_5: f32;
    var param_6: vec4<f32>;
    var param_7: vec4<f32>;
    var param_8: f32;

    let _e59 = (*texel);
    baseFloat = floor(_e59);
    let _e61 = baseFloat;
    base = vec2<i32>(_e61);
    let _e63 = (*texel);
    let _e64 = baseFloat;
    f = (_e63 - _e64);
    let _e67 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e67) - vec2<i32>(1i, 1i));
    let _e70 = base;
    let _e71 = hi;
    let _e73 = textureLoad(previousColor, clamp(_e70, vec2<i32>(0i, 0i), _e71), 0i);
    c00_ = _e73;
    let _e74 = base;
    let _e76 = hi;
    let _e78 = textureLoad(previousColor, clamp((_e74 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e76), 0i);
    c10_ = _e78;
    let _e79 = base;
    let _e81 = hi;
    let _e83 = textureLoad(previousColor, clamp((_e79 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e81), 0i);
    c01_ = _e83;
    let _e84 = base;
    let _e86 = hi;
    let _e88 = textureLoad(previousColor, clamp((_e84 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e86), 0i);
    c11_ = _e88;
    let _e89 = c00_;
    param = _e89;
    let _e90 = c10_;
    param_1 = _e90;
    let _e92 = f[0u];
    param_2 = _e92;
    let _e93 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param), (&param_1), (&param_2));
    top = _e93;
    let _e94 = c01_;
    param_3 = _e94;
    let _e95 = c11_;
    param_4 = _e95;
    let _e97 = f[0u];
    param_5 = _e97;
    let _e98 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_3), (&param_4), (&param_5));
    bottom = _e98;
    let _e99 = top;
    param_6 = _e99;
    let _e100 = bottom;
    param_7 = _e100;
    let _e102 = f[1u];
    param_8 = _e102;
    let _e103 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_6), (&param_7), (&param_8));
    return _e103;
}

fn mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b(x_2: ptr<function, f32>, y_2: ptr<function, f32>, a_2: ptr<function, f32>) -> f32 {
    var oneMinus_2: f32;
    var left_2: f32;
    var right_2: f32;
    var result_2: f32;

    let _e46 = (*a_2);
    oneMinus_2 = (1f - _e46);
    let _e48 = (*x_2);
    let _e49 = oneMinus_2;
    left_2 = (_e48 * _e49);
    let _e51 = (*y_2);
    let _e52 = (*a_2);
    right_2 = (_e51 * _e52);
    let _e54 = left_2;
    let _e55 = right_2;
    result_2 = (_e54 + _e55);
    let _e57 = result_2;
    return _e57;
}

fn samplePreviousDepthLinear_u0028_vf2_u003b(texel_1: ptr<function, vec2<f32>>) -> f32 {
    var baseFloat_1: vec2<f32>;
    var base_1: vec2<i32>;
    var f_1: vec2<f32>;
    var hi_1: vec2<i32>;
    var d00_: f32;
    var d10_: f32;
    var d01_: f32;
    var d11_: f32;
    var top_1: f32;
    var param_9: f32;
    var param_10: f32;
    var param_11: f32;
    var bottom_1: f32;
    var param_12: f32;
    var param_13: f32;
    var param_14: f32;
    var param_15: f32;
    var param_16: f32;
    var param_17: f32;

    let _e59 = (*texel_1);
    baseFloat_1 = floor(_e59);
    let _e61 = baseFloat_1;
    base_1 = vec2<i32>(_e61);
    let _e63 = (*texel_1);
    let _e64 = baseFloat_1;
    f_1 = (_e63 - _e64);
    let _e67 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e67) - vec2<i32>(1i, 1i));
    let _e70 = base_1;
    let _e71 = hi_1;
    let _e73 = textureLoad(previousLinearDepth, clamp(_e70, vec2<i32>(0i, 0i), _e71), 0i);
    d00_ = _e73.x;
    let _e75 = base_1;
    let _e77 = hi_1;
    let _e79 = textureLoad(previousLinearDepth, clamp((_e75 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e77), 0i);
    d10_ = _e79.x;
    let _e81 = base_1;
    let _e83 = hi_1;
    let _e85 = textureLoad(previousLinearDepth, clamp((_e81 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e83), 0i);
    d01_ = _e85.x;
    let _e87 = base_1;
    let _e89 = hi_1;
    let _e91 = textureLoad(previousLinearDepth, clamp((_e87 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e89), 0i);
    d11_ = _e91.x;
    let _e93 = d00_;
    param_9 = _e93;
    let _e94 = d10_;
    param_10 = _e94;
    let _e96 = f_1[0u];
    param_11 = _e96;
    let _e97 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_9), (&param_10), (&param_11));
    top_1 = _e97;
    let _e98 = d01_;
    param_12 = _e98;
    let _e99 = d11_;
    param_13 = _e99;
    let _e101 = f_1[0u];
    param_14 = _e101;
    let _e102 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
    bottom_1 = _e102;
    let _e103 = top_1;
    param_15 = _e103;
    let _e104 = bottom_1;
    param_16 = _e104;
    let _e106 = f_1[1u];
    param_17 = _e106;
    let _e107 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_15), (&param_16), (&param_17));
    return _e107;
}

fn linearizeDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator: f32;
    var numerator: f32;
    var result_3: f32;

    let _e46 = pc.zFar;
    let _e48 = pc.zNear;
    range = (_e46 - _e48);
    let _e50 = (*z);
    let _e51 = range;
    scaled_1 = (_e50 * _e51);
    let _e54 = pc.zNear;
    let _e55 = scaled_1;
    denominator = (_e54 + _e55);
    let _e58 = pc.zNear;
    let _e60 = pc.zFar;
    numerator = (_e58 * _e60);
    let _e62 = numerator;
    let _e63 = denominator;
    result_3 = (_e62 / _e63);
    let _e65 = result_3;
    return _e65;
}

fn finite2_u0028_vf2_u003b(v_1: ptr<function, vec2<f32>>) -> bool {
    var phi_97_: bool;

    let _e40 = (*v_1);
    let _e41 = (*v_1);
    let _e43 = all((_e40 == _e41));
    phi_97_ = _e43;
    if _e43 {
        let _e44 = (*v_1);
        phi_97_ = all((abs(_e44) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e49 = phi_97_;
    return _e49;
}

fn finite1_u0028_f1_u003b(v_2: ptr<function, f32>) -> bool {
    var phi_82_: bool;

    let _e40 = (*v_2);
    let _e41 = (*v_2);
    let _e42 = (_e40 == _e41);
    phi_82_ = _e42;
    if _e42 {
        let _e43 = (*v_2);
        phi_82_ = (abs(_e43) <= 340282350000000000000000000000000000000f);
    }
    let _e47 = phi_82_;
    return _e47;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_127_: bool;

    let _e40 = (*v_3);
    let _e41 = (*v_3);
    let _e43 = all((_e40 == _e41));
    phi_127_ = _e43;
    if _e43 {
        let _e44 = (*v_3);
        phi_127_ = all((abs(_e44) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e49 = phi_127_;
    return _e49;
}

fn main_1() {
    var pixel_1: vec2<i32>;
    var current: vec4<f32>;
    var result_4: vec4<f32>;
    var valid: f32;
    var velocity: vec2<f32>;
    var jitterDelta: vec2<f32>;
    var jitterTexel: vec2<f32>;
    var motionTexel: vec2<f32>;
    var unjitteredPreviousTexel: vec2<f32>;
    var previousTexel: vec2<f32>;
    var previousCenter: vec2<f32>;
    var inside: bool;
    var param_18: vec4<f32>;
    var param_19: f32;
    var param_20: vec2<f32>;
    var param_21: vec2<f32>;
    var currentDevice: f32;
    var currentLinear: f32;
    var param_22: f32;
    var priorLinear: f32;
    var param_23: vec2<f32>;
    var depthLimit: f32;
    var param_24: f32;
    var param_25: f32;
    var prior: vec4<f32>;
    var param_26: vec2<f32>;
    var neighborhoodMin: vec3<f32>;
    var neighborhoodMax: vec3<f32>;
    var y_3: i32;
    var x_3: i32;
    var c: vec3<f32>;
    var param_27: vec2<i32>;
    var param_28: vec4<f32>;
    var param_29: vec3<f32>;
    var param_30: vec3<f32>;
    var clampedPrior: vec3<f32>;
    var confidence_1: f32;
    var baseWeight: f32;
    var weight_1: f32;
    var param_31: f32;
    var param_32: f32;
    var resolvedRgb: vec3<f32>;
    var param_33: vec3<f32>;
    var param_34: vec3<f32>;
    var param_35: f32;
    var param_36: vec4<f32>;
    var phi_521_: bool;
    var phi_530_: bool;
    var phi_539_: bool;
    var phi_545_: bool;
    var phi_587_: bool;
    var phi_602_: bool;
    var phi_658_: bool;
    var phi_664_: bool;

    let _e85 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e85.xy);
    let _e88 = pixel_1;
    let _e90 = pc.extent;
    if any((_e88 >= bitcast<vec2<i32>>(_e90))) {
        return;
    }
    let _e94 = pixel_1;
    let _e95 = textureLoad(currentColor, _e94, 0i);
    current = _e95;
    let _e96 = current;
    result_4 = _e96;
    let _e97 = pixel_1;
    let _e98 = textureLoad(motionValidity, _e97, 0i);
    valid = _e98.x;
    let _e100 = pixel_1;
    let _e101 = textureLoad(motionVelocity, _e100, 0i);
    velocity = _e101.xy;
    let _e104 = pc.previousEffectiveJitterUv;
    let _e106 = pc.currentEffectiveJitterUv;
    jitterDelta = (_e104 - _e106);
    let _e108 = jitterDelta;
    let _e110 = pc.extent;
    jitterTexel = (_e108 * vec2<f32>(_e110));
    let _e113 = velocity;
    let _e115 = pc.extent;
    motionTexel = (_e113 * vec2<f32>(_e115));
    let _e118 = pixel_1;
    let _e120 = motionTexel;
    unjitteredPreviousTexel = (vec2<f32>(_e118) - _e120);
    let _e122 = unjitteredPreviousTexel;
    let _e123 = jitterTexel;
    previousTexel = (_e122 + _e123);
    let _e125 = previousTexel;
    previousCenter = (_e125 + vec2(0.5f));
    let _e128 = previousCenter;
    let _e130 = all((_e128 >= vec2<f32>(0f, 0f)));
    phi_521_ = _e130;
    if _e130 {
        let _e131 = previousCenter;
        let _e133 = pc.extent;
        phi_521_ = all((_e131 < vec2<f32>(_e133)));
    }
    let _e138 = phi_521_;
    inside = _e138;
    let _e139 = current;
    param_18 = _e139;
    let _e140 = finite4_u0028_vf4_u003b((&param_18));
    phi_530_ = _e140;
    if _e140 {
        let _e141 = valid;
        param_19 = _e141;
        let _e142 = finite1_u0028_f1_u003b((&param_19));
        phi_530_ = _e142;
    }
    let _e144 = phi_530_;
    let _e145 = valid;
    let _e147 = (_e144 && (_e145 > 0f));
    phi_539_ = _e147;
    if _e147 {
        let _e148 = velocity;
        param_20 = _e148;
        let _e149 = finite2_u0028_vf2_u003b((&param_20));
        phi_539_ = _e149;
    }
    let _e151 = phi_539_;
    phi_545_ = _e151;
    if _e151 {
        let _e152 = previousTexel;
        param_21 = _e152;
        let _e153 = finite2_u0028_vf2_u003b((&param_21));
        phi_545_ = _e153;
    }
    let _e155 = phi_545_;
    let _e156 = inside;
    if (_e155 && _e156) {
        let _e158 = pixel_1;
        let _e159 = textureLoad(currentDeviceDepth, _e158, 0i);
        currentDevice = _e159.x;
        let _e161 = currentDevice;
        param_22 = _e161;
        let _e162 = linearizeDepth_u0028_f1_u003b((&param_22));
        currentLinear = _e162;
        let _e163 = previousTexel;
        param_23 = _e163;
        let _e164 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_23));
        priorLinear = _e164;
        let _e166 = pc.depthThresholdAbsolute;
        let _e168 = pc.depthThresholdRelative;
        let _e169 = currentLinear;
        let _e170 = priorLinear;
        depthLimit = max(_e166, (_e168 * max(_e169, _e170)));
        let _e174 = currentLinear;
        param_24 = _e174;
        let _e175 = finite1_u0028_f1_u003b((&param_24));
        phi_587_ = _e175;
        if _e175 {
            let _e176 = priorLinear;
            param_25 = _e176;
            let _e177 = finite1_u0028_f1_u003b((&param_25));
            phi_587_ = _e177;
        }
        let _e179 = phi_587_;
        let _e180 = currentLinear;
        let _e183 = priorLinear;
        let _e185 = ((_e179 && (_e180 > 0f)) && (_e183 > 0f));
        phi_602_ = _e185;
        if _e185 {
            let _e186 = currentLinear;
            let _e187 = priorLinear;
            let _e190 = depthLimit;
            phi_602_ = (abs((_e186 - _e187)) <= _e190);
        }
        let _e193 = phi_602_;
        if _e193 {
            let _e194 = previousTexel;
            param_26 = _e194;
            let _e195 = samplePreviousColorLinear_u0028_vf2_u003b((&param_26));
            prior = _e195;
            neighborhoodMin = vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f);
            neighborhoodMax = vec3<f32>(-340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f);
            y_3 = -1i;
            loop {
                let _e196 = y_3;
                if (_e196 <= 1i) {
                    x_3 = -1i;
                    loop {
                        let _e198 = x_3;
                        if (_e198 <= 1i) {
                            let _e200 = pixel_1;
                            let _e201 = x_3;
                            let _e202 = y_3;
                            param_27 = (_e200 + vec2<i32>(_e201, _e202));
                            let _e205 = sampleCurrent_u0028_vi2_u003b((&param_27));
                            c = _e205.xyz;
                            let _e207 = neighborhoodMin;
                            let _e208 = c;
                            neighborhoodMin = min(_e207, _e208);
                            let _e210 = neighborhoodMax;
                            let _e211 = c;
                            neighborhoodMax = max(_e210, _e211);
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e213 = x_3;
                            x_3 = (_e213 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e215 = y_3;
                    y_3 = (_e215 + 1i);
                }
            }
            let _e217 = prior;
            param_28 = _e217;
            let _e218 = finite4_u0028_vf4_u003b((&param_28));
            phi_658_ = _e218;
            if _e218 {
                let _e219 = neighborhoodMin;
                param_29 = _e219;
                let _e220 = finite3_u0028_vf3_u003b((&param_29));
                phi_658_ = _e220;
            }
            let _e222 = phi_658_;
            phi_664_ = _e222;
            if _e222 {
                let _e223 = neighborhoodMax;
                param_30 = _e223;
                let _e224 = finite3_u0028_vf3_u003b((&param_30));
                phi_664_ = _e224;
            }
            let _e226 = phi_664_;
            if _e226 {
                let _e227 = prior;
                let _e229 = neighborhoodMin;
                let _e230 = neighborhoodMax;
                clampedPrior = clamp(_e227.xyz, _e229, _e230);
                let _e232 = valid;
                confidence_1 = clamp(_e232, 0f, 1f);
                let _e235 = pc.historyWeight;
                baseWeight = clamp(_e235, 0f, 1f);
                let _e237 = baseWeight;
                param_31 = _e237;
                let _e238 = confidence_1;
                param_32 = _e238;
                let _e239 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_31), (&param_32));
                weight_1 = _e239;
                let _e240 = current;
                param_33 = _e240.xyz;
                let _e242 = clampedPrior;
                param_34 = _e242;
                let _e243 = weight_1;
                param_35 = _e243;
                let _e244 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_33), (&param_34), (&param_35));
                resolvedRgb = _e244;
                let _e245 = resolvedRgb;
                let _e247 = current[3u];
                result_4 = vec4<f32>(_e245.x, _e245.y, _e245.z, _e247);
            }
        }
    }
    let _e252 = result_4;
    param_36 = _e252;
    let _e253 = finite4_u0028_vf4_u003b((&param_36));
    if !(_e253) {
        result_4 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e255 = pixel_1;
    let _e256 = result_4;
    textureStore(resolvedColor, _e255, _e256);
    return;
}

@compute @workgroup_size(8, 8, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
