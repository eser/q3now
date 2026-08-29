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
@id(19) override cg_tint_r: f32 = 1f;
@id(20) override cg_tint_g: f32 = 1f;
@id(21) override cg_tint_b: f32 = 1f;
@id(22) override cg_saturation: f32 = 1f;
@id(23) override cg_contrast: f32 = 1f;
@id(27) override sunrays_density: f32 = 1f;
@id(26) override sunrays_samples: i32 = 64i;
@id(33) override sunrays_threshold: f32 = 0.32f;
@id(24) override chromatic_strength: f32 = 0f;
@id(2) override saturation: f32 = 1f;

var<private> frag_tex_coord_1: vec2<f32>;
@group(2) @binding(0)
var<uniform> eb: ExposureBlock;
@group(1) @binding(0)
var depthMap: texture_2d<f32>;
@group(1) @binding(32)
var depthMap_sampler: sampler;
@group(0) @binding(0)
var texture0_: texture_2d<f32>;
@group(0) @binding(32)
var texture0_sampler: sampler;
var<private> out_color: vec4<f32>;

fn applyColorGrading_u0028_vf3_u003b(color: ptr<function, vec3<f32>>) -> vec3<f32> {
    var colorGradeTint: vec3<f32>;
    var luma: f32;

    colorGradeTint[0u] = cg_tint_r;
    colorGradeTint[1u] = cg_tint_g;
    colorGradeTint[2u] = cg_tint_b;
    let _e104 = colorGradeTint;
    let _e105 = (*color);
    (*color) = (_e105 * _e104);
    let _e107 = (*color);
    luma = dot(_e107, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e109 = luma;
    let _e111 = (*color);
    (*color) = mix(vec3(_e109), _e111, vec3(cg_saturation));
    let _e114 = (*color);
    (*color) = (((_e114 - vec3(0.5f)) * cg_contrast) + vec3(0.5f));
    let _e120 = (*color);
    return clamp(_e120, vec3(0f), vec3(1f));
}

fn tonemapReinhard_u0028_vf3_u003b(color_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e99 = (*color_1);
    let _e100 = (*color_1);
    return (_e99 / (vec3(1f) + _e100));
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
    let _e121 = midIn;
    let _e122 = a;
    let _e125 = hdrMax;
    let _e126 = a;
    let _e128 = midOut;
    let _e131 = hdrMax;
    let _e132 = a;
    let _e133 = d;
    let _e136 = midIn;
    let _e137 = a;
    let _e138 = d;
    let _e142 = midOut;
    b = ((-(pow(_e121, _e122)) + (pow(_e125, _e126) * _e128)) / ((pow(_e131, (_e132 * _e133)) - pow(_e136, (_e137 * _e138))) * _e142));
    let _e145 = hdrMax;
    let _e146 = a;
    let _e147 = d;
    let _e150 = midIn;
    let _e151 = a;
    let _e154 = hdrMax;
    let _e155 = a;
    let _e157 = midIn;
    let _e158 = a;
    let _e159 = d;
    let _e163 = midOut;
    let _e166 = hdrMax;
    let _e167 = a;
    let _e168 = d;
    let _e171 = midIn;
    let _e172 = a;
    let _e173 = d;
    let _e177 = midOut;
    c = (((pow(_e145, (_e146 * _e147)) * pow(_e150, _e151)) - ((pow(_e154, _e155) * pow(_e157, (_e158 * _e159))) * _e163)) / ((pow(_e166, (_e167 * _e168)) - pow(_e171, (_e172 * _e173))) * _e177));
    let _e180 = (*color_2);
    let _e181 = a;
    let _e183 = (*color_2);
    let _e184 = a;
    let _e185 = d;
    let _e188 = b;
    let _e190 = c;
    return (pow(_e180, _e181) / ((pow(_e183, (_e184 * _e185)) * _e188) + _e190));
}

fn agxSigmoid_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x2_: vec3<f32>;
    var x4_: vec3<f32>;

    let _e101 = (*x);
    let _e102 = (*x);
    x2_ = (_e101 * _e102);
    let _e104 = x2_;
    let _e105 = x2_;
    x4_ = (_e104 * _e105);
    let _e107 = x4_;
    let _e109 = x2_;
    let _e111 = x4_;
    let _e113 = (*x);
    let _e116 = x4_;
    let _e119 = x2_;
    let _e121 = (*x);
    let _e124 = x2_;
    let _e127 = (*x);
    return ((((((((_e107 * 15.5f) * _e109) - ((_e111 * 40.14f) * _e113)) + (_e116 * 31.96f)) - ((_e119 * 6.868f) * _e121)) + (_e124 * 0.4298f)) + (_e127 * 0.1191f)) - vec3(0.00232f));
}

