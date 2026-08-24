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
        let _e32 = gl_InstanceIndex_1;
        let _e36 = unnamed.entMat[(_e32 * 2i)];
        local = _e36;
    } else {
        let _e38 = ubo.mvp;
        local = _e38;
    }
    let _e39 = local;
    mvp = _e39;
    let _e40 = mvp;
    let _e41 = in_position_1;
    unnamed_1.gl_Position = (_e40 * vec4<f32>(_e41.x, _e41.y, _e41.z, 1f));
    let _e48 = gl_InstanceIndex_1;
    let _e51 = temporalPayloads.temporalPayload[_e48];
    temporal.currentMvp = _e51.currentMvp;
    temporal.previousMvp = _e51.previousMvp;
    temporal.outcomeReserved = _e51.outcomeReserved;
    let _e58 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e58.x, _e58.y, _e58.z, 1f);
    let _e64 = temporal.currentMvp;
    let _e65 = temporalLocalPosition;
    temporalCurrentClip = (_e64 * _e65);
    let _e68 = temporal.previousMvp;
    let _e69 = temporalLocalPosition;
    temporalPreviousClip = (_e68 * _e69);
    let _e73 = temporal.outcomeReserved[0u];
    temporalOutcome = _e73;
    let _e74 = in_color0_1;
    frag_color0_ = _e74;
    let _e76 = ubo.eyePos;
    let _e78 = in_position_1;
    viewer = normalize((_e76.xyz - _e78));
    let _e81 = in_normal_1;
    let _e82 = viewer;
    d = dot(_e81, _e82);
    let _e84 = in_normal_1;
    let _e87 = d;
    let _e89 = viewer;
    reflected = (((_e84.yz * 2f) * _e87) - _e89.yz);
    let _e93 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e93 * 0.5f));
    let _e98 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e98 * 0.5f));
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(5) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_normal_1 = in_normal;
    main_1();
    let _e17 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e17);
    let _e19 = unnamed_1.gl_Position;
    let _e20 = temporalCurrentClip;
    let _e21 = temporalPreviousClip;
    let _e22 = temporalOutcome;
    let _e23 = frag_color0_;
    let _e24 = frag_tex_coord0_;
    return VertexOutput(_e19, _e20, _e21, _e22, _e23, _e24);
}
