struct MediaVolume {
    originRadius: vec4<f32>,
    extentShape: vec4<f32>,
    albedoAnisotropy: vec4<f32>,
    emissiveIntensity: vec4<f32>,
    media: vec4<f32>,
}

struct Params {
    invViewProjection: mat4x4<f32>,
    eyeNear: vec4<f32>,
    farGlobalDensity: vec4<f32>,
    globalAlbedoAnisotropy: vec4<f32>,
    gridVolumeCount: vec4<u32>,
    timelineSeed: vec4<f32>,
}

struct Volumes {
    v: array<MediaVolume>,
}

struct Media {
    scatteringExtinction: array<vec4<f32>>,
}

@group(0) @binding(0)
var<uniform> p: Params;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(1)
var<storage> unnamed: Volumes;
@group(0) @binding(2)
var<storage, read_write> unnamed_1: Media;

fn hash13_u0028_vf3_u003b(q: ptr<function, vec3<f32>>) -> f32 {
    let _e29 = (*q);
    (*q) = fract((_e29 * 0.1031f));
    let _e32 = (*q);
    let _e33 = (*q);
    let _e38 = (*q);
    (*q) = (_e38 + vec3(dot(_e32, (_e33.yzx + vec3(33.33f)))));
    let _e42 = (*q)[0u];
    let _e44 = (*q)[1u];
    let _e47 = (*q)[2u];
    return fract(((_e42 + _e44) * _e47));
}

fn volumeWeight_u0028_struct_u002d_MediaVolume_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf41_u003b_vf3_u003b(m: ptr<function, MediaVolume>, world: ptr<function, vec3<f32>>) -> f32 {
    var local: vec3<f32>;
    var shape: f32;
    var weight: f32;
    var local_1: f32;
    var param: vec3<f32>;

    let _e35 = (*world);
    let _e37 = (*m).originRadius;
    local = (_e35 - _e37.xyz);
    let _e42 = (*m).extentShape[3u];
    shape = _e42;
    let _e43 = shape;
    if (_e43 < 1.5f) {
        let _e47 = (*m).originRadius[3u];
        let _e51 = (*m).originRadius[3u];
        let _e52 = local;
        local_1 = (1f - smoothstep((_e47 * 0.85f), _e51, length(_e52)));
    } else {
        let _e57 = local[0u];
        let _e61 = (*m).extentShape[0u];
        let _e64 = local[1u];
        let _e68 = (*m).extentShape[1u];
        let _e72 = local[2u];
        let _e76 = (*m).extentShape[2u];
        local_1 = (1f - smoothstep(0.85f, 1f, max(max((abs(_e57) / _e61), (abs(_e64) / _e68)), (abs(_e72) / _e76))));
    }
    let _e81 = local_1;
    weight = _e81;
    let _e84 = (*m).media[2u];
    if (_e84 > 0f) {
        let _e86 = (*world);
        let _e89 = (*m).media[2u];
        let _e94 = p.timelineSeed[0u];
        let _e97 = p.timelineSeed[1u];
        let _e100 = p.timelineSeed[3u];
        param = ((_e86 / vec3(_e89)) + vec3<f32>(_e94, _e97, _e100));
        let _e103 = hash13_u0028_vf3_u003b((&param));
        let _e105 = weight;
        weight = (_e105 * mix(0.55f, 1f, _e103));
    }
    let _e107 = weight;
    let _e111 = (*m).media[1u];
    let _e115 = (*world)[2u];
    let _e118 = (*m).originRadius[2u];
    return (max(_e107, 0f) * exp((-(max(_e111, 0f)) * max((_e115 - _e118), 0f))));
}

