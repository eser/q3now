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
var<private> in_normal_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;

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
    let _e75 = ubo.eyePos;
    let _e77 = in_position_1;
    viewer = normalize((_e75.xyz - _e77));
    let _e80 = in_normal_1;
    let _e81 = viewer;
    d = dot(_e80, _e81);
    let _e83 = in_normal_1;
    let _e86 = d;
    let _e88 = viewer;
    reflected = (((_e83.yz * 2f) * _e86) - _e88.yz);
    let _e92 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e92 * 0.5f));
    let _e97 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e97 * 0.5f));
    let _e101 = in_tex_coord1_1;
    frag_tex_coord1_ = _e101;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
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
