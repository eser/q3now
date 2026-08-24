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
    @location(6) member_5: vec4<f32>,
    @location(1) member_6: vec2<f32>,
    @location(2) member_7: vec2<f32>,
    @location(3) member_8: vec2<f32>,
    @location(4) member_9: vec2<f32>,
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
var<private> frag_color2_: vec4<f32>;
var<private> in_color2_1: vec4<f32>;
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
        let _e46 = gl_InstanceIndex_1;
        let _e50 = unnamed.entMat[(_e46 * 2i)];
        local = _e50;
    } else {
        let _e52 = ubo.mvp;
        local = _e52;
    }
    let _e53 = local;
    mvp = _e53;
    let _e54 = mvp;
    let _e55 = in_position_1;
    unnamed_1.gl_Position = (_e54 * vec4<f32>(_e55.x, _e55.y, _e55.z, 1f));
    let _e62 = gl_InstanceIndex_1;
    let _e65 = temporalPayloads.temporalPayload[_e62];
    temporal.currentMvp = _e65.currentMvp;
    temporal.previousMvp = _e65.previousMvp;
    temporal.outcomeReserved = _e65.outcomeReserved;
    let _e72 = in_position_1;
    temporalLocalPosition = vec4<f32>(_e72.x, _e72.y, _e72.z, 1f);
    let _e78 = temporal.currentMvp;
    let _e79 = temporalLocalPosition;
    temporalCurrentClip = (_e78 * _e79);
    let _e82 = temporal.previousMvp;
    let _e83 = temporalLocalPosition;
    temporalPreviousClip = (_e82 * _e83);
    let _e87 = temporal.outcomeReserved[0u];
    temporalOutcome = _e87;
    let _e88 = in_color0_1;
    frag_color0_ = _e88;
    let _e89 = in_color1_1;
    frag_color1_ = _e89;
    let _e90 = in_color2_1;
    frag_color2_ = _e90;
    let _e91 = in_tex_coord0_1;
    frag_tex_coord0_ = _e91;
    let _e92 = in_tex_coord1_1;
    frag_tex_coord1_ = _e92;
    let _e93 = in_tex_coord2_1;
    frag_tex_coord2_ = _e93;
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
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(7) in_color2_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_color2_1 = in_color2_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e30 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e30);
    let _e32 = unnamed_1.gl_Position;
    let _e33 = temporalCurrentClip;
    let _e34 = temporalPreviousClip;
    let _e35 = temporalOutcome;
    let _e36 = frag_color0_;
    let _e37 = frag_color1_;
    let _e38 = frag_color2_;
    let _e39 = frag_tex_coord0_;
    let _e40 = frag_tex_coord1_;
    let _e41 = frag_tex_coord2_;
    let _e42 = fog_tex_coord;
    return VertexOutput(_e32, _e33, _e34, _e35, _e36, _e37, _e38, _e39, _e40, _e41, _e42);
}
