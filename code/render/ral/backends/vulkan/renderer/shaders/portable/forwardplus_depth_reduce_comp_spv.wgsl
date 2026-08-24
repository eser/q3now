struct Push {
    params: vec4<f32>,
}

struct TileDepth {
    tileDepth: array<f32>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var<uniform> unnamed: Push;
@group(0) @binding(0)
var sceneDepth: texture_2d<f32>;
@group(0) @binding(1)
var depthSampler: sampler;
@group(0) @binding(2)
var<storage, read_write> unnamed_1: TileDepth;

fn main_1() {
    var tx: u32;
    var ty: u32;
    var tilesX: u32;
    var tilesY: u32;
    var tileIndex: u32;
    var screenW: f32;
    var screenH: f32;
    var x0_: f32;
    var y0_: f32;
    var dMin: f32;
    var dMax: f32;
    var any_: bool;
    var j: u32;
    var i: u32;
    var px: f32;
    var py: f32;
    var uv: vec2<f32>;
    var d: f32;

    let _e38 = gl_GlobalInvocationID_1[0u];
    tx = _e38;
    let _e40 = gl_GlobalInvocationID_1[1u];
    ty = _e40;
    let _e43 = unnamed.params[2u];
    tilesX = u32(_e43);
    let _e47 = unnamed.params[3u];
    tilesY = u32(_e47);
    let _e49 = tx;
    let _e50 = tilesX;
    let _e52 = ty;
    let _e53 = tilesY;
    if ((_e49 >= _e50) || (_e52 >= _e53)) {
        return;
    }
    let _e56 = ty;
    let _e57 = tilesX;
    let _e59 = tx;
    tileIndex = ((_e56 * _e57) + _e59);
    let _e63 = unnamed.params[0u];
    screenW = _e63;
    let _e66 = unnamed.params[1u];
    screenH = _e66;
    let _e67 = tx;
    x0_ = f32((_e67 * 16u));
    let _e70 = ty;
    y0_ = f32((_e70 * 16u));
    dMin = 1f;
    dMax = 0f;
    any_ = false;
    j = 0u;
    loop {
        let _e73 = j;
        if (_e73 < 16u) {
            i = 0u;
            loop {
                let _e75 = i;
                if (_e75 < 16u) {
                    let _e77 = x0_;
                    let _e78 = i;
                    px = (_e77 + f32(_e78));
                    let _e81 = y0_;
                    let _e82 = j;
                    py = (_e81 + f32(_e82));
                    let _e85 = px;
                    let _e86 = screenW;
                    let _e88 = py;
                    let _e89 = screenH;
                    if ((_e85 >= _e86) || (_e88 >= _e89)) {
                        continue;
                    }
                    let _e92 = px;
                    let _e93 = py;
                    let _e97 = screenW;
                    let _e98 = screenH;
                    uv = ((vec2<f32>(_e92, _e93) + vec2(0.5f)) / vec2<f32>(_e97, _e98));
                    let _e101 = uv;
                    let _e102 = textureSampleLevel(sceneDepth, depthSampler, _e101, 0f);
                    d = _e102.x;
                    let _e104 = d;
                    if (_e104 <= 0f) {
                        continue;
                    }
                    let _e106 = dMin;
                    let _e107 = d;
                    dMin = min(_e106, _e107);
                    let _e109 = dMax;
                    let _e110 = d;
                    dMax = max(_e109, _e110);
                    any_ = true;
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e112 = i;
                    i = (_e112 + bitcast<u32>(1i));
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e115 = j;
            j = (_e115 + bitcast<u32>(1i));
        }
    }
    let _e118 = any_;
    if !(_e118) {
        dMin = 0f;
        dMax = 1f;
    }
    let _e120 = tileIndex;
    let _e123 = dMin;
    unnamed_1.tileDepth[((2u * _e120) + 0u)] = _e123;
    let _e126 = tileIndex;
    let _e129 = dMax;
    unnamed_1.tileDepth[((2u * _e126) + 1u)] = _e129;
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
