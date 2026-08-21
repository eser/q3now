struct RtMetrics {
    rtMetrics: vec4<f32>,
}

@group(1) @binding(0) 
var blendTex: texture_2d<f32>;
@group(1) @binding(32) 
var blendTex_sampler: sampler;
var<private> offset_1: vec4<f32>;
var<private> texcoord_1: vec2<f32>;
var<private> out_color: vec4<f32>;
@group(0) @binding(0) 
var colorTex: texture_2d<f32>;
@group(0) @binding(32) 
var colorTex_sampler: sampler;
@group(3) @binding(0) 
var<uniform> unnamed: RtMetrics;

fn main_1() {
    var a: vec4<f32>;
    var h: bool;
    var blendingOffset: vec4<f32>;
    var blendingWeight: vec2<f32>;
    var blendingCoord: vec4<f32>;

    let _e23 = offset_1;
    let _e25 = textureSample(blendTex, blendTex_sampler, _e23.xy);
    a[0u] = _e25.w;
    let _e28 = offset_1;
    let _e30 = textureSample(blendTex, blendTex_sampler, _e28.zw);
    a[1u] = _e30.y;
    let _e33 = texcoord_1;
    let _e34 = textureSample(blendTex, blendTex_sampler, _e33);
    let _e35 = _e34.xz;
    a[3u] = _e35.x;
    a[2u] = _e35.y;
    let _e40 = a;
    if (dot(_e40, vec4<f32>(1f, 1f, 1f, 1f)) < 0.00001f) {
        let _e43 = texcoord_1;
        let _e44 = textureSampleLevel(colorTex, colorTex_sampler, _e43, 0f);
        out_color = _e44;
        return;
    }
    let _e46 = a[0u];
    let _e48 = a[2u];
    let _e51 = a[1u];
    let _e53 = a[3u];
    h = (max(_e46, _e48) > max(_e51, _e53));
    let _e57 = a[1u];
    let _e59 = a[3u];
    blendingOffset = vec4<f32>(0f, _e57, 0f, _e59);
    let _e61 = a;
    blendingWeight = _e61.yw;
    let _e63 = h;
    if _e63 {
        let _e65 = a[0u];
        let _e67 = a[2u];
        blendingOffset = vec4<f32>(_e65, 0f, _e67, 0f);
        let _e69 = a;
        blendingWeight = _e69.xz;
    }
    let _e71 = blendingWeight;
    let _e73 = blendingWeight;
    blendingWeight = (_e73 / vec2(dot(_e71, vec2<f32>(1f, 1f))));
    let _e76 = blendingOffset;
    let _e78 = unnamed.rtMetrics;
    let _e79 = _e78.xy;
    let _e81 = unnamed.rtMetrics;
    let _e83 = -(_e81.xy);
    let _e90 = texcoord_1;
    blendingCoord = ((_e76 * vec4<f32>(_e79.x, _e79.y, _e83.x, _e83.y)) + _e90.xyxy);
    let _e94 = blendingWeight[0u];
    let _e95 = blendingCoord;
    let _e97 = textureSampleLevel(colorTex, colorTex_sampler, _e95.xy, 0f);
    out_color = (_e97 * _e94);
    let _e100 = blendingWeight[1u];
    let _e101 = blendingCoord;
    let _e103 = textureSampleLevel(colorTex, colorTex_sampler, _e101.zw, 0f);
    let _e105 = out_color;
    out_color = (_e105 + (_e103 * _e100));
    return;
}

@fragment 
fn main(@location(1) offset: vec4<f32>, @location(0) texcoord: vec2<f32>) -> @location(0) vec4<f32> {
    offset_1 = offset;
    texcoord_1 = texcoord;
    main_1();
    let _e5 = out_color;
    return _e5;
}
