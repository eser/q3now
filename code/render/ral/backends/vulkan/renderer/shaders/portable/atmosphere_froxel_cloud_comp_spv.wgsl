struct Params {
    invViewProjection: mat4x4<f32>,
    eyeNear: vec4<f32>,
    farTimeCoverageShadow: vec4<f32>,
    windDensity: vec4<f32>,
    layerErosion: vec4<f32>,
    sunDirectionIntensity: vec4<f32>,
    sunColorLightning: vec4<f32>,
    ambientColor: vec4<f32>,
    grid: vec4<u32>,
}

struct LitMedia {
    sourceRadianceExtinction: array<vec4<f32>>,
}

struct CloudMedia {
    cloudRadianceExtinction: array<vec4<f32>>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(0)
var<uniform> p: Params;
@group(0) @binding(1)
var<storage> unnamed: LitMedia;
@group(0) @binding(2)
var<storage, read_write> unnamed_1: CloudMedia;

fn hash13_u0028_vf3_u003b(q: ptr<function, vec3<f32>>) -> f32 {
    let _e51 = (*q);
    (*q) = fract((_e51 * 0.1031f));
    let _e54 = (*q);
    let _e55 = (*q);
    let _e60 = (*q);
    (*q) = (_e60 + vec3(dot(_e54, (_e55.yzx + vec3(33.33f)))));
    let _e64 = (*q)[0u];
    let _e66 = (*q)[1u];
    let _e69 = (*q)[2u];
    return fract(((_e64 + _e66) * _e69));
}

fn valueNoise_u0028_vf3_u003b(x: ptr<function, vec3<f32>>) -> f32 {
    var cell: vec3<f32>;
    var f: vec3<f32>;
    var n000_: f32;
    var param: vec3<f32>;
    var n100_: f32;
    var param_1: vec3<f32>;
    var n010_: f32;
    var param_2: vec3<f32>;
    var n110_: f32;
    var param_3: vec3<f32>;
    var n001_: f32;
    var param_4: vec3<f32>;
    var n101_: f32;
    var param_5: vec3<f32>;
    var n011_: f32;
    var param_6: vec3<f32>;
    var n111_: f32;
    var param_7: vec3<f32>;

    let _e69 = (*x);
    cell = floor(_e69);
    let _e71 = (*x);
    f = fract(_e71);
    let _e73 = f;
    let _e74 = f;
    let _e76 = f;
    f = ((_e73 * _e74) * (vec3(3f) - (_e76 * 2f)));
    let _e81 = cell;
    param = (_e81 + vec3<f32>(0f, 0f, 0f));
    let _e83 = hash13_u0028_vf3_u003b((&param));
    n000_ = _e83;
    let _e84 = cell;
    param_1 = (_e84 + vec3<f32>(1f, 0f, 0f));
    let _e86 = hash13_u0028_vf3_u003b((&param_1));
    n100_ = _e86;
    let _e87 = cell;
    param_2 = (_e87 + vec3<f32>(0f, 1f, 0f));
    let _e89 = hash13_u0028_vf3_u003b((&param_2));
    n010_ = _e89;
    let _e90 = cell;
    param_3 = (_e90 + vec3<f32>(1f, 1f, 0f));
    let _e92 = hash13_u0028_vf3_u003b((&param_3));
    n110_ = _e92;
    let _e93 = cell;
    param_4 = (_e93 + vec3<f32>(0f, 0f, 1f));
    let _e95 = hash13_u0028_vf3_u003b((&param_4));
    n001_ = _e95;
    let _e96 = cell;
    param_5 = (_e96 + vec3<f32>(1f, 0f, 1f));
    let _e98 = hash13_u0028_vf3_u003b((&param_5));
    n101_ = _e98;
    let _e99 = cell;
    param_6 = (_e99 + vec3<f32>(0f, 1f, 1f));
    let _e101 = hash13_u0028_vf3_u003b((&param_6));
    n011_ = _e101;
    let _e102 = cell;
    param_7 = (_e102 + vec3<f32>(1f, 1f, 1f));
    let _e104 = hash13_u0028_vf3_u003b((&param_7));
    n111_ = _e104;
    let _e105 = n000_;
    let _e106 = n100_;
    let _e108 = f[0u];
    let _e110 = n010_;
    let _e111 = n110_;
    let _e113 = f[0u];
    let _e116 = f[1u];
    let _e118 = n001_;
    let _e119 = n101_;
    let _e121 = f[0u];
    let _e123 = n011_;
    let _e124 = n111_;
    let _e126 = f[0u];
    let _e129 = f[1u];
    let _e132 = f[2u];
    return mix(mix(mix(_e105, _e106, _e108), mix(_e110, _e111, _e113), _e116), mix(mix(_e118, _e119, _e121), mix(_e123, _e124, _e126), _e129), _e132);
}

fn main_1() {
    var c: vec3<u32>;
    var dims: vec3<u32>;
    var index: u32;
    var source: vec4<f32>;
    var cover: f32;
    var uv: vec2<f32>;
    var slice: f32;
    var nearZ: f32;
    var farZ: f32;
    var distanceZ: f32;
    var worldH: vec4<f32>;
    var world: vec3<f32>;
    var base: f32;
    var top: f32;
    var edge: f32;
    var envelope: f32;
    var advected: vec3<f32>;
    var scale: f32;
    var shape: f32;
    var param_8: vec3<f32>;
    var threshold: f32;
    var density: f32;
    var extinction: f32;
    var viewVector: vec3<f32>;
    var viewLength: f32;
    var viewDir: vec3<f32>;
    var local: vec3<f32>;
    var sunVector: vec3<f32>;
    var sunLength: f32;
    var sunDir: vec3<f32>;
    var local_1: vec3<f32>;
    var forward: f32;
    var selfShadow: f32;
    var illumination: vec3<f32>;
    var cloudRadiance: vec3<f32>;

    let _e85 = gl_GlobalInvocationID_1;
    c = _e85;
    let _e87 = p.grid;
    dims = _e87.xyz;
    let _e89 = c;
    let _e90 = dims;
    if any((_e89 >= _e90)) {
        return;
    }
    let _e94 = c[2u];
    let _e96 = dims[1u];
    let _e99 = c[1u];
    let _e102 = dims[0u];
    let _e105 = c[0u];
    index = ((((_e94 * _e96) + _e99) * _e102) + _e105);
    let _e107 = index;
    let _e110 = unnamed.sourceRadianceExtinction[_e107];
    source = _e110;
    let _e113 = p.farTimeCoverageShadow[2u];
    cover = clamp(_e113, 0f, 1f);
    let _e115 = cover;
    if (_e115 <= 0f) {
        let _e117 = index;
        let _e118 = source;
        unnamed_1.cloudRadianceExtinction[_e117] = _e118;
        return;
    }
    let _e121 = c;
    let _e126 = dims;
    uv = ((vec2<f32>(_e121.xy) + vec2(0.5f)) / vec2<f32>(_e126.xy));
    let _e131 = c[2u];
    let _e135 = dims[2u];
    slice = ((f32(_e131) + 0.5f) / f32(_e135));
    let _e140 = p.eyeNear[3u];
    nearZ = max(_e140, 0.01f);
    let _e144 = p.farTimeCoverageShadow[0u];
    let _e145 = nearZ;
    farZ = max(_e144, (_e145 + 0.01f));
    let _e148 = nearZ;
    let _e149 = farZ;
    let _e150 = nearZ;
    let _e152 = slice;
    distanceZ = (_e148 * pow((_e149 / _e150), _e152));
    let _e156 = p.invViewProjection;
    let _e157 = uv;
    let _e160 = ((_e157 * 2f) - vec2(1f));
    let _e161 = nearZ;
    let _e162 = distanceZ;
    worldH = (_e156 * vec4<f32>(_e160.x, _e160.y, (1f - (_e161 / _e162)), 1f));
    let _e169 = worldH;
    let _e172 = worldH[3u];
    world = (_e169.xyz / vec3(max(abs(_e172), 0.000001f)));
    let _e179 = p.layerErosion[0u];
    base = _e179;
    let _e182 = p.layerErosion[1u];
    let _e183 = base;
    top = max(_e182, (_e183 + 1f));
    let _e186 = top;
    let _e187 = base;
    let _e191 = p.layerErosion[2u];
    edge = max(((_e186 - _e187) * clamp(_e191, 0.02f, 0.45f)), 1f);
    let _e195 = base;
    let _e196 = base;
    let _e197 = edge;
    let _e200 = world[2u];
    let _e202 = top;
    let _e203 = edge;
    let _e205 = top;
    let _e207 = world[2u];
    envelope = (smoothstep(_e195, (_e196 + _e197), _e200) * (1f - smoothstep((_e202 - _e203), _e205, _e207)));
    let _e211 = envelope;
    if (_e211 <= 0f) {
        let _e213 = index;
        let _e214 = source;
        unnamed_1.cloudRadianceExtinction[_e213] = _e214;
        return;
    }
    let _e217 = world;
    let _e219 = p.windDensity;
    let _e223 = p.farTimeCoverageShadow[1u];
    advected = (_e217 + (_e219.xyz * _e223));
    let _e226 = top;
    let _e227 = base;
    scale = max(((_e226 - _e227) * 0.32f), 48f);
    let _e231 = advected;
    let _e232 = scale;
    param_8 = (_e231 / vec3(_e232));
    let _e235 = valueNoise_u0028_vf3_u003b((&param_8));
    shape = _e235;
    let _e236 = cover;
    threshold = mix(0.82f, 0.3f, _e236);
    let _e238 = threshold;
    let _e239 = threshold;
    let _e242 = p.layerErosion[3u];
    let _e246 = shape;
    let _e248 = envelope;
    density = (smoothstep(_e238, min((_e239 + max(_e242, 0.08f)), 1f), _e246) * _e248);
    let _e250 = density;
    let _e253 = p.windDensity[3u];
    extinction = (_e250 * max(_e253, 0f));
    let _e256 = world;
    let _e258 = p.eyeNear;
    viewVector = (_e256 - _e258.xyz);
    let _e261 = viewVector;
    viewLength = length(_e261);
    let _e263 = viewLength;
    if (_e263 > 0.00001f) {
        let _e265 = viewVector;
        let _e266 = viewLength;
        local = (_e265 / vec3(_e266));
    } else {
        local = vec3<f32>(0f, 0f, 1f);
    }
    let _e269 = local;
    viewDir = _e269;
    let _e271 = p.sunDirectionIntensity;
    sunVector = _e271.xyz;
    let _e273 = sunVector;
    sunLength = length(_e273);
    let _e275 = sunLength;
    if (_e275 > 0.00001f) {
        let _e277 = sunVector;
        let _e278 = sunLength;
        local_1 = (_e277 / vec3(_e278));
    } else {
        local_1 = vec3<f32>(0f, 0f, 1f);
    }
    let _e281 = local_1;
    sunDir = _e281;
    let _e282 = viewDir;
    let _e283 = sunDir;
    forward = pow(max(dot(_e282, _e283), 0f), 4f);
    let _e287 = extinction;
    let _e289 = top;
    let _e290 = base;
    selfShadow = exp(((-(_e287) * (_e289 - _e290)) * 0.18f));
    let _e296 = p.ambientColor;
    let _e299 = p.sunColorLightning;
    let _e303 = p.sunDirectionIntensity[3u];
    let _e305 = selfShadow;
    let _e307 = forward;
    let _e312 = p.farTimeCoverageShadow[3u];
    illumination = (_e296.xyz + ((((_e299.xyz * _e303) * _e305) * mix(0.35f, 1f, _e307)) * (1f - clamp(_e312, 0f, 1f))));
    let _e318 = p.sunColorLightning;
    let _e320 = illumination;
    illumination = (_e320 + _e318.www);
    let _e322 = extinction;
    let _e324 = illumination;
    cloudRadiance = ((vec3<f32>(0.92f, 0.95f, 1f) * _e322) * _e324);
    let _e326 = index;
    let _e327 = source;
    let _e328 = cloudRadiance;
    let _e329 = extinction;
    unnamed_1.cloudRadianceExtinction[_e326] = (_e327 + vec4<f32>(_e328.x, _e328.y, _e328.z, _e329));
    return;
}

@compute @workgroup_size(4, 4, 4)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