fn main_1() {
    var c: vec3<u32>;
    var dims: vec3<u32>;
    var index: u32;
    var uv: vec2<f32>;
    var slice: f32;
    var nearZ: f32;
    var farZ: f32;
    var distanceZ: f32;
    var worldH: vec4<f32>;
    var world_1: vec3<f32>;
    var extinction: f32;
    var scattering: vec3<f32>;
    var count: u32;
    var i: u32;
    var weight_1: f32;
    var param_1: MediaVolume;
    var param_2: vec3<f32>;
    var localExtinction: f32;

    let _e46 = gl_GlobalInvocationID_1;
    c = _e46;
    let _e48 = p.gridVolumeCount;
    dims = _e48.xyz;
    let _e50 = c;
    let _e51 = dims;
    if any((_e50 >= _e51)) {
        return;
    }
    let _e55 = c[2u];
    let _e57 = dims[1u];
    let _e60 = c[1u];
    let _e63 = dims[0u];
    let _e66 = c[0u];
    index = ((((_e55 * _e57) + _e60) * _e63) + _e66);
    let _e68 = c;
    let _e73 = dims;
    uv = ((vec2<f32>(_e68.xy) + vec2(0.5f)) / vec2<f32>(_e73.xy));
    let _e78 = c[2u];
    let _e82 = dims[2u];
    slice = ((f32(_e78) + 0.5f) / f32(_e82));
    let _e87 = p.eyeNear[3u];
    nearZ = max(_e87, 0.01f);
    let _e91 = p.farGlobalDensity[0u];
    let _e92 = nearZ;
    farZ = max(_e91, (_e92 + 0.01f));
    let _e95 = nearZ;
    let _e96 = farZ;
    let _e97 = nearZ;
    let _e99 = slice;
    distanceZ = (_e95 * pow((_e96 / _e97), _e99));
    let _e103 = p.invViewProjection;
    let _e104 = uv;
    let _e107 = ((_e104 * 2f) - vec2(1f));
    let _e108 = nearZ;
    let _e109 = distanceZ;
    worldH = (_e103 * vec4<f32>(_e107.x, _e107.y, (1f - (_e108 / _e109)), 1f));
    let _e116 = worldH;
    let _e119 = worldH[3u];
    world_1 = (_e116.xyz / vec3(max(abs(_e119), 0.000001f)));
    let _e126 = p.farGlobalDensity[1u];
    let _e130 = p.farGlobalDensity[2u];
    let _e134 = world_1[2u];
    extinction = (max(_e126, 0f) * exp((-(max(_e130, 0f)) * max(_e134, 0f))));
    let _e140 = p.globalAlbedoAnisotropy;
    let _e142 = extinction;
    scattering = (_e140.xyz * _e142);
    let _e146 = p.gridVolumeCount[3u];
    count = min(_e146, 64u);
    i = 0u;
    loop {
        let _e148 = i;
        let _e149 = count;
        if (_e148 < _e149) {
            let _e151 = i;
            let _e154 = unnamed.v[_e151];
            param_1.originRadius = _e154.originRadius;
            param_1.extentShape = _e154.extentShape;
            param_1.albedoAnisotropy = _e154.albedoAnisotropy;
            param_1.emissiveIntensity = _e154.emissiveIntensity;
            param_1.media = _e154.media;
            let _e165 = world_1;
            param_2 = _e165;
            let _e166 = volumeWeight_u0028_struct_u002d_MediaVolume_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf41_u003b_vf3_u003b((&param_1), (&param_2));
            weight_1 = _e166;
            let _e167 = i;
            let _e172 = unnamed.v[_e167].media[0u];
            let _e174 = weight_1;
            localExtinction = (max(_e172, 0f) * _e174);
            let _e176 = localExtinction;
            let _e177 = extinction;
            extinction = (_e177 + _e176);
            let _e179 = i;
            let _e183 = unnamed.v[_e179].albedoAnisotropy;
            let _e185 = localExtinction;
            let _e187 = i;
            let _e191 = unnamed.v[_e187].emissiveIntensity;
            let _e193 = i;
            let _e198 = unnamed.v[_e193].emissiveIntensity[3u];
            let _e201 = weight_1;
            let _e204 = scattering;
            scattering = (_e204 + ((_e183.xyz * _e185) + ((_e191.xyz * max(_e198, 0f)) * _e201)));
            continue;
        } else {
            break;
        }
        continuing {
            let _e206 = i;
            i = (_e206 + bitcast<u32>(1i));
        }
    }
    let _e209 = index;
    let _e210 = scattering;
    let _e211 = extinction;
    unnamed_1.scatteringExtinction[_e209] = vec4<f32>(_e210.x, _e210.y, _e210.z, _e211);
    return;
}

@compute @workgroup_size(4, 4, 4)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
