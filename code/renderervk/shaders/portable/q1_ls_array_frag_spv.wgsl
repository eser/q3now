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
@group(1) @binding(2) 
var wired_bindless_image_arrays: binding_array<texture_2d_array<f32>>;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e36 = (*c);
    (*c) = max(_e36, vec3<f32>(0f, 0f, 0f));
    let _e38 = (*c);
    cutoff = (_e38 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e40 = (*c);
    lo = (_e40 / vec3(12.92f));
    let _e43 = (*c);
    hi = pow(((_e43 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e48 = hi;
    let _e49 = lo;
    let _e50 = cutoff;
    return mix(_e48, _e49, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e50));
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

    let _e54 = unnamed._animPad[0u];
    t = (_e54 * 0.01f);
    let _e58 = unnamed._animPad[1u];
    nf = max(1f, _e58);
    let _e60 = t;
    let _e61 = nf;
    animPos = (_e60 - (floor((_e60 / _e61)) * _e61));
    let _e66 = animPos;
    frame = i32(_e66);
    let _e68 = frame;
    let _e70 = (f32(_e68) + 1f);
    let _e71 = nf;
    nextFr = i32((_e70 - (floor((_e70 / _e71)) * _e71)));
    let _e77 = animPos;
    blend = fract(_e77);
    let _e82 = unnamed.packed_indices[0i][0u];
    let _e88 = unnamed.packed_indices[0i][0u];
    let _e93 = frag_tex_coord0_1;
    let _e94 = frame;
    let _e98 = vec3<f32>(_e93.x, _e93.y, f32(_e94));
    let _e104 = textureSample(wired_bindless_image_arrays[(_e82 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], vec2<f32>(_e98.x, _e98.y), i32(_e98.z));
    diffuseA = _e104;
    let _e108 = unnamed.packed_indices[0i][0u];
    let _e114 = unnamed.packed_indices[0i][0u];
    let _e119 = frag_tex_coord0_1;
    let _e120 = nextFr;
    let _e124 = vec3<f32>(_e119.x, _e119.y, f32(_e120));
    let _e130 = textureSample(wired_bindless_image_arrays[(_e108 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], vec2<f32>(_e124.x, _e124.y), i32(_e124.z));
    diffuseB = _e130;
    let _e131 = diffuseA;
    param = _e131.xyz;
    let _e133 = sRGBToLinear_u0028_vf3_u003b((&param));
    diffuseA[0u] = _e133.x;
    diffuseA[1u] = _e133.y;
    diffuseA[2u] = _e133.z;
    let _e140 = diffuseB;
    param_1 = _e140.xyz;
    let _e142 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    diffuseB[0u] = _e142.x;
    diffuseB[1u] = _e142.y;
    diffuseB[2u] = _e142.z;
    let _e149 = diffuseA;
    let _e150 = diffuseB;
    let _e151 = blend;
    diffuse = mix(_e149, _e150, vec4(_e151));
    let _e157 = unnamed.packed_indices[0i][2u];
    let _e163 = unnamed.packed_indices[0i][2u];
    let _e168 = frag_tex_coord1_1;
    let _e169 = textureSample(wired_bindless_images[(_e157 & 4095u)], wired_bindless_samplers[((_e163 >> bitcast<u32>(12i)) & 255u)], _e168);
    t0_ = _e169;
    let _e170 = t0_;
    param_2 = _e170.xyz;
    let _e172 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    t0_[0u] = _e172.x;
    t0_[1u] = _e172.y;
    t0_[2u] = _e172.z;
    let _e182 = unnamed.packed_indices[0i][3u];
    let _e188 = unnamed.packed_indices[0i][3u];
    let _e193 = frag_tex_coord1_1;
    let _e194 = textureSample(wired_bindless_images[(_e182 & 4095u)], wired_bindless_samplers[((_e188 >> bitcast<u32>(12i)) & 255u)], _e193);
    t1_ = _e194;
    let _e195 = t1_;
    param_3 = _e195.xyz;
    let _e197 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    t1_[0u] = _e197.x;
    t1_[1u] = _e197.y;
    t1_[2u] = _e197.z;
    let _e207 = unnamed.packed_indices[1i][0u];
    let _e213 = unnamed.packed_indices[1i][0u];
    let _e218 = frag_tex_coord1_1;
    let _e219 = textureSample(wired_bindless_images[(_e207 & 4095u)], wired_bindless_samplers[((_e213 >> bitcast<u32>(12i)) & 255u)], _e218);
    t2_ = _e219;
    let _e220 = t2_;
    param_4 = _e220.xyz;
    let _e222 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    t2_[0u] = _e222.x;
    t2_[1u] = _e222.y;
    t2_[2u] = _e222.z;
    let _e232 = unnamed.packed_indices[1i][1u];
    let _e238 = unnamed.packed_indices[1i][1u];
    let _e243 = frag_tex_coord1_1;
    let _e244 = textureSample(wired_bindless_images[(_e232 & 4095u)], wired_bindless_samplers[((_e238 >> bitcast<u32>(12i)) & 255u)], _e243);
    t3_ = _e244;
    let _e245 = t3_;
    param_5 = _e245.xyz;
    let _e247 = sRGBToLinear_u0028_vf3_u003b((&param_5));
    t3_[0u] = _e247.x;
    t3_[1u] = _e247.y;
    t3_[2u] = _e247.z;
    let _e254 = t0_;
    let _e257 = unnamed.q1StyleIntensities[0u];
    let _e259 = t1_;
    let _e262 = unnamed.q1StyleIntensities[1u];
    let _e265 = t2_;
    let _e268 = unnamed.q1StyleIntensities[2u];
    let _e271 = t3_;
    let _e274 = unnamed.q1StyleIntensities[3u];
    lm = ((((_e254 * _e257) + (_e259 * _e262)) + (_e265 * _e268)) + (_e271 * _e274));
    let _e277 = diffuse;
    let _e278 = lm;
    out_color = (_e277 * _e278);
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
