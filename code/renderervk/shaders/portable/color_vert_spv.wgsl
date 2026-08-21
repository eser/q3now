struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct UBO {
    _pad_to_mvp: array<vec4<f32>, 30>,
    mvp: mat4x4<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
@group(0) @binding(0) 
var<uniform> ubo: UBO;
var<private> in_position_1: vec3<f32>;

fn main_1() {
    let _e8 = ubo.mvp;
    let _e9 = in_position_1;
    unnamed.gl_Position = (_e8 * vec4<f32>(_e9.x, _e9.y, _e9.z, 1f));
    return;
}

@vertex 
fn main(@location(0) in_position: vec3<f32>) -> @builtin(position) vec4<f32> {
    in_position_1 = in_position;
    main_1();
    let _e5 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e5);
    let _e7 = unnamed.gl_Position;
    return _e7;
}