fn tonemapAgX_u0028_vf3_u003b(color_3: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e100 = (*color_3);
    (*color_3) = (mat3x3<f32>(vec3<f32>(0.85662717f, 0.09512124f, 0.048251607f), vec3<f32>(0.13731897f, 0.761242f, 0.10143904f), vec3<f32>(0.11189821f, 0.076799415f, 0.81130236f)) * _e100);
    let _e102 = (*color_3);
    (*color_3) = log2(max(_e102, vec3<f32>(0.0000000001f, 0.0000000001f, 0.0000000001f)));
    let _e105 = (*color_3);
    (*color_3) = clamp(((_e105 - vec3(-12.47393f)) / vec3(16.499998f)), vec3(0f), vec3(1f));
    let _e113 = (*color_3);
    param = _e113;
    let _e114 = agxSigmoid_u0028_vf3_u003b((&param));
    (*color_3) = _e114;
    let _e115 = (*color_3);
    (*color_3) = (mat3x3<f32>(vec3<f32>(1.1271006f, -0.11060664f, -0.016493939f), vec3<f32>(-0.14132977f, 1.1578237f, -0.016493939f), vec3<f32>(-0.14132977f, -0.11060664f, 1.2519364f)) * _e115);
    let _e117 = (*color_3);
    return max(_e117, vec3<f32>(0f, 0f, 0f));
}

fn tonemapPBRNeutral_u0028_vf3_u003b(color_4: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x_1: f32;
    var offset: f32;
    var local: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e106 = (*color_4)[0u];
    let _e108 = (*color_4)[1u];
    let _e110 = (*color_4)[2u];
    x_1 = min(_e106, min(_e108, _e110));
    let _e113 = x_1;
    if (_e113 < 0.08f) {
        let _e115 = x_1;
        let _e116 = x_1;
        let _e118 = x_1;
        local = (_e115 - ((6.25f * _e116) * _e118));
    } else {
        local = 0.04f;
    }
    let _e121 = local;
    offset = _e121;
    let _e122 = offset;
    let _e123 = (*color_4);
    (*color_4) = (_e123 - vec3(_e122));
    let _e127 = (*color_4)[0u];
    let _e129 = (*color_4)[1u];
    let _e131 = (*color_4)[2u];
    peak = max(_e127, max(_e129, _e131));
    let _e134 = peak;
    if (_e134 < 0.76f) {
        let _e136 = (*color_4);
        return _e136;
    }
    let _e137 = peak;
    newPeak = (1f - (0.0576f / ((_e137 + 0.24f) - 0.76f)));
    let _e142 = newPeak;
    let _e143 = peak;
    let _e145 = (*color_4);
    (*color_4) = (_e145 * (_e142 / _e143));
    let _e147 = peak;
    let _e148 = newPeak;
    g = (1f - (1f / ((0.15f * (_e147 - _e148)) + 1f)));
    let _e154 = (*color_4);
    let _e155 = newPeak;
    let _e157 = g;
    return mix(_e154, (vec3<f32>(1f, 1f, 1f) * _e155), vec3(_e157));
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

    let _e108 = (*peak_1);
    startCompression = (0.76f * _e108);
    let _e111 = (*color_5)[0u];
    let _e113 = (*color_5)[1u];
    let _e115 = (*color_5)[2u];
    x_2 = min(_e111, min(_e113, _e115));
    let _e118 = x_2;
    if (_e118 < 0.08f) {
        let _e120 = x_2;
        let _e121 = x_2;
        let _e123 = x_2;
        local_1 = (_e120 - ((6.25f * _e121) * _e123));
    } else {
        local_1 = 0.04f;
    }
    let _e126 = local_1;
    offset_1 = _e126;
    let _e127 = offset_1;
    let _e128 = (*color_5);
    (*color_5) = (_e128 - vec3(_e127));
    let _e132 = (*color_5)[0u];
    let _e134 = (*color_5)[1u];
    let _e136 = (*color_5)[2u];
    pk = max(_e132, max(_e134, _e136));
    let _e139 = pk;
    let _e140 = startCompression;
    if (_e139 < _e140) {
        let _e142 = (*color_5);
        return _e142;
    }
    let _e143 = (*peak_1);
    let _e144 = startCompression;
    d_1 = (_e143 - _e144);
    let _e146 = (*peak_1);
    let _e147 = d_1;
    let _e148 = d_1;
    let _e150 = pk;
    let _e151 = d_1;
    let _e153 = startCompression;
    newPeak_1 = (_e146 - ((_e147 * _e148) / ((_e150 + _e151) - _e153)));
    let _e157 = newPeak_1;
    let _e158 = pk;
    let _e160 = (*color_5);
    (*color_5) = (_e160 * (_e157 / _e158));
    let _e162 = pk;
    let _e163 = newPeak_1;
    g_1 = (1f - (1f / ((0.15f * (_e162 - _e163)) + 1f)));
    let _e169 = (*color_5);
    let _e170 = newPeak_1;
    let _e172 = g_1;
    return mix(_e169, (vec3<f32>(1f, 1f, 1f) * _e170), vec3(_e172));
}

