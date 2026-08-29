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

fn sampleChromatic_u0028_vf2_u003b(uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset: vec2<f32>;
    var r: f32;
    var g: f32;
    var b: f32;

    let _e32 = (*uv);
    dir = (_e32 - vec2<f32>(0.5f, 0.5f));
    let _e34 = dir;
    radial = length(_e34);
    let _e36 = dir;
    let _e37 = radial;
    offset = (_e36 * ((chromatic_strength * _e37) * 0.015f));
    let _e41 = (*uv);
    let _e42 = offset;
    let _e47 = textureSample(texture0_, texture0_sampler, clamp((_e41 + _e42), vec2(0f), vec2(1f)));
    r = _e47.x;
    let _e49 = (*uv);
    let _e50 = textureSample(texture0_, texture0_sampler, _e49);
    g = _e50.y;
    let _e52 = (*uv);
    let _e53 = offset;
    let _e58 = textureSample(texture0_, texture0_sampler, clamp((_e52 - _e53), vec2(0f), vec2(1f)));
    b = _e58.z;
    let _e60 = r;
    let _e61 = g;
    let _e62 = b;
    return vec3<f32>(_e60, _e61, _e62);
}

fn main_1() {
    var base: vec3<f32>;
    var local: vec3<f32>;
    var param: vec2<f32>;
    var shadowLuma: f32;
    var normalized: f32;
    var curved: f32;
    var luma: vec3<f32>;
    var phi_124_: bool;

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
    shadowLuma = dot(max(_e43, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e47 = eb.shadowExponent;
    let _e49 = shadowLuma;
    let _e51 = ((_e47 != 1f) && (_e49 > 0f));
    phi_124_ = _e51;
    if _e51 {
        let _e52 = shadowLuma;
        let _e54 = eb.shadowPivot;
        phi_124_ = (_e52 < _e54);
    }
    let _e57 = phi_124_;
    if _e57 {
        let _e58 = shadowLuma;
        let _e60 = eb.shadowPivot;
        normalized = (_e58 / _e60);
        let _e62 = normalized;
        let _e64 = eb.shadowExponent;
        let _e67 = eb.shadowPivot;
        curved = (pow(_e62, _e64) * _e67);
        let _e69 = curved;
        let _e70 = shadowLuma;
        let _e72 = base;
        base = (_e72 * (_e69 / _e70));
    }
    if (saturation != 1f) {
        let _e75 = base;
        luma = vec3(dot(_e75, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e78 = luma;
        let _e79 = base;
        base = mix(_e78, _e79, vec3(saturation));
    }
    let _e82 = base;
    out_color = vec4<f32>(_e82.x, _e82.y, _e82.z, 1f);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
