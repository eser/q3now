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
@id(19) override cg_tint_r: f32 = 1f;
@id(20) override cg_tint_g: f32 = 1f;
@id(21) override cg_tint_b: f32 = 1f;
@id(22) override cg_saturation: f32 = 1f;
@id(23) override cg_contrast: f32 = 1f;
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

fn applyColorGrading_u0028_vf3_u003b(color: ptr<function, vec3<f32>>) -> vec3<f32> {
    var colorGradeTint: vec3<f32>;
    var luma: f32;

    colorGradeTint[0u] = cg_tint_r;
    colorGradeTint[1u] = cg_tint_g;
    colorGradeTint[2u] = cg_tint_b;
    let _e92 = colorGradeTint;
    let _e93 = (*color);
    (*color) = (_e93 * _e92);
    let _e95 = (*color);
    luma = dot(_e95, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e97 = luma;
    let _e99 = (*color);
    (*color) = mix(vec3(_e97), _e99, vec3(cg_saturation));
    let _e102 = (*color);
    (*color) = (((_e102 - vec3(0.5f)) * cg_contrast) + vec3(0.5f));
    let _e108 = (*color);
    return clamp(_e108, vec3(0f), vec3(1f));
}

fn tonemapReinhard_u0028_vf3_u003b(color_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e87 = (*color_1);
    let _e88 = (*color_1);
    return (_e87 / (vec3(1f) + _e88));
}

fn tonemapLottes_u0028_vf3_u003b(color_2: ptr<function, vec3<f32>>) -> vec3<f32> {
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
    let _e109 = midIn;
    let _e110 = a;
    let _e113 = hdrMax;
    let _e114 = a;
    let _e116 = midOut;
    let _e119 = hdrMax;
    let _e120 = a;
    let _e121 = d;
    let _e124 = midIn;
    let _e125 = a;
    let _e126 = d;
    let _e130 = midOut;
    b = ((-(pow(_e109, _e110)) + (pow(_e113, _e114) * _e116)) / ((pow(_e119, (_e120 * _e121)) - pow(_e124, (_e125 * _e126))) * _e130));
    let _e133 = hdrMax;
    let _e134 = a;
    let _e135 = d;
    let _e138 = midIn;
    let _e139 = a;
    let _e142 = hdrMax;
    let _e143 = a;
    let _e145 = midIn;
    let _e146 = a;
    let _e147 = d;
    let _e151 = midOut;
    let _e154 = hdrMax;
    let _e155 = a;
    let _e156 = d;
    let _e159 = midIn;
    let _e160 = a;
    let _e161 = d;
    let _e165 = midOut;
    c = (((pow(_e133, (_e134 * _e135)) * pow(_e138, _e139)) - ((pow(_e142, _e143) * pow(_e145, (_e146 * _e147))) * _e151)) / ((pow(_e154, (_e155 * _e156)) - pow(_e159, (_e160 * _e161))) * _e165));
    let _e168 = (*color_2);
    let _e169 = a;
    let _e171 = (*color_2);
    let _e172 = a;
    let _e173 = d;
    let _e176 = b;
    let _e178 = c;
    return (pow(_e168, _e169) / ((pow(_e171, (_e172 * _e173)) * _e176) + _e178));
}

fn agxSigmoid_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x2_: vec3<f32>;
    var x4_: vec3<f32>;

    let _e89 = (*x);
    let _e90 = (*x);
    x2_ = (_e89 * _e90);
    let _e92 = x2_;
    let _e93 = x2_;
    x4_ = (_e92 * _e93);
    let _e95 = x4_;
    let _e97 = x2_;
    let _e99 = x4_;
    let _e101 = (*x);
    let _e104 = x4_;
    let _e107 = x2_;
    let _e109 = (*x);
    let _e112 = x2_;
    let _e115 = (*x);
    return ((((((((_e95 * 15.5f) * _e97) - ((_e99 * 40.14f) * _e101)) + (_e104 * 31.96f)) - ((_e107 * 6.868f) * _e109)) + (_e112 * 0.4298f)) + (_e115 * 0.1191f)) - vec3(0.00232f));
}

