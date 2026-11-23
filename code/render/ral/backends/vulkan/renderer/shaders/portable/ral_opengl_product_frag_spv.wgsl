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
    var local_1: vec3<f32>;
    var param_24: vec3<f32>;
    var phi_812_: bool;
    var phi_853_: bool;
    var phi_910_: bool;
    var phi_940_: bool;
    var phi_1048_: bool;

    let _e149 = texCoord_1;
    let _e150 = textureSample(baseTexture, baseSampler, _e149);
    base = _e150;
    let _e153 = productMaterial.beamFlags[1u];
    if (_e153 != 0u) {
        let _e156 = base[0u];
        let _e158 = base[1u];
        let _e161 = base[0u];
        let _e163 = base[1u];
        let _e166 = base[2u];
        signedDistance = (max(min(_e156, _e158), min(max(_e161, _e163), _e166)) - 0.5f);
        let _e170 = signedDistance;
        let _e171 = fwidth(_e170);
        screenWidth = max(_e171, 0.0009765625f);
        let _e173 = signedDistance;
        let _e174 = screenWidth;
        base = vec4<f32>(1f, 1f, 1f, clamp(((_e173 / _e174) + 0.5f), 0f, 1f));
    }
    let _e181 = productMaterial.beamFlags[0u];
    if (_e181 != 0u) {
        let _e183 = color_1;
        let _e190 = color_1;
        let _e194 = color_1;
        linearColor = select(pow(((_e183.xyz + vec3(0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f)), (_e190.xyz / vec3(12.92f)), (_e194.xyz <= vec3<f32>(0.04045f, 0.04045f, 0.04045f)));
        let _e198 = base;
        let _e200 = linearColor;
        let _e202 = wired_atmosphere_sample_u0028_();
        let _e204 = ((_e198.xyz * _e200) * _e202.w);
        let _e206 = base[3u];
        let _e208 = color_1[3u];
        outColor = vec4<f32>(_e204.x, _e204.y, _e204.z, (_e206 * _e208));
        return;
    }
    let _e216 = productDraw.atmosphereHeightSky[3u];
    let _e217 = (_e216 > 0.5f);
    phi_812_ = _e217;
    if _e217 {
        let _e220 = productDraw.emissiveRadiance[3u];
        phi_812_ = (_e220 >= 0f);
    }
    let _e223 = phi_812_;
    if _e223 {
        let _e224 = worldPosition_1;
        let _e226 = productDraw.atmosphereEyeDensity;
        direction_1 = normalize((_e224 - _e226.xyz));
        let _e230 = direction_1;
        param_8 = _e230;
        param_9 = 0f;
        let _e231 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_8), (&param_9));
        current = _e231;
        let _e232 = direction_1;
        param_10 = _e232;
        param_11 = 0f;
        let _e233 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_10), (&param_11));
        previous = _e233;
        let _e234 = previous;
        let _e235 = current;
        let _e238 = productDraw.emissiveRadiance[3u];
        let _e241 = mix(_e234, _e235, vec3(clamp(_e238, 0f, 1f)));
        base = vec4<f32>(_e241.x, _e241.y, _e241.z, 1f);
    }
    let _e248 = productDraw.atmosphereHeightSky[3u];
    let _e249 = (_e248 > 0.5f);
    phi_853_ = _e249;
    if _e249 {
        let _e252 = productDraw.atmosphereHeightSky[2u];
        phi_853_ = (_e252 > 0f);
    }
    let _e255 = phi_853_;
    if _e255 {
        let _e256 = worldPosition_1;
        let _e258 = productDraw.atmosphereEyeDensity;
        direction_2 = normalize((_e256 - _e258.xyz));
        let _e263 = direction_2[1u];
        let _e265 = direction_2[0u];
        let _e270 = direction_2[2u];
        physicalUv = vec2<f32>(((atan2(_e263, _e265) / 6.2831855f) + 0.5f), ((asin(clamp(_e270, -1f, 1f)) / 3.1415927f) + 0.5f));
        let _e276 = physicalUv;
        let _e277 = textureSample(physicalSkyTexture, physicalSkySampler, _e276);
        physical_1 = _e277.xyz;
        let _e281 = productDraw.atmosphereHeightSky[2u];
        let _e283 = base;
        param_12 = _e283.xyz;
        let _e285 = physical_1;
        param_13 = _e285;
        param_14 = clamp(_e281, 0f, 1f);
        let _e286 = wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
        param_15 = _e286;
        let _e287 = wired_display_visibility_u0028_vf3_u003b((&param_15));
        outColor = vec4<f32>(_e287.x, _e287.y, _e287.z, 1f);
        return;
    }
    let _e294 = productDraw.atmosphereHeightSky[3u];
    let _e295 = (_e294 > 0.5f);
    phi_910_ = _e295;
    if _e295 {
        let _e298 = productDraw.emissiveRadiance[3u];
        phi_910_ = (_e298 >= 0f);
    }
    let _e301 = phi_910_;
    if _e301 {
        let _e302 = base;
        param_16 = _e302.xyz;
        let _e304 = wired_display_visibility_u0028_vf3_u003b((&param_16));
        outColor = vec4<f32>(_e304.x, _e304.y, _e304.z, 1f);
        return;
    }
    let _e311 = productDraw.viewportAlpha[2u];
    alphaMode = i32((_e311 + 0.5f));
    let _e315 = productMaterial.baseColor;
    let _e316 = base;
    base = (_e316 * _e315);
    let _e318 = alphaMode;
    let _e319 = (_e318 == 1i);
    phi_940_ = _e319;
    if _e319 {
        let _e321 = base[3u];
        let _e324 = productDraw.viewportAlpha[3u];
        phi_940_ = (_e321 < _e324);
    }
    let _e327 = phi_940_;
    if _e327 {
        discard;
    }
    let _e330 = productDraw.localSh[0i];
    let _e334 = productDraw.localSh[1i];
    let _e337 = worldNormal_1[0u];
    let _e342 = productDraw.localSh[2i];
    let _e345 = worldNormal_1[1u];
    let _e350 = productDraw.localSh[3i];
    let _e353 = worldNormal_1[2u];
    localSh = max((((_e330.xyz + (_e334.xyz * _e337)) + (_e342.xyz * _e345)) + (_e350.xyz * _e353)), vec3<f32>(0f, 0f, 0f));
    let _e359 = productDraw.staticLighting[0u];
    if (_e359 == 2u) {
        let _e361 = lightmapCoord_1;
        let _e364 = productDraw.staticLighting[1u];
        address = vec3<f32>(_e361.x, _e361.y, f32(_e364));
        let _e369 = address;
        let _e375 = textureSample(staticRadianceTexture, staticRadianceSampler, vec2<f32>(_e369.x, _e369.y), i32(_e369.z));
        radiance_1 = _e375.xyz;
        let _e377 = address;
        let _e383 = textureSample(staticDirectionTexture, staticDirectionSampler, vec2<f32>(_e377.x, _e377.y), i32(_e377.z));
        encodedDirection_1 = _e383.xy;
        let _e387 = productDraw.staticLighting[2u];
        if (_e387 == 2u) {
            let _e389 = encodedDirection_1;
            encodedDirection_1 = ((_e389 * 0.5f) + vec2(0.5f));
        }
        let _e393 = radiance_1;
        param_17 = _e393;
        let _e394 = encodedDirection_1;
        param_18 = _e394;
        let _e395 = worldNormal_1;
        param_19 = _e395;
        let _e396 = wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b((&param_17), (&param_18), (&param_19));
        lighting = _e396;
        let _e399 = productDraw.staticLighting[3u];
        if (_e399 != 0u) {
            let _e401 = address;
            let _e407 = textureSample(staticVisibilityTexture, staticVisibilitySampler, vec2<f32>(_e401.x, _e401.y), i32(_e401.z));
            let _e409 = lighting;
            lighting = (_e409 * _e407.x);
        }
    } else {
        let _e413 = productDraw.staticLighting[0u];
        let _e414 = (_e413 == 1u);
        phi_1048_ = _e414;
        if !(_e414) {
            let _e418 = productDraw.projectionLightmap[3u];
            phi_1048_ = (_e418 >= 0.5f);
        }
        let _e421 = phi_1048_;
        if _e421 {
            let _e422 = lightmapCoord_1;
            let _e423 = textureSample(lightmapTexture, lightmapSampler, _e422);
            lighting = min((_e423.xyz * 2f), vec3<f32>(1f, 1f, 1f));
        } else {
            let _e430 = productDraw.localSh[0i][3u];
            let _e432 = localSh;
            lighting = select(vec3<f32>(1f, 1f, 1f), _e432, vec3((_e430 >= 0.5f)));
        }
    }
    let _e435 = base;
    let _e437 = lighting;
    let _e439 = color_1;
    let _e442 = base;
    let _e445 = productDraw.emissiveRadiance;
    lit = (((_e435.xyz * _e437) * _e439.xyz) + (_e442.xyz * _e445.xyz));
    let _e451 = productMaterial.factors[0u];
    if (_e451 >= 0f) {
        let _e455 = productMaterial.factors[0u];
        metallic = clamp(_e455, 0f, 1f);
        let _e459 = productMaterial.factors[1u];
        roughness = clamp(_e459, 0f, 1f);
        let _e463 = productMaterial.factors[2u];
        occlusion = clamp(_e463, 0f, 1f);
        let _e467 = productMaterial.factors[3u];
        environmentBlend = clamp(_e467, 0f, 1f);
        let _e469 = worldNormal_1;
        normal = normalize(_e469);
        let _e472 = productDraw.atmosphereEyeDensity;
        let _e474 = worldPosition_1;
        viewDirection = normalize((_e472.xyz - _e474));
        let _e477 = viewDirection;
        let _e479 = normal;
        reflected = reflect(-(_e477), _e479);
        let _e481 = roughness;
        let _e482 = textureNumLevels(currentSkyRadianceTexture);
        let _e487 = reflected;
        param_20 = _e487;
        param_21 = (_e481 * f32((i32(_e482) - 1i)));
        let _e488 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_20), (&param_21));
        currentSpecular = _e488;
        let _e489 = roughness;
        let _e490 = textureNumLevels(previousSkyRadianceTexture);
        let _e495 = reflected;
        param_22 = _e495;
        param_23 = (_e489 * f32((i32(_e490) - 1i)));
        let _e496 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_22), (&param_23));
        previousSpecular = _e496;
        let _e497 = base;
        let _e499 = metallic;
        f0_ = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e497.xyz, vec3(_e499));
        let _e502 = normal;
        let _e503 = viewDirection;
        nDotV = max(dot(_e502, _e503), 0f);
        let _e506 = f0_;
        let _e507 = f0_;
        let _e510 = nDotV;
        fresnel = (_e506 + ((vec3(1f) - _e507) * pow((1f - _e510), 5f)));
        let _e515 = previousSpecular;
        let _e516 = currentSpecular;
        let _e517 = environmentBlend;
        let _e520 = fresnel;
        specularIbl = (mix(_e515, _e516, vec3(_e517)) * _e520);
        let _e522 = specularIbl;
        let _e523 = occlusion;
        let _e525 = lit;
        lit = (_e525 + (_e522 * _e523));
    }
    let _e527 = wired_atmosphere_sample_u0028_();
    media = _e527;
    let _e528 = lit;
    let _e530 = media[3u];
    let _e532 = alphaMode;
    let _e534 = alphaMode;
    if ((_e532 == 3i) || (_e534 == 4i)) {
        local_1 = vec3<f32>(0f, 0f, 0f);
    } else {
        let _e537 = media;
        local_1 = _e537.xyz;
    }
    let _e539 = local_1;
    lit = ((_e528 * _e530) + _e539);
    let _e543 = productDraw.viewOriginDrawSpace[3u];
    if (_e543 < 0.5f) {
        let _e545 = lit;
        param_24 = _e545;
        let _e546 = wired_display_visibility_u0028_vf3_u003b((&param_24));
        lit = _e546;
    }
    let _e547 = lit;
    let _e549 = base[3u];
    let _e551 = color_1[3u];
    outColor = vec4<f32>(_e547.x, _e547.y, _e547.z, (_e549 * _e551));
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
