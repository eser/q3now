struct ProductDraw {
    viewOriginDrawSpace: vec4<f32>,
    viewForward: vec4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    projectionLightmap: vec4<f32>,
    viewportAlpha: vec4<f32>,
    atmosphereEyeDensity: vec4<f32>,
    atmosphereColorVisibility: vec4<f32>,
    atmosphereHeightSky: vec4<f32>,
    atmosphereFroxelGrid: vec4<u32>,
    localSh: array<vec4<f32>, 4>,
    staticLighting: vec4<u32>,
    emissiveRadiance: vec4<f32>,
}

struct AtmosphereFroxels {
    values: array<vec4<f32>>,
}

struct ProductMaterialFactors {
    baseColor: vec4<f32>,
    factors: vec4<f32>,
    beamStartColor: vec4<f32>,
    beamEndColor: vec4<f32>,
    beamScaleOffset: vec4<f32>,
    beamFlags: vec4<u32>,
}

struct FragmentOutput {
    @location(1) member: vec4<f32>,
    @location(0) member_1: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> productDraw: ProductDraw;
@group(1) @binding(10)
var physicalSkyTexture: texture_2d<f32>;
@group(1) @binding(11)
var physicalSkySampler: sampler;
@group(1) @binding(12)
var currentSkyRadianceTexture: texture_2d_array<f32>;
@group(1) @binding(13)
var currentSkyRadianceSampler: sampler;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> worldPosition_1: vec3<f32>;
@group(2) @binding(0)
var<storage> atmosphereFroxels: AtmosphereFroxels;
@group(2) @binding(1)
var<uniform> productMaterial: ProductMaterialFactors;
var<private> outCoverage: vec4<f32>;
@group(1) @binding(0)
var baseTexture: texture_2d<f32>;
@group(1) @binding(1)
var baseSampler: sampler;
var<private> texCoord_1: vec2<f32>;
var<private> color_1: vec4<f32>;
var<private> outColor: vec4<f32>;
@group(1) @binding(14)
var previousSkyRadianceTexture: texture_2d_array<f32>;
@group(1) @binding(15)
var previousSkyRadianceSampler: sampler;
var<private> worldNormal_1: vec3<f32>;
var<private> lightmapCoord_1: vec2<f32>;
@group(1) @binding(4)
var staticRadianceTexture: texture_2d_array<f32>;
@group(1) @binding(5)
var staticRadianceSampler: sampler;
@group(1) @binding(6)
var staticDirectionTexture: texture_2d_array<f32>;
@group(1) @binding(7)
var staticDirectionSampler: sampler;
@group(1) @binding(8)
var staticVisibilityTexture: texture_2d_array<f32>;
@group(1) @binding(9)
var staticVisibilitySampler: sampler;
@group(1) @binding(2)
var lightmapTexture: texture_2d<f32>;
@group(1) @binding(3)
var lightmapSampler: sampler;

fn wired_lighting_decode_oct_u0028_vf2_u003b(encoded: ptr<function, vec2<f32>>) -> vec3<f32> {
    var f: vec2<f32>;
    var n: vec3<f32>;
    var folded: vec2<f32>;

    let _e105 = (*encoded);
    f = ((_e105 * 2f) - vec2(1f));
    let _e109 = f;
    let _e111 = f[0u];
    let _e115 = f[1u];
    n = vec3<f32>(_e109.x, _e109.y, ((1f - abs(_e111)) - abs(_e115)));
    let _e122 = n[2u];
    if (_e122 < 0f) {
        let _e124 = n;
        let _e129 = n;
        folded = ((vec2(1f) - abs(_e124.yx)) * sign(_e129.xy));
        let _e133 = folded;
        n[0u] = _e133.x;
        n[1u] = _e133.y;
    }
    let _e138 = n;
    return normalize(_e138);
}

fn wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b(radiance: ptr<function, vec3<f32>>, encodedDirection: ptr<function, vec2<f32>>, materialNormal: ptr<function, vec3<f32>>) -> vec3<f32> {
    var dominantDirection: vec3<f32>;
    var param: vec2<f32>;

    let _e106 = (*encodedDirection);
    param = _e106;
    let _e107 = wired_lighting_decode_oct_u0028_vf2_u003b((&param));
    dominantDirection = _e107;
    let _e108 = (*radiance);
    let _e110 = (*materialNormal);
    let _e112 = dominantDirection;
    return (max(_e108, vec3<f32>(0f, 0f, 0f)) * max(dot(normalize(_e110), _e112), 0f));
}

fn wired_display_visibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_274_: bool;
    var phi_281_: bool;

