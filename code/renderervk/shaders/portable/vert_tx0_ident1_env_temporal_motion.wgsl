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
var<private> in_normal_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;
    var viewer: vec3<f32>;
    var d: f32;
    var reflected: vec2<f32>;

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
    let _e73 = ubo.eyePos;
    let _e75 = in_position_1;
    viewer = normalize((_e73.xyz - _e75));
    let _e78 = in_normal_1;
    let _e79 = viewer;
    d = dot(_e78, _e79);
    let _e81 = in_normal_1;
    let _e84 = d;
    let _e86 = viewer;
    reflected = (((_e81.yz * 2f) * _e84) - _e86.yz);
    let _e90 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e90 * 0.5f));
    let _e95 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e95 * 0.5f));
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
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
