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
    let _e102 = colorGradeTint;
    let _e103 = (*color);
    (*color) = (_e103 * _e102);
    let _e105 = (*color);
    luma = dot(_e105, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e107 = luma;
    let _e109 = (*color);
    (*color) = mix(vec3(_e107), _e109, vec3(cg_saturation));
    let _e112 = (*color);
    (*color) = (((_e112 - vec3(0.5f)) * cg_contrast) + vec3(0.5f));
    let _e118 = (*color);
    return clamp(_e118, vec3(0f), vec3(1f));
}

fn tonemapReinhard_u0028_vf3_u003b(color_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e97 = (*color_1);
    let _e98 = (*color_1);
    return (_e97 / (vec3(1f) + _e98));
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
    let _e119 = midIn;
    let _e120 = a;
    let _e123 = hdrMax;
    let _e124 = a;
    let _e126 = midOut;
    let _e129 = hdrMax;
    let _e130 = a;
    let _e131 = d;
    let _e134 = midIn;
    let _e135 = a;
    let _e136 = d;
    let _e140 = midOut;
    b = ((-(pow(_e119, _e120)) + (pow(_e123, _e124) * _e126)) / ((pow(_e129, (_e130 * _e131)) - pow(_e134, (_e135 * _e136))) * _e140));
    let _e143 = hdrMax;
    let _e144 = a;
    let _e145 = d;
    let _e148 = midIn;
    let _e149 = a;
    let _e152 = hdrMax;
    let _e153 = a;
    let _e155 = midIn;
    let _e156 = a;
    let _e157 = d;
    let _e161 = midOut;
    let _e164 = hdrMax;
    let _e165 = a;
    let _e166 = d;
    let _e169 = midIn;
    let _e170 = a;
    let _e171 = d;
    let _e175 = midOut;
    c = (((pow(_e143, (_e144 * _e145)) * pow(_e148, _e149)) - ((pow(_e152, _e153) * pow(_e155, (_e156 * _e157))) * _e161)) / ((pow(_e164, (_e165 * _e166)) - pow(_e169, (_e170 * _e171))) * _e175));
    let _e178 = (*color_2);
    let _e179 = a;
    let _e181 = (*color_2);
    let _e182 = a;
    let _e183 = d;
    let _e186 = b;
    let _e188 = c;
    return (pow(_e178, _e179) / ((pow(_e181, (_e182 * _e183)) * _e186) + _e188));
}

fn agxSigmoid_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x2_: vec3<f32>;
    var x4_: vec3<f32>;

    let _e99 = (*x);
    let _e100 = (*x);
    x2_ = (_e99 * _e100);
    let _e102 = x2_;
    let _e103 = x2_;
    x4_ = (_e102 * _e103);
    let _e105 = x4_;
    let _e107 = x2_;
    let _e109 = x4_;
    let _e111 = (*x);
    let _e114 = x4_;
    let _e117 = x2_;
    let _e119 = (*x);
    let _e122 = x2_;
    let _e125 = (*x);
    return ((((((((_e105 * 15.5f) * _e107) - ((_e109 * 40.14f) * _e111)) + (_e114 * 31.96f)) - ((_e117 * 6.868f) * _e119)) + (_e122 * 0.4298f)) + (_e125 * 0.1191f)) - vec3(0.00232f));
}

fn tonemapAgX_u0028_vf3_u003b(color_3: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e98 = (*color_3);
    (*color_3) = (mat3x3<f32>(vec3<f32>(0.85662717f, 0.09512124f, 0.048251607f), vec3<f32>(0.13731897f, 0.761242f, 0.10143904f), vec3<f32>(0.11189821f, 0.076799415f, 0.81130236f)) * _e98);
    let _e100 = (*color_3);
    (*color_3) = log2(max(_e100, vec3<f32>(0.0000000001f, 0.0000000001f, 0.0000000001f)));
    let _e103 = (*color_3);
    (*color_3) = clamp(((_e103 - vec3(-12.47393f)) / vec3(16.499998f)), vec3(0f), vec3(1f));
    let _e111 = (*color_3);
    param = _e111;
    let _e112 = agxSigmoid_u0028_vf3_u003b((&param));
    (*color_3) = _e112;
    let _e113 = (*color_3);
    (*color_3) = (mat3x3<f32>(vec3<f32>(1.1271006f, -0.11060664f, -0.016493939f), vec3<f32>(-0.14132977f, 1.1578237f, -0.016493939f), vec3<f32>(-0.14132977f, -0.11060664f, 1.2519364f)) * _e113);
    let _e115 = (*color_3);
    return max(_e115, vec3<f32>(0f, 0f, 0f));
}

