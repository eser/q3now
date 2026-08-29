struct Params {
    outputGrid: vec4<u32>,
    depthRange: vec4<f32>,
}

struct Integrated {
    media: array<vec4<f32>>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(0)
var<uniform> p: Params;
@group(0) @binding(2)
var sceneDepth: texture_2d<f32>;
@group(0) @binding(34)
var sceneDepth_sampler: sampler;
@group(0) @binding(3)
var<storage> unnamed: Integrated;
@group(0) @binding(1)
var sceneHdr: texture_2d<f32>;
@group(0) @binding(33)
var sceneHdr_sampler: sampler;
@group(0) @binding(4)
var composedHdr: texture_storage_2d<rgba16float,write>;

fn main_1() {
    var pixel: vec2<u32>;
    var uv: vec2<f32>;
    var depth: f32;
    var local: f32;
    var linearFraction: f32;
    var local_1: f32;
    var froxelXY: vec2<u32>;
    var slices: u32;
    var z: u32;
    var index: u32;
    var atmosphere: vec4<f32>;
    var scene: vec3<f32>;

    let _e31 = gl_GlobalInvocationID_1;
    pixel = _e31.xy;
    let _e33 = pixel;
    let _e35 = p.outputGrid;
    if any((_e33 >= _e35.xy)) {
        return;
    }
    let _e39 = pixel;
    let _e44 = p.outputGrid;
    uv = ((vec2<f32>(_e39) + vec2(0.5f)) / vec2<f32>(_e44.xy));
    let _e50 = p.depthRange[2u];
    if (_e50 > 0.5f) {
        let _e52 = uv;
        let _e53 = textureSampleLevel(sceneDepth, sceneDepth_sampler, _e52, 0f);
        local = _e53.x;
    } else {
        local = 0f;
    }
    let _e55 = local;
    depth = _e55;
    let _e56 = depth;
    if (_e56 <= 0f) {
        local_1 = 1f;
    } else {
        let _e58 = depth;
        local_1 = clamp((1f - _e58), 0f, 1f);
    }
    let _e61 = local_1;
    linearFraction = _e61;
    let _e62 = uv;
    let _e64 = p.outputGrid;
    let _e70 = p.outputGrid;
    froxelXY = min(vec2<u32>((_e62 * vec2<f32>(_e64.zw))), (_e70.zw - vec2(1u)));
    let _e77 = p.depthRange[3u];
    slices = u32(max(_e77, 1f));
    let _e80 = linearFraction;
    let _e81 = slices;
    let _e85 = slices;
    z = min(u32((_e80 * f32(_e81))), (_e85 - 1u));
    let _e88 = z;
    let _e91 = p.outputGrid[3u];
    let _e94 = froxelXY[1u];
    let _e98 = p.outputGrid[2u];
    let _e101 = froxelXY[0u];
    index = ((((_e88 * _e91) + _e94) * _e98) + _e101);
    let _e103 = index;
    let _e106 = unnamed.media[_e103];
    atmosphere = _e106;
    let _e107 = uv;
    let _e108 = textureSampleLevel(sceneHdr, sceneHdr_sampler, _e107, 0f);
    scene = _e108.xyz;
    let _e110 = pixel;
    let _e112 = scene;
    let _e114 = atmosphere[3u];
    let _e116 = atmosphere;
    let _e118 = ((_e112 * _e114) + _e116.xyz);
    textureStore(composedHdr, bitcast<vec2<i32>>(_e110), vec4<f32>(_e118.x, _e118.y, _e118.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
