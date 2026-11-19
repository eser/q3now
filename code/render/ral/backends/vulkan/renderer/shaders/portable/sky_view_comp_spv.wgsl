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
@group(0) @binding(3)
var skyViewLut: texture_storage_2d<rgba16float,write>;
@group(0) @binding(0)
var<uniform> sky: SkyParams;
@group(0) @binding(1)
var transmittanceLut: texture_2d<f32>;
@group(0) @binding(33)
var transmittanceLut_sampler: sampler;
@group(0) @binding(2)
var multiscatterLut: texture_2d<f32>;
@group(0) @binding(34)
var multiscatterLut_sampler: sampler;

fn miePhase_u0028_f1_u003b_f1_u003b(cosTheta: ptr<function, f32>, g: ptr<function, f32>) -> f32 {
    var g2_: f32;
    var denom: f32;

    let _e82 = (*g);
    let _e83 = (*g);
    g2_ = (_e82 * _e83);
    let _e85 = g2_;
    let _e87 = (*g);
    let _e89 = (*cosTheta);
    denom = max(((1f + _e85) - ((2f * _e87) * _e89)), 0.001f);
    let _e93 = g2_;
    let _e95 = denom;
    let _e97 = denom;
    return ((1f - _e93) / ((12.566371f * _e95) * sqrt(_e97)));
}

fn rayleighPhase_u0028_f1_u003b(cosTheta_1: ptr<function, f32>) -> f32 {
    let _e79 = (*cosTheta_1);
    let _e80 = (*cosTheta_1);
    return ((3f * (1f + (_e79 * _e80))) / 50.265484f);
}

