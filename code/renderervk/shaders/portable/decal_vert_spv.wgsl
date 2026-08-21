struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct DecalFrame {
    mvp: mat4x4<f32>,
    timeCount: vec4<f32>,
    invMvp: mat4x4<f32>,
    reconParams: vec4<f32>,
}

struct Decal {
    originRadius: vec4<f32>,
    normalOrient: vec4<f32>,
    rgba: vec4<f32>,
    textureIndex: u32,
    spawnTime: f32,
    lifetimeInv: f32,
    blendMode: u32,
}

struct Pool {
    decals: array<Decal>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec4<f32>,
    @location(2) @interpolate(flat) member_2: u32,
    @location(3) @interpolate(flat) member_3: vec3<f32>,
    @location(4) @interpolate(flat) member_4: vec3<f32>,
    @location(5) @interpolate(flat) member_5: vec3<f32>,
    @location(6) @interpolate(flat) member_6: vec3<f32>,
    @location(7) @interpolate(flat) member_7: f32,
    @location(8) @interpolate(flat) member_8: u32,
}

@id(0) override DECAL_BLEND_MODE: u32 = 0u;
override override_type_17_: bool = (DECAL_BLEND_MODE == 0u);

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;
var<private> fragTextureIndex: u32;
var<private> fragDecalOrigin: vec3<f32>;
var<private> fragDecalTangent: vec3<f32>;
var<private> fragDecalBitangent: vec3<f32>;
var<private> fragDecalNormal: vec3<f32>;
var<private> fragDecalRadius: f32;
var<private> fragNoProject: u32;
var<private> gl_InstanceIndex_1: i32;
var<private> gl_VertexIndex_1: i32;
@group(0) @binding(0) 
var<uniform> unnamed_1: DecalFrame;
@group(0) @binding(1) 
var<storage> unnamed_2: Pool;

fn emitDegenerate_u0028_() {
    unnamed.gl_Position = vec4<f32>(0f, 0f, 0f, 1f);
    fragUV = vec2<f32>(0f, 0f);
    fragColor = vec4<f32>(0f, 0f, 0f, 0f);
    fragTextureIndex = 0u;
    fragDecalOrigin = vec3<f32>(0f, 0f, 0f);
    fragDecalTangent = vec3<f32>(0f, 0f, 0f);
    fragDecalBitangent = vec3<f32>(0f, 0f, 0f);
    fragDecalNormal = vec3<f32>(0f, 0f, 0f);
    fragDecalRadius = 0f;
    fragNoProject = 0u;
    return;
}

