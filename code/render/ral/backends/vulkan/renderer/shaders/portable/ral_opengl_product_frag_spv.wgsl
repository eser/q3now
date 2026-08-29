struct ProductDraw {
    viewOriginDrawSpace: vec4<f32>,
    viewForward: vec4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    projectionLightmap: vec4<f32>,
    viewportAlpha: vec4<f32>,
    atmosphereEyeDensity: vec4<f32>,
    atmosphereColorVisibility: vec4<f32>,
    atmosphereHeightCloud: vec4<f32>,
    atmosphereFroxelGrid: vec4<u32>,
    localSh: array<vec4<f32>, 4>,
    staticLighting: vec4<u32>,
    emissiveRadiance: vec4<f32>,
}

struct AtmosphereFroxels {
    values: array<vec4<f32>>,
}

@group(0) @binding(0)
var<uniform> productDraw: ProductDraw;
@group(1) @binding(0)
var baseTexture: texture_2d<f32>;
@group(1) @binding(1)
var baseSampler: sampler;
var<private> texCoord_1: vec2<f32>;
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
var<private> color_1: vec4<f32>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> worldPosition_1: vec3<f32>;
@group(2) @binding(0)
var<storage> atmosphereFroxels: AtmosphereFroxels;
var<private> outColor: vec4<f32>;