    let _e105 = (*sceneColor);
    let _e109 = productDraw.viewForward[3u];
    exposed = (max(_e105, vec3<f32>(0f, 0f, 0f)) * _e109);
    let _e111 = exposed;
    luminance = dot(_e111, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e113 = luminance;
    let _e114 = (_e113 > 0f);
    phi_274_ = _e114;
    if _e114 {
        let _e115 = luminance;
        let _e118 = productDraw.viewUp[3u];
        phi_274_ = (_e115 < _e118);
    }
    let _e121 = phi_274_;
    phi_281_ = _e121;
    if _e121 {
        let _e124 = productDraw.viewLeft[3u];
        phi_281_ = (_e124 != 1f);
    }
    let _e127 = phi_281_;
    if _e127 {
        let _e128 = luminance;
        let _e131 = productDraw.viewUp[3u];
        let _e135 = productDraw.viewLeft[3u];
        let _e139 = productDraw.viewUp[3u];
        curved = (pow((_e128 / _e131), _e135) * _e139);
        let _e141 = curved;
        let _e142 = luminance;
        let _e144 = exposed;
        exposed = (_e144 * (_e141 / _e142));
    }
    let _e146 = exposed;
    return _e146;
}

fn wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b(authored: ptr<function, vec3<f32>>, physical: ptr<function, vec3<f32>>, blend: ptr<function, f32>) -> vec3<f32> {
    var authoredEnergy: f32;
    var physicalEnergy: f32;
    var scale: f32;
    var physicalTint: vec3<f32>;
    var local: vec3<f32>;
    var relit: vec3<f32>;

    let _e110 = (*authored);
    authoredEnergy = dot(max(_e110, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e113 = (*physical);
    physicalEnergy = dot(max(_e113, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e116 = authoredEnergy;
    if (_e116 <= 0.00001f) {
        let _e118 = (*authored);
        let _e119 = (*physical);
        let _e120 = (*blend);
        return mix(_e118, _e119, vec3(_e120));
    }
    let _e123 = physicalEnergy;
    let _e124 = authoredEnergy;
    scale = clamp((_e123 / max(_e124, 0.18f)), 0.65f, 1.75f);
    let _e128 = physicalEnergy;
    if (_e128 > 0.00001f) {
        let _e130 = (*physical);
        let _e131 = physicalEnergy;
        local = (_e130 / vec3(_e131));
    } else {
        local = vec3<f32>(1f, 1f, 1f);
    }
    let _e134 = local;
    physicalTint = _e134;
    let _e135 = (*authored);
    let _e136 = scale;
    let _e138 = physicalTint;
    let _e143 = (*physical);
    relit = (((_e135 * _e136) * mix(vec3<f32>(1f, 1f, 1f), clamp(_e138, vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(2f, 2f, 2f)), vec3(0.12f))) + (_e143 * 0.08f));
    let _e146 = (*authored);
    let _e147 = relit;
    let _e148 = (*blend);
    return mix(_e146, _e147, vec3(_e148));
}

fn wired_sky_cube_uv_layer_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: vec3<f32>;
    var face: u32;
    var uv: vec2<f32>;
    var phi_135_: bool;

    let _e105 = (*dir);
    a = abs(_e105);
    let _e108 = a[0u];
    let _e110 = a[1u];
    let _e111 = (_e108 >= _e110);
    phi_135_ = _e111;
    if _e111 {
        let _e113 = a[0u];
        let _e115 = a[2u];
        phi_135_ = (_e113 >= _e115);
    }
    let _e118 = phi_135_;
    if _e118 {
        let _e120 = (*dir)[0u];
        if (_e120 >= 0f) {
            face = 0u;
            let _e123 = (*dir)[2u];
            let _e126 = (*dir)[1u];
            let _e130 = a[0u];
            uv = (vec2<f32>(-(_e123), -(_e126)) / vec2(_e130));
        } else {
            face = 1u;
            let _e134 = (*dir)[2u];
            let _e136 = (*dir)[1u];
            let _e140 = a[0u];
            uv = (vec2<f32>(_e134, -(_e136)) / vec2(_e140));
        }
    } else {
        let _e144 = a[1u];
        let _e146 = a[2u];
        if (_e144 >= _e146) {
            let _e149 = (*dir)[1u];
            if (_e149 >= 0f) {
                face = 2u;
                let _e152 = (*dir)[0u];
                let _e154 = (*dir)[2u];
                let _e157 = a[1u];
                uv = (vec2<f32>(_e152, _e154) / vec2(_e157));
            } else {
                face = 3u;
                let _e161 = (*dir)[0u];
                let _e163 = (*dir)[2u];
                let _e167 = a[1u];
                uv = (vec2<f32>(_e161, -(_e163)) / vec2(_e167));
            }
        } else {
            let _e171 = (*dir)[2u];
            if (_e171 >= 0f) {
                face = 4u;
                let _e174 = (*dir)[0u];
                let _e176 = (*dir)[1u];
                let _e180 = a[2u];
                uv = (vec2<f32>(_e174, -(_e176)) / vec2(_e180));
            } else {
                face = 5u;
                let _e184 = (*dir)[0u];
                let _e187 = (*dir)[1u];
                let _e191 = a[2u];
                uv = (vec2<f32>(-(_e184), -(_e187)) / vec2(_e191));
            }
        }
    }
    let _e194 = uv;
    let _e197 = ((_e194 * 0.5f) + vec2(0.5f));
    let _e198 = face;
    return vec3<f32>(_e197.x, _e197.y, f32(_e198));
}

fn wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(skyTexture: texture_2d_array<f32>, skySampler: sampler, direction: ptr<function, vec3<f32>>, lod: ptr<function, f32>) -> vec3<f32> {
    var param_1: vec3<f32>;

    let _e106 = (*direction);
    param_1 = _e106;
    let _e107 = wired_sky_cube_uv_layer_u0028_vf3_u003b((&param_1));
    let _e108 = (*lod);
    let _e114 = textureSampleLevel(skyTexture, skySampler, vec2<f32>(_e107.x, _e107.y), i32(_e107.z), _e108);
    return _e114.xyz;
}

fn wired_aerial_inscatter_u0028_vf3_u003b_vf3_u003b(mediaColor: ptr<function, vec3<f32>>, viewDir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var packedHaze: u32;
    var aerial: f32;
    var skyViewUv: vec2<f32>;
    var skyIn: vec3<f32>;
    var param_2: vec3<f32>;
    var param_3: f32;
    var skySum: vec3<f32>;
    var face_1: i32;
    var axis: vec3<f32>;
    var param_4: vec3<f32>;
    var param_5: f32;
    var skyMean: f32;
    var directional: vec3<f32>;
    var sunDir: vec3<f32>;
    var cosT: f32;
    var hg: f32;
    var tinted: vec3<f32>;

    let _e122 = productDraw.atmosphereFroxelGrid[3u];
    packedHaze = _e122;
    let _e123 = packedHaze;
    aerial = (f32((_e123 >> bitcast<u32>(24u))) / 255f);
    let _e128 = aerial;
    if (_e128 <= 0f) {
        let _e130 = (*mediaColor);
        return _e130;
    }
    let _e133 = productDraw.atmosphereHeightSky[2u];
    if (_e133 > 0f) {
        let _e136 = (*viewDir)[1u];
        let _e138 = (*viewDir)[0u];
        let _e144 = (*viewDir)[2u];
        skyViewUv = vec2<f32>(fract(((atan2(_e136, _e138) / 6.2831855f) + 0.5f)), clamp(((asin(clamp(_e144, -1f, 1f)) / 3.1415927f) + 0.5f), 0f, 1f));
        let _e151 = skyViewUv;
        let _e152 = textureSampleLevel(physicalSkyTexture, physicalSkySampler, _e151, 0f);
        skyIn = _e152.xyz;
    } else {
        let _e154 = (*viewDir);
        param_2 = _e154;
        param_3 = 4f;
        let _e155 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_2), (&param_3));
        skyIn = _e155;
    }
    skySum = vec3<f32>(0f, 0f, 0f);
    face_1 = 0i;
    loop {
        let _e156 = face_1;
        if (_e156 < 6i) {
            axis = vec3<f32>(0f, 0f, 0f);
            let _e158 = face_1;
            let _e160 = face_1;
            axis[(_e158 / 2i)] = select(1f, -1f, ((_e160 & 1i) != 0i));
            let _e165 = axis;
            param_4 = _e165;
            param_5 = 7f;
            let _e166 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_4), (&param_5));
            let _e167 = skySum;
            skySum = (_e167 + _e166);
            continue;
        } else {
            break;
        }
        continuing {
            let _e169 = face_1;
            face_1 = (_e169 + 1i);
        }
    }
    let _e171 = skySum;
    skyMean = max(dot((_e171 / vec3(6f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f)), 0.0001f);
    let _e176 = skyIn;
    let _e177 = skyMean;
    directional = (_e176 / vec3(_e177));
    let _e180 = packedHaze;
    sunDir = normalize((unpack4x8snorm(_e180).xyz + vec3<f32>(0.000001f, 0.000001f, 0.000001f)));
    let _e185 = (*viewDir);
    let _e186 = sunDir;
    cosT = dot(_e185, _e186);
    let _e188 = cosT;
    hg = (0.6975f / max(pow((1.3025f - (1.1f * _e188)), 1.5f), 0.0001f));
    let _e194 = (*mediaColor);
    let _e195 = directional;
    let _e197 = aerial;
    tinted = (_e194 * mix(vec3<f32>(1f, 1f, 1f), clamp(_e195, vec3<f32>(0.35f, 0.35f, 0.35f), vec3<f32>(3f, 3f, 3f)), vec3(_e197)));
    let _e201 = tinted;
    let _e202 = hg;
    let _e205 = aerial;
    return (_e201 * mix(1f, (0.75f + (0.25f * _e202)), _e205));
}

fn wired_atmosphere_sample_u0028_() -> vec4<f32> {
    var grid: vec3<u32>;
    var x: u32;
    var y: u32;
    var farDistance: f32;
    var viewDistance: f32;
    var slice: f32;
    var z: u32;
    var density: f32;
    var distanceToEye: f32;
    var heightWeight: f32;
    var transmittance: f32;
    var inscatter: vec3<f32>;
    var param_6: vec3<f32>;
    var param_7: vec3<f32>;

    let _e117 = productDraw.viewOriginDrawSpace[3u];
    if (_e117 >= 0.5f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e121 = productDraw.atmosphereFroxelGrid[2u];
    if (_e121 > 0u) {
        let _e124 = productDraw.atmosphereFroxelGrid;
        grid = _e124.xyz;
        let _e127 = gl_FragCoord_1[0u];
        let _e131 = grid[0u];
        x = min((u32(_e127) / 16u), (_e131 - 1u));
        let _e135 = gl_FragCoord_1[1u];
        let _e139 = grid[1u];
        y = min((u32(_e135) / 16u), (_e139 - 1u));
        let _e144 = productDraw.atmosphereColorVisibility[3u];
        farDistance = max(_e144, 4.01f);
        let _e146 = worldPosition_1;
        let _e148 = productDraw.atmosphereEyeDensity;
        let _e152 = productDraw.viewForward;
        viewDistance = dot((_e146 - _e148.xyz), _e152.xyz);
        let _e155 = viewDistance;
        let _e159 = farDistance;
        slice = clamp((log((max(_e155, 4f) / 4f)) / log((_e159 / 4f))), 0f, 0.9999f);
        let _e164 = slice;
        let _e166 = grid[2u];
        let _e171 = grid[2u];
        z = min(u32((_e164 * f32(_e166))), (_e171 - 1u));
        let _e176 = productDraw.atmosphereFroxelGrid[3u];
        let _e177 = z;
        let _e179 = grid[1u];
        let _e181 = y;
        let _e184 = grid[0u];
        let _e187 = x;
        let _e191 = atmosphereFroxels.values[((_e176 + (((_e177 * _e179) + _e181) * _e184)) + _e187)];
        return _e191;
    }
    let _e194 = productDraw.atmosphereEyeDensity[3u];
    density = _e194;
    let _e197 = productDraw.atmosphereColorVisibility[3u];
    if (_e197 > 0f) {
        let _e199 = density;
        let _e202 = productDraw.atmosphereColorVisibility[3u];
        density = max(_e199, (3.912023f / _e202));
    }
    let _e205 = density;
    if (_e205 <= 0f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e207 = worldPosition_1;
    let _e209 = productDraw.atmosphereEyeDensity;
    distanceToEye = length((_e207 - _e209.xyz));
    let _e215 = productDraw.atmosphereHeightSky[1u];
    let _e218 = worldPosition_1[2u];
    let _e221 = productDraw.atmosphereHeightSky[0u];
    heightWeight = exp((-(_e215) * max((_e218 - _e221), 0f)));
    let _e226 = density;
    let _e228 = heightWeight;
    let _e230 = distanceToEye;
    transmittance = clamp(exp(((-(_e226) * _e228) * _e230)), 0f, 1f);
    let _e234 = worldPosition_1;
    let _e236 = productDraw.atmosphereEyeDensity;
    let _e241 = productDraw.atmosphereColorVisibility;
    param_6 = _e241.xyz;
    param_7 = normalize((_e234 - _e236.xyz));
    let _e243 = wired_aerial_inscatter_u0028_vf3_u003b_vf3_u003b((&param_6), (&param_7));
    inscatter = _e243;
    let _e244 = inscatter;
    let _e245 = transmittance;
    let _e247 = (_e244 * (1f - _e245));
    let _e248 = transmittance;
    return vec4<f32>(_e247.x, _e247.y, _e247.z, _e248);
}

fn main_1() {
    var coverage: f32;
    var base: vec4<f32>;
    var signedDistance: f32;
    var screenWidth: f32;
    var linearColor: vec3<f32>;
    var direction_1: vec3<f32>;
    var current: vec3<f32>;
    var param_8: vec3<f32>;
    var param_9: f32;
    var previous: vec3<f32>;
    var param_10: vec3<f32>;
    var param_11: f32;
    var direction_2: vec3<f32>;
    var physicalUv: vec2<f32>;
    var physical_1: vec3<f32>;
    var param_12: vec3<f32>;
    var param_13: vec3<f32>;
    var param_14: f32;
    var param_15: vec3<f32>;
    var param_16: vec3<f32>;
    var alphaMode: i32;
    var localSh: vec3<f32>;
    var address: vec3<f32>;
    var radiance_1: vec3<f32>;
    var encodedDirection_1: vec2<f32>;
    var lighting: vec3<f32>;
    var param_17: vec3<f32>;
    var param_18: vec2<f32>;
    var param_19: vec3<f32>;
    var boosted: vec3<f32>;
    var peak: f32;
    var local_1: vec3<f32>;
    var lit: vec3<f32>;
    var metallic: f32;
    var roughness: f32;
    var occlusion: f32;
    var environmentBlend: f32;
    var normal: vec3<f32>;
    var viewDirection: vec3<f32>;
    var reflected: vec3<f32>;
    var currentSpecular: vec3<f32>;
    var param_20: vec3<f32>;
    var param_21: f32;
    var previousSpecular: vec3<f32>;
    var param_22: vec3<f32>;
    var param_23: f32;
    var f0_: vec3<f32>;
    var nDotV: f32;
    var fresnel: vec3<f32>;
    var specularIbl: vec3<f32>;
    var media: vec4<f32>;
    var local_2: vec3<f32>;
    var param_24: vec3<f32>;
    var phi_821_: bool;
    var phi_862_: bool;
    var phi_919_: bool;
    var phi_949_: bool;
    var phi_1057_: bool;

    let _e156 = productMaterial.beamFlags[2u];
    coverage = select(0f, 1f, (_e156 != 0u));
    let _e159 = coverage;
    let _e160 = coverage;
    outCoverage = vec4<f32>(_e159, 0f, 0f, _e160);
    let _e162 = texCoord_1;
    let _e163 = textureSample(baseTexture, baseSampler, _e162);
    base = _e163;
    let _e166 = productMaterial.beamFlags[1u];
    if (_e166 != 0u) {
        let _e169 = base[0u];
        let _e171 = base[1u];
        let _e174 = base[0u];
        let _e176 = base[1u];
        let _e179 = base[2u];
        signedDistance = (max(min(_e169, _e171), min(max(_e174, _e176), _e179)) - 0.5f);
        let _e183 = signedDistance;
        let _e184 = fwidth(_e183);
        screenWidth = max(_e184, 0.0009765625f);
        let _e186 = signedDistance;
        let _e187 = screenWidth;
        base = vec4<f32>(1f, 1f, 1f, clamp(((_e186 / _e187) + 0.5f), 0f, 1f));
    }
    let _e194 = productMaterial.beamFlags[0u];
    if (_e194 != 0u) {
        let _e196 = color_1;
        let _e203 = color_1;
        let _e207 = color_1;
        linearColor = select(pow(((_e196.xyz + vec3(0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f)), (_e203.xyz / vec3(12.92f)), (_e207.xyz <= vec3<f32>(0.04045f, 0.04045f, 0.04045f)));
        let _e211 = base;
        let _e213 = linearColor;
        let _e215 = wired_atmosphere_sample_u0028_();
        let _e217 = ((_e211.xyz * _e213) * _e215.w);
        let _e219 = base[3u];
        let _e221 = color_1[3u];
        outColor = vec4<f32>(_e217.x, _e217.y, _e217.z, (_e219 * _e221));
        return;
    }
    let _e229 = productDraw.atmosphereHeightSky[3u];
    let _e230 = (_e229 > 0.5f);
    phi_821_ = _e230;
    if _e230 {
        let _e233 = productDraw.emissiveRadiance[3u];
        phi_821_ = (_e233 >= 0f);
    }
    let _e236 = phi_821_;
    if _e236 {
        let _e237 = worldPosition_1;
        let _e239 = productDraw.atmosphereEyeDensity;
        direction_1 = normalize((_e237 - _e239.xyz));
        let _e243 = direction_1;
        param_8 = _e243;
        param_9 = 0f;
        let _e244 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_8), (&param_9));
        current = _e244;
        let _e245 = direction_1;
        param_10 = _e245;
        param_11 = 0f;
        let _e246 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_10), (&param_11));
        previous = _e246;
        let _e247 = previous;
        let _e248 = current;
        let _e251 = productDraw.emissiveRadiance[3u];
        let _e254 = mix(_e247, _e248, vec3(clamp(_e251, 0f, 1f)));
        base = vec4<f32>(_e254.x, _e254.y, _e254.z, 1f);
    }
    let _e261 = productDraw.atmosphereHeightSky[3u];
    let _e262 = (_e261 > 0.5f);
    phi_862_ = _e262;
    if _e262 {
        let _e265 = productDraw.atmosphereHeightSky[2u];
        phi_862_ = (_e265 > 0f);
    }
    let _e268 = phi_862_;
    if _e268 {
        let _e269 = worldPosition_1;
        let _e271 = productDraw.atmosphereEyeDensity;
        direction_2 = normalize((_e269 - _e271.xyz));
        let _e276 = direction_2[1u];
        let _e278 = direction_2[0u];
        let _e283 = direction_2[2u];
        physicalUv = vec2<f32>(((atan2(_e276, _e278) / 6.2831855f) + 0.5f), ((asin(clamp(_e283, -1f, 1f)) / 3.1415927f) + 0.5f));
        let _e289 = physicalUv;
        let _e290 = textureSample(physicalSkyTexture, physicalSkySampler, _e289);
        physical_1 = _e290.xyz;
        let _e294 = productDraw.atmosphereHeightSky[2u];
        let _e296 = base;
        param_12 = _e296.xyz;
        let _e298 = physical_1;
        param_13 = _e298;
        param_14 = clamp(_e294, 0f, 1f);
        let _e299 = wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
        param_15 = _e299;
        let _e300 = wired_display_visibility_u0028_vf3_u003b((&param_15));
        outColor = vec4<f32>(_e300.x, _e300.y, _e300.z, 1f);
        return;
    }
    let _e307 = productDraw.atmosphereHeightSky[3u];
    let _e308 = (_e307 > 0.5f);
    phi_919_ = _e308;
    if _e308 {
        let _e311 = productDraw.emissiveRadiance[3u];
        phi_919_ = (_e311 >= 0f);
    }
    let _e314 = phi_919_;
    if _e314 {
        let _e315 = base;
        param_16 = _e315.xyz;
        let _e317 = wired_display_visibility_u0028_vf3_u003b((&param_16));
        outColor = vec4<f32>(_e317.x, _e317.y, _e317.z, 1f);
        return;
    }
    let _e324 = productDraw.viewportAlpha[2u];
    alphaMode = i32((_e324 + 0.5f));
    let _e328 = productMaterial.baseColor;
    let _e329 = base;
    base = (_e329 * _e328);
    let _e331 = alphaMode;
    let _e332 = (_e331 == 1i);
    phi_949_ = _e332;
    if _e332 {
        let _e334 = base[3u];
        let _e337 = productDraw.viewportAlpha[3u];
        phi_949_ = (_e334 < _e337);
    }
    let _e340 = phi_949_;
    if _e340 {
        discard;
    }
    let _e343 = productDraw.localSh[0i];
    let _e347 = productDraw.localSh[1i];
    let _e350 = worldNormal_1[0u];
    let _e355 = productDraw.localSh[2i];
    let _e358 = worldNormal_1[1u];
    let _e363 = productDraw.localSh[3i];
    let _e366 = worldNormal_1[2u];
    localSh = max((((_e343.xyz + (_e347.xyz * _e350)) + (_e355.xyz * _e358)) + (_e363.xyz * _e366)), vec3<f32>(0f, 0f, 0f));
    let _e372 = productDraw.staticLighting[0u];
    if (_e372 == 2u) {
        let _e374 = lightmapCoord_1;
        let _e377 = productDraw.staticLighting[1u];
        address = vec3<f32>(_e374.x, _e374.y, f32(_e377));
        let _e382 = address;
        let _e388 = textureSample(staticRadianceTexture, staticRadianceSampler, vec2<f32>(_e382.x, _e382.y), i32(_e382.z));
        radiance_1 = _e388.xyz;
        let _e390 = address;
        let _e396 = textureSample(staticDirectionTexture, staticDirectionSampler, vec2<f32>(_e390.x, _e390.y), i32(_e390.z));
        encodedDirection_1 = _e396.xy;
        let _e400 = productDraw.staticLighting[2u];
        if (_e400 == 2u) {
            let _e402 = encodedDirection_1;
            encodedDirection_1 = ((_e402 * 0.5f) + vec2(0.5f));
        }
        let _e406 = radiance_1;
        param_17 = _e406;
        let _e407 = encodedDirection_1;
        param_18 = _e407;
        let _e408 = worldNormal_1;
        param_19 = _e408;
        let _e409 = wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b((&param_17), (&param_18), (&param_19));
        lighting = _e409;
        let _e412 = productDraw.staticLighting[3u];
        if (_e412 != 0u) {
            let _e414 = address;
            let _e420 = textureSample(staticVisibilityTexture, staticVisibilitySampler, vec2<f32>(_e414.x, _e414.y), i32(_e414.z));
            let _e422 = lighting;
            lighting = (_e422 * _e420.x);
        }
    } else {
        let _e426 = productDraw.staticLighting[0u];
        let _e427 = (_e426 == 1u);
        phi_1057_ = _e427;
        if !(_e427) {
            let _e431 = productDraw.projectionLightmap[3u];
            phi_1057_ = (_e431 >= 0.5f);
        }
        let _e434 = phi_1057_;
        if _e434 {
            let _e435 = lightmapCoord_1;
            let _e436 = textureSample(lightmapTexture, lightmapSampler, _e435);
            let _e440 = productDraw.projectionLightmap[3u];
            boosted = (_e436.xyz * max(_e440, 1f));
            let _e444 = boosted[0u];
            let _e446 = boosted[1u];
            let _e448 = boosted[2u];
            peak = max(_e444, max(_e446, _e448));
            let _e451 = peak;
            if (_e451 > 1f) {
                let _e453 = boosted;
                let _e454 = peak;
                local_1 = (_e453 / vec3(_e454));
            } else {
                let _e457 = boosted;
                local_1 = _e457;
            }
            let _e458 = local_1;
            lighting = _e458;
        } else {
            let _e462 = productDraw.localSh[0i][3u];
            let _e464 = localSh;
            lighting = select(vec3<f32>(1f, 1f, 1f), _e464, vec3((_e462 >= 0.5f)));
        }
    }
    let _e467 = base;
    let _e469 = lighting;
    let _e471 = color_1;
    let _e474 = base;
    let _e477 = productDraw.emissiveRadiance;
    lit = (((_e467.xyz * _e469) * _e471.xyz) + (_e474.xyz * _e477.xyz));
    let _e483 = productMaterial.factors[0u];
    if (_e483 >= 0f) {
        let _e487 = productMaterial.factors[0u];
        metallic = clamp(_e487, 0f, 1f);
        let _e491 = productMaterial.factors[1u];
        roughness = clamp(_e491, 0f, 1f);
        let _e495 = productMaterial.factors[2u];
        occlusion = clamp(_e495, 0f, 1f);
        let _e499 = productMaterial.factors[3u];
        environmentBlend = clamp(_e499, 0f, 1f);
        let _e501 = worldNormal_1;
        normal = normalize(_e501);
        let _e504 = productDraw.atmosphereEyeDensity;
        let _e506 = worldPosition_1;
        viewDirection = normalize((_e504.xyz - _e506));
        let _e509 = viewDirection;
        let _e511 = normal;
        reflected = reflect(-(_e509), _e511);
        let _e513 = roughness;
        let _e514 = textureNumLevels(currentSkyRadianceTexture);
        let _e519 = reflected;
        param_20 = _e519;
        param_21 = (_e513 * f32((i32(_e514) - 1i)));
        let _e520 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_20), (&param_21));
        currentSpecular = _e520;
        let _e521 = roughness;
        let _e522 = textureNumLevels(previousSkyRadianceTexture);
        let _e527 = reflected;
        param_22 = _e527;
        param_23 = (_e521 * f32((i32(_e522) - 1i)));
        let _e528 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_22), (&param_23));
        previousSpecular = _e528;
        let _e529 = base;
        let _e531 = metallic;
        f0_ = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e529.xyz, vec3(_e531));
        let _e534 = normal;
        let _e535 = viewDirection;
        nDotV = max(dot(_e534, _e535), 0f);
        let _e538 = f0_;
        let _e539 = f0_;
        let _e542 = nDotV;
        fresnel = (_e538 + ((vec3(1f) - _e539) * pow((1f - _e542), 5f)));
        let _e547 = previousSpecular;
        let _e548 = currentSpecular;
        let _e549 = environmentBlend;
        let _e552 = fresnel;
        specularIbl = (mix(_e547, _e548, vec3(_e549)) * _e552);
        let _e554 = specularIbl;
        let _e555 = occlusion;
        let _e557 = lit;
        lit = (_e557 + (_e554 * _e555));
    }
    let _e559 = wired_atmosphere_sample_u0028_();
    media = _e559;
    let _e560 = lit;
    let _e562 = media[3u];
    let _e564 = alphaMode;
    let _e566 = alphaMode;
    if ((_e564 == 3i) || (_e566 == 4i)) {
        local_2 = vec3<f32>(0f, 0f, 0f);
    } else {
        let _e569 = media;
        local_2 = _e569.xyz;
    }
    let _e571 = local_2;
    lit = ((_e560 * _e562) + _e571);
    let _e575 = productDraw.viewOriginDrawSpace[3u];
    if (_e575 < 0.5f) {
        let _e577 = lit;
        param_24 = _e577;
        let _e578 = wired_display_visibility_u0028_vf3_u003b((&param_24));
        lit = _e578;
    }
    let _e579 = lit;
    let _e581 = base[3u];
    let _e583 = color_1[3u];
    outColor = vec4<f32>(_e579.x, _e579.y, _e579.z, (_e581 * _e583));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(3) worldPosition: vec3<f32>, @location(0) texCoord: vec2<f32>, @location(2) color: vec4<f32>, @location(4) worldNormal: vec3<f32>, @location(1) lightmapCoord: vec2<f32>) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    worldPosition_1 = worldPosition;
    texCoord_1 = texCoord;
    color_1 = color;
    worldNormal_1 = worldNormal;
    lightmapCoord_1 = lightmapCoord;
    main_1();
    let _e14 = outCoverage;
    let _e15 = outColor;
    return FragmentOutput(_e14, _e15);
}