fn applyTonemap_u0028_vf3_u003b(color_6: ptr<function, vec3<f32>>) -> vec3<f32> {
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: f32;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e106 = (*color_6);
    (*color_6) = (_e106 * tonemap_exposure);
    if override_type_7_ {
        if override_type_7_1 {
            let _e108 = (*color_6);
            param_1 = _e108;
            param_2 = hdr_peak_norm;
            let _e109 = tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b((&param_1), (&param_2));
            local_2 = _e109;
        } else {
            let _e110 = (*color_6);
            param_3 = _e110;
            let _e111 = tonemapPBRNeutral_u0028_vf3_u003b((&param_3));
            local_2 = _e111;
        }
        let _e112 = local_2;
        return _e112;
    } else {
        if override_type_7_2 {
            let _e113 = (*color_6);
            param_4 = _e113;
            let _e114 = tonemapAgX_u0028_vf3_u003b((&param_4));
            return _e114;
        } else {
            if override_type_7_3 {
                let _e115 = (*color_6);
                param_5 = _e115;
                let _e116 = tonemapLottes_u0028_vf3_u003b((&param_5));
                return _e116;
            } else {
                if override_type_7_4 {
                    let _e117 = (*color_6);
                    param_6 = _e117;
                    let _e118 = tonemapReinhard_u0028_vf3_u003b((&param_6));
                    return _e118;
                } else {
                    let _e119 = (*color_6);
                    return _e119;
                }
            }
        }
    }
}

fn computeSunRays_u0028_() -> vec3<f32> {
    var deltaUV: vec2<f32>;
    var uv: vec2<f32>;
    var illumination: vec3<f32>;
    var weight: f32;
    var i: i32;
    var depth: f32;
    var isSky: f32;
    var sceneColor: vec3<f32>;
    var threshold: vec3<f32>;
    var bright: vec3<f32>;
    var phi_555_: bool;
    var phi_562_: bool;
    var phi_569_: bool;

    let _e108 = frag_tex_coord_1;
    let _e110 = eb.sunScreenX;
    let _e112 = eb.sunScreenY;
    deltaUV = (((_e108 - vec2<f32>(_e110, _e112)) * sunrays_density) / vec2(f32(sunrays_samples)));
    let _e119 = frag_tex_coord_1;
    uv = _e119;
    illumination = vec3<f32>(0f, 0f, 0f);
    weight = 1f;
    i = 0i;
    loop {
        let _e120 = i;
        if (_e120 < sunrays_samples) {
            let _e122 = deltaUV;
            let _e123 = uv;
            uv = (_e123 - _e122);
            let _e126 = uv[0u];
            let _e127 = (_e126 < 0f);
            phi_555_ = _e127;
            if !(_e127) {
                let _e130 = uv[0u];
                phi_555_ = (_e130 > 1f);
            }
            let _e133 = phi_555_;
            phi_562_ = _e133;
            if !(_e133) {
                let _e136 = uv[1u];
                phi_562_ = (_e136 < 0f);
            }
            let _e139 = phi_562_;
            phi_569_ = _e139;
            if !(_e139) {
                let _e142 = uv[1u];
                phi_569_ = (_e142 > 1f);
            }
            let _e145 = phi_569_;
            if _e145 {
                break;
            }
            let _e146 = uv;
            let _e147 = textureSample(depthMap, depthMap_sampler, _e146);
            depth = _e147.x;
            let _e149 = depth;
            isSky = step(_e149, 0.001f);
            let _e151 = uv;
            let _e152 = textureSample(texture0_, texture0_sampler, _e151);
            sceneColor = _e152.xyz;
            threshold[0u] = sunrays_threshold;
            threshold[1u] = sunrays_threshold;
            threshold[2u] = sunrays_threshold;
            let _e157 = sceneColor;
            let _e158 = threshold;
            bright = max((_e157 - _e158), vec3<f32>(0f, 0f, 0f));
            let _e161 = bright;
            let _e162 = isSky;
            let _e164 = weight;
            let _e166 = illumination;
            illumination = (_e166 + ((_e161 * _e162) * _e164));
            let _e169 = eb.sunrayDecay;
            let _e170 = weight;
            weight = (_e170 * _e169);
            continue;
        } else {
            break;
        }
        continuing {
            let _e172 = i;
            i = (_e172 + 1i);
        }
    }
    let _e175 = illumination;
    illumination = (_e175 / vec3(f32(sunrays_samples)));
    let _e178 = illumination;
    let _e180 = eb.sunrayIntensity;
    return (_e178 * _e180);
}