fn wired_display_visibility_u0028_vf3_u003b(sceneColor: ptr<function, vec3<f32>>) -> vec3<f32> {
    var exposed: vec3<f32>;
    var luminance: f32;
    var curved: f32;
    var phi_124_: bool;
    var phi_131_: bool;

    let _e56 = (*sceneColor);
    let _e60 = productDraw.viewForward[3u];
    exposed = (max(_e56, vec3<f32>(0f, 0f, 0f)) * _e60);
    let _e62 = exposed;
    luminance = dot(_e62, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
    let _e64 = luminance;
    let _e65 = (_e64 > 0f);
    phi_124_ = _e65;
    if _e65 {
        let _e66 = luminance;
        let _e69 = productDraw.viewUp[3u];
        phi_124_ = (_e66 < _e69);
    }
    let _e72 = phi_124_;
    phi_131_ = _e72;
    if _e72 {
        let _e75 = productDraw.viewLeft[3u];
        phi_131_ = (_e75 != 1f);
    }
    let _e78 = phi_131_;
    if _e78 {
        let _e79 = luminance;
        let _e82 = productDraw.viewUp[3u];
        let _e86 = productDraw.viewLeft[3u];
        let _e90 = productDraw.viewUp[3u];
        curved = (pow((_e79 / _e82), _e86) * _e90);
        let _e92 = curved;
        let _e93 = luminance;
        let _e95 = exposed;
        exposed = (_e95 * (_e92 / _e93));
    }
    let _e97 = exposed;
    return _e97;
}

fn wired_lighting_decode_oct_u0028_vf2_u003b(encoded: ptr<function, vec2<f32>>) -> vec3<f32> {
    var f: vec2<f32>;
    var n: vec3<f32>;
    var folded: vec2<f32>;

    let _e56 = (*encoded);
    f = ((_e56 * 2f) - vec2(1f));
    let _e60 = f;
    let _e62 = f[0u];
    let _e66 = f[1u];
    n = vec3<f32>(_e60.x, _e60.y, ((1f - abs(_e62)) - abs(_e66)));
    let _e73 = n[2u];
    if (_e73 < 0f) {
        let _e75 = n;
        let _e80 = n;
        folded = ((vec2(1f) - abs(_e75.yx)) * sign(_e80.xy));
        let _e84 = folded;
        n[0u] = _e84.x;
        n[1u] = _e84.y;
    }
    let _e89 = n;
    return normalize(_e89);
}

fn wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b(radiance: ptr<function, vec3<f32>>, encodedDirection: ptr<function, vec2<f32>>, materialNormal: ptr<function, vec3<f32>>) -> vec3<f32> {
    var dominantDirection: vec3<f32>;
    var param: vec2<f32>;

    let _e57 = (*encodedDirection);
    param = _e57;
    let _e58 = wired_lighting_decode_oct_u0028_vf2_u003b((&param));
    dominantDirection = _e58;
    let _e59 = (*radiance);
    let _e61 = (*materialNormal);
    let _e63 = dominantDirection;
    return (max(_e59, vec3<f32>(0f, 0f, 0f)) * max(dot(normalize(_e61), _e63), 0f));
}

fn main_1() {
    var base: vec4<f32>;
    var alphaMode: i32;
    var localSh: vec3<f32>;
    var address: vec3<f32>;
    var radiance_1: vec3<f32>;
    var encodedDirection_1: vec2<f32>;
    var lighting: vec3<f32>;
    var param_1: vec3<f32>;
    var param_2: vec2<f32>;
    var param_3: vec3<f32>;
    var lit: vec3<f32>;
    var grid: vec3<u32>;
    var x: u32;
    var y: u32;
    var farDistance: f32;
    var viewDistance: f32;
    var slice: f32;
    var z: u32;
    var media: vec4<f32>;
    var param_4: vec3<f32>;
    var density: f32;
    var distanceToEye: f32;
    var heightWeight: f32;
    var transmittance: f32;
    var param_5: vec3<f32>;
    var phi_186_: bool;
    var phi_302_: bool;
    var phi_351_: bool;

    let _e77 = texCoord_1;
    let _e78 = textureSample(baseTexture, baseSampler, _e77);
    base = _e78;
    let _e81 = productDraw.viewportAlpha[2u];
    alphaMode = i32((_e81 + 0.5f));
    let _e84 = alphaMode;
    let _e85 = (_e84 == 1i);
    phi_186_ = _e85;
    if _e85 {
        let _e87 = base[3u];
        let _e90 = productDraw.viewportAlpha[3u];
        phi_186_ = (_e87 < _e90);
    }
    let _e93 = phi_186_;
    if _e93 {
        discard;
    }
    let _e96 = productDraw.localSh[0i];
    let _e100 = productDraw.localSh[1i];
    let _e103 = worldNormal_1[0u];
    let _e108 = productDraw.localSh[2i];
    let _e111 = worldNormal_1[1u];
    let _e116 = productDraw.localSh[3i];
    let _e119 = worldNormal_1[2u];
    localSh = max((((_e96.xyz + (_e100.xyz * _e103)) + (_e108.xyz * _e111)) + (_e116.xyz * _e119)), vec3<f32>(0f, 0f, 0f));
    let _e125 = productDraw.staticLighting[0u];
    if (_e125 == 2u) {
        let _e127 = lightmapCoord_1;
        let _e130 = productDraw.staticLighting[1u];
        address = vec3<f32>(_e127.x, _e127.y, f32(_e130));
        let _e135 = address;
        let _e141 = textureSample(staticRadianceTexture, staticRadianceSampler, vec2<f32>(_e135.x, _e135.y), i32(_e135.z));
        radiance_1 = _e141.xyz;
        let _e143 = address;
        let _e149 = textureSample(staticDirectionTexture, staticDirectionSampler, vec2<f32>(_e143.x, _e143.y), i32(_e143.z));
        encodedDirection_1 = _e149.xy;
        let _e153 = productDraw.staticLighting[2u];
        if (_e153 == 2u) {
            let _e155 = encodedDirection_1;
            encodedDirection_1 = ((_e155 * 0.5f) + vec2(0.5f));
        }
        let _e159 = radiance_1;
        param_1 = _e159;
        let _e160 = encodedDirection_1;
        param_2 = _e160;
        let _e161 = worldNormal_1;
        param_3 = _e161;
        let _e162 = wired_lighting_directional_static_indirect_u0028_vf3_u003b_vf2_u003b_vf3_u003b((&param_1), (&param_2), (&param_3));
        lighting = _e162;
        let _e165 = productDraw.staticLighting[3u];
        if (_e165 != 0u) {
            let _e167 = address;
            let _e173 = textureSample(staticVisibilityTexture, staticVisibilitySampler, vec2<f32>(_e167.x, _e167.y), i32(_e167.z));
            let _e175 = lighting;
            lighting = (_e175 * _e173.x);
        }
    } else {
        let _e179 = productDraw.staticLighting[0u];
        let _e180 = (_e179 == 1u);
        phi_302_ = _e180;
        if !(_e180) {
            let _e184 = productDraw.projectionLightmap[3u];
            phi_302_ = (_e184 >= 0.5f);
        }
        let _e187 = phi_302_;
        if _e187 {
            let _e188 = lightmapCoord_1;
            let _e189 = textureSample(lightmapTexture, lightmapSampler, _e188);
            lighting = min((_e189.xyz * 2f), vec3<f32>(1f, 1f, 1f));
        } else {
            let _e196 = productDraw.localSh[0i][3u];
            let _e198 = localSh;
            lighting = select(vec3<f32>(1f, 1f, 1f), _e198, vec3((_e196 >= 0.5f)));
        }
    }
    let _e201 = base;
    let _e203 = lighting;
    let _e205 = color_1;
    let _e208 = base;
    let _e211 = productDraw.emissiveRadiance;
    lit = (((_e201.xyz * _e203) * _e205.xyz) + (_e208.xyz * _e211.xyz));
    let _e217 = productDraw.viewOriginDrawSpace[3u];
    let _e218 = (_e217 < 0.5f);
    phi_351_ = _e218;
    if _e218 {
        let _e221 = productDraw.atmosphereFroxelGrid[2u];
        phi_351_ = (_e221 > 0u);
    }
    let _e224 = phi_351_;
    if _e224 {
        let _e226 = productDraw.atmosphereFroxelGrid;
        grid = _e226.xyz;
        let _e229 = gl_FragCoord_1[0u];
        let _e233 = grid[0u];
        x = min((u32(_e229) / 16u), (_e233 - 1u));
        let _e237 = gl_FragCoord_1[1u];
        let _e241 = grid[1u];
        y = min((u32(_e237) / 16u), (_e241 - 1u));
        let _e246 = productDraw.atmosphereColorVisibility[3u];
        farDistance = max(_e246, 4.01f);
        let _e248 = worldPosition_1;
        let _e250 = productDraw.atmosphereEyeDensity;
        viewDistance = length((_e248 - _e250.xyz));
        let _e254 = viewDistance;
        let _e258 = farDistance;
        slice = clamp((log((max(_e254, 4f) / 4f)) / log((_e258 / 4f))), 0f, 0.9999f);
        let _e263 = slice;
        let _e265 = grid[2u];
        let _e270 = grid[2u];
        z = min(u32((_e263 * f32(_e265))), (_e270 - 1u));
        let _e275 = productDraw.atmosphereFroxelGrid[3u];
        let _e276 = z;
        let _e278 = grid[1u];
        let _e280 = y;
        let _e283 = grid[0u];
        let _e286 = x;
        let _e290 = atmosphereFroxels.values[((_e275 + (((_e276 * _e278) + _e280) * _e283)) + _e286)];
        media = _e290;
        let _e291 = lit;
        let _e293 = media[3u];
        let _e295 = media;
        param_4 = ((_e291 * _e293) + _e295.xyz);
        let _e298 = wired_display_visibility_u0028_vf3_u003b((&param_4));
        let _e300 = base[3u];
        let _e302 = color_1[3u];
        outColor = vec4<f32>(_e298.x, _e298.y, _e298.z, (_e300 * _e302));
        return;
    }
    let _e310 = productDraw.atmosphereEyeDensity[3u];
    density = _e310;
    let _e313 = productDraw.atmosphereColorVisibility[3u];
    if (_e313 > 0f) {
        let _e315 = density;
        let _e318 = productDraw.atmosphereColorVisibility[3u];
        density = max(_e315, (3.912023f / _e318));
    }
    let _e323 = productDraw.viewOriginDrawSpace[3u];
    let _e325 = density;
    if ((_e323 < 0.5f) && (_e325 > 0f)) {
        let _e328 = worldPosition_1;
        let _e330 = productDraw.atmosphereEyeDensity;
        distanceToEye = length((_e328 - _e330.xyz));
        let _e336 = productDraw.atmosphereHeightCloud[1u];
        let _e339 = worldPosition_1[2u];
        let _e342 = productDraw.atmosphereHeightCloud[0u];
        heightWeight = exp((-(_e336) * max((_e339 - _e342), 0f)));
        let _e347 = density;
        let _e349 = heightWeight;
        let _e351 = distanceToEye;
        transmittance = exp(((-(_e347) * _e349) * _e351));
        let _e355 = productDraw.atmosphereColorVisibility;
        let _e357 = lit;
        let _e358 = transmittance;
        lit = mix(_e355.xyz, _e357, vec3(clamp(_e358, 0f, 1f)));
    }
    let _e364 = productDraw.viewOriginDrawSpace[3u];
    if (_e364 < 0.5f) {
        let _e366 = lit;
        param_5 = _e366;
        let _e367 = wired_display_visibility_u0028_vf3_u003b((&param_5));
        lit = _e367;
    }
    let _e368 = lit;
    let _e370 = base[3u];
    let _e372 = color_1[3u];
    outColor = vec4<f32>(_e368.x, _e368.y, _e368.z, (_e370 * _e372));
    return;
}

@fragment
fn main(@location(0) texCoord: vec2<f32>, @location(4) worldNormal: vec3<f32>, @location(1) lightmapCoord: vec2<f32>, @location(2) color: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(3) worldPosition: vec3<f32>) -> @location(0) vec4<f32> {
    texCoord_1 = texCoord;
    worldNormal_1 = worldNormal;
    lightmapCoord_1 = lightmapCoord;
    color_1 = color;
    gl_FragCoord_1 = gl_FragCoord;
    worldPosition_1 = worldPosition;
    main_1();
    let _e13 = outColor;
    return _e13;
}
