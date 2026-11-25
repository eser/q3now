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
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_60_: bool;

    let _e44 = (*value);
    let _e45 = (*value);
    let _e47 = all((_e44 == _e45));
    phi_60_ = _e47;
    if _e47 {
        let _e48 = (*value);
        phi_60_ = all((abs(_e48) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e53 = phi_60_;
    return _e53;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_45_: bool;

    let _e44 = (*value_1);
    let _e45 = (*value_1);
    let _e47 = all((_e44 == _e45));
    phi_45_ = _e47;
    if _e47 {
        let _e48 = (*value_1);
        phi_45_ = all((abs(_e48) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e53 = phi_45_;
    return _e53;
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
    let _e55 = temporalOutcome_1;
    let _e56 = (_e55 != 1u);
    phi_85_ = _e56;
    if !(_e56) {
        let _e58 = temporalCurrentClip_1;
        param = _e58;
        let _e59 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_85_ = !(_e59);
    }
    let _e62 = phi_85_;
    phi_94_ = _e62;
    if !(_e62) {
        let _e64 = temporalPreviousClip_1;
        param_1 = _e64;
        let _e65 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_94_ = !(_e65);
    }
    let _e68 = phi_94_;
    phi_104_ = _e68;
    if !(_e68) {
        let _e71 = temporalCurrentClip_1[3u];
        phi_104_ = (_e71 <= 0.000001f);
    }
    let _e74 = phi_104_;
    phi_111_ = _e74;
    if !(_e74) {
        let _e77 = temporalPreviousClip_1[3u];
        phi_111_ = (_e77 <= 0.000001f);
    }
    let _e80 = phi_111_;
    if _e80 {
        return;
    }
    let _e81 = temporalCurrentClip_1;
    let _e84 = temporalCurrentClip_1[3u];
    currentNdc = (_e81.xy / vec2(_e84));
    let _e87 = temporalPreviousClip_1;
    let _e90 = temporalPreviousClip_1[3u];
    previousNdc = (_e87.xy / vec2(_e90));
    let _e93 = currentNdc;
    param_2 = _e93;
    let _e94 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e95 = !(_e94);
    phi_140_ = _e95;
    if !(_e95) {
        let _e97 = previousNdc;
        param_3 = _e97;
        let _e98 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_140_ = !(_e98);
    }
    let _e101 = phi_140_;
    if _e101 {
        return;
    }
    let _e102 = currentNdc;
    currentUv = ((_e102 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e105 = previousNdc;
    previousUv = ((_e105 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e108 = currentUv;
    let _e109 = previousUv;
    velocity = (_e108 - _e109);
    let _e111 = velocity;
    param_4 = _e111;
    let _e112 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e112) {
        return;
    }
    let _e114 = velocity;
    out_temporal_velocity = _e114;
    let _e115 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e115, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e47 = (*c);
    (*c) = max(_e47, vec3<f32>(0f, 0f, 0f));
    let _e49 = (*c);
    cutoff = (_e49 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e51 = (*c);
    lo = (_e51 / vec3(12.92f));
    let _e54 = (*c);
    hi = pow(((_e54 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e59 = hi;
    let _e60 = lo;
    let _e61 = cutoff;
    return mix(_e59, _e60, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e61));
}

fn main_1() {
    var animBlend: f32;
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

    let _e62 = unnamed._animPad[0u];
    animBlend = _e62;
    let _e66 = unnamed.packed_indices[0i][0u];
    let _e72 = unnamed.packed_indices[0i][0u];
    let _e77 = frag_tex_coord0_1;
    let _e78 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e72 >> bitcast<u32>(12i)) & 255u)], _e77);
    diffuseA = _e78;
    let _e82 = unnamed.packed_indices[0i][1u];
    let _e88 = unnamed.packed_indices[0i][1u];
    let _e93 = frag_tex_coord0_1;
    let _e94 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    diffuseB = _e94;
    let _e95 = diffuseA;
    param_5 = _e95.xyz;
    let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_5));
    diffuseA[0u] = _e97.x;
    diffuseA[1u] = _e97.y;
    diffuseA[2u] = _e97.z;
    let _e104 = diffuseB;
    param_6 = _e104.xyz;
    let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    diffuseB[0u] = _e106.x;
    diffuseB[1u] = _e106.y;
    diffuseB[2u] = _e106.z;
    let _e113 = diffuseA;
    let _e114 = diffuseB;
    let _e115 = animBlend;
    diffuse = mix(_e113, _e114, vec4(_e115));
    let _e121 = unnamed.packed_indices[0i][2u];
    let _e127 = unnamed.packed_indices[0i][2u];
    let _e132 = frag_tex_coord1_1;
    let _e133 = textureSample(wired_bindless_images[(_e121 & 4095u)], wired_bindless_samplers[((_e127 >> bitcast<u32>(12i)) & 255u)], _e132);
    t0_ = _e133;
    let _e134 = t0_;
    param_7 = _e134.xyz;
    let _e136 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    t0_[0u] = _e136.x;
    t0_[1u] = _e136.y;
    t0_[2u] = _e136.z;
    let _e146 = unnamed.packed_indices[0i][3u];
    let _e152 = unnamed.packed_indices[0i][3u];
    let _e157 = frag_tex_coord1_1;
    let _e158 = textureSample(wired_bindless_images[(_e146 & 4095u)], wired_bindless_samplers[((_e152 >> bitcast<u32>(12i)) & 255u)], _e157);
    t1_ = _e158;
    let _e159 = t1_;
    param_8 = _e159.xyz;
    let _e161 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    t1_[0u] = _e161.x;
    t1_[1u] = _e161.y;
    t1_[2u] = _e161.z;
    let _e171 = unnamed.packed_indices[1i][0u];
    let _e177 = unnamed.packed_indices[1i][0u];
    let _e182 = frag_tex_coord1_1;
    let _e183 = textureSample(wired_bindless_images[(_e171 & 4095u)], wired_bindless_samplers[((_e177 >> bitcast<u32>(12i)) & 255u)], _e182);
    t2_ = _e183;
    let _e184 = t2_;
    param_9 = _e184.xyz;
    let _e186 = sRGBToLinear_u0028_vf3_u003b((&param_9));
    t2_[0u] = _e186.x;
    t2_[1u] = _e186.y;
    t2_[2u] = _e186.z;
    let _e196 = unnamed.packed_indices[1i][1u];
    let _e202 = unnamed.packed_indices[1i][1u];
    let _e207 = frag_tex_coord1_1;
    let _e208 = textureSample(wired_bindless_images[(_e196 & 4095u)], wired_bindless_samplers[((_e202 >> bitcast<u32>(12i)) & 255u)], _e207);
    t3_ = _e208;
    let _e209 = t3_;
    param_10 = _e209.xyz;
    let _e211 = sRGBToLinear_u0028_vf3_u003b((&param_10));
    t3_[0u] = _e211.x;
    t3_[1u] = _e211.y;
    t3_[2u] = _e211.z;
    let _e218 = t0_;
    let _e221 = unnamed.q1StyleIntensities[0u];
    let _e223 = t1_;
    let _e226 = unnamed.q1StyleIntensities[1u];
    let _e229 = t2_;
    let _e232 = unnamed.q1StyleIntensities[2u];
    let _e235 = t3_;
    let _e238 = unnamed.q1StyleIntensities[3u];
    lm = ((((_e218 * _e221) + (_e223 * _e226)) + (_e229 * _e232)) + (_e235 * _e238));
    let _e241 = diffuse;
    let _e242 = lm;
    out_color = (_e241 * _e242);
    param_11 = 1f;
    let _e244 = out_color;
    param_12 = _e244;
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
