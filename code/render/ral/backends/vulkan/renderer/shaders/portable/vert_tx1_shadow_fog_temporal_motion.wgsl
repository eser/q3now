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
    @location(0) member_4: vec4<f32>,
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
var<private> shadowData: vec4<f32>;
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
        let _e41 = gl_InstanceIndex_1;
        let _e45 = unnamed.entMat[(_e41 * 2i)];
        local = _e45;
    } else {
        let _e47 = ubo.mvp;
        local = _e47;
    }
    let _e48 = local;
    mvp = _e48;
    let _e49 = mvp;
    let _e50 = in_position_1;
    unnamed_1.gl_Position = (_e49 * vec4<f32>(_e50.x, _e50.y, _e50.z, 1f));
    let _e57 = gl_InstanceIndex_1;
    let _e60 = temporalPayloads.temporalPayload[_e57];
    temporal.currentMvp = _e60.currentMvp;
    temporal.previousMvp = _e60.previousMvp;
    temporal.outcomeReserved = _e60.outcomeReserved;
    let _e67 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e67.x, _e67.y, _e67.z, 1f);
    let _e73 = temporal.currentMvp;
    let _e74 = temporalLocalPosition;
    temporalCurrentClip = (_e73 * _e74);
    let _e77 = temporal.previousMvp;
    let _e78 = temporalLocalPosition;
    temporalPreviousClip = (_e77 * _e78);
    let _e82 = temporal.outcomeReserved[0u];
    temporalOutcome = _e82;
    let _e83 = in_position_1;
    let _e86 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e83.x, _e83.y, _e83.z, _e86);
    let _e91 = in_color0_1;
    frag_color0_ = _e91;
    let _e92 = in_tex_coord0_1;
    frag_tex_coord0_ = _e92;
    let _e93 = in_tex_coord1_1;
    frag_tex_coord1_ = _e93;
    let _e94 = in_position_1;
    let _e96 = ubo.fogDistanceVector;
    let _e101 = ubo.fogDistanceVector[3u];
    s = (dot(_e94, _e96.xyz) + _e101);
    let _e103 = in_position_1;
    let _e105 = ubo.fogDepthVector;
    let _e110 = ubo.fogDepthVector[3u];
    t = (dot(_e103, _e105.xyz) + _e110);
    let _e114 = ubo.fogEyeT[1u];
    if (_e114 == 1f) {
        let _e116 = t;
        if (_e116 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e118 = t;
        if (_e118 < 1f) {
            t = 0.03125f;
        } else {
            let _e120 = t;
            let _e122 = t;
            let _e125 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e120) / (_e122 - _e125)));
        }
    }
    let _e129 = s;
    let _e130 = t;
    fog_tex_coord = vec2<f32>(_e129, _e130);
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
    let _e22 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e22);
    let _e24 = unnamed_1.gl_Position;
    let _e25 = temporalCurrentClip;
    let _e26 = temporalPreviousClip;
    let _e27 = temporalOutcome;
    let _e28 = shadowData;
    let _e29 = frag_color0_;
    let _e30 = frag_tex_coord0_;
    let _e31 = frag_tex_coord1_;
    let _e32 = fog_tex_coord;
    return VertexOutput(_e24, _e25, _e26, _e27, _e28, _e29, _e30, _e31, _e32);
}
