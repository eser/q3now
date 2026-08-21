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
    @location(0) member: vec4<f32>,
    @location(1) member_1: vec2<f32>,
    @location(2) member_2: vec2<f32>,
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
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e33 = gl_InstanceIndex_1;
        let _e37 = unnamed.entMat[(_e33 * 2i)];
        local = _e37;
    } else {
        let _e39 = ubo.mvp;
        local = _e39;
    }
    let _e40 = local;
    mvp = _e40;
    let _e41 = mvp;
    let _e42 = in_position_1;
    unnamed_1.gl_Position = (_e41 * vec4<f32>(_e42.x, _e42.y, _e42.z, 1f));
    let _e49 = in_color0_1;
    frag_color0_ = _e49;
    let _e50 = in_tex_coord0_1;
    frag_tex_coord0_ = _e50;
    let _e51 = in_tex_coord1_1;
    frag_tex_coord1_ = _e51;
    let _e52 = in_position_1;
    let _e54 = ubo.fogDistanceVector;
    let _e59 = ubo.fogDistanceVector[3u];
    s = (dot(_e52, _e54.xyz) + _e59);
    let _e61 = in_position_1;
    let _e63 = ubo.fogDepthVector;
    let _e68 = ubo.fogDepthVector[3u];
    t = (dot(_e61, _e63.xyz) + _e68);
    let _e72 = ubo.fogEyeT[1u];
    if (_e72 == 1f) {
        let _e74 = t;
        if (_e74 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e76 = t;
        if (_e76 < 1f) {
            t = 0.03125f;
        } else {
            let _e78 = t;
            let _e80 = t;
            let _e83 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e78) / (_e80 - _e83)));
        }
    }
    let _e87 = s;
    let _e88 = t;
    fog_tex_coord = vec2<f32>(_e87, _e88);
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
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    let _e21 = frag_color0_;
    let _e22 = frag_tex_coord0_;
    let _e23 = frag_tex_coord1_;
    let _e24 = fog_tex_coord;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24);
}
