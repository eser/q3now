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
    let _e92 = (*color);
    let _e93 = (*color);
    return (_e92 / (vec3(1f) + _e93));
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
    let _e114 = midIn;
    let _e115 = a;
    let _e118 = hdrMax;
    let _e119 = a;
    let _e121 = midOut;
    let _e124 = hdrMax;
    let _e125 = a;
    let _e126 = d;
    let _e129 = midIn;
    let _e130 = a;
    let _e131 = d;
    let _e135 = midOut;
    b = ((-(pow(_e114, _e115)) + (pow(_e118, _e119) * _e121)) / ((pow(_e124, (_e125 * _e126)) - pow(_e129, (_e130 * _e131))) * _e135));
    let _e138 = hdrMax;
    let _e139 = a;
    let _e140 = d;
    let _e143 = midIn;
    let _e144 = a;
    let _e147 = hdrMax;
    let _e148 = a;
    let _e150 = midIn;
    let _e151 = a;
    let _e152 = d;
    let _e156 = midOut;
    let _e159 = hdrMax;
    let _e160 = a;
    let _e161 = d;
    let _e164 = midIn;
    let _e165 = a;
    let _e166 = d;
    let _e170 = midOut;
    c = (((pow(_e138, (_e139 * _e140)) * pow(_e143, _e144)) - ((pow(_e147, _e148) * pow(_e150, (_e151 * _e152))) * _e156)) / ((pow(_e159, (_e160 * _e161)) - pow(_e164, (_e165 * _e166))) * _e170));
    let _e173 = (*color_1);
    let _e174 = a;
    let _e176 = (*color_1);
    let _e177 = a;
    let _e178 = d;
    let _e181 = b;
    let _e183 = c;
    return (pow(_e173, _e174) / ((pow(_e176, (_e177 * _e178)) * _e181) + _e183));
}

fn agxSigmoid_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x2_: vec3<f32>;
    var x4_: vec3<f32>;

    let _e94 = (*x);
    let _e95 = (*x);
    x2_ = (_e94 * _e95);
    let _e97 = x2_;
    let _e98 = x2_;
    x4_ = (_e97 * _e98);
    let _e100 = x4_;
    let _e102 = x2_;
    let _e104 = x4_;
    let _e106 = (*x);
    let _e109 = x4_;
    let _e112 = x2_;
    let _e114 = (*x);
    let _e117 = x2_;
    let _e120 = (*x);
    return ((((((((_e100 * 15.5f) * _e102) - ((_e104 * 40.14f) * _e106)) + (_e109 * 31.96f)) - ((_e112 * 6.868f) * _e114)) + (_e117 * 0.4298f)) + (_e120 * 0.1191f)) - vec3(0.00232f));
}

fn tonemapAgX_u0028_vf3_u003b(color_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e93 = (*color_2);
    (*color_2) = (mat3x3<f32>(vec3<f32>(0.85662717f, 0.09512124f, 0.048251607f), vec3<f32>(0.13731897f, 0.761242f, 0.10143904f), vec3<f32>(0.11189821f, 0.076799415f, 0.81130236f)) * _e93);
    let _e95 = (*color_2);
    (*color_2) = log2(max(_e95, vec3<f32>(0.0000000001f, 0.0000000001f, 0.0000000001f)));
    let _e98 = (*color_2);
    (*color_2) = clamp(((_e98 - vec3(-12.47393f)) / vec3(16.499998f)), vec3(0f), vec3(1f));
    let _e106 = (*color_2);
    param = _e106;
    let _e107 = agxSigmoid_u0028_vf3_u003b((&param));
    (*color_2) = _e107;
    let _e108 = (*color_2);
    (*color_2) = (mat3x3<f32>(vec3<f32>(1.1271006f, -0.11060664f, -0.016493939f), vec3<f32>(-0.14132977f, 1.1578237f, -0.016493939f), vec3<f32>(-0.14132977f, -0.11060664f, 1.2519364f)) * _e108);
    let _e110 = (*color_2);
    return max(_e110, vec3<f32>(0f, 0f, 0f));
}

