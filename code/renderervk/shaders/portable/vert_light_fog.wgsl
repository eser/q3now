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
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec3<f32>,
    @location(2) member_2: vec4<f32>,
    @location(3) member_3: vec4<f32>,
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
var<private> frag_tex_coord: vec2<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> N: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> L: vec4<f32>;
var<private> V: vec4<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e34 = gl_InstanceIndex_1;
        let _e38 = unnamed.entMat[(_e34 * 2i)];
        local = _e38;
    } else {
        let _e40 = ubo.mvp;
        local = _e40;
    }
    let _e41 = local;
    mvp = _e41;
    let _e42 = mvp;
    let _e43 = in_position_1;
    unnamed_1.gl_Position = (_e42 * vec4<f32>(_e43.x, _e43.y, _e43.z, 1f));
    let _e50 = in_tex_coord_1;
    frag_tex_coord = _e50;
    let _e51 = in_normal_1;
    N = _e51;
    let _e53 = ubo.lightPos;
    let _e54 = in_position_1;
    L = (_e53 - vec4<f32>(_e54.x, _e54.y, _e54.z, 1f));
    let _e61 = ubo.eyePos;
    let _e62 = in_position_1;
    V = (_e61 - vec4<f32>(_e62.x, _e62.y, _e62.z, 1f));
    let _e68 = in_position_1;
    let _e70 = ubo.fogDistanceVector;
    let _e75 = ubo.fogDistanceVector[3u];
    s = (dot(_e68, _e70.xyz) + _e75);
    let _e77 = in_position_1;
    let _e79 = ubo.fogDepthVector;
    let _e84 = ubo.fogDepthVector[3u];
    t = (dot(_e77, _e79.xyz) + _e84);
    let _e88 = ubo.fogEyeT[1u];
    if (_e88 == 1f) {
        let _e90 = t;
        if (_e90 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e92 = t;
        if (_e92 < 1f) {
            t = 0.03125f;
        } else {
            let _e94 = t;
            let _e96 = t;
            let _e99 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e94) / (_e96 - _e99)));
        }
    }
    let _e103 = s;
    let _e104 = t;
    fog_tex_coord = vec2<f32>(_e103, _e104);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_tex_coord: vec2<f32>, @location(2) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord_1 = in_tex_coord;
    in_normal_1 = in_normal;
    main_1();
    let _e17 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e17);
    let _e19 = unnamed_1.gl_Position;
    let _e20 = frag_tex_coord;
    let _e21 = N;
    let _e22 = L;
    let _e23 = V;
    let _e24 = fog_tex_coord;
    return VertexOutput(_e19, _e20, _e21, _e22, _e23, _e24);
}
