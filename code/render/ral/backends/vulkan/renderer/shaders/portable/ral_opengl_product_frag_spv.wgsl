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

    let _e103 = (*encoded);
    f = ((_e103 * 2f) - vec2(1f));
    let _e107 = f;
    let _e109 = f[0u];
    let _e113 = f[1u];
    n = vec3<f32>(_e107.x, _e107.y, ((1f - abs(_e109)) - abs(_e113)));
    let _e120 = n[2u];
    if (_e120 < 0f) {
        let _e122 = n;
        let _e127 = n;
        folded = ((vec2(1f) - abs(_e122.yx)) * sign(_e127.xy));
        let _e131 = folded;
        n[0u] = _e131.x;
        n[1u] = _e131.y;
    }
    let _e136 = n;
    return normalize(_e136);
}

fn wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b(radiance: ptr<function, vec3<f32>>, encodedDirection: ptr<function, vec2<f32>>, materialNormal: ptr<function, vec3<f32>>) -> vec3<f32> {
    var dominantDirection: vec3<f32>;
    var param: vec2<f32>;

    let _e104 = (*encodedDirection);
    param = _e104;
    let _e105 = wired_lighting_decode_oct_u0028_vf2_u003b((&param));
    dominantDirection = _e105;
    let _e106 = (*radiance);
    let _e108 = (*materialNormal);
    let _e110 = dominantDirection;
    return (max(_e106, vec3<f32>(0f, 0f, 0f)) * max(dot(normalize(_e108), _e110), 0f));
}

fn wired_display_visibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_274_: bool;
    var phi_281_: bool;

    let _e103 = (*sceneColor);
    let _e107 = productDraw.viewForward[3u];
    exposed = (max(_e103, vec3<f32>(0f, 0f, 0f)) * _e107);
    let _e109 = exposed;
    luminance = dot(_e109, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e111 = luminance;
    let _e112 = (_e111 > 0f);
    phi_274_ = _e112;
    if _e112 {
        let _e113 = luminance;
        let _e116 = productDraw.viewUp[3u];
        phi_274_ = (_e113 < _e116);
    }
    let _e119 = phi_274_;
    phi_281_ = _e119;
    if _e119 {
        let _e122 = productDraw.viewLeft[3u];
        phi_281_ = (_e122 != 1f);
    }
    let _e125 = phi_281_;
    if _e125 {
        let _e126 = luminance;
        let _e129 = productDraw.viewUp[3u];
        let _e133 = productDraw.viewLeft[3u];
        let _e137 = productDraw.viewUp[3u];
        curved = (pow((_e126 / _e129), _e133) * _e137);
        let _e139 = curved;
        let _e140 = luminance;
        let _e142 = exposed;
        exposed = (_e142 * (_e139 / _e140));
    }
    let _e144 = exposed;
    return _e144;
}

