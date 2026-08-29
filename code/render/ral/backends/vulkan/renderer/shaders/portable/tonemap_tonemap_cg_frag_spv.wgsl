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
    let _e94 = colorGradeTint;
    let _e95 = (*color);
    (*color) = (_e95 * _e94);
    let _e97 = (*color);
    luma = dot(_e97, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e99 = luma;
    let _e101 = (*color);
    (*color) = mix(vec3(_e99), _e101, vec3(cg_saturation));
    let _e104 = (*color);
    (*color) = (((_e104 - vec3(0.5f)) * cg_contrast) + vec3(0.5f));
    let _e110 = (*color);
    return clamp(_e110, vec3(0f), vec3(1f));
}

fn tonemapReinhard_u0028_vf3_u003b(color_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    let _e89 = (*color_1);
    let _e90 = (*color_1);
    return (_e89 / (vec3(1f) + _e90));
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
    let _e111 = midIn;
    let _e112 = a;
    let _e115 = hdrMax;
    let _e116 = a;
    let _e118 = midOut;
    let _e121 = hdrMax;
    let _e122 = a;
    let _e123 = d;
    let _e126 = midIn;
    let _e127 = a;
    let _e128 = d;
    let _e132 = midOut;
    b = ((-(pow(_e111, _e112)) + (pow(_e115, _e116) * _e118)) / ((pow(_e121, (_e122 * _e123)) - pow(_e126, (_e127 * _e128))) * _e132));
    let _e135 = hdrMax;
    let _e136 = a;
    let _e137 = d;
    let _e140 = midIn;
    let _e141 = a;
    let _e144 = hdrMax;
    let _e145 = a;
    let _e147 = midIn;
    let _e148 = a;
    let _e149 = d;
    let _e153 = midOut;
    let _e156 = hdrMax;
    let _e157 = a;
    let _e158 = d;
    let _e161 = midIn;
    let _e162 = a;
    let _e163 = d;
    let _e167 = midOut;
    c = (((pow(_e135, (_e136 * _e137)) * pow(_e140, _e141)) - ((pow(_e144, _e145) * pow(_e147, (_e148 * _e149))) * _e153)) / ((pow(_e156, (_e157 * _e158)) - pow(_e161, (_e162 * _e163))) * _e167));
    let _e170 = (*color_2);
    let _e171 = a;
    let _e173 = (*color_2);
    let _e174 = a;
    let _e175 = d;
    let _e178 = b;
    let _e180 = c;
    return (pow(_e170, _e171) / ((pow(_e173, (_e174 * _e175)) * _e178) + _e180));
}

fn agxSigmoid_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x2_: vec3<f32>;
    var x4_: vec3<f32>;

    let _e91 = (*x);
    let _e92 = (*x);
    x2_ = (_e91 * _e92);
    let _e94 = x2_;
    let _e95 = x2_;
    x4_ = (_e94 * _e95);
    let _e97 = x4_;
    let _e99 = x2_;
    let _e101 = x4_;
    let _e103 = (*x);
    let _e106 = x4_;
    let _e109 = x2_;
    let _e111 = (*x);
    let _e114 = x2_;
    let _e117 = (*x);
    return ((((((((_e97 * 15.5f) * _e99) - ((_e101 * 40.14f) * _e103)) + (_e106 * 31.96f)) - ((_e109 * 6.868f) * _e111)) + (_e114 * 0.4298f)) + (_e117 * 0.1191f)) - vec3(0.00232f));
}

fn tonemapAgX_u0028_vf3_u003b(color_3: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param: vec3<f32>;

    let _e90 = (*color_3);
    (*color_3) = (mat3x3<f32>(vec3<f32>(0.85662717f, 0.09512124f, 0.048251607f), vec3<f32>(0.13731897f, 0.761242f, 0.10143904f), vec3<f32>(0.11189821f, 0.076799415f, 0.81130236f)) * _e90);
    let _e92 = (*color_3);
    (*color_3) = log2(max(_e92, vec3<f32>(0.0000000001f, 0.0000000001f, 0.0000000001f)));
    let _e95 = (*color_3);
    (*color_3) = clamp(((_e95 - vec3(-12.47393f)) / vec3(16.499998f)), vec3(0f), vec3(1f));
    let _e103 = (*color_3);
    param = _e103;
    let _e104 = agxSigmoid_u0028_vf3_u003b((&param));
    (*color_3) = _e104;
    let _e105 = (*color_3);
    (*color_3) = (mat3x3<f32>(vec3<f32>(1.1271006f, -0.11060664f, -0.016493939f), vec3<f32>(-0.14132977f, 1.1578237f, -0.016493939f), vec3<f32>(-0.14132977f, -0.11060664f, 1.2519364f)) * _e105);
    let _e107 = (*color_3);
    return max(_e107, vec3<f32>(0f, 0f, 0f));
}

