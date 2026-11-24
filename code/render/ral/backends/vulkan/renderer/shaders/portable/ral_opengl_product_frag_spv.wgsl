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

    let _e106 = (*encoded);
    f = ((_e106 * 2f) - vec2(1f));
    let _e110 = f;
    let _e112 = f[0u];
    let _e116 = f[1u];
    n = vec3<f32>(_e110.x, _e110.y, ((1f - abs(_e112)) - abs(_e116)));
    let _e123 = n[2u];
    if (_e123 < 0f) {
        let _e125 = n;
        let _e130 = n;
        folded = ((vec2(1f) - abs(_e125.yx)) * sign(_e130.xy));
        let _e134 = folded;
        n[0u] = _e134.x;
        n[1u] = _e134.y;
    }
    let _e139 = n;
    return normalize(_e139);
}

fn wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b(radiance: ptr<function, vec3<f32>>, encodedDirection: ptr<function, vec2<f32>>, materialNormal: ptr<function, vec3<f32>>) -> vec3<f32> {
    var dominantDirection: vec3<f32>;
    var param: vec2<f32>;

    let _e107 = (*encodedDirection);
    param = _e107;
    let _e108 = wired_lighting_decode_oct_u0028_vf2_u003b((&param));
    dominantDirection = _e108;
    let _e109 = (*radiance);
    let _e111 = (*materialNormal);
    let _e113 = dominantDirection;
    return (max(_e109, vec3<f32>(0f, 0f, 0f)) * max(dot(normalize(_e111), _e113), 0f));
}

fn wired_display_visibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_279_: bool;
    var phi_286_: bool;

    let _e106 = (*sceneColor);
    let _e110 = productDraw.viewForward[3u];
    exposed = (max(_e106, vec3<f32>(0f, 0f, 0f)) * _e110);
    let _e112 = exposed;
    luminance = dot(_e112, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e114 = luminance;
    let _e115 = (_e114 > 0f);
    phi_279_ = _e115;
    if _e115 {
        let _e116 = luminance;
        let _e119 = productDraw.viewUp[3u];
        phi_279_ = (_e116 < _e119);
    }
    let _e122 = phi_279_;
    phi_286_ = _e122;
    if _e122 {
        let _e125 = productDraw.viewLeft[3u];
        phi_286_ = (_e125 != 1f);
    }
    let _e128 = phi_286_;
    if _e128 {
        let _e129 = luminance;
        let _e132 = productDraw.viewUp[3u];
        let _e136 = productDraw.viewLeft[3u];
        let _e140 = productDraw.viewUp[3u];
        curved = (pow((_e129 / _e132), _e136) * _e140);
        let _e142 = curved;
        let _e143 = luminance;
        let _e145 = exposed;
        exposed = (_e145 * (_e142 / _e143));
    }
    let _e147 = exposed;
    return _e147;
}

