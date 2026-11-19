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
    var segment: vec3<f32>;
    var lengthSquared: f32;
    var radius: f32;
    var attenuation: f32;

    let _e53 = gl_GlobalInvocationID_1;
    c = _e53;
    let _e55 = p.gridLightCount;
    dims = _e55.xyz;
    let _e57 = c;
    let _e58 = dims;
    if any((_e57 >= _e58)) {
        return;
    }
    let _e62 = c[2u];
    let _e64 = dims[1u];
    let _e67 = c[1u];
    let _e70 = dims[0u];
    let _e73 = c[0u];
    index = ((((_e62 * _e64) + _e67) * _e70) + _e73);
    let _e75 = index;
    let _e78 = unnamed.scatteringExtinction[_e75];
    medium = _e78;
    let _e79 = c;
    let _e84 = dims;
    uv = ((vec2<f32>(_e79.xy) + vec2(0.5f)) / vec2<f32>(_e84.xy));
    let _e89 = c[2u];
    let _e93 = dims[2u];
    slice = ((f32(_e89) + 0.5f) / f32(_e93));
    let _e98 = p.eyeNear[3u];
    nearZ = max(_e98, 0.01f);
    let _e102 = p.farPad[0u];
    let _e103 = nearZ;
    farZ = max(_e102, (_e103 + 0.01f));
    let _e106 = nearZ;
    let _e107 = farZ;
    let _e108 = nearZ;
    let _e110 = slice;
    distanceZ = (_e106 * pow((_e107 / _e108), _e110));
    let _e114 = p.invViewProjection;
    let _e115 = uv;
    let _e118 = ((_e115 * 2f) - vec2(1f));
    let _e119 = nearZ;
    let _e120 = distanceZ;
    worldH = (_e114 * vec4<f32>(_e118.x, _e118.y, (1f - (_e119 / _e120)), 1f));
    let _e127 = worldH;
    let _e130 = worldH[3u];
    world = (_e127.xyz / vec3(max(abs(_e130), 0.000001f)));
    let _e136 = p.ambientCloud;
    illumination = _e136.xyz;
    let _e139 = p.sunColorCloudShadow;
    let _e143 = p.sunDirectionIntensity[3u];
    let _e147 = p.sunColorCloudShadow[3u];
    let _e150 = illumination;
    illumination = (_e150 + ((_e139.xyz * _e143) * (1f - _e147)));
    let _e153 = p.moonColorLightning;
    let _e157 = p.moonDirectionIntensity[3u];
    let _e159 = illumination;
    illumination = (_e159 + (_e153.xyz * _e157));
    let _e162 = p.moonColorLightning;
    let _e164 = illumination;
    illumination = (_e164 + _e162.www);
    let _e167 = c[1u];
    let _e169 = dims[0u];
    let _e172 = c[0u];
    tile = ((_e167 * _e169) + _e172);
    let _e174 = tile;
    base = (_e174 * 33u);
    let _e176 = base;
    let _e179 = unnamed_1.tileLight[_e176];
    let _e183 = p.gridLightCount[3u];
    count = min(min(_e179, 32u), _e183);
    i = 0u;
    loop {
        let _e185 = i;
        let _e186 = count;
        if (_e185 < _e186) {
            let _e188 = base;
            let _e190 = i;
            let _e194 = unnamed_1.tileLight[((_e188 + 1u) + _e190)];
            lightIndex = _e194;
            let _e195 = lightIndex;
            let _e198 = p.gridLightCount[3u];
            if (_e195 >= _e198) {
                continue;
            }
            let _e200 = lightIndex;
            let _e203 = unnamed_2.light[_e200];
            l.positionRadius = _e203.positionRadius;
            l.colorInvRadius2_ = _e203.colorInvRadius2_;
            l.endpointLinear = _e203.endpointLinear;
            let _e211 = l.positionRadius;
            let _e213 = world;
            delta = (_e211.xyz - _e213);
            let _e217 = l.endpointLinear[3u];
            if (_e217 > 0.5f) {
                let _e220 = l.endpointLinear;
                let _e223 = l.positionRadius;
                segment = (_e220.xyz - _e223.xyz);
                let _e226 = segment;
                let _e227 = segment;
                lengthSquared = dot(_e226, _e227);
                let _e229 = lengthSquared;
                if (_e229 > 0f) {
                    let _e231 = segment;
                    let _e232 = delta;
                    let _e233 = segment;
                    let _e236 = lengthSquared;
                    let _e240 = delta;
                    delta = (_e240 + (_e231 * clamp((-(dot(_e232, _e233)) / _e236), 0f, 1f)));
                }
            }
            let _e244 = l.positionRadius[3u];
            radius = max(_e244, 0.001f);
            let _e246 = delta;
            let _e248 = radius;
            attenuation = max((1f - (length(_e246) / _e248)), 0f);
            let _e252 = attenuation;
            let _e253 = attenuation;
            attenuation = (_e253 * _e252);
            let _e256 = l.colorInvRadius2_;
            let _e258 = attenuation;
            let _e260 = illumination;
            illumination = (_e260 + (_e256.xyz * _e258));
            continue;
        } else {
            break;
        }
        continuing {
            let _e262 = i;
            i = (_e262 + bitcast<u32>(1i));
        }
    }
    let _e265 = index;
    let _e266 = medium;
    let _e268 = illumination;
    let _e269 = (_e266.xyz * _e268);
    let _e271 = medium[3u];
    unnamed_3.radianceExtinction[_e265] = vec4<f32>(_e269.x, _e269.y, _e269.z, _e271);
    return;
}

@compute @workgroup_size(4, 4, 4)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
