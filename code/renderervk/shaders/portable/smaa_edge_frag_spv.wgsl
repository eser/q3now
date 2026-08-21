struct RtMetrics {
    rtMetrics: vec4<f32>,
}

@id(0) override SMAA_THRESHOLD: f32 = 0.1f;

@group(0) @binding(0) 
var colorTex: texture_2d<f32>;
@group(0) @binding(32) 
var colorTex_sampler: sampler;
var<private> texcoord_1: vec2<f32>;
var<private> offset0_1: vec4<f32>;
var<private> offset1_1: vec4<f32>;
var<private> offset2_1: vec4<f32>;
var<private> out_edges: vec2<f32>;
@group(3) @binding(0) 
var<uniform> unnamed: RtMetrics;

fn main_1() {
    var threshold: vec2<f32>;
    var L: f32;
    var Lleft: f32;
    var Ltop: f32;
    var delta: vec4<f32>;
    var relDelta: vec2<f32>;
    var edges: vec2<f32>;
    var Lright: f32;
    var Lbottom: f32;
    var maxDelta: vec2<f32>;
    var Lleftleft: f32;
    var Ltoptop: f32;
    var finalDelta: f32;

    threshold[0u] = SMAA_THRESHOLD;
    threshold[1u] = SMAA_THRESHOLD;
    let _e37 = texcoord_1;
    let _e38 = textureSample(colorTex, colorTex_sampler, _e37);
    L = dot(_e38.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e41 = offset0_1;
    let _e43 = textureSample(colorTex, colorTex_sampler, _e41.xy);
    Lleft = dot(_e43.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e46 = offset0_1;
    let _e48 = textureSample(colorTex, colorTex_sampler, _e46.zw);
    Ltop = dot(_e48.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e51 = L;
    let _e52 = Lleft;
    let _e53 = Ltop;
    let _e57 = abs((vec2(_e51) - vec2<f32>(_e52, _e53)));
    delta[0u] = _e57.x;
    delta[1u] = _e57.y;
    let _e62 = delta;
    let _e64 = L;
    relDelta = (_e62.xy / vec2(max(_e64, 0.0001f)));
    let _e68 = threshold;
    let _e69 = relDelta;
    edges = step(_e68, _e69);
    let _e71 = edges;
    if (dot(_e71, vec2<f32>(1f, 1f)) == 0f) {
        discard;
    }
    let _e74 = offset1_1;
    let _e76 = textureSample(colorTex, colorTex_sampler, _e74.xy);
    Lright = dot(_e76.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e79 = offset1_1;
    let _e81 = textureSample(colorTex, colorTex_sampler, _e79.zw);
    Lbottom = dot(_e81.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e84 = L;
    let _e85 = Lright;
    let _e86 = Lbottom;
    let _e90 = abs((vec2(_e84) - vec2<f32>(_e85, _e86)));
    delta[2u] = _e90.x;
    delta[3u] = _e90.y;
    let _e95 = delta;
    let _e97 = delta;
    maxDelta = max(_e95.xy, _e97.zw);
    let _e100 = offset2_1;
    let _e102 = textureSample(colorTex, colorTex_sampler, _e100.xy);
    Lleftleft = dot(_e102.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e105 = offset2_1;
    let _e107 = textureSample(colorTex, colorTex_sampler, _e105.zw);
    Ltoptop = dot(_e107.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e110 = Lleft;
    let _e111 = Ltop;
    let _e113 = Lleftleft;
    let _e114 = Ltoptop;
    let _e117 = abs((vec2<f32>(_e110, _e111) - vec2<f32>(_e113, _e114)));
    delta[2u] = _e117.x;
    delta[3u] = _e117.y;
    let _e122 = maxDelta;
    let _e123 = delta;
    maxDelta = max(_e122, _e123.zw);
    let _e127 = maxDelta[0u];
    let _e129 = maxDelta[1u];
    finalDelta = max(_e127, _e129);
    let _e131 = finalDelta;
    let _e132 = delta;
    let _e137 = edges;
    edges = (_e137 * step(vec2(_e131), (_e132.xy * 2f)));
    let _e139 = edges;
    out_edges = _e139;
    return;
}

@fragment 
fn main(@location(0) texcoord: vec2<f32>, @location(1) offset0_: vec4<f32>, @location(2) offset1_: vec4<f32>, @location(3) offset2_: vec4<f32>) -> @location(0) vec2<f32> {
    texcoord_1 = texcoord;
    offset0_1 = offset0_;
    offset1_1 = offset1_;
    offset2_1 = offset2_;
    main_1();
    let _e9 = out_edges;
    return _e9;
}
