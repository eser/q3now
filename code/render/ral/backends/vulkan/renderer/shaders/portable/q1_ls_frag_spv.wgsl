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

@group(0) @binding(0)
var<uniform> unnamed: UBO;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e34 = (*c);
    (*c) = max(_e34, vec3<f32>(0f, 0f, 0f));
    let _e36 = (*c);
    cutoff = (_e36 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e38 = (*c);
    lo = (_e38 / vec3(12.92f));
    let _e41 = (*c);
    hi = pow(((_e41 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e46 = hi;
    let _e47 = lo;
    let _e48 = cutoff;
    return mix(_e46, _e47, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e48));
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

    let _e47 = unnamed._animPad[0u];
    animBlend = _e47;
    let _e51 = unnamed.packed_indices[0i][0u];
    let _e57 = unnamed.packed_indices[0i][0u];
    let _e62 = frag_tex_coord0_1;
    let _e63 = textureSample(wired_bindless_images[(_e51 & 4095u)], wired_bindless_samplers[((_e57 >> bitcast<u32>(12i)) & 255u)], _e62);
    diffuseA = _e63;
    let _e67 = unnamed.packed_indices[0i][1u];
    let _e73 = unnamed.packed_indices[0i][1u];
    let _e78 = frag_tex_coord0_1;
    let _e79 = textureSample(wired_bindless_images[(_e67 & 4095u)], wired_bindless_samplers[((_e73 >> bitcast<u32>(12i)) & 255u)], _e78);
    diffuseB = _e79;
    let _e80 = diffuseA;
    param = _e80.xyz;
    let _e82 = sRGBToLinear_u0028_vf3_u003b((&param));
    diffuseA[0u] = _e82.x;
    diffuseA[1u] = _e82.y;
    diffuseA[2u] = _e82.z;
    let _e89 = diffuseB;
    param_1 = _e89.xyz;
    let _e91 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    diffuseB[0u] = _e91.x;
    diffuseB[1u] = _e91.y;
    diffuseB[2u] = _e91.z;
    let _e98 = diffuseA;
    let _e99 = diffuseB;
    let _e100 = animBlend;
    diffuse = mix(_e98, _e99, vec4(_e100));
    let _e106 = unnamed.packed_indices[0i][2u];
    let _e112 = unnamed.packed_indices[0i][2u];
    let _e117 = frag_tex_coord1_1;
    let _e118 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    t0_ = _e118;
    let _e119 = t0_;
    param_2 = _e119.xyz;
    let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    t0_[0u] = _e121.x;
    t0_[1u] = _e121.y;
    t0_[2u] = _e121.z;
    let _e131 = unnamed.packed_indices[0i][3u];
    let _e137 = unnamed.packed_indices[0i][3u];
    let _e142 = frag_tex_coord1_1;
    let _e143 = textureSample(wired_bindless_images[(_e131 & 4095u)], wired_bindless_samplers[((_e137 >> bitcast<u32>(12i)) & 255u)], _e142);
    t1_ = _e143;
    let _e144 = t1_;
    param_3 = _e144.xyz;
    let _e146 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    t1_[0u] = _e146.x;
    t1_[1u] = _e146.y;
    t1_[2u] = _e146.z;
    let _e156 = unnamed.packed_indices[1i][0u];
    let _e162 = unnamed.packed_indices[1i][0u];
    let _e167 = frag_tex_coord1_1;
    let _e168 = textureSample(wired_bindless_images[(_e156 & 4095u)], wired_bindless_samplers[((_e162 >> bitcast<u32>(12i)) & 255u)], _e167);
    t2_ = _e168;
    let _e169 = t2_;
    param_4 = _e169.xyz;
    let _e171 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    t2_[0u] = _e171.x;
    t2_[1u] = _e171.y;
    t2_[2u] = _e171.z;
    let _e181 = unnamed.packed_indices[1i][1u];
    let _e187 = unnamed.packed_indices[1i][1u];
    let _e192 = frag_tex_coord1_1;
    let _e193 = textureSample(wired_bindless_images[(_e181 & 4095u)], wired_bindless_samplers[((_e187 >> bitcast<u32>(12i)) & 255u)], _e192);
    t3_ = _e193;
    let _e194 = t3_;
    param_5 = _e194.xyz;
    let _e196 = sRGBToLinear_u0028_vf3_u003b((&param_5));
    t3_[0u] = _e196.x;
    t3_[1u] = _e196.y;
    t3_[2u] = _e196.z;
    let _e203 = t0_;
    let _e206 = unnamed.q1StyleIntensities[0u];
    let _e208 = t1_;
    let _e211 = unnamed.q1StyleIntensities[1u];
    let _e214 = t2_;
    let _e217 = unnamed.q1StyleIntensities[2u];
    let _e220 = t3_;
    let _e223 = unnamed.q1StyleIntensities[3u];
    lm = ((((_e203 * _e206) + (_e208 * _e211)) + (_e214 * _e217)) + (_e220 * _e223));
    let _e226 = diffuse;
    let _e227 = lm;
    out_color = (_e226 * _e227);
    return;
}

@fragment
fn main(@location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e5 = out_color;
    return _e5;
}
