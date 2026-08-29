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
@id(12) override hdr_mode: i32 = 0i;
@id(13) override hdr_peak_norm: f32 = 10f;

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
    let _e47 = colorGradeTint;
    let _e48 = (*color);
    (*color) = (_e48 * _e47);
    let _e50 = (*color);
    luma = dot(_e50, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e52 = luma;
    let _e54 = (*color);
    (*color) = mix(vec3(_e52), _e54, vec3(cg_saturation));
    let _e57 = (*color);
    (*color) = (((_e57 - vec3(0.5f)) * cg_contrast) + vec3(0.5f));
    let _e63 = (*color);
    return clamp(_e63, vec3(0f), vec3(1f));
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
    var phi_117_: bool;
    var phi_124_: bool;
    var phi_131_: bool;

    let _e51 = frag_tex_coord_1;
    let _e53 = eb.sunScreenX;
    let _e55 = eb.sunScreenY;
    deltaUV = (((_e51 - vec2<f32>(_e53, _e55)) * sunrays_density) / vec2(f32(sunrays_samples)));
    let _e62 = frag_tex_coord_1;
    uv = _e62;
    illumination = vec3<f32>(0f, 0f, 0f);
    weight = 1f;
    i = 0i;
    loop {
        let _e63 = i;
        if (_e63 < sunrays_samples) {
            let _e65 = deltaUV;
            let _e66 = uv;
            uv = (_e66 - _e65);
            let _e69 = uv[0u];
            let _e70 = (_e69 < 0f);
            phi_117_ = _e70;
            if !(_e70) {
                let _e73 = uv[0u];
                phi_117_ = (_e73 > 1f);
            }
            let _e76 = phi_117_;
            phi_124_ = _e76;
            if !(_e76) {
                let _e79 = uv[1u];
                phi_124_ = (_e79 < 0f);
            }
            let _e82 = phi_124_;
            phi_131_ = _e82;
            if !(_e82) {
                let _e85 = uv[1u];
                phi_131_ = (_e85 > 1f);
            }
            let _e88 = phi_131_;
            if _e88 {
                break;
            }
            let _e89 = uv;
            let _e90 = textureSample(depthMap, depthMap_sampler, _e89);
            depth = _e90.x;
            let _e92 = depth;
            isSky = step(_e92, 0.001f);
            let _e94 = uv;
            let _e95 = textureSample(texture0_, texture0_sampler, _e94);
            sceneColor = _e95.xyz;
            threshold[0u] = sunrays_threshold;
            threshold[1u] = sunrays_threshold;
            threshold[2u] = sunrays_threshold;
            let _e100 = sceneColor;
            let _e101 = threshold;
            bright = max((_e100 - _e101), vec3<f32>(0f, 0f, 0f));
            let _e104 = bright;
            let _e105 = isSky;
            let _e107 = weight;
            let _e109 = illumination;
            illumination = (_e109 + ((_e104 * _e105) * _e107));
            let _e112 = eb.sunrayDecay;
            let _e113 = weight;
            weight = (_e113 * _e112);
            continue;
        } else {
            break;
        }
        continuing {
            let _e115 = i;
            i = (_e115 + 1i);
        }
    }
    let _e118 = illumination;
    illumination = (_e118 / vec3(f32(sunrays_samples)));
    let _e121 = illumination;
    let _e123 = eb.sunrayIntensity;
    return (_e121 * _e123);
}

fn sampleChromatic_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset: vec2<f32>;
    var r: f32;
    var g: f32;
    var b: f32;

    let _e48 = (*uv_1);
    dir = (_e48 - vec2<f32>(0.5f, 0.5f));
    let _e50 = dir;
    radial = length(_e50);
    let _e52 = dir;
    let _e53 = radial;
    offset = (_e52 * ((chromatic_strength * _e53) * 0.015f));
    let _e57 = (*uv_1);
    let _e58 = offset;
    let _e63 = textureSample(texture0_, texture0_sampler, clamp((_e57 + _e58), vec2(0f), vec2(1f)));
    r = _e63.x;
    let _e65 = (*uv_1);
    let _e66 = textureSample(texture0_, texture0_sampler, _e65);
    g = _e66.y;
    let _e68 = (*uv_1);
    let _e69 = offset;
    let _e74 = textureSample(texture0_, texture0_sampler, clamp((_e68 - _e69), vec2(0f), vec2(1f)));
    b = _e74.z;
    let _e76 = r;
    let _e77 = g;
    let _e78 = b;
    return vec3<f32>(_e76, _e77, _e78);
}

fn main_1() {
    var base: vec3<f32>;
    var local: vec3<f32>;
    var param: vec2<f32>;
    var shadowLuma: f32;
    var normalized: f32;
    var curved: f32;
    var param_1: vec3<f32>;
    var luma_1: vec3<f32>;
    var phi_273_: bool;

    if (chromatic_strength > 0f) {
        let _e50 = frag_tex_coord_1;
        param = _e50;
        let _e51 = sampleChromatic_u0028_vf2_u003b((&param));
        local = _e51;
    } else {
        let _e52 = frag_tex_coord_1;
        let _e53 = textureSample(texture0_, texture0_sampler, _e52);
        local = _e53.xyz;
    }
    let _e55 = local;
    base = _e55;
    let _e57 = eb.exposure_bias;
    let _e58 = base;
    base = (_e58 * _e57);
    let _e60 = base;
    shadowLuma = dot(max(_e60, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e64 = eb.shadowExponent;
    let _e66 = shadowLuma;
    let _e68 = ((_e64 != 1f) && (_e66 > 0f));
    phi_273_ = _e68;
    if _e68 {
        let _e69 = shadowLuma;
        let _e71 = eb.shadowPivot;
        phi_273_ = (_e69 < _e71);
    }
    let _e74 = phi_273_;
    if _e74 {
        let _e75 = shadowLuma;
        let _e77 = eb.shadowPivot;
        normalized = (_e75 / _e77);
        let _e79 = normalized;
        let _e81 = eb.shadowExponent;
        let _e84 = eb.shadowPivot;
        curved = (pow(_e79, _e81) * _e84);
        let _e86 = curved;
        let _e87 = shadowLuma;
        let _e89 = base;
        base = (_e89 * (_e86 / _e87));
    }
    let _e91 = computeSunRays_u0028_();
    let _e92 = base;
    base = (_e92 + _e91);
    let _e94 = base;
    param_1 = _e94;
    let _e95 = applyColorGrading_u0028_vf3_u003b((&param_1));
    base = _e95;
    if (saturation != 1f) {
        let _e97 = base;
        luma_1 = vec3(dot(_e97, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e100 = luma_1;
        let _e101 = base;
        base = mix(_e100, _e101, vec3(saturation));
    }
    let _e104 = base;
    out_color = vec4<f32>(_e104.x, _e104.y, _e104.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
