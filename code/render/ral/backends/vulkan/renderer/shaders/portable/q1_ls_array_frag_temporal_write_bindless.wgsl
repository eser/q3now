enable wgpu_binding_array;

struct UBO {
    _eyePos: vec4<f32>,
    _animPad: vec4<f32>,
    _pad1_: vec4<f32>,
    _pad2_: vec4<f32>,
    _fogDist: vec4<f32>,
    _fogDepth: vec4<f32>,
    _fogEyeT: vec4<f32>,
    _fogColor: vec4<f32>,
    q1StyleIntensities: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 25>,
    packed_indices: array<vec4<u32>, 3>,
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: vec4<f32>,
    @location(0) member_2: vec4<f32>,
}

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
@group(1) @binding(2)
var wired_bindless_image_arrays: binding_array<texture_2d_array<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_60_: bool;

    let _e46 = (*value);
    let _e47 = (*value);
    let _e49 = all((_e46 == _e47));
    phi_60_ = _e49;
    if _e49 {
        let _e50 = (*value);
        phi_60_ = all((abs(_e50) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e55 = phi_60_;
    return _e55;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_45_: bool;

    let _e46 = (*value_1);
    let _e47 = (*value_1);
    let _e49 = all((_e46 == _e47));
    phi_45_ = _e49;
    if _e49 {
        let _e50 = (*value_1);
        phi_45_ = all((abs(_e50) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e55 = phi_45_;
    return _e55;
}

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_85_: bool;
    var phi_94_: bool;
    var phi_104_: bool;
    var phi_111_: bool;
    var phi_140_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e57 = temporalOutcome_1;
    let _e58 = (_e57 != 1u);
    phi_85_ = _e58;
    if !(_e58) {
        let _e60 = temporalCurrentClip_1;
        param = _e60;
        let _e61 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_85_ = !(_e61);
    }
    let _e64 = phi_85_;
    phi_94_ = _e64;
    if !(_e64) {
        let _e66 = temporalPreviousClip_1;
        param_1 = _e66;
        let _e67 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_94_ = !(_e67);
    }
    let _e70 = phi_94_;
    phi_104_ = _e70;
    if !(_e70) {
        let _e73 = temporalCurrentClip_1[3u];
        phi_104_ = (_e73 <= 0.000001f);
    }
    let _e76 = phi_104_;
    phi_111_ = _e76;
    if !(_e76) {
        let _e79 = temporalPreviousClip_1[3u];
        phi_111_ = (_e79 <= 0.000001f);
    }
    let _e82 = phi_111_;
    if _e82 {
        return;
    }
    let _e83 = temporalCurrentClip_1;
    let _e86 = temporalCurrentClip_1[3u];
    currentNdc = (_e83.xy / vec2(_e86));
    let _e89 = temporalPreviousClip_1;
    let _e92 = temporalPreviousClip_1[3u];
    previousNdc = (_e89.xy / vec2(_e92));
    let _e95 = currentNdc;
    param_2 = _e95;
    let _e96 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e97 = !(_e96);
    phi_140_ = _e97;
    if !(_e97) {
        let _e99 = previousNdc;
        param_3 = _e99;
        let _e100 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_140_ = !(_e100);
    }
    let _e103 = phi_140_;
    if _e103 {
        return;
    }
    let _e104 = currentNdc;
    currentUv = ((_e104 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e107 = previousNdc;
    previousUv = ((_e107 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e110 = currentUv;
    let _e111 = previousUv;
    velocity = (_e110 - _e111);
    let _e113 = velocity;
    param_4 = _e113;
    let _e114 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e114) {
        return;
    }
    let _e116 = velocity;
    out_temporal_velocity = _e116;
    let _e117 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e117, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e49 = (*c);
    (*c) = max(_e49, vec3<f32>(0f, 0f, 0f));
    let _e51 = (*c);
    cutoff = (_e51 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e53 = (*c);
    lo = (_e53 / vec3(12.92f));
    let _e56 = (*c);
    hi = pow(((_e56 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e61 = hi;
    let _e62 = lo;
    let _e63 = cutoff;
    return mix(_e61, _e62, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e63));
}

fn main_1() {
    var t: f32;
    var nf: f32;
    var animPos: f32;
    var frame: i32;
    var nextFr: i32;
    var blend: f32;
    var diffuseA: vec4<f32>;
    var diffuseB: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;
    var diffuse: vec4<f32>;
    var t0_: vec4<f32>;
    var param_7: vec3<f32>;
    var t1_: vec4<f32>;
    var param_8: vec3<f32>;
    var t2_: vec4<f32>;
    var param_9: vec3<f32>;
    var t3_: vec4<f32>;
    var param_10: vec3<f32>;
    var lm: vec4<f32>;
    var param_11: f32;
    var param_12: vec4<f32>;

    let _e69 = unnamed._animPad[0u];
    t = (_e69 * 0.01f);
    let _e73 = unnamed._animPad[1u];
    nf = max(1f, _e73);
    let _e75 = t;
    let _e76 = nf;
    animPos = (_e75 - (floor((_e75 / _e76)) * _e76));
    let _e81 = animPos;
    frame = i32(_e81);
    let _e83 = frame;
    let _e85 = (f32(_e83) + 1f);
    let _e86 = nf;
    nextFr = i32((_e85 - (floor((_e85 / _e86)) * _e86)));
    let _e92 = animPos;
    blend = fract(_e92);
    let _e97 = unnamed.packed_indices[0i][0u];
    let _e103 = unnamed.packed_indices[0i][0u];
    let _e108 = frag_tex_coord0_1;
    let _e109 = frame;
    let _e113 = vec3<f32>(_e108.x, _e108.y, f32(_e109));
    let _e119 = textureSample(wired_bindless_image_arrays[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], vec2<f32>(_e113.x, _e113.y), i32(_e113.z));
    diffuseA = _e119;
    let _e123 = unnamed.packed_indices[0i][0u];
    let _e129 = unnamed.packed_indices[0i][0u];
    let _e134 = frag_tex_coord0_1;
    let _e135 = nextFr;
    let _e139 = vec3<f32>(_e134.x, _e134.y, f32(_e135));
    let _e145 = textureSample(wired_bindless_image_arrays[(_e123 & 4095u)], wired_bindless_samplers[((_e129 >> bitcast<u32>(12i)) & 255u)], vec2<f32>(_e139.x, _e139.y), i32(_e139.z));
    diffuseB = _e145;
    let _e146 = diffuseA;
    param_5 = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param_5));
    diffuseA[0u] = _e148.x;
    diffuseA[1u] = _e148.y;
    diffuseA[2u] = _e148.z;
    let _e155 = diffuseB;
    param_6 = _e155.xyz;
    let _e157 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    diffuseB[0u] = _e157.x;
    diffuseB[1u] = _e157.y;
    diffuseB[2u] = _e157.z;
    let _e164 = diffuseA;
    let _e165 = diffuseB;
    let _e166 = blend;
    diffuse = mix(_e164, _e165, vec4(_e166));
    let _e172 = unnamed.packed_indices[0i][2u];
    let _e178 = unnamed.packed_indices[0i][2u];
    let _e183 = frag_tex_coord1_1;
    let _e184 = textureSample(wired_bindless_images[(_e172 & 4095u)], wired_bindless_samplers[((_e178 >> bitcast<u32>(12i)) & 255u)], _e183);
    t0_ = _e184;
    let _e185 = t0_;
    param_7 = _e185.xyz;
    let _e187 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    t0_[0u] = _e187.x;
    t0_[1u] = _e187.y;
    t0_[2u] = _e187.z;
    let _e197 = unnamed.packed_indices[0i][3u];
    let _e203 = unnamed.packed_indices[0i][3u];
    let _e208 = frag_tex_coord1_1;
    let _e209 = textureSample(wired_bindless_images[(_e197 & 4095u)], wired_bindless_samplers[((_e203 >> bitcast<u32>(12i)) & 255u)], _e208);
    t1_ = _e209;
    let _e210 = t1_;
    param_8 = _e210.xyz;
    let _e212 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    t1_[0u] = _e212.x;
    t1_[1u] = _e212.y;
    t1_[2u] = _e212.z;
    let _e222 = unnamed.packed_indices[1i][0u];
    let _e228 = unnamed.packed_indices[1i][0u];
    let _e233 = frag_tex_coord1_1;
    let _e234 = textureSample(wired_bindless_images[(_e222 & 4095u)], wired_bindless_samplers[((_e228 >> bitcast<u32>(12i)) & 255u)], _e233);
    t2_ = _e234;
    let _e235 = t2_;
    param_9 = _e235.xyz;
    let _e237 = sRGBToLinear_u0028_vf3_u003b((&param_9));
    t2_[0u] = _e237.x;
    t2_[1u] = _e237.y;
    t2_[2u] = _e237.z;
    let _e247 = unnamed.packed_indices[1i][1u];
    let _e253 = unnamed.packed_indices[1i][1u];
    let _e258 = frag_tex_coord1_1;
    let _e259 = textureSample(wired_bindless_images[(_e247 & 4095u)], wired_bindless_samplers[((_e253 >> bitcast<u32>(12i)) & 255u)], _e258);
    t3_ = _e259;
    let _e260 = t3_;
    param_10 = _e260.xyz;
    let _e262 = sRGBToLinear_u0028_vf3_u003b((&param_10));
    t3_[0u] = _e262.x;
    t3_[1u] = _e262.y;
    t3_[2u] = _e262.z;
    let _e269 = t0_;
    let _e272 = unnamed.q1StyleIntensities[0u];
    let _e274 = t1_;
    let _e277 = unnamed.q1StyleIntensities[1u];
    let _e280 = t2_;
    let _e283 = unnamed.q1StyleIntensities[2u];
    let _e286 = t3_;
    let _e289 = unnamed.q1StyleIntensities[3u];
    lm = ((((_e269 * _e272) + (_e274 * _e277)) + (_e280 * _e283)) + (_e286 * _e289));
    let _e292 = diffuse;
    let _e293 = lm;
    out_color = (_e292 * _e293);
    param_11 = 1f;
    let _e295 = out_color;
    param_12 = _e295;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_11), (&param_12));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
