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

@id(28) override lottes_contrast: f32 = 1.6f;
@id(29) override lottes_shoulder: f32 = 0.977f;
@id(32) override lottes_hdr_max: f32 = 8f;
@id(30) override lottes_mid_in: f32 = 0.18f;
@id(31) override lottes_mid_out: f32 = 0.267f;
@id(18) override tonemap_exposure: f32 = 1f;
@id(17) override tonemap_mode: i32 = 1i;
override override_type_7_: bool = (tonemap_mode == 1i);
@id(12) override hdr_mode: i32 = 0i;
override override_type_7_1: bool = (hdr_mode == 1i);
@id(13) override hdr_peak_norm: f32 = 10f;
override override_type_7_2: bool = (tonemap_mode == 2i);
override override_type_7_3: bool = (tonemap_mode == 3i);
override override_type_7_4: bool = (tonemap_mode == 4i);
@id(24) override chromatic_strength: f32 = 0f;
@id(2) override saturation: f32 = 1f;

@group(0) @binding(0) 
var texture0_: texture_2d<f32>;
@group(0) @binding(32) 
var texture0_sampler: sampler;
var<private> frag_tex_coord_1: vec2<f32>;
@group(2) @binding(0) 
var<uniform> eb: ExposureBlock;
var<private> out_color: vec4<f32>;

fn tonemapReinhard_u0028_vf3_u003b(color: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e82 = (*color);
    let _e83 = (*color);
    return (_e82 / (vec3(1f) + _e83));
}

fn tonemapLottes_u0028_vf3_u003b(color_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: vec3<f32>;
    var d: vec3<f32>;
    var hdrMax: vec3<f32>;
    var midIn: vec3<f32>;
    var midOut: vec3<f32>;
    var b: vec3<f32>;
    var c: vec3<f32>;

    a[0u] = lottes_contrast;
    a[1u] = lottes_contrast;
    a[2u] = lottes_contrast;
    d[0u] = lottes_shoulder;
    d[1u] = lottes_shoulder;
    d[2u] = lottes_shoulder;
    hdrMax[0u] = lottes_hdr_max;
    hdrMax[1u] = lottes_hdr_max;
    hdrMax[2u] = lottes_hdr_max;
    midIn[0u] = lottes_mid_in;
    midIn[1u] = lottes_mid_in;
    midIn[2u] = lottes_mid_in;
    midOut[0u] = lottes_mid_out;
    midOut[1u] = lottes_mid_out;
    midOut[2u] = lottes_mid_out;
    let _e104 = midIn;
    let _e105 = a;
    let _e108 = hdrMax;
    let _e109 = a;
    let _e111 = midOut;
    let _e114 = hdrMax;
    let _e115 = a;
    let _e116 = d;
    let _e119 = midIn;
    let _e120 = a;
    let _e121 = d;
    let _e125 = midOut;
    b = ((-(pow(_e104, _e105)) + (pow(_e108, _e109) * _e111)) / ((pow(_e114, (_e115 * _e116)) - pow(_e119, (_e120 * _e121))) * _e125));
    let _e128 = hdrMax;
    let _e129 = a;
    let _e130 = d;
    let _e133 = midIn;
    let _e134 = a;
    let _e137 = hdrMax;
    let _e138 = a;
    let _e140 = midIn;
    let _e141 = a;
    let _e142 = d;
    let _e146 = midOut;
    let _e149 = hdrMax;
    let _e150 = a;
    let _e151 = d;
    let _e154 = midIn;
    let _e155 = a;
    let _e156 = d;
    let _e160 = midOut;
    c = (((pow(_e128, (_e129 * _e130)) * pow(_e133, _e134)) - ((pow(_e137, _e138) * pow(_e140, (_e141 * _e142))) * _e146)) / ((pow(_e149, (_e150 * _e151)) - pow(_e154, (_e155 * _e156))) * _e160));
    let _e163 = (*color_1);
    let _e164 = a;
    let _e166 = (*color_1);
    let _e167 = a;
    let _e168 = d;
    let _e171 = b;
    let _e173 = c;
    return (pow(_e163, _e164) / ((pow(_e166, (_e167 * _e168)) * _e171) + _e173));
}

fn agxSigmoid_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x2_: vec3<f32>;
    var x4_: vec3<f32>;

    let _e84 = (*x);
    let _e85 = (*x);
    x2_ = (_e84 * _e85);
    let _e87 = x2_;
    let _e88 = x2_;
    x4_ = (_e87 * _e88);
    let _e90 = x4_;
    let _e92 = x2_;
    let _e94 = x4_;
    let _e96 = (*x);
    let _e99 = x4_;
    let _e102 = x2_;
    let _e104 = (*x);
    let _e107 = x2_;
    let _e110 = (*x);
    return ((((((((_e90 * 15.5f) * _e92) - ((_e94 * 40.14f) * _e96)) + (_e99 * 31.96f)) - ((_e102 * 6.868f) * _e104)) + (_e107 * 0.4298f)) + (_e110 * 0.1191f)) - vec3(0.00232f));
}

