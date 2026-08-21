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

    let _e29 = (*uv);
    dir = (_e29 - vec2<f32>(0.5f, 0.5f));
    let _e31 = dir;
    radial = length(_e31);
    let _e33 = dir;
    let _e34 = radial;
    offset = (_e33 * ((chromatic_strength * _e34) * 0.015f));
    let _e38 = (*uv);
    let _e39 = offset;
    let _e44 = textureSample(texture0_, texture0_sampler, clamp((_e38 + _e39), vec2(0f), vec2(1f)));
    r = _e44.x;
    let _e46 = (*uv);
    let _e47 = textureSample(texture0_, texture0_sampler, _e46);
    g = _e47.y;
    let _e49 = (*uv);
    let _e50 = offset;
    let _e55 = textureSample(texture0_, texture0_sampler, clamp((_e49 - _e50), vec2(0f), vec2(1f)));
    b = _e55.z;
    let _e57 = r;
    let _e58 = g;
    let _e59 = b;
    return vec3<f32>(_e57, _e58, _e59);
}

fn main_1() {
    var base: vec3<f32>;
    var local: vec3<f32>;
    var param: vec2<f32>;
    var luma: vec3<f32>;

    if (chromatic_strength > 0f) {
        let _e27 = frag_tex_coord_1;
        param = _e27;
        let _e28 = sampleChromatic_u0028_vf2_u003b((&param));
        local = _e28;
    } else {
        let _e29 = frag_tex_coord_1;
        let _e30 = textureSample(texture0_, texture0_sampler, _e29);
        local = _e30.xyz;
    }
    let _e32 = local;
    base = _e32;
    let _e34 = eb.exposure_bias;
    let _e35 = base;
    base = (_e35 * _e34);
    if (saturation != 1f) {
        let _e38 = base;
        luma = vec3(dot(_e38, vec3<f32>(0.2126f, 0.7152f, 0.0722f)));
        let _e41 = luma;
        let _e42 = base;
        base = mix(_e41, _e42, vec3(saturation));
    }
    let _e45 = base;
    out_color = vec4<f32>(_e45.x, _e45.y, _e45.z, 1f);
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
