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
var<private> in_normal_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var temporal: TemporalPayloadRecord;
    var temporalLocalPosition: vec4<f32>;
    var viewer: vec3<f32>;
    var d: f32;
    var reflected: vec2<f32>;
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
    let _e84 = ubo.eyePos;
    let _e86 = in_position_1;
    viewer = normalize((_e84.xyz - _e86));
    let _e89 = in_normal_1;
    let _e90 = viewer;
    d = dot(_e89, _e90);
    let _e92 = in_normal_1;
    let _e95 = d;
    let _e97 = viewer;
    reflected = (((_e92.yz * 2f) * _e95) - _e97.yz);
    let _e101 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e101 * 0.5f));
    let _e106 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e106 * 0.5f));
    let _e110 = in_position_1;
    let _e112 = ubo.fogDistanceVector;
    let _e117 = ubo.fogDistanceVector[3u];
    s = (dot(_e110, _e112.xyz) + _e117);
    let _e119 = in_position_1;
    let _e121 = ubo.fogDepthVector;
    let _e126 = ubo.fogDepthVector[3u];
    t = (dot(_e119, _e121.xyz) + _e126);
    let _e130 = ubo.fogEyeT[1u];
    if (_e130 == 1f) {
        let _e132 = t;
        if (_e132 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e134 = t;
        if (_e134 < 1f) {
            t = 0.03125f;
        } else {
            let _e136 = t;
            let _e138 = t;
            let _e141 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e136) / (_e138 - _e141)));
        }
    }
    let _e145 = s;
    let _e146 = t;
    fog_tex_coord = vec2<f32>(_e145, _e146);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
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
