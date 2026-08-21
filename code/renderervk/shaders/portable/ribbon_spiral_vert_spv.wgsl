struct RailRibbonHeader {
    startBeamLen: vec4<f32>,
    beamAxisAge: vec4<f32>,
    colorDuration: vec4<f32>,
    misc: vec4<f32>,
    perpAxis: array<vec4<f32>, 36>,
}

struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
}

struct Headers {
    headers: array<RailRibbonHeader>,
}

struct EffectsUBO {
    mvp: mat4x4<f32>,
    eyeWorld: vec4<f32>,
    frameParams: vec4<f32>,
    _v2_: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec2<f32>,
    @location(1) member_1: vec4<f32>,
    @location(2) @interpolate(flat) member_2: u32,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f));
var<private> fragUV: vec2<f32>;
var<private> fragColor: vec4<f32>;
var<private> fragShaderHandle: u32;
@group(0) @binding(0) 
var<storage> unnamed_1: Headers;
var<private> gl_InstanceIndex_1: i32;
var<private> gl_VertexIndex_1: i32;
@group(1) @binding(0) 
var<uniform> unnamed_2: EffectsUBO;

fn helixPoint_u0028_i1_u003b_i1_u003b_vf3_u003b_vf3_u003b_f1_u003b_f1_u003b_f1_u003b_f1_u003b_f1_u003b_f1_u003b_struct_u002d_RailRibbonHeader_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u005b_36_u005d_1_u003b_vf3_u003b_vf3_u003b_f1_u003b_f1_u003b(j: ptr<function, i32>, numSegs: ptr<function, i32>, start: ptr<function, vec3<f32>>, beamAxis: ptr<function, vec3<f32>>, beamLen: ptr<function, f32>, curRadius: ptr<function, f32>, curSpacing: ptr<function, f32>, curWidth: ptr<function, f32>, frac: ptr<function, f32>, trailAlpha: ptr<function, f32>, hdr: ptr<function, RailRibbonHeader>, pos: ptr<function, vec3<f32>>, normal: ptr<function, vec3<f32>>, halfW: ptr<function, f32>, alpha: ptr<function, f32>) -> bool {
    var d: f32;
    var ring: i32;
    var perp: vec3<f32>;
    var segPos: f32;
    var unwindFade: f32;

    let _e86 = (*j);
    let _e87 = (*numSegs);
    if (_e86 > (_e87 - 1i)) {
        return false;
    }
    let _e90 = (*beamLen);
    let _e91 = (*j);
    let _e93 = (*curSpacing);
    d = (_e90 - (f32(_e91) * _e93));
    let _e96 = d;
    if (_e96 < 0f) {
        return false;
    }
    let _e98 = (*j);
    let _e99 = (_e98 * 2i);
    ring = (_e99 - (i32(floor((f32(_e99) / f32(36i)))) * 36i));
    let _e107 = ring;
    if (_e107 < 0i) {
        let _e109 = ring;
        ring = (_e109 + 36i);
    }
    let _e111 = ring;
    let _e114 = (*hdr).perpAxis[_e111];
    perp = _e114.xyz;
    let _e116 = (*start);
    let _e117 = d;
    let _e118 = (*beamAxis);
    let _e121 = (*curRadius);
    let _e122 = perp;
    (*pos) = ((_e116 + (_e118 * _e117)) + (_e122 * _e121));
    let _e125 = (*beamAxis);
    let _e126 = perp;
    (*normal) = normalize(cross(_e125, _e126));
    let _e129 = (*curWidth);
    (*halfW) = _e129;
    let _e130 = (*j);
    let _e132 = (*numSegs);
    segPos = (f32(_e130) / f32((_e132 - 1i)));
    let _e136 = (*frac);
    let _e137 = segPos;
    unwindFade = (1f - (_e136 * (1f + _e137)));
    let _e141 = unwindFade;
    if (_e141 < 0f) {
        unwindFade = 0f;
    }
    let _e143 = (*trailAlpha);
    let _e144 = unwindFade;
    (*alpha) = (_e143 * _e144);
    return true;
}

fn emitDegenerate_u0028_() {
    unnamed.gl_Position = vec4<f32>(0f, 0f, -2f, 1f);
    fragUV = vec2<f32>(0f, 0f);
    fragColor = vec4<f32>(0f, 0f, 0f, 0f);
    fragShaderHandle = 0u;
    return;
}

