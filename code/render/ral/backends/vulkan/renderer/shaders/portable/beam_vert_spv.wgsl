struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct BeamHeader {
    start: vec4<f32>,
    end: vec4<f32>,
    startColor: vec4<f32>,
    endColor: vec4<f32>,
    uvScroll: vec2<f32>,
    startWidth: f32,
    endWidth: f32,
    spawnTime: f32,
    shaderHandle: u32,
    axialCopies: u32,
    flags: u32,
}

struct Headers {
    beams: array<BeamHeader>,
}

struct EffectsUBO {
    mvp: mat4x4<f32>,
    eyeWorld: vec4<f32>,
    frameParams: vec4<f32>,
    stageParams: vec4<f32>,
}

struct StageCounts {
    stageCounts: array<u32>,
}

struct PrimitiveStageGPU {
    imageSlot: u32,
    blendPacked: u32,
    rgbGen: u32,
    alphaGen: u32,
    uvScale: vec2<f32>,
    uvScroll: vec2<f32>,
}

struct Stages {
    stages: array<PrimitiveStageGPU>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec4<f32>,
    @location(2) @interpolate(flat) member_2: u32,
    @location(3) @interpolate(flat) member_3: u32,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;
var<private> fragShaderHandle: u32;
var<private> fragImageSlot: u32;
var<private> gl_InstanceIndex_1: i32;
var<private> gl_VertexIndex_1: i32;
@group(0) @binding(0)
var<storage> unnamed_1: Headers;
@group(1) @binding(0)
var<uniform> unnamed_2: EffectsUBO;
@group(0) @binding(3)
var<storage> unnamed_3: StageCounts;
@group(0) @binding(2)
var<storage> unnamed_4: Stages;

fn emitDegenerate_u0028_() {
    unnamed.gl_Position = vec4<f32>(0f, 0f, 2f, 1f);
    fragUV = vec2<f32>(0f, 0f);
    fragColor = vec4<f32>(0f, 0f, 0f, 0f);
    fragShaderHandle = 0u;
    fragImageSlot = 0u;
    return;
}

fn main_1() {
    var beamIdx: u32;
    var vertexIdx: u32;
    var copyIdx: u32;
    var localIdx: u32;
    var hdr: BeamHeader;
    var stageIdx: u32;
    var stageCount: u32;
    var stage: PrimitiveStageGPU;
    var startW: vec3<f32>;
    var endW: vec3<f32>;
    var axis: vec3<f32>;
    var len: f32;
    var axisN: vec3<f32>;
    var center: vec3<f32>;
    var toEye: vec3<f32>;
    var sideRaw: vec3<f32>;
    var sideLen: f32;
    var ad: vec3<f32>;
    var fb: vec3<f32>;
    var local: vec3<f32>;
    var side: vec3<f32>;
    var angle: f32;
    var c: f32;
    var s: f32;
    var rotSide: vec3<f32>;
    var v0_: vec3<f32>;
    var v1_: vec3<f32>;
    var v2_: vec3<f32>;
    var v3_: vec3<f32>;
    var worldPos: vec3<f32>;
    var vertColor: vec4<f32>;
    var uv: vec2<f32>;
    var scrollT: f32;
    var baseUV: vec2<f32>;
    var totalScroll: vec2<f32>;
    var phi_231_: bool;

    let _e78 = gl_InstanceIndex_1;
    beamIdx = bitcast<u32>(_e78);
    let _e80 = gl_VertexIndex_1;
    vertexIdx = bitcast<u32>(_e80);
    let _e82 = vertexIdx;
    copyIdx = (_e82 / 6u);
    let _e84 = vertexIdx;
    localIdx = (_e84 % 6u);
    let _e86 = beamIdx;
    let _e89 = unnamed_1.beams[_e86];
    hdr.start = _e89.start;
    hdr.end = _e89.end;
    hdr.startColor = _e89.startColor;
    hdr.endColor = _e89.endColor;
    hdr.uvScroll = _e89.uvScroll;
    hdr.startWidth = _e89.startWidth;
    hdr.endWidth = _e89.endWidth;
    hdr.spawnTime = _e89.spawnTime;
    hdr.shaderHandle = _e89.shaderHandle;
    hdr.axialCopies = _e89.axialCopies;
    hdr.flags = _e89.flags;
    let _e114 = unnamed_2.stageParams[0u];
    stageIdx = u32(_e114);
    let _e117 = hdr.shaderHandle;
    let _e120 = unnamed_3.stageCounts[_e117];
    stageCount = _e120;
    let _e121 = stageIdx;
    let _e122 = stageCount;
    if (_e121 >= _e122) {
        emitDegenerate_u0028_();
        return;
    }
    let _e125 = hdr.shaderHandle;
    let _e127 = stageIdx;
    let _e131 = unnamed_4.stages[((_e125 * 4u) + _e127)];
    stage.imageSlot = _e131.imageSlot;
    stage.blendPacked = _e131.blendPacked;
    stage.rgbGen = _e131.rgbGen;
    stage.alphaGen = _e131.alphaGen;
    stage.uvScale = _e131.uvScale;
    stage.uvScroll = _e131.uvScroll;
    let _e144 = copyIdx;
    let _e146 = hdr.axialCopies;
    if (_e144 >= _e146) {
        emitDegenerate_u0028_();
        return;
    }
    let _e149 = hdr.start;
    startW = _e149.xyz;
    let _e152 = hdr.end;
    endW = _e152.xyz;
    let _e154 = endW;
    let _e155 = startW;
    axis = (_e154 - _e155);
    let _e157 = axis;
    len = length(_e157);
    let _e159 = len;
    if (_e159 < 0.0001f) {
        emitDegenerate_u0028_();
        return;
    }
    let _e161 = axis;
    let _e162 = len;
    axisN = (_e161 / vec3(_e162));
    let _e165 = startW;
    let _e166 = endW;
    center = ((_e165 + _e166) * 0.5f);
    let _e170 = unnamed_2.eyeWorld;
    let _e172 = center;
    toEye = (_e170.xyz - _e172);
    let _e174 = axisN;
    let _e175 = toEye;
    sideRaw = cross(_e174, _e175);
    let _e177 = sideRaw;
    sideLen = length(_e177);
    let _e179 = sideLen;
    if (_e179 < 0.0001f) {
        let _e181 = axisN;
        ad = abs(_e181);
        let _e184 = ad[0u];
        let _e186 = ad[1u];
        let _e187 = (_e184 <= _e186);
        phi_231_ = _e187;
        if _e187 {
            let _e189 = ad[0u];
            let _e191 = ad[2u];
            phi_231_ = (_e189 <= _e191);
        }
        let _e194 = phi_231_;
        if _e194 {
            local = vec3<f32>(1f, 0f, 0f);
        } else {
            let _e196 = ad[1u];
            let _e198 = ad[2u];
            local = select(vec3<f32>(0f, 0f, 1f), vec3<f32>(0f, 1f, 0f), vec3((_e196 <= _e198)));
        }
        let _e202 = local;
        fb = _e202;
        let _e203 = axisN;
        let _e204 = fb;
        side = normalize(cross(_e203, _e204));
    } else {
        let _e207 = sideRaw;
        let _e208 = sideLen;
        side = (_e207 / vec3(_e208));
    }
    let _e211 = copyIdx;
    let _e215 = hdr.axialCopies;
    angle = ((f32(_e211) * 3.1415927f) / f32(_e215));
    let _e218 = angle;
    c = cos(_e218);
    let _e220 = angle;
    s = sin(_e220);
    let _e222 = side;
    let _e223 = c;
    let _e225 = axisN;
    let _e226 = side;
    let _e228 = s;
    let _e231 = axisN;
    let _e232 = axisN;
    let _e233 = side;
    let _e236 = c;
    rotSide = (((_e222 * _e223) + (cross(_e225, _e226) * _e228)) + ((_e231 * dot(_e232, _e233)) * (1f - _e236)));
    let _e240 = startW;
    let _e241 = rotSide;
    let _e243 = hdr.startWidth;
    v0_ = (_e240 - (_e241 * _e243));
    let _e246 = startW;
    let _e247 = rotSide;
    let _e249 = hdr.startWidth;
    v1_ = (_e246 + (_e247 * _e249));
    let _e252 = endW;
    let _e253 = rotSide;
    let _e255 = hdr.endWidth;
    v2_ = (_e252 - (_e253 * _e255));
    let _e258 = endW;
    let _e259 = rotSide;
    let _e261 = hdr.endWidth;
    v3_ = (_e258 + (_e259 * _e261));
    let _e264 = localIdx;
    switch bitcast<i32>(_e264) {
        case 0: {
            let _e269 = v0_;
            worldPos = _e269;
            let _e271 = hdr.startColor;
            vertColor = _e271;
            uv = vec2<f32>(0f, 0f);
            break;
        }
        case 1: {
            let _e272 = v1_;
            worldPos = _e272;
            let _e274 = hdr.startColor;
            vertColor = _e274;
            uv = vec2<f32>(0f, 1f);
            break;
        }
        case 2: {
            let _e275 = v2_;
            worldPos = _e275;
            let _e277 = hdr.endColor;
            vertColor = _e277;
            uv = vec2<f32>(1f, 0f);
            break;
        }
        case 3: {
            let _e278 = v1_;
            worldPos = _e278;
            let _e280 = hdr.startColor;
            vertColor = _e280;
            uv = vec2<f32>(0f, 1f);
            break;
        }
        case 4: {
            let _e281 = v3_;
            worldPos = _e281;
            let _e283 = hdr.endColor;
            vertColor = _e283;
            uv = vec2<f32>(1f, 1f);
            break;
        }
        default: {
            let _e266 = v2_;
            worldPos = _e266;
            let _e268 = hdr.endColor;
            vertColor = _e268;
            uv = vec2<f32>(1f, 0f);
            break;
        }
    }
    let _e285 = unnamed_2.mvp;
    let _e286 = worldPos;
    unnamed.gl_Position = (_e285 * vec4<f32>(_e286.x, _e286.y, _e286.z, 1f));
    let _e294 = hdr.flags;
    if ((_e294 & 32u) != 0u) {
        let _e299 = unnamed_2.frameParams[1u];
        scrollT = _e299;
    } else {
        let _e302 = unnamed_2.frameParams[1u];
        let _e304 = hdr.spawnTime;
        scrollT = max((_e302 - _e304), 0f);
    }
    let _e307 = uv;
    let _e309 = stage.uvScale;
    baseUV = (_e307 * _e309);
    let _e312 = stage.uvScroll;
    let _e314 = hdr.uvScroll;
    totalScroll = (_e312 + _e314);
    let _e316 = baseUV;
    let _e317 = totalScroll;
    let _e318 = scrollT;
    fragUV = (_e316 + (_e317 * _e318));
    let _e321 = vertColor;
    fragColor = _e321;
    let _e323 = hdr.shaderHandle;
    fragShaderHandle = _e323;
    let _e325 = stage.imageSlot;
    fragImageSlot = _e325;
    return;
}

@vertex
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e13 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e13);
    let _e15 = unnamed.gl_Position;
    let _e16 = fragUV;
    let _e17 = fragColor;
    let _e18 = fragShaderHandle;
    let _e19 = fragImageSlot;
    return VertexOutput(_e15, _e16, _e17, _e18, _e19);
}
