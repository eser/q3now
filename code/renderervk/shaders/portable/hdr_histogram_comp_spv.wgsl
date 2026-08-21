struct Dims {
    imageSize: vec2<u32>,
}

struct Histogram {
    bins: array<u32, 256>,
}

struct Histogram_1 {
    bins: array<atomic<u32>, 256>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var<uniform> unnamed: Dims;
@group(0) @binding(0) 
var colorImage: texture_2d<f32>;
@group(0) @binding(1) 
var colorSampler: sampler;
@group(0) @binding(2) 
var<storage, read_write> unnamed_1: Histogram_1;

fn main_1() {
    var coord: vec2<u32>;
    var hdr: vec3<f32>;
    var luma: f32;
    var uv: vec2<f32>;
    var fromC: vec2<f32>;
    var dist2_: f32;
    var weight: f32;
    var bin: u32;
    var ev: f32;
    var norm: f32;
    var fixedWeight: u32;
    var phi_38_: bool;

    let _e36 = gl_GlobalInvocationID_1;
    coord = _e36.xy;
    let _e39 = coord[0u];
    let _e42 = unnamed.imageSize[0u];
    let _e43 = (_e39 >= _e42);
    phi_38_ = _e43;
    if !(_e43) {
        let _e46 = coord[1u];
        let _e49 = unnamed.imageSize[1u];
        phi_38_ = (_e46 >= _e49);
    }
    let _e52 = phi_38_;
    if _e52 {
        return;
    }
    let _e53 = coord;
    let _e55 = textureLoad(colorImage, bitcast<vec2<i32>>(_e53), 0i);
    hdr = _e55.xyz;
    let _e57 = hdr;
    luma = dot(_e57, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e59 = coord;
    let _e64 = unnamed.imageSize;
    uv = ((vec2<f32>(_e59) + vec2(0.5f)) / vec2<f32>(_e64));
    let _e67 = uv;
    fromC = (_e67 - vec2<f32>(0.5f, 0.5f));
    let _e69 = fromC;
    let _e70 = fromC;
    dist2_ = dot(_e69, _e70);
    let _e72 = dist2_;
    weight = clamp((1f - (2f * _e72)), 0f, 1f);
    let _e76 = luma;
    if (_e76 <= 0.00001f) {
        bin = 0u;
    } else {
        let _e78 = luma;
        ev = log2(_e78);
        let _e80 = ev;
        norm = clamp(((_e80 - -10f) / 12f), 0f, 1f);
        let _e84 = norm;
        bin = u32(((_e84 * 255f) + 0.5f));
    }
    let _e88 = weight;
    fixedWeight = u32(((_e88 * 1024f) + 0.5f));
    let _e92 = fixedWeight;
    if (_e92 > 0u) {
        let _e94 = bin;
        let _e97 = fixedWeight;
        let _e98 = atomicAdd((&unnamed_1.bins[_e94]), _e97);
    }
    return;
}

@compute @workgroup_size(16, 16, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
