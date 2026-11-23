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
@group(1) @binding(0)
var baseTexture: texture_2d<f32>;
@group(1) @binding(1)
var baseSampler: sampler;
var<private> texCoord_1: vec2<f32>;
@group(2) @binding(1)
var<uniform> productMaterial: ProductMaterialFactors;
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

    let _e104 = (*encoded);
    f = ((_e104 * 2f) - vec2(1f));
    let _e108 = f;
    let _e110 = f[0u];
    let _e114 = f[1u];
    n = vec3<f32>(_e108.x, _e108.y, ((1f - abs(_e110)) - abs(_e114)));
    let _e121 = n[2u];
    if (_e121 < 0f) {
        let _e123 = n;
        let _e128 = n;
        folded = ((vec2(1f) - abs(_e123.yx)) * sign(_e128.xy));
        let _e132 = folded;
        n[0u] = _e132.x;
        n[1u] = _e132.y;
    }
    let _e137 = n;
    return normalize(_e137);
}

fn wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b(radiance: ptr<function, vec3<f32>>, encodedDirection: ptr<function, vec2<f32>>, materialNormal: ptr<function, vec3<f32>>) -> vec3<f32> {
    var dominantDirection: vec3<f32>;
    var param: vec2<f32>;

    let _e105 = (*encodedDirection);
    param = _e105;
    let _e106 = wired_lighting_decode_oct_u0028_vf2_u003b((&param));
    dominantDirection = _e106;
    let _e107 = (*radiance);
    let _e109 = (*materialNormal);
    let _e111 = dominantDirection;
    return (max(_e107, vec3<f32>(0f, 0f, 0f)) * max(dot(normalize(_e109), _e111), 0f));
}

fn wired_display_visibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_274_: bool;
    var phi_281_: bool;

    let _e104 = (*sceneColor);
    let _e108 = productDraw.viewForward[3u];
    exposed = (max(_e104, vec3<f32>(0f, 0f, 0f)) * _e108);
    let _e110 = exposed;
    luminance = dot(_e110, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e112 = luminance;
    let _e113 = (_e112 > 0f);
    phi_274_ = _e113;
    if _e113 {
        let _e114 = luminance;
        let _e117 = productDraw.viewUp[3u];
        phi_274_ = (_e114 < _e117);
    }
    let _e120 = phi_274_;
    phi_281_ = _e120;
    if _e120 {
        let _e123 = productDraw.viewLeft[3u];
        phi_281_ = (_e123 != 1f);
    }
    let _e126 = phi_281_;
    if _e126 {
        let _e127 = luminance;
        let _e130 = productDraw.viewUp[3u];
        let _e134 = productDraw.viewLeft[3u];
        let _e138 = productDraw.viewUp[3u];
        curved = (pow((_e127 / _e130), _e134) * _e138);
        let _e140 = curved;
        let _e141 = luminance;
        let _e143 = exposed;
        exposed = (_e143 * (_e140 / _e141));
    }
    let _e145 = exposed;
    return _e145;
}

