struct EntityMatrices {
    entMat: array<mat4x4<f32>>,
}

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_mvp: array<vec4<f32>, 22>,
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
    @location(1) member_5: vec2<f32>,
    @location(2) member_6: vec2<f32>,
    @location(4) member_7: vec2<f32>,
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
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e42 = gl_InstanceIndex_1;
        let _e46 = unnamed.entMat[(_e42 * 2i)];
        local = _e46;
    } else {
        let _e48 = ubo.mvp;
        local = _e48;
    }
    let _e49 = local;
    mvp = _e49;
    let _e50 = mvp;
    let _e51 = in_position_1;
    unnamed_1.gl_Position = (_e50 * vec4<f32>(_e51.x, _e51.y, _e51.z, 1f));
    let _e58 = gl_InstanceIndex_1;
    let _e61 = temporalPayloads.temporalPayload[_e58];
    temporal.currentMvp = _e61.currentMvp;
    temporal.previousMvp = _e61.previousMvp;
    temporal.outcomeReserved = _e61.outcomeReserved;
    let _e68 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e68.x, _e68.y, _e68.z, 1f);
    let _e74 = temporal.currentMvp;
    let _e75 = temporalLocalPosition;
    temporalCurrentClip = (_e74 * _e75);
    let _e78 = temporal.previousMvp;
    let _e79 = temporalLocalPosition;
    temporalPreviousClip = (_e78 * _e79);
    let _e83 = temporal.outcomeReserved[0u];
    temporalOutcome = _e83;
    let _e84 = in_color0_1;
    frag_color0_ = _e84;
    let _e85 = in_color1_1;
    frag_color1_ = _e85;
    let _e86 = in_tex_coord0_1;
    frag_tex_coord0_ = _e86;
    let _e87 = in_tex_coord1_1;
    frag_tex_coord1_ = _e87;
    let _e88 = in_position_1;
    let _e90 = ubo.fogDistanceVector;
    let _e95 = ubo.fogDistanceVector[3u];
    s = (dot(_e88, _e90.xyz) + _e95);
    let _e97 = in_position_1;
    let _e99 = ubo.fogDepthVector;
    let _e104 = ubo.fogDepthVector[3u];
    t = (dot(_e97, _e99.xyz) + _e104);
    let _e108 = ubo.fogEyeT[1u];
    if (_e108 == 1f) {
        let _e110 = t;
        if (_e110 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e112 = t;
        if (_e112 < 1f) {
            t = 0.03125f;
        } else {
            let _e114 = t;
            let _e116 = t;
            let _e119 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e114) / (_e116 - _e119)));
        }
    }
    let _e123 = s;
    let _e124 = t;
    fog_tex_coord = vec2<f32>(_e123, _e124);
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
    let _e30 = frag_color0_;
    let _e31 = frag_color1_;
    let _e32 = frag_tex_coord0_;
    let _e33 = frag_tex_coord1_;
    let _e34 = fog_tex_coord;
    return VertexOutput(_e26, _e27, _e28, _e29, _e30, _e31, _e32, _e33, _e34);
}