fn tonemapAgX_u0028_vf3_u003b(color_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e83 = (*color_2);
    (*color_2) = (mat3x3<f32>(vec3<f32>(0.85662717f, 0.09512124f, 0.048251607f), vec3<f32>(0.13731897f, 0.761242f, 0.10143904f), vec3<f32>(0.11189821f, 0.076799415f, 0.81130236f)) * _e83);
    let _e85 = (*color_2);
    (*color_2) = log2(max(_e85, vec3<f32>(0.0000000001f, 0.0000000001f, 0.0000000001f)));
    let _e88 = (*color_2);
    (*color_2) = clamp(((_e88 - vec3(-12.47393f)) / vec3(16.499998f)), vec3(0f), vec3(1f));
    let _e96 = (*color_2);
    param = _e96;
    let _e97 = agxSigmoid_u0028_vf3_u003b((&param));
    (*color_2) = _e97;
    let _e98 = (*color_2);
    (*color_2) = (mat3x3<f32>(vec3<f32>(1.1271006f, -0.11060664f, -0.016493939f), vec3<f32>(-0.14132977f, 1.1578237f, -0.016493939f), vec3<f32>(-0.14132977f, -0.11060664f, 1.2519364f)) * _e98);
    let _e100 = (*color_2);
    return max(_e100, vec3<f32>(0f, 0f, 0f));
}

fn tonemapPBRNeutral_u0028_vf3_u003b(color_3: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x_1: f32;
    var offset: f32;
    var local: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e89 = (*color_3)[0u];
    let _e91 = (*color_3)[1u];
    let _e93 = (*color_3)[2u];
    x_1 = min(_e89, min(_e91, _e93));
    let _e96 = x_1;
    if (_e96 < 0.08f) {
        let _e98 = x_1;
        let _e99 = x_1;
        let _e101 = x_1;
        local = (_e98 - ((6.25f * _e99) * _e101));
    } else {
        local = 0.04f;
    }
    let _e104 = local;
    offset = _e104;
    let _e105 = offset;
    let _e106 = (*color_3);
    (*color_3) = (_e106 - vec3(_e105));
    let _e110 = (*color_3)[0u];
    let _e112 = (*color_3)[1u];
    let _e114 = (*color_3)[2u];
    peak = max(_e110, max(_e112, _e114));
    let _e117 = peak;
    if (_e117 < 0.76f) {
        let _e119 = (*color_3);
        return _e119;
    }
    let _e120 = peak;
    newPeak = (1f - (0.0576f / ((_e120 + 0.24f) - 0.76f)));
    let _e125 = newPeak;
    let _e126 = peak;
    let _e128 = (*color_3);
    (*color_3) = (_e128 * (_e125 / _e126));
    let _e130 = peak;
    let _e131 = newPeak;
    g = (1f - (1f / ((0.15f * (_e130 - _e131)) + 1f)));
    let _e137 = (*color_3);
    let _e138 = newPeak;
    let _e140 = g;
    return mix(_e137, (vec3<f32>(1f, 1f, 1f) * _e138), vec3(_e140));
}

fn tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b(color_4: ptr<function, vec3<f32>>, peak_1: ptr<function, f32>) -> vec3<f32> {
    var startCompression: f32;
    var x_2: f32;
    var offset_1: f32;
    var local_1: f32;
    var pk: f32;
    var d_1: f32;
    var newPeak_1: f32;
    var g_1: f32;

    let _e91 = (*peak_1);
    startCompression = (0.76f * _e91);
    let _e94 = (*color_4)[0u];
    let _e96 = (*color_4)[1u];
    let _e98 = (*color_4)[2u];
    x_2 = min(_e94, min(_e96, _e98));
    let _e101 = x_2;
    if (_e101 < 0.08f) {
        let _e103 = x_2;
        let _e104 = x_2;
        let _e106 = x_2;
        local_1 = (_e103 - ((6.25f * _e104) * _e106));
    } else {
        local_1 = 0.04f;
    }
    let _e109 = local_1;
    offset_1 = _e109;
    let _e110 = offset_1;
    let _e111 = (*color_4);
    (*color_4) = (_e111 - vec3(_e110));
    let _e115 = (*color_4)[0u];
    let _e117 = (*color_4)[1u];
    let _e119 = (*color_4)[2u];
    pk = max(_e115, max(_e117, _e119));
    let _e122 = pk;
    let _e123 = startCompression;
    if (_e122 < _e123) {
        let _e125 = (*color_4);
        return _e125;
    }
    let _e126 = (*peak_1);
    let _e127 = startCompression;
    d_1 = (_e126 - _e127);
    let _e129 = (*peak_1);
    let _e130 = d_1;
    let _e131 = d_1;
    let _e133 = pk;
    let _e134 = d_1;
    let _e136 = startCompression;
    newPeak_1 = (_e129 - ((_e130 * _e131) / ((_e133 + _e134) - _e136)));
    let _e140 = newPeak_1;
    let _e141 = pk;
    let _e143 = (*color_4);
    (*color_4) = (_e143 * (_e140 / _e141));
    let _e145 = pk;
    let _e146 = newPeak_1;
    g_1 = (1f - (1f / ((0.15f * (_e145 - _e146)) + 1f)));
    let _e152 = (*color_4);
    let _e153 = newPeak_1;
    let _e155 = g_1;
    return mix(_e152, (vec3<f32>(1f, 1f, 1f) * _e153), vec3(_e155));
}