fn tonemapAgX_u0028_vf3_u003b(color_3: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e88 = (*color_3);
    (*color_3) = (mat3x3<f32>(vec3<f32>(0.85662717f, 0.09512124f, 0.048251607f), vec3<f32>(0.13731897f, 0.761242f, 0.10143904f), vec3<f32>(0.11189821f, 0.076799415f, 0.81130236f)) * _e88);
    let _e90 = (*color_3);
    (*color_3) = log2(max(_e90, vec3<f32>(0.0000000001f, 0.0000000001f, 0.0000000001f)));
    let _e93 = (*color_3);
    (*color_3) = clamp(((_e93 - vec3(-12.47393f)) / vec3(16.499998f)), vec3(0f), vec3(1f));
    let _e101 = (*color_3);
    param = _e101;
    let _e102 = agxSigmoid_u0028_vf3_u003b((&param));
    (*color_3) = _e102;
    let _e103 = (*color_3);
    (*color_3) = (mat3x3<f32>(vec3<f32>(1.1271006f, -0.11060664f, -0.016493939f), vec3<f32>(-0.14132977f, 1.1578237f, -0.016493939f), vec3<f32>(-0.14132977f, -0.11060664f, 1.2519364f)) * _e103);
    let _e105 = (*color_3);
    return max(_e105, vec3<f32>(0f, 0f, 0f));
}

fn tonemapPBRNeutral_u0028_vf3_u003b(color_4: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x_1: f32;
    var offset: f32;
    var local: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e94 = (*color_4)[0u];
    let _e96 = (*color_4)[1u];
    let _e98 = (*color_4)[2u];
    x_1 = min(_e94, min(_e96, _e98));
    let _e101 = x_1;
    if (_e101 < 0.08f) {
        let _e103 = x_1;
        let _e104 = x_1;
        let _e106 = x_1;
        local = (_e103 - ((6.25f * _e104) * _e106));
    } else {
        local = 0.04f;
    }
    let _e109 = local;
    offset = _e109;
    let _e110 = offset;
    let _e111 = (*color_4);
    (*color_4) = (_e111 - vec3(_e110));
    let _e115 = (*color_4)[0u];
    let _e117 = (*color_4)[1u];
    let _e119 = (*color_4)[2u];
    peak = max(_e115, max(_e117, _e119));
    let _e122 = peak;
    if (_e122 < 0.76f) {
        let _e124 = (*color_4);
        return _e124;
    }
    let _e125 = peak;
    newPeak = (1f - (0.0576f / ((_e125 + 0.24f) - 0.76f)));
    let _e130 = newPeak;
    let _e131 = peak;
    let _e133 = (*color_4);
    (*color_4) = (_e133 * (_e130 / _e131));
    let _e135 = peak;
    let _e136 = newPeak;
    g = (1f - (1f / ((0.15f * (_e135 - _e136)) + 1f)));
    let _e142 = (*color_4);
    let _e143 = newPeak;
    let _e145 = g;
    return mix(_e142, (vec3<f32>(1f, 1f, 1f) * _e143), vec3(_e145));
}

fn tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b(color_5: ptr<function, vec3<f32>>, peak_1: ptr<function, f32>) -> vec3<f32> {
    var startCompression: f32;
    var x_2: f32;
    var offset_1: f32;
    var local_1: f32;
    var pk: f32;
    var d_1: f32;
    var newPeak_1: f32;
    var g_1: f32;

    let _e96 = (*peak_1);
    startCompression = (0.76f * _e96);
    let _e99 = (*color_5)[0u];
    let _e101 = (*color_5)[1u];
    let _e103 = (*color_5)[2u];
    x_2 = min(_e99, min(_e101, _e103));
    let _e106 = x_2;
    if (_e106 < 0.08f) {
        let _e108 = x_2;
        let _e109 = x_2;
        let _e111 = x_2;
        local_1 = (_e108 - ((6.25f * _e109) * _e111));
    } else {
        local_1 = 0.04f;
    }
    let _e114 = local_1;
    offset_1 = _e114;
    let _e115 = offset_1;
    let _e116 = (*color_5);
    (*color_5) = (_e116 - vec3(_e115));
    let _e120 = (*color_5)[0u];
    let _e122 = (*color_5)[1u];
    let _e124 = (*color_5)[2u];
    pk = max(_e120, max(_e122, _e124));
    let _e127 = pk;
    let _e128 = startCompression;
    if (_e127 < _e128) {
        let _e130 = (*color_5);
        return _e130;
    }
    let _e131 = (*peak_1);
    let _e132 = startCompression;
    d_1 = (_e131 - _e132);
    let _e134 = (*peak_1);
    let _e135 = d_1;
    let _e136 = d_1;
    let _e138 = pk;
    let _e139 = d_1;
    let _e141 = startCompression;
    newPeak_1 = (_e134 - ((_e135 * _e136) / ((_e138 + _e139) - _e141)));
    let _e145 = newPeak_1;
    let _e146 = pk;
    let _e148 = (*color_5);
    (*color_5) = (_e148 * (_e145 / _e146));
    let _e150 = pk;
    let _e151 = newPeak_1;
    g_1 = (1f - (1f / ((0.15f * (_e150 - _e151)) + 1f)));
    let _e157 = (*color_5);
    let _e158 = newPeak_1;
    let _e160 = g_1;
    return mix(_e157, (vec3<f32>(1f, 1f, 1f) * _e158), vec3(_e160));
}

