struct Params {
    outputGrid: vec4<u32>,
    depthRange: vec4<f32>,
    invViewProjection: mat4x4<f32>,
    eyeSkyControl: vec4<f32>,
    radianceControl: vec4<f32>,
    mediaColorDensity: vec4<f32>,
    mediaHeight: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> p: Params;
@group(0) @binding(5)
var skyViewLut: texture_2d<f32>;
@group(0) @binding(37)
var skyViewLut_sampler: sampler;
@group(0) @binding(6)
var currentSkyRadiance: texture_cube<f32>;
@group(0) @binding(38)
var currentSkyRadiance_sampler: sampler;
var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(1)
var sceneHdr: texture_2d<f32>;
@group(0) @binding(33)
var sceneHdr_sampler: sampler;
@group(0) @binding(2)
var sceneDepth: texture_2d<f32>;
@group(0) @binding(34)
var sceneDepth_sampler: sampler;
@group(0) @binding(4)
var composedHdr: texture_storage_2d<rgba16float,write>;
@group(0) @binding(7)
var previousSkyRadiance: texture_cube<f32>;
@group(0) @binding(39)
var previousSkyRadiance_sampler: sampler;

fn skyViewUv_u0028_vf3_u003b(direction: ptr<function, vec3<f32>>) -> vec2<f32> {
    var azimuth: f32;
    var elevation: f32;

    let _e73 = (*direction)[1u];
    let _e75 = (*direction)[0u];
    azimuth = atan2(_e73, _e75);
    let _e78 = (*direction)[2u];
    elevation = asin(clamp(_e78, -1f, 1f));
    let _e81 = azimuth;
    let _e85 = elevation;
    return vec2<f32>(fract(((_e81 / 6.2831855f) + 0.5f)), clamp(((_e85 / 3.1415927f) + 0.5f), 0f, 1f));
}

fn wiredAerialInscatter_u0028_vf3_u003b_vf3_u003b(mediaColor: ptr<function, vec3<f32>>, viewDir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var aerial: f32;
    var skyIn: vec3<f32>;
    var local: vec3<f32>;
    var param: vec3<f32>;
    var skySum: vec3<f32>;
    var skyMean: f32;
    var directional: vec3<f32>;
    var sunDir: vec3<f32>;
    var cosT: f32;
    var hg: f32;
    var tinted: vec3<f32>;

    let _e84 = p.mediaHeight[3u];
    aerial = clamp(_e84, 0f, 1f);
    let _e86 = aerial;
    if (_e86 <= 0f) {
        let _e88 = (*mediaColor);
        return _e88;
    }
    let _e91 = p.eyeSkyControl[3u];
    if (_e91 > 0f) {
        let _e93 = (*viewDir);
        param = _e93;
        let _e94 = skyViewUv_u0028_vf3_u003b((&param));
        let _e95 = textureSampleLevel(skyViewLut, skyViewLut_sampler, _e94, 0f);
        local = _e95.xyz;
    } else {
        let _e97 = (*viewDir);
        let _e98 = textureSampleLevel(currentSkyRadiance, currentSkyRadiance_sampler, _e97, 4f);
        local = _e98.xyz;
    }
    let _e100 = local;
    skyIn = _e100;
    let _e101 = textureSampleLevel(currentSkyRadiance, currentSkyRadiance_sampler, vec3<f32>(1f, 0f, 0f), 7f);
    let _e103 = textureSampleLevel(currentSkyRadiance, currentSkyRadiance_sampler, vec3<f32>(-1f, 0f, 0f), 7f);
    let _e106 = textureSampleLevel(currentSkyRadiance, currentSkyRadiance_sampler, vec3<f32>(0f, 1f, 0f), 7f);
    let _e109 = textureSampleLevel(currentSkyRadiance, currentSkyRadiance_sampler, vec3<f32>(0f, -1f, 0f), 7f);
    let _e112 = textureSampleLevel(currentSkyRadiance, currentSkyRadiance_sampler, vec3<f32>(0f, 0f, 1f), 7f);
    let _e115 = textureSampleLevel(currentSkyRadiance, currentSkyRadiance_sampler, vec3<f32>(0f, 0f, -1f), 7f);
    skySum = (((((_e101.xyz + _e103.xyz) + _e106.xyz) + _e109.xyz) + _e112.xyz) + _e115.xyz);
    let _e118 = skySum;
    skyMean = max(dot((_e118 / vec3(6f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f)), 0.0001f);
    let _e123 = skyIn;
    let _e124 = skyMean;
    directional = (_e123 / vec3(_e124));
    let _e129 = p.depthRange[3u];
    let _e132 = p.radianceControl[3u];
    let _e135 = p.mediaHeight[2u];
    sunDir = normalize((vec3<f32>(_e129, _e132, _e135) + vec3<f32>(0.000001f, 0.000001f, 0.000001f)));
    let _e139 = (*viewDir);
    let _e140 = sunDir;
    cosT = dot(_e139, _e140);
    let _e142 = cosT;
    hg = (0.6975f / max(pow((1.3025f - (1.1f * _e142)), 1.5f), 0.0001f));
    let _e148 = (*mediaColor);
    let _e149 = directional;
    let _e151 = aerial;
    tinted = (_e148 * mix(vec3<f32>(1f, 1f, 1f), clamp(_e149, vec3<f32>(0.35f, 0.35f, 0.35f), vec3<f32>(3f, 3f, 3f)), vec3(_e151)));
    let _e155 = tinted;
    let _e156 = hg;
    let _e159 = aerial;
    return (_e155 * mix(1f, (0.75f + (0.25f * _e156)), _e159));
}

fn wiredComposeAuthoredPhysicalSky_u0028_vf3_u003b_vf3_u003b_f1_u003b(authored: ptr<function, vec3<f32>>, physical: ptr<function, vec3<f32>>, blend: ptr<function, f32>) -> vec3<f32> {
    var authoredEnergy: f32;
    var physicalEnergy: f32;
    var scale: f32;
    var physicalTint: vec3<f32>;
    var local_1: vec3<f32>;
    var relit: vec3<f32>;

    let _e78 = (*authored);
    authoredEnergy = dot(max(_e78, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e81 = (*physical);
    physicalEnergy = dot(max(_e81, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e84 = authoredEnergy;
    if (_e84 <= 0.00001f) {
        let _e86 = (*authored);
        let _e87 = (*physical);
        let _e88 = (*blend);
        return mix(_e86, _e87, vec3(_e88));
    }
    let _e91 = physicalEnergy;
    let _e92 = authoredEnergy;
    scale = clamp((_e91 / max(_e92, 0.18f)), 0.65f, 1.75f);
    let _e96 = physicalEnergy;
    if (_e96 > 0.00001f) {
        let _e98 = (*physical);
        let _e99 = physicalEnergy;
        local_1 = (_e98 / vec3(_e99));
    } else {
        local_1 = vec3<f32>(1f, 1f, 1f);
    }
    let _e102 = local_1;
    physicalTint = _e102;
    let _e103 = (*authored);
    let _e104 = scale;
    let _e106 = physicalTint;
    let _e111 = (*physical);
    relit = (((_e103 * _e104) * mix(vec3<f32>(1f, 1f, 1f), clamp(_e106, vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(2f, 2f, 2f)), vec3(0.12f))) + (_e111 * 0.08f));
    let _e114 = (*authored);
    let _e115 = relit;
    let _e116 = (*blend);
    return mix(_e114, _e115, vec3(_e116));
}

fn skyDirection_u0028_vf2_u003b(screenUv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var worldH: vec4<f32>;
    var world: vec3<f32>;

    let _e73 = p.invViewProjection;
    let _e74 = (*screenUv);
    let _e77 = ((_e74 * 2f) - vec2(1f));
    worldH = (_e73 * vec4<f32>(_e77.x, _e77.y, 0f, 1f));
    let _e82 = worldH;
    let _e85 = worldH[3u];
    world = (_e82.xyz / vec3(max(abs(_e85), 0.000001f)));
    let _e90 = world;
    let _e92 = p.eyeSkyControl;
    return normalize((_e90 - _e92.xyz));
}

fn main_1() {
    var pixel: vec2<u32>;
    var uv: vec2<f32>;
    var scene: vec3<f32>;
    var depth: f32;
    var local_2: f32;
    var direction_1: vec3<f32>;
    var param_1: vec2<f32>;
    var physicalSky: vec3<f32>;
    var param_2: vec3<f32>;
    var param_3: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: f32;
    var worldH_1: vec4<f32>;
    var world_1: vec3<f32>;
    var distanceToEye: f32;
    var heightWeight: f32;
    var transmittance: f32;
    var inscatter: vec3<f32>;
    var param_6: vec3<f32>;
    var param_7: vec3<f32>;
    var phi_400_: bool;

    let _e89 = gl_GlobalInvocationID_1;
    pixel = _e89.xy;
    let _e91 = pixel;
    let _e93 = p.outputGrid;
    if any((_e91 >= _e93.xy)) {
        return;
    }
    let _e97 = pixel;
    let _e102 = p.outputGrid;
    uv = ((vec2<f32>(_e97) + vec2(0.5f)) / vec2<f32>(_e102.xy));
    let _e106 = uv;
    let _e107 = textureSampleLevel(sceneHdr, sceneHdr_sampler, _e106, 0f);
    scene = _e107.xyz;
    let _e111 = p.depthRange[2u];
    if (_e111 > 0.5f) {
        let _e113 = uv;
        let _e114 = textureSampleLevel(sceneDepth, sceneDepth_sampler, _e113, 0f);
        local_2 = _e114.x;
    } else {
        local_2 = 1f;
    }
    let _e116 = local_2;
    depth = _e116;
    let _e117 = depth;
    if (_e117 <= 0f) {
        let _e119 = uv;
        param_1 = _e119;
        let _e120 = skyDirection_u0028_vf2_u003b((&param_1));
        direction_1 = _e120;
        let _e123 = p.eyeSkyControl[3u];
        if (_e123 > 0f) {
            let _e125 = direction_1;
            param_2 = _e125;
            let _e126 = skyViewUv_u0028_vf3_u003b((&param_2));
            let _e127 = textureSampleLevel(skyViewLut, skyViewLut_sampler, _e126, 0f);
            physicalSky = _e127.xyz;
            let _e131 = p.eyeSkyControl[3u];
            let _e133 = scene;
            param_3 = _e133;
            let _e134 = physicalSky;
            param_4 = _e134;
            param_5 = clamp(_e131, 0f, 1f);
            let _e135 = wiredComposeAuthoredPhysicalSky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_3), (&param_4), (&param_5));
            scene = _e135;
        }
    } else {
        let _e138 = p.depthRange[2u];
        let _e139 = (_e138 > 0.5f);
        phi_400_ = _e139;
        if _e139 {
            let _e142 = p.mediaColorDensity[3u];
            phi_400_ = (_e142 > 0f);
        }
        let _e145 = phi_400_;
        if _e145 {
            let _e147 = p.invViewProjection;
            let _e148 = uv;
            let _e151 = ((_e148 * 2f) - vec2(1f));
            let _e152 = depth;
            worldH_1 = (_e147 * vec4<f32>(_e151.x, _e151.y, _e152, 1f));
            let _e158 = worldH_1[3u];
            if (abs(_e158) > 0.000001f) {
                let _e161 = worldH_1;
                let _e164 = worldH_1[3u];
                world_1 = (_e161.xyz / vec3(_e164));
                let _e167 = world_1;
                let _e169 = p.eyeSkyControl;
                distanceToEye = length((_e167 - _e169.xyz));
                let _e175 = p.mediaHeight[1u];
                let _e178 = world_1[2u];
                let _e181 = p.mediaHeight[0u];
                heightWeight = exp((-(_e175) * max((_e178 - _e181), 0f)));
                let _e188 = p.mediaColorDensity[3u];
                let _e190 = heightWeight;
                let _e192 = distanceToEye;
                transmittance = exp(((-(_e188) * _e190) * _e192));
                let _e195 = world_1;
                let _e197 = p.eyeSkyControl;
                let _e202 = p.mediaColorDensity;
                param_6 = _e202.xyz;
                param_7 = normalize((_e195 - _e197.xyz));
                let _e204 = wiredAerialInscatter_u0028_vf3_u003b_vf3_u003b((&param_6), (&param_7));
                inscatter = _e204;
                let _e205 = inscatter;
                let _e206 = scene;
                let _e207 = transmittance;
                scene = mix(_e205, _e206, vec3(clamp(_e207, 0f, 1f)));
            }
        }
    }
    let _e211 = pixel;
    let _e213 = scene;
    textureStore(composedHdr, bitcast<vec2<i32>>(_e211), vec4<f32>(_e213.x, _e213.y, _e213.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
