struct EffectsUBO {
    mvp: mat4x4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    frameParams: vec4<f32>,
}

var<private> outColor: vec4<f32>;
var<private> fragColor_1: vec4<f32>;
@group(1) @binding(0)
var<uniform> unnamed: EffectsUBO;
var<private> fragUV_1: vec2<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e21 = (*c);
    (*c) = max(_e21, vec3<f32>(0f, 0f, 0f));
    let _e23 = (*c);
    cutoff = (_e23 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e25 = (*c);
    lo = (_e25 / vec3(12.92f));
    let _e28 = (*c);
    hi = pow(((_e28 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e33 = hi;
    let _e34 = lo;
    let _e35 = cutoff;
    return mix(_e33, _e34, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e35));
}

fn main_1() {
    var param: vec3<f32>;

    let _e18 = fragColor_1;
    param = _e18.xyz;
    let _e20 = sRGBToLinear_u0028_vf3_u003b((&param));
    let _e22 = fragColor_1[3u];
    outColor = vec4<f32>(_e20.x, _e20.y, _e20.z, _e22);
    return;
}

@fragment
fn main(@location(1) fragColor: vec4<f32>, @location(0) fragUV: vec2<f32>) -> @location(0) vec4<f32> {
    fragColor_1 = fragColor;
    fragUV_1 = fragUV;
    main_1();
    let _e5 = outColor;
    return _e5;
}
