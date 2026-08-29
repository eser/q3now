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

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
    gl_PointSize: f32,
    gl_ClipDistance: array<f32, 1>,
    gl_CullDistance: array<f32, 1>,
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
var<private> worldPosition: vec3<f32>;
var<private> worldNormal: vec3<f32>;
var<private> in_normal_1: vec3<f32>;

fn main_1() {
    var delta: vec3<f32>;
    var forward: f32;

    let _e31 = productDraw.viewOriginDrawSpace[3u];
    if (_e31 >= 0.5f) {
        let _e34 = in_position_1[0u];
        let _e37 = productDraw.viewportAlpha[0u];
        let _e42 = in_position_1[1u];
        let _e45 = productDraw.viewportAlpha[1u];
        unnamed.gl_Position = vec4<f32>((((_e34 / _e37) * 2f) - 1f), (1f - ((_e42 / _e45) * 2f)), 0f, 1f);
    } else {
        let _e51 = in_position_1;
        let _e53 = productDraw.viewOriginDrawSpace;
        delta = (_e51 - _e53.xyz);
        let _e56 = delta;
        let _e58 = productDraw.viewForward;
        forward = dot(_e56, _e58.xyz);
        let _e61 = delta;
        let _e63 = productDraw.viewLeft;
        let _e69 = productDraw.projectionLightmap[0u];
        let _e71 = delta;
        let _e73 = productDraw.viewUp;
        let _e78 = productDraw.projectionLightmap[1u];
        let _e80 = forward;
        let _e83 = productDraw.projectionLightmap[2u];
        let _e85 = forward;
        unnamed.gl_Position = vec4<f32>((-(dot(_e61, _e63.xyz)) * _e69), (dot(_e71, _e73.xyz) * _e78), (_e80 - _e83), _e85);
    }
    let _e88 = in_tex_coord_1;
    texCoord = _e88;
    let _e89 = in_lightmap_coord_1;
    lightmapCoord = _e89;
    let _e90 = in_color_1;
    color = _e90;
    let _e91 = in_position_1;
    worldPosition = _e91;
    let _e92 = in_normal_1;
    worldNormal = normalize(_e92);
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
