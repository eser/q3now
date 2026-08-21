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
    @location(5) member_4: vec4<f32>,
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
var<private> frag_color0_: vec4<f32>;
var<private> in_color0_1: vec4<f32>;
var<private> frag_color1_: vec4<f32>;
var<private> in_color1_1: vec4<f32>;
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
        let _e47 = gl_InstanceIndex_1;
        let _e51 = unnamed.entMat[(_e47 * 2i)];
        local = _e51;
    } else {
        let _e53 = ubo.mvp;
        local = _e53;
    }
    let _e54 = local;
    mvp = _e54;
    let _e55 = mvp;
    let _e56 = in_position_1;
    unnamed_1.gl_Position = (_e55 * vec4<f32>(_e56.x, _e56.y, _e56.z, 1f));
    let _e63 = gl_InstanceIndex_1;
    let _e66 = temporalPayloads.temporalPayload[_e63];
    temporal.currentMvp = _e66.currentMvp;
    temporal.previousMvp = _e66.previousMvp;
    temporal.outcomeReserved = _e66.outcomeReserved;
    let _e73 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e73.x, _e73.y, _e73.z, 1f);
    let _e79 = temporal.currentMvp;
    let _e80 = temporalLocalPosition;
    temporalCurrentClip = (_e79 * _e80);
    let _e83 = temporal.previousMvp;
    let _e84 = temporalLocalPosition;
    temporalPreviousClip = (_e83 * _e84);
    let _e88 = temporal.outcomeReserved[0u];
    temporalOutcome = _e88;
    let _e89 = in_color0_1;
    frag_color0_ = _e89;
    let _e90 = in_color1_1;
    frag_color1_ = _e90;
    let _e92 = ubo.eyePos;
    let _e94 = in_position_1;
    viewer = normalize((_e92.xyz - _e94));
    let _e97 = in_normal_1;
    let _e98 = viewer;
    d = dot(_e97, _e98);
    let _e100 = in_normal_1;
    let _e103 = d;
    let _e105 = viewer;
    reflected = (((_e100.yz * 2f) * _e103) - _e105.yz);
    let _e109 = reflected[0u];
    frag_tex_coord0_[0u] = (0.5f + (_e109 * 0.5f));
    let _e114 = reflected[1u];
    frag_tex_coord0_[1u] = (0.5f - (_e114 * 0.5f));
    let _e118 = in_tex_coord1_1;
    frag_tex_coord1_ = _e118;
    let _e119 = in_position_1;
    let _e121 = ubo.fogDistanceVector;
    let _e126 = ubo.fogDistanceVector[3u];
    s = (dot(_e119, _e121.xyz) + _e126);
    let _e128 = in_position_1;
    let _e130 = ubo.fogDepthVector;
    let _e135 = ubo.fogDepthVector[3u];
    t = (dot(_e128, _e130.xyz) + _e135);
    let _e139 = ubo.fogEyeT[1u];
    if (_e139 == 1f) {
        let _e141 = t;
        if (_e141 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e143 = t;
        if (_e143 < 1f) {
            t = 0.03125f;
        } else {
            let _e145 = t;
            let _e147 = t;
            let _e150 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e145) / (_e147 - _e150)));
        }
    }
    let _e154 = s;
    let _e155 = t;
    fog_tex_coord = vec2<f32>(_e154, _e155);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(5) in_normal: vec3<f32>, @location(3) in_tex_coord1_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_normal_1 = in_normal;
    in_tex_coord1_1 = in_tex_coord1_;
    main_1();
    let _e24 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e24);
    let _e26 = unnamed_1.gl_Position;
    let _e27 = temporalCurrentClip;
    let _e28 = temporalPreviousClip;
    let _e29 = temporalOutcome;
    let _e30 = frag_color0_;
    let _e31 = frag_color1_;
    let _e32 = frag_tex_coord0_;
    let _e33 = frag_tex_coord1_;
    let _e34 = fog_tex_coord;
    return VertexOutput(_e26, _e27, _e28, _e29, _e30, _e31, _e32, _e33, _e34);
}
