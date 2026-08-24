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
    @location(2) member_4: vec2<f32>,
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
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;

    if override_type_5_ {
        let _e26 = gl_InstanceIndex_1;
        let _e30 = unnamed.entMat[(_e26 * 2i)];
        local = _e30;
    } else {
        let _e32 = ubo.mvp;
        local = _e32;
    }
    let _e33 = local;
    mvp = _e33;
    let _e34 = mvp;
    let _e35 = in_position_1;
    unnamed_1.gl_Position = (_e34 * vec4<f32>(_e35.x, _e35.y, _e35.z, 1f));
    let _e42 = gl_InstanceIndex_1;
    let _e45 = temporalPayloads.temporalPayload[_e42];
    temporal.currentMvp = _e45.currentMvp;
    temporal.previousMvp = _e45.previousMvp;
    temporal.outcomeReserved = _e45.outcomeReserved;
    let _e52 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e52.x, _e52.y, _e52.z, 1f);
    let _e58 = temporal.currentMvp;
    let _e59 = temporalLocalPosition;
    temporalCurrentClip = (_e58 * _e59);
    let _e62 = temporal.previousMvp;
    let _e63 = temporalLocalPosition;
    temporalPreviousClip = (_e62 * _e63);
    let _e67 = temporal.outcomeReserved[0u];
    temporalOutcome = _e67;
    let _e68 = in_tex_coord0_1;
    frag_tex_coord0_ = _e68;
    let _e69 = in_tex_coord1_1;
    frag_tex_coord1_ = _e69;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e17 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e17);
    let _e19 = unnamed_1.gl_Position;
    let _e20 = temporalCurrentClip;
    let _e21 = temporalPreviousClip;
    let _e22 = temporalOutcome;
    let _e23 = frag_tex_coord0_;
    let _e24 = frag_tex_coord1_;
    return VertexOutput(_e19, _e20, _e21, _e22, _e23, _e24);
}
