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
@group(3) @binding(0)
var aoMap: texture_2d<f32>;
@group(3) @binding(32)
var aoMap_sampler: sampler;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;
@group(2) @binding(0)
var<uniform> eb: ExposureBlock;

fn main_1() {
    var ao: f32;
    var base: vec3<f32>;
    var local: vec3<f32>;
    var param: vec2<f32>;
    var luma: vec3<f32>;

    let _e29 = frag_tex_coord_1;
    let _e30 = textureSample(aoMap, aoMap_sampler, _e29);
    ao = clamp(_e30.x, 0f, 1f);
    let _e33 = ao;
    out_color = vec4<f32>(_e33, _e33, _e33, 1f);
    return;
}

fn sampleChromatic_u0028_vf2_u003b(uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var dir: vec2<f32>;
    var radial: f32;
    var offset: vec2<f32>;
    var r: f32;
    var g: f32;
    var b: f32;

    let _e31 = (*uv);
    dir = (_e31 - vec2<f32>(0.5f, 0.5f));
    let _e33 = dir;
    radial = length(_e33);
    let _e35 = dir;
    let _e36 = radial;
    offset = (_e35 * ((chromatic_strength * _e36) * 0.015f));
    let _e40 = (*uv);
    let _e41 = offset;
    let _e46 = textureSample(texture0_, texture0_sampler, clamp((_e40 + _e41), vec2(0f), vec2(1f)));
    r = _e46.x;
    let _e48 = (*uv);
    let _e49 = textureSample(texture0_, texture0_sampler, _e48);
    g = _e49.y;
    let _e51 = (*uv);
    let _e52 = offset;
    let _e57 = textureSample(texture0_, texture0_sampler, clamp((_e51 - _e52), vec2(0f), vec2(1f)));
    b = _e57.z;
    let _e59 = r;
    let _e60 = g;
    let _e61 = b;
    return vec3<f32>(_e59, _e60, _e61);
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    main_1();
    let _e3 = out_color;
    return _e3;
}
