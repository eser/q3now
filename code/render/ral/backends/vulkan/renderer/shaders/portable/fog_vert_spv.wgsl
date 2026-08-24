struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_mvp: array<vec4<f32>, 22>,
    mvp: mat4x4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(4) member: vec2<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
@group(0) @binding(0)
var<uniform> unnamed_1: UBO;
var<private> in_position_1: vec3<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var s: f32;
    var t: f32;

    let _e21 = unnamed_1.mvp;
    let _e22 = in_position_1;
    unnamed.gl_Position = (_e21 * vec4<f32>(_e22.x, _e22.y, _e22.z, 1f));
    let _e29 = in_position_1;
    let _e31 = unnamed_1.fogDistanceVector;
    let _e36 = unnamed_1.fogDistanceVector[3u];
    s = (dot(_e29, _e31.xyz) + _e36);
    let _e38 = in_position_1;
    let _e40 = unnamed_1.fogDepthVector;
    let _e45 = unnamed_1.fogDepthVector[3u];
    t = (dot(_e38, _e40.xyz) + _e45);
    let _e49 = unnamed_1.fogEyeT[1u];
    if (_e49 == 1f) {
        let _e51 = t;
        if (_e51 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e53 = t;
        if (_e53 < 1f) {
            t = 0.03125f;
        } else {
            let _e55 = t;
            let _e57 = t;
            let _e60 = unnamed_1.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e55) / (_e57 - _e60)));
        }
    }
    let _e64 = s;
    let _e65 = t;
    fog_tex_coord = vec2<f32>(_e64, _e65);
    return;
}

@vertex
fn main(@location(0) in_position: vec3<f32>) -> VertexOutput {
    in_position_1 = in_position;
    main_1();
    let _e6 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e6);
    let _e8 = unnamed.gl_Position;
    let _e9 = fog_tex_coord;
    return VertexOutput(_e8, _e9);
}
