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
    let _e43 = pc.zNear;
    let _e45 = pc.zFar;
    let _e48 = pc.zNear;
    let _e49 = (*z);
    let _e51 = pc.zFar;
    let _e53 = pc.zNear;
    return ((_e43 * _e45) / (_e48 + (_e49 * (_e51 - _e53))));
}

fn mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b(x: ptr<function, vec3<f32>>, y: ptr<function, vec3<f32>>, a: ptr<function, f32>) -> vec3<f32> {
    var oneMinus: f32;
    var left: vec3<f32>;
    var right: vec3<f32>;
    var result: vec3<f32>;

    let _e48 = (*a);
    oneMinus = (1f - _e48);
    let _e50 = (*x);
    let _e51 = oneMinus;
    left = (_e50 * _e51);
    let _e53 = (*y);
    let _e54 = (*a);
    right = (_e53 * _e54);
    let _e56 = left;
    let _e57 = right;
    result = (_e56 + _e57);
    let _e59 = result;
    return _e59;
}

fn scalePrecise_u0028_f1_u003b_f1_u003b(weight: ptr<function, f32>, confidence: ptr<function, f32>) -> f32 {
    var scaled: f32;

    let _e44 = (*weight);
    let _e45 = (*confidence);
    scaled = (_e44 * _e45);
    let _e47 = scaled;
    return _e47;
}

fn finite3_u0028_vf3_u003b(v: ptr<function, vec3<f32>>) -> bool {
    var phi_115_: bool;

    let _e42 = (*v);
    let _e43 = (*v);
    let _e45 = all((_e42 == _e43));
    phi_115_ = _e45;
    if _e45 {
        let _e46 = (*v);
        phi_115_ = all((abs(_e46) <= vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e51 = phi_115_;
    return _e51;
}

fn sampleCurrent_u0028_vi2_u003b(pixel: ptr<function, vec2<i32>>) -> vec4<f32> {
    var bounded: vec2<i32>;

    let _e43 = (*pixel);
    let _e45 = pc.extent;
    bounded = clamp(_e43, vec2<i32>(0i, 0i), (bitcast<vec2<i32>>(_e45) - vec2<i32>(1i, 1i)));
    let _e49 = bounded;
    let _e50 = textureLoad(currentColor, _e49, 0i);
    return _e50;
}

fn mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b(x_1: ptr<function, vec4<f32>>, y_1: ptr<function, vec4<f32>>, a_1: ptr<function, f32>) -> vec4<f32> {
    var oneMinus_1: f32;
    var left_1: vec4<f32>;
    var right_1: vec4<f32>;
    var result_1: vec4<f32>;

    let _e48 = (*a_1);
    oneMinus_1 = (1f - _e48);
    let _e50 = (*x_1);
    let _e51 = oneMinus_1;
    left_1 = (_e50 * _e51);
    let _e53 = (*y_1);
    let _e54 = (*a_1);
    right_1 = (_e53 * _e54);
    let _e56 = left_1;
    let _e57 = right_1;
    result_1 = (_e56 + _e57);
    let _e59 = result_1;
    return _e59;
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

    let _e61 = (*texel);
    baseFloat = floor(_e61);
    let _e63 = baseFloat;
    base = vec2<i32>(_e63);
    let _e65 = (*texel);
    let _e66 = baseFloat;
    f = (_e65 - _e66);
    let _e69 = pc.extent;
    hi = (bitcast<vec2<i32>>(_e69) - vec2<i32>(1i, 1i));
    let _e72 = base;
    let _e73 = hi;
    let _e75 = textureLoad(previousColor, clamp(_e72, vec2<i32>(0i, 0i), _e73), 0i);
    c00_ = _e75;
    let _e76 = base;
    let _e78 = hi;
    let _e80 = textureLoad(previousColor, clamp((_e76 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e78), 0i);
    c10_ = _e80;
    let _e81 = base;
    let _e83 = hi;
    let _e85 = textureLoad(previousColor, clamp((_e81 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e83), 0i);
    c01_ = _e85;
    let _e86 = base;
    let _e88 = hi;
    let _e90 = textureLoad(previousColor, clamp((_e86 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e88), 0i);
    c11_ = _e90;
    let _e91 = c00_;
    param = _e91;
    let _e92 = c10_;
    param_1 = _e92;
    let _e94 = f[0u];
    param_2 = _e94;
    let _e95 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param), (&param_1), (&param_2));
    top = _e95;
    let _e96 = c01_;
    param_3 = _e96;
    let _e97 = c11_;
    param_4 = _e97;
    let _e99 = f[0u];
    param_5 = _e99;
    let _e100 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_3), (&param_4), (&param_5));
    bottom = _e100;
    let _e101 = top;
    param_6 = _e101;
    let _e102 = bottom;
    param_7 = _e102;
    let _e104 = f[1u];
    param_8 = _e104;
    let _e105 = mixPrecise_u0028_vf4_u003b_vf4_u003b_f1_u003b((&param_6), (&param_7), (&param_8));
    return _e105;
}

fn mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b(x_2: ptr<function, f32>, y_2: ptr<function, f32>, a_2: ptr<function, f32>) -> f32 {
    var oneMinus_2: f32;
    var left_2: f32;
    var right_2: f32;
    var result_2: f32;

    let _e48 = (*a_2);
    oneMinus_2 = (1f - _e48);
    let _e50 = (*x_2);
    let _e51 = oneMinus_2;
    left_2 = (_e50 * _e51);
    let _e53 = (*y_2);
    let _e54 = (*a_2);
    right_2 = (_e53 * _e54);
    let _e56 = left_2;
    let _e57 = right_2;
    result_2 = (_e56 + _e57);
    let _e59 = result_2;
    return _e59;
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

    let _e61 = (*texel_1);
    baseFloat_1 = floor(_e61);
    let _e63 = baseFloat_1;
    base_1 = vec2<i32>(_e63);
    let _e65 = (*texel_1);
    let _e66 = baseFloat_1;
    f_1 = (_e65 - _e66);
    let _e69 = pc.extent;
    hi_1 = (bitcast<vec2<i32>>(_e69) - vec2<i32>(1i, 1i));
    let _e72 = base_1;
    let _e73 = hi_1;
    let _e75 = textureLoad(previousLinearDepth, clamp(_e72, vec2<i32>(0i, 0i), _e73), 0i);
    d00_ = _e75.x;
    let _e77 = base_1;
    let _e79 = hi_1;
    let _e81 = textureLoad(previousLinearDepth, clamp((_e77 + vec2<i32>(1i, 0i)), vec2<i32>(0i, 0i), _e79), 0i);
    d10_ = _e81.x;
    let _e83 = base_1;
    let _e85 = hi_1;
    let _e87 = textureLoad(previousLinearDepth, clamp((_e83 + vec2<i32>(0i, 1i)), vec2<i32>(0i, 0i), _e85), 0i);
    d01_ = _e87.x;
    let _e89 = base_1;
    let _e91 = hi_1;
    let _e93 = textureLoad(previousLinearDepth, clamp((_e89 + vec2<i32>(1i, 1i)), vec2<i32>(0i, 0i), _e91), 0i);
    d11_ = _e93.x;
    let _e95 = d00_;
    param_9 = _e95;
    let _e96 = d10_;
    param_10 = _e96;
    let _e98 = f_1[0u];
    param_11 = _e98;
    let _e99 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_9), (&param_10), (&param_11));
    top_1 = _e99;
    let _e100 = d01_;
    param_12 = _e100;
    let _e101 = d11_;
    param_13 = _e101;
    let _e103 = f_1[0u];
    param_14 = _e103;
    let _e104 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
    bottom_1 = _e104;
    let _e105 = top_1;
    param_15 = _e105;
    let _e106 = bottom_1;
    param_16 = _e106;
    let _e108 = f_1[1u];
    param_17 = _e108;
    let _e109 = mixPrecise_u0028_f1_u003b_f1_u003b_f1_u003b((&param_15), (&param_16), (&param_17));
    return _e109;
}

