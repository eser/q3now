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
@id(24) override chromatic_strength: f32 = 0f;
@id(2) override saturation: f32 = 1f;
@id(12) override hdr_mode: i32 = 0i;
@id(13) override hdr_peak_norm: f32 = 10f;

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
    let _e36 = colorGradeTint;
    let _e37 = (*color);
    (*color) = (_e37 * _e36);
    let _e39 = (*color);
    luma = dot(_e39, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e41 = luma;
    let _e43 = (*color);
    (*color) = mix(vec3(_e41), _e43, vec3(cg_saturation));
    let _e46 = (*color);
    (*color) = (((_e46 - vec3(0.5f)) * cg_contrast) + vec3(0.5f));
    let _e52 = (*color);
    return clamp(_e52, vec3(0f), vec3(1f));
}

fn sampleChromatic_u0028_vf2_u003b(uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset: vec2<f32>;
    var r: f32;
    var g: f32;
    var b: f32;

    let _e37 = (*uv);
    dir = (_e37 - vec2<f32>(0.5f, 0.5f));
    let _e39 = dir;
    radial = length(_e39);
    let _e41 = dir;
    let _e42 = radial;
    offset = (_e41 * ((chromatic_strength * _e42) * 0.015f));
    let _e46 = (*uv);
    let _e47 = offset;
    let _e52 = textureSample(texture0_, texture0_sampler, clamp((_e46 + _e47), vec2(0f), vec2(1f)));
    r = _e52.x;
    let _e54 = (*uv);
    let _e55 = textureSample(texture0_, texture0_sampler, _e54);
    g = _e55.y;
    let _e57 = (*uv);
    let _e58 = offset;
    let _e63 = textureSample(texture0_, texture0_sampler, clamp((_e57 - _e58), vec2(0f), vec2(1f)));
    b = _e63.z;
    let _e65 = r;
    let _e66 = g;
    let _e67 = b;
    return vec3<f32>(_e65, _e66, _e67);
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
    var phi_160_: bool;

    if (chromatic_strength > 0f) {
        let _e39 = frag_tex_coord_1;
        param = _e39;
        let _e40 = sampleChromatic_u0028_vf2_u003b((&param));
        local = _e40;
    } else {
        let _e41 = frag_tex_coord_1;
        let _e42 = textureSample(texture0_, texture0_sampler, _e41);
        local = _e42.xyz;
    }
    let _e44 = local;
    base = _e44;
    let _e46 = eb.exposure_bias;
    let _e47 = base;
    base = (_e47 * _e46);
    let _e49 = base;
    shadowLuma = dot(max(_e49, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e53 = eb.shadowExponent;
    let _e55 = shadowLuma;
    let _e57 = ((_e53 != 1f) && (_e55 > 0f));
    phi_160_ = _e57;
    if _e57 {
        let _e58 = shadowLuma;
        let _e60 = eb.shadowPivot;
        phi_160_ = (_e58 < _e60);
    }
    let _e63 = phi_160_;
    if _e63 {
        let _e64 = shadowLuma;
        let _e66 = eb.shadowPivot;
        normalized = (_e64 / _e66);
        let _e68 = normalized;
        let _e70 = eb.shadowExponent;
        let _e73 = eb.shadowPivot;
        curved = (pow(_e68, _e70) * _e73);
        let _e75 = curved;
        let _e76 = shadowLuma;
        let _e78 = base;
        base = (_e78 * (_e75 / _e76));
    }
    let _e80 = base;
    param_1 = _e80;
    let _e81 = applyColorGrading_u0028_vf3_u003b((&param_1));
    base = _e81;
    if (saturation != 1f) {
        let _e83 = base;
        luma_1 = vec3(dot(_e83, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e86 = luma_1;
        let _e87 = base;
        base = mix(_e86, _e87, vec3(saturation));
    }
    let _e90 = base;
    out_color = vec4<f32>(_e90.x, _e90.y, _e90.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