fn wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b(authored: ptr<function, vec3<f32>>, physical: ptr<function, vec3<f32>>, blend: ptr<function, f32>) -> vec3<f32> {
    var authoredEnergy: f32;
    var physicalEnergy: f32;
    var scale: f32;
    var physicalTint: vec3<f32>;
    var local: vec3<f32>;
    var relit: vec3<f32>;

    let _e108 = (*authored);
    authoredEnergy = dot(max(_e108, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e111 = (*physical);
    physicalEnergy = dot(max(_e111, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e114 = authoredEnergy;
    if (_e114 <= 0.00001f) {
        let _e116 = (*authored);
        let _e117 = (*physical);
        let _e118 = (*blend);
        return mix(_e116, _e117, vec3(_e118));
    }
    let _e121 = physicalEnergy;
    let _e122 = authoredEnergy;
    scale = clamp((_e121 / max(_e122, 0.18f)), 0.65f, 1.75f);
    let _e126 = physicalEnergy;
    if (_e126 > 0.00001f) {
        let _e128 = (*physical);
        let _e129 = physicalEnergy;
        local = (_e128 / vec3(_e129));
    } else {
        local = vec3<f32>(1f, 1f, 1f);
    }
    let _e132 = local;
    physicalTint = _e132;
    let _e133 = (*authored);
    let _e134 = scale;
    let _e136 = physicalTint;
    let _e141 = (*physical);
    relit = (((_e133 * _e134) * mix(vec3<f32>(1f, 1f, 1f), clamp(_e136, vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(2f, 2f, 2f)), vec3(0.12f))) + (_e141 * 0.08f));
    let _e144 = (*authored);
    let _e145 = relit;
    let _e146 = (*blend);
    return mix(_e144, _e145, vec3(_e146));
}

fn wired_sky_cube_uv_layer_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: vec3<f32>;
    var face: u32;
    var uv: vec2<f32>;
    var phi_135_: bool;

    let _e103 = (*dir);
    a = abs(_e103);
    let _e106 = a[0u];
    let _e108 = a[1u];
    let _e109 = (_e106 >= _e108);
    phi_135_ = _e109;
    if _e109 {
        let _e111 = a[0u];
        let _e113 = a[2u];
        phi_135_ = (_e111 >= _e113);
    }
    let _e116 = phi_135_;
    if _e116 {
        let _e118 = (*dir)[0u];
        if (_e118 >= 0f) {
            face = 0u;
            let _e121 = (*dir)[2u];
            let _e124 = (*dir)[1u];
            let _e128 = a[0u];
            uv = (vec2<f32>(-(_e121), -(_e124)) / vec2(_e128));
        } else {
            face = 1u;
            let _e132 = (*dir)[2u];
            let _e134 = (*dir)[1u];
            let _e138 = a[0u];
            uv = (vec2<f32>(_e132, -(_e134)) / vec2(_e138));
        }
    } else {
        let _e142 = a[1u];
        let _e144 = a[2u];
        if (_e142 >= _e144) {
            let _e147 = (*dir)[1u];
            if (_e147 >= 0f) {
                face = 2u;
                let _e150 = (*dir)[0u];
                let _e152 = (*dir)[2u];
                let _e155 = a[1u];
                uv = (vec2<f32>(_e150, _e152) / vec2(_e155));
            } else {
                face = 3u;
                let _e159 = (*dir)[0u];
                let _e161 = (*dir)[2u];
                let _e165 = a[1u];
                uv = (vec2<f32>(_e159, -(_e161)) / vec2(_e165));
            }
        } else {
            let _e169 = (*dir)[2u];
            if (_e169 >= 0f) {
                face = 4u;
                let _e172 = (*dir)[0u];
                let _e174 = (*dir)[1u];
                let _e178 = a[2u];
                uv = (vec2<f32>(_e172, -(_e174)) / vec2(_e178));
            } else {
                face = 5u;
                let _e182 = (*dir)[0u];
                let _e185 = (*dir)[1u];
                let _e189 = a[2u];
                uv = (vec2<f32>(-(_e182), -(_e185)) / vec2(_e189));
            }
        }
    }
    let _e192 = uv;
    let _e195 = ((_e192 * 0.5f) + vec2(0.5f));
    let _e196 = face;
    return vec3<f32>(_e195.x, _e195.y, f32(_e196));
}

fn wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(skyTexture: texture_2d_array<f32>, skySampler: sampler, direction: ptr<function, vec3<f32>>, lod: ptr<function, f32>) -> vec3<f32> {
    var param_1: vec3<f32>;

    let _e104 = (*direction);
    param_1 = _e104;
    let _e105 = wired_sky_cube_uv_layer_u0028_vf3_u003b((&param_1));
    let _e106 = (*lod);
    let _e112 = textureSampleLevel(skyTexture, skySampler, vec2<f32>(_e105.x, _e105.y), i32(_e105.z), _e106);
    return _e112.xyz;
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

    let _e120 = productDraw.atmosphereFroxelGrid[3u];
    packedHaze = _e120;
    let _e121 = packedHaze;
    aerial = (f32((_e121 >> bitcast<u32>(24u))) / 255f);
    let _e126 = aerial;
    if (_e126 <= 0f) {
        let _e128 = (*mediaColor);
        return _e128;
    }
    let _e131 = productDraw.atmosphereHeightSky[2u];
    if (_e131 > 0f) {
        let _e134 = (*viewDir)[1u];
        let _e136 = (*viewDir)[0u];
        let _e142 = (*viewDir)[2u];
        skyViewUv = vec2<f32>(fract(((atan2(_e134, _e136) / 6.2831855f) + 0.5f)), clamp(((asin(clamp(_e142, -1f, 1f)) / 3.1415927f) + 0.5f), 0f, 1f));
        let _e149 = skyViewUv;
        let _e150 = textureSampleLevel(physicalSkyTexture, physicalSkySampler, _e149, 0f);
        skyIn = _e150.xyz;
    } else {
        let _e152 = (*viewDir);
        param_2 = _e152;
        param_3 = 4f;
        let _e153 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_2), (&param_3));
        skyIn = _e153;
    }
    skySum = vec3<f32>(0f, 0f, 0f);
    face_1 = 0i;
    loop {
        let _e154 = face_1;
        if (_e154 < 6i) {
            axis = vec3<f32>(0f, 0f, 0f);
            let _e156 = face_1;
            let _e158 = face_1;
            axis[(_e156 / 2i)] = select(1f, -1f, ((_e158 & 1i) != 0i));
            let _e163 = axis;
            param_4 = _e163;
            param_5 = 7f;
            let _e164 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_4), (&param_5));
            let _e165 = skySum;
            skySum = (_e165 + _e164);
            continue;
        } else {
            break;
        }
        continuing {
            let _e167 = face_1;
            face_1 = (_e167 + 1i);
        }
    }
    let _e169 = skySum;
    skyMean = max(dot((_e169 / vec3(6f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f)), 0.0001f);
    let _e174 = skyIn;
    let _e175 = skyMean;
    directional = (_e174 / vec3(_e175));
    let _e178 = packedHaze;
    sunDir = normalize((unpack4x8snorm(_e178).xyz + vec3<f32>(0.000001f, 0.000001f, 0.000001f)));
    let _e183 = (*viewDir);
    let _e184 = sunDir;
    cosT = dot(_e183, _e184);
    let _e186 = cosT;
    hg = (0.6975f / max(pow((1.3025f - (1.1f * _e186)), 1.5f), 0.0001f));
    let _e192 = (*mediaColor);
    let _e193 = directional;
    let _e195 = aerial;
    tinted = (_e192 * mix(vec3<f32>(1f, 1f, 1f), clamp(_e193, vec3<f32>(0.35f, 0.35f, 0.35f), vec3<f32>(3f, 3f, 3f)), vec3(_e195)));
    let _e199 = tinted;
    let _e200 = hg;
    let _e203 = aerial;
    return (_e199 * mix(1f, (0.75f + (0.25f * _e200)), _e203));
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

    let _e115 = productDraw.viewOriginDrawSpace[3u];
    if (_e115 >= 0.5f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e119 = productDraw.atmosphereFroxelGrid[2u];
    if (_e119 > 0u) {
        let _e122 = productDraw.atmosphereFroxelGrid;
        grid = _e122.xyz;
        let _e125 = gl_FragCoord_1[0u];
        let _e129 = grid[0u];
        x = min((u32(_e125) / 16u), (_e129 - 1u));
        let _e133 = gl_FragCoord_1[1u];
        let _e137 = grid[1u];
        y = min((u32(_e133) / 16u), (_e137 - 1u));
        let _e142 = productDraw.atmosphereColorVisibility[3u];
        farDistance = max(_e142, 4.01f);
        let _e144 = worldPosition_1;
        let _e146 = productDraw.atmosphereEyeDensity;
        let _e150 = productDraw.viewForward;
        viewDistance = dot((_e144 - _e146.xyz), _e150.xyz);
        let _e153 = viewDistance;
        let _e157 = farDistance;
        slice = clamp((log((max(_e153, 4f) / 4f)) / log((_e157 / 4f))), 0f, 0.9999f);
        let _e162 = slice;
        let _e164 = grid[2u];
        let _e169 = grid[2u];
        z = min(u32((_e162 * f32(_e164))), (_e169 - 1u));
        let _e174 = productDraw.atmosphereFroxelGrid[3u];
        let _e175 = z;
        let _e177 = grid[1u];
        let _e179 = y;
        let _e182 = grid[0u];
        let _e185 = x;
        let _e189 = atmosphereFroxels.values[((_e174 + (((_e175 * _e177) + _e179) * _e182)) + _e185)];
        return _e189;
    }
    let _e192 = productDraw.atmosphereEyeDensity[3u];
    density = _e192;
    let _e195 = productDraw.atmosphereColorVisibility[3u];
    if (_e195 > 0f) {
        let _e197 = density;
        let _e200 = productDraw.atmosphereColorVisibility[3u];
        density = max(_e197, (3.912023f / _e200));
    }
    let _e203 = density;
    if (_e203 <= 0f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e205 = worldPosition_1;
    let _e207 = productDraw.atmosphereEyeDensity;
    distanceToEye = length((_e205 - _e207.xyz));
    let _e213 = productDraw.atmosphereHeightSky[1u];
    let _e216 = worldPosition_1[2u];
    let _e219 = productDraw.atmosphereHeightSky[0u];
    heightWeight = exp((-(_e213) * max((_e216 - _e219), 0f)));
    let _e224 = density;
    let _e226 = heightWeight;
    let _e228 = distanceToEye;
    transmittance = clamp(exp(((-(_e224) * _e226) * _e228)), 0f, 1f);
    let _e232 = worldPosition_1;
    let _e234 = productDraw.atmosphereEyeDensity;
    let _e239 = productDraw.atmosphereColorVisibility;
    param_6 = _e239.xyz;
    param_7 = normalize((_e232 - _e234.xyz));
    let _e241 = wired_aerial_inscatter_u0028_vf3_u003b_vf3_u003b((&param_6), (&param_7));
    inscatter = _e241;
    let _e242 = inscatter;
    let _e243 = transmittance;
    let _e245 = (_e242 * (1f - _e243));
    let _e246 = transmittance;
    return vec4<f32>(_e245.x, _e245.y, _e245.z, _e246);
}

fn main_1() {
    var base: vec4<f32>;
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
    var phi_780_: bool;
    var phi_821_: bool;
    var phi_878_: bool;
    var phi_908_: bool;
    var phi_1016_: bool;

    let _e146 = texCoord_1;
    let _e147 = textureSample(baseTexture, baseSampler, _e146);
    base = _e147;
    let _e150 = productMaterial.beamFlags[0u];
    if (_e150 != 0u) {
        let _e152 = color_1;
        let _e159 = color_1;
        let _e163 = color_1;
        linearColor = select(pow(((_e152.xyz + vec3(0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f)), (_e159.xyz / vec3(12.92f)), (_e163.xyz <= vec3<f32>(0.04045f, 0.04045f, 0.04045f)));
        let _e167 = base;
        let _e169 = linearColor;
        let _e171 = wired_atmosphere_sample_u0028_();
        let _e173 = ((_e167.xyz * _e169) * _e171.w);
        let _e175 = base[3u];
        let _e177 = color_1[3u];
        outColor = vec4<f32>(_e173.x, _e173.y, _e173.z, (_e175 * _e177));
        return;
    }
    let _e185 = productDraw.atmosphereHeightSky[3u];
    let _e186 = (_e185 > 0.5f);
    phi_780_ = _e186;
    if _e186 {
        let _e189 = productDraw.emissiveRadiance[3u];
        phi_780_ = (_e189 >= 0f);
    }
    let _e192 = phi_780_;
    if _e192 {
        let _e193 = worldPosition_1;
        let _e195 = productDraw.atmosphereEyeDensity;
        direction_1 = normalize((_e193 - _e195.xyz));
        let _e199 = direction_1;
        param_8 = _e199;
        param_9 = 0f;
        let _e200 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_8), (&param_9));
        current = _e200;
        let _e201 = direction_1;
        param_10 = _e201;
        param_11 = 0f;
        let _e202 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_10), (&param_11));
        previous = _e202;
        let _e203 = previous;
        let _e204 = current;
        let _e207 = productDraw.emissiveRadiance[3u];
        let _e210 = mix(_e203, _e204, vec3(clamp(_e207, 0f, 1f)));
        base = vec4<f32>(_e210.x, _e210.y, _e210.z, 1f);
    }
    let _e217 = productDraw.atmosphereHeightSky[3u];
    let _e218 = (_e217 > 0.5f);
    phi_821_ = _e218;
    if _e218 {
        let _e221 = productDraw.atmosphereHeightSky[2u];
        phi_821_ = (_e221 > 0f);
    }
    let _e224 = phi_821_;
    if _e224 {
        let _e225 = worldPosition_1;
        let _e227 = productDraw.atmosphereEyeDensity;
        direction_2 = normalize((_e225 - _e227.xyz));
        let _e232 = direction_2[1u];
        let _e234 = direction_2[0u];
        let _e239 = direction_2[2u];
        physicalUv = vec2<f32>(((atan2(_e232, _e234) / 6.2831855f) + 0.5f), ((asin(clamp(_e239, -1f, 1f)) / 3.1415927f) + 0.5f));
        let _e245 = physicalUv;
        let _e246 = textureSample(physicalSkyTexture, physicalSkySampler, _e245);
        physical_1 = _e246.xyz;
        let _e250 = productDraw.atmosphereHeightSky[2u];
        let _e252 = base;
        param_12 = _e252.xyz;
        let _e254 = physical_1;
        param_13 = _e254;
        param_14 = clamp(_e250, 0f, 1f);
        let _e255 = wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_12), (&param_13), (&param_14));
        param_15 = _e255;
        let _e256 = wired_display_visibility_u0028_vf3_u003b((&param_15));
        outColor = vec4<f32>(_e256.x, _e256.y, _e256.z, 1f);
        return;
    }
    let _e263 = productDraw.atmosphereHeightSky[3u];
    let _e264 = (_e263 > 0.5f);
    phi_878_ = _e264;
    if _e264 {
        let _e267 = productDraw.emissiveRadiance[3u];
        phi_878_ = (_e267 >= 0f);
    }
    let _e270 = phi_878_;
    if _e270 {
        let _e271 = base;
        param_16 = _e271.xyz;
        let _e273 = wired_display_visibility_u0028_vf3_u003b((&param_16));
        outColor = vec4<f32>(_e273.x, _e273.y, _e273.z, 1f);
        return;
    }
    let _e280 = productDraw.viewportAlpha[2u];
    alphaMode = i32((_e280 + 0.5f));
    let _e284 = productMaterial.baseColor;
    let _e285 = base;
    base = (_e285 * _e284);
    let _e287 = alphaMode;
    let _e288 = (_e287 == 1i);
    phi_908_ = _e288;
    if _e288 {
        let _e290 = base[3u];
        let _e293 = productDraw.viewportAlpha[3u];
        phi_908_ = (_e290 < _e293);
    }
    let _e296 = phi_908_;
    if _e296 {
        discard;
    }
    let _e299 = productDraw.localSh[0i];
    let _e303 = productDraw.localSh[1i];
    let _e306 = worldNormal_1[0u];
    let _e311 = productDraw.localSh[2i];
    let _e314 = worldNormal_1[1u];
    let _e319 = productDraw.localSh[3i];
    let _e322 = worldNormal_1[2u];
    localSh = max((((_e299.xyz + (_e303.xyz * _e306)) + (_e311.xyz * _e314)) + (_e319.xyz * _e322)), vec3<f32>(0f, 0f, 0f));
    let _e328 = productDraw.staticLighting[0u];
    if (_e328 == 2u) {
        let _e330 = lightmapCoord_1;
        let _e333 = productDraw.staticLighting[1u];
        address = vec3<f32>(_e330.x, _e330.y, f32(_e333));
        let _e338 = address;
        let _e344 = textureSample(staticRadianceTexture, staticRadianceSampler, vec2<f32>(_e338.x, _e338.y), i32(_e338.z));
        radiance_1 = _e344.xyz;
        let _e346 = address;
        let _e352 = textureSample(staticDirectionTexture, staticDirectionSampler, vec2<f32>(_e346.x, _e346.y), i32(_e346.z));
        encodedDirection_1 = _e352.xy;
        let _e356 = productDraw.staticLighting[2u];
        if (_e356 == 2u) {
            let _e358 = encodedDirection_1;
            encodedDirection_1 = ((_e358 * 0.5f) + vec2(0.5f));
        }
        let _e362 = radiance_1;
        param_17 = _e362;
        let _e363 = encodedDirection_1;
        param_18 = _e363;
        let _e364 = worldNormal_1;
        param_19 = _e364;
        let _e365 = wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b((&param_17), (&param_18), (&param_19));
        lighting = _e365;
        let _e368 = productDraw.staticLighting[3u];
        if (_e368 != 0u) {
            let _e370 = address;
            let _e376 = textureSample(staticVisibilityTexture, staticVisibilitySampler, vec2<f32>(_e370.x, _e370.y), i32(_e370.z));
            let _e378 = lighting;
            lighting = (_e378 * _e376.x);
        }
    } else {
        let _e382 = productDraw.staticLighting[0u];
        let _e383 = (_e382 == 1u);
        phi_1016_ = _e383;
        if !(_e383) {
            let _e387 = productDraw.projectionLightmap[3u];
            phi_1016_ = (_e387 >= 0.5f);
        }
        let _e390 = phi_1016_;
        if _e390 {
            let _e391 = lightmapCoord_1;
            let _e392 = textureSample(lightmapTexture, lightmapSampler, _e391);
            lighting = min((_e392.xyz * 2f), vec3<f32>(1f, 1f, 1f));
        } else {
            let _e399 = productDraw.localSh[0i][3u];
            let _e401 = localSh;
            lighting = select(vec3<f32>(1f, 1f, 1f), _e401, vec3((_e399 >= 0.5f)));
        }
    }
    let _e404 = base;
    let _e406 = lighting;
    let _e408 = color_1;
    let _e411 = base;
    let _e414 = productDraw.emissiveRadiance;
    lit = (((_e404.xyz * _e406) * _e408.xyz) + (_e411.xyz * _e414.xyz));
    let _e420 = productMaterial.factors[0u];
    if (_e420 >= 0f) {
        let _e424 = productMaterial.factors[0u];
        metallic = clamp(_e424, 0f, 1f);
        let _e428 = productMaterial.factors[1u];
        roughness = clamp(_e428, 0f, 1f);
        let _e432 = productMaterial.factors[2u];
        occlusion = clamp(_e432, 0f, 1f);
        let _e436 = productMaterial.factors[3u];
        environmentBlend = clamp(_e436, 0f, 1f);
        let _e438 = worldNormal_1;
        normal = normalize(_e438);
        let _e441 = productDraw.atmosphereEyeDensity;
        let _e443 = worldPosition_1;
        viewDirection = normalize((_e441.xyz - _e443));
        let _e446 = viewDirection;
        let _e448 = normal;
        reflected = reflect(-(_e446), _e448);
        let _e450 = roughness;
        let _e451 = textureNumLevels(currentSkyRadianceTexture);
        let _e456 = reflected;
        param_20 = _e456;
        param_21 = (_e450 * f32((i32(_e451) - 1i)));
        let _e457 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_20), (&param_21));
        currentSpecular = _e457;
        let _e458 = roughness;
        let _e459 = textureNumLevels(previousSkyRadianceTexture);
        let _e464 = reflected;
        param_22 = _e464;
        param_23 = (_e458 * f32((i32(_e459) - 1i)));
        let _e465 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_22), (&param_23));
        previousSpecular = _e465;
        let _e466 = base;
        let _e468 = metallic;
        f0_ = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e466.xyz, vec3(_e468));
        let _e471 = normal;
        let _e472 = viewDirection;
        nDotV = max(dot(_e471, _e472), 0f);
        let _e475 = f0_;
        let _e476 = f0_;
        let _e479 = nDotV;
        fresnel = (_e475 + ((vec3(1f) - _e476) * pow((1f - _e479), 5f)));
        let _e484 = previousSpecular;
        let _e485 = currentSpecular;
        let _e486 = environmentBlend;
        let _e489 = fresnel;
        specularIbl = (mix(_e484, _e485, vec3(_e486)) * _e489);
        let _e491 = specularIbl;
        let _e492 = occlusion;
        let _e494 = lit;
        lit = (_e494 + (_e491 * _e492));
    }
    let _e496 = wired_atmosphere_sample_u0028_();
    media = _e496;
    let _e497 = lit;
    let _e499 = media[3u];
    let _e501 = alphaMode;
    let _e503 = alphaMode;
    if ((_e501 == 3i) || (_e503 == 4i)) {
        local_1 = vec3<f32>(0f, 0f, 0f);
    } else {
        let _e506 = media;
        local_1 = _e506.xyz;
    }
    let _e508 = local_1;
    lit = ((_e497 * _e499) + _e508);
    let _e512 = productDraw.viewOriginDrawSpace[3u];
    if (_e512 < 0.5f) {
        let _e514 = lit;
        param_24 = _e514;
        let _e515 = wired_display_visibility_u0028_vf3_u003b((&param_24));
        lit = _e515;
    }
    let _e516 = lit;
    let _e518 = base[3u];
    let _e520 = color_1[3u];
    outColor = vec4<f32>(_e516.x, _e516.y, _e516.z, (_e518 * _e520));
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
