struct LensPush {
    viewport: vec2<f32>,
    count: i32,
    radiusPx: f32,
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
    var texel: vec2<f32>;
    var local: vec2<f32>;
    var visible: i32;
    var t: i32;
    var suv: vec2<f32>;
    var indexable: array<vec2<f32>, 9>;
    var scene: f32;
    var phi_65_: bool;
    var phi_73_: bool;
    var phi_80_: bool;

    let _e45 = gl_GlobalInvocationID_1[0u];
    idx = _e45;
    let _e46 = idx;
    let _e48 = unnamed.count;
    if (_e46 >= bitcast<u32>(_e48)) {
        return;
    }
    let _e51 = idx;
    let _e55 = unnamed_1.lensSources[_e51].screenPosDepth;
    uv = _e55.xy;
    let _e57 = idx;
    let _e62 = unnamed_1.lensSources[_e57].screenPosDepth[2u];
    compareZ = _e62;
    let _e64 = uv[0u];
    let _e65 = (_e64 < 0f);
    phi_65_ = _e65;
    if !(_e65) {
        let _e68 = uv[0u];
        phi_65_ = (_e68 > 1f);
    }
    let _e71 = phi_65_;
    phi_73_ = _e71;
    if !(_e71) {
        let _e74 = uv[1u];
        phi_73_ = (_e74 < 0f);
    }
    let _e77 = phi_73_;
    phi_80_ = _e77;
    if !(_e77) {
        let _e80 = uv[1u];
        phi_80_ = (_e80 > 1f);
    }
    let _e83 = phi_80_;
    if _e83 {
        let _e84 = idx;
        unnamed_1.lensSources[_e84].colorVis[3u] = 0f;
        return;
    }
    let _e91 = unnamed.viewport[0u];
    if (_e91 > 0f) {
        let _e94 = unnamed.radiusPx;
        let _e96 = unnamed.viewport;
        local = (vec2(_e94) / _e96);
    } else {
        local = vec2<f32>(0f, 0f);
    }
    let _e99 = local;
    texel = _e99;
    visible = 0i;
    t = 0i;
    loop {
        let _e100 = t;
        if (_e100 < 9i) {
            let _e102 = uv;
            let _e103 = t;
            indexable = array<vec2<f32>, 9>(vec2<f32>(0f, 0f), vec2<f32>(1f, 0f), vec2<f32>(-1f, 0f), vec2<f32>(0f, 1f), vec2<f32>(0f, -1f), vec2<f32>(0.7f, 0.7f), vec2<f32>(-0.7f, 0.7f), vec2<f32>(0.7f, -0.7f), vec2<f32>(-0.7f, -0.7f));
            let _e105 = indexable[_e103];
            let _e106 = texel;
            suv = clamp((_e102 + (_e105 * _e106)), vec2<f32>(0f, 0f), vec2<f32>(1f, 1f));
            let _e110 = suv;
            let _e111 = textureSampleLevel(depthTex, depthSampler, _e110, 0f);
            scene = _e111.x;
            let _e113 = scene;
            let _e114 = compareZ;
            if (_e113 <= (_e114 + 0.0008f)) {
                let _e117 = visible;
                visible = (_e117 + 1i);
            }
            continue;
        } else {
            break;
        }
        continuing {
            let _e119 = t;
            t = (_e119 + 1i);
        }
    }
    let _e121 = idx;
    let _e122 = visible;
    unnamed_1.lensSources[_e121].colorVis[3u] = (f32(_e122) / 9f);
    return;
}

@compute @workgroup_size(64, 1, 1) 
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
