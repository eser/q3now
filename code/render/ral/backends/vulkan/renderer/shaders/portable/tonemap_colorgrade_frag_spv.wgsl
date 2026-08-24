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
    let _e33 = colorGradeTint;
    let _e34 = (*color);
    (*color) = (_e34 * _e33);
    let _e36 = (*color);
    luma = dot(_e36, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e38 = luma;
    let _e40 = (*color);
    (*color) = mix(vec3(_e38), _e40, vec3(cg_saturation));
    let _e43 = (*color);
    (*color) = (((_e43 - vec3(0.5f)) * cg_contrast) + vec3(0.5f));
    let _e49 = (*color);
    return clamp(_e49, vec3(0f), vec3(1f));
}

fn sampleChromatic_u0028_vf2_u003b(uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset: vec2<f32>;
    var r: f32;
    var g: f32;
    var b: f32;

    let _e34 = (*uv);
    dir = (_e34 - vec2<f32>(0.5f, 0.5f));
    let _e36 = dir;
    radial = length(_e36);
    let _e38 = dir;
    let _e39 = radial;
    offset = (_e38 * ((chromatic_strength * _e39) * 0.015f));
    let _e43 = (*uv);
    let _e44 = offset;
    let _e49 = textureSample(texture0_, texture0_sampler, clamp((_e43 + _e44), vec2(0f), vec2(1f)));
    r = _e49.x;
    let _e51 = (*uv);
    let _e52 = textureSample(texture0_, texture0_sampler, _e51);
    g = _e52.y;
    let _e54 = (*uv);
    let _e55 = offset;
    let _e60 = textureSample(texture0_, texture0_sampler, clamp((_e54 - _e55), vec2(0f), vec2(1f)));
    b = _e60.z;
    let _e62 = r;
    let _e63 = g;
    let _e64 = b;
    return vec3<f32>(_e62, _e63, _e64);
}

fn main_1() {
    var base: vec3<f32>;
    var local: vec3<f32>;
    var param: vec2<f32>;
    var param_1: vec3<f32>;
    var luma_1: vec3<f32>;

    if (chromatic_strength > 0f) {
        let _e33 = frag_tex_coord_1;
        param = _e33;
        let _e34 = sampleChromatic_u0028_vf2_u003b((&param));
        local = _e34;
    } else {
        let _e35 = frag_tex_coord_1;
        let _e36 = textureSample(texture0_, texture0_sampler, _e35);
        local = _e36.xyz;
    }
    let _e38 = local;
    base = _e38;
    let _e40 = eb.exposure_bias;
    let _e41 = base;
    base = (_e41 * _e40);
    let _e43 = base;
    param_1 = _e43;
    let _e44 = applyColorGrading_u0028_vf3_u003b((&param_1));
    base = _e44;
    if (saturation != 1f) {
        let _e46 = base;
        luma_1 = vec3(dot(_e46, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e49 = luma_1;
        let _e50 = base;
        base = mix(_e49, _e50, vec3(saturation));
    }
    let _e53 = base;
    out_color = vec4<f32>(_e53.x, _e53.y, _e53.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
