struct ProductDraw {
    viewOriginDrawSpace: vec4<f32>,
    viewForward: vec4<f32>,
    viewLeft: vec4<f32>,
    viewUp: vec4<f32>,
    projectionLightmap: vec4<f32>,
    viewportAlpha: vec4<f32>,
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

fn main_1() {
    var delta: vec3<f32>;
    var forward: f32;

    let _e27 = productDraw.viewOriginDrawSpace[3u];
    if (_e27 >= 0.5f) {
        let _e30 = in_position_1[0u];
        let _e33 = productDraw.viewportAlpha[0u];
        let _e38 = in_position_1[1u];
        let _e41 = productDraw.viewportAlpha[1u];
        unnamed.gl_Position = vec4<f32>((((_e30 / _e33) * 2f) - 1f), (1f - ((_e38 / _e41) * 2f)), 0f, 1f);
    } else {
        let _e47 = in_position_1;
        let _e49 = productDraw.viewOriginDrawSpace;
        delta = (_e47 - _e49.xyz);
        let _e52 = delta;
        let _e54 = productDraw.viewForward;
        forward = dot(_e52, _e54.xyz);
        let _e57 = delta;
        let _e59 = productDraw.viewLeft;
        let _e65 = productDraw.projectionLightmap[0u];
        let _e67 = delta;
        let _e69 = productDraw.viewUp;
        let _e74 = productDraw.projectionLightmap[1u];
        let _e76 = forward;
        let _e79 = productDraw.projectionLightmap[2u];
        let _e81 = forward;
        unnamed.gl_Position = vec4<f32>((-(dot(_e57, _e59.xyz)) * _e65), (dot(_e67, _e69.xyz) * _e74), (_e76 - _e79), _e81);
    }
    let _e84 = in_tex_coord_1;
    texCoord = _e84;
    let _e85 = in_lightmap_coord_1;
    lightmapCoord = _e85;
    let _e86 = in_color_1;
    color = _e86;
    return;
}

@vertex
fn main(@location(0) in_position: vec3<f32>, @location(1) in_tex_coord: vec2<f32>, @location(2) in_lightmap_coord: vec2<f32>, @location(3) in_color: vec4<f32>) -> VertexOutput {
    in_position_1 = in_position;
    in_tex_coord_1 = in_tex_coord;
    in_lightmap_coord_1 = in_lightmap_coord;
    in_color_1 = in_color;
    main_1();
    let _e14 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e14);
    let _e16 = unnamed.gl_Position;
    let _e17 = texCoord;
    let _e18 = lightmapCoord;
    let _e19 = color;
    return VertexOutput(_e16, _e17, _e18, _e19);
}
