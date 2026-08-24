struct ProductDraw {
    viewOriginDrawSpace: vec4<f32>,
    viewForward: vec4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    projectionLightmap: vec4<f32>,
    viewportAlpha: vec4<f32>,
}

@group(1) @binding(0)
var baseTexture: texture_2d<f32>;
@group(1) @binding(1)
var baseSampler: sampler;
var<private> texCoord_1: vec2<f32>;
@group(0) @binding(0)
var<uniform> productDraw: ProductDraw;
@group(1) @binding(2)
var lightmapTexture: texture_2d<f32>;
@group(1) @binding(3)
var lightmapSampler: sampler;
var<private> lightmapCoord_1: vec2<f32>;
var<private> outColor: vec4<f32>;
var<private> color_1: vec4<f32>;

fn main_1() {
    var base: vec4<f32>;
    var alphaMode: i32;
    var lighting: vec3<f32>;
    var local: vec3<f32>;
    var phi_53_: bool;

    let _e22 = texCoord_1;
    let _e23 = textureSample(baseTexture, baseSampler, _e22);
    base = _e23;
    let _e26 = productDraw.viewportAlpha[2u];
    alphaMode = i32((_e26 + 0.5f));
    let _e29 = alphaMode;
    let _e30 = (_e29 == 1i);
    phi_53_ = _e30;
    if _e30 {
        let _e32 = base[3u];
        let _e35 = productDraw.viewportAlpha[3u];
        phi_53_ = (_e32 < _e35);
    }
    let _e38 = phi_53_;
    if _e38 {
        discard;
    }
    let _e41 = productDraw.projectionLightmap[3u];
    if (_e41 >= 0.5f) {
        let _e43 = lightmapCoord_1;
        let _e44 = textureSample(lightmapTexture, lightmapSampler, _e43);
        local = min((_e44.xyz * 2f), vec3<f32>(1f, 1f, 1f));
    } else {
        local = vec3<f32>(1f, 1f, 1f);
    }
    let _e48 = local;
    lighting = _e48;
    let _e49 = base;
    let _e51 = lighting;
    let _e53 = color_1;
    let _e55 = ((_e49.xyz * _e51) * _e53.xyz);
    let _e57 = base[3u];
    let _e59 = color_1[3u];
    outColor = vec4<f32>(_e55.x, _e55.y, _e55.z, (_e57 * _e59));
    return;
}

@fragment
fn main(@location(0) texCoord: vec2<f32>, @location(1) lightmapCoord: vec2<f32>, @location(2) color: vec4<f32>) -> @location(0) vec4<f32> {
    texCoord_1 = texCoord;
    lightmapCoord_1 = lightmapCoord;
    color_1 = color;
    main_1();
    let _e7 = outColor;
    return _e7;
}
