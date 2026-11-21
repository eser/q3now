struct Push {
    invViewProj: mat4x4<f32>,
    params: vec4<f32>,
    tune: vec4<f32>,
}

struct Counters {
    counters: array<u32, 16>,
}

struct Reached {
    reached: array<u32>,
}

struct SurfAABB {
    mins: vec4<f32>,
    maxs: vec4<f32>,
    plane: vec4<f32>,
    meta_: vec4<u32>,
}

struct Aabb {
    surfaces: array<SurfAABB>,
}

struct OccludedBits {
    occluded: array<u32>,
}

struct Counters_1 {
    counters: array<atomic<u32>, 16>,
}

@group(0) @binding(6)
var<uniform> unnamed: Push;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var sceneDepth: texture_2d<f32>;
@group(0) @binding(4)
var depthSampler: sampler;
@group(0) @binding(5)
var<storage, read_write> unnamed_1: Counters_1;
@group(0) @binding(1)
var<storage> unnamed_2: Reached;
@group(0) @binding(0)
var<storage> unnamed_3: Aabb;
@group(0) @binding(2)
var<storage> unnamed_4: OccludedBits;

fn main_1() {
    var stride: u32;
    var px: vec2<i32>;
    var size: vec2<f32>;
    var d: f32;
    var uv: vec2<f32>;
    var w: vec4<f32>;
    var p: vec3<f32>;
    var eps: f32;
    var inKept: bool;
    var inDiscarded: bool;
    var count: u32;
    var i: u32;
    var mn: vec3<f32>;
    var mx: vec3<f32>;
    var phi_70_: bool;
    var phi_215_: bool;

    let _e47 = unnamed.params[3u];
    stride = u32(_e47);
    let _e49 = gl_GlobalInvocationID_1;
    let _e52 = stride;
    let _e56 = stride;
    px = ((bitcast<vec2<i32>>(_e49.xy) * vec2(bitcast<i32>(_e52))) + vec2(bitcast<i32>((_e56 / 2u))));
    let _e63 = unnamed.params[1u];
    let _e66 = unnamed.params[2u];
    size = vec2<f32>(_e63, _e66);
    let _e69 = px[0u];
    let _e71 = size[0u];
    let _e73 = (_e69 >= i32(_e71));
    phi_70_ = _e73;
    if !(_e73) {
        let _e76 = px[1u];
        let _e78 = size[1u];
        phi_70_ = (_e76 >= i32(_e78));
    }
    let _e82 = phi_70_;
    if _e82 {
        return;
    }
    let _e83 = px;
    let _e84 = textureLoad(sceneDepth, _e83, 0i);
    d = _e84.x;
    let _e86 = d;
    if (_e86 <= 0f) {
        let _e90 = atomicAdd((&unnamed_1.counters[12i]), 1u);
        return;
    }
    let _e93 = atomicAdd((&unnamed_1.counters[8i]), 1u);
    let _e94 = px;
    let _e98 = size;
    uv = ((vec2<f32>(_e94) + vec2(0.5f)) / _e98);
    let _e101 = unnamed.invViewProj;
    let _e102 = uv;
    let _e105 = ((_e102 * 2f) - vec2(1f));
    let _e106 = d;
    w = (_e101 * vec4<f32>(_e105.x, _e105.y, _e106, 1f));
    let _e111 = w;
    let _e114 = w[3u];
    p = (_e111.xyz / vec3(_e114));
    let _e119 = unnamed.tune[0u];
    eps = _e119;
    inKept = false;
    inDiscarded = false;
    let _e122 = unnamed.params[0u];
    count = u32(_e122);
    i = 0u;
    loop {
        let _e124 = i;
        let _e125 = count;
        if (_e124 < _e125) {
            let _e127 = i;
            let _e132 = unnamed_2.reached[(_e127 >> bitcast<u32>(5u))];
            let _e133 = i;
            if ((_e132 & (1u << bitcast<u32>((_e133 & 31u)))) == 0u) {
                continue;
            }
            let _e139 = i;
            let _e143 = unnamed_3.surfaces[_e139].mins;
            let _e145 = eps;
            mn = (_e143.xyz - vec3(_e145));
            let _e148 = i;
            let _e152 = unnamed_3.surfaces[_e148].maxs;
            let _e154 = eps;
            mx = (_e152.xyz + vec3(_e154));
            let _e157 = p;
            let _e158 = mn;
            let _e160 = any((_e157 < _e158));
            phi_215_ = _e160;
            if !(_e160) {
                let _e162 = p;
                let _e163 = mx;
                phi_215_ = any((_e162 > _e163));
            }
            let _e167 = phi_215_;
            if _e167 {
                continue;
            }
            let _e168 = i;
            let _e173 = unnamed_4.occluded[(_e168 >> bitcast<u32>(5u))];
            let _e174 = i;
            if ((_e173 & (1u << bitcast<u32>((_e174 & 31u)))) != 0u) {
                inDiscarded = true;
            } else {
                inKept = true;
                break;
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e180 = i;
            i = (_e180 + bitcast<u32>(1i));
        }
    }
    let _e183 = inKept;
    if _e183 {
        let _e186 = atomicAdd((&unnamed_1.counters[9i]), 1u);
    } else {
        let _e187 = inDiscarded;
        if _e187 {
            let _e190 = atomicAdd((&unnamed_1.counters[10i]), 1u);
        } else {
            let _e193 = atomicAdd((&unnamed_1.counters[11i]), 1u);
        }
    }
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
