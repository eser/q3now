enable wgpu_binding_array;

struct EffectsUBO {
    mvp: mat4x4<f32>,
    eyeWorld: vec4<f32>,
    frameParams: vec4<f32>,
    stageParams: vec4<f32>,
}

var<private> fragImageSlot_1: u32;
@group(0) @binding(1) 
var shaderImages: binding_array<texture_2d<f32>, 64>;
@group(0) @binding(33) 
var shaderImages_sampler: binding_array<sampler, 64>;
var<private> fragUV_1: vec2<f32>;
var<private> outColor: vec4<f32>;
var<private> fragColor_1: vec4<f32>;
@group(1) @binding(0) 
var<uniform> unnamed: EffectsUBO;
var<private> fragShaderHandle_1: u32;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e31 = (*c);
    (*c) = max(_e31, vec3<f32>(0f, 0f, 0f));
    let _e33 = (*c);
    cutoff = (_e33 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e35 = (*c);
    lo = (_e35 / vec3(12.92f));
    let _e38 = (*c);
    hi = pow(((_e38 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e43 = hi;
    let _e44 = lo;
    let _e45 = cutoff;
    return mix(_e43, _e44, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e45));
}

fn decodeColorTexel_u0028_vf4_u003b_u1_u003b(c_1: ptr<function, vec4<f32>>, domain: ptr<function, u32>) -> vec4<f32> {
    var param: vec3<f32>;

    let _e30 = (*domain);
    if (_e30 != 0u) {
        let _e32 = (*c_1);
        return _e32;
    }
    let _e33 = (*c_1);
    param = _e33.xyz;
    let _e35 = sRGBToLinear_u0028_vf3_u003b((&param));
    (*c_1)[0u] = _e35.x;
    (*c_1)[1u] = _e35.y;
    (*c_1)[2u] = _e35.z;
    let _e42 = (*c_1);
    return _e42;
}

fn main_1() {
    var domain_1: u32;
    var handle: u32;
    var slot: u32;
    var texel: vec4<f32>;
    var param_1: vec4<f32>;
    var param_2: u32;
    var param_3: vec3<f32>;

    let _e34 = fragImageSlot_1;
    domain_1 = (_e34 >> bitcast<u32>(31u));
    let _e37 = fragImageSlot_1;
    handle = (_e37 & 2147483647u);
    let _e39 = handle;
    let _e41 = handle;
    slot = select(0u, _e41, (_e39 < 64u));
    let _e43 = slot;
    let _e46 = fragUV_1;
    let _e47 = textureSample(shaderImages[_e43], shaderImages_sampler[_e43], _e46);
    param_1 = _e47;
    let _e48 = domain_1;
    param_2 = _e48;
    let _e49 = decodeColorTexel_u0028_vf4_u003b_u1_u003b((&param_1), (&param_2));
    texel = _e49;
    let _e50 = texel;
    let _e52 = fragColor_1;
    param_3 = _e52.xyz;
    let _e54 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e55 = (_e50.xyz * _e54);
    let _e57 = texel[3u];
    let _e59 = fragColor_1[3u];
    outColor = vec4<f32>(_e55.x, _e55.y, _e55.z, (_e57 * _e59));
    return;
}

@fragment 
fn main(@location(3) @interpolate(flat) fragImageSlot: u32, @location(0) fragUV: vec2<f32>, @location(1) fragColor: vec4<f32>, @location(2) @interpolate(flat) fragShaderHandle: u32) -> @location(0) vec4<f32> {
    fragImageSlot_1 = fragImageSlot;
    fragUV_1 = fragUV;
    fragColor_1 = fragColor;
    fragShaderHandle_1 = fragShaderHandle;
    main_1();
    let _e9 = outColor;
    return _e9;
}
