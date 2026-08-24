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
    @location(0) member_3: vec4<f32>,
    @location(1) member_4: vec2<f32>,
    @location(2) member_5: vec2<f32>,
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
        let _e28 = gl_InstanceIndex_1;
        let _e32 = unnamed.entMat[(_e28 * 2i)];
        local = _e32;
    } else {
        let _e34 = ubo.mvp;
        local = _e34;
    }
    let _e35 = local;
    mvp = _e35;
    let _e36 = mvp;
    let _e37 = in_position_1;
    unnamed_1.gl_Position = (_e36 * vec4<f32>(_e37.x, _e37.y, _e37.z, 1f));
    let _e44 = gl_InstanceIndex_1;
    let _e47 = temporalPayloads.temporalPayload[_e44];
    temporal.currentMvp = _e47.currentMvp;
    temporal.previousMvp = _e47.previousMvp;
    temporal.outcomeReserved = _e47.outcomeReserved;
    let _e54 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e54.x, _e54.y, _e54.z, 1f);
    let _e60 = temporal.currentMvp;
    let _e61 = temporalLocalPosition;
    temporalCurrentClip = (_e60 * _e61);
    let _e64 = temporal.previousMvp;
    let _e65 = temporalLocalPosition;
    temporalPreviousClip = (_e64 * _e65);
    let _e69 = temporal.outcomeReserved[0u];
    temporalOutcome = _e69;
    let _e70 = in_color0_1;
    frag_color0_ = _e70;
    let _e71 = in_tex_coord0_1;
    frag_tex_coord0_ = _e71;
    let _e72 = in_tex_coord1_1;
    frag_tex_coord1_ = _e72;
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
    let _e20 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e20);
    let _e22 = unnamed_1.gl_Position;
    let _e23 = temporalCurrentClip;
    let _e24 = temporalPreviousClip;
    let _e25 = temporalOutcome;
    let _e26 = frag_color0_;
    let _e27 = frag_tex_coord0_;
    let _e28 = frag_tex_coord1_;
    return VertexOutput(_e22, _e23, _e24, _e25, _e26, _e27, _e28);
}
