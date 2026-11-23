struct EntityMatrices {
    entMat: array<mat4x4<f32>>,
}

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    _pad_to_mvp: array<vec4<f32>, 26>,
    mvp: mat4x4<f32>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
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
    @location(10) member: vec4<f32>,
    @location(11) member_1: vec4<f32>,
    @location(12) @interpolate(flat) member_2: u32,
    @location(7) member_3: vec4<f32>,
    @location(0) member_4: vec4<f32>,
    @location(1) member_5: vec2<f32>,
    @location(2) member_6: vec2<f32>,
}

@id(16) override ENTITY_SSBO: i32 = 0i;
override override_type_5_: bool = (ENTITY_SSBO != 0i);

@group(3) @binding(0)
var<storage> unnamed: EntityMatrices;
var<private> gl_InstanceIndex_1: i32;
@group(0) @binding(0)
var<uniform> ubo: UBO;
var<private> unnamed_1: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> in_position_1: vec3<f32>;
@group(3) @binding(1)
var<storage> temporalPayloads: TemporalPayloads;
var<private> temporalCurrentClip: vec4<f32>;
var<private> temporalPreviousClip: vec4<f32>;
var<private> temporalOutcome: u32;
var<private> shadowData: vec4<f32>;
var<private> frag_color0_: vec4<f32>;
var<private> in_color0_1: vec4<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;

    if override_type_5_ {
        let _e30 = gl_InstanceIndex_1;
        let _e34 = unnamed.entMat[(_e30 * 2i)];
        local = _e34;
    } else {
        let _e36 = ubo.mvp;
        local = _e36;
    }
    let _e37 = local;
    mvp = _e37;
    let _e38 = mvp;
    let _e39 = in_position_1;
    unnamed_1.gl_Position = (_e38 * vec4<f32>(_e39.x, _e39.y, _e39.z, 1f));
    let _e46 = gl_InstanceIndex_1;
    let _e49 = temporalPayloads.temporalPayload[_e46];
    temporal.currentMvp = _e49.currentMvp;
    temporal.previousMvp = _e49.previousMvp;
    temporal.outcomeReserved = _e49.outcomeReserved;
    let _e56 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e56.x, _e56.y, _e56.z, 1f);
    let _e62 = temporal.currentMvp;
    let _e63 = temporalLocalPosition;
    temporalCurrentClip = (_e62 * _e63);
    let _e66 = temporal.previousMvp;
    let _e67 = temporalLocalPosition;
    temporalPreviousClip = (_e66 * _e67);
    let _e71 = temporal.outcomeReserved[0u];
    temporalOutcome = _e71;
    let _e72 = in_position_1;
    let _e75 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e72.x, _e72.y, _e72.z, _e75);
    let _e80 = in_color0_1;
    frag_color0_ = _e80;
    let _e81 = in_tex_coord0_1;
    frag_tex_coord0_ = _e81;
    let _e82 = in_tex_coord1_1;
    frag_tex_coord1_ = _e82;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e21 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e21);
    let _e23 = unnamed_1.gl_Position;
    let _e24 = temporalCurrentClip;
    let _e25 = temporalPreviousClip;
    let _e26 = temporalOutcome;
    let _e27 = shadowData;
    let _e28 = frag_color0_;
    let _e29 = frag_tex_coord0_;
    let _e30 = frag_tex_coord1_;
    return VertexOutput(_e23, _e24, _e25, _e26, _e27, _e28, _e29, _e30);
}
