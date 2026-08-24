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
    @location(8) member: vec3<f32>,
    @location(9) member_1: vec3<f32>,
    @location(0) member_2: vec4<f32>,
    @location(5) member_3: vec4<f32>,
    @location(6) member_4: vec4<f32>,
    @location(1) member_5: vec2<f32>,
    @location(2) member_6: vec2<f32>,
    @location(3) member_7: vec2<f32>,
    @location(4) member_8: vec2<f32>,
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
var<private> ibl_N: vec3<f32>;
var<private> in_normal_1: vec3<f32>;
var<private> ibl_V: vec3<f32>;
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
    var s: f32;
    var t: f32;

    if override_type_5_ {
        let _e42 = gl_InstanceIndex_1;
        let _e46 = unnamed.entMat[(_e42 * 2i)];
        local = _e46;
    } else {
        let _e48 = ubo.mvp;
        local = _e48;
    }
    let _e49 = local;
    mvp = _e49;
    let _e50 = mvp;
    let _e51 = in_position_1;
    unnamed_1.gl_Position = (_e50 * vec4<f32>(_e51.x, _e51.y, _e51.z, 1f));
    let _e58 = in_normal_1;
    ibl_N = _e58;
    let _e60 = ubo.eyePos;
    let _e62 = in_position_1;
    ibl_V = normalize((_e60.xyz - _e62));
    let _e65 = in_color0_1;
    frag_color0_ = _e65;
    let _e66 = in_color1_1;
    frag_color1_ = _e66;
    let _e67 = in_color2_1;
    frag_color2_ = _e67;
    let _e68 = in_tex_coord0_1;
    frag_tex_coord0_ = _e68;
    let _e69 = in_tex_coord1_1;
    frag_tex_coord1_ = _e69;
    let _e70 = in_tex_coord2_1;
    frag_tex_coord2_ = _e70;
    let _e71 = in_position_1;
    let _e73 = ubo.fogDistanceVector;
    let _e78 = ubo.fogDistanceVector[3u];
    s = (dot(_e71, _e73.xyz) + _e78);
    let _e80 = in_position_1;
    let _e82 = ubo.fogDepthVector;
    let _e87 = ubo.fogDepthVector[3u];
    t = (dot(_e80, _e82.xyz) + _e87);
    let _e91 = ubo.fogEyeT[1u];
    if (_e91 == 1f) {
        let _e93 = t;
        if (_e93 < 0f) {
            t = 0.03125f;
        } else {
            t = 0.96875f;
        }
    } else {
        let _e95 = t;
        if (_e95 < 1f) {
            t = 0.03125f;
        } else {
            let _e97 = t;
            let _e99 = t;
            let _e102 = ubo.fogEyeT[0u];
            t = (0.03125f + ((0.9375f * _e97) / (_e99 - _e102)));
        }
    }
    let _e106 = s;
    let _e107 = t;
    fog_tex_coord = vec2<f32>(_e106, _e107);
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @location(0) in_position: vec3<f32>, @location(5) in_normal: vec3<f32>, @location(1) in_color0_: vec4<f32>, @location(6) in_color1_: vec4<f32>, @location(7) in_color2_: vec4<f32>, @location(2) in_tex_coord0_: vec2<f32>, @location(3) in_tex_coord1_: vec2<f32>, @location(4) in_tex_coord2_: vec2<f32>) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    in_position_1 = in_position;
    in_normal_1 = in_normal;
    in_color0_1 = in_color0_;
    in_color1_1 = in_color1_;
    in_color2_1 = in_color2_;
    in_tex_coord0_1 = in_tex_coord0_;
    in_tex_coord1_1 = in_tex_coord1_;
    in_tex_coord2_1 = in_tex_coord2_;
    main_1();
    let _e31 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e31);
    let _e33 = unnamed_1.gl_Position;
    let _e34 = ibl_N;
    let _e35 = ibl_V;
    let _e36 = frag_color0_;
    let _e37 = frag_color1_;
    let _e38 = frag_color2_;
    let _e39 = frag_tex_coord0_;
    let _e40 = frag_tex_coord1_;
    let _e41 = frag_tex_coord2_;
    let _e42 = fog_tex_coord;
    return VertexOutput(_e33, _e34, _e35, _e36, _e37, _e38, _e39, _e40, _e41, _e42);
}
