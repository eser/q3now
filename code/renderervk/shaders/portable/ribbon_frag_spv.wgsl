enable wgpu_binding_array;

struct EffectsUBO {
    mvp: mat4x4<f32>,
    eyeWorld: vec4<f32>,
    frameParams: vec4<f32>,
    _v2_: vec4<f32>,
}

var<private> fragShaderHandle_1: u32;
@group(0) @binding(2) 
var shaderImages: binding_array<texture_2d<f32>, 64>;
@group(0) @binding(34) 
var shaderImages_sampler: binding_array<sampler, 64>;
var<private> fragUV_1: vec2<f32>;
var<private> outColor: vec4<f32>;
var<private> fragColor_1: vec4<f32>;
@group(1) @binding(0) 
var<uniform> unnamed: EffectsUBO;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e30 = (*c);
    (*c) = max(_e30, vec3<f32>(0f, 0f, 0f));
    let _e32 = (*c);
    cutoff = (_e32 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e34 = (*c);
    lo = (_e34 / vec3(12.92f));
    let _e37 = (*c);
    hi = pow(((_e37 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e42 = hi;
    let _e43 = lo;
    let _e44 = cutoff;
    return mix(_e42, _e43, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e44));
}

fn sampleColorTexBindless_u0028_vf4_u003b_u1_u003b(sampled: ptr<function, vec4<f32>>, domain: ptr<function, u32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e30 = (*sampled);
    c_1 = _e30;
    let _e31 = (*domain);
    if (_e31 != 0u) {
        let _e33 = c_1;
        return _e33;
    }
    let _e34 = c_1;
    param = _e34.xyz;
    let _e36 = sRGBToLinear_u0028_vf3_u003b((&param));
    c_1[0u] = _e36.x;
    c_1[1u] = _e36.y;
    c_1[2u] = _e36.z;
    let _e43 = c_1;
    return _e43;
}

fn main_1() {
    var domain_1: u32;
    var handle: u32;
    var slot: u32;
    var texel: vec4<f32>;
    var param_1: vec4<f32>;
    var param_2: u32;
    var param_3: vec3<f32>;

    let _e33 = fragShaderHandle_1;
    domain_1 = (_e33 >> bitcast<u32>(31u));
    let _e36 = fragShaderHandle_1;
    handle = (_e36 & 2147483647u);
    let _e38 = handle;
    let _e40 = handle;
    slot = select(0u, _e40, (_e38 < 64u));
    let _e42 = slot;
    let _e45 = fragUV_1;
    let _e46 = textureSample(shaderImages[_e42], shaderImages_sampler[_e42], _e45);
    param_1 = _e46;
    let _e47 = domain_1;
    param_2 = _e47;
    let _e48 = sampleColorTexBindless_u0028_vf4_u003b_u1_u003b((&param_1), (&param_2));
    texel = _e48;
    let _e49 = texel;
    let _e51 = fragColor_1;
    param_3 = _e51.xyz;
    let _e53 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e54 = (_e49.xyz * _e53);
    let _e56 = texel[3u];
    let _e58 = fragColor_1[3u];
    outColor = vec4<f32>(_e54.x, _e54.y, _e54.z, (_e56 * _e58));
    return;
}

@fragment 
fn main(@location(2) @interpolate(flat) fragShaderHandle: u32, @location(0) fragUV: vec2<f32>, @location(1) fragColor: vec4<f32>) -> @location(0) vec4<f32> {
    fragShaderHandle_1 = fragShaderHandle;
    fragUV_1 = fragUV;
    fragColor_1 = fragColor;
    main_1();
    let _e7 = outColor;
    return _e7;
}
