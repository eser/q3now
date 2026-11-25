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
@group(1) @binding(2)
var wired_bindless_image_arrays: binding_array<texture_2d_array<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e42 = (*sceneColor)[0u];
    let _e44 = (*sceneColor)[1u];
    let _e46 = (*sceneColor)[2u];
    let _e51 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e42, max(_e44, _e46)), 0f, 1f), 0f, 0f, clamp(_e51, 0f, 1f));
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e43 = (*c);
    (*c) = max(_e43, vec3<f32>(0f, 0f, 0f));
    let _e45 = (*c);
    cutoff = (_e45 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e47 = (*c);
    lo = (_e47 / vec3(12.92f));
    let _e50 = (*c);
    hi = pow(((_e50 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e55 = hi;
    let _e56 = lo;
    let _e57 = cutoff;
    return mix(_e55, _e56, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e57));
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

    let _e63 = unnamed._animPad[0u];
    t = (_e63 * 0.01f);
    let _e67 = unnamed._animPad[1u];
    nf = max(1f, _e67);
    let _e69 = t;
    let _e70 = nf;
    animPos = (_e69 - (floor((_e69 / _e70)) * _e70));
    let _e75 = animPos;
    frame = i32(_e75);
    let _e77 = frame;
    let _e79 = (f32(_e77) + 1f);
    let _e80 = nf;
    nextFr = i32((_e79 - (floor((_e79 / _e80)) * _e80)));
    let _e86 = animPos;
    blend = fract(_e86);
    let _e91 = unnamed.packed_indices[0i][0u];
    let _e97 = unnamed.packed_indices[0i][0u];
    let _e102 = frag_tex_coord0_1;
    let _e103 = frame;
    let _e107 = vec3<f32>(_e102.x, _e102.y, f32(_e103));
    let _e113 = textureSample(wired_bindless_image_arrays[(_e91 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], vec2<f32>(_e107.x, _e107.y), i32(_e107.z));
    diffuseA = _e113;
    let _e117 = unnamed.packed_indices[0i][0u];
    let _e123 = unnamed.packed_indices[0i][0u];
    let _e128 = frag_tex_coord0_1;
    let _e129 = nextFr;
    let _e133 = vec3<f32>(_e128.x, _e128.y, f32(_e129));
    let _e139 = textureSample(wired_bindless_image_arrays[(_e117 & 4095u)], wired_bindless_samplers[((_e123 >> bitcast<u32>(12i)) & 255u)], vec2<f32>(_e133.x, _e133.y), i32(_e133.z));
    diffuseB = _e139;
    let _e140 = diffuseA;
    param = _e140.xyz;
    let _e142 = sRGBToLinear_u0028_vf3_u003b((&param));
    diffuseA[0u] = _e142.x;
    diffuseA[1u] = _e142.y;
    diffuseA[2u] = _e142.z;
    let _e149 = diffuseB;
    param_1 = _e149.xyz;
    let _e151 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    diffuseB[0u] = _e151.x;
    diffuseB[1u] = _e151.y;
    diffuseB[2u] = _e151.z;
    let _e158 = diffuseA;
    let _e159 = diffuseB;
    let _e160 = blend;
    diffuse = mix(_e158, _e159, vec4(_e160));
    let _e166 = unnamed.packed_indices[0i][2u];
    let _e172 = unnamed.packed_indices[0i][2u];
    let _e177 = frag_tex_coord1_1;
    let _e178 = textureSample(wired_bindless_images[(_e166 & 4095u)], wired_bindless_samplers[((_e172 >> bitcast<u32>(12i)) & 255u)], _e177);
    t0_ = _e178;
    let _e179 = t0_;
    param_2 = _e179.xyz;
    let _e181 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    t0_[0u] = _e181.x;
    t0_[1u] = _e181.y;
    t0_[2u] = _e181.z;
    let _e191 = unnamed.packed_indices[0i][3u];
    let _e197 = unnamed.packed_indices[0i][3u];
    let _e202 = frag_tex_coord1_1;
    let _e203 = textureSample(wired_bindless_images[(_e191 & 4095u)], wired_bindless_samplers[((_e197 >> bitcast<u32>(12i)) & 255u)], _e202);
    t1_ = _e203;
    let _e204 = t1_;
    param_3 = _e204.xyz;
    let _e206 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    t1_[0u] = _e206.x;
    t1_[1u] = _e206.y;
    t1_[2u] = _e206.z;
    let _e216 = unnamed.packed_indices[1i][0u];
    let _e222 = unnamed.packed_indices[1i][0u];
    let _e227 = frag_tex_coord1_1;
    let _e228 = textureSample(wired_bindless_images[(_e216 & 4095u)], wired_bindless_samplers[((_e222 >> bitcast<u32>(12i)) & 255u)], _e227);
    t2_ = _e228;
    let _e229 = t2_;
    param_4 = _e229.xyz;
    let _e231 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    t2_[0u] = _e231.x;
    t2_[1u] = _e231.y;
    t2_[2u] = _e231.z;
    let _e241 = unnamed.packed_indices[1i][1u];
    let _e247 = unnamed.packed_indices[1i][1u];
    let _e252 = frag_tex_coord1_1;
    let _e253 = textureSample(wired_bindless_images[(_e241 & 4095u)], wired_bindless_samplers[((_e247 >> bitcast<u32>(12i)) & 255u)], _e252);
    t3_ = _e253;
    let _e254 = t3_;
    param_5 = _e254.xyz;
    let _e256 = sRGBToLinear_u0028_vf3_u003b((&param_5));
    t3_[0u] = _e256.x;
    t3_[1u] = _e256.y;
    t3_[2u] = _e256.z;
    let _e263 = t0_;
    let _e266 = unnamed.q1StyleIntensities[0u];
    let _e268 = t1_;
    let _e271 = unnamed.q1StyleIntensities[1u];
    let _e274 = t2_;
    let _e277 = unnamed.q1StyleIntensities[2u];
    let _e280 = t3_;
    let _e283 = unnamed.q1StyleIntensities[3u];
    lm = ((((_e263 * _e266) + (_e268 * _e271)) + (_e274 * _e277)) + (_e280 * _e283));
    let _e286 = diffuse;
    let _e287 = lm;
    out_color = (_e286 * _e287);
    param_6 = 1f;
    let _e289 = out_color;
    param_7 = _e289;
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