fn tonemapPBRNeutral_u0028_vf3_u003b(color_4: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x_1: f32;
    var offset: f32;
    var local: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e104 = (*color_4)[0u];
    let _e106 = (*color_4)[1u];
    let _e108 = (*color_4)[2u];
    x_1 = min(_e104, min(_e106, _e108));
    let _e111 = x_1;
    if (_e111 < 0.08f) {
        let _e113 = x_1;
        let _e114 = x_1;
        let _e116 = x_1;
        local = (_e113 - ((6.25f * _e114) * _e116));
    } else {
        local = 0.04f;
    }
    let _e119 = local;
    offset = _e119;
    let _e120 = offset;
    let _e121 = (*color_4);
    (*color_4) = (_e121 - vec3(_e120));
    let _e125 = (*color_4)[0u];
    let _e127 = (*color_4)[1u];
    let _e129 = (*color_4)[2u];
    peak = max(_e125, max(_e127, _e129));
    let _e132 = peak;
    if (_e132 < 0.76f) {
        let _e134 = (*color_4);
        return _e134;
    }
    let _e135 = peak;
    newPeak = (1f - (0.0576f / ((_e135 + 0.24f) - 0.76f)));
    let _e140 = newPeak;
    let _e141 = peak;
    let _e143 = (*color_4);
    (*color_4) = (_e143 * (_e140 / _e141));
    let _e145 = peak;
    let _e146 = newPeak;
    g = (1f - (1f / ((0.15f * (_e145 - _e146)) + 1f)));
    let _e152 = (*color_4);
    let _e153 = newPeak;
    let _e155 = g;
    return mix(_e152, (vec3<f32>(1f, 1f, 1f) * _e153), vec3(_e155));
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

    let _e106 = (*peak_1);
    startCompression = (0.76f * _e106);
    let _e109 = (*color_5)[0u];
    let _e111 = (*color_5)[1u];
    let _e113 = (*color_5)[2u];
    x_2 = min(_e109, min(_e111, _e113));
    let _e116 = x_2;
    if (_e116 < 0.08f) {
        let _e118 = x_2;
        let _e119 = x_2;
        let _e121 = x_2;
        local_1 = (_e118 - ((6.25f * _e119) * _e121));
    } else {
        local_1 = 0.04f;
    }
    let _e124 = local_1;
    offset_1 = _e124;
    let _e125 = offset_1;
    let _e126 = (*color_5);
    (*color_5) = (_e126 - vec3(_e125));
    let _e130 = (*color_5)[0u];
    let _e132 = (*color_5)[1u];
    let _e134 = (*color_5)[2u];
    pk = max(_e130, max(_e132, _e134));
    let _e137 = pk;
    let _e138 = startCompression;
    if (_e137 < _e138) {
        let _e140 = (*color_5);
        return _e140;
    }
    let _e141 = (*peak_1);
    let _e142 = startCompression;
    d_1 = (_e141 - _e142);
    let _e144 = (*peak_1);
    let _e145 = d_1;
    let _e146 = d_1;
    let _e148 = pk;
    let _e149 = d_1;
    let _e151 = startCompression;
    newPeak_1 = (_e144 - ((_e145 * _e146) / ((_e148 + _e149) - _e151)));
    let _e155 = newPeak_1;
    let _e156 = pk;
    let _e158 = (*color_5);
    (*color_5) = (_e158 * (_e155 / _e156));
    let _e160 = pk;
    let _e161 = newPeak_1;
    g_1 = (1f - (1f / ((0.15f * (_e160 - _e161)) + 1f)));
    let _e167 = (*color_5);
    let _e168 = newPeak_1;
    let _e170 = g_1;
    return mix(_e167, (vec3<f32>(1f, 1f, 1f) * _e168), vec3(_e170));
}

