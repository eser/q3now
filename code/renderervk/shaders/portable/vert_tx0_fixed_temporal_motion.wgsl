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
    @location(1) member_3: vec2<f32>,
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
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;

    if override_type_5_ {
        let _e24 = gl_InstanceIndex_1;
        let _e28 = unnamed.entMat[(_e24 * 2i)];
        local = _e28;
    } else {
        let _e30 = ubo.mvp;
        local = _e30;
    }
    let _e31 = local;
    mvp = _e31;
    let _e32 = mvp;
    let _e33 = in_position_1;
    unnamed_1.gl_Position = (_e32 * vec4<f32>(_e33.x, _e33.y, _e33.z, 1f));
    let _e40 = gl_InstanceIndex_1;
    let _e43 = temporalPayloads.temporalPayload[_e40];
    temporal.currentMvp = _e43.currentMvp;
    temporal.previousMvp = _e43.previousMvp;
    temporal.outcomeReserved = _e43.outcomeReserved;
    let _e50 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e50.x, _e50.y, _e50.z, 1f);
    let _e56 = temporal.currentMvp;
    let _e57 = temporalLocalPosition;
    temporalCurrentClip = (_e56 * _e57);
    let _e60 = temporal.previousMvp;
    let _e61 = temporalLocalPosition;
    temporalPreviousClip = (_e60 * _e61);
    let _e65 = temporal.outcomeReserved[0u];
    temporalOutcome = _e65;
    let _e66 = in_tex_coord0_1;
    frag_tex_coord0_ = _e66;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e14 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e14);
    let _e16 = unnamed_1.gl_Position;
    let _e17 = temporalCurrentClip;
    let _e18 = temporalPreviousClip;
    let _e19 = temporalOutcome;
    let _e20 = frag_tex_coord0_;
    return VertexOutput(_e16, _e17, _e18, _e19, _e20);
}
