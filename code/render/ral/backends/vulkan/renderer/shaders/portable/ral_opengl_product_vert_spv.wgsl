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

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
    gl_PointSize: f32,
    gl_ClipDistance: array<f32, 1>,
    gl_CullDistance: array<f32, 1>,
}

struct ProductMaterialFactors {
    baseColor: vec4<f32>,
    factors: vec4<f32>,
    beamStartColor: vec4<f32>,
    beamEndColor: vec4<f32>,
    beamScaleOffset: vec4<f32>,
    beamFlags: vec4<u32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec2<f32>,
    @location(2) member_2: vec4<f32>,
    @location(3) member_3: vec3<f32>,
    @location(4) member_4: vec3<f32>,
}

@group(0) @binding(0)
var<uniform> productDraw: ProductDraw;
var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f), 1f, array<f32, 1>(), array<f32, 1>());
var<private> in_position_1: vec3<f32>;
var<private> texCoord: vec2<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> lightmapCoord: vec2<f32>;
var<private> in_lightmap_coord_1: vec2<f32>;
var<private> color: vec4<f32>;
var<private> in_color_1: vec4<f32>;
@group(2) @binding(1)
var<uniform> productMaterial: ProductMaterialFactors;
var<private> worldPosition: vec3<f32>;
var<private> worldNormal: vec3<f32>;
var<private> in_normal_1: vec3<f32>;

fn main_1() {
    var delta: vec3<f32>;
    var forward: f32;
    var phi_128_: bool;

    let _e33 = productDraw.viewOriginDrawSpace[3u];
    if (_e33 >= 0.5f) {
        let _e36 = in_position_1[0u];
        let _e39 = productDraw.viewportAlpha[0u];
        let _e44 = in_position_1[1u];
        let _e47 = productDraw.viewportAlpha[1u];
        unnamed.gl_Position = vec4<f32>((((_e36 / _e39) * 2f) - 1f), (1f - ((_e44 / _e47) * 2f)), 0f, 1f);
    } else {
        let _e53 = in_position_1;
        let _e55 = productDraw.viewOriginDrawSpace;
        delta = (_e53 - _e55.xyz);
        let _e58 = delta;
        let _e60 = productDraw.viewForward;
        forward = dot(_e58, _e60.xyz);
        let _e63 = delta;
        let _e65 = productDraw.viewLeft;
        let _e71 = productDraw.projectionLightmap[0u];
        let _e73 = delta;
        let _e75 = productDraw.viewUp;
        let _e80 = productDraw.projectionLightmap[1u];
        let _e82 = forward;
        let _e85 = productDraw.projectionLightmap[2u];
        let _e87 = forward;
        unnamed.gl_Position = vec4<f32>((-(dot(_e63, _e65.xyz)) * _e71), (dot(_e73, _e75.xyz) * _e80), (_e82 - _e85), _e87);
    }
    let _e90 = in_tex_coord_1;
    texCoord = _e90;
    let _e91 = in_lightmap_coord_1;
    lightmapCoord = _e91;
    let _e92 = in_color_1;
    color = _e92;
    let _e95 = productDraw.atmosphereHeightSky[3u];
    let _e96 = (_e95 < 0.5f);
    phi_128_ = _e96;
    if _e96 {
        let _e99 = productMaterial.beamFlags[0u];
        phi_128_ = (_e99 != 0u);
    }
    let _e102 = phi_128_;
    if _e102 {
        let _e104 = productMaterial.beamStartColor;
        let _e106 = productMaterial.beamEndColor;
        let _e108 = in_tex_coord_1[0u];
        color = mix(_e104, _e106, vec4(_e108));
        let _e111 = in_tex_coord_1;
        let _e113 = productMaterial.beamScaleOffset;
        let _e117 = productMaterial.beamScaleOffset;
        texCoord = ((_e111 * _e113.xy) + _e117.zw);
    }
    let _e120 = in_position_1;
    worldPosition = _e120;
    let _e121 = in_normal_1;
    worldNormal = normalize(_e121);
    return;
}

@vertex
fn main(@location(0) in_position: vec3<f32>, @location(1) in_tex_coord: vec2<f32>, @location(2) in_lightmap_coord: vec2<f32>, @location(3) in_color: vec4<f32>, @location(4) in_normal: vec3<f32>) -> VertexOutput {
    in_position_1 = in_position;
    in_tex_coord_1 = in_tex_coord;
    in_lightmap_coord_1 = in_lightmap_coord;
    in_color_1 = in_color;
    in_normal_1 = in_normal;
    main_1();
    let _e18 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e18);
    let _e20 = unnamed.gl_Position;
    let _e21 = texCoord;
    let _e22 = lightmapCoord;
    let _e23 = color;
    let _e24 = worldPosition;
    let _e25 = worldNormal;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24, _e25);
}
