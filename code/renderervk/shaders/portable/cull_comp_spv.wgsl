struct Reached {
    reached: array<u32>,
}

struct Push {
    frustum: array<vec4<f32>, 4>,
    viewOrigin: vec4<f32>,
    params: vec4<u32>,
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

struct Visible {
    visible: array<u32>,
}

struct Visible_1 {
    visible: array<atomic<u32>>,
}

@group(0) @binding(1) 
var<storage> unnamed: Reached;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var<uniform> unnamed_1: Push;
@group(0) @binding(0) 
var<storage> unnamed_2: Aabb;
@group(0) @binding(2) 
var<storage, read_write> unnamed_3: Visible_1;

fn boxFullyBehind_u0028_vf3_u003b_vf3_u003b_vf4_u003b(mins: ptr<function, vec3<f32>>, maxs: ptr<function, vec3<f32>>, plane: ptr<function, vec4<f32>>) -> bool {
    var n: vec3<f32>;
    var p: vec3<f32>;
    var local: f32;
    var local_1: f32;
    var local_2: f32;

    let _e30 = (*plane);
    n = _e30.xyz;
    let _e33 = n[0u];
    if (_e33 >= 0f) {
        let _e36 = (*maxs)[0u];
        local = _e36;
    } else {
        let _e38 = (*mins)[0u];
        local = _e38;
    }
    let _e39 = local;
    p[0u] = _e39;
    let _e42 = n[1u];
    if (_e42 >= 0f) {
        let _e45 = (*maxs)[1u];
        local_1 = _e45;
    } else {
        let _e47 = (*mins)[1u];
        local_1 = _e47;
    }
    let _e48 = local_1;
    p[1u] = _e48;
    let _e51 = n[2u];
    if (_e51 >= 0f) {
        let _e54 = (*maxs)[2u];
        local_2 = _e54;
    } else {
        let _e56 = (*mins)[2u];
        local_2 = _e56;
    }
    let _e57 = local_2;
    p[2u] = _e57;
    let _e59 = n;
    let _e60 = p;
    let _e63 = (*plane)[3u];
    return ((dot(_e59, _e60) - _e63) < 0f);
}

fn reachedBit_u0028_u1_u003b(i: ptr<function, u32>) -> bool {
    let _e23 = (*i);
    let _e28 = unnamed.reached[(_e23 >> bitcast<u32>(5u))];
    let _e29 = (*i);
    return ((_e28 & (1u << bitcast<u32>((_e29 & 31u)))) != 0u);
}

fn main_1() {
    var i_1: u32;
    var count: u32;
    var flagBase: u32;
    var vis: bool;
    var param: u32;
    var mn: vec3<f32>;
    var mx: vec3<f32>;
    var p_1: i32;
    var param_1: vec3<f32>;
    var param_2: vec3<f32>;
    var param_3: vec4<f32>;
    var ct: u32;
    var pl: vec4<f32>;
    var d: f32;
    var slot: u32;
    var phi_140_: bool;
    var phi_192_: bool;

    let _e38 = gl_GlobalInvocationID_1[0u];
    i_1 = _e38;
    let _e41 = unnamed_1.params[0u];
    count = _e41;
    let _e42 = i_1;
    let _e43 = count;
    if (_e42 >= _e43) {
        return;
    }
    let _e45 = count;
    flagBase = (_e45 + 1u);
    let _e47 = i_1;
    param = _e47;
    let _e48 = reachedBit_u0028_u1_u003b((&param));
    vis = _e48;
    let _e49 = vis;
    phi_140_ = _e49;
    if _e49 {
        let _e52 = unnamed_1.params[2u];
        phi_140_ = (_e52 != 0u);
    }
    let _e55 = phi_140_;
    if _e55 {
        let _e56 = i_1;
        let _e60 = unnamed_2.surfaces[_e56].mins;
        mn = _e60.xyz;
        let _e62 = i_1;
        let _e66 = unnamed_2.surfaces[_e62].maxs;
        mx = _e66.xyz;
        p_1 = 0i;
        loop {
            let _e68 = p_1;
            if (_e68 < 4i) {
                let _e70 = p_1;
                let _e71 = mn;
                param_1 = _e71;
                let _e72 = mx;
                param_2 = _e72;
                let _e75 = unnamed_1.frustum[_e70];
                param_3 = _e75;
                let _e76 = boxFullyBehind_u0028_vf3_u003b_vf3_u003b_vf4_u003b((&param_1), (&param_2), (&param_3));
                if _e76 {
                    vis = false;
                    break;
                }
                continue;
            } else {
                break;
            }
            continuing {
                let _e77 = p_1;
                p_1 = (_e77 + 1i);
            }
        }
    }
    let _e79 = vis;
    phi_192_ = _e79;
    if _e79 {
        let _e82 = unnamed_1.params[1u];
        phi_192_ = (_e82 != 0u);
    }
    let _e85 = phi_192_;
    if _e85 {
        let _e86 = i_1;
        let _e91 = unnamed_2.surfaces[_e86].meta_[0u];
        ct = _e91;
        let _e92 = ct;
        if (_e92 != 2u) {
            let _e94 = i_1;
            let _e98 = unnamed_2.surfaces[_e94].plane;
            pl = _e98;
            let _e100 = unnamed_1.viewOrigin;
            let _e102 = pl;
            d = dot(_e100.xyz, _e102.xyz);
            let _e105 = ct;
            if (_e105 == 0u) {
                let _e107 = d;
                let _e109 = pl[3u];
                if (_e107 < (_e109 - 8f)) {
                    vis = false;
                }
            } else {
                let _e112 = d;
                let _e114 = pl[3u];
                if (_e112 > (_e114 + 8f)) {
                    vis = false;
                }
            }
        }
    }
    let _e117 = flagBase;
    let _e118 = i_1;
    let _e120 = vis;
    atomicStore((&unnamed_3.visible[(_e117 + _e118)]), select(0u, 1u, _e120));
    let _e124 = vis;
    if _e124 {
        let _e127 = atomicAdd((&unnamed_3.visible[0i]), 1u);
        slot = _e127;
        let _e128 = slot;
        let _e130 = count;
        if ((_e128 + 1u) <= _e130) {
            let _e132 = slot;
            let _e134 = i_1;
            atomicStore((&unnamed_3.visible[(1u + _e132)]), _e134);
        }
    }
    return;
}

@compute @workgroup_size(256, 1, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
