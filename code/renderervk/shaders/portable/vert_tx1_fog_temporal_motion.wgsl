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
    @location(1) member_4: vec2<f32>,
    @location(2) member_5: vec2<f32>,
    @location(4) member_6: vec2<f32>,
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
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e40 = gl_InstanceIndex_1;
        let _e44 = unnamed.entMat[(_e40 * 2i)];
        local = _e44;
    } else {
        let _e46 = ubo.mvp;
        local = _e46;
    }
    let _e47 = local;
    mvp = _e47;
    let _e48 = mvp;
    let _e49 = in_position_1;
    unnamed_1.gl_Position = (_e48 * vec4<f32>(_e49.x, _e49.y, _e49.z, 1f));
    let _e56 = gl_InstanceIndex_1;
    let _e59 = temporalPayloads.temporalPayload[_e56];
    temporal.currentMvp = _e59.currentMvp;
    temporal.previousMvp = _e59.previousMvp;
    temporal.outcomeReserved = _e59.outcomeReserved;
    let _e66 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e66.x, _e66.y, _e66.z, 1f);
    let _e72 = temporal.currentMvp;
    let _e73 = temporalLocalPosition;
    temporalCurrentClip = (_e72 * _e73);
    let _e76 = temporal.previousMvp;
    let _e77 = temporalLocalPosition;
    temporalPreviousClip = (_e76 * _e77);
    let _e81 = temporal.outcomeReserved[0u];
    temporalOutcome = _e81;
    let _e82 = in_color0_1;
    frag_color0_ = _e82;
    let _e83 = in_tex_coord0_1;
    frag_tex_coord0_ = _e83;
    let _e84 = in_tex_coord1_1;
    frag_tex_coord1_ = _e84;
    let _e85 = in_position_1;
    let _e87 = ubo.fogDistanceVector;
    let _e92 = ubo.fogDistanceVector[3u];
    s = (dot(_e85, _e87.xyz) + _e92);
    let _e94 = in_position_1;
    let _e96 = ubo.fogDepthVector;
    let _e101 = ubo.fogDepthVector[3u];
    t = (dot(_e94, _e96.xyz) + _e101);
    let _e105 = ubo.fogEyeT[1u];
    if (_e105 == 1f) {
        let _e107 = t;
        if (_e107 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e109 = t;
        if (_e109 < 1f) {
            t = 0.03125f;
        } else {
            let _e111 = t;
            let _e113 = t;
            let _e116 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e111) / (_e113 - _e116)));
        }
    }
    let _e120 = s;
    let _e121 = t;
    fog_tex_coord = vec2<f32>(_e120, _e121);
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
    let _e27 = frag_color0_;
    let _e28 = frag_tex_coord0_;
    let _e29 = frag_tex_coord1_;
    let _e30 = fog_tex_coord;
    return VertexOutput(_e23, _e24, _e25, _e26, _e27, _e28, _e29, _e30);
}