fn main_1() {
    var coord: vec2<i32>;
    var extent: vec2<i32>;
    var uv: vec2<f32>;
    var azimuth: f32;
    var elevation: f32;
    var viewDir: vec3<f32>;
    var sunDir: vec3<f32>;
    var cosTheta_2: f32;
    var transUv: vec2<f32>;
    var transmittance: vec3<f32>;
    var multi: vec3<f32>;
    var betaRayleigh: vec3<f32>;
    var param: f32;
    var betaMie: vec3<f32>;
    var param_1: f32;
    var param_2: f32;
    var sunHeight: f32;
    var twilight: f32;
    var aboveHorizon: f32;
    var elevationMix: f32;
    var zenithTint: vec3<f32>;
    var horizonTint: vec3<f32>;
    var groundTint: vec3<f32>;
    var directionalSky: vec3<f32>;
    var sunEnergy: f32;
    var radiance: vec3<f32>;
    var angularRadius: f32;
    var resolvedRadius: f32;
    var sunDisk: f32;
    var sunCorona: f32;
    var moonDir: vec3<f32>;
    var moonTheta: f32;
    var moonRadius: f32;
    var moonDisk: f32;

    let _e112 = gl_GlobalInvocationID_1;
    coord = bitcast<vec2<i32>>(_e112.xy);
    let _e115 = textureDimensions(skyViewLut);
    extent = vec2<i32>(_e115);
    let _e117 = coord;
    let _e118 = extent;
    if any((_e117 >= _e118)) {
        return;
    }
    let _e121 = coord;
    let _e125 = extent;
    uv = ((vec2<f32>(_e121) + vec2(0.5f)) / vec2<f32>(_e125));
    let _e129 = uv[0u];
    azimuth = (((_e129 * 2f) - 1f) * 3.1415927f);
    let _e134 = uv[1u];
    elevation = ((_e134 - 0.5f) * 3.1415927f);
    let _e137 = elevation;
    let _e139 = azimuth;
    let _e142 = elevation;
    let _e144 = azimuth;
    let _e147 = elevation;
    viewDir = normalize(vec3<f32>((cos(_e137) * cos(_e139)), (cos(_e142) * sin(_e144)), sin(_e147)));
    let _e152 = sky.sunDirectionAngularRadius;
    sunDir = normalize(_e152.xyz);
    let _e155 = viewDir;
    let _e156 = sunDir;
    cosTheta_2 = clamp(dot(_e155, _e156), -1f, 1f);
    let _e160 = viewDir[2u];
    let _e165 = sky.atmosphereRadii[2u];
    transUv = vec2<f32>(((_e160 * 0.5f) + 0.5f), clamp(_e165, 0f, 1f));
    let _e168 = transUv;
    let _e169 = textureSampleLevel(transmittanceLut, transmittanceLut_sampler, _e168, 0f);
    transmittance = _e169.xyz;
    let _e171 = cosTheta_2;
    let _e175 = transUv[1u];
    let _e177 = textureSampleLevel(multiscatterLut, multiscatterLut_sampler, vec2<f32>(((_e171 * 0.5f) + 0.5f), _e175), 0f);
    multi = _e177.xyz;
    let _e179 = cosTheta_2;
    param = _e179;
    let _e180 = rayleighPhase_u0028_f1_u003b((&param));
    betaRayleigh = (vec3<f32>(0.18f, 0.34f, 0.72f) * _e180);
    let _e184 = sky.scatteringScale[3u];
    let _e186 = cosTheta_2;
    param_1 = _e186;
    param_2 = clamp(_e184, 0f, 0.95f);
    let _e187 = miePhase_u0028_f1_u003b_f1_u003b((&param_1), (&param_2));
    betaMie = (vec3<f32>(0.42f, 0.34f, 0.24f) * _e187);
    let _e190 = sunDir[2u];
    sunHeight = clamp(((_e190 * 0.5f) + 0.5f), 0f, 1f);
    let _e194 = sunHeight;
    twilight = (1f - smoothstep(0.42f, 0.62f, _e194));
    let _e198 = viewDir[2u];
    aboveHorizon = smoothstep(-0.08f, 0.04f, _e198);
    let _e201 = viewDir[2u];
    elevationMix = pow(clamp(_e201, 0f, 1f), 0.35f);
    let _e204 = sunHeight;
    zenithTint = mix(vec3<f32>(0.025f, 0.035f, 0.1f), vec3<f32>(0.035f, 0.16f, 0.46f), vec3(_e204));
    let _e207 = twilight;
    horizonTint = mix(vec3<f32>(0.3f, 0.52f, 0.75f), vec3<f32>(0.95f, 0.24f, 0.055f), vec3(_e207));
    let _e211 = sky.groundExposure;
    groundTint = (max(_e211.xyz, vec3<f32>(0.015f, 0.015f, 0.015f)) * 0.22f);
    let _e215 = horizonTint;
    let _e216 = zenithTint;
    let _e217 = elevationMix;
    directionalSky = mix(_e215, _e216, vec3(_e217));
    let _e220 = groundTint;
    let _e221 = directionalSky;
    let _e222 = aboveHorizon;
    directionalSky = mix(_e220, _e221, vec3(_e222));
    let _e226 = sky.sunRadiance;
    sunEnergy = dot(_e226.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e229 = directionalSky;
    let _e230 = sunEnergy;
    radiance = (_e229 * (0.22f + (0.48f * max(_e230, 0f))));
    let _e236 = sky.sunRadiance;
    let _e238 = betaRayleigh;
    let _e239 = betaMie;
    let _e242 = transmittance;
    let _e246 = radiance;
    radiance = (_e246 + (((_e236.xyz * (_e238 + _e239)) * (vec3<f32>(1f, 1f, 1f) - _e242)) * 0.45f));
    let _e248 = multi;
    let _e250 = radiance;
    radiance = (_e250 + (_e248 * 0.12f));
    let _e254 = sky.sunDirectionAngularRadius[3u];
    angularRadius = max(_e254, 0.0001f);
    let _e256 = angularRadius;
    resolvedRadius = max(_e256, 0.006f);
    let _e258 = resolvedRadius;
    let _e261 = resolvedRadius;
    let _e263 = cosTheta_2;
    sunDisk = smoothstep(cos((_e258 * 1.8f)), cos(_e261), _e263);
    let _e265 = cosTheta_2;
    let _e269 = cosTheta_2;
    sunCorona = ((pow(max(_e265, 0f), 256f) * 0.55f) + (pow(max(_e269, 0f), 24f) * 0.06f));
    let _e275 = sky.sunRadiance;
    let _e277 = transmittance;
    let _e279 = sunDisk;
    let _e281 = sunCorona;
    let _e284 = radiance;
    radiance = (_e284 + ((_e275.xyz * _e277) * ((_e279 * 7f) + _e281)));
    let _e287 = sky.moonDirectionAngularRadius;
    moonDir = normalize(_e287.xyz);
    let _e290 = viewDir;
    let _e291 = moonDir;
    moonTheta = clamp(dot(_e290, _e291), -1f, 1f);
    let _e296 = sky.moonDirectionAngularRadius[3u];
    moonRadius = max(_e296, 0.009f);
    let _e298 = moonRadius;
    let _e301 = moonRadius;
    let _e303 = moonTheta;
    moonDisk = smoothstep(cos((_e298 * 1.6f)), cos(_e301), _e303);
    let _e306 = sky.moonRadiance;
    let _e308 = moonDisk;
    let _e310 = moonTheta;
    let _e316 = radiance;
    radiance = (_e316 + (_e306.xyz * ((_e308 * 3f) + (pow(max(_e310, 0f), 96f) * 0.1f))));
    let _e320 = sky.groundExposure[3u];
    let _e322 = radiance;
    radiance = (_e322 * max(_e320, 0f));
    let _e324 = coord;
    let _e325 = radiance;
    let _e326 = max(_e325, vec3<f32>(0f, 0f, 0f));
    textureStore(skyViewLut, _e324, vec4<f32>(_e326.x, _e326.y, _e326.z, 1f));
    return;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID: vec3<u32>) {
    gl_GlobalInvocationID_1 = gl_GlobalInvocationID;
    main_1();
}
