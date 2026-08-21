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
    @location(5) member_4: vec3<f32>,
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
var<private> frag_tex_coord: vec2<f32>;
var<private> in_tex_coord_1: vec2<f32>;
var<private> N: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> L: vec4<f32>;
var<private> V: vec4<f32>;
var<private> frag_position: vec3<f32>;
var<private> fog_tex_coord: vec2<f32>;

fn main_1() {
    var mvp: mat4x4<f32>;
    var local: mat4x4<f32>;
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e35 = gl_InstanceIndex_1;
        let _e39 = unnamed.entMat[(_e35 * 2i)];
        local = _e39;
    } else {
        let _e41 = ubo.mvp;
        local = _e41;
    }
    let _e42 = local;
    mvp = _e42;
    let _e43 = mvp;
    let _e44 = in_position_1;
    unnamed_1.gl_Position = (_e43 * vec4<f32>(_e44.x, _e44.y, _e44.z, 1f));
    let _e51 = in_tex_coord_1;
    frag_tex_coord = _e51;
    let _e52 = in_normal_1;
    N = _e52;
    let _e54 = ubo.lightPos;
    let _e55 = in_position_1;
    L = (_e54 - vec4<f32>(_e55.x, _e55.y, _e55.z, 1f));
    let _e62 = ubo.eyePos;
    let _e63 = in_position_1;
    V = (_e62 - vec4<f32>(_e63.x, _e63.y, _e63.z, 1f));
    let _e69 = in_position_1;
    frag_position = _e69;
    let _e70 = in_position_1;
    let _e72 = ubo.fogDistanceVector;
    let _e77 = ubo.fogDistanceVector[3u];
    s = (dot(_e70, _e72.xyz) + _e77);
    let _e79 = in_position_1;
    let _e81 = ubo.fogDepthVector;
    let _e86 = ubo.fogDepthVector[3u];
    t = (dot(_e79, _e81.xyz) + _e86);
    let _e90 = ubo.fogEyeT[1u];
    if (_e90 == 1f) {
        let _e92 = t;
        if (_e92 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e94 = t;
        if (_e94 < 1f) {
            t = 0.03125f;
        } else {
            let _e96 = t;
            let _e98 = t;
            let _e101 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e96) / (_e98 - _e101)));
        }
    }
    let _e105 = s;
    let _e106 = t;
    fog_tex_coord = vec2<f32>(_e105, _e106);
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(1) in_tex_coord: vec2<f32>, @location(2) in_normal: vec3<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_tex_coord_1 = in_tex_coord;
    in_normal_1 = in_normal;
    main_1();
    let _e18 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e18);
    let _e20 = unnamed_1.gl_Position;
    let _e21 = frag_tex_coord;
    let _e22 = N;
    let _e23 = L;
    let _e24 = V;
    let _e25 = frag_position;
    let _e26 = fog_tex_coord;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24, _e25, _e26);
}
