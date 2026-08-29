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
    shadowExponent: f32,
    shadowPivot: f32,
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
    let _e84 = (*color);
    let _e85 = (*color);
    return (_e84 / (vec3(1f) + _e85));
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
    let _e106 = midIn;
    let _e107 = a;
    let _e110 = hdrMax;
    let _e111 = a;
    let _e113 = midOut;
    let _e116 = hdrMax;
    let _e117 = a;
    let _e118 = d;
    let _e121 = midIn;
    let _e122 = a;
    let _e123 = d;
    let _e127 = midOut;
    b = ((-(pow(_e106, _e107)) + (pow(_e110, _e111) * _e113)) / ((pow(_e116, (_e117 * _e118)) - pow(_e121, (_e122 * _e123))) * _e127));
    let _e130 = hdrMax;
    let _e131 = a;
    let _e132 = d;
    let _e135 = midIn;
    let _e136 = a;
    let _e139 = hdrMax;
    let _e140 = a;
    let _e142 = midIn;
    let _e143 = a;
    let _e144 = d;
    let _e148 = midOut;
    let _e151 = hdrMax;
    let _e152 = a;
    let _e153 = d;
    let _e156 = midIn;
    let _e157 = a;
    let _e158 = d;
    let _e162 = midOut;
    c = (((pow(_e130, (_e131 * _e132)) * pow(_e135, _e136)) - ((pow(_e139, _e140) * pow(_e142, (_e143 * _e144))) * _e148)) / ((pow(_e151, (_e152 * _e153)) - pow(_e156, (_e157 * _e158))) * _e162));
    let _e165 = (*color_1);
    let _e166 = a;
    let _e168 = (*color_1);
    let _e169 = a;
    let _e170 = d;
    let _e173 = b;
    let _e175 = c;
    return (pow(_e165, _e166) / ((pow(_e168, (_e169 * _e170)) * _e173) + _e175));
}

fn agxSigmoid_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x2_: vec3<f32>;
    var x4_: vec3<f32>;

    let _e86 = (*x);
    let _e87 = (*x);
    x2_ = (_e86 * _e87);
    let _e89 = x2_;
    let _e90 = x2_;
    x4_ = (_e89 * _e90);
    let _e92 = x4_;
    let _e94 = x2_;
    let _e96 = x4_;
    let _e98 = (*x);
    let _e101 = x4_;
    let _e104 = x2_;
    let _e106 = (*x);
    let _e109 = x2_;
    let _e112 = (*x);
    return ((((((((_e92 * 15.5f) * _e94) - ((_e96 * 40.14f) * _e98)) + (_e101 * 31.96f)) - ((_e104 * 6.868f) * _e106)) + (_e109 * 0.4298f)) + (_e112 * 0.1191f)) - vec3(0.00232f));
}

fn tonemapAgX_u0028_vf3_u003b(color_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e85 = (*color_2);
    (*color_2) = (mat3x3<f32>(vec3<f32>(0.85662717f, 0.09512124f, 0.048251607f), vec3<f32>(0.13731897f, 0.761242f, 0.10143904f), vec3<f32>(0.11189821f, 0.076799415f, 0.81130236f)) * _e85);
    let _e87 = (*color_2);
    (*color_2) = log2(max(_e87, vec3<f32>(0.0000000001f, 0.0000000001f, 0.0000000001f)));
    let _e90 = (*color_2);
    (*color_2) = clamp(((_e90 - vec3(-12.47393f)) / vec3(16.499998f)), vec3(0f), vec3(1f));
    let _e98 = (*color_2);
    param = _e98;
    let _e99 = agxSigmoid_u0028_vf3_u003b((&param));
    (*color_2) = _e99;
    let _e100 = (*color_2);
    (*color_2) = (mat3x3<f32>(vec3<f32>(1.1271006f, -0.11060664f, -0.016493939f), vec3<f32>(-0.14132977f, 1.1578237f, -0.016493939f), vec3<f32>(-0.14132977f, -0.11060664f, 1.2519364f)) * _e100);
    let _e102 = (*color_2);
    return max(_e102, vec3<f32>(0f, 0f, 0f));
}

