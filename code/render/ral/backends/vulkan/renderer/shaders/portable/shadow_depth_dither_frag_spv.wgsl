var<private> gl_FragCoord_1: vec4<f32>;
var<private> in_opacity_1: f32;

fn main_1() {
    var n: f32;

    let _e7 = gl_FragCoord_1;
    n = fract((52.982918f * fract(dot(_e7.xy, vec2<f32>(0.06711056f, 0.00583715f)))));
    let _e13 = n;
    let _e14 = in_opacity_1;
    if (_e13 >= _e14) {
        discard;
    }
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) @interpolate(flat) in_opacity: f32) {
    gl_FragCoord_1 = gl_FragCoord;
    in_opacity_1 = in_opacity;
    main_1();
}
