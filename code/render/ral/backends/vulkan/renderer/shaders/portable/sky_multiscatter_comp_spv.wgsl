struct SkyParams {
    sunDirectionAngularRadius: vec4<f32>,
    sunRadiance: vec4<f32>,
    moonDirectionAngularRadius: vec4<f32>,
    moonRadiance: vec4<f32>,
    atmosphereRadii: vec4<f32>,
    scatteringScale: vec4<f32>,
    groundExposure: vec4<f32>,
}

var<private> gl_GlobalInvocationID_1: vec3<u32>;
@group(0) @binding(2)
var multiscatterLut: texture_storage_2d<rgba16float,write>;
@group(0) @binding(1)
var transmittanceLut: texture_2d<f32>;
@group(0) @binding(33)
var transmittanceLut_sampler: sampler;
@group(0) @binding(0)
var<uniform> sky: SkyParams;

fn main_1() {
    var coord: vec2<i32>;
    var extent: vec2<i32>;
    var uv: vec2<f32>;
    var accumulated: vec3<f32>;
    var i: i32;
    var u: f32;
    var sampleUv: vec2<f32>;
    var transmittance: vec3<f32>;
    var energy: vec3<f32>;

    let _e34 = gl_GlobalInvocationID_1;
    coord = bitcast<vec2<i32>>(_e34.xy);
    let _e37 = textureDimensions(multiscatterLut);
    extent = vec2<i32>(_e37);
    let _e39 = coord;
    let _e40 = extent;
    if any((_e39 >= _e40)) {
        return;
    }
    let _e43 = coord;
    let _e47 = extent;
    uv = ((vec2<f32>(_e43) + vec2(0.5f)) / vec2<f32>(_e47));
    accumulated = vec3<f32>(0f, 0f, 0f);
    i = 0i;
    loop {
        let _e50 = i;
        if (_e50 < 16i) {
            let _e52 = i;
            u = ((f32(_e52) + 0.5f) / 16f);
            let _e56 = u;
            let _e58 = uv[0u];
            let _e62 = uv[1u];
            sampleUv = vec2<f32>(fract((_e56 + _e58)), _e62);
            let _e64 = sampleUv;
            let _e65 = textureSampleLevel(transmittanceLut, transmittanceLut_sampler, _e64, 0f);
            transmittance = _e65.xyz;
            let _e67 = transmittance;
            let _e69 = transmittance;
            let _e71 = accumulated;
            accumulated = (_e71 + ((vec3<f32>(1f, 1f, 1f) - _e67) * _e69));
            continue;
        } else {
            break;
        }
        continuing {
            let _e73 = i;
            i = (_e73 + 1i);
        }
    }
    let _e75 = accumulated;
    energy = (_e75 / vec3(16f));
    let _e79 = sky.sunRadiance;
    let _e82 = energy;
    energy = (_e82 * max(_e79.xyz, vec3<f32>(0.001f, 0.001f, 0.001f)));
    let _e86 = sky.groundExposure[3u];
    let _e90 = energy;
    energy = (_e90 * (0.35f + (0.65f * clamp(_e86, 0f, 4f))));
    let _e92 = coord;
    let _e93 = energy;
    let _e94 = max(_e93, vec3<f32>(0f, 0f, 0f));
    textureStore(multiscatterLut, _e92, vec4<f32>(_e94.x, _e94.y, _e94.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