fn tonemapPBRNeutral_u0028_vf3_u003b(color_3: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x_1: f32;
    var offset: f32;
    var local: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e91 = (*color_3)[0u];
    let _e93 = (*color_3)[1u];
    let _e95 = (*color_3)[2u];
    x_1 = min(_e91, min(_e93, _e95));
    let _e98 = x_1;
    if (_e98 < 0.08f) {
        let _e100 = x_1;
        let _e101 = x_1;
        let _e103 = x_1;
        local = (_e100 - ((6.25f * _e101) * _e103));
    } else {
        local = 0.04f;
    }
    let _e106 = local;
    offset = _e106;
    let _e107 = offset;
    let _e108 = (*color_3);
    (*color_3) = (_e108 - vec3(_e107));
    let _e112 = (*color_3)[0u];
    let _e114 = (*color_3)[1u];
    let _e116 = (*color_3)[2u];
    peak = max(_e112, max(_e114, _e116));
    let _e119 = peak;
    if (_e119 < 0.76f) {
        let _e121 = (*color_3);
        return _e121;
    }
    let _e122 = peak;
    newPeak = (1f - (0.0576f / ((_e122 + 0.24f) - 0.76f)));
    let _e127 = newPeak;
    let _e128 = peak;
    let _e130 = (*color_3);
    (*color_3) = (_e130 * (_e127 / _e128));
    let _e132 = peak;
    let _e133 = newPeak;
    g = (1f - (1f / ((0.15f * (_e132 - _e133)) + 1f)));
    let _e139 = (*color_3);
    let _e140 = newPeak;
    let _e142 = g;
    return mix(_e139, (vec3<f32>(1f, 1f, 1f) * _e140), vec3(_e142));
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

    let _e93 = (*peak_1);
    startCompression = (0.76f * _e93);
    let _e96 = (*color_4)[0u];
    let _e98 = (*color_4)[1u];
    let _e100 = (*color_4)[2u];
    x_2 = min(_e96, min(_e98, _e100));
    let _e103 = x_2;
    if (_e103 < 0.08f) {
        let _e105 = x_2;
        let _e106 = x_2;
        let _e108 = x_2;
        local_1 = (_e105 - ((6.25f * _e106) * _e108));
    } else {
        local_1 = 0.04f;
    }
    let _e111 = local_1;
    offset_1 = _e111;
    let _e112 = offset_1;
    let _e113 = (*color_4);
    (*color_4) = (_e113 - vec3(_e112));
    let _e117 = (*color_4)[0u];
    let _e119 = (*color_4)[1u];
    let _e121 = (*color_4)[2u];
    pk = max(_e117, max(_e119, _e121));
    let _e124 = pk;
    let _e125 = startCompression;
    if (_e124 < _e125) {
        let _e127 = (*color_4);
        return _e127;
    }
    let _e128 = (*peak_1);
    let _e129 = startCompression;
    d_1 = (_e128 - _e129);
    let _e131 = (*peak_1);
    let _e132 = d_1;
    let _e133 = d_1;
    let _e135 = pk;
    let _e136 = d_1;
    let _e138 = startCompression;
    newPeak_1 = (_e131 - ((_e132 * _e133) / ((_e135 + _e136) - _e138)));
    let _e142 = newPeak_1;
    let _e143 = pk;
    let _e145 = (*color_4);
    (*color_4) = (_e145 * (_e142 / _e143));
    let _e147 = pk;
    let _e148 = newPeak_1;
    g_1 = (1f - (1f / ((0.15f * (_e147 - _e148)) + 1f)));
    let _e154 = (*color_4);
    let _e155 = newPeak_1;
    let _e157 = g_1;
    return mix(_e154, (vec3<f32>(1f, 1f, 1f) * _e155), vec3(_e157));
}

