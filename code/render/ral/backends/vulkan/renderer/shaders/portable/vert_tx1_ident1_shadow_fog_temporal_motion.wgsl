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
    @location(7) member_3: vec4<f32>,
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
var<private> shadowData: vec4<f32>;
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
        let _e39 = gl_InstanceIndex_1;
        let _e43 = unnamed.entMat[(_e39 * 2i)];
        local = _e43;
    } else {
        let _e45 = ubo.mvp;
        local = _e45;
    }
    let _e46 = local;
    mvp = _e46;
    let _e47 = mvp;
    let _e48 = in_position_1;
    unnamed_1.gl_Position = (_e47 * vec4<f32>(_e48.x, _e48.y, _e48.z, 1f));
    let _e55 = gl_InstanceIndex_1;
    let _e58 = temporalPayloads.temporalPayload[_e55];
    temporal.currentMvp = _e58.currentMvp;
    temporal.previousMvp = _e58.previousMvp;
    temporal.outcomeReserved = _e58.outcomeReserved;
    let _e65 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e65.x, _e65.y, _e65.z, 1f);
    let _e71 = temporal.currentMvp;
    let _e72 = temporalLocalPosition;
    temporalCurrentClip = (_e71 * _e72);
    let _e75 = temporal.previousMvp;
    let _e76 = temporalLocalPosition;
    temporalPreviousClip = (_e75 * _e76);
    let _e80 = temporal.outcomeReserved[0u];
    temporalOutcome = _e80;
    let _e81 = in_position_1;
    let _e84 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e81.x, _e81.y, _e81.z, _e84);
    let _e89 = in_tex_coord0_1;
    frag_tex_coord0_ = _e89;
    let _e90 = in_tex_coord1_1;
    frag_tex_coord1_ = _e90;
    let _e91 = in_position_1;
    let _e93 = ubo.fogDistanceVector;
    let _e98 = ubo.fogDistanceVector[3u];
    s = (dot(_e91, _e93.xyz) + _e98);
    let _e100 = in_position_1;
    let _e102 = ubo.fogDepthVector;
    let _e107 = ubo.fogDepthVector[3u];
    t = (dot(_e100, _e102.xyz) + _e107);
    let _e111 = ubo.fogEyeT[1u];
    if (_e111 == 1f) {
        let _e113 = t;
        if (_e113 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e115 = t;
        if (_e115 < 1f) {
            t = 0.03125f;
        } else {
            let _e117 = t;
            let _e119 = t;
            let _e122 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e117) / (_e119 - _e122)));
        }
    }
    let _e126 = s;
    let _e127 = t;
    fog_tex_coord = vec2<f32>(_e126, _e127);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e19 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e19);
    let _e21 = unnamed_1.gl_Position;
    let _e22 = temporalCurrentClip;
    let _e23 = temporalPreviousClip;
    let _e24 = temporalOutcome;
    let _e25 = shadowData;
    let _e26 = frag_tex_coord0_;
    let _e27 = frag_tex_coord1_;
    let _e28 = fog_tex_coord;
    return VertexOutput(_e21, _e22, _e23, _e24, _e25, _e26, _e27, _e28);
}
