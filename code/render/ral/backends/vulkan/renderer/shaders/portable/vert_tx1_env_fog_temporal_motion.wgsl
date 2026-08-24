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
        let _e45 = gl_InstanceIndex_1;
        let _e49 = unnamed.entMat[(_e45 * 2i)];
        local = _e49;
    } else {
        let _e51 = ubo.mvp;
        local = _e51;
    }
    let _e52 = local;
    mvp = _e52;
    let _e53 = mvp;
    let _e54 = in_position_1;
    unnamed_1.gl_Position = (_e53 * vec4<f32>(_e54.x, _e54.y, _e54.z, 1f));
    let _e61 = gl_InstanceIndex_1;
    let _e64 = temporalPayloads.temporalPayload[_e61];
    temporal.currentMvp = _e64.currentMvp;
    temporal.previousMvp = _e64.previousMvp;
    temporal.outcomeReserved = _e64.outcomeReserved;
    let _e71 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e71.x, _e71.y, _e71.z, 1f);
    let _e77 = temporal.currentMvp;
    let _e78 = temporalLocalPosition;
    temporalCurrentClip = (_e77 * _e78);
    let _e81 = temporal.previousMvp;
    let _e82 = temporalLocalPosition;
    temporalPreviousClip = (_e81 * _e82);
    let _e86 = temporal.outcomeReserved[0u];
    temporalOutcome = _e86;
    let _e87 = in_color0_1;
    frag_color0_ = _e87;
    let _e89 = ubo.eyePos;
    let _e91 = in_position_1;
    viewer = normalize((_e89.xyz - _e91));
    let _e94 = in_normal_1;
    let _e95 = viewer;
    d = dot(_e94, _e95);
    let _e97 = in_normal_1;
    let _e100 = d;
    let _e102 = viewer;
    reflected = (((_e97.yz * 2f) * _e100) - _e102.yz);
    let _e106 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e106 * 0.5f));
    let _e111 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e111 * 0.5f));
    let _e115 = in_tex_coord1_1;
    frag_tex_coord1_ = _e115;
    let _e116 = in_position_1;
    let _e118 = ubo.fogDistanceVector;
    let _e123 = ubo.fogDistanceVector[3u];
    s = (dot(_e116, _e118.xyz) + _e123);
    let _e125 = in_position_1;
    let _e127 = ubo.fogDepthVector;
    let _e132 = ubo.fogDepthVector[3u];
    t = (dot(_e125, _e127.xyz) + _e132);
    let _e136 = ubo.fogEyeT[1u];
    if (_e136 == 1f) {
        let _e138 = t;
        if (_e138 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e140 = t;
        if (_e140 < 1f) {
            t = 0.03125f;
        } else {
            let _e142 = t;
            let _e144 = t;
            let _e147 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e142) / (_e144 - _e147)));
        }
    }
    let _e151 = s;
    let _e152 = t;
    fog_tex_coord = vec2<f32>(_e151, _e152);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_normal_1 = in_normal;
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
