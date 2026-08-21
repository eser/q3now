struct HistoryConsumePush {
    extent: vec2<u32>,
    frameLo: u32,
    frameHi: u32,
    worldIndex: u32,
    planGeneration: u32,
    allocationGeneration: u32,
    historyValid: u32,
    readIndex: u32,
    writeIndex: u32,
    zNear: f32,
    zFar: f32,
}

struct HistoryWitness {
    words: array<u32, 20>,
}

@group(0) @binding(6)
var<uniform> pc: HistoryConsumePush;
@group(0) @binding(0) 
var currentColor: texture_2d<f32>;
@group(0) @binding(4) 
var nearestSampler: sampler;
@group(0) @binding(1) 
var currentDeviceDepth: texture_2d<f32>;
@group(0) @binding(5) 
var<storage, read_write> witness: HistoryWitness;
@group(0) @binding(2) 
var previousColor: texture_2d<f32>;
@group(0) @binding(3) 
var previousLinearDepth: texture_2d<f32>;

fn linearizeDepth_u0028_f1_u003b(z: ptr<function, f32>) -> f32 {
    let _e34 = pc.zNear;
    let _e36 = pc.zFar;
    let _e39 = pc.zNear;
    let _e40 = (*z);
    let _e42 = pc.zFar;
    let _e44 = pc.zNear;
    return ((_e34 * _e36) / (_e39 + (_e40 * (_e42 - _e44))));
}

fn main_1() {
    var uv: vec2<f32>;
    var current: vec4<f32>;
    var currentDepth: f32;
    var param: f32;
    var previous: vec4<f32>;
    var previousDepth: f32;

    let _e39 = pc.extent;
    let _e46 = pc.extent;
    uv = ((vec2<f32>((_e39 / vec2(2u))) + vec2(0.5f)) / vec2<f32>(_e46));
    let _e49 = uv;
    let _e50 = textureSampleLevel(currentColor, nearestSampler, _e49, 0f);
    current = _e50;
    let _e51 = uv;
    let _e52 = textureSampleLevel(currentDeviceDepth, nearestSampler, _e51, 0f);
    param = _e52.x;
    let _e54 = linearizeDepth_u0028_f1_u003b((&param));
    currentDepth = _e54;
    witness.words[0i] = 1213420593u;
    let _e58 = pc.historyValid;
    witness.words[1i] = _e58;
    let _e62 = pc.frameLo;
    witness.words[2i] = _e62;
    let _e66 = pc.frameHi;
    witness.words[3i] = _e66;
    let _e70 = pc.worldIndex;
    witness.words[4i] = _e70;
    let _e74 = pc.planGeneration;
    witness.words[5i] = _e74;
    let _e78 = pc.allocationGeneration;
    witness.words[6i] = _e78;
    let _e82 = pc.readIndex;
    witness.words[7i] = _e82;
    let _e86 = pc.writeIndex;
    witness.words[8i] = _e86;
    let _e91 = pc.extent[0u];
    witness.words[9i] = _e91;
    let _e96 = pc.extent[1u];
    witness.words[10i] = _e96;
    let _e99 = current;
    witness.words[11i] = pack2x16float(_e99.xy);
    let _e104 = current;
    witness.words[12i] = pack2x16float(_e104.zw);
    let _e109 = currentDepth;
    witness.words[13i] = bitcast<u32>(_e109);
    let _e114 = pc.historyValid;
    if (_e114 != 0u) {
        let _e116 = uv;
        let _e117 = textureSampleLevel(previousColor, nearestSampler, _e116, 0f);
        previous = _e117;
        let _e118 = uv;
        let _e119 = textureSampleLevel(previousLinearDepth, nearestSampler, _e118, 0f);
        previousDepth = _e119.x;
        let _e121 = previous;
        witness.words[14i] = pack2x16float(_e121.xy);
        let _e126 = previous;
        witness.words[15i] = pack2x16float(_e126.zw);
        let _e131 = previousDepth;
        witness.words[16i] = bitcast<u32>(_e131);
    } else {
        witness.words[14i] = 0u;
        witness.words[15i] = 0u;
        witness.words[16i] = 0u;
    }
    return;
}

@compute @workgroup_size(1, 1, 1) 
fn main() {
    main_1();
}