fn main_1() {
    var decalIdx: u32;
    var vertInQuad: u32;
    var d: Decal;
    var fadeAlpha: f32;
    var age: f32;
    var frac: f32;
    var origin: vec3<f32>;
    var radius: f32;
    var normal: vec3<f32>;
    var roll: f32;
    var nlen: f32;
    var up: vec3<f32>;
    var tangent: vec3<f32>;
    var bitangent: vec3<f32>;
    var cs: f32;
    var sn: f32;
    var rTangent: vec3<f32>;
    var rBitangent: vec3<f32>;
    var c: vec2<f32>;
    var indexable: array<vec2<f32>, 6>;
    var zbias: f32;
    var worldPos: vec3<f32>;
    var indexable_1: array<vec2<f32>, 6>;
    var phi_110_: bool;

    let _e75 = gl_InstanceIndex_1;
    decalIdx = bitcast<u32>(_e75);
    let _e77 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e77) % 6u);
    let _e80 = decalIdx;
    let _e84 = unnamed_1.timeCount[1u];
    if (f32(_e80) >= _e84) {
        emitDegenerate_u0028_();
        return;
    }
    let _e86 = decalIdx;
    let _e89 = unnamed_2.decals[_e86];
    d.originRadius = _e89.originRadius;
    d.normalOrient = _e89.normalOrient;
    d.rgba = _e89.rgba;
    d.textureIndex = _e89.textureIndex;
    d.spawnTime = _e89.spawnTime;
    d.lifetimeInv = _e89.lifetimeInv;
    d.blendMode = _e89.blendMode;
    let _e105 = d.spawnTime;
    let _e106 = (_e105 == 0f);
    phi_110_ = _e106;
    if _e106 {
        let _e108 = d.lifetimeInv;
        phi_110_ = (_e108 == 0f);
    }
    let _e111 = phi_110_;
    if _e111 {
        emitDegenerate_u0028_();
        return;
    }
    let _e113 = d.blendMode;
    if (_e113 != DECAL_BLEND_MODE) {
        emitDegenerate_u0028_();
        return;
    }
    fadeAlpha = 1f;
    let _e116 = d.lifetimeInv;
    if (_e116 > 0f) {
        let _e120 = unnamed_1.timeCount[0u];
        let _e122 = d.spawnTime;
        age = (_e120 - _e122);
        let _e124 = age;
        let _e126 = d.lifetimeInv;
        frac = (_e124 * _e126);
        let _e128 = frac;
        if (_e128 >= 1f) {
            emitDegenerate_u0028_();
            return;
        }
        let _e130 = frac;
        fadeAlpha = clamp(((1f - _e130) / 0.1f), 0f, 1f);
    }
    let _e135 = d.originRadius;
    origin = _e135.xyz;
    let _e139 = d.originRadius[3u];
    radius = _e139;
    let _e141 = d.normalOrient;
    normal = _e141.xyz;
    let _e145 = d.normalOrient[3u];
    roll = _e145;
    let _e146 = normal;
    nlen = length(_e146);
    let _e148 = nlen;
    if (_e148 < 0.0001f) {
        emitDegenerate_u0028_();
        return;
    }
    let _e150 = nlen;
    let _e151 = normal;
    normal = (_e151 / vec3(_e150));
    up = vec3<f32>(0f, 0f, 1f);
    let _e154 = normal;
    let _e155 = up;
    tangent = cross(_e154, _e155);
    let _e157 = tangent;
    let _e158 = tangent;
    if (dot(_e157, _e158) < 0.000001f) {
        let _e161 = normal;
        tangent = cross(_e161, vec3<f32>(1f, 0f, 0f));
    }
    let _e163 = tangent;
    tangent = normalize(_e163);
    let _e165 = normal;
    let _e166 = tangent;
    bitangent = normalize(cross(_e165, _e166));
    let _e169 = roll;
    cs = cos(_e169);
    let _e171 = roll;
    sn = sin(_e171);
    let _e173 = cs;
    let _e174 = tangent;
    let _e176 = sn;
    let _e177 = bitangent;
    rTangent = ((_e174 * _e173) + (_e177 * _e176));
    let _e180 = sn;
    let _e182 = tangent;
    let _e184 = cs;
    let _e185 = bitangent;
    rBitangent = ((_e182 * -(_e180)) + (_e185 * _e184));
    let _e188 = vertInQuad;
    indexable = array<vec2<f32>, 6>(vec2<f32>(-1f, -1f), vec2<f32>(-1f, 1f), vec2<f32>(1f, -1f), vec2<f32>(1f, -1f), vec2<f32>(-1f, 1f), vec2<f32>(1f, 1f));
    let _e190 = indexable[_e188];
    c = _e190;
    let _e191 = radius;
    zbias = max((_e191 * 0.02f), 0.5f);
    let _e194 = origin;
    let _e195 = rTangent;
    let _e197 = c[0u];
    let _e198 = radius;
    let _e202 = rBitangent;
    let _e204 = c[1u];
    let _e205 = radius;
    let _e209 = normal;
    let _e210 = zbias;
    worldPos = (((_e194 + (_e195 * (_e197 * _e198))) + (_e202 * (_e204 * _e205))) + (_e209 * _e210));
    let _e214 = unnamed_1.mvp;
    let _e215 = worldPos;
    unnamed.gl_Position = (_e214 * vec4<f32>(_e215.x, _e215.y, _e215.z, 1f));
    let _e222 = vertInQuad;
    indexable_1 = array<vec2<f32>, 6>(vec2<f32>(0f, 0f), vec2<f32>(0f, 1f), vec2<f32>(1f, 0f), vec2<f32>(1f, 0f), vec2<f32>(0f, 1f), vec2<f32>(1f, 1f));
    let _e224 = indexable_1[_e222];
    fragUV = _e224;
    let _e226 = d.rgba;
    fragColor = _e226;
    if override_type_17_ {
        let _e227 = fadeAlpha;
        let _e229 = fragColor[3u];
        fragColor[3u] = (_e229 * _e227);
    } else {
        let _e232 = fadeAlpha;
        let _e233 = fragColor;
        let _e235 = (_e233.xyz * _e232);
        fragColor[0u] = _e235.x;
        fragColor[1u] = _e235.y;
        fragColor[2u] = _e235.z;
    }
    let _e243 = d.textureIndex;
    fragTextureIndex = (_e243 & 2147483647u);
    let _e246 = d.textureIndex;
    fragNoProject = ((_e246 >> bitcast<u32>(31u)) & 1u);
    let _e250 = origin;
    fragDecalOrigin = _e250;
    let _e251 = rTangent;
    fragDecalTangent = _e251;
    let _e252 = rBitangent;
    fragDecalBitangent = _e252;
    let _e253 = normal;
    fragDecalNormal = _e253;
    let _e254 = radius;
    fragDecalRadius = _e254;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e18 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e18);
    let _e20 = unnamed.gl_Position;
    let _e21 = fragUV;
    let _e22 = fragColor;
    let _e23 = fragTextureIndex;
    let _e24 = fragDecalOrigin;
    let _e25 = fragDecalTangent;
    let _e26 = fragDecalBitangent;
    let _e27 = fragDecalNormal;
    let _e28 = fragDecalRadius;
    let _e29 = fragNoProject;
    return VertexOutput(_e20, _e21, _e22, _e23, _e24, _e25, _e26, _e27, _e28, _e29);
}
