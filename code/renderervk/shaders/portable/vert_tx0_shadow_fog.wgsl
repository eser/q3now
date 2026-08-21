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

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(7) member: vec4<f32>,
    @location(0) member_1: vec4<f32>,
    @location(1) member_2: vec2<f32>,
    @location(4) member_3: vec2<f32>,
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
var<private> shadowData: vec4<f32>;
var<private> frag_color0_: vec4<f32>;
var<private> in_color0_1: vec4<f32>;
var<private> frag_tex_coord0_: vec2<f32>;
var<private> in_tex_coord0_1: vec2<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e32 = gl_InstanceIndex_1;
        let _e36 = unnamed.entMat[(_e32 * 2i)];
        local = _e36;
    } else {
        let _e38 = ubo.mvp;
        local = _e38;
    }
    let _e39 = local;
    mvp = _e39;
    let _e40 = mvp;
    let _e41 = in_position_1;
    unnamed_1.gl_Position = (_e40 * vec4<f32>(_e41.x, _e41.y, _e41.z, 1f));
    let _e48 = in_position_1;
    let _e51 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e48.x, _e48.y, _e48.z, _e51);
    let _e56 = in_color0_1;
    frag_color0_ = _e56;
    let _e57 = in_tex_coord0_1;
    frag_tex_coord0_ = _e57;
    let _e58 = in_position_1;
    let _e60 = ubo.fogDistanceVector;
    let _e65 = ubo.fogDistanceVector[3u];
    s = (dot(_e58, _e60.xyz) + _e65);
    let _e67 = in_position_1;
    let _e69 = ubo.fogDepthVector;
    let _e74 = ubo.fogDepthVector[3u];
    t = (dot(_e67, _e69.xyz) + _e74);
    let _e78 = ubo.fogEyeT[1u];
    if (_e78 == 1f) {
        let _e80 = t;
        if (_e80 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e82 = t;
        if (_e82 < 1f) {
            t = 0.03125f;
        } else {
            let _e84 = t;
            let _e86 = t;
            let _e89 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e84) / (_e86 - _e89)));
        }
    }
    let _e93 = s;
    let _e94 = t;
    fog_tex_coord = vec2<f32>(_e93, _e94);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_color0_1 = in_color0_;
    in_tex_coord0_1 = in_tex_coord0_;
    main_1();
    let _e16 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e16);
    let _e18 = unnamed_1.gl_Position;
    let _e19 = shadowData;
    let _e20 = frag_color0_;
    let _e21 = frag_tex_coord0_;
    let _e22 = fog_tex_coord;
    return VertexOutput(_e18, _e19, _e20, _e21, _e22);
}