fn applyTonemap_u0028_vf3_u003b(color_6: ptr<function, vec3<f32>>) -> vec3<f32> {
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: f32;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e94 = (*color_6);
    (*color_6) = (_e94 * tonemap_exposure);
    if override_type_7_ {
        if override_type_7_1 {
            let _e96 = (*color_6);
            param_1 = _e96;
            param_2 = hdr_peak_norm;
            let _e97 = tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b((&param_1), (&param_2));
            local_2 = _e97;
        } else {
            let _e98 = (*color_6);
            param_3 = _e98;
            let _e99 = tonemapPBRNeutral_u0028_vf3_u003b((&param_3));
            local_2 = _e99;
        }
        let _e100 = local_2;
        return _e100;
    } else {
        if override_type_7_2 {
            let _e101 = (*color_6);
            param_4 = _e101;
            let _e102 = tonemapAgX_u0028_vf3_u003b((&param_4));
            return _e102;
        } else {
            if override_type_7_3 {
                let _e103 = (*color_6);
                param_5 = _e103;
                let _e104 = tonemapLottes_u0028_vf3_u003b((&param_5));
                return _e104;
            } else {
                if override_type_7_4 {
                    let _e105 = (*color_6);
                    param_6 = _e105;
                    let _e106 = tonemapReinhard_u0028_vf3_u003b((&param_6));
                    return _e106;
                } else {
                    let _e107 = (*color_6);
                    return _e107;
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

    let _e93 = (*uv);
    dir = (_e93 - vec2<f32>(0.5f, 0.5f));
    let _e95 = dir;
    radial = length(_e95);
    let _e97 = dir;
    let _e98 = radial;
    offset_2 = (_e97 * ((chromatic_strength * _e98) * 0.015f));
    let _e102 = (*uv);
    let _e103 = offset_2;
    let _e108 = textureSample(texture0_, texture0_sampler, clamp((_e102 + _e103), vec2(0f), vec2(1f)));
    r = _e108.x;
    let _e110 = (*uv);
    let _e111 = textureSample(texture0_, texture0_sampler, _e110);
    g_2 = _e111.y;
    let _e113 = (*uv);
    let _e114 = offset_2;
    let _e119 = textureSample(texture0_, texture0_sampler, clamp((_e113 - _e114), vec2(0f), vec2(1f)));
    b_1 = _e119.z;
    let _e121 = r;
    let _e122 = g_2;
    let _e123 = b_1;
    return vec3<f32>(_e121, _e122, _e123);
}

fn main_1() {
    var base: vec3<f32>;
    var local_3: vec3<f32>;
    var param_7: vec2<f32>;
    var param_8: vec3<f32>;
    var param_9: vec3<f32>;
    var luma_1: vec3<f32>;

    if (chromatic_strength > 0f) {
        let _e93 = frag_tex_coord_1;
        param_7 = _e93;
        let _e94 = sampleChromatic_u0028_vf2_u003b((&param_7));
        local_3 = _e94;
    } else {
        let _e95 = frag_tex_coord_1;
        let _e96 = textureSample(texture0_, texture0_sampler, _e95);
        local_3 = _e96.xyz;
    }
    let _e98 = local_3;
    base = _e98;
    let _e100 = eb.exposure_bias;
    let _e101 = base;
    base = (_e101 * _e100);
    let _e103 = base;
    param_8 = _e103;
    let _e104 = applyTonemap_u0028_vf3_u003b((&param_8));
    base = _e104;
    let _e105 = base;
    param_9 = _e105;
    let _e106 = applyColorGrading_u0028_vf3_u003b((&param_9));
    base = _e106;
    if (saturation != 1f) {
        let _e108 = base;
        luma_1 = vec3(dot(_e108, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e111 = luma_1;
        let _e112 = base;
        base = mix(_e111, _e112, vec3(saturation));
    }
    let _e115 = base;
    out_color = vec4<f32>(_e115.x, _e115.y, _e115.z, 1f);
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
