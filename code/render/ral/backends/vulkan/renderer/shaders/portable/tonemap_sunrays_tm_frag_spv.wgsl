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

fn tonemapReinhard_u0028_vf3_u003b(color: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e94 = (*color);
    let _e95 = (*color);
    return (_e94 / (vec3(1f) + _e95));
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
    let _e116 = midIn;
    let _e117 = a;
    let _e120 = hdrMax;
    let _e121 = a;
    let _e123 = midOut;
    let _e126 = hdrMax;
    let _e127 = a;
    let _e128 = d;
    let _e131 = midIn;
    let _e132 = a;
    let _e133 = d;
    let _e137 = midOut;
    b = ((-(pow(_e116, _e117)) + (pow(_e120, _e121) * _e123)) / ((pow(_e126, (_e127 * _e128)) - pow(_e131, (_e132 * _e133))) * _e137));
    let _e140 = hdrMax;
    let _e141 = a;
    let _e142 = d;
    let _e145 = midIn;
    let _e146 = a;
    let _e149 = hdrMax;
    let _e150 = a;
    let _e152 = midIn;
    let _e153 = a;
    let _e154 = d;
    let _e158 = midOut;
    let _e161 = hdrMax;
    let _e162 = a;
    let _e163 = d;
    let _e166 = midIn;
    let _e167 = a;
    let _e168 = d;
    let _e172 = midOut;
    c = (((pow(_e140, (_e141 * _e142)) * pow(_e145, _e146)) - ((pow(_e149, _e150) * pow(_e152, (_e153 * _e154))) * _e158)) / ((pow(_e161, (_e162 * _e163)) - pow(_e166, (_e167 * _e168))) * _e172));
    let _e175 = (*color_1);
    let _e176 = a;
    let _e178 = (*color_1);
    let _e179 = a;
    let _e180 = d;
    let _e183 = b;
    let _e185 = c;
    return (pow(_e175, _e176) / ((pow(_e178, (_e179 * _e180)) * _e183) + _e185));
}

fn agxSigmoid_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x2_: vec3<f32>;
    var x4_: vec3<f32>;

    let _e96 = (*x);
    let _e97 = (*x);
    x2_ = (_e96 * _e97);
    let _e99 = x2_;
    let _e100 = x2_;
    x4_ = (_e99 * _e100);
    let _e102 = x4_;
    let _e104 = x2_;
    let _e106 = x4_;
    let _e108 = (*x);
    let _e111 = x4_;
    let _e114 = x2_;
    let _e116 = (*x);
    let _e119 = x2_;
    let _e122 = (*x);
    return ((((((((_e102 * 15.5f) * _e104) - ((_e106 * 40.14f) * _e108)) + (_e111 * 31.96f)) - ((_e114 * 6.868f) * _e116)) + (_e119 * 0.4298f)) + (_e122 * 0.1191f)) - vec3(0.00232f));
}

fn tonemapAgX_u0028_vf3_u003b(color_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e95 = (*color_2);
    (*color_2) = (mat3x3<f32>(vec3<f32>(0.85662717f, 0.09512124f, 0.048251607f), vec3<f32>(0.13731897f, 0.761242f, 0.10143904f), vec3<f32>(0.11189821f, 0.076799415f, 0.81130236f)) * _e95);
    let _e97 = (*color_2);
    (*color_2) = log2(max(_e97, vec3<f32>(0.0000000001f, 0.0000000001f, 0.0000000001f)));
    let _e100 = (*color_2);
    (*color_2) = clamp(((_e100 - vec3(-12.47393f)) / vec3(16.499998f)), vec3(0f), vec3(1f));
    let _e108 = (*color_2);
    param = _e108;
    let _e109 = agxSigmoid_u0028_vf3_u003b((&param));
    (*color_2) = _e109;
    let _e110 = (*color_2);
    (*color_2) = (mat3x3<f32>(vec3<f32>(1.1271006f, -0.11060664f, -0.016493939f), vec3<f32>(-0.14132977f, 1.1578237f, -0.016493939f), vec3<f32>(-0.14132977f, -0.11060664f, 1.2519364f)) * _e110);
    let _e112 = (*color_2);
    return max(_e112, vec3<f32>(0f, 0f, 0f));
}

