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
@group(0) @binding(0)
var<uniform> unnamed: UBO;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e40 = (*sceneColor)[0u];
    let _e42 = (*sceneColor)[1u];
    let _e44 = (*sceneColor)[2u];
    let _e49 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e40, max(_e42, _e44)), 0f, 1f), 0f, 0f, clamp(_e49, 0f, 1f));
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e41 = (*c);
    (*c) = max(_e41, vec3<f32>(0f, 0f, 0f));
    let _e43 = (*c);
    cutoff = (_e43 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e45 = (*c);
    lo = (_e45 / vec3(12.92f));
    let _e48 = (*c);
    hi = pow(((_e48 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e53 = hi;
    let _e54 = lo;
    let _e55 = cutoff;
    return mix(_e53, _e54, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e55));
}

fn main_1() {
    var animBlend: f32;
    var diffuseA: vec4<f32>;
    var diffuseB: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;
    var diffuse: vec4<f32>;
    var t0_: vec4<f32>;
    var param_2: vec3<f32>;
    var t1_: vec4<f32>;
    var param_3: vec3<f32>;
    var t2_: vec4<f32>;
    var param_4: vec3<f32>;
    var t3_: vec4<f32>;
    var param_5: vec3<f32>;
    var lm: vec4<f32>;
    var param_6: f32;
    var param_7: vec4<f32>;

    let _e56 = unnamed._animPad[0u];
    animBlend = _e56;
    let _e60 = unnamed.packed_indices[0i][0u];
    let _e66 = unnamed.packed_indices[0i][0u];
    let _e71 = frag_tex_coord0_1;
    let _e72 = textureSample(wired_bindless_images[(_e60 & 4095u)], wired_bindless_samplers[((_e66 >> bitcast<u32>(12i)) & 255u)], _e71);
    diffuseA = _e72;
    let _e76 = unnamed.packed_indices[0i][1u];
    let _e82 = unnamed.packed_indices[0i][1u];
    let _e87 = frag_tex_coord0_1;
    let _e88 = textureSample(wired_bindless_images[(_e76 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    diffuseB = _e88;
    let _e89 = diffuseA;
    param = _e89.xyz;
    let _e91 = sRGBToLinear_u0028_vf3_u003b((&param));
    diffuseA[0u] = _e91.x;
    diffuseA[1u] = _e91.y;
    diffuseA[2u] = _e91.z;
    let _e98 = diffuseB;
    param_1 = _e98.xyz;
    let _e100 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    diffuseB[0u] = _e100.x;
    diffuseB[1u] = _e100.y;
    diffuseB[2u] = _e100.z;
    let _e107 = diffuseA;
    let _e108 = diffuseB;
    let _e109 = animBlend;
    diffuse = mix(_e107, _e108, vec4(_e109));
    let _e115 = unnamed.packed_indices[0i][2u];
    let _e121 = unnamed.packed_indices[0i][2u];
    let _e126 = frag_tex_coord1_1;
    let _e127 = textureSample(wired_bindless_images[(_e115 & 4095u)], wired_bindless_samplers[((_e121 >> bitcast<u32>(12i)) & 255u)], _e126);
    t0_ = _e127;
    let _e128 = t0_;
    param_2 = _e128.xyz;
    let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    t0_[0u] = _e130.x;
    t0_[1u] = _e130.y;
    t0_[2u] = _e130.z;
    let _e140 = unnamed.packed_indices[0i][3u];
    let _e146 = unnamed.packed_indices[0i][3u];
    let _e151 = frag_tex_coord1_1;
    let _e152 = textureSample(wired_bindless_images[(_e140 & 4095u)], wired_bindless_samplers[((_e146 >> bitcast<u32>(12i)) & 255u)], _e151);
    t1_ = _e152;
    let _e153 = t1_;
    param_3 = _e153.xyz;
    let _e155 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    t1_[0u] = _e155.x;
    t1_[1u] = _e155.y;
    t1_[2u] = _e155.z;
    let _e165 = unnamed.packed_indices[1i][0u];
    let _e171 = unnamed.packed_indices[1i][0u];
    let _e176 = frag_tex_coord1_1;
    let _e177 = textureSample(wired_bindless_images[(_e165 & 4095u)], wired_bindless_samplers[((_e171 >> bitcast<u32>(12i)) & 255u)], _e176);
    t2_ = _e177;
    let _e178 = t2_;
    param_4 = _e178.xyz;
    let _e180 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    t2_[0u] = _e180.x;
    t2_[1u] = _e180.y;
    t2_[2u] = _e180.z;
    let _e190 = unnamed.packed_indices[1i][1u];
    let _e196 = unnamed.packed_indices[1i][1u];
    let _e201 = frag_tex_coord1_1;
    let _e202 = textureSample(wired_bindless_images[(_e190 & 4095u)], wired_bindless_samplers[((_e196 >> bitcast<u32>(12i)) & 255u)], _e201);
    t3_ = _e202;
    let _e203 = t3_;
    param_5 = _e203.xyz;
    let _e205 = sRGBToLinear_u0028_vf3_u003b((&param_5));
    t3_[0u] = _e205.x;
    t3_[1u] = _e205.y;
    t3_[2u] = _e205.z;
    let _e212 = t0_;
    let _e215 = unnamed.q1StyleIntensities[0u];
    let _e217 = t1_;
    let _e220 = unnamed.q1StyleIntensities[1u];
    let _e223 = t2_;
    let _e226 = unnamed.q1StyleIntensities[2u];
    let _e229 = t3_;
    let _e232 = unnamed.q1StyleIntensities[3u];
    lm = ((((_e212 * _e215) + (_e217 * _e220)) + (_e223 * _e226)) + (_e229 * _e232));
    let _e235 = diffuse;
    let _e236 = lm;
    out_color = (_e235 * _e236);
    param_6 = 1f;
    let _e238 = out_color;
    param_7 = _e238;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_6), (&param_7));
    return;
}

@fragment
fn main(@location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e13 = out_temporal_velocity;
    let _e14 = out_temporal_validity;
    let _e15 = out_color;
    return FragmentOutput(_e13, _e14, _e15);
}