fn main_1() {
    var hdr_1: RailRibbonHeader;
    var vertInQuad: u32;
    var segIdx: i32;
    var start_1: vec3<f32>;
    var beamLen_1: f32;
    var beamAxis_1: vec3<f32>;
    var age: f32;
    var duration: f32;
    var baseAlpha: f32;
    var frac_1: f32;
    var local: f32;
    var trailAlpha_1: f32;
    var easedFrac: f32;
    var curRadius_1: f32;
    var curSpacing_1: f32;
    var curWidth_1: f32;
    var numSegs_1: i32;
    var pi: i32;
    var indexable: array<u32, 6>;
    var sgn: f32;
    var indexable_1: array<f32, 6>;
    var pos_1: vec3<f32>;
    var normal_1: vec3<f32>;
    var halfW_1: f32;
    var alpha_1: f32;
    var param: i32;
    var param_1: i32;
    var param_2: vec3<f32>;
    var param_3: vec3<f32>;
    var param_4: f32;
    var param_5: f32;
    var param_6: f32;
    var param_7: f32;
    var param_8: f32;
    var param_9: f32;
    var param_10: RailRibbonHeader;
    var param_11: vec3<f32>;
    var param_12: vec3<f32>;
    var param_13: f32;
    var param_14: f32;
    var worldPos: vec3<f32>;
    var denom: f32;
    var local_1: f32;

    let _e109 = gl_InstanceIndex_1;
    let _e112 = unnamed_1.headers[_e109];
    hdr_1.startBeamLen = _e112.startBeamLen;
    hdr_1.beamAxisAge = _e112.beamAxisAge;
    hdr_1.colorDuration = _e112.colorDuration;
    hdr_1.misc = _e112.misc;
    hdr_1.perpAxis[0i] = _e112.perpAxis[0];
    hdr_1.perpAxis[1i] = _e112.perpAxis[1];
    hdr_1.perpAxis[2i] = _e112.perpAxis[2];
    hdr_1.perpAxis[3i] = _e112.perpAxis[3];
    hdr_1.perpAxis[4i] = _e112.perpAxis[4];
    hdr_1.perpAxis[5i] = _e112.perpAxis[5];
    hdr_1.perpAxis[6i] = _e112.perpAxis[6];
    hdr_1.perpAxis[7i] = _e112.perpAxis[7];
    hdr_1.perpAxis[8i] = _e112.perpAxis[8];
    hdr_1.perpAxis[9i] = _e112.perpAxis[9];
    hdr_1.perpAxis[10i] = _e112.perpAxis[10];
    hdr_1.perpAxis[11i] = _e112.perpAxis[11];
    hdr_1.perpAxis[12i] = _e112.perpAxis[12];
    hdr_1.perpAxis[13i] = _e112.perpAxis[13];
    hdr_1.perpAxis[14i] = _e112.perpAxis[14];
    hdr_1.perpAxis[15i] = _e112.perpAxis[15];
    hdr_1.perpAxis[16i] = _e112.perpAxis[16];
    hdr_1.perpAxis[17i] = _e112.perpAxis[17];
    hdr_1.perpAxis[18i] = _e112.perpAxis[18];
    hdr_1.perpAxis[19i] = _e112.perpAxis[19];
    hdr_1.perpAxis[20i] = _e112.perpAxis[20];
    hdr_1.perpAxis[21i] = _e112.perpAxis[21];
    hdr_1.perpAxis[22i] = _e112.perpAxis[22];
    hdr_1.perpAxis[23i] = _e112.perpAxis[23];
    hdr_1.perpAxis[24i] = _e112.perpAxis[24];
    hdr_1.perpAxis[25i] = _e112.perpAxis[25];
    hdr_1.perpAxis[26i] = _e112.perpAxis[26];
    hdr_1.perpAxis[27i] = _e112.perpAxis[27];
    hdr_1.perpAxis[28i] = _e112.perpAxis[28];
    hdr_1.perpAxis[29i] = _e112.perpAxis[29];
    hdr_1.perpAxis[30i] = _e112.perpAxis[30];
    hdr_1.perpAxis[31i] = _e112.perpAxis[31];
    hdr_1.perpAxis[32i] = _e112.perpAxis[32];
    hdr_1.perpAxis[33i] = _e112.perpAxis[33];
    hdr_1.perpAxis[34i] = _e112.perpAxis[34];
    hdr_1.perpAxis[35i] = _e112.perpAxis[35];
    let _e195 = gl_VertexIndex_1;
    vertInQuad = (bitcast<u32>(_e195) % 6u);
    let _e198 = gl_VertexIndex_1;
    segIdx = bitcast<i32>((bitcast<u32>(_e198) / 6u));
    let _e203 = hdr_1.startBeamLen;
    start_1 = _e203.xyz;
    let _e207 = hdr_1.startBeamLen[3u];
    beamLen_1 = _e207;
    let _e209 = hdr_1.beamAxisAge;
    beamAxis_1 = _e209.xyz;
    let _e213 = hdr_1.beamAxisAge[3u];
    age = _e213;
    let _e216 = hdr_1.colorDuration[3u];
    duration = _e216;
    let _e219 = hdr_1.misc[0u];
    baseAlpha = _e219;
    let _e220 = duration;
    if (_e220 > 0f) {
        let _e222 = age;
        let _e223 = duration;
        local = (_e222 / _e223);
    } else {
        local = 1f;
    }
    let _e225 = local;
    frac_1 = _e225;
    let _e226 = frac_1;
    if (_e226 < 0f) {
        frac_1 = 0f;
    }
    let _e228 = frac_1;
    if (_e228 > 1f) {
        frac_1 = 1f;
    }
    let _e230 = frac_1;
    trailAlpha_1 = (1f - _e230);
    let _e232 = frac_1;
    let _e234 = frac_1;
    easedFrac = (1f - ((1f - _e232) * (1f - _e234)));
    let _e238 = easedFrac;
    curRadius_1 = (2f + (_e238 * 2f));
    let _e241 = easedFrac;
    curSpacing_1 = (3f * (1f - (_e241 * 0.667f)));
    let _e245 = easedFrac;
    curWidth_1 = (1.5f * (1f + (_e245 * 1.5f)));
    let _e249 = beamLen_1;
    let _e250 = curSpacing_1;
    numSegs_1 = i32((_e249 / _e250));
    let _e253 = numSegs_1;
    if (_e253 > 2048i) {
        numSegs_1 = 2048i;
    }
    let _e255 = numSegs_1;
    if (_e255 < 2i) {
        numSegs_1 = 2i;
    }
    let _e257 = segIdx;
    let _e258 = numSegs_1;
    if (_e257 > (_e258 - 2i)) {
        emitDegenerate_u0028_();
        return;
    }
    let _e261 = segIdx;
    let _e262 = vertInQuad;
    indexable = array<u32, 6>(0u, 1u, 0u, 0u, 1u, 1u);
    let _e264 = indexable[_e262];
    pi = (_e261 + bitcast<i32>(_e264));
    let _e267 = vertInQuad;
    indexable_1 = array<f32, 6>(-1f, -1f, 1f, 1f, -1f, 1f);
    let _e269 = indexable_1[_e267];
    sgn = _e269;
    let _e270 = pi;
    param = _e270;
    let _e271 = numSegs_1;
    param_1 = _e271;
    let _e272 = start_1;
    param_2 = _e272;
    let _e273 = beamAxis_1;
    param_3 = _e273;
    let _e274 = beamLen_1;
    param_4 = _e274;
    let _e275 = curRadius_1;
    param_5 = _e275;
    let _e276 = curSpacing_1;
    param_6 = _e276;
    let _e277 = curWidth_1;
    param_7 = _e277;
    let _e278 = frac_1;
    param_8 = _e278;
    let _e279 = trailAlpha_1;
    param_9 = _e279;
    let _e280 = hdr_1;
    param_10 = _e280;
    let _e281 = helixPoint_u0028_i1_u003b_i1_u003b_vf3_u003b_vf3_u003b_f1_u003b_f1_u003b_f1_u003b_f1_u003b_f1_u003b_f1_u003b_struct_u002d_RailRibbonHeader_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u002d_vf4_u005b_36_u005d_1_u003b_vf3_u003b_vf3_u003b_f1_u003b_f1_u003b((&param), (&param_1), (&param_2), (&param_3), (&param_4), (&param_5), (&param_6), (&param_7), (&param_8), (&param_9), (&param_10), (&param_11), (&param_12), (&param_13), (&param_14));
    let _e282 = param_11;
    pos_1 = _e282;
    let _e283 = param_12;
    normal_1 = _e283;
    let _e284 = param_13;
    halfW_1 = _e284;
    let _e285 = param_14;
    alpha_1 = _e285;
    if !(_e281) {
        emitDegenerate_u0028_();
        return;
    }
    let _e287 = pos_1;
    let _e288 = normal_1;
    let _e289 = halfW_1;
    let _e290 = sgn;
    worldPos = (_e287 + (_e288 * (_e289 * _e290)));
    let _e295 = unnamed_2.mvp;
    let _e296 = worldPos;
    unnamed.gl_Position = (_e295 * vec4<f32>(_e296.x, _e296.y, _e296.z, 1f));
    let _e303 = numSegs_1;
    if (_e303 > 1i) {
        let _e305 = numSegs_1;
        local_1 = f32((_e305 - 1i));
    } else {
        local_1 = 1f;
    }
    let _e308 = local_1;
    denom = _e308;
    let _e309 = pi;
    let _e311 = denom;
    let _e313 = sgn;
    fragUV = vec2<f32>((f32(_e309) / _e311), select(0f, 1f, (_e313 > 0f)));
    let _e318 = hdr_1.colorDuration;
    let _e319 = _e318.xyz;
    let _e320 = baseAlpha;
    let _e321 = alpha_1;
    fragColor = vec4<f32>(_e319.x, _e319.y, _e319.z, (_e320 * _e321));
    fragShaderHandle = 0u;
    return;
}

@vertex 
fn main(@builtin(instance_index) gl_InstanceIndex: u32, @builtin(vertex_index) gl_VertexIndex: u32) -> VertexOutput {
    gl_InstanceIndex_1 = i32(gl_InstanceIndex);
    gl_VertexIndex_1 = i32(gl_VertexIndex);
    main_1();
    let _e12 = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e12);
    let _e14 = unnamed.gl_Position;
    let _e15 = fragUV;
    let _e16 = fragColor;
    let _e17 = fragShaderHandle;
    return VertexOutput(_e14, _e15, _e16, _e17);
}