fn wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b(authored: ptr<function, vec3<f32>>, physical: ptr<function, vec3<f32>>, blend: ptr<function, f32>) -> vec3<f32> {
    var authoredEnergy: f32;
    var physicalEnergy: f32;
    var scale: f32;
    var physicalTint: vec3<f32>;
    var local: vec3<f32>;
    var relit: vec3<f32>;

    let _e111 = (*authored);
    authoredEnergy = dot(max(_e111, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e114 = (*physical);
    physicalEnergy = dot(max(_e114, vec3<f32>(0f, 0f, 0f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e117 = authoredEnergy;
    if (_e117 <= 0.00001f) {
        let _e119 = (*authored);
        let _e120 = (*physical);
        let _e121 = (*blend);
        return mix(_e119, _e120, vec3(_e121));
    }
    let _e124 = physicalEnergy;
    let _e125 = authoredEnergy;
    scale = clamp((_e124 / max(_e125, 0.18f)), 0.65f, 1.75f);
    let _e129 = physicalEnergy;
    if (_e129 > 0.00001f) {
        let _e131 = (*physical);
        let _e132 = physicalEnergy;
        local = (_e131 / vec3(_e132));
    } else {
        local = vec3<f32>(1f, 1f, 1f);
    }
    let _e135 = local;
    physicalTint = _e135;
    let _e136 = (*authored);
    let _e137 = scale;
    let _e139 = physicalTint;
    let _e144 = (*physical);
    relit = (((_e136 * _e137) * mix(vec3<f32>(1f, 1f, 1f), clamp(_e139, vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(2f, 2f, 2f)), vec3(0.12f))) + (_e144 * 0.08f));
    let _e147 = (*authored);
    let _e148 = relit;
    let _e149 = (*blend);
    return mix(_e147, _e148, vec3(_e149));
}

fn wired_sky_cube_uv_layer_u0028_vf3_u003b(dir: ptr<function, vec3<f32>>) -> vec3<f32> {
    var a: vec3<f32>;
    var face: u32;
    var uv: vec2<f32>;
    var phi_140_: bool;

    let _e106 = (*dir);
    a = abs(_e106);
    let _e109 = a[0u];
    let _e111 = a[1u];
    let _e112 = (_e109 >= _e111);
    phi_140_ = _e112;
    if _e112 {
        let _e114 = a[0u];
        let _e116 = a[2u];
        phi_140_ = (_e114 >= _e116);
    }
    let _e119 = phi_140_;
    if _e119 {
        let _e121 = (*dir)[0u];
        if (_e121 >= 0f) {
            face = 0u;
            let _e124 = (*dir)[2u];
            let _e127 = (*dir)[1u];
            let _e131 = a[0u];
            uv = (vec2<f32>(-(_e124), -(_e127)) / vec2(_e131));
        } else {
            face = 1u;
            let _e135 = (*dir)[2u];
            let _e137 = (*dir)[1u];
            let _e141 = a[0u];
            uv = (vec2<f32>(_e135, -(_e137)) / vec2(_e141));
        }
    } else {
        let _e145 = a[1u];
        let _e147 = a[2u];
        if (_e145 >= _e147) {
            let _e150 = (*dir)[1u];
            if (_e150 >= 0f) {
                face = 2u;
                let _e153 = (*dir)[0u];
                let _e155 = (*dir)[2u];
                let _e158 = a[1u];
                uv = (vec2<f32>(_e153, _e155) / vec2(_e158));
            } else {
                face = 3u;
                let _e162 = (*dir)[0u];
                let _e164 = (*dir)[2u];
                let _e168 = a[1u];
                uv = (vec2<f32>(_e162, -(_e164)) / vec2(_e168));
            }
        } else {
            let _e172 = (*dir)[2u];
            if (_e172 >= 0f) {
                face = 4u;
                let _e175 = (*dir)[0u];
                let _e177 = (*dir)[1u];
                let _e181 = a[2u];
                uv = (vec2<f32>(_e175, -(_e177)) / vec2(_e181));
            } else {
                face = 5u;
                let _e185 = (*dir)[0u];
                let _e188 = (*dir)[1u];
                let _e192 = a[2u];
                uv = (vec2<f32>(-(_e185), -(_e188)) / vec2(_e192));
            }
        }
    }
    let _e195 = uv;
    let _e198 = ((_e195 * 0.5f) + vec2(0.5f));
    let _e199 = face;
    return vec3<f32>(_e198.x, _e198.y, f32(_e199));
}

fn wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(skyTexture: texture_2d_array<f32>, skySampler: sampler, direction: ptr<function, vec3<f32>>, lod: ptr<function, f32>) -> vec3<f32> {
    var param_1: vec3<f32>;

    let _e107 = (*direction);
    param_1 = _e107;
    let _e108 = wired_sky_cube_uv_layer_u0028_vf3_u003b((&param_1));
    let _e109 = (*lod);
    let _e115 = textureSampleLevel(skyTexture, skySampler, vec2<f32>(_e108.x, _e108.y), i32(_e108.z), _e109);
    return _e115.xyz;
}

fn wiredWriteTrust_u0028_vf4_u003b(sceneColor_1: ptr<function, vec4<f32>>) {
    var marking: bool;
    var blended: bool;

    let _e107 = productMaterial.beamFlags[2u];
    marking = (_e107 != 0u);
    let _e111 = productMaterial.beamFlags[3u];
    blended = (_e111 != 0u);
    let _e113 = blended;
    if !(_e113) {
        let _e115 = marking;
        outCoverage = vec4<f32>(select(1f, 0f, _e115), 0f, 0f, 1f);
    } else {
        let _e118 = marking;
        if _e118 {
            let _e120 = (*sceneColor_1)[0u];
            let _e122 = (*sceneColor_1)[1u];
            let _e124 = (*sceneColor_1)[2u];
            let _e129 = (*sceneColor_1)[3u];
            outCoverage = vec4<f32>(clamp(max(_e120, max(_e122, _e124)), 0f, 1f), 0f, 0f, clamp(_e129, 0f, 1f));
        } else {
            outCoverage = vec4<f32>(0f, 0f, 0f, 0f);
        }
    }
    return;
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

    let _e123 = productDraw.atmosphereFroxelGrid[3u];
    packedHaze = _e123;
    let _e124 = packedHaze;
    aerial = (f32((_e124 >> bitcast<u32>(24u))) / 255f);
    let _e129 = aerial;
    if (_e129 <= 0f) {
        let _e131 = (*mediaColor);
        return _e131;
    }
    let _e134 = productDraw.atmosphereHeightSky[2u];
    if (_e134 > 0f) {
        let _e137 = (*viewDir)[1u];
        let _e139 = (*viewDir)[0u];
        let _e145 = (*viewDir)[2u];
        skyViewUv = vec2<f32>(fract(((atan2(_e137, _e139) / 6.2831855f) + 0.5f)), clamp(((asin(clamp(_e145, -1f, 1f)) / 3.1415927f) + 0.5f), 0f, 1f));
        let _e152 = skyViewUv;
        let _e153 = textureSampleLevel(physicalSkyTexture, physicalSkySampler, _e152, 0f);
        skyIn = _e153.xyz;
    } else {
        let _e155 = (*viewDir);
        param_2 = _e155;
        param_3 = 4f;
        let _e156 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_2), (&param_3));
        skyIn = _e156;
    }
    skySum = vec3<f32>(0f, 0f, 0f);
    face_1 = 0i;
    loop {
        let _e157 = face_1;
        if (_e157 < 6i) {
            axis = vec3<f32>(0f, 0f, 0f);
            let _e159 = face_1;
            let _e161 = face_1;
            axis[(_e159 / 2i)] = select(1f, -1f, ((_e161 & 1i) != 0i));
            let _e166 = axis;
            param_4 = _e166;
            param_5 = 7f;
            let _e167 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_4), (&param_5));
            let _e168 = skySum;
            skySum = (_e168 + _e167);
            continue;
        } else {
            break;
        }
        continuing {
            let _e170 = face_1;
            face_1 = (_e170 + 1i);
        }
    }
    let _e172 = skySum;
    skyMean = max(dot((_e172 / vec3(6f)), vec3<f32>(0.2126f, 0.7152f, 0.0722f)), 0.0001f);
    let _e177 = skyIn;
    let _e178 = skyMean;
    directional = (_e177 / vec3(_e178));
    let _e181 = packedHaze;
    sunDir = normalize((unpack4x8snorm(_e181).xyz + vec3<f32>(0.000001f, 0.000001f, 0.000001f)));
    let _e186 = (*viewDir);
    let _e187 = sunDir;
    cosT = dot(_e186, _e187);
    let _e189 = cosT;
    hg = (0.6975f / max(pow((1.3025f - (1.1f * _e189)), 1.5f), 0.0001f));
    let _e195 = (*mediaColor);
    let _e196 = directional;
    let _e198 = aerial;
    tinted = (_e195 * mix(vec3<f32>(1f, 1f, 1f), clamp(_e196, vec3<f32>(0.35f, 0.35f, 0.35f), vec3<f32>(3f, 3f, 3f)), vec3(_e198)));
    let _e202 = tinted;
    let _e203 = hg;
    let _e206 = aerial;
    return (_e202 * mix(1f, (0.75f + (0.25f * _e203)), _e206));
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

    let _e118 = productDraw.viewOriginDrawSpace[3u];
    if (_e118 >= 0.5f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e122 = productDraw.atmosphereFroxelGrid[2u];
    if (_e122 > 0u) {
        let _e125 = productDraw.atmosphereFroxelGrid;
        grid = _e125.xyz;
        let _e128 = gl_FragCoord_1[0u];
        let _e132 = grid[0u];
        x = min((u32(_e128) / 16u), (_e132 - 1u));
        let _e136 = gl_FragCoord_1[1u];
        let _e140 = grid[1u];
        y = min((u32(_e136) / 16u), (_e140 - 1u));
        let _e145 = productDraw.atmosphereColorVisibility[3u];
        farDistance = max(_e145, 4.01f);
        let _e147 = worldPosition_1;
        let _e149 = productDraw.atmosphereEyeDensity;
        let _e153 = productDraw.viewForward;
        viewDistance = dot((_e147 - _e149.xyz), _e153.xyz);
        let _e156 = viewDistance;
        let _e160 = farDistance;
        slice = clamp((log((max(_e156, 4f) / 4f)) / log((_e160 / 4f))), 0f, 0.9999f);
        let _e165 = slice;
        let _e167 = grid[2u];
        let _e172 = grid[2u];
        z = min(u32((_e165 * f32(_e167))), (_e172 - 1u));
        let _e177 = productDraw.atmosphereFroxelGrid[3u];
        let _e178 = z;
        let _e180 = grid[1u];
        let _e182 = y;
        let _e185 = grid[0u];
        let _e188 = x;
        let _e192 = atmosphereFroxels.values[((_e177 + (((_e178 * _e180) + _e182) * _e185)) + _e188)];
        return _e192;
    }
    let _e195 = productDraw.atmosphereEyeDensity[3u];
    density = _e195;
    let _e198 = productDraw.atmosphereColorVisibility[3u];
    if (_e198 > 0f) {
        let _e200 = density;
        let _e203 = productDraw.atmosphereColorVisibility[3u];
        density = max(_e200, (3.912023f / _e203));
    }
    let _e206 = density;
    if (_e206 <= 0f) {
        return vec4<f32>(0f, 0f, 0f, 1f);
    }
    let _e208 = worldPosition_1;
    let _e210 = productDraw.atmosphereEyeDensity;
    distanceToEye = length((_e208 - _e210.xyz));
    let _e216 = productDraw.atmosphereHeightSky[1u];
    let _e219 = worldPosition_1[2u];
    let _e222 = productDraw.atmosphereHeightSky[0u];
    heightWeight = exp((-(_e216) * max((_e219 - _e222), 0f)));
    let _e227 = density;
    let _e229 = heightWeight;
    let _e231 = distanceToEye;
    transmittance = clamp(exp(((-(_e227) * _e229) * _e231)), 0f, 1f);
    let _e235 = worldPosition_1;
    let _e237 = productDraw.atmosphereEyeDensity;
    let _e242 = productDraw.atmosphereColorVisibility;
    param_6 = _e242.xyz;
    param_7 = normalize((_e235 - _e237.xyz));
    let _e244 = wired_aerial_inscatter_u0028_vf3_u003b_vf3_u003b((&param_6), (&param_7));
    inscatter = _e244;
    let _e245 = inscatter;
    let _e246 = transmittance;
    let _e248 = (_e245 * (1f - _e246));
    let _e249 = transmittance;
    return vec4<f32>(_e248.x, _e248.y, _e248.z, _e249);
}

fn main_1() {
    var base: vec4<f32>;
    var signedDistance: f32;
    var screenWidth: f32;
    var linearColor: vec3<f32>;
    var param_8: vec4<f32>;
    var direction_1: vec3<f32>;
    var current: vec3<f32>;
    var param_9: vec3<f32>;
    var param_10: f32;
    var previous: vec3<f32>;
    var param_11: vec3<f32>;
    var param_12: f32;
    var direction_2: vec3<f32>;
    var physicalUv: vec2<f32>;
    var physical_1: vec3<f32>;
    var param_13: vec3<f32>;
    var param_14: vec3<f32>;
    var param_15: f32;
    var param_16: vec3<f32>;
    var param_17: vec4<f32>;
    var param_18: vec3<f32>;
    var param_19: vec4<f32>;
    var alphaMode: i32;
    var localSh: vec3<f32>;
    var address: vec3<f32>;
    var radiance_1: vec3<f32>;
    var encodedDirection_1: vec2<f32>;
    var lighting: vec3<f32>;
    var param_20: vec3<f32>;
    var param_21: vec2<f32>;
    var param_22: vec3<f32>;
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
    var param_23: vec3<f32>;
    var param_24: f32;
    var previousSpecular: vec3<f32>;
    var param_25: vec3<f32>;
    var param_26: f32;
    var f0_: vec3<f32>;
    var nDotV: f32;
    var fresnel: vec3<f32>;
    var specularIbl: vec3<f32>;
    var media: vec4<f32>;
    var local_2: vec3<f32>;
    var param_27: vec3<f32>;
    var param_28: vec4<f32>;
    var phi_855_: bool;
    var phi_896_: bool;
    var phi_956_: bool;
    var phi_989_: bool;
    var phi_1097_: bool;

    let _e158 = texCoord_1;
    let _e159 = textureSample(baseTexture, baseSampler, _e158);
    base = _e159;
    let _e162 = productMaterial.beamFlags[1u];
    if (_e162 != 0u) {
        let _e165 = base[0u];
        let _e167 = base[1u];
        let _e170 = base[0u];
        let _e172 = base[1u];
        let _e175 = base[2u];
        signedDistance = (max(min(_e165, _e167), min(max(_e170, _e172), _e175)) - 0.5f);
        let _e179 = signedDistance;
        let _e180 = fwidth(_e179);
        screenWidth = max(_e180, 0.0009765625f);
        let _e182 = signedDistance;
        let _e183 = screenWidth;
        base = vec4<f32>(1f, 1f, 1f, clamp(((_e182 / _e183) + 0.5f), 0f, 1f));
    }
    let _e190 = productMaterial.beamFlags[0u];
    if (_e190 != 0u) {
        let _e192 = color_1;
        let _e199 = color_1;
        let _e203 = color_1;
        linearColor = select(pow(((_e192.xyz + vec3(0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f)), (_e199.xyz / vec3(12.92f)), (_e203.xyz <= vec3<f32>(0.04045f, 0.04045f, 0.04045f)));
        let _e207 = base;
        let _e209 = linearColor;
        let _e211 = wired_atmosphere_sample_u0028_();
        let _e213 = ((_e207.xyz * _e209) * _e211.w);
        let _e215 = base[3u];
        let _e217 = color_1[3u];
        outColor = vec4<f32>(_e213.x, _e213.y, _e213.z, (_e215 * _e217));
        let _e223 = outColor;
        param_8 = _e223;
        wiredWriteTrust_u0028_vf4_u003b((&param_8));
        return;
    }
    let _e226 = productDraw.atmosphereHeightSky[3u];
    let _e227 = (_e226 > 0.5f);
    phi_855_ = _e227;
    if _e227 {
        let _e230 = productDraw.emissiveRadiance[3u];
        phi_855_ = (_e230 >= 0f);
    }
    let _e233 = phi_855_;
    if _e233 {
        let _e234 = worldPosition_1;
        let _e236 = productDraw.atmosphereEyeDensity;
        direction_1 = normalize((_e234 - _e236.xyz));
        let _e240 = direction_1;
        param_9 = _e240;
        param_10 = 0f;
        let _e241 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_9), (&param_10));
        current = _e241;
        let _e242 = direction_1;
        param_11 = _e242;
        param_12 = 0f;
        let _e243 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_11), (&param_12));
        previous = _e243;
        let _e244 = previous;
        let _e245 = current;
        let _e248 = productDraw.emissiveRadiance[3u];
        let _e251 = mix(_e244, _e245, vec3(clamp(_e248, 0f, 1f)));
        base = vec4<f32>(_e251.x, _e251.y, _e251.z, 1f);
    }
    let _e258 = productDraw.atmosphereHeightSky[3u];
    let _e259 = (_e258 > 0.5f);
    phi_896_ = _e259;
    if _e259 {
        let _e262 = productDraw.atmosphereHeightSky[2u];
        phi_896_ = (_e262 > 0f);
    }
    let _e265 = phi_896_;
    if _e265 {
        let _e266 = worldPosition_1;
        let _e268 = productDraw.atmosphereEyeDensity;
        direction_2 = normalize((_e266 - _e268.xyz));
        let _e273 = direction_2[1u];
        let _e275 = direction_2[0u];
        let _e280 = direction_2[2u];
        physicalUv = vec2<f32>(((atan2(_e273, _e275) / 6.2831855f) + 0.5f), ((asin(clamp(_e280, -1f, 1f)) / 3.1415927f) + 0.5f));
        let _e286 = physicalUv;
        let _e287 = textureSample(physicalSkyTexture, physicalSkySampler, _e286);
        physical_1 = _e287.xyz;
        let _e291 = productDraw.atmosphereHeightSky[2u];
        let _e293 = base;
        param_13 = _e293.xyz;
        let _e295 = physical_1;
        param_14 = _e295;
        param_15 = clamp(_e291, 0f, 1f);
        let _e296 = wired_compose_authored_physical_sky_u0028_vf3_u003b_vf3_u003b_f1_u003b((&param_13), (&param_14), (&param_15));
        param_16 = _e296;
        let _e297 = wired_display_visibility_u0028_vf3_u003b((&param_16));
        outColor = vec4<f32>(_e297.x, _e297.y, _e297.z, 1f);
        let _e302 = outColor;
        param_17 = _e302;
        wiredWriteTrust_u0028_vf4_u003b((&param_17));
        return;
    }
    let _e305 = productDraw.atmosphereHeightSky[3u];
    let _e306 = (_e305 > 0.5f);
    phi_956_ = _e306;
    if _e306 {
        let _e309 = productDraw.emissiveRadiance[3u];
        phi_956_ = (_e309 >= 0f);
    }
    let _e312 = phi_956_;
    if _e312 {
        let _e313 = base;
        param_18 = _e313.xyz;
        let _e315 = wired_display_visibility_u0028_vf3_u003b((&param_18));
        outColor = vec4<f32>(_e315.x, _e315.y, _e315.z, 1f);
        let _e320 = outColor;
        param_19 = _e320;
        wiredWriteTrust_u0028_vf4_u003b((&param_19));
        return;
    }
    let _e323 = productDraw.viewportAlpha[2u];
    alphaMode = i32((_e323 + 0.5f));
    let _e327 = productMaterial.baseColor;
    let _e328 = base;
    base = (_e328 * _e327);
    let _e330 = alphaMode;
    let _e331 = (_e330 == 1i);
    phi_989_ = _e331;
    if _e331 {
        let _e333 = base[3u];
        let _e336 = productDraw.viewportAlpha[3u];
        phi_989_ = (_e333 < _e336);
    }
    let _e339 = phi_989_;
    if _e339 {
        discard;
    }
    let _e342 = productDraw.localSh[0i];
    let _e346 = productDraw.localSh[1i];
    let _e349 = worldNormal_1[0u];
    let _e354 = productDraw.localSh[2i];
    let _e357 = worldNormal_1[1u];
    let _e362 = productDraw.localSh[3i];
    let _e365 = worldNormal_1[2u];
    localSh = max((((_e342.xyz + (_e346.xyz * _e349)) + (_e354.xyz * _e357)) + (_e362.xyz * _e365)), vec3<f32>(0f, 0f, 0f));
    let _e371 = productDraw.staticLighting[0u];
    if (_e371 == 2u) {
        let _e373 = lightmapCoord_1;
        let _e376 = productDraw.staticLighting[1u];
        address = vec3<f32>(_e373.x, _e373.y, f32(_e376));
        let _e381 = address;
        let _e387 = textureSample(staticRadianceTexture, staticRadianceSampler, vec2<f32>(_e381.x, _e381.y), i32(_e381.z));
        radiance_1 = _e387.xyz;
        let _e389 = address;
        let _e395 = textureSample(staticDirectionTexture, staticDirectionSampler, vec2<f32>(_e389.x, _e389.y), i32(_e389.z));
        encodedDirection_1 = _e395.xy;
        let _e399 = productDraw.staticLighting[2u];
        if (_e399 == 2u) {
            let _e401 = encodedDirection_1;
            encodedDirection_1 = ((_e401 * 0.5f) + vec2(0.5f));
        }
        let _e405 = radiance_1;
        param_20 = _e405;
        let _e406 = encodedDirection_1;
        param_21 = _e406;
        let _e407 = worldNormal_1;
        param_22 = _e407;
        let _e408 = wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b((&param_20), (&param_21), (&param_22));
        lighting = _e408;
        let _e411 = productDraw.staticLighting[3u];
        if (_e411 != 0u) {
            let _e413 = address;
            let _e419 = textureSample(staticVisibilityTexture, staticVisibilitySampler, vec2<f32>(_e413.x, _e413.y), i32(_e413.z));
            let _e421 = lighting;
            lighting = (_e421 * _e419.x);
        }
    } else {
        let _e425 = productDraw.staticLighting[0u];
        let _e426 = (_e425 == 1u);
        phi_1097_ = _e426;
        if !(_e426) {
            let _e430 = productDraw.projectionLightmap[3u];
            phi_1097_ = (_e430 >= 0.5f);
        }
        let _e433 = phi_1097_;
        if _e433 {
            let _e434 = lightmapCoord_1;
            let _e435 = textureSample(lightmapTexture, lightmapSampler, _e434);
            let _e439 = productDraw.projectionLightmap[3u];
            boosted = (_e435.xyz * max(_e439, 1f));
            let _e443 = boosted[0u];
            let _e445 = boosted[1u];
            let _e447 = boosted[2u];
            peak = max(_e443, max(_e445, _e447));
            let _e450 = peak;
            if (_e450 > 1f) {
                let _e452 = boosted;
                let _e453 = peak;
                local_1 = (_e452 / vec3(_e453));
            } else {
                let _e456 = boosted;
                local_1 = _e456;
            }
            let _e457 = local_1;
            lighting = _e457;
        } else {
            let _e461 = productDraw.localSh[0i][3u];
            let _e463 = localSh;
            lighting = select(vec3<f32>(1f, 1f, 1f), _e463, vec3((_e461 >= 0.5f)));
        }
    }
    let _e466 = base;
    let _e468 = lighting;
    let _e470 = color_1;
    let _e473 = base;
    let _e476 = productDraw.emissiveRadiance;
    lit = (((_e466.xyz * _e468) * _e470.xyz) + (_e473.xyz * _e476.xyz));
    let _e482 = productMaterial.factors[0u];
    if (_e482 >= 0f) {
        let _e486 = productMaterial.factors[0u];
        metallic = clamp(_e486, 0f, 1f);
        let _e490 = productMaterial.factors[1u];
        roughness = clamp(_e490, 0f, 1f);
        let _e494 = productMaterial.factors[2u];
        occlusion = clamp(_e494, 0f, 1f);
        let _e498 = productMaterial.factors[3u];
        environmentBlend = clamp(_e498, 0f, 1f);
        let _e500 = worldNormal_1;
        normal = normalize(_e500);
        let _e503 = productDraw.atmosphereEyeDensity;
        let _e505 = worldPosition_1;
        viewDirection = normalize((_e503.xyz - _e505));
        let _e508 = viewDirection;
        let _e510 = normal;
        reflected = reflect(-(_e508), _e510);
        let _e512 = roughness;
        let _e513 = textureNumLevels(currentSkyRadianceTexture);
        let _e518 = reflected;
        param_23 = _e518;
        param_24 = (_e512 * f32((i32(_e513) - 1i)));
        let _e519 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(currentSkyRadianceTexture, currentSkyRadianceSampler, (&param_23), (&param_24));
        currentSpecular = _e519;
        let _e520 = roughness;
        let _e521 = textureNumLevels(previousSkyRadianceTexture);
        let _e526 = reflected;
        param_25 = _e526;
        param_26 = (_e520 * f32((i32(_e521) - 1i)));
        let _e527 = wired_sample_sky_radiance_u0028_tA21_u003b_p1_u003b_vf3_u003b_f1_u003b(previousSkyRadianceTexture, previousSkyRadianceSampler, (&param_25), (&param_26));
        previousSpecular = _e527;
        let _e528 = base;
        let _e530 = metallic;
        f0_ = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e528.xyz, vec3(_e530));
        let _e533 = normal;
        let _e534 = viewDirection;
        nDotV = max(dot(_e533, _e534), 0f);
        let _e537 = f0_;
        let _e538 = f0_;
        let _e541 = nDotV;
        fresnel = (_e537 + ((vec3(1f) - _e538) * pow((1f - _e541), 5f)));
        let _e546 = previousSpecular;
        let _e547 = currentSpecular;
        let _e548 = environmentBlend;
        let _e551 = fresnel;
        specularIbl = (mix(_e546, _e547, vec3(_e548)) * _e551);
        let _e553 = specularIbl;
        let _e554 = occlusion;
        let _e556 = lit;
        lit = (_e556 + (_e553 * _e554));
    }
    let _e558 = wired_atmosphere_sample_u0028_();
    media = _e558;
    let _e559 = lit;
    let _e561 = media[3u];
    let _e563 = alphaMode;
    let _e565 = alphaMode;
    if ((_e563 == 3i) || (_e565 == 4i)) {
        local_2 = vec3<f32>(0f, 0f, 0f);
    } else {
        let _e568 = media;
        local_2 = _e568.xyz;
    }
    let _e570 = local_2;
    lit = ((_e559 * _e561) + _e570);
    let _e574 = productDraw.viewOriginDrawSpace[3u];
    if (_e574 < 0.5f) {
        let _e576 = lit;
        param_27 = _e576;
        let _e577 = wired_display_visibility_u0028_vf3_u003b((&param_27));
        lit = _e577;
    }
    let _e578 = lit;
    let _e580 = base[3u];
    let _e582 = color_1[3u];
    outColor = vec4<f32>(_e578.x, _e578.y, _e578.z, (_e580 * _e582));
    let _e588 = outColor;
    param_28 = _e588;
    wiredWriteTrust_u0028_vf4_u003b((&param_28));
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
