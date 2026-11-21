struct Push {
    viewProj: mat4x4<f32>,
    params: vec4<f32>,
    tune: vec4<f32>,
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

struct Counters {
    counters: array<u32, 16>,
}

struct OccludedBits {
    occluded: array<u32>,
}

struct Counters_1 {
    counters: array<atomic<u32>, 16>,
}

struct OccludedBits_1 {
    occluded: array<atomic<u32>>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(6)
var<uniform> unnamed: Push;
@group(0) @binding(1)
var<storage> unnamed_1: Reached;
@group(0) @binding(0)
var<storage> unnamed_2: Aabb;
@group(0) @binding(4)
var<storage, read_write> unnamed_3: Counters_1;
@group(0) @binding(2)
var hiz: texture_2d<f32>;
@group(0) @binding(3)
var hizSampler: sampler;
@group(0) @binding(5)
var<storage, read_write> unnamed_4: OccludedBits_1;

fn main_1() {
    var i: u32;
    var count: u32;
    var idxCount: u32;
    var mn: vec3<f32>;
    var mx: vec3<f32>;
    var ndcMin: vec2<f32>;
    var ndcMax: vec2<f32>;
    var nearest: f32;
    var crossesNear: bool;
    var outsideMask: u32;
    var c: i32;
    var p: vec3<f32>;
    var local: f32;
    var local_1: f32;
    var local_2: f32;
    var q: vec4<f32>;
    var n: vec3<f32>;
    var size: vec2<f32>;
    var pmin: vec2<f32>;
    var pmax: vec2<f32>;
    var extent: f32;
    var level: i32;
    var texelPx: f32;
    var dim: vec2<i32>;
    var t0_: vec2<i32>;
    var t1_: vec2<i32>;
    var farthest: f32;

    let _e73 = gl_GlobalInvocationID_1[0u];
    i = _e73;
    let _e76 = unnamed.params[0u];
    count = u32(_e76);
    let _e78 = i;
    let _e79 = count;
    if (_e78 >= _e79) {
        return;
    }
    let _e81 = i;
    let _e86 = unnamed_1.reached[(_e81 >> bitcast<u32>(5u))];
    let _e87 = i;
    if ((_e86 & (1u << bitcast<u32>((_e87 & 31u)))) == 0u) {
        return;
    }
    let _e93 = i;
    let _e98 = unnamed_2.surfaces[_e93].meta_[2u];
    idxCount = _e98;
    let _e99 = i;
    let _e103 = unnamed_2.surfaces[_e99].mins;
    mn = _e103.xyz;
    let _e105 = i;
    let _e109 = unnamed_2.surfaces[_e105].maxs;
    mx = _e109.xyz;
    ndcMin = vec2<f32>(1000000000000000000000000000000f, 1000000000000000000000000000000f);
    ndcMax = vec2<f32>(-1000000000000000000000000000000f, -1000000000000000000000000000000f);
    nearest = -1000000000000000000000000000000f;
    crossesNear = false;
    outsideMask = 63u;
    c = 0i;
    loop {
        let _e111 = c;
        if (_e111 < 8i) {
            let _e113 = c;
            if ((_e113 & 1i) != 0i) {
                let _e117 = mx[0u];
                local = _e117;
            } else {
                let _e119 = mn[0u];
                local = _e119;
            }
            let _e120 = local;
            let _e121 = c;
            if ((_e121 & 2i) != 0i) {
                let _e125 = mx[1u];
                local_1 = _e125;
            } else {
                let _e127 = mn[1u];
                local_1 = _e127;
            }
            let _e128 = local_1;
            let _e129 = c;
            if ((_e129 & 4i) != 0i) {
                let _e133 = mx[2u];
                local_2 = _e133;
            } else {
                let _e135 = mn[2u];
                local_2 = _e135;
            }
            let _e136 = local_2;
            p = vec3<f32>(_e120, _e128, _e136);
            let _e139 = unnamed.viewProj;
            let _e140 = p;
            q = (_e139 * vec4<f32>(_e140.x, _e140.y, _e140.z, 1f));
            let _e147 = q[0u];
            let _e149 = q[3u];
            if (_e147 >= -(_e149)) {
                let _e152 = outsideMask;
                outsideMask = (_e152 & 4294967294u);
            }
            let _e155 = q[0u];
            let _e157 = q[3u];
            if (_e155 <= _e157) {
                let _e159 = outsideMask;
                outsideMask = (_e159 & 4294967293u);
            }
            let _e162 = q[1u];
            let _e164 = q[3u];
            if (_e162 >= -(_e164)) {
                let _e167 = outsideMask;
                outsideMask = (_e167 & 4294967291u);
            }
            let _e170 = q[1u];
            let _e172 = q[3u];
            if (_e170 <= _e172) {
                let _e174 = outsideMask;
                outsideMask = (_e174 & 4294967287u);
            }
            let _e177 = q[2u];
            if (_e177 >= 0f) {
                let _e179 = outsideMask;
                outsideMask = (_e179 & 4294967279u);
            }
            let _e182 = q[2u];
            let _e184 = q[3u];
            if (_e182 <= _e184) {
                let _e186 = outsideMask;
                outsideMask = (_e186 & 4294967263u);
            }
            let _e189 = q[3u];
            if (_e189 <= 0.0001f) {
                crossesNear = true;
                continue;
            }
            let _e191 = q;
            let _e194 = q[3u];
            n = (_e191.xyz / vec3(_e194));
            let _e197 = ndcMin;
            let _e198 = n;
            ndcMin = min(_e197, _e198.xy);
            let _e201 = ndcMax;
            let _e202 = n;
            ndcMax = max(_e201, _e202.xy);
            let _e205 = nearest;
            let _e207 = n[2u];
            nearest = max(_e205, _e207);
            continue;
        } else {
            break;
        }
        continuing {
            let _e209 = c;
            c = (_e209 + 1i);
        }
    }
    let _e211 = outsideMask;
    if (_e211 != 0u) {
        let _e215 = atomicAdd((&unnamed_3.counters[6i]), 1u);
        let _e218 = idxCount;
        let _e219 = atomicAdd((&unnamed_3.counters[7i]), _e218);
        return;
    }
    let _e222 = atomicAdd((&unnamed_3.counters[0i]), 1u);
    let _e225 = idxCount;
    let _e226 = atomicAdd((&unnamed_3.counters[3i]), _e225);
    let _e227 = crossesNear;
    if _e227 {
        let _e230 = atomicAdd((&unnamed_3.counters[2i]), 1u);
        let _e233 = idxCount;
        let _e234 = atomicAdd((&unnamed_3.counters[5i]), _e233);
        return;
    }
    let _e237 = unnamed.params[2u];
    let _e240 = unnamed.params[3u];
    size = vec2<f32>(_e237, _e240);
    let _e242 = ndcMin;
    let _e246 = size;
    let _e250 = size;
    pmin = clamp(((((_e242 * 0.5f) + vec2(0.5f)) * _e246) - vec2(1f)), vec2<f32>(0f, 0f), (_e250 - vec2(1f)));
    let _e254 = ndcMax;
    let _e258 = size;
    let _e262 = size;
    pmax = clamp(((((_e254 * 0.5f) + vec2(0.5f)) * _e258) + vec2(1f)), vec2<f32>(0f, 0f), (_e262 - vec2(1f)));
    let _e267 = pmax[0u];
    let _e269 = pmin[0u];
    let _e272 = pmax[1u];
    let _e274 = pmin[1u];
    extent = max(max((_e267 - _e269), (_e272 - _e274)), 1f);
    let _e278 = extent;
    let _e285 = unnamed.params[1u];
    level = clamp((i32(ceil(log2(_e278))) - 1i), 0i, (i32(_e285) - 1i));
    let _e289 = level;
    texelPx = exp2(f32((_e289 + 1i)));
    let _e293 = level;
    let _e294 = textureDimensions(hiz, _e293);
    dim = vec2<i32>(_e294);
    let _e296 = pmin;
    let _e297 = texelPx;
    let _e302 = dim;
    t0_ = clamp(vec2<i32>(floor((_e296 / vec2(_e297)))), vec2<i32>(0i, 0i), (_e302 - vec2(1i)));
    let _e306 = pmax;
    let _e307 = texelPx;
    let _e312 = dim;
    t1_ = clamp(vec2<i32>(floor((_e306 / vec2(_e307)))), vec2<i32>(0i, 0i), (_e312 - vec2(1i)));
    let _e317 = t0_[0u];
    let _e319 = t0_[1u];
    let _e321 = level;
    let _e322 = textureLoad(hiz, vec2<i32>(_e317, _e319), _e321);
    farthest = _e322.x;
    let _e324 = farthest;
    let _e326 = t1_[0u];
    let _e328 = t0_[1u];
    let _e330 = level;
    let _e331 = textureLoad(hiz, vec2<i32>(_e326, _e328), _e330);
    farthest = min(_e324, _e331.x);
    let _e334 = farthest;
    let _e336 = t0_[0u];
    let _e338 = t1_[1u];
    let _e340 = level;
    let _e341 = textureLoad(hiz, vec2<i32>(_e336, _e338), _e340);
    farthest = min(_e334, _e341.x);
    let _e344 = farthest;
    let _e346 = t1_[0u];
    let _e348 = t1_[1u];
    let _e350 = level;
    let _e351 = textureLoad(hiz, vec2<i32>(_e346, _e348), _e350);
    farthest = min(_e344, _e351.x);
    let _e354 = farthest;
    let _e355 = nearest;
    let _e358 = unnamed.tune[0u];
    if (_e354 > (_e355 + _e358)) {
        let _e363 = atomicAdd((&unnamed_3.counters[1i]), 1u);
        let _e366 = idxCount;
        let _e367 = atomicAdd((&unnamed_3.counters[4i]), _e366);
        let _e368 = i;
        let _e373 = i;
        let _e377 = atomicOr((&unnamed_4.occluded[(_e368 >> bitcast<u32>(5u))]), (1u << bitcast<u32>((_e373 & 31u))));
    }
    return;
}

@compute @workgroup_size(256, 1, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