fn applyTonemap_u0028_vf3_u003b(color_5: ptr<function, vec3<f32>>) -> vec3<f32> {
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: f32;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e89 = (*color_5);
    (*color_5) = (_e89 * tonemap_exposure);
    if override_type_7_ {
        if override_type_7_1 {
            let _e91 = (*color_5);
            param_1 = _e91;
            param_2 = hdr_peak_norm;
            let _e92 = tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b((&param_1), (&param_2));
            local_2 = _e92;
        } else {
            let _e93 = (*color_5);
            param_3 = _e93;
            let _e94 = tonemapPBRNeutral_u0028_vf3_u003b((&param_3));
            local_2 = _e94;
        }
        let _e95 = local_2;
        return _e95;
    } else {
        if override_type_7_2 {
            let _e96 = (*color_5);
            param_4 = _e96;
            let _e97 = tonemapAgX_u0028_vf3_u003b((&param_4));
            return _e97;
        } else {
            if override_type_7_3 {
                let _e98 = (*color_5);
                param_5 = _e98;
                let _e99 = tonemapLottes_u0028_vf3_u003b((&param_5));
                return _e99;
            } else {
                if override_type_7_4 {
                    let _e100 = (*color_5);
                    param_6 = _e100;
                    let _e101 = tonemapReinhard_u0028_vf3_u003b((&param_6));
                    return _e101;
                } else {
                    let _e102 = (*color_5);
                    return _e102;
                }
            }
        }
    }
}

fn sampleChromatic_u0028_vf2_u003b(uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset_2: vec2<f32>;
    var r: f32;
    var g_2: f32;
    var b_1: f32;

    let _e88 = (*uv);
    dir = (_e88 - vec2<f32>(0.5f, 0.5f));
    let _e90 = dir;
    radial = length(_e90);
    let _e92 = dir;
    let _e93 = radial;
    offset_2 = (_e92 * ((chromatic_strength * _e93) * 0.015f));
    let _e97 = (*uv);
    let _e98 = offset_2;
    let _e103 = textureSample(texture0_, texture0_sampler, clamp((_e97 + _e98), vec2(0f), vec2(1f)));
    r = _e103.x;
    let _e105 = (*uv);
    let _e106 = textureSample(texture0_, texture0_sampler, _e105);
    g_2 = _e106.y;
    let _e108 = (*uv);
    let _e109 = offset_2;
    let _e114 = textureSample(texture0_, texture0_sampler, clamp((_e108 - _e109), vec2(0f), vec2(1f)));
    b_1 = _e114.z;
    let _e116 = r;
    let _e117 = g_2;
    let _e118 = b_1;
    return vec3<f32>(_e116, _e117, _e118);
}

fn main_1() {
    var base: vec3<f32>;
    var local_3: vec3<f32>;
    var param_7: vec2<f32>;
    var param_8: vec3<f32>;
    var luma: vec3<f32>;

    if (chromatic_strength > 0f) {
        let _e87 = frag_tex_coord_1;
        param_7 = _e87;
        let _e88 = sampleChromatic_u0028_vf2_u003b((&param_7));
        local_3 = _e88;
    } else {
        let _e89 = frag_tex_coord_1;
        let _e90 = textureSample(texture0_, texture0_sampler, _e89);
        local_3 = _e90.xyz;
    }
    let _e92 = local_3;
    base = _e92;
    let _e94 = eb.exposure_bias;
    let _e95 = base;
    base = (_e95 * _e94);
    let _e97 = base;
    param_8 = _e97;
    let _e98 = applyTonemap_u0028_vf3_u003b((&param_8));
    base = _e98;
    if (saturation != 1f) {
        let _e100 = base;
        luma = vec3(dot(_e100, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e103 = luma;
        let _e104 = base;
        base = mix(_e103, _e104, vec3(saturation));
    }
    let _e107 = base;
    out_color = vec4<f32>(_e107.x, _e107.y, _e107.z, 1f);
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