fn linearizeDepth_u0028_f1_u003b(z_1: ptr<function, f32>) -> f32 {
    var range: f32;
    var scaled_1: f32;
    var denominator: f32;
    var numerator: f32;
    var result_3: f32;

    let _e48 = pc.zFar;
    let _e50 = pc.zNear;
    range = (_e48 - _e50);
    let _e52 = (*z_1);
    let _e53 = range;
    scaled_1 = (_e52 * _e53);
    let _e56 = pc.zNear;
    let _e57 = scaled_1;
    denominator = (_e56 + _e57);
    let _e60 = pc.zNear;
    let _e62 = pc.zFar;
    numerator = (_e60 * _e62);
    let _e64 = numerator;
    let _e65 = denominator;
    result_3 = (_e64 / _e65);
    let _e67 = result_3;
    return _e67;
}

fn finite2_u0028_vf2_u003b(v_1: ptr<function, vec2<f32>>) -> bool {
    var phi_100_: bool;

    let _e42 = (*v_1);
    let _e43 = (*v_1);
    let _e45 = all((_e42 == _e43));
    phi_100_ = _e45;
    if _e45 {
        let _e46 = (*v_1);
        phi_100_ = all((abs(_e46) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e51 = phi_100_;
    return _e51;
}

fn finite1_u0028_f1_u003b(v_2: ptr<function, f32>) -> bool {
    var phi_85_: bool;

    let _e42 = (*v_2);
    let _e43 = (*v_2);
    let _e44 = (_e42 == _e43);
    phi_85_ = _e44;
    if _e44 {
        let _e45 = (*v_2);
        phi_85_ = (abs(_e45) <= 340282350000000000000000000000000000000f);
    }
    let _e49 = phi_85_;
    return _e49;
}

fn finite4_u0028_vf4_u003b(v_3: ptr<function, vec4<f32>>) -> bool {
    var phi_130_: bool;

    let _e42 = (*v_3);
    let _e43 = (*v_3);
    let _e45 = all((_e42 == _e43));
    phi_130_ = _e45;
    if _e45 {
        let _e46 = (*v_3);
        phi_130_ = all((abs(_e46) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e51 = phi_130_;
    return _e51;
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
    var param_37: f32;
    var phi_542_: bool;
    var phi_551_: bool;
    var phi_560_: bool;
    var phi_566_: bool;
    var phi_608_: bool;
    var phi_623_: bool;
    var phi_679_: bool;
    var phi_685_: bool;

    let _e88 = gl_GlobalInvocationID_1;
    pixel_1 = bitcast<vec2<i32>>(_e88.xy);
    let _e91 = pixel_1;
    let _e93 = pc.extent;
    if any((_e91 >= bitcast<vec2<i32>>(_e93))) {
        return;
    }
    let _e97 = pixel_1;
    let _e98 = textureLoad(currentColor, _e97, 0i);
    current = _e98;
    let _e99 = current;
    result_4 = _e99;
    let _e100 = pixel_1;
    let _e101 = textureLoad(motionValidity, _e100, 0i);
    valid = _e101.x;
    let _e103 = pixel_1;
    let _e104 = textureLoad(motionVelocity, _e103, 0i);
    velocity = _e104.xy;
    let _e107 = pc.previousEffectiveJitterUv;
    let _e109 = pc.currentEffectiveJitterUv;
    jitterDelta = (_e107 - _e109);
    let _e111 = jitterDelta;
    let _e113 = pc.extent;
    jitterTexel = (_e111 * vec2<f32>(_e113));
    let _e116 = velocity;
    let _e118 = pc.extent;
    motionTexel = (_e116 * vec2<f32>(_e118));
    let _e121 = pixel_1;
    let _e123 = motionTexel;
    unjitteredPreviousTexel = (vec2<f32>(_e121) - _e123);
    let _e125 = unjitteredPreviousTexel;
    let _e126 = jitterTexel;
    previousTexel = (_e125 + _e126);
    let _e128 = previousTexel;
    previousCenter = (_e128 + vec2(0.5f));
    let _e131 = previousCenter;
    let _e133 = all((_e131 >= vec2<f32>(0f, 0f)));
    phi_542_ = _e133;
    if _e133 {
        let _e134 = previousCenter;
        let _e136 = pc.extent;
        phi_542_ = all((_e134 < vec2<f32>(_e136)));
    }
    let _e141 = phi_542_;
    inside = _e141;
    let _e142 = current;
    param_18 = _e142;
    let _e143 = finite4_u0028_vf4_u003b((&param_18));
    phi_551_ = _e143;
    if _e143 {
        let _e144 = valid;
        param_19 = _e144;
        let _e145 = finite1_u0028_f1_u003b((&param_19));
        phi_551_ = _e145;
    }
    let _e147 = phi_551_;
    let _e148 = valid;
    let _e150 = (_e147 && (_e148 > 0f));
    phi_560_ = _e150;
    if _e150 {
        let _e151 = velocity;
        param_20 = _e151;
        let _e152 = finite2_u0028_vf2_u003b((&param_20));
        phi_560_ = _e152;
    }
    let _e154 = phi_560_;
    phi_566_ = _e154;
    if _e154 {
        let _e155 = previousTexel;
        param_21 = _e155;
        let _e156 = finite2_u0028_vf2_u003b((&param_21));
        phi_566_ = _e156;
    }
    let _e158 = phi_566_;
    let _e159 = inside;
    if (_e158 && _e159) {
        let _e161 = pixel_1;
        let _e162 = textureLoad(currentDeviceDepth, _e161, 0i);
        currentDevice = _e162.x;
        let _e164 = currentDevice;
        param_22 = _e164;
        let _e165 = linearizeDepth_u0028_f1_u003b((&param_22));
        currentLinear = _e165;
        let _e166 = previousTexel;
        param_23 = _e166;
        let _e167 = samplePreviousDepthLinear_u0028_vf2_u003b((&param_23));
        priorLinear = _e167;
        let _e169 = pc.depthThresholdAbsolute;
        let _e171 = pc.depthThresholdRelative;
        let _e172 = currentLinear;
        let _e173 = priorLinear;
        depthLimit = max(_e169, (_e171 * max(_e172, _e173)));
        let _e177 = currentLinear;
        param_24 = _e177;
        let _e178 = finite1_u0028_f1_u003b((&param_24));
        phi_608_ = _e178;
        if _e178 {
            let _e179 = priorLinear;
            param_25 = _e179;
            let _e180 = finite1_u0028_f1_u003b((&param_25));
            phi_608_ = _e180;
        }
        let _e182 = phi_608_;
        let _e183 = currentLinear;
        let _e186 = priorLinear;
        let _e188 = ((_e182 && (_e183 > 0f)) && (_e186 > 0f));
        phi_623_ = _e188;
        if _e188 {
            let _e189 = currentLinear;
            let _e190 = priorLinear;
            let _e193 = depthLimit;
            phi_623_ = (abs((_e189 - _e190)) <= _e193);
        }
        let _e196 = phi_623_;
        if _e196 {
            let _e197 = previousTexel;
            param_26 = _e197;
            let _e198 = samplePreviousColorLinear_u0028_vf2_u003b((&param_26));
            prior = _e198;
            neighborhoodMin = vec3<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f);
            neighborhoodMax = vec3<f32>(-340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f, -340282350000000000000000000000000000000f);
            y_3 = -1i;
            loop {
                let _e199 = y_3;
                if (_e199 <= 1i) {
                    x_3 = -1i;
                    loop {
                        let _e201 = x_3;
                        if (_e201 <= 1i) {
                            let _e203 = pixel_1;
                            let _e204 = x_3;
                            let _e205 = y_3;
                            param_27 = (_e203 + vec2<i32>(_e204, _e205));
                            let _e208 = sampleCurrent_u0028_vi2_u003b((&param_27));
                            c = _e208.xyz;
                            let _e210 = neighborhoodMin;
                            let _e211 = c;
                            neighborhoodMin = min(_e210, _e211);
                            let _e213 = neighborhoodMax;
                            let _e214 = c;
                            neighborhoodMax = max(_e213, _e214);
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e216 = x_3;
                            x_3 = (_e216 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e218 = y_3;
                    y_3 = (_e218 + 1i);
                }
            }
            let _e220 = prior;
            param_28 = _e220;
            let _e221 = finite4_u0028_vf4_u003b((&param_28));
            phi_679_ = _e221;
            if _e221 {
                let _e222 = neighborhoodMin;
                param_29 = _e222;
                let _e223 = finite3_u0028_vf3_u003b((&param_29));
                phi_679_ = _e223;
            }
            let _e225 = phi_679_;
            phi_685_ = _e225;
            if _e225 {
                let _e226 = neighborhoodMax;
                param_30 = _e226;
                let _e227 = finite3_u0028_vf3_u003b((&param_30));
                phi_685_ = _e227;
            }
            let _e229 = phi_685_;
            if _e229 {
                let _e230 = prior;
                let _e232 = neighborhoodMin;
                let _e233 = neighborhoodMax;
                clampedPrior = clamp(_e230.xyz, _e232, _e233);
                let _e235 = valid;
                confidence_1 = clamp(_e235, 0f, 1f);
                let _e238 = pc.historyWeight;
                baseWeight = clamp(_e238, 0f, 1f);
                let _e240 = baseWeight;
                param_31 = _e240;
                let _e241 = confidence_1;
                param_32 = _e241;
                let _e242 = scalePrecise_u0028_f1_u003b_f1_u003b((&param_31), (&param_32));
                weight_1 = _e242;
                let _e243 = current;
                param_33 = _e243.xyz;
                let _e245 = clampedPrior;
                param_34 = _e245;
                let _e246 = weight_1;
                param_35 = _e246;
                let _e247 = mixPrecise_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_33), (&param_34), (&param_35));
                resolvedRgb = _e247;
                let _e248 = resolvedRgb;
                let _e250 = current[3u];
                result_4 = vec4<f32>(_e248.x, _e248.y, _e248.z, _e250);
            }
        }
    }
    let _e255 = result_4;
    param_36 = _e255;
    let _e256 = finite4_u0028_vf4_u003b((&param_36));
    if !(_e256) {
        result_4 = vec4<f32>(0f, 0f, 0f, 0f);
    }
    let _e258 = pixel_1;
    let _e259 = result_4;
    textureStore(resolvedColor, _e258, _e259);
    let _e260 = pixel_1;
    let _e261 = result_4;
    textureStore(historyOutColor, _e260, _e261);
    let _e262 = pixel_1;
    let _e263 = pixel_1;
    let _e264 = textureLoad(currentDeviceDepth, _e263, 0i);
    param_37 = _e264.x;
    let _e266 = storeLinearDepth_u0028_f1_u003b((&param_37));
    textureStore(historyOutLinearDepth, _e262, vec4(_e266));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