fn tonemapPBRNeutral_u0028_vf3_u003b(color_4: ptr<function, vec3<f32>>) -> vec3<f32> {
    var x_1: f32;
    var offset: f32;
    var local: f32;
    var peak: f32;
    var newPeak: f32;
    var g: f32;

    let _e96 = (*color_4)[0u];
    let _e98 = (*color_4)[1u];
    let _e100 = (*color_4)[2u];
    x_1 = min(_e96, min(_e98, _e100));
    let _e103 = x_1;
    if (_e103 < 0.08f) {
        let _e105 = x_1;
        let _e106 = x_1;
        let _e108 = x_1;
        local = (_e105 - ((6.25f * _e106) * _e108));
    } else {
        local = 0.04f;
    }
    let _e111 = local;
    offset = _e111;
    let _e112 = offset;
    let _e113 = (*color_4);
    (*color_4) = (_e113 - vec3(_e112));
    let _e117 = (*color_4)[0u];
    let _e119 = (*color_4)[1u];
    let _e121 = (*color_4)[2u];
    peak = max(_e117, max(_e119, _e121));
    let _e124 = peak;
    if (_e124 < 0.76f) {
        let _e126 = (*color_4);
        return _e126;
    }
    let _e127 = peak;
    newPeak = (1f - (0.0576f / ((_e127 + 0.24f) - 0.76f)));
    let _e132 = newPeak;
    let _e133 = peak;
    let _e135 = (*color_4);
    (*color_4) = (_e135 * (_e132 / _e133));
    let _e137 = peak;
    let _e138 = newPeak;
    g = (1f - (1f / ((0.15f * (_e137 - _e138)) + 1f)));
    let _e144 = (*color_4);
    let _e145 = newPeak;
    let _e147 = g;
    return mix(_e144, (vec3<f32>(1f, 1f, 1f) * _e145), vec3(_e147));
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

    let _e98 = (*peak_1);
    startCompression = (0.76f * _e98);
    let _e101 = (*color_5)[0u];
    let _e103 = (*color_5)[1u];
    let _e105 = (*color_5)[2u];
    x_2 = min(_e101, min(_e103, _e105));
    let _e108 = x_2;
    if (_e108 < 0.08f) {
        let _e110 = x_2;
        let _e111 = x_2;
        let _e113 = x_2;
        local_1 = (_e110 - ((6.25f * _e111) * _e113));
    } else {
        local_1 = 0.04f;
    }
    let _e116 = local_1;
    offset_1 = _e116;
    let _e117 = offset_1;
    let _e118 = (*color_5);
    (*color_5) = (_e118 - vec3(_e117));
    let _e122 = (*color_5)[0u];
    let _e124 = (*color_5)[1u];
    let _e126 = (*color_5)[2u];
    pk = max(_e122, max(_e124, _e126));
    let _e129 = pk;
    let _e130 = startCompression;
    if (_e129 < _e130) {
        let _e132 = (*color_5);
        return _e132;
    }
    let _e133 = (*peak_1);
    let _e134 = startCompression;
    d_1 = (_e133 - _e134);
    let _e136 = (*peak_1);
    let _e137 = d_1;
    let _e138 = d_1;
    let _e140 = pk;
    let _e141 = d_1;
    let _e143 = startCompression;
    newPeak_1 = (_e136 - ((_e137 * _e138) / ((_e140 + _e141) - _e143)));
    let _e147 = newPeak_1;
    let _e148 = pk;
    let _e150 = (*color_5);
    (*color_5) = (_e150 * (_e147 / _e148));
    let _e152 = pk;
    let _e153 = newPeak_1;
    g_1 = (1f - (1f / ((0.15f * (_e152 - _e153)) + 1f)));
    let _e159 = (*color_5);
    let _e160 = newPeak_1;
    let _e162 = g_1;
    return mix(_e159, (vec3<f32>(1f, 1f, 1f) * _e160), vec3(_e162));
}