fn tonemapPBRNeutral_u0028_vf3_u003b(color_3: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x_1: f32;
    var offset: f32;
    var local: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e101 = (*color_3)[0u];
    let _e103 = (*color_3)[1u];
    let _e105 = (*color_3)[2u];
    x_1 = min(_e101, min(_e103, _e105));
    let _e108 = x_1;
    if (_e108 < 0.08f) {
        let _e110 = x_1;
        let _e111 = x_1;
        let _e113 = x_1;
        local = (_e110 - ((6.25f * _e111) * _e113));
    } else {
        local = 0.04f;
    }
    let _e116 = local;
    offset = _e116;
    let _e117 = offset;
    let _e118 = (*color_3);
    (*color_3) = (_e118 - vec3(_e117));
    let _e122 = (*color_3)[0u];
    let _e124 = (*color_3)[1u];
    let _e126 = (*color_3)[2u];
    peak = max(_e122, max(_e124, _e126));
    let _e129 = peak;
    if (_e129 < 0.76f) {
        let _e131 = (*color_3);
        return _e131;
    }
    let _e132 = peak;
    newPeak = (1f - (0.0576f / ((_e132 + 0.24f) - 0.76f)));
    let _e137 = newPeak;
    let _e138 = peak;
    let _e140 = (*color_3);
    (*color_3) = (_e140 * (_e137 / _e138));
    let _e142 = peak;
    let _e143 = newPeak;
    g = (1f - (1f / ((0.15f * (_e142 - _e143)) + 1f)));
    let _e149 = (*color_3);
    let _e150 = newPeak;
    let _e152 = g;
    return mix(_e149, (vec3<f32>(1f, 1f, 1f) * _e150), vec3(_e152));
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

    let _e103 = (*peak_1);
    startCompression = (0.76f * _e103);
    let _e106 = (*color_4)[0u];
    let _e108 = (*color_4)[1u];
    let _e110 = (*color_4)[2u];
    x_2 = min(_e106, min(_e108, _e110));
    let _e113 = x_2;
    if (_e113 < 0.08f) {
        let _e115 = x_2;
        let _e116 = x_2;
        let _e118 = x_2;
        local_1 = (_e115 - ((6.25f * _e116) * _e118));
    } else {
        local_1 = 0.04f;
    }
    let _e121 = local_1;
    offset_1 = _e121;
    let _e122 = offset_1;
    let _e123 = (*color_4);
    (*color_4) = (_e123 - vec3(_e122));
    let _e127 = (*color_4)[0u];
    let _e129 = (*color_4)[1u];
    let _e131 = (*color_4)[2u];
    pk = max(_e127, max(_e129, _e131));
    let _e134 = pk;
    let _e135 = startCompression;
    if (_e134 < _e135) {
        let _e137 = (*color_4);
        return _e137;
    }
    let _e138 = (*peak_1);
    let _e139 = startCompression;
    d_1 = (_e138 - _e139);
    let _e141 = (*peak_1);
    let _e142 = d_1;
    let _e143 = d_1;
    let _e145 = pk;
    let _e146 = d_1;
    let _e148 = startCompression;
    newPeak_1 = (_e141 - ((_e142 * _e143) / ((_e145 + _e146) - _e148)));
    let _e152 = newPeak_1;
    let _e153 = pk;
    let _e155 = (*color_4);
    (*color_4) = (_e155 * (_e152 / _e153));
    let _e157 = pk;
    let _e158 = newPeak_1;
    g_1 = (1f - (1f / ((0.15f * (_e157 - _e158)) + 1f)));
    let _e164 = (*color_4);
    let _e165 = newPeak_1;
    let _e167 = g_1;
    return mix(_e164, (vec3<f32>(1f, 1f, 1f) * _e165), vec3(_e167));
}