fn tonemapPBRNeutral_u0028_vf3_u003b(color_3: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x_1: f32;
    var offset: f32;
    var local: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e99 = (*color_3)[0u];
    let _e101 = (*color_3)[1u];
    let _e103 = (*color_3)[2u];
    x_1 = min(_e99, min(_e101, _e103));
    let _e106 = x_1;
    if (_e106 < 0.08f) {
        let _e108 = x_1;
        let _e109 = x_1;
        let _e111 = x_1;
        local = (_e108 - ((6.25f * _e109) * _e111));
    } else {
        local = 0.04f;
    }
    let _e114 = local;
    offset = _e114;
    let _e115 = offset;
    let _e116 = (*color_3);
    (*color_3) = (_e116 - vec3(_e115));
    let _e120 = (*color_3)[0u];
    let _e122 = (*color_3)[1u];
    let _e124 = (*color_3)[2u];
    peak = max(_e120, max(_e122, _e124));
    let _e127 = peak;
    if (_e127 < 0.76f) {
        let _e129 = (*color_3);
        return _e129;
    }
    let _e130 = peak;
    newPeak = (1f - (0.0576f / ((_e130 + 0.24f) - 0.76f)));
    let _e135 = newPeak;
    let _e136 = peak;
    let _e138 = (*color_3);
    (*color_3) = (_e138 * (_e135 / _e136));
    let _e140 = peak;
    let _e141 = newPeak;
    g = (1f - (1f / ((0.15f * (_e140 - _e141)) + 1f)));
    let _e147 = (*color_3);
    let _e148 = newPeak;
    let _e150 = g;
    return mix(_e147, (vec3<f32>(1f, 1f, 1f) * _e148), vec3(_e150));
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

    let _e101 = (*peak_1);
    startCompression = (0.76f * _e101);
    let _e104 = (*color_4)[0u];
    let _e106 = (*color_4)[1u];
    let _e108 = (*color_4)[2u];
    x_2 = min(_e104, min(_e106, _e108));
    let _e111 = x_2;
    if (_e111 < 0.08f) {
        let _e113 = x_2;
        let _e114 = x_2;
        let _e116 = x_2;
        local_1 = (_e113 - ((6.25f * _e114) * _e116));
    } else {
        local_1 = 0.04f;
    }
    let _e119 = local_1;
    offset_1 = _e119;
    let _e120 = offset_1;
    let _e121 = (*color_4);
    (*color_4) = (_e121 - vec3(_e120));
    let _e125 = (*color_4)[0u];
    let _e127 = (*color_4)[1u];
    let _e129 = (*color_4)[2u];
    pk = max(_e125, max(_e127, _e129));
    let _e132 = pk;
    let _e133 = startCompression;
    if (_e132 < _e133) {
        let _e135 = (*color_4);
        return _e135;
    }
    let _e136 = (*peak_1);
    let _e137 = startCompression;
    d_1 = (_e136 - _e137);
    let _e139 = (*peak_1);
    let _e140 = d_1;
    let _e141 = d_1;
    let _e143 = pk;
    let _e144 = d_1;
    let _e146 = startCompression;
    newPeak_1 = (_e139 - ((_e140 * _e141) / ((_e143 + _e144) - _e146)));
    let _e150 = newPeak_1;
    let _e151 = pk;
    let _e153 = (*color_4);
    (*color_4) = (_e153 * (_e150 / _e151));
    let _e155 = pk;
    let _e156 = newPeak_1;
    g_1 = (1f - (1f / ((0.15f * (_e155 - _e156)) + 1f)));
    let _e162 = (*color_4);
    let _e163 = newPeak_1;
    let _e165 = g_1;
    return mix(_e162, (vec3<f32>(1f, 1f, 1f) * _e163), vec3(_e165));
}

