struct Push {
    sizes: vec4<u32>,
    mode: vec4<u32>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(4)
var<uniform> unnamed: Push;
@group(0) @binding(0)
var srcDepth: texture_2d<f32>;
@group(0) @binding(1)
var srcSampler: sampler;
@group(0) @binding(2)
var prevLevel: texture_storage_2d<r32float,read>;
@group(0) @binding(3)
var dstLevel: texture_storage_2d<r32float,write>;

fn main_1() {
    var id: vec2<u32>;
    var hi: vec2<i32>;
    var base: vec2<i32>;
    var nx: i32;
    var ny: i32;
    var d: f32;
    var y: i32;
    var x: i32;
    var t: vec2<i32>;
    var v: f32;
    var local: f32;
    var phi_41_: bool;
    var phi_78_: bool;
    var phi_94_: bool;

    let _e29 = gl_GlobalInvocationID_1;
    id = _e29.xy;
    let _e32 = id[0u];
    let _e35 = unnamed.sizes[2u];
    let _e36 = (_e32 >= _e35);
    phi_41_ = _e36;
    if !(_e36) {
        let _e39 = id[1u];
        let _e42 = unnamed.sizes[3u];
        phi_41_ = (_e39 >= _e42);
    }
    let _e45 = phi_41_;
    if _e45 {
        return;
    }
    let _e48 = unnamed.sizes[0u];
    let _e53 = unnamed.sizes[1u];
    hi = vec2<i32>((bitcast<i32>(_e48) - 1i), (bitcast<i32>(_e53) - 1i));
    let _e57 = id;
    base = (bitcast<vec2<i32>>(_e57) * vec2(2i));
    let _e62 = id[0u];
    let _e65 = unnamed.sizes[2u];
    let _e67 = (_e62 == (_e65 - 1u));
    phi_78_ = _e67;
    if _e67 {
        let _e70 = unnamed.sizes[0u];
        phi_78_ = ((_e70 & 1u) != 0u);
    }
    let _e74 = phi_78_;
    nx = select(2i, 3i, _e74);
    let _e77 = id[1u];
    let _e80 = unnamed.sizes[3u];
    let _e82 = (_e77 == (_e80 - 1u));
    phi_94_ = _e82;
    if _e82 {
        let _e85 = unnamed.sizes[1u];
        phi_94_ = ((_e85 & 1u) != 0u);
    }
    let _e89 = phi_94_;
    ny = select(2i, 3i, _e89);
    d = 1f;
    y = 0i;
    loop {
        let _e91 = y;
        let _e92 = ny;
        if (_e91 < _e92) {
            x = 0i;
            loop {
                let _e94 = x;
                let _e95 = nx;
                if (_e94 < _e95) {
                    let _e97 = base;
                    let _e98 = x;
                    let _e99 = y;
                    let _e102 = hi;
                    t = min((_e97 + vec2<i32>(_e98, _e99)), _e102);
                    let _e106 = unnamed.mode[0u];
                    if (_e106 != 0u) {
                        let _e108 = t;
                        let _e109 = textureLoad(srcDepth, _e108, 0i);
                        local = _e109.x;
                    } else {
                        let _e111 = t;
                        let _e112 = textureLoad(prevLevel, _e111);
                        local = _e112.x;
                    }
                    let _e114 = local;
                    v = _e114;
                    let _e115 = d;
                    let _e116 = v;
                    d = min(_e115, _e116);
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e118 = x;
                    x = (_e118 + 1i);
                }
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e120 = y;
            y = (_e120 + 1i);
        }
    }
    let _e122 = id;
    let _e124 = d;
    textureStore(dstLevel, bitcast<vec2<i32>>(_e122), vec4<f32>(_e124, 0f, 0f, 0f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