fn sampleChromatic_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset_2: vec2<f32>;
    var r: f32;
    var g_2: f32;
    var b_1: f32;

    let _e105 = (*uv_1);
    dir = (_e105 - vec2<f32>(0.5f, 0.5f));
    let _e107 = dir;
    radial = length(_e107);
    let _e109 = dir;
    let _e110 = radial;
    offset_2 = (_e109 * ((chromatic_strength * _e110) * 0.015f));
    let _e114 = (*uv_1);
    let _e115 = offset_2;
    let _e120 = textureSample(texture0_, texture0_sampler, clamp((_e114 + _e115), vec2(0f), vec2(1f)));
    r = _e120.x;
    let _e122 = (*uv_1);
    let _e123 = textureSample(texture0_, texture0_sampler, _e122);
    g_2 = _e123.y;
    let _e125 = (*uv_1);
    let _e126 = offset_2;
    let _e131 = textureSample(texture0_, texture0_sampler, clamp((_e125 - _e126), vec2(0f), vec2(1f)));
    b_1 = _e131.z;
    let _e133 = r;
    let _e134 = g_2;
    let _e135 = b_1;
    return vec3<f32>(_e133, _e134, _e135);
}

fn main_1() {
    var base: vec3<f32>;
    var local_3: vec3<f32>;
    var param_7: vec2<f32>;
    var shadowLuma: f32;
    var normalized: f32;
    var curved: f32;
    var param_8: vec3<f32>;
    var param_9: vec3<f32>;
    var luma_1: vec3<f32>;
    var phi_710_: bool;

    if (chromatic_strength > 0f) {
        let _e108 = frag_tex_coord_1;
        param_7 = _e108;
        let _e109 = sampleChromatic_u0028_vf2_u003b((&param_7));
        local_3 = _e109;
    } else {
        let _e110 = frag_tex_coord_1;
        let _e111 = textureSample(texture0_, texture0_sampler, _e110);
        local_3 = _e111.xyz;
    }
    let _e113 = local_3;
    base = _e113;
    let _e115 = eb.exposure_bias;
    let _e116 = base;
    base = (_e116 * _e115);
    let _e118 = base;
    shadowLuma = dot(max(_e118, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e122 = eb.shadowExponent;
    let _e124 = shadowLuma;
    let _e126 = ((_e122 != 1f) && (_e124 > 0f));
    phi_710_ = _e126;
    if _e126 {
        let _e127 = shadowLuma;
        let _e129 = eb.shadowPivot;
        phi_710_ = (_e127 < _e129);
    }
    let _e132 = phi_710_;
    if _e132 {
        let _e133 = shadowLuma;
        let _e135 = eb.shadowPivot;
        normalized = (_e133 / _e135);
        let _e137 = normalized;
        let _e139 = eb.shadowExponent;
        let _e142 = eb.shadowPivot;
        curved = (pow(_e137, _e139) * _e142);
        let _e144 = curved;
        let _e145 = shadowLuma;
        let _e147 = base;
        base = (_e147 * (_e144 / _e145));
    }
    let _e149 = computeSunRays_u0028_();
    let _e150 = base;
    base = (_e150 + _e149);
    let _e152 = base;
    param_8 = _e152;
    let _e153 = applyTonemap_u0028_vf3_u003b((&param_8));
    base = _e153;
    let _e154 = base;
    param_9 = _e154;
    let _e155 = applyColorGrading_u0028_vf3_u003b((&param_9));
    base = _e155;
    if (saturation != 1f) {
        let _e157 = base;
        luma_1 = vec3(dot(_e157, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e160 = luma_1;
        let _e161 = base;
        base = mix(_e160, _e161, vec3(saturation));
    }
    let _e164 = base;
    out_color = vec4<f32>(_e164.x, _e164.y, _e164.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
