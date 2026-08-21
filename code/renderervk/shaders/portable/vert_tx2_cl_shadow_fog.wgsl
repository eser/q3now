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
    @location(5) member_2: vec4<f32>,
    @location(6) member_3: vec4<f32>,
    @location(1) member_4: vec2<f32>,
    @location(2) member_5: vec2<f32>,
    @location(3) member_6: vec2<f32>,
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
var<private> shadowData: vec4<f32>;
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
        let _e40 = gl_InstanceIndex_1;
        let _e44 = unnamed.entMat[(_e40 * 2i)];
        local = _e44;
    } else {
        let _e46 = ubo.mvp;
        local = _e46;
    }
    let _e47 = local;
    mvp = _e47;
    let _e48 = mvp;
    let _e49 = in_position_1;
    unnamed_1.gl_Position = (_e48 * vec4<f32>(_e49.x, _e49.y, _e49.z, 1f));
    let _e56 = in_position_1;
    let _e59 = unnamed_1.gl_Position[3u];
    shadowData = vec4<f32>(_e56.x, _e56.y, _e56.z, _e59);
    let _e64 = in_color0_1;
    frag_color0_ = _e64;
    let _e65 = in_color1_1;
    frag_color1_ = _e65;
    let _e66 = in_color2_1;
    frag_color2_ = _e66;
    let _e67 = in_tex_coord0_1;
    frag_tex_coord0_ = _e67;
    let _e68 = in_tex_coord1_1;
    frag_tex_coord1_ = _e68;
    let _e69 = in_tex_coord2_1;
    frag_tex_coord2_ = _e69;
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
    let _e28 = unnamed_1.gl_Position.y;
    unnamed_1.gl_Position.y = -(_e28);
    let _e30 = unnamed_1.gl_Position;
    let _e31 = shadowData;
    let _e32 = frag_color0_;
    let _e33 = frag_color1_;
    let _e34 = frag_color2_;
    let _e35 = frag_tex_coord0_;
    let _e36 = frag_tex_coord1_;
    let _e37 = frag_tex_coord2_;
    let _e38 = fog_tex_coord;
    return VertexOutput(_e30, _e31, _e32, _e33, _e34, _e35, _e36, _e37, _e38);
}
