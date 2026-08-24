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
    @location(4) member_4: vec2<f32>,
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
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e36 = gl_InstanceIndex_1;
        let _e40 = unnamed.entMat[(_e36 * 2i)];
        local = _e40;
    } else {
        let _e42 = ubo.mvp;
        local = _e42;
    }
    let _e43 = local;
    mvp = _e43;
    let _e44 = mvp;
    let _e45 = in_position_1;
    unnamed_1.gl_Position = (_e44 * vec4<f32>(_e45.x, _e45.y, _e45.z, 1f));
    let _e52 = gl_InstanceIndex_1;
    let _e55 = temporalPayloads.temporalPayload[_e52];
    temporal.currentMvp = _e55.currentMvp;
    temporal.previousMvp = _e55.previousMvp;
    temporal.outcomeReserved = _e55.outcomeReserved;
    let _e62 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e62.x, _e62.y, _e62.z, 1f);
    let _e68 = temporal.currentMvp;
    let _e69 = temporalLocalPosition;
    temporalCurrentClip = (_e68 * _e69);
    let _e72 = temporal.previousMvp;
    let _e73 = temporalLocalPosition;
    temporalPreviousClip = (_e72 * _e73);
    let _e77 = temporal.outcomeReserved[0u];
    temporalOutcome = _e77;
    let _e78 = in_tex_coord0_1;
    frag_tex_coord0_ = _e78;
    let _e79 = in_position_1;
    let _e81 = ubo.fogDistanceVector;
    let _e86 = ubo.fogDistanceVector[3u];
    s = (dot(_e79, _e81.xyz) + _e86);
    let _e88 = in_position_1;
    let _e90 = ubo.fogDepthVector;
    let _e95 = ubo.fogDepthVector[3u];
    t = (dot(_e88, _e90.xyz) + _e95);
    let _e99 = ubo.fogEyeT[1u];
    if (_e99 == 1f) {
        let _e101 = t;
        if (_e101 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e103 = t;
        if (_e103 < 1f) {
            t = 0.03125f;
        } else {
            let _e105 = t;
            let _e107 = t;
            let _e110 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e105) / (_e107 - _e110)));
        }
    }
    let _e114 = s;
    let _e115 = t;
    fog_tex_coord = vec2<f32>(_e114, _e115);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e15 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e15);
    let _e17 = unnamed_1.gl_Position;
    let _e18 = temporalCurrentClip;
    let _e19 = temporalPreviousClip;
    let _e20 = temporalOutcome;
    let _e21 = frag_tex_coord0_;
    let _e22 = fog_tex_coord;
    return VertexOutput(_e17, _e18, _e19, _e20, _e21, _e22);
}