fn wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b(authored: ptr<function, vec3<f32>>, physical: ptr<function, vec3<f32>>, blend: ptr<function, f32>) -> vec3<f32> {
    var authoredEnergy: f32;
    var physicalEnergy: f32;
    var scale: f32;
    var physicalTint: vec3<f32>;
    var local: vec3<f32>;
    var relit: vec3<f32>;

    let _e109 = (*authored);
    authoredEnergy = dot(max(_e109, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e112 = (*physical);
    physicalEnergy = dot(max(_e112, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e115 = authoredEnergy;
    if (_e115 <= 0.00001f) {
        let _e117 = (*authored);
        let _e118 = (*physical);
        let _e119 = (*blend);
        return mix(_e117, _e118, vec3(_e119));
    }
    let _e122 = physicalEnergy;
    let _e123 = authoredEnergy;
    scale = clamp((_e122 / max(_e123, 0.18f)), 0.65f, 1.75f);
    let _e127 = physicalEnergy;
    if (_e127 > 0.00001f) {
        let _e129 = (*physical);
        let _e130 = physicalEnergy;
        local = (_e129 / vec3(_e130));
    } else {
        local = vec3<f32>(1f, 1f, 1f);
    }
    let _e133 = local;
    physicalTint = _e133;
    let _e134 = (*authored);
    let _e135 = scale;
    let _e137 = physicalTint;
    let _e142 = (*physical);
    relit = (((_e134 * _e135) * mix(vec3<f32>(1f, 1f, 1f), clamp(_e137, vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(2f, 2f, 2f)), vec3(0.12f))) + (_e142 * 0.08f));
    let _e145 = (*authored);
    let _e146 = relit;
    let _e147 = (*blend);
    return mix(_e145, _e146, vec3(_e147));
}

fn wired_sky_cube_uv_layer_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: vec3<f32>;
    var face: u32;
    var uv: vec2<f32>;
    var phi_135_: bool;

    let _e104 = (*dir);
    a = abs(_e104);
    let _e107 = a[0u];
    let _e109 = a[1u];
    let _e110 = (_e107 >= _e109);
    phi_135_ = _e110;
    if _e110 {
        let _e112 = a[0u];
        let _e114 = a[2u];
        phi_135_ = (_e112 >= _e114);
    }
    let _e117 = phi_135_;
    if _e117 {
        let _e119 = (*dir)[0u];
        if (_e119 >= 0f) {
            face = 0u;
            let _e122 = (*dir)[2u];
            let _e125 = (*dir)[1u];
            let _e129 = a[0u];
            uv = (vec2<f32>(-(_e122), -(_e125)) / vec2(_e129));
        } else {
            face = 1u;
            let _e133 = (*dir)[2u];
            let _e135 = (*dir)[1u];
            let _e139 = a[0u];
            uv = (vec2<f32>(_e133, -(_e135)) / vec2(_e139));
        }
    } else {
        let _e143 = a[1u];
        let _e145 = a[2u];
        if (_e143 >= _e145) {
            let _e148 = (*dir)[1u];
            if (_e148 >= 0f) {
                face = 2u;
                let _e151 = (*dir)[0u];
                let _e153 = (*dir)[2u];
                let _e156 = a[1u];
                uv = (vec2<f32>(_e151, _e153) / vec2(_e156));
            } else {
                face = 3u;
                let _e160 = (*dir)[0u];
                let _e162 = (*dir)[2u];
                let _e166 = a[1u];
                uv = (vec2<f32>(_e160, -(_e162)) / vec2(_e166));
            }
        } else {
            let _e170 = (*dir)[2u];
            if (_e170 >= 0f) {
                face = 4u;
                let _e173 = (*dir)[0u];
                let _e175 = (*dir)[1u];
                let _e179 = a[2u];
                uv = (vec2<f32>(_e173, -(_e175)) / vec2(_e179));
            } else {
                face = 5u;
                let _e183 = (*dir)[0u];
                let _e186 = (*dir)[1u];
                let _e190 = a[2u];
                uv = (vec2<f32>(-(_e183), -(_e186)) / vec2(_e190));
            }
        }
    }
    let _e193 = uv;
    let _e196 = ((_e193 * 0.5f) + vec2(0.5f));
    let _e197 = face;
    return vec3<f32>(_e196.x, _e196.y, f32(_e197));
}

fn wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(skyTexture: texture_2d_array<f32>, skySampler: sampler, direction: ptr<function, vec3<f32>>, lod: ptr<function, f32>) -> vec3<f32> {
    var param_1: vec3<f32>;

    let _e105 = (*direction);
    param_1 = _e105;
    let _e106 = wired_sky_cube_uv_layer_u0028_vf3_u003b((&param_1));
    let _e107 = (*lod);
    let _e113 = textureSampleLevel(skyTexture, skySampler, vec2<f32>(_e106.x, _e106.y), i32(_e106.z), _e107);
    return _e113.xyz;
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

    let _e121 = productDraw.atmosphereFroxelGrid[3u];
    packedHaze = _e121;
    let _e122 = packedHaze;
    aerial = (f32((_e122 >> bitcast<u32>(24u))) / 255f);
    let _e127 = aerial;
    if (_e127 <= 0f) {
        let _e129 = (*mediaColor);
        return _e129;
    }
    let _e132 = productDraw.atmosphereHeightSky[2u];
    if (_e132 > 0f) {
        let _e135 = (*viewDir)[1u];
        let _e137 = (*viewDir)[0u];
        let _e143 = (*viewDir)[2u];
        skyViewUv = vec2<f32>(fract(((atan2(_e135, _e137) / 6.2831855f) + 0.5f)), clamp(((asin(clamp(_e143, -1f, 1f)) / 3.1415927f) + 0.5f), 0f, 1f));
        let _e150 = skyViewUv;
        let _e151 = textureSampleLevel(physicalSkyTexture, physicalSkySampler, _e150, 0f);
        skyIn = _e151.xyz;
    } else {
        let _e153 = (*viewDir);
        param_2 = _e153;
        param_3 = 4f;
        let _e154 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_2), (&param_3));
        skyIn = _e154;
    }
    skySum = vec3<f32>(0f, 0f, 0f);
    face_1 = 0i;
    loop {
        let _e155 = face_1;
        if (_e155 < 6i) {
            axis = vec3<f32>(0f, 0f, 0f);
            let _e157 = face_1;
            let _e159 = face_1;
            axis[(_e157 / 2i)] = select(1f, -1f, ((_e159 & 1i) != 0i));
            let _e164 = axis;
            param_4 = _e164;
            param_5 = 7f;
            let _e165 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_4), (&param_5));
            let _e166 = skySum;
            skySum = (_e166 + _e165);
            continue;
        } else {
            break;
        }
        continuing {
            let _e168 = face_1;
            face_1 = (_e168 + 1i);
        }
    }
    let _e170 = skySum;
    skyMean = max(dot((_e170 / vec3(6f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f)), 0.0001f);
    let _e175 = skyIn;
    let _e176 = skyMean;
    directional = (_e175 / vec3(_e176));
    let _e179 = packedHaze;
    sunDir = normalize((unpack4x8snorm(_e179).xyz + vec3<f32>(0.000001f, 0.000001f, 0.000001f)));
    let _e184 = (*viewDir);
    let _e185 = sunDir;
    cosT = dot(_e184, _e185);
    let _e187 = cosT;
    hg = (0.6975f / max(pow((1.3025f - (1.1f * _e187)), 1.5f), 0.0001f));
    let _e193 = (*mediaColor);
    let _e194 = directional;
    let _e196 = aerial;
    tinted = (_e193 * mix(vec3<f32>(1f, 1f, 1f), clamp(_e194, vec3<f32>(0.35f, 0.35f, 0.35f), vec3<f32>(3f, 3f, 3f)), vec3(_e196)));
    let _e200 = tinted;
    let _e201 = hg;
    let _e204 = aerial;
    return (_e200 * mix(1f, (0.75f + (0.25f * _e201)), _e204));
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

    let _e116 = productDraw.viewOriginDrawSpace[3u];
    if (_e116 >= 0.5f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e120 = productDraw.atmosphereFroxelGrid[2u];
    if (_e120 > 0u) {
        let _e123 = productDraw.atmosphereFroxelGrid;
        grid = _e123.xyz;
        let _e126 = gl_FragCoord_1[0u];
        let _e130 = grid[0u];
        x = min((u32(_e126) / 16u), (_e130 - 1u));
        let _e134 = gl_FragCoord_1[1u];
        let _e138 = grid[1u];
        y = min((u32(_e134) / 16u), (_e138 - 1u));
        let _e143 = productDraw.atmosphereColorVisibility[3u];
        farDistance = max(_e143, 4.01f);
        let _e145 = worldPosition_1;
        let _e147 = productDraw.atmosphereEyeDensity;
        let _e151 = productDraw.viewForward;
        viewDistance = dot((_e145 - _e147.xyz), _e151.xyz);
        let _e154 = viewDistance;
        let _e158 = farDistance;
        slice = clamp((log((max(_e154, 4f) / 4f)) / log((_e158 / 4f))), 0f, 0.9999f);
        let _e163 = slice;
        let _e165 = grid[2u];
        let _e170 = grid[2u];
        z = min(u32((_e163 * f32(_e165))), (_e170 - 1u));
        let _e175 = productDraw.atmosphereFroxelGrid[3u];
        let _e176 = z;
        let _e178 = grid[1u];
        let _e180 = y;
        let _e183 = grid[0u];
        let _e186 = x;
        let _e190 = atmosphereFroxels.values[((_e175 + (((_e176 * _e178) + _e180) * _e183)) + _e186)];
        return _e190;
    }
    let _e193 = productDraw.atmosphereEyeDensity[3u];
    density = _e193;
    let _e196 = productDraw.atmosphereColorVisibility[3u];
    if (_e196 > 0f) {
        let _e198 = density;
        let _e201 = productDraw.atmosphereColorVisibility[3u];
        density = max(_e198, (3.912023f / _e201));
    }
    let _e204 = density;
    if (_e204 <= 0f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e206 = worldPosition_1;
    let _e208 = productDraw.atmosphereEyeDensity;
    distanceToEye = length((_e206 - _e208.xyz));
    let _e214 = productDraw.atmosphereHeightSky[1u];
    let _e217 = worldPosition_1[2u];
    let _e220 = productDraw.atmosphereHeightSky[0u];
    heightWeight = exp((-(_e214) * max((_e217 - _e220), 0f)));
    let _e225 = density;
    let _e227 = heightWeight;
    let _e229 = distanceToEye;
    transmittance = clamp(exp(((-(_e225) * _e227) * _e229)), 0f, 1f);
    let _e233 = worldPosition_1;
    let _e235 = productDraw.atmosphereEyeDensity;
    let _e240 = productDraw.atmosphereColorVisibility;
    param_6 = _e240.xyz;
    param_7 = normalize((_e233 - _e235.xyz));
    let _e242 = wired_aerial_inscatter_u0028_vf3_u003b_vf3_u003b((&param_6), (&param_7));
    inscatter = _e242;
    let _e243 = inscatter;
    let _e244 = transmittance;
    let _e246 = (_e243 * (1f - _e244));
    let _e247 = transmittance;
    return vec4<f32>(_e246.x, _e246.y, _e246.z, _e247);
}

fn main_1() {
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
    var phi_812_: bool;
    var phi_853_: bool;
    var phi_910_: bool;
    var phi_940_: bool;
    var phi_1048_: bool;

    let _e152 = texCoord_1;
    let _e153 = textureSample(baseTexture, baseSampler, _e152);
    base = _e153;
    let _e156 = productMaterial.beamFlags[1u];
    if (_e156 != 0u) {
        let _e159 = base[0u];
        let _e161 = base[1u];
        let _e164 = base[0u];
        let _e166 = base[1u];
        let _e169 = base[2u];
        signedDistance = (max(min(_e159, _e161), min(max(_e164, _e166), _e169)) - 0.5f);
        let _e173 = signedDistance;
        let _e174 = fwidth(_e173);
        screenWidth = max(_e174, 0.0009765625f);
        let _e176 = signedDistance;
        let _e177 = screenWidth;
        base = vec4<f32>(1f, 1f, 1f, clamp(((_e176 / _e177) + 0.5f), 0f, 1f));
    }
    let _e184 = productMaterial.beamFlags[0u];
    if (_e184 != 0u) {
        let _e186 = color_1;
        let _e193 = color_1;
        let _e197 = color_1;
        linearColor = select(pow(((_e186.xyz + vec3(0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f)), (_e193.xyz / vec3(12.92f)), (_e197.xyz <= vec3<f32>(0.04045f, 0.04045f, 0.04045f)));
        let _e201 = base;
        let _e203 = linearColor;
        let _e205 = wired_atmosphere_sample_u0028_();
        let _e207 = ((_e201.xyz * _e203) * _e205.w);
        let _e209 = base[3u];
        let _e211 = color_1[3u];
        outColor = vec4<f32>(_e207.x, _e207.y, _e207.z, (_e209 * _e211));
        return;
    }
    let _e219 = productDraw.atmosphereHeightSky[3u];
    let _e220 = (_e219 > 0.5f);
    phi_812_ = _e220;
    if _e220 {
        let _e223 = productDraw.emissiveRadiance[3u];
        phi_812_ = (_e223 >= 0f);
    }
    let _e226 = phi_812_;
    if _e226 {
        let _e227 = worldPosition_1;
        let _e229 = productDraw.atmosphereEyeDensity;
        direction_1 = normalize((_e227 - _e229.xyz));
        let _e233 = direction_1;
        param_8 = _e233;
        param_9 = 0f;
        let _e234 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_8), (&param_9));
        current = _e234;
        let _e235 = direction_1;
        param_10 = _e235;
        param_11 = 0f;
        let _e236 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_10), (&param_11));
        previous = _e236;
        let _e237 = previous;
        let _e238 = current;
        let _e241 = productDraw.emissiveRadiance[3u];
        let _e244 = mix(_e237, _e238, vec3(clamp(_e241, 0f, 1f)));
        base = vec4<f32>(_e244.x, _e244.y, _e244.z, 1f);
    }
    let _e251 = productDraw.atmosphereHeightSky[3u];
    let _e252 = (_e251 > 0.5f);
    phi_853_ = _e252;
    if _e252 {
        let _e255 = productDraw.atmosphereHeightSky[2u];
        phi_853_ = (_e255 > 0f);
    }
    let _e258 = phi_853_;
    if _e258 {
        let _e259 = worldPosition_1;
        let _e261 = productDraw.atmosphereEyeDensity;
        direction_2 = normalize((_e259 - _e261.xyz));
        let _e266 = direction_2[1u];
        let _e268 = direction_2[0u];
        let _e273 = direction_2[2u];
        physicalUv = vec2<f32>(((atan2(_e266, _e268) / 6.2831855f) + 0.5f), ((asin(clamp(_e273, -1f, 1f)) / 3.1415927f) + 0.5f));
        let _e279 = physicalUv;
        let _e280 = textureSample(physicalSkyTexture, physicalSkySampler, _e279);
        physical_1 = _e280.xyz;
        let _e284 = productDraw.atmosphereHeightSky[2u];
        let _e286 = base;
        param_12 = _e286.xyz;
        let _e288 = physical_1;
        param_13 = _e288;
        param_14 = clamp(_e284, 0f, 1f);
        let _e289 = wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
        param_15 = _e289;
        let _e290 = wired_display_visibility_u0028_vf3_u003b((&param_15));
        outColor = vec4<f32>(_e290.x, _e290.y, _e290.z, 1f);
        return;
    }
    let _e297 = productDraw.atmosphereHeightSky[3u];
    let _e298 = (_e297 > 0.5f);
    phi_910_ = _e298;
    if _e298 {
        let _e301 = productDraw.emissiveRadiance[3u];
        phi_910_ = (_e301 >= 0f);
    }
    let _e304 = phi_910_;
    if _e304 {
        let _e305 = base;
        param_16 = _e305.xyz;
        let _e307 = wired_display_visibility_u0028_vf3_u003b((&param_16));
        outColor = vec4<f32>(_e307.x, _e307.y, _e307.z, 1f);
        return;
    }
    let _e314 = productDraw.viewportAlpha[2u];
    alphaMode = i32((_e314 + 0.5f));
    let _e318 = productMaterial.baseColor;
    let _e319 = base;
    base = (_e319 * _e318);
    let _e321 = alphaMode;
    let _e322 = (_e321 == 1i);
    phi_940_ = _e322;
    if _e322 {
        let _e324 = base[3u];
        let _e327 = productDraw.viewportAlpha[3u];
        phi_940_ = (_e324 < _e327);
    }
    let _e330 = phi_940_;
    if _e330 {
        discard;
    }
    let _e333 = productDraw.localSh[0i];
    let _e337 = productDraw.localSh[1i];
    let _e340 = worldNormal_1[0u];
    let _e345 = productDraw.localSh[2i];
    let _e348 = worldNormal_1[1u];
    let _e353 = productDraw.localSh[3i];
    let _e356 = worldNormal_1[2u];
    localSh = max((((_e333.xyz + (_e337.xyz * _e340)) + (_e345.xyz * _e348)) + (_e353.xyz * _e356)), vec3<f32>(0f, 0f, 0f));
    let _e362 = productDraw.staticLighting[0u];
    if (_e362 == 2u) {
        let _e364 = lightmapCoord_1;
        let _e367 = productDraw.staticLighting[1u];
        address = vec3<f32>(_e364.x, _e364.y, f32(_e367));
        let _e372 = address;
        let _e378 = textureSample(staticRadianceTexture, staticRadianceSampler, vec2<f32>(_e372.x, _e372.y), i32(_e372.z));
        radiance_1 = _e378.xyz;
        let _e380 = address;
        let _e386 = textureSample(staticDirectionTexture, staticDirectionSampler, vec2<f32>(_e380.x, _e380.y), i32(_e380.z));
        encodedDirection_1 = _e386.xy;
        let _e390 = productDraw.staticLighting[2u];
        if (_e390 == 2u) {
            let _e392 = encodedDirection_1;
            encodedDirection_1 = ((_e392 * 0.5f) + vec2(0.5f));
        }
        let _e396 = radiance_1;
        param_17 = _e396;
        let _e397 = encodedDirection_1;
        param_18 = _e397;
        let _e398 = worldNormal_1;
        param_19 = _e398;
        let _e399 = wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b((&param_17), (&param_18), (&param_19));
        lighting = _e399;
        let _e402 = productDraw.staticLighting[3u];
        if (_e402 != 0u) {
            let _e404 = address;
            let _e410 = textureSample(staticVisibilityTexture, staticVisibilitySampler, vec2<f32>(_e404.x, _e404.y), i32(_e404.z));
            let _e412 = lighting;
            lighting = (_e412 * _e410.x);
        }
    } else {
        let _e416 = productDraw.staticLighting[0u];
        let _e417 = (_e416 == 1u);
        phi_1048_ = _e417;
        if !(_e417) {
            let _e421 = productDraw.projectionLightmap[3u];
            phi_1048_ = (_e421 >= 0.5f);
        }
        let _e424 = phi_1048_;
        if _e424 {
            let _e425 = lightmapCoord_1;
            let _e426 = textureSample(lightmapTexture, lightmapSampler, _e425);
            let _e430 = productDraw.projectionLightmap[3u];
            boosted = (_e426.xyz * max(_e430, 1f));
            let _e434 = boosted[0u];
            let _e436 = boosted[1u];
            let _e438 = boosted[2u];
            peak = max(_e434, max(_e436, _e438));
            let _e441 = peak;
            if (_e441 > 1f) {
                let _e443 = boosted;
                let _e444 = peak;
                local_1 = (_e443 / vec3(_e444));
            } else {
                let _e447 = boosted;
                local_1 = _e447;
            }
            let _e448 = local_1;
            lighting = _e448;
        } else {
            let _e452 = productDraw.localSh[0i][3u];
            let _e454 = localSh;
            lighting = select(vec3<f32>(1f, 1f, 1f), _e454, vec3((_e452 >= 0.5f)));
        }
    }
    let _e457 = base;
    let _e459 = lighting;
    let _e461 = color_1;
    let _e464 = base;
    let _e467 = productDraw.emissiveRadiance;
    lit = (((_e457.xyz * _e459) * _e461.xyz) + (_e464.xyz * _e467.xyz));
    let _e473 = productMaterial.factors[0u];
    if (_e473 >= 0f) {
        let _e477 = productMaterial.factors[0u];
        metallic = clamp(_e477, 0f, 1f);
        let _e481 = productMaterial.factors[1u];
        roughness = clamp(_e481, 0f, 1f);
        let _e485 = productMaterial.factors[2u];
        occlusion = clamp(_e485, 0f, 1f);
        let _e489 = productMaterial.factors[3u];
        environmentBlend = clamp(_e489, 0f, 1f);
        let _e491 = worldNormal_1;
        normal = normalize(_e491);
        let _e494 = productDraw.atmosphereEyeDensity;
        let _e496 = worldPosition_1;
        viewDirection = normalize((_e494.xyz - _e496));
        let _e499 = viewDirection;
        let _e501 = normal;
        reflected = reflect(-(_e499), _e501);
        let _e503 = roughness;
        let _e504 = textureNumLevels(currentSkyRadianceTexture);
        let _e509 = reflected;
        param_20 = _e509;
        param_21 = (_e503 * f32((i32(_e504) - 1i)));
        let _e510 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_20), (&param_21));
        currentSpecular = _e510;
        let _e511 = roughness;
        let _e512 = textureNumLevels(previousSkyRadianceTexture);
        let _e517 = reflected;
        param_22 = _e517;
        param_23 = (_e511 * f32((i32(_e512) - 1i)));
        let _e518 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_22), (&param_23));
        previousSpecular = _e518;
        let _e519 = base;
        let _e521 = metallic;
        f0_ = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e519.xyz, vec3(_e521));
        let _e524 = normal;
        let _e525 = viewDirection;
        nDotV = max(dot(_e524, _e525), 0f);
        let _e528 = f0_;
        let _e529 = f0_;
        let _e532 = nDotV;
        fresnel = (_e528 + ((vec3(1f) - _e529) * pow((1f - _e532), 5f)));
        let _e537 = previousSpecular;
        let _e538 = currentSpecular;
        let _e539 = environmentBlend;
        let _e542 = fresnel;
        specularIbl = (mix(_e537, _e538, vec3(_e539)) * _e542);
        let _e544 = specularIbl;
        let _e545 = occlusion;
        let _e547 = lit;
        lit = (_e547 + (_e544 * _e545));
    }
    let _e549 = wired_atmosphere_sample_u0028_();
    media = _e549;
    let _e550 = lit;
    let _e552 = media[3u];
    let _e554 = alphaMode;
    let _e556 = alphaMode;
    if ((_e554 == 3i) || (_e556 == 4i)) {
        local_2 = vec3<f32>(0f, 0f, 0f);
    } else {
        let _e559 = media;
        local_2 = _e559.xyz;
    }
    let _e561 = local_2;
    lit = ((_e550 * _e552) + _e561);
    let _e565 = productDraw.viewOriginDrawSpace[3u];
    if (_e565 < 0.5f) {
        let _e567 = lit;
        param_24 = _e567;
        let _e568 = wired_display_visibility_u0028_vf3_u003b((&param_24));
        lit = _e568;
    }
    let _e569 = lit;
    let _e571 = base[3u];
    let _e573 = color_1[3u];
    outColor = vec4<f32>(_e569.x, _e569.y, _e569.z, (_e571 * _e573));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(3) worldPosition: vec3<f32>, @location(0) texCoord: vec2<f32>, @location(2) color: vec4<f32>, @location(4) worldNormal: vec3<f32>, @location(1) lightmapCoord: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    worldPosition_1 = worldPosition;
    texCoord_1 = texCoord;
    color_1 = color;
    worldNormal_1 = worldNormal;
    lightmapCoord_1 = lightmapCoord;
    main_1();
    let _e13 = outColor;
    return _e13;
}