fn applyTonemap_u0028_vf3_u003b(color_6: ptr<function, vec3<f32>>) -> vec3<f32> {
    var local_2: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: f32;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e96 = (*color_6);
    (*color_6) = (_e96 * tonemap_exposure);
    if override_type_7_ {
        if override_type_7_1 {
            let _e98 = (*color_6);
            param_1 = _e98;
            param_2 = hdr_peak_norm;
            let _e99 = tonemapPBRNeutralHDR_u0028_vf3_u003b_f1_u003b((&param_1), (&param_2));
            local_2 = _e99;
        } else {
            let _e100 = (*color_6);
            param_3 = _e100;
            let _e101 = tonemapPBRNeutral_u0028_vf3_u003b((&param_3));
            local_2 = _e101;
        }
        let _e102 = local_2;
        return _e102;
    } else {
        if override_type_7_2 {
            let _e103 = (*color_6);
            param_4 = _e103;
            let _e104 = tonemapAgX_u0028_vf3_u003b((&param_4));
            return _e104;
        } else {
            if override_type_7_3 {
                let _e105 = (*color_6);
                param_5 = _e105;
                let _e106 = tonemapLottes_u0028_vf3_u003b((&param_5));
                return _e106;
            } else {
                if override_type_7_4 {
                    let _e107 = (*color_6);
                    param_6 = _e107;
                    let _e108 = tonemapReinhard_u0028_vf3_u003b((&param_6));
                    return _e108;
                } else {
                    let _e109 = (*color_6);
                    return _e109;
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

    let _e95 = (*uv);
    dir = (_e95 - vec2<f32>(0.5f, 0.5f));
    let _e97 = dir;
    radial = length(_e97);
    let _e99 = dir;
    let _e100 = radial;
    offset_2 = (_e99 * ((chromatic_strength * _e100) * 0.015f));
    let _e104 = (*uv);
    let _e105 = offset_2;
    let _e110 = textureSample(texture0_, texture0_sampler, clamp((_e104 + _e105), vec2(0f), vec2(1f)));
    r = _e110.x;
    let _e112 = (*uv);
    let _e113 = textureSample(texture0_, texture0_sampler, _e112);
    g_2 = _e113.y;
    let _e115 = (*uv);
    let _e116 = offset_2;
    let _e121 = textureSample(texture0_, texture0_sampler, clamp((_e115 - _e116), vec2(0f), vec2(1f)));
    b_1 = _e121.z;
    let _e123 = r;
    let _e124 = g_2;
    let _e125 = b_1;
    return vec3<f32>(_e123, _e124, _e125);
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
    var phi_598_: bool;

    if (chromatic_strength > 0f) {
        let _e98 = frag_tex_coord_1;
        param_7 = _e98;
        let _e99 = sampleChromatic_u0028_vf2_u003b((&param_7));
        local_3 = _e99;
    } else {
        let _e100 = frag_tex_coord_1;
        let _e101 = textureSample(texture0_, texture0_sampler, _e100);
        local_3 = _e101.xyz;
    }
    let _e103 = local_3;
    base = _e103;
    let _e105 = eb.exposure_bias;
    let _e106 = base;
    base = (_e106 * _e105);
    let _e108 = base;
    shadowLuma = dot(max(_e108, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e112 = eb.shadowExponent;
    let _e114 = shadowLuma;
    let _e116 = ((_e112 != 1f) && (_e114 > 0f));
    phi_598_ = _e116;
    if _e116 {
        let _e117 = shadowLuma;
        let _e119 = eb.shadowPivot;
        phi_598_ = (_e117 < _e119);
    }
    let _e122 = phi_598_;
    if _e122 {
        let _e123 = shadowLuma;
        let _e125 = eb.shadowPivot;
        normalized = (_e123 / _e125);
        let _e127 = normalized;
        let _e129 = eb.shadowExponent;
        let _e132 = eb.shadowPivot;
        curved = (pow(_e127, _e129) * _e132);
        let _e134 = curved;
        let _e135 = shadowLuma;
        let _e137 = base;
        base = (_e137 * (_e134 / _e135));
    }
    let _e139 = base;
    param_8 = _e139;
    let _e140 = applyTonemap_u0028_vf3_u003b((&param_8));
    base = _e140;
    let _e141 = base;
    param_9 = _e141;
    let _e142 = applyColorGrading_u0028_vf3_u003b((&param_9));
    base = _e142;
    if (saturation != 1f) {
        let _e144 = base;
        luma_1 = vec3(dot(_e144, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e147 = luma_1;
        let _e148 = base;
        base = mix(_e147, _e148, vec3(saturation));
    }
    let _e151 = base;
    out_color = vec4<f32>(_e151.x, _e151.y, _e151.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
