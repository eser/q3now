struct Params {
    gridHistory: vec4<u32>,
    depthRangeTemporal: vec4<f32>,
}

struct LitMedia {
    source: array<vec4<f32>>,
}

struct History {
    previous: array<vec4<f32>>,
}

struct Integrated {
    integrated: array<vec4<f32>>,
}

struct NextHistory {
    nextHistory: array<vec4<f32>>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(0)
var<uniform> p: Params;
@group(0) @binding(1)
var<storage> unnamed: LitMedia;
@group(0) @binding(2)
var<storage> unnamed_1: History;
@group(0) @binding(3)
var<storage, read_write> unnamed_2: Integrated;
@group(0) @binding(4)
var<storage, read_write> unnamed_3: NextHistory;

fn main_1() {
    var c: vec2<u32>;
    var sliceStride: u32;
    var transmittance: f32;
    var radiance: vec3<f32>;
    var nearZ: f32;
    var farZ: f32;
    var previousZ: f32;
    var z: u32;
    var index: u32;
    var slice: f32;
    var currentZ: f32;
    var stepLength: f32;
    var m: vec4<f32>;
    var stepT: f32;
    var current: vec4<f32>;
    var weight: f32;

    let _e36 = gl_GlobalInvocationID_1;
    c = _e36.xy;
    let _e38 = c;
    let _e40 = p.gridHistory;
    if any((_e38 >= _e40.xy)) {
        return;
    }
    let _e46 = p.gridHistory[0u];
    let _e49 = p.gridHistory[1u];
    sliceStride = (_e46 * _e49);
    transmittance = 1f;
    radiance = vec3<f32>(0f, 0f, 0f);
    let _e53 = p.depthRangeTemporal[0u];
    nearZ = max(_e53, 0.01f);
    let _e57 = p.depthRangeTemporal[1u];
    let _e58 = nearZ;
    farZ = max(_e57, (_e58 + 0.01f));
    let _e61 = nearZ;
    previousZ = _e61;
    z = 0u;
    loop {
        let _e62 = z;
        let _e65 = p.gridHistory[2u];
        if (_e62 < _e65) {
            let _e67 = z;
            let _e68 = sliceStride;
            let _e71 = c[1u];
            let _e74 = p.gridHistory[0u];
            let _e78 = c[0u];
            index = (((_e67 * _e68) + (_e71 * _e74)) + _e78);
            let _e80 = z;
            let _e85 = p.gridHistory[2u];
            slice = ((f32(_e80) + 1f) / f32(_e85));
            let _e88 = nearZ;
            let _e89 = farZ;
            let _e90 = nearZ;
            let _e92 = slice;
            currentZ = (_e88 * pow((_e89 / _e90), _e92));
            let _e95 = currentZ;
            let _e96 = previousZ;
            stepLength = (_e95 - _e96);
            let _e98 = index;
            let _e101 = unnamed.source[_e98];
            m = _e101;
            let _e103 = m[3u];
            let _e106 = stepLength;
            stepT = exp((-(max(_e103, 0f)) * _e106));
            let _e109 = transmittance;
            let _e110 = m;
            let _e113 = stepT;
            let _e117 = m[3u];
            let _e121 = radiance;
            radiance = (_e121 + (((_e110.xyz * _e109) * (1f - _e113)) / vec3(max(_e117, 0.00001f))));
            let _e123 = stepT;
            let _e124 = transmittance;
            transmittance = (_e124 * _e123);
            let _e126 = radiance;
            let _e127 = transmittance;
            current = vec4<f32>(_e126.x, _e126.y, _e126.z, _e127);
            let _e134 = p.gridHistory[3u];
            if (_e134 != 0u) {
                let _e138 = p.depthRangeTemporal[2u];
                let _e142 = p.depthRangeTemporal[3u];
                weight = (clamp(_e138, 0f, 0.98f) * (1f - clamp(_e142, 0f, 1f)));
                let _e146 = current;
                let _e147 = index;
                let _e150 = unnamed_1.previous[_e147];
                let _e151 = weight;
                current = mix(_e146, _e150, vec4(_e151));
            }
            let _e154 = index;
            let _e155 = current;
            unnamed_2.integrated[_e154] = _e155;
            let _e158 = index;
            let _e159 = current;
            unnamed_3.nextHistory[_e158] = _e159;
            let _e162 = currentZ;
            previousZ = _e162;
            continue;
        } else {
            break;
        }
        continuing {
            let _e163 = z;
            z = (_e163 + bitcast<u32>(1i));
        }
    }
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
