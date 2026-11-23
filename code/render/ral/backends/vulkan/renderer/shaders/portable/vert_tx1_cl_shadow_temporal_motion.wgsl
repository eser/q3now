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
    @location(5) member_5: vec4<f32>,
    @location(1) member_6: vec2<f32>,
    @location(2) member_7: vec2<f32>,
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
var<private> frag_color1_: vec4<f32>;
var<private> in_color1_1: vec4<f32>;
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
    let _e74 = in_position_1;
    let _e77 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e74.x, _e74.y, _e74.z, _e77);
    let _e82 = in_color0_1;
    frag_color0_ = _e82;
    let _e83 = in_color1_1;
    frag_color1_ = _e83;
    let _e84 = in_tex_coord0_1;
    frag_tex_coord0_ = _e84;
    let _e85 = in_tex_coord1_1;
    frag_tex_coord1_ = _e85;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e24 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e24);
    let _e26 = unnamed_1.gl_Position;
    let _e27 = temporalCurrentClip;
    let _e28 = temporalPreviousClip;
    let _e29 = temporalOutcome;
    let _e30 = shadowData;
    let _e31 = frag_color0_;
    let _e32 = frag_color1_;
    let _e33 = frag_tex_coord0_;
    let _e34 = frag_tex_coord1_;
    return VertexOutput(_e26, _e27, _e28, _e29, _e30, _e31, _e32, _e33, _e34);
}