fn applyTonemap_u0028_vf3_u003b(color_5: ptr<function, vec3<f32>>) -> vec3<f32> {
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: f32;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e101 = (*color_5);
    (*color_5) = (_e101 * tonemap_exposure);
    if override_type_7_ {
        if override_type_7_1 {
            let _e103 = (*color_5);
            param_1 = _e103;
            param_2 = hdr_peak_norm;
            let _e104 = tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b((&param_1), (&param_2));
            local_2 = _e104;
        } else {
            let _e105 = (*color_5);
            param_3 = _e105;
            let _e106 = tonemapPBRNeutral_u0028_vf3_u003b((&param_3));
            local_2 = _e106;
        }
        let _e107 = local_2;
        return _e107;
    } else {
        if override_type_7_2 {
            let _e108 = (*color_5);
            param_4 = _e108;
            let _e109 = tonemapAgX_u0028_vf3_u003b((&param_4));
            return _e109;
        } else {
            if override_type_7_3 {
                let _e110 = (*color_5);
                param_5 = _e110;
                let _e111 = tonemapLottes_u0028_vf3_u003b((&param_5));
                return _e111;
            } else {
                if override_type_7_4 {
                    let _e112 = (*color_5);
                    param_6 = _e112;
                    let _e113 = tonemapReinhard_u0028_vf3_u003b((&param_6));
                    return _e113;
                } else {
                    let _e114 = (*color_5);
                    return _e114;
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
    var phi_515_: bool;
    var phi_522_: bool;
    var phi_529_: bool;

    let _e103 = frag_tex_coord_1;
    let _e105 = eb.sunScreenX;
    let _e107 = eb.sunScreenY;
    deltaUV = (((_e103 - vec2<f32>(_e105, _e107)) * sunrays_density) / vec2(f32(sunrays_samples)));
    let _e114 = frag_tex_coord_1;
    uv = _e114;
    illumination = vec3<f32>(0f, 0f, 0f);
    weight = 1f;
    i = 0i;
    loop {
        let _e115 = i;
        if (_e115 < sunrays_samples) {
            let _e117 = deltaUV;
            let _e118 = uv;
            uv = (_e118 - _e117);
            let _e121 = uv[0u];
            let _e122 = (_e121 < 0f);
            phi_515_ = _e122;
            if !(_e122) {
                let _e125 = uv[0u];
                phi_515_ = (_e125 > 1f);
            }
            let _e128 = phi_515_;
            phi_522_ = _e128;
            if !(_e128) {
                let _e131 = uv[1u];
                phi_522_ = (_e131 < 0f);
            }
            let _e134 = phi_522_;
            phi_529_ = _e134;
            if !(_e134) {
                let _e137 = uv[1u];
                phi_529_ = (_e137 > 1f);
            }
            let _e140 = phi_529_;
            if _e140 {
                break;
            }
            let _e141 = uv;
            let _e142 = textureSample(depthMap, depthMap_sampler, _e141);
            depth = _e142.x;
            let _e144 = depth;
            isSky = step(_e144, 0.001f);
            let _e146 = uv;
            let _e147 = textureSample(texture0_, texture0_sampler, _e146);
            sceneColor = _e147.xyz;
            threshold[0u] = sunrays_threshold;
            threshold[1u] = sunrays_threshold;
            threshold[2u] = sunrays_threshold;
            let _e152 = sceneColor;
            let _e153 = threshold;
            bright = max((_e152 - _e153), vec3<f32>(0f, 0f, 0f));
            let _e156 = bright;
            let _e157 = isSky;
            let _e159 = weight;
            let _e161 = illumination;
            illumination = (_e161 + ((_e156 * _e157) * _e159));
            let _e164 = eb.sunrayDecay;
            let _e165 = weight;
            weight = (_e165 * _e164);
            continue;
        } else {
            break;
        }
        continuing {
            let _e167 = i;
            i = (_e167 + 1i);
        }
    }
    let _e170 = illumination;
    illumination = (_e170 / vec3(f32(sunrays_samples)));
    let _e173 = illumination;
    let _e175 = eb.sunrayIntensity;
    return (_e173 * _e175);
}

fn sampleChromatic_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset_2: vec2<f32>;
    var r: f32;
    var g_2: f32;
    var b_1: f32;

    let _e100 = (*uv_1);
    dir = (_e100 - vec2<f32>(0.5f, 0.5f));
    let _e102 = dir;
    radial = length(_e102);
    let _e104 = dir;
    let _e105 = radial;
    offset_2 = (_e104 * ((chromatic_strength * _e105) * 0.015f));
    let _e109 = (*uv_1);
    let _e110 = offset_2;
    let _e115 = textureSample(texture0_, texture0_sampler, clamp((_e109 + _e110), vec2(0f), vec2(1f)));
    r = _e115.x;
    let _e117 = (*uv_1);
    let _e118 = textureSample(texture0_, texture0_sampler, _e117);
    g_2 = _e118.y;
    let _e120 = (*uv_1);
    let _e121 = offset_2;
    let _e126 = textureSample(texture0_, texture0_sampler, clamp((_e120 - _e121), vec2(0f), vec2(1f)));
    b_1 = _e126.z;
    let _e128 = r;
    let _e129 = g_2;
    let _e130 = b_1;
    return vec3<f32>(_e128, _e129, _e130);
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
    var phi_675_: bool;

    if (chromatic_strength > 0f) {
        let _e102 = frag_tex_coord_1;
        param_7 = _e102;
        let _e103 = sampleChromatic_u0028_vf2_u003b((&param_7));
        local_3 = _e103;
    } else {
        let _e104 = frag_tex_coord_1;
        let _e105 = textureSample(texture0_, texture0_sampler, _e104);
        local_3 = _e105.xyz;
    }
    let _e107 = local_3;
    base = _e107;
    let _e109 = eb.exposure_bias;
    let _e110 = base;
    base = (_e110 * _e109);
    let _e112 = base;
    shadowLuma = dot(max(_e112, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e116 = eb.shadowExponent;
    let _e118 = shadowLuma;
    let _e120 = ((_e116 != 1f) && (_e118 > 0f));
    phi_675_ = _e120;
    if _e120 {
        let _e121 = shadowLuma;
        let _e123 = eb.shadowPivot;
        phi_675_ = (_e121 < _e123);
    }
    let _e126 = phi_675_;
    if _e126 {
        let _e127 = shadowLuma;
        let _e129 = eb.shadowPivot;
        normalized = (_e127 / _e129);
        let _e131 = normalized;
        let _e133 = eb.shadowExponent;
        let _e136 = eb.shadowPivot;
        curved = (pow(_e131, _e133) * _e136);
        let _e138 = curved;
        let _e139 = shadowLuma;
        let _e141 = base;
        base = (_e141 * (_e138 / _e139));
    }
    let _e143 = computeSunRays_u0028_();
    let _e144 = base;
    base = (_e144 + _e143);
    let _e146 = base;
    param_8 = _e146;
    let _e147 = applyTonemap_u0028_vf3_u003b((&param_8));
    base = _e147;
    if (saturation != 1f) {
        let _e149 = base;
        luma = vec3(dot(_e149, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e152 = luma;
        let _e153 = base;
        base = mix(_e152, _e153, vec3(saturation));
    }
    let _e156 = base;
    out_color = vec4<f32>(_e156.x, _e156.y, _e156.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
