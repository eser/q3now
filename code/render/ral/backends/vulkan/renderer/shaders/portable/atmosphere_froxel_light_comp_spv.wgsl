struct Params {
    invViewProjection: mat4x4<f32>,
    eyeNear: vec4<f32>,
    farPad: vec4<f32>,
    gridLightCount: vec4<u32>,
    sunDirectionIntensity: vec4<f32>,
    sunColorCloudShadow: vec4<f32>,
    moonDirectionIntensity: vec4<f32>,
    moonColorLightning: vec4<f32>,
    ambientCloud: vec4<f32>,
}

struct Media {
    scatteringExtinction: array<vec4<f32>>,
}

struct TileLights {
    tileLight: array<u32>,
}

struct ForwardLight {
    positionRadius: vec4<f32>,
    colorInvRadius2_: vec4<f32>,
    endpointLinear: vec4<f32>,
}

struct Lights {
    light: array<ForwardLight>,
}

struct LitMedia {
    radianceExtinction: array<vec4<f32>>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(0)
var<uniform> p: Params;
@group(0) @binding(1)
var<storage> unnamed: Media;
@group(0) @binding(2)
var<storage> unnamed_1: TileLights;
@group(0) @binding(3)
var<storage> unnamed_2: Lights;
@group(0) @binding(4)
var<storage, read_write> unnamed_3: LitMedia;

fn main_1() {
    var c: vec3<u32>;
    var dims: vec3<u32>;
    var index: u32;
    var medium: vec4<f32>;
    var uv: vec2<f32>;
    var slice: f32;
    var nearZ: f32;
    var farZ: f32;
    var distanceZ: f32;
    var worldH: vec4<f32>;
    var world: vec3<f32>;
    var illumination: vec3<f32>;
    var tile: u32;
    var base: u32;
    var count: u32;
    var i: u32;
    var lightIndex: u32;
    var l: ForwardLight;
    var delta: vec3<f32>;
    var radius: f32;
    var attenuation: f32;

    let _e51 = gl_GlobalInvocationID_1;
    c = _e51;
    let _e53 = p.gridLightCount;
    dims = _e53.xyz;
    let _e55 = c;
    let _e56 = dims;
    if any((_e55 >= _e56)) {
        return;
    }
    let _e60 = c[2u];
    let _e62 = dims[1u];
    let _e65 = c[1u];
    let _e68 = dims[0u];
    let _e71 = c[0u];
    index = ((((_e60 * _e62) + _e65) * _e68) + _e71);
    let _e73 = index;
    let _e76 = unnamed.scatteringExtinction[_e73];
    medium = _e76;
    let _e77 = c;
    let _e82 = dims;
    uv = ((vec2<f32>(_e77.xy) + vec2(0.5f)) / vec2<f32>(_e82.xy));
    let _e87 = c[2u];
    let _e91 = dims[2u];
    slice = ((f32(_e87) + 0.5f) / f32(_e91));
    let _e96 = p.eyeNear[3u];
    nearZ = max(_e96, 0.01f);
    let _e100 = p.farPad[0u];
    let _e101 = nearZ;
    farZ = max(_e100, (_e101 + 0.01f));
    let _e104 = nearZ;
    let _e105 = farZ;
    let _e106 = nearZ;
    let _e108 = slice;
    distanceZ = (_e104 * pow((_e105 / _e106), _e108));
    let _e112 = p.invViewProjection;
    let _e113 = uv;
    let _e116 = ((_e113 * 2f) - vec2(1f));
    let _e117 = nearZ;
    let _e118 = distanceZ;
    worldH = (_e112 * vec4<f32>(_e116.x, _e116.y, (1f - (_e117 / _e118)), 1f));
    let _e125 = worldH;
    let _e128 = worldH[3u];
    world = (_e125.xyz / vec3(max(abs(_e128), 0.000001f)));
    let _e134 = p.ambientCloud;
    illumination = _e134.xyz;
    let _e137 = p.sunColorCloudShadow;
    let _e141 = p.sunDirectionIntensity[3u];
    let _e145 = p.sunColorCloudShadow[3u];
    let _e148 = illumination;
    illumination = (_e148 + ((_e137.xyz * _e141) * (1f - _e145)));
    let _e151 = p.moonColorLightning;
    let _e155 = p.moonDirectionIntensity[3u];
    let _e157 = illumination;
    illumination = (_e157 + (_e151.xyz * _e155));
    let _e160 = p.moonColorLightning;
    let _e162 = illumination;
    illumination = (_e162 + _e160.www);
    let _e165 = c[1u];
    let _e167 = dims[0u];
    let _e170 = c[0u];
    tile = ((_e165 * _e167) + _e170);
    let _e172 = tile;
    base = (_e172 * 33u);
    let _e174 = base;
    let _e177 = unnamed_1.tileLight[_e174];
    let _e181 = p.gridLightCount[3u];
    count = min(min(_e177, 32u), _e181);
    i = 0u;
    loop {
        let _e183 = i;
        let _e184 = count;
        if (_e183 < _e184) {
            let _e186 = base;
            let _e188 = i;
            let _e192 = unnamed_1.tileLight[((_e186 + 1u) + _e188)];
            lightIndex = _e192;
            let _e193 = lightIndex;
            let _e196 = p.gridLightCount[3u];
            if (_e193 >= _e196) {
                continue;
            }
            let _e198 = lightIndex;
            let _e201 = unnamed_2.light[_e198];
            l.positionRadius = _e201.positionRadius;
            l.colorInvRadius2_ = _e201.colorInvRadius2_;
            l.endpointLinear = _e201.endpointLinear;
            let _e209 = l.positionRadius;
            let _e211 = world;
            delta = (_e209.xyz - _e211);
            let _e215 = l.positionRadius[3u];
            radius = max(_e215, 0.001f);
            let _e217 = delta;
            let _e219 = radius;
            attenuation = max((1f - (length(_e217) / _e219)), 0f);
            let _e223 = attenuation;
            let _e224 = attenuation;
            attenuation = (_e224 * _e223);
            let _e227 = l.colorInvRadius2_;
            let _e229 = attenuation;
            let _e231 = illumination;
            illumination = (_e231 + (_e227.xyz * _e229));
            continue;
        } else {
            break;
        }
        continuing {
            let _e233 = i;
            i = (_e233 + bitcast<u32>(1i));
        }
    }
    let _e236 = index;
    let _e237 = medium;
    let _e239 = illumination;
    let _e240 = (_e237.xyz * _e239);
    let _e242 = medium[3u];
    unnamed_3.radianceExtinction[_e236] = vec4<f32>(_e240.x, _e240.y, _e240.z, _e242);
    return;
}

@compute @workgroup_size(4, 4, 4)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
