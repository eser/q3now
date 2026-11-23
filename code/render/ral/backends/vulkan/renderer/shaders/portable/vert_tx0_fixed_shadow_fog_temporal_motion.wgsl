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
    @location(4) member_5: vec2<f32>,
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
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e37 = gl_InstanceIndex_1;
        let _e41 = unnamed.entMat[(_e37 * 2i)];
        local = _e41;
    } else {
        let _e43 = ubo.mvp;
        local = _e43;
    }
    let _e44 = local;
    mvp = _e44;
    let _e45 = mvp;
    let _e46 = in_position_1;
    unnamed_1.gl_Position = (_e45 * vec4<f32>(_e46.x, _e46.y, _e46.z, 1f));
    let _e53 = gl_InstanceIndex_1;
    let _e56 = temporalPayloads.temporalPayload[_e53];
    temporal.currentMvp = _e56.currentMvp;
    temporal.previousMvp = _e56.previousMvp;
    temporal.outcomeReserved = _e56.outcomeReserved;
    let _e63 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e63.x, _e63.y, _e63.z, 1f);
    let _e69 = temporal.currentMvp;
    let _e70 = temporalLocalPosition;
    temporalCurrentClip = (_e69 * _e70);
    let _e73 = temporal.previousMvp;
    let _e74 = temporalLocalPosition;
    temporalPreviousClip = (_e73 * _e74);
    let _e78 = temporal.outcomeReserved[0u];
    temporalOutcome = _e78;
    let _e79 = in_position_1;
    let _e82 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e79.x, _e79.y, _e79.z, _e82);
    let _e87 = in_tex_coord0_1;
    frag_tex_coord0_ = _e87;
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
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e16 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e16);
    let _e18 = unnamed_1.gl_Position;
    let _e19 = temporalCurrentClip;
    let _e20 = temporalPreviousClip;
    let _e21 = temporalOutcome;
    let _e22 = shadowData;
    let _e23 = frag_tex_coord0_;
    let _e24 = fog_tex_coord;
    return VertexOutput(_e18, _e19, _e20, _e21, _e22, _e23, _e24);
}
