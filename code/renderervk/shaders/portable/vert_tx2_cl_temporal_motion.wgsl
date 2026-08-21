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
    @location(5) member_4: vec4<f32>,
    @location(6) member_5: vec4<f32>,
    @location(1) member_6: vec2<f32>,
    @location(2) member_7: vec2<f32>,
    @location(3) member_8: vec2<f32>,
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
var<private> frag_color1_: vec4<f32>;
var<private> in_color1_1: vec4<f32>;
var<private> frag_color2_: vec4<f32>;
var<private> in_color2_1: vec4<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_: vec2<f32>;
var<private> in_tex_coord2_1: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;

    if override_type_5_ {
        let _e34 = gl_InstanceIndex_1;
        let _e38 = unnamed.entMat[(_e34 * 2i)];
        local = _e38;
    } else {
        let _e40 = ubo.mvp;
        local = _e40;
    }
    let _e41 = local;
    mvp = _e41;
    let _e42 = mvp;
    let _e43 = in_position_1;
    unnamed_1.gl_Position = (_e42 * vec4<f32>(_e43.x, _e43.y, _e43.z, 1f));
    let _e50 = gl_InstanceIndex_1;
    let _e53 = temporalPayloads.temporalPayload[_e50];
    temporal.currentMvp = _e53.currentMvp;
    temporal.previousMvp = _e53.previousMvp;
    temporal.outcomeReserved = _e53.outcomeReserved;
    let _e60 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e60.x, _e60.y, _e60.z, 1f);
    let _e66 = temporal.currentMvp;
    let _e67 = temporalLocalPosition;
    temporalCurrentClip = (_e66 * _e67);
    let _e70 = temporal.previousMvp;
    let _e71 = temporalLocalPosition;
    temporalPreviousClip = (_e70 * _e71);
    let _e75 = temporal.outcomeReserved[0u];
    temporalOutcome = _e75;
    let _e76 = in_color0_1;
    frag_color0_ = _e76;
    let _e77 = in_color1_1;
    frag_color1_ = _e77;
    let _e78 = in_color2_1;
    frag_color2_ = _e78;
    let _e79 = in_tex_coord0_1;
    frag_tex_coord0_ = _e79;
    let _e80 = in_tex_coord1_1;
    frag_tex_coord1_ = _e80;
    let _e81 = in_tex_coord2_1;
    frag_tex_coord2_ = _e81;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(7) in_color2_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_color2_1 = in_color2_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e29 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e29);
    let _e31 = unnamed_1.gl_Position;
    let _e32 = temporalCurrentClip;
    let _e33 = temporalPreviousClip;
    let _e34 = temporalOutcome;
    let _e35 = frag_color0_;
    let _e36 = frag_color1_;
    let _e37 = frag_color2_;
    let _e38 = frag_tex_coord0_;
    let _e39 = frag_tex_coord1_;
    let _e40 = frag_tex_coord2_;
    return VertexOutput(_e31, _e32, _e33, _e34, _e35, _e36, _e37, _e38, _e39, _e40);
}
