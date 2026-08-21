struct FragmentOutput {
    @location(0) member: vec4<f32>,
    @location(1) member_1: vec2<f32>,
    @location(2) member_2: f32,
}

@group(1) @binding(0) 
var texture0_: texture_2d<f32>;
@group(1) @binding(32) 
var texture0_sampler: sampler;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
var<private> frag_normal_1: vec3<f32>;
var<private> frag_tangent_1: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e26 = (*c);
    (*c) = max(_e26, vec3<f32>(0f, 0f, 0f));
    let _e28 = (*c);
    cutoff = (_e28 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e30 = (*c);
    lo = (_e30 / vec3(12.92f));
    let _e33 = (*c);
    hi = pow(((_e33 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e38 = hi;
    let _e39 = lo;
    let _e40 = cutoff;
    return mix(_e38, _e39, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e40));
}

fn main_1() {
    var tex: vec4<f32>;
    var param: vec3<f32>;

    let _e24 = frag_tex_coord_1;
    let _e25 = textureSample(texture0_, texture0_sampler, _e24);
    tex = _e25;
    let _e26 = tex;
    param = _e26.xyz;
    let _e28 = sRGBToLinear_u0028_vf3_u003b((&param));
    let _e30 = tex[3u];
    out_color = vec4<f32>(_e28.x, _e28.y, _e28.z, _e30);
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

@fragment 
fn main(@location(0) frag_tex_coord: vec2<f32>, @location(1) frag_normal: vec3<f32>, @location(2) frag_tangent: vec4<f32>) -> FragmentOutput {
    frag_tex_coord_1 = frag_tex_coord;
    frag_normal_1 = frag_normal;
    frag_tangent_1 = frag_tangent;
    main_1();
    let _e9 = out_color;
    let _e10 = out_temporal_velocity;
    let _e11 = out_temporal_validity;
    return FragmentOutput(_e9, _e10, _e11);
}
