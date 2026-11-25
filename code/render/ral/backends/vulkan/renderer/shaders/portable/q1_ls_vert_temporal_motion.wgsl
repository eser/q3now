struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
    gl_PointSize: f32,
    gl_ClipDistance: array<f32, 1>,
    gl_CullDistance: array<f32, 1>,
}

struct UBO {
    _pad_to_mvp: array<vec4<f32>, 30>,
    mvp: mat4x4<f32>,
}

struct TemporalPayloadRecord {
    currentMvp: mat4x4<f32>,
    previousMvp: mat4x4<f32>,
    outcomeReserved: vec4<u32>,
}

struct TemporalPayloads {
    temporalPayload: array<TemporalPayloadRecord>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(1) member: vec2<f32>,
    @location(2) member_1: vec2<f32>,
    @location(10) member_2: vec4<f32>,
    @location(11) member_3: vec4<f32>,
    @location(12) @interpolate(flat) member_4: u32,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f), 1f, array<f32, 1>(), array<f32, 1>());
@group(0) @binding(0)
var<uniform> ubo: UBO;
var<private> in_position_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;
@group(3) @binding(1)
var<storage> temporalPayloads: TemporalPayloads;
var<private> gl_InstanceIndex_1: i32;
var<private> temporalCurrentClip: vec4<f32>;
var<private> temporalPreviousClip: vec4<f32>;
var<private> temporalOutcome: u32;

fn main_1() {
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;

    let _e22 = ubo.mvp;
    let _e23 = in_position_1;
    unnamed.gl_Position = (_e22 * vec4<f32>(_e23.x, _e23.y, _e23.z, 1f));
    let _e30 = in_tex_coord0_1;
    frag_tex_coord0_ = _e30;
    let _e31 = in_tex_coord1_1;
    frag_tex_coord1_ = _e31;
    let _e32 = gl_InstanceIndex_1;
    let _e35 = temporalPayloads.temporalPayload[_e32];
    temporal.currentMvp = _e35.currentMvp;
    temporal.previousMvp = _e35.previousMvp;
    temporal.outcomeReserved = _e35.outcomeReserved;
    let _e42 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e42.x, _e42.y, _e42.z, 1f);
    let _e48 = temporal.currentMvp;
    let _e49 = temporalLocalPosition;
    temporalCurrentClip = (_e48 * _e49);
    let _e52 = temporal.previousMvp;
    let _e53 = temporalLocalPosition;
    temporalPreviousClip = (_e52 * _e53);
    let _e57 = temporal.outcomeReserved[0u];
    temporalOutcome = _e57;
    return;
}

@vertex
fn main(@location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>, @builtin(instance_index) gl_InstanceIndex: u32) -> VertexOutput {
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    main_1();
    let _e17 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e17);
    let _e19 = unnamed.gl_Position;
    let _e20 = frag_tex_coord0_;
    let _e21 = frag_tex_coord1_;
    let _e22 = temporalCurrentClip;
    let _e23 = temporalPreviousClip;
    let _e24 = temporalOutcome;
    return VertexOutput(_e19, _e20, _e21, _e22, _e23, _e24);
}
