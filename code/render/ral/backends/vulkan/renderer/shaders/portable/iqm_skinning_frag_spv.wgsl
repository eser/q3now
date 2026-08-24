@group(1) @binding(0)
var texture0_: texture_2d<f32>;
@group(1) @binding(32)
var texture0_sampler: sampler;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> frag_normal_1: vec3<f32>;
var<private> frag_tangent_1: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e23 = (*c);
    (*c) = max(_e23, vec3<f32>(0f, 0f, 0f));
    let _e25 = (*c);
    cutoff = (_e25 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e27 = (*c);
    lo = (_e27 / vec3(12.92f));
    let _e30 = (*c);
    hi = pow(((_e30 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e35 = hi;
    let _e36 = lo;
    let _e37 = cutoff;
    return mix(_e35, _e36, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e37));
}

fn main_1() {
    var tex: vec4<f32>;
    var param: vec3<f32>;

    let _e21 = frag_tex_coord_1;
    let _e22 = textureSample(texture0_, texture0_sampler, _e21);
    tex = _e22;
    let _e23 = tex;
    param = _e23.xyz;
    let _e25 = sRGBToLinear_u0028_vf3_u003b((&param));
    let _e27 = tex[3u];
    out_color = vec4<f32>(_e25.x, _e25.y, _e25.z, _e27);
    return;
}

@fragment
fn main(@location(0) frag_tex_coord: vec2<f32>, @location(1) frag_normal: vec3<f32>, @location(2) frag_tangent: vec4<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord_1 = frag_tex_coord;
    frag_normal_1 = frag_normal;
    frag_tangent_1 = frag_tangent;
    main_1();
    let _e7 = out_color;
    return _e7;
}
