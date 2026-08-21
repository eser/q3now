struct RibbonHeader {
    pointOffset: u32,
    pointCount: u32,
    shaderHandle: u32,
    flags: u32,
    uvScroll: vec2<f32>,
}

struct Headers {
    headers: array<RibbonHeader>,
}

struct RibbonPoint {
    posW: vec4<f32>,
    rgba: vec4<f32>,
    normal: vec4<f32>,
}

struct Points {
    points: array<RibbonPoint>,
}

struct EffectsUBO {
    mvp: mat4x4<f32>,
    eyeWorld: vec4<f32>,
    frameParams: vec4<f32>,
    _v2_: vec4<f32>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec4<f32>,
    @location(2) @interpolate(flat) member_2: u32,
}

@group(0) @binding(1) 
var<storage> unnamed: Headers;
var<private> gl_InstanceIndex_1: i32;
var<private> gl_VertexIndex_1: i32;
@group(0) @binding(0) 
var<storage> unnamed_1: Points;
@group(1) @binding(0) 
var<uniform> unnamed_2: EffectsUBO;
var<private> unnamed_3: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;
var<private> fragShaderHandle: u32;

fn main_1() {
    var hdr: RibbonHeader;
    var vertInQuad: u32;
    var segIdx: u32;
    var pi: u32;
    var indexable: array<u32, 6>;
    var sgn: f32;
    var indexable_1: array<f32, 6>;
    var p: RibbonPoint;
    var halfW: f32;
    var normal: vec3<f32>;
    var a: vec3<f32>;
    var b: vec3<f32>;
    var segAxis: vec3<f32>;
    var wantUp: bool;
    var ref_: vec3<f32>;
    var local: vec3<f32>;
    var ad: vec3<f32>;
    var fb: vec3<f32>;
    var local_1: vec3<f32>;
    var worldPos: vec3<f32>;
    var denom: f32;
    var local_2: f32;
    var baseUV: vec2<f32>;
    var phi_151_: bool;
    var phi_197_: bool;

    let _e53 = gl_InstanceIndex_1;
    let _e56 = unnamed.headers[_e53];
    hdr.pointOffset = _e56.pointOffset;
    hdr.pointCount = _e56.pointCount;
    hdr.shaderHandle = _e56.shaderHandle;
    hdr.flags = _e56.flags;
    hdr.uvScroll = _e56.uvScroll;
    let _e67 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e67) % 6u);
    let _e70 = gl_VertexIndex_1;
    segIdx = (bitcast<u32>(_e70) / 6u);
    let _e74 = hdr.pointOffset;
    let _e75 = segIdx;
    let _e77 = vertInQuad;
    indexable = array<u32, 6>(0u, 1u, 0u, 0u, 1u, 1u);
    let _e79 = indexable[_e77];
    pi = ((_e74 + _e75) + _e79);
    let _e81 = vertInQuad;
    indexable_1 = array<f32, 6>(-1f, -1f, 1f, 1f, -1f, 1f);
    let _e83 = indexable_1[_e81];
    sgn = _e83;
    let _e84 = pi;
    let _e87 = unnamed_1.points[_e84];
    p.posW = _e87.posW;
    p.rgba = _e87.rgba;
    p.normal = _e87.normal;
    let _e96 = p.posW[3u];
    halfW = _e96;
    let _e98 = hdr.flags;
    if ((_e98 & 16u) != 0u) {
        let _e102 = p.normal;
        normal = _e102.xyz;
    } else {
        let _e105 = hdr.pointOffset;
        let _e106 = segIdx;
        let _e111 = unnamed_1.points[(_e105 + _e106)].posW;
        a = _e111.xyz;
        let _e114 = hdr.pointOffset;
        let _e115 = segIdx;
        let _e121 = unnamed_1.points[((_e114 + _e115) + 1u)].posW;
        b = _e121.xyz;
        let _e123 = b;
        let _e124 = a;
        segAxis = (_e123 - _e124);
        let _e127 = hdr.flags;
        let _e129 = ((_e127 & 8u) != 0u);
        phi_151_ = _e129;
        if _e129 {
            let _e131 = hdr.flags;
            phi_151_ = ((_e131 & 1u) == 0u);
        }
        let _e135 = phi_151_;
        wantUp = _e135;
        let _e136 = wantUp;
        if _e136 {
            local = vec3<f32>(0f, 0f, 1f);
        } else {
            let _e137 = a;
            let _e139 = unnamed_2.eyeWorld;
            local = (_e137 - _e139.xyz);
        }
        let _e142 = local;
        ref_ = _e142;
        let _e143 = segAxis;
        let _e144 = ref_;
        normal = cross(_e143, _e144);
        let _e146 = normal;
        let _e147 = normal;
        if (dot(_e146, _e147) < 0.000000000001f) {
            let _e150 = segAxis;
            ad = abs(_e150);
            let _e153 = ad[0u];
            let _e155 = ad[1u];
            let _e156 = (_e153 <= _e155);
            phi_197_ = _e156;
            if _e156 {
                let _e158 = ad[0u];
                let _e160 = ad[2u];
                phi_197_ = (_e158 <= _e160);
            }
            let _e163 = phi_197_;
            if _e163 {
                local_1 = vec3<f32>(1f, 0f, 0f);
            } else {
                let _e165 = ad[1u];
                let _e167 = ad[2u];
                local_1 = select(vec3<f32>(0f, 0f, 1f), vec3<f32>(0f, 1f, 0f), vec3((_e165 <= _e167)));
            }
            let _e171 = local_1;
            fb = _e171;
            let _e172 = segAxis;
            let _e173 = fb;
            normal = cross(_e172, _e173);
        }
        let _e175 = normal;
        normal = normalize(_e175);
    }
    let _e178 = p.posW;
    let _e180 = normal;
    let _e181 = halfW;
    let _e182 = sgn;
    worldPos = (_e178.xyz + (_e180 * (_e181 * _e182)));
    let _e187 = unnamed_2.mvp;
    let _e188 = worldPos;
    unnamed_3.gl_Position = (_e187 * vec4<f32>(_e188.x, _e188.y, _e188.z, 1f));
    let _e196 = hdr.pointCount;
    if (_e196 > 1u) {
        let _e199 = hdr.pointCount;
        local_2 = f32((_e199 - 1u));
    } else {
        local_2 = 1f;
    }
    let _e202 = local_2;
    denom = _e202;
    let _e203 = pi;
    let _e205 = hdr.pointOffset;
    let _e208 = denom;
    let _e210 = sgn;
    baseUV = vec2<f32>((f32((_e203 - _e205)) / _e208), select(0f, 1f, (_e210 > 0f)));
    let _e214 = baseUV;
    let _e216 = hdr.uvScroll;
    let _e219 = unnamed_2.frameParams[1u];
    fragUV = fract((_e214 + (_e216 * _e219)));
    let _e224 = p.rgba;
    fragColor = _e224;
    let _e226 = hdr.shaderHandle;
    fragShaderHandle = _e226;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e12 = unnamed_3.gl_Position.y;
    unnamed_3.gl_Position.y = -(_e12);
    let _e14 = unnamed_3.gl_Position;
    let _e15 = fragUV;
    let _e16 = fragColor;
    let _e17 = fragShaderHandle;
    return VertexOutput(_e14, _e15, _e16, _e17);
}
