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
var<private> in_normal_1: vec3<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> frag_tex_coord1_: vec2<f32>;
var<private> in_tex_coord1_1: vec2<f32>;
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
    let _e86 = ubo.eyePos;
    let _e88 = in_position_1;
    viewer = normalize((_e86.xyz - _e88));
    let _e91 = in_normal_1;
    let _e92 = viewer;
    d = dot(_e91, _e92);
    let _e94 = in_normal_1;
    let _e97 = d;
    let _e99 = viewer;
    reflected = (((_e94.yz * 2f) * _e97) - _e99.yz);
    let _e103 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e103 * 0.5f));
    let _e108 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e108 * 0.5f));
    let _e112 = in_tex_coord1_1;
    frag_tex_coord1_ = _e112;
    let _e113 = in_position_1;
    let _e115 = ubo.fogDistanceVector;
    let _e120 = ubo.fogDistanceVector[3u];
    s = (dot(_e113, _e115.xyz) + _e120);
    let _e122 = in_position_1;
    let _e124 = ubo.fogDepthVector;
    let _e129 = ubo.fogDepthVector[3u];
    t = (dot(_e122, _e124.xyz) + _e129);
    let _e133 = ubo.fogEyeT[1u];
    if (_e133 == 1f) {
        let _e135 = t;
        if (_e135 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e137 = t;
        if (_e137 < 1f) {
            t = 0.03125f;
        } else {
            let _e139 = t;
            let _e141 = t;
            let _e144 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e139) / (_e141 - _e144)));
        }
    }
    let _e148 = s;
    let _e149 = t;
    fog_tex_coord = vec2<f32>(_e148, _e149);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
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