fn applyTonemap_u0028_vf3_u003b(color_5: ptr<function, vec3<f32>>) -> vec3<f32> {
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: f32;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e99 = (*color_5);
    (*color_5) = (_e99 * tonemap_exposure);
    if override_type_7_ {
        if override_type_7_1 {
            let _e101 = (*color_5);
            param_1 = _e101;
            param_2 = hdr_peak_norm;
            let _e102 = tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b((&param_1), (&param_2));
            local_2 = _e102;
        } else {
            let _e103 = (*color_5);
            param_3 = _e103;
            let _e104 = tonemapPBRNeutral_u0028_vf3_u003b((&param_3));
            local_2 = _e104;
        }
        let _e105 = local_2;
        return _e105;
    } else {
        if override_type_7_2 {
            let _e106 = (*color_5);
            param_4 = _e106;
            let _e107 = tonemapAgX_u0028_vf3_u003b((&param_4));
            return _e107;
        } else {
            if override_type_7_3 {
                let _e108 = (*color_5);
                param_5 = _e108;
                let _e109 = tonemapLottes_u0028_vf3_u003b((&param_5));
                return _e109;
            } else {
                if override_type_7_4 {
                    let _e110 = (*color_5);
                    param_6 = _e110;
                    let _e111 = tonemapReinhard_u0028_vf3_u003b((&param_6));
                    return _e111;
                } else {
                    let _e112 = (*color_5);
                    return _e112;
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

    let _e101 = frag_tex_coord_1;
    let _e103 = eb.sunScreenX;
    let _e105 = eb.sunScreenY;
    deltaUV = (((_e101 - vec2<f32>(_e103, _e105)) * sunrays_density) / vec2(f32(sunrays_samples)));
    let _e112 = frag_tex_coord_1;
    uv = _e112;
    illumination = vec3<f32>(0f, 0f, 0f);
    weight = 1f;
    i = 0i;
    loop {
        let _e113 = i;
        if (_e113 < sunrays_samples) {
            let _e115 = deltaUV;
            let _e116 = uv;
            uv = (_e116 - _e115);
            let _e119 = uv[0u];
            let _e120 = (_e119 < 0f);
            phi_515_ = _e120;
            if !(_e120) {
                let _e123 = uv[0u];
                phi_515_ = (_e123 > 1f);
            }
            let _e126 = phi_515_;
            phi_522_ = _e126;
            if !(_e126) {
                let _e129 = uv[1u];
                phi_522_ = (_e129 < 0f);
            }
            let _e132 = phi_522_;
            phi_529_ = _e132;
            if !(_e132) {
                let _e135 = uv[1u];
                phi_529_ = (_e135 > 1f);
            }
            let _e138 = phi_529_;
            if _e138 {
                break;
            }
            let _e139 = uv;
            let _e140 = textureSample(depthMap, depthMap_sampler, _e139);
            depth = _e140.x;
            let _e142 = depth;
            isSky = step(_e142, 0.001f);
            let _e144 = uv;
            let _e145 = textureSample(texture0_, texture0_sampler, _e144);
            sceneColor = _e145.xyz;
            threshold[0u] = sunrays_threshold;
            threshold[1u] = sunrays_threshold;
            threshold[2u] = sunrays_threshold;
            let _e150 = sceneColor;
            let _e151 = threshold;
            bright = max((_e150 - _e151), vec3<f32>(0f, 0f, 0f));
            let _e154 = bright;
            let _e155 = isSky;
            let _e157 = weight;
            let _e159 = illumination;
            illumination = (_e159 + ((_e154 * _e155) * _e157));
            let _e162 = eb.sunrayDecay;
            let _e163 = weight;
            weight = (_e163 * _e162);
            continue;
        } else {
            break;
        }
        continuing {
            let _e165 = i;
            i = (_e165 + 1i);
        }
    }
    let _e168 = illumination;
    illumination = (_e168 / vec3(f32(sunrays_samples)));
    let _e171 = illumination;
    let _e173 = eb.sunrayIntensity;
    return (_e171 * _e173);
}

fn sampleChromatic_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset_2: vec2<f32>;
    var r: f32;
    var g_2: f32;
    var b_1: f32;

    let _e98 = (*uv_1);
    dir = (_e98 - vec2<f32>(0.5f, 0.5f));
    let _e100 = dir;
    radial = length(_e100);
    let _e102 = dir;
    let _e103 = radial;
    offset_2 = (_e102 * ((chromatic_strength * _e103) * 0.015f));
    let _e107 = (*uv_1);
    let _e108 = offset_2;
    let _e113 = textureSample(texture0_, texture0_sampler, clamp((_e107 + _e108), vec2(0f), vec2(1f)));
    r = _e113.x;
    let _e115 = (*uv_1);
    let _e116 = textureSample(texture0_, texture0_sampler, _e115);
    g_2 = _e116.y;
    let _e118 = (*uv_1);
    let _e119 = offset_2;
    let _e124 = textureSample(texture0_, texture0_sampler, clamp((_e118 - _e119), vec2(0f), vec2(1f)));
    b_1 = _e124.z;
    let _e126 = r;
    let _e127 = g_2;
    let _e128 = b_1;
    return vec3<f32>(_e126, _e127, _e128);
}

fn main_1() {
    var base: vec3<f32>;
    var local_3: vec3<f32>;
    var param_7: vec2<f32>;
    var param_8: vec3<f32>;
    var luma: vec3<f32>;

    if (chromatic_strength > 0f) {
        let _e97 = frag_tex_coord_1;
        param_7 = _e97;
        let _e98 = sampleChromatic_u0028_vf2_u003b((&param_7));
        local_3 = _e98;
    } else {
        let _e99 = frag_tex_coord_1;
        let _e100 = textureSample(texture0_, texture0_sampler, _e99);
        local_3 = _e100.xyz;
    }
    let _e102 = local_3;
    base = _e102;
    let _e104 = eb.exposure_bias;
    let _e105 = base;
    base = (_e105 * _e104);
    let _e107 = computeSunRays_u0028_();
    let _e108 = base;
    base = (_e108 + _e107);
    let _e110 = base;
    param_8 = _e110;
    let _e111 = applyTonemap_u0028_vf3_u003b((&param_8));
    base = _e111;
    if (saturation != 1f) {
        let _e113 = base;
        luma = vec3(dot(_e113, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e116 = luma;
        let _e117 = base;
        base = mix(_e116, _e117, vec3(saturation));
    }
    let _e120 = base;
    out_color = vec4<f32>(_e120.x, _e120.y, _e120.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
