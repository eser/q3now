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
    @location(1) member_3: vec2<f32>,
    @location(2) member_4: vec2<f32>,
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
        let _e38 = gl_InstanceIndex_1;
        let _e42 = unnamed.entMat[(_e38 * 2i)];
        local = _e42;
    } else {
        let _e44 = ubo.mvp;
        local = _e44;
    }
    let _e45 = local;
    mvp = _e45;
    let _e46 = mvp;
    let _e47 = in_position_1;
    unnamed_1.gl_Position = (_e46 * vec4<f32>(_e47.x, _e47.y, _e47.z, 1f));
    let _e54 = gl_InstanceIndex_1;
    let _e57 = temporalPayloads.temporalPayload[_e54];
    temporal.currentMvp = _e57.currentMvp;
    temporal.previousMvp = _e57.previousMvp;
    temporal.outcomeReserved = _e57.outcomeReserved;
    let _e64 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e64.x, _e64.y, _e64.z, 1f);
    let _e70 = temporal.currentMvp;
    let _e71 = temporalLocalPosition;
    temporalCurrentClip = (_e70 * _e71);
    let _e74 = temporal.previousMvp;
    let _e75 = temporalLocalPosition;
    temporalPreviousClip = (_e74 * _e75);
    let _e79 = temporal.outcomeReserved[0u];
    temporalOutcome = _e79;
    let _e80 = in_tex_coord0_1;
    frag_tex_coord0_ = _e80;
    let _e81 = in_tex_coord1_1;
    frag_tex_coord1_ = _e81;
    let _e82 = in_position_1;
    let _e84 = ubo.fogDistanceVector;
    let _e89 = ubo.fogDistanceVector[3u];
    s = (dot(_e82, _e84.xyz) + _e89);
    let _e91 = in_position_1;
    let _e93 = ubo.fogDepthVector;
    let _e98 = ubo.fogDepthVector[3u];
    t = (dot(_e91, _e93.xyz) + _e98);
    let _e102 = ubo.fogEyeT[1u];
    if (_e102 == 1f) {
        let _e104 = t;
        if (_e104 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e106 = t;
        if (_e106 < 1f) {
            t = 0.03125f;
        } else {
            let _e108 = t;
            let _e110 = t;
            let _e113 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e108) / (_e110 - _e113)));
        }
    }
    let _e117 = s;
    let _e118 = t;
    fog_tex_coord = vec2<f32>(_e117, _e118);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    let _e21 = temporalCurrentClip;
    let _e22 = temporalPreviousClip;
    let _e23 = temporalOutcome;
    let _e24 = frag_tex_coord0_;
    let _e25 = frag_tex_coord1_;
    let _e26 = fog_tex_coord;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24, _e25, _e26);
}
