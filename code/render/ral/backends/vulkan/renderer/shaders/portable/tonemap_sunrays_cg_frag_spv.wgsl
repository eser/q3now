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
    let _e45 = colorGradeTint;
    let _e46 = (*color);
    (*color) = (_e46 * _e45);
    let _e48 = (*color);
    luma = dot(_e48, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e50 = luma;
    let _e52 = (*color);
    (*color) = mix(vec3(_e50), _e52, vec3(cg_saturation));
    let _e55 = (*color);
    (*color) = (((_e55 - vec3(0.5f)) * cg_contrast) + vec3(0.5f));
    let _e61 = (*color);
    return clamp(_e61, vec3(0f), vec3(1f));
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

    let _e49 = frag_tex_coord_1;
    let _e51 = eb.sunScreenX;
    let _e53 = eb.sunScreenY;
    deltaUV = (((_e49 - vec2<f32>(_e51, _e53)) * sunrays_density) / vec2(f32(sunrays_samples)));
    let _e60 = frag_tex_coord_1;
    uv = _e60;
    illumination = vec3<f32>(0f, 0f, 0f);
    weight = 1f;
    i = 0i;
    loop {
        let _e61 = i;
        if (_e61 < sunrays_samples) {
            let _e63 = deltaUV;
            let _e64 = uv;
            uv = (_e64 - _e63);
            let _e67 = uv[0u];
            let _e68 = (_e67 < 0f);
            phi_117_ = _e68;
            if !(_e68) {
                let _e71 = uv[0u];
                phi_117_ = (_e71 > 1f);
            }
            let _e74 = phi_117_;
            phi_124_ = _e74;
            if !(_e74) {
                let _e77 = uv[1u];
                phi_124_ = (_e77 < 0f);
            }
            let _e80 = phi_124_;
            phi_131_ = _e80;
            if !(_e80) {
                let _e83 = uv[1u];
                phi_131_ = (_e83 > 1f);
            }
            let _e86 = phi_131_;
            if _e86 {
                break;
            }
            let _e87 = uv;
            let _e88 = textureSample(depthMap, depthMap_sampler, _e87);
            depth = _e88.x;
            let _e90 = depth;
            isSky = step(_e90, 0.001f);
            let _e92 = uv;
            let _e93 = textureSample(texture0_, texture0_sampler, _e92);
            sceneColor = _e93.xyz;
            threshold[0u] = sunrays_threshold;
            threshold[1u] = sunrays_threshold;
            threshold[2u] = sunrays_threshold;
            let _e98 = sceneColor;
            let _e99 = threshold;
            bright = max((_e98 - _e99), vec3<f32>(0f, 0f, 0f));
            let _e102 = bright;
            let _e103 = isSky;
            let _e105 = weight;
            let _e107 = illumination;
            illumination = (_e107 + ((_e102 * _e103) * _e105));
            let _e110 = eb.sunrayDecay;
            let _e111 = weight;
            weight = (_e111 * _e110);
            continue;
        } else {
            break;
        }
        continuing {
            let _e113 = i;
            i = (_e113 + 1i);
        }
    }
    let _e116 = illumination;
    illumination = (_e116 / vec3(f32(sunrays_samples)));
    let _e119 = illumination;
    let _e121 = eb.sunrayIntensity;
    return (_e119 * _e121);
}

fn sampleChromatic_u0028_vf2_u003b(uv_1: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset: vec2<f32>;
    var r: f32;
    var g: f32;
    var b: f32;

    let _e46 = (*uv_1);
    dir = (_e46 - vec2<f32>(0.5f, 0.5f));
    let _e48 = dir;
    radial = length(_e48);
    let _e50 = dir;
    let _e51 = radial;
    offset = (_e50 * ((chromatic_strength * _e51) * 0.015f));
    let _e55 = (*uv_1);
    let _e56 = offset;
    let _e61 = textureSample(texture0_, texture0_sampler, clamp((_e55 + _e56), vec2(0f), vec2(1f)));
    r = _e61.x;
    let _e63 = (*uv_1);
    let _e64 = textureSample(texture0_, texture0_sampler, _e63);
    g = _e64.y;
    let _e66 = (*uv_1);
    let _e67 = offset;
    let _e72 = textureSample(texture0_, texture0_sampler, clamp((_e66 - _e67), vec2(0f), vec2(1f)));
    b = _e72.z;
    let _e74 = r;
    let _e75 = g;
    let _e76 = b;
    return vec3<f32>(_e74, _e75, _e76);
}

fn main_1() {
    var base: vec3<f32>;
    var local: vec3<f32>;
    var param: vec2<f32>;
    var param_1: vec3<f32>;
    var luma_1: vec3<f32>;

    if (chromatic_strength > 0f) {
        let _e45 = frag_tex_coord_1;
        param = _e45;
        let _e46 = sampleChromatic_u0028_vf2_u003b((&param));
        local = _e46;
    } else {
        let _e47 = frag_tex_coord_1;
        let _e48 = textureSample(texture0_, texture0_sampler, _e47);
        local = _e48.xyz;
    }
    let _e50 = local;
    base = _e50;
    let _e52 = eb.exposure_bias;
    let _e53 = base;
    base = (_e53 * _e52);
    let _e55 = computeSunRays_u0028_();
    let _e56 = base;
    base = (_e56 + _e55);
    let _e58 = base;
    param_1 = _e58;
    let _e59 = applyColorGrading_u0028_vf3_u003b((&param_1));
    base = _e59;
    if (saturation != 1f) {
        let _e61 = base;
        luma_1 = vec3(dot(_e61, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e64 = luma_1;
        let _e65 = base;
        base = mix(_e64, _e65, vec3(saturation));
    }
    let _e68 = base;
    out_color = vec4<f32>(_e68.x, _e68.y, _e68.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