fn applyTonemap_u0028_vf3_u003b(color_5: ptr<function, vec3<f32>>) -> vec3<f32> {
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: f32;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e91 = (*color_5);
    (*color_5) = (_e91 * tonemap_exposure);
    if override_type_7_ {
        if override_type_7_1 {
            let _e93 = (*color_5);
            param_1 = _e93;
            param_2 = hdr_peak_norm;
            let _e94 = tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b((&param_1), (&param_2));
            local_2 = _e94;
        } else {
            let _e95 = (*color_5);
            param_3 = _e95;
            let _e96 = tonemapPBRNeutral_u0028_vf3_u003b((&param_3));
            local_2 = _e96;
        }
        let _e97 = local_2;
        return _e97;
    } else {
        if override_type_7_2 {
            let _e98 = (*color_5);
            param_4 = _e98;
            let _e99 = tonemapAgX_u0028_vf3_u003b((&param_4));
            return _e99;
        } else {
            if override_type_7_3 {
                let _e100 = (*color_5);
                param_5 = _e100;
                let _e101 = tonemapLottes_u0028_vf3_u003b((&param_5));
                return _e101;
            } else {
                if override_type_7_4 {
                    let _e102 = (*color_5);
                    param_6 = _e102;
                    let _e103 = tonemapReinhard_u0028_vf3_u003b((&param_6));
                    return _e103;
                } else {
                    let _e104 = (*color_5);
                    return _e104;
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

    let _e90 = (*uv);
    dir = (_e90 - vec2<f32>(0.5f, 0.5f));
    let _e92 = dir;
    radial = length(_e92);
    let _e94 = dir;
    let _e95 = radial;
    offset_2 = (_e94 * ((chromatic_strength * _e95) * 0.015f));
    let _e99 = (*uv);
    let _e100 = offset_2;
    let _e105 = textureSample(texture0_, texture0_sampler, clamp((_e99 + _e100), vec2(0f), vec2(1f)));
    r = _e105.x;
    let _e107 = (*uv);
    let _e108 = textureSample(texture0_, texture0_sampler, _e107);
    g_2 = _e108.y;
    let _e110 = (*uv);
    let _e111 = offset_2;
    let _e116 = textureSample(texture0_, texture0_sampler, clamp((_e110 - _e111), vec2(0f), vec2(1f)));
    b_1 = _e116.z;
    let _e118 = r;
    let _e119 = g_2;
    let _e120 = b_1;
    return vec3<f32>(_e118, _e119, _e120);
}

fn main_1() {
    var base: vec3<f32>;
    var local_3: vec3<f32>;
    var param_7: vec2<f32>;
    var shadowLuma: f32;
    var normalized: f32;
    var curved: f32;
    var param_8: vec3<f32>;
    var luma: vec3<f32>;
    var phi_563_: bool;

    if (chromatic_strength > 0f) {
        let _e92 = frag_tex_coord_1;
        param_7 = _e92;
        let _e93 = sampleChromatic_u0028_vf2_u003b((&param_7));
        local_3 = _e93;
    } else {
        let _e94 = frag_tex_coord_1;
        let _e95 = textureSample(texture0_, texture0_sampler, _e94);
        local_3 = _e95.xyz;
    }
    let _e97 = local_3;
    base = _e97;
    let _e99 = eb.exposure_bias;
    let _e100 = base;
    base = (_e100 * _e99);
    let _e102 = base;
    shadowLuma = dot(max(_e102, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e106 = eb.shadowExponent;
    let _e108 = shadowLuma;
    let _e110 = ((_e106 != 1f) && (_e108 > 0f));
    phi_563_ = _e110;
    if _e110 {
        let _e111 = shadowLuma;
        let _e113 = eb.shadowPivot;
        phi_563_ = (_e111 < _e113);
    }
    let _e116 = phi_563_;
    if _e116 {
        let _e117 = shadowLuma;
        let _e119 = eb.shadowPivot;
        normalized = (_e117 / _e119);
        let _e121 = normalized;
        let _e123 = eb.shadowExponent;
        let _e126 = eb.shadowPivot;
        curved = (pow(_e121, _e123) * _e126);
        let _e128 = curved;
        let _e129 = shadowLuma;
        let _e131 = base;
        base = (_e131 * (_e128 / _e129));
    }
    let _e133 = base;
    param_8 = _e133;
    let _e134 = applyTonemap_u0028_vf3_u003b((&param_8));
    base = _e134;
    if (saturation != 1f) {
        let _e136 = base;
        luma = vec3(dot(_e136, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e139 = luma;
        let _e140 = base;
        base = mix(_e139, _e140, vec3(saturation));
    }
    let _e143 = base;
    out_color = vec4<f32>(_e143.x, _e143.y, _e143.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
