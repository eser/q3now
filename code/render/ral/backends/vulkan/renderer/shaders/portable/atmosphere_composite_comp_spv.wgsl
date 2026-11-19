struct Params {
    outputGrid: vec4<u32>,
    depthRange: vec4<f32>,
    invViewProjection: mat4x4<f32>,
    eyeSkyControl: vec4<f32>,
}

struct Integrated {
    media: array<vec4<f32>>,
}

@group(0) @binding(0)
var<uniform> p: Params;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(2)
var sceneDepth: texture_2d<f32>;
@group(0) @binding(34)
var sceneDepth_sampler: sampler;
@group(0) @binding(3)
var<storage> unnamed: Integrated;
@group(0) @binding(1)
var sceneHdr: texture_2d<f32>;
@group(0) @binding(33)
var sceneHdr_sampler: sampler;
@group(0) @binding(5)
var skyViewLut: texture_2d<f32>;
@group(0) @binding(37)
var skyViewLut_sampler: sampler;
@group(0) @binding(4)
var composedHdr: texture_storage_2d<rgba16float,write>;

fn wiredComposeAuthoredPhysicalSky_u0028_vf3_u003b_vf3_u003b_f1_u003b(authored: ptr<function, vec3<f32>>, physical: ptr<function, vec3<f32>>, blend: ptr<function, f32>) -> vec3<f32> {
    var authoredEnergy: f32;
    var physicalEnergy: f32;
    var scale: f32;
    var physicalTint: vec3<f32>;
    var local: vec3<f32>;
    var relit: vec3<f32>;

    let _e51 = (*authored);
    authoredEnergy = dot(max(_e51, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e54 = (*physical);
    physicalEnergy = dot(max(_e54, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e57 = authoredEnergy;
    if (_e57 <= 0.00001f) {
        let _e59 = (*authored);
        let _e60 = (*physical);
        let _e61 = (*blend);
        return mix(_e59, _e60, vec3(_e61));
    }
    let _e64 = physicalEnergy;
    let _e65 = authoredEnergy;
    scale = clamp((_e64 / max(_e65, 0.18f)), 0.65f, 1.75f);
    let _e69 = physicalEnergy;
    if (_e69 > 0.00001f) {
        let _e71 = (*physical);
        let _e72 = physicalEnergy;
        local = (_e71 / vec3(_e72));
    } else {
        local = vec3<f32>(1f, 1f, 1f);
    }
    let _e75 = local;
    physicalTint = _e75;
    let _e76 = (*authored);
    let _e77 = scale;
    let _e79 = physicalTint;
    let _e84 = (*physical);
    relit = (((_e76 * _e77) * mix(vec3<f32>(1f, 1f, 1f), clamp(_e79, vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(2f, 2f, 2f)), vec3(0.12f))) + (_e84 * 0.08f));
    let _e87 = (*authored);
    let _e88 = relit;
    let _e89 = (*blend);
    return mix(_e87, _e88, vec3(_e89));
}

fn skyViewUv_u0028_vf2_u003b(screenUv: ptr<function, vec2<f32>>) -> vec2<f32> {
    var worldH: vec4<f32>;
    var direction: vec3<f32>;
    var azimuth: f32;
    var elevation: f32;

    let _e48 = p.invViewProjection;
    let _e49 = (*screenUv);
    let _e52 = ((_e49 * 2f) - vec2(1f));
    worldH = (_e48 * vec4<f32>(_e52.x, _e52.y, 0f, 1f));
    let _e57 = worldH;
    let _e60 = p.eyeSkyControl;
    let _e63 = worldH[3u];
    direction = normalize((_e57.xyz - (_e60.xyz * _e63)));
    let _e68 = direction[1u];
    let _e70 = direction[0u];
    azimuth = atan2(_e68, _e70);
    let _e73 = direction[2u];
    elevation = asin(clamp(_e73, -1f, 1f));
    let _e76 = azimuth;
    let _e80 = elevation;
    return vec2<f32>(fract(((_e76 / 6.2831855f) + 0.5f)), clamp(((_e80 / 3.1415927f) + 0.5f), 0f, 1f));
}

fn main_1() {
    var pixel: vec2<u32>;
    var uv: vec2<f32>;
    var depth: f32;
    var local_1: f32;
    var nearZ: f32;
    var farZ: f32;
    var inverseW: vec4<f32>;
    var reciprocalDistance: f32;
    var distanceZ: f32;
    var local_2: f32;
    var sliceFraction: f32;
    var froxelXY: vec2<u32>;
    var slices: u32;
    var z: u32;
    var index: u32;
    var atmosphere: vec4<f32>;
    var scene: vec3<f32>;
    var physicalSky: vec3<f32>;
    var param: vec2<f32>;
    var param_1: vec3<f32>;
    var param_2: vec3<f32>;
    var param_3: f32;
    var phi_334_: bool;

    let _e64 = gl_GlobalInvocationID_1;
    pixel = _e64.xy;
    let _e66 = pixel;
    let _e68 = p.outputGrid;
    if any((_e66 >= _e68.xy)) {
        return;
    }
    let _e72 = pixel;
    let _e77 = p.outputGrid;
    uv = ((vec2<f32>(_e72) + vec2(0.5f)) / vec2<f32>(_e77.xy));
    let _e83 = p.depthRange[2u];
    if (_e83 > 0.5f) {
        let _e85 = uv;
        let _e86 = textureSampleLevel(sceneDepth, sceneDepth_sampler, _e85, 0f);
        local_1 = _e86.x;
    } else {
        local_1 = 0f;
    }
    let _e88 = local_1;
    depth = _e88;
    let _e91 = p.depthRange[0u];
    nearZ = max(_e91, 0.01f);
    let _e95 = p.depthRange[1u];
    let _e96 = nearZ;
    farZ = max(_e95, (_e96 + 0.01f));
    let _e102 = p.invViewProjection[0][3u];
    let _e106 = p.invViewProjection[1][3u];
    let _e110 = p.invViewProjection[2][3u];
    let _e114 = p.invViewProjection[3][3u];
    inverseW = vec4<f32>(_e102, _e106, _e110, _e114);
    let _e116 = inverseW;
    let _e117 = uv;
    let _e120 = ((_e117 * 2f) - vec2(1f));
    let _e121 = depth;
    reciprocalDistance = dot(_e116, vec4<f32>(_e120.x, _e120.y, _e121, 1f));
    let _e126 = depth;
    if (_e126 <= 0f) {
        let _e128 = farZ;
        local_2 = _e128;
    } else {
        let _e129 = reciprocalDistance;
        let _e130 = farZ;
        let _e134 = nearZ;
        let _e135 = farZ;
        local_2 = clamp((1f / max(_e129, (1f / _e130))), _e134, _e135);
    }
    let _e137 = local_2;
    distanceZ = _e137;
    let _e138 = distanceZ;
    let _e139 = nearZ;
    let _e142 = farZ;
    let _e143 = nearZ;
    sliceFraction = clamp((log((_e138 / _e139)) / log((_e142 / _e143))), 0f, 1f);
    let _e148 = uv;
    let _e150 = p.outputGrid;
    let _e156 = p.outputGrid;
    froxelXY = min(vec2<u32>((_e148 * vec2<f32>(_e150.zw))), (_e156.zw - vec2(1u)));
    let _e163 = p.depthRange[3u];
    slices = u32(max(_e163, 1f));
    let _e166 = sliceFraction;
    let _e167 = slices;
    let _e171 = slices;
    z = min(u32((_e166 * f32(_e167))), (_e171 - 1u));
    let _e174 = z;
    let _e177 = p.outputGrid[3u];
    let _e180 = froxelXY[1u];
    let _e184 = p.outputGrid[2u];
    let _e187 = froxelXY[0u];
    index = ((((_e174 * _e177) + _e180) * _e184) + _e187);
    let _e189 = index;
    let _e192 = unnamed.media[_e189];
    atmosphere = _e192;
    let _e193 = uv;
    let _e194 = textureSampleLevel(sceneHdr, sceneHdr_sampler, _e193, 0f);
    scene = _e194.xyz;
    let _e196 = depth;
    let _e197 = (_e196 <= 0f);
    phi_334_ = _e197;
    if _e197 {
        let _e200 = p.eyeSkyControl[3u];
        phi_334_ = (_e200 > 0f);
    }
    let _e203 = phi_334_;
    if _e203 {
        let _e204 = uv;
        param = _e204;
        let _e205 = skyViewUv_u0028_vf2_u003b((&param));
        let _e206 = textureSampleLevel(skyViewLut, skyViewLut_sampler, _e205, 0f);
        physicalSky = _e206.xyz;
        let _e210 = p.eyeSkyControl[3u];
        let _e212 = scene;
        param_1 = _e212;
        let _e213 = physicalSky;
        param_2 = _e213;
        param_3 = clamp(_e210, 0f, 1f);
        let _e214 = wiredComposeAuthoredPhysicalSky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_1), (&param_2), (&param_3));
        scene = _e214;
    }
    let _e215 = pixel;
    let _e217 = scene;
    let _e219 = atmosphere[3u];
    let _e221 = atmosphere;
    let _e223 = ((_e217 * _e219) + _e221.xyz);
    textureStore(composedHdr, bitcast<vec2<i32>>(_e215), vec4<f32>(_e223.x, _e223.y, _e223.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
