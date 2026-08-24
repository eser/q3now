struct Histogram {
    bins: array<u32, 256>,
}

struct Accumulator {
    accum: f32,
}

struct ExposureBlock {
    exposure_bias: f32,
    key: f32,
    pctLow: f32,
    pctHigh: f32,
    rateUp: f32,
    rateDown: f32,
    minExp: f32,
    maxExp: f32,
    autoEnabled: i32,
    brightness: f32,
    sunScreenX: f32,
    sunScreenY: f32,
    sunrayIntensity: f32,
    sunrayDecay: f32,
}

struct Push {
    dt: f32,
}

@group(0) @binding(0)
var<storage> unnamed: Histogram;
@group(0) @binding(1)
var<storage, read_write> unnamed_1: Accumulator;
@group(0) @binding(2)
var<storage, read_write> unnamed_2: ExposureBlock;
@group(0) @binding(3)
var<uniform> unnamed_3: Push;

fn binEV_u0028_u1_u003b(i: ptr<function, u32>) -> f32 {
    let _e25 = (*i);
    return (-10f + ((f32(_e25) / 255f) * 12f));
}

fn main_1() {
    var total: f32;
    var i_1: u32;
    var target_: f32;
    var loW: f32;
    var hiW: f32;
    var cum: f32;
    var wSum: f32;
    var evSum: f32;
    var i_2: u32;
    var w: f32;
    var lo: f32;
    var hi: f32;
    var a: f32;
    var b: f32;
    var clipped: f32;
    var param: u32;
    var avgEV: f32;
    var local: f32;
    var param_1: u32;
    var avgLuma: f32;
    var rate: f32;
    var local_1: f32;
    var a_1: f32;
    var smoothed: f32;

    total = 0f;
    i_1 = 1u;
    loop {
        let _e48 = i_1;
        if (_e48 < 256u) {
            let _e50 = i_1;
            let _e53 = unnamed.bins[_e50];
            let _e55 = total;
            total = (_e55 + f32(_e53));
            continue;
        } else {
            break;
        }
        continuing {
            let _e57 = i_1;
            i_1 = (_e57 + bitcast<u32>(1i));
        }
    }
    let _e60 = total;
    if (_e60 <= 0f) {
        let _e63 = unnamed_1.accum;
        target_ = _e63;
    } else {
        let _e65 = unnamed_2.pctLow;
        let _e66 = total;
        loW = (_e65 * _e66);
        let _e69 = unnamed_2.pctHigh;
        let _e71 = total;
        hiW = ((1f - _e69) * _e71);
        let _e73 = hiW;
        let _e74 = loW;
        if (_e73 <= _e74) {
            loW = 0f;
            let _e76 = total;
            hiW = _e76;
        }
        cum = 0f;
        wSum = 0f;
        evSum = 0f;
        i_2 = 1u;
        loop {
            let _e77 = i_2;
            if (_e77 < 256u) {
                let _e79 = i_2;
                let _e82 = unnamed.bins[_e79];
                w = f32(_e82);
                let _e84 = w;
                if (_e84 <= 0f) {
                    continue;
                }
                let _e86 = cum;
                lo = _e86;
                let _e87 = cum;
                let _e88 = w;
                hi = (_e87 + _e88);
                let _e90 = hi;
                cum = _e90;
                let _e91 = lo;
                let _e92 = loW;
                a = max(_e91, _e92);
                let _e94 = hi;
                let _e95 = hiW;
                b = min(_e94, _e95);
                let _e97 = b;
                let _e98 = a;
                if (_e97 > _e98) {
                    let _e100 = b;
                    let _e101 = a;
                    clipped = (_e100 - _e101);
                    let _e103 = i_2;
                    param = _e103;
                    let _e104 = binEV_u0028_u1_u003b((&param));
                    let _e105 = clipped;
                    let _e107 = evSum;
                    evSum = (_e107 + (_e104 * _e105));
                    let _e109 = clipped;
                    let _e110 = wSum;
                    wSum = (_e110 + _e109);
                }
                continue;
            } else {
                break;
            }
            continuing {
                let _e112 = i_2;
                i_2 = (_e112 + bitcast<u32>(1i));
            }
        }
        let _e115 = wSum;
        if (_e115 > 0f) {
            let _e117 = evSum;
            let _e118 = wSum;
            local = (_e117 / _e118);
        } else {
            param_1 = 128u;
            let _e120 = binEV_u0028_u1_u003b((&param_1));
            local = _e120;
        }
        let _e121 = local;
        avgEV = _e121;
        let _e122 = avgEV;
        avgLuma = exp2(_e122);
        let _e125 = unnamed_2.key;
        let _e126 = avgLuma;
        target_ = (_e125 / max(_e126, 0.0001f));
        let _e129 = target_;
        let _e131 = unnamed_2.minExp;
        let _e133 = unnamed_2.maxExp;
        target_ = clamp(_e129, _e131, _e133);
    }
    let _e135 = target_;
    let _e137 = unnamed_1.accum;
    if (_e135 > _e137) {
        let _e140 = unnamed_2.rateUp;
        local_1 = _e140;
    } else {
        let _e142 = unnamed_2.rateDown;
        local_1 = _e142;
    }
    let _e143 = local_1;
    rate = _e143;
    let _e145 = unnamed_3.dt;
    let _e148 = rate;
    a_1 = (1f - exp((-(max(_e145, 0f)) * _e148)));
    let _e153 = unnamed_1.accum;
    let _e154 = target_;
    let _e155 = a_1;
    smoothed = mix(_e153, _e154, _e155);
    let _e157 = smoothed;
    unnamed_1.accum = _e157;
    let _e159 = smoothed;
    let _e161 = unnamed_2.brightness;
    unnamed_2.exposure_bias = (_e159 * _e161);
    return;
}

@compute @workgroup_size(1, 1, 1)
fn main() {
    main_1();
}
