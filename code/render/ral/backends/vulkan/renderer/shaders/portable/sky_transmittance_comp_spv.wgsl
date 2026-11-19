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
@group(0) @binding(1)
var transmittanceLut: texture_storage_2d<rgba16float,write>;
@group(0) @binding(0)
var<uniform> sky: SkyParams;

fn main_1() {
    var coord: vec2<i32>;
    var extent: vec2<i32>;
    var uv: vec2<f32>;
    var altitude: f32;
    var mu: f32;
    var horizon: f32;
    var airMass: f32;
    var rayleigh: vec3<f32>;
    var mie: vec3<f32>;
    var absorption: vec3<f32>;
    var transmittance: vec3<f32>;

    let _e40 = gl_GlobalInvocationID_1;
    coord = bitcast<vec2<i32>>(_e40.xy);
    let _e43 = textureDimensions(transmittanceLut);
    extent = vec2<i32>(_e43);
    let _e45 = coord;
    let _e46 = extent;
    if any((_e45 >= _e46)) {
        return;
    }
    let _e49 = coord;
    let _e53 = extent;
    uv = ((vec2<f32>(_e49) + vec2(0.5f)) / vec2<f32>(_e53));
    let _e57 = uv[1u];
    altitude = _e57;
    let _e59 = uv[0u];
    mu = ((_e59 * 2f) - 1f);
    let _e62 = mu;
    let _e63 = mu;
    horizon = sqrt(max((1f - (_e62 * _e63)), 0f));
    let _e68 = mu;
    airMass = mix(32f, 1f, clamp(((_e68 * 0.5f) + 0.5f), 0f, 1f));
    let _e73 = altitude;
    let _e75 = airMass;
    airMass = (_e75 * mix(1f, 0.035f, _e73));
    let _e77 = horizon;
    let _e79 = airMass;
    airMass = (_e79 * mix(1f, 1.8f, _e77));
    let _e83 = sky.scatteringScale[0u];
    rayleigh = (vec3<f32>(0.005802f, 0.013558f, 0.0331f) * max(_e83, 0.001f));
    let _e88 = sky.scatteringScale[1u];
    mie = (vec3<f32>(0.003996f, 0.003996f, 0.003996f) * max(_e88, 0.001f));
    let _e93 = sky.scatteringScale[2u];
    absorption = (vec3<f32>(0.00065f, 0.001881f, 0.000085f) * max(_e93, 0.001f));
    let _e96 = rayleigh;
    let _e97 = mie;
    let _e99 = absorption;
    let _e102 = airMass;
    transmittance = exp((-(((_e96 + _e97) + _e99)) * _e102));
    let _e105 = coord;
    let _e106 = transmittance;
    let _e107 = clamp(_e106, vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f));
    let _e108 = airMass;
    textureStore(transmittanceLut, _e105, vec4<f32>(_e107.x, _e107.y, _e107.z, _e108));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