fn applyTonemap_u0028_vf3_u003b(color_6: ptr<function, vec3<f32>>) -> vec3<f32> {
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: f32;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e104 = (*color_6);
    (*color_6) = (_e104 * tonemap_exposure);
    if override_type_7_ {
        if override_type_7_1 {
            let _e106 = (*color_6);
            param_1 = _e106;
            param_2 = hdr_peak_norm;
            let _e107 = tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b((&param_1), (&param_2));
            local_2 = _e107;
        } else {
            let _e108 = (*color_6);
            param_3 = _e108;
            let _e109 = tonemapPBRNeutral_u0028_vf3_u003b((&param_3));
            local_2 = _e109;
        }
        let _e110 = local_2;
        return _e110;
    } else {
        if override_type_7_2 {
            let _e111 = (*color_6);
            param_4 = _e111;
            let _e112 = tonemapAgX_u0028_vf3_u003b((&param_4));
            return _e112;
        } else {
            if override_type_7_3 {
                let _e113 = (*color_6);
                param_5 = _e113;
                let _e114 = tonemapLottes_u0028_vf3_u003b((&param_5));
                return _e114;
            } else {
                if override_type_7_4 {
                    let _e115 = (*color_6);
                    param_6 = _e115;
                    let _e116 = tonemapReinhard_u0028_vf3_u003b((&param_6));
                    return _e116;
                } else {
                    let _e117 = (*color_6);
                    return _e117;
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

    let _e106 = frag_tex_coord_1;
    let _e108 = eb.sunScreenX;
    let _e110 = eb.sunScreenY;
    deltaUV = (((_e106 - vec2<f32>(_e108, _e110)) * sunrays_density) / vec2(f32(sunrays_samples)));
    let _e117 = frag_tex_coord_1;
    uv = _e117;
    illumination = vec3<f32>(0f, 0f, 0f);
    weight = 1f;
    i = 0i;
    loop {
        let _e118 = i;
        if (_e118 < sunrays_samples) {
            let _e120 = deltaUV;
            let _e121 = uv;
            uv = (_e121 - _e120);
            let _e124 = uv[0u];
            let _e125 = (_e124 < 0f);
            phi_555_ = _e125;
            if !(_e125) {
                let _e128 = uv[0u];
                phi_555_ = (_e128 > 1f);
            }
            let _e131 = phi_555_;
            phi_562_ = _e131;
            if !(_e131) {
                let _e134 = uv[1u];
                phi_562_ = (_e134 < 0f);
            }
            let _e137 = phi_562_;
            phi_569_ = _e137;
            if !(_e137) {
                let _e140 = uv[1u];
                phi_569_ = (_e140 > 1f);
            }
            let _e143 = phi_569_;
            if _e143 {
                break;
            }
            let _e144 = uv;
            let _e145 = textureSample(depthMap, depthMap_sampler, _e144);
            depth = _e145.x;
            let _e147 = depth;
            isSky = step(_e147, 0.001f);
            let _e149 = uv;
            let _e150 = textureSample(texture0_, texture0_sampler, _e149);
            sceneColor = _e150.xyz;
            threshold[0u] = sunrays_threshold;
            threshold[1u] = sunrays_threshold;
            threshold[2u] = sunrays_threshold;
            let _e155 = sceneColor;
            let _e156 = threshold;
            bright = max((_e155 - _e156), vec3<f32>(0f, 0f, 0f));
            let _e159 = bright;
            let _e160 = isSky;
            let _e162 = weight;
            let _e164 = illumination;
            illumination = (_e164 + ((_e159 * _e160) * _e162));
            let _e167 = eb.sunrayDecay;
            let _e168 = weight;
            weight = (_e168 * _e167);
            continue;
        } else {
            break;
        }
        continuing {
            let _e170 = i;
            i = (_e170 + 1i);
        }
    }
    let _e173 = illumination;
    illumination = (_e173 / vec3(f32(sunrays_samples)));
    let _e176 = illumination;
    let _e178 = eb.sunrayIntensity;
    return (_e176 * _e178);
}

fn sampleChromatic_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset_2: vec2<f32>;
    var r: f32;
    var g_2: f32;
    var b_1: f32;

    let _e103 = (*uv_1);
    dir = (_e103 - vec2<f32>(0.5f, 0.5f));
    let _e105 = dir;
    radial = length(_e105);
    let _e107 = dir;
    let _e108 = radial;
    offset_2 = (_e107 * ((chromatic_strength * _e108) * 0.015f));
    let _e112 = (*uv_1);
    let _e113 = offset_2;
    let _e118 = textureSample(texture0_, texture0_sampler, clamp((_e112 + _e113), vec2(0f), vec2(1f)));
    r = _e118.x;
    let _e120 = (*uv_1);
    let _e121 = textureSample(texture0_, texture0_sampler, _e120);
    g_2 = _e121.y;
    let _e123 = (*uv_1);
    let _e124 = offset_2;
    let _e129 = textureSample(texture0_, texture0_sampler, clamp((_e123 - _e124), vec2(0f), vec2(1f)));
    b_1 = _e129.z;
    let _e131 = r;
    let _e132 = g_2;
    let _e133 = b_1;
    return vec3<f32>(_e131, _e132, _e133);
}

fn main_1() {
    var base: vec3<f32>;
    var local_3: vec3<f32>;
    var param_7: vec2<f32>;
    var param_8: vec3<f32>;
    var param_9: vec3<f32>;
    var luma_1: vec3<f32>;

    if (chromatic_strength > 0f) {
        let _e103 = frag_tex_coord_1;
        param_7 = _e103;
        let _e104 = sampleChromatic_u0028_vf2_u003b((&param_7));
        local_3 = _e104;
    } else {
        let _e105 = frag_tex_coord_1;
        let _e106 = textureSample(texture0_, texture0_sampler, _e105);
        local_3 = _e106.xyz;
    }
    let _e108 = local_3;
    base = _e108;
    let _e110 = eb.exposure_bias;
    let _e111 = base;
    base = (_e111 * _e110);
    let _e113 = computeSunRays_u0028_();
    let _e114 = base;
    base = (_e114 + _e113);
    let _e116 = base;
    param_8 = _e116;
    let _e117 = applyTonemap_u0028_vf3_u003b((&param_8));
    base = _e117;
    let _e118 = base;
    param_9 = _e118;
    let _e119 = applyColorGrading_u0028_vf3_u003b((&param_9));
    base = _e119;
    if (saturation != 1f) {
        let _e121 = base;
        luma_1 = vec3(dot(_e121, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e124 = luma_1;
        let _e125 = base;
        base = mix(_e124, _e125, vec3(saturation));
    }
    let _e128 = base;
    out_color = vec4<f32>(_e128.x, _e128.y, _e128.z, 1f);
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
