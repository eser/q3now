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
    @location(3) member_7: vec2<f32>,
    @location(4) member_8: vec2<f32>,
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
var<private> frag_tex_coord2_: vec2<f32>;
var<private> in_tex_coord2_1: vec2<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e43 = gl_InstanceIndex_1;
        let _e47 = unnamed.entMat[(_e43 * 2i)];
        local = _e47;
    } else {
        let _e49 = ubo.mvp;
        local = _e49;
    }
    let _e50 = local;
    mvp = _e50;
    let _e51 = mvp;
    let _e52 = in_position_1;
    unnamed_1.gl_Position = (_e51 * vec4<f32>(_e52.x, _e52.y, _e52.z, 1f));
    let _e59 = gl_InstanceIndex_1;
    let _e62 = temporalPayloads.temporalPayload[_e59];
    temporal.currentMvp = _e62.currentMvp;
    temporal.previousMvp = _e62.previousMvp;
    temporal.outcomeReserved = _e62.outcomeReserved;
    let _e69 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e69.x, _e69.y, _e69.z, 1f);
    let _e75 = temporal.currentMvp;
    let _e76 = temporalLocalPosition;
    temporalCurrentClip = (_e75 * _e76);
    let _e79 = temporal.previousMvp;
    let _e80 = temporalLocalPosition;
    temporalPreviousClip = (_e79 * _e80);
    let _e84 = temporal.outcomeReserved[0u];
    temporalOutcome = _e84;
    let _e85 = in_position_1;
    let _e88 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e85.x, _e85.y, _e85.z, _e88);
    let _e93 = in_color0_1;
    frag_color0_ = _e93;
    let _e94 = in_tex_coord0_1;
    frag_tex_coord0_ = _e94;
    let _e95 = in_tex_coord1_1;
    frag_tex_coord1_ = _e95;
    let _e96 = in_tex_coord2_1;
    frag_tex_coord2_ = _e96;
    let _e97 = in_position_1;
    let _e99 = ubo.fogDistanceVector;
    let _e104 = ubo.fogDistanceVector[3u];
    s = (dot(_e97, _e99.xyz) + _e104);
    let _e106 = in_position_1;
    let _e108 = ubo.fogDepthVector;
    let _e113 = ubo.fogDepthVector[3u];
    t = (dot(_e106, _e108.xyz) + _e113);
    let _e117 = ubo.fogEyeT[1u];
    if (_e117 == 1f) {
        let _e119 = t;
        if (_e119 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e121 = t;
        if (_e121 < 1f) {
            t = 0.03125f;
        } else {
            let _e123 = t;
            let _e125 = t;
            let _e128 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e123) / (_e125 - _e128)));
        }
    }
    let _e132 = s;
    let _e133 = t;
    fog_tex_coord = vec2<f32>(_e132, _e133);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e25 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e25);
    let _e27 = unnamed_1.gl_Position;
    let _e28 = temporalCurrentClip;
    let _e29 = temporalPreviousClip;
    let _e30 = temporalOutcome;
    let _e31 = shadowData;
    let _e32 = frag_color0_;
    let _e33 = frag_tex_coord0_;
    let _e34 = frag_tex_coord1_;
    let _e35 = frag_tex_coord2_;
    let _e36 = fog_tex_coord;
    return VertexOutput(_e27, _e28, _e29, _e30, _e31, _e32, _e33, _e34, _e35, _e36);
}
