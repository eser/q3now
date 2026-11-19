struct LensPush {
    viewport: vec2<f32>,
    count: i32,
    radiusPx: f32,
    depthConvention: vec4<f32>,
}

struct LensSource {
    screenPosDepth: vec4<f32>,
    colorVis: vec4<f32>,
}

struct LensSources {
    lensSources: array<LensSource>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(3)
var<uniform> unnamed: LensPush;
@group(0) @binding(2)
var<storage, read_write> unnamed_1: LensSources;
@group(0) @binding(0)
var depthTex: texture_2d<f32>;
@group(0) @binding(1)
var depthSampler: sampler;

fn main_1() {
    var idx: u32;
    var uv: vec2<f32>;
    var compareZ: f32;
    var radius: f32;
    var local: f32;
    var texel: vec2<f32>;
    var local_1: vec2<f32>;
    var visible: i32;
    var t: i32;
    var suv: vec2<f32>;
    var indexable: array<vec2<f32>, 9>;
    var scene: f32;
    var phi_65_: bool;
    var phi_73_: bool;
    var phi_80_: bool;
    var phi_114_: bool;

    let _e48 = gl_GlobalInvocationID_1[0u];
    idx = _e48;
    let _e49 = idx;
    let _e51 = unnamed.count;
    if (_e49 >= bitcast<u32>(_e51)) {
        return;
    }
    let _e54 = idx;
    let _e58 = unnamed_1.lensSources[_e54].screenPosDepth;
    uv = _e58.xy;
    let _e60 = idx;
    let _e65 = unnamed_1.lensSources[_e60].screenPosDepth[2u];
    compareZ = _e65;
    let _e67 = uv[0u];
    let _e68 = (_e67 < 0f);
    phi_65_ = _e68;
    if !(_e68) {
        let _e71 = uv[0u];
        phi_65_ = (_e71 > 1f);
    }
    let _e74 = phi_65_;
    phi_73_ = _e74;
    if !(_e74) {
        let _e77 = uv[1u];
        phi_73_ = (_e77 < 0f);
    }
    let _e80 = phi_73_;
    phi_80_ = _e80;
    if !(_e80) {
        let _e83 = uv[1u];
        phi_80_ = (_e83 > 1f);
    }
    let _e86 = phi_80_;
    if _e86 {
        let _e87 = idx;
        unnamed_1.lensSources[_e87].colorVis[3u] = 0f;
        return;
    }
    let _e94 = unnamed.depthConvention[2u];
    if (_e94 > 0.5f) {
        let _e96 = idx;
        let _e101 = unnamed_1.lensSources[_e96].screenPosDepth[3u];
        local = _e101;
    } else {
        let _e103 = unnamed.radiusPx;
        local = _e103;
    }
    let _e104 = local;
    radius = _e104;
    let _e107 = unnamed.viewport[0u];
    let _e108 = (_e107 > 0f);
    phi_114_ = _e108;
    if _e108 {
        let _e111 = unnamed.viewport[1u];
        phi_114_ = (_e111 > 0f);
    }
    let _e114 = phi_114_;
    if _e114 {
        let _e115 = radius;
        let _e118 = unnamed.viewport;
        local_1 = (vec2(max(_e115, 0f)) / _e118);
    } else {
        local_1 = vec2<f32>(0f, 0f);
    }
    let _e121 = local_1;
    texel = _e121;
    visible = 0i;
    t = 0i;
    loop {
        let _e122 = t;
        if (_e122 < 9i) {
            let _e124 = uv;
            let _e125 = t;
            indexable = array<vec2<f32>, 9>(vec2<f32>(0f, 0f), vec2<f32>(1f, 0f), vec2<f32>(-1f, 0f), vec2<f32>(0f, 1f), vec2<f32>(0f, -1f), vec2<f32>(0.7f, 0.7f), vec2<f32>(-0.7f, 0.7f), vec2<f32>(0.7f, -0.7f), vec2<f32>(-0.7f, -0.7f));
            let _e127 = indexable[_e125];
            let _e128 = texel;
            suv = clamp((_e124 + (_e127 * _e128)), vec2<f32>(0f, 0f), vec2<f32>(1f, 1f));
            let _e132 = suv;
            let _e133 = textureSampleLevel(depthTex, depthSampler, _e132, 0f);
            scene = _e133.x;
            let _e135 = scene;
            let _e136 = compareZ;
            let _e140 = unnamed.depthConvention[0u];
            let _e144 = unnamed.depthConvention[1u];
            if (((_e135 - _e136) * _e140) <= _e144) {
                let _e146 = visible;
                visible = (_e146 + 1i);
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e148 = t;
            t = (_e148 + 1i);
        }
    }
    let _e150 = idx;
    let _e151 = visible;
    let _e154 = idx;
    let _e159 = unnamed_1.lensSources[_e154].colorVis[0u];
    unnamed_1.lensSources[_e150].colorVis[3u] = ((f32(_e151) / 9f) * clamp(_e159, 0f, 1f));
    return;
}

@compute @workgroup_size(64, 1, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
