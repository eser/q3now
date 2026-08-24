enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 30>,
    packed_indices: array<vec4<u32>, 3>,
}

@id(12) override distortion_scale: f32 = 0.02f;
@id(4) override tex_domain: i32 = 0i;
override override_type_10_: i32 = (tex_domain & 1i);
override override_type_5_: bool = (override_type_10_ != 0i);
@id(13) override tint_strength: f32 = 0.25f;
@id(14) override specular_power: f32 = 32f;
@id(0) override alpha_test_func: i32 = 0i;
@id(1) override alpha_test_value: f32 = 0f;
@id(3) override alpha_to_coverage: i32 = 0i;
@id(5) override abs_light: i32 = 0i;

@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> gl_FragCoord_1: vec4<f32>;
var<private> frag_tex_coord_1: vec2<f32>;
var<private> V_1: vec4<f32>;
var<private> N_1: vec3<f32>;
var<private> L_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e65 = (*c);
    (*c) = max(_e65, vec3<f32>(0f, 0f, 0f));
    let _e67 = (*c);
    cutoff = (_e67 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e69 = (*c);
    lo = (_e69 / vec3(12.92f));
    let _e72 = (*c);
    hi = pow(((_e72 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e77 = hi;
    let _e78 = lo;
    let _e79 = cutoff;
    return mix(_e77, _e78, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e79));
}

fn hash_u0028_vf2_u003b(p: ptr<function, vec2<f32>>) -> f32 {
    let _e62 = (*p);
    return fract((sin(dot(_e62, vec2<f32>(127.1f, 311.7f))) * 43758.547f));
}

fn noise_u0028_vf2_u003b(p_1: ptr<function, vec2<f32>>) -> f32 {
    var i: vec2<f32>;
    var f: vec2<f32>;
    var param: vec2<f32>;
    var param_1: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;

    let _e68 = (*p_1);
    i = floor(_e68);
    let _e70 = (*p_1);
    f = fract(_e70);
    let _e72 = f;
    let _e73 = f;
    let _e75 = f;
    f = ((_e72 * _e73) * (vec2(3f) - (_e75 * 2f)));
    let _e80 = i;
    param = _e80;
    let _e81 = hash_u0028_vf2_u003b((&param));
    let _e82 = i;
    param_1 = (_e82 + vec2<f32>(1f, 0f));
    let _e84 = hash_u0028_vf2_u003b((&param_1));
    let _e86 = f[0u];
    let _e88 = i;
    param_2 = (_e88 + vec2<f32>(0f, 1f));
    let _e90 = hash_u0028_vf2_u003b((&param_2));
    let _e91 = i;
    param_3 = (_e91 + vec2<f32>(1f, 1f));
    let _e93 = hash_u0028_vf2_u003b((&param_3));
    let _e95 = f[0u];
    let _e98 = f[1u];
    return mix(mix(_e81, _e84, _e86), mix(_e90, _e93, _e95), _e98);
}

fn rippleNoise_u0028_vf2_u003b(uv: ptr<function, vec2<f32>>) -> vec2<f32> {
    var n: vec2<f32>;
    var param_4: vec2<f32>;
    var param_5: vec2<f32>;
    var param_6: vec2<f32>;
    var param_7: vec2<f32>;

    let _e67 = (*uv);
    param_4 = (_e67 * 6f);
    let _e69 = noise_u0028_vf2_u003b((&param_4));
    let _e71 = (*uv);
    param_5 = (_e71 * 12f);
    let _e73 = noise_u0028_vf2_u003b((&param_5));
    n[0u] = ((_e69 * 0.6f) + (_e73 * 0.3f));
    let _e77 = (*uv);
    param_6 = ((_e77 * 6f) + vec2(43f));
    let _e81 = noise_u0028_vf2_u003b((&param_6));
    let _e83 = (*uv);
    param_7 = ((_e83 * 12f) + vec2(43f));
    let _e87 = noise_u0028_vf2_u003b((&param_7));
    n[1u] = ((_e81 * 0.6f) + (_e87 * 0.3f));
    let _e91 = n;
    return ((_e91 - vec2(0.5f)) * 2f);
}

fn main_1() {
    var screenSize: vec2<i32>;
    var screenUV: vec2<f32>;
    var ripple: vec2<f32>;
    var param_8: vec2<f32>;
    var refractionUV: vec2<f32>;
    var refraction: vec3<f32>;
    var waterTex: vec4<f32>;
    var waterColor: vec3<f32>;
    var local: vec3<f32>;
    var param_9: vec3<f32>;
    var nV: vec3<f32>;
    var fresnel: f32;
    var tinted: vec3<f32>;
    var finalColor: vec3<f32>;
    var nL: vec3<f32>;
    var halfVec: vec3<f32>;
    var spec: f32;

    let _e81 = unnamed.packed_indices[1i][2u];
    let _e87 = unnamed.packed_indices[1i][2u];
    let _e92 = textureDimensions(wired_bindless_images[(_e81 & 4095u)], 0i);
    screenSize = vec2<i32>(_e92);
    let _e94 = gl_FragCoord_1;
    let _e96 = screenSize;
    screenUV = (_e94.xy / vec2<f32>(_e96));
    let _e99 = frag_tex_coord_1;
    param_8 = _e99;
    let _e100 = rippleNoise_u0028_vf2_u003b((&param_8));
    ripple = _e100;
    let _e101 = screenUV;
    let _e102 = ripple;
    refractionUV = clamp((_e101 + (_e102 * distortion_scale)), vec2(0.001f), vec2(0.999f));
    let _e111 = unnamed.packed_indices[1i][2u];
    let _e117 = unnamed.packed_indices[1i][2u];
    let _e122 = refractionUV;
    let _e123 = textureSample(wired_bindless_images[(_e111 & 4095u)], wired_bindless_samplers[((_e117 >> bitcast<u32>(12i)) & 255u)], _e122);
    refraction = _e123.xyz;
    let _e128 = unnamed.packed_indices[0i][0u];
    let _e134 = unnamed.packed_indices[0i][0u];
    let _e139 = frag_tex_coord_1;
    let _e140 = textureSample(wired_bindless_images[(_e128 & 4095u)], wired_bindless_samplers[((_e134 >> bitcast<u32>(12i)) & 255u)], _e139);
    waterTex = _e140;
    if override_type_5_ {
        let _e141 = waterTex;
        local = _e141.xyz;
    } else {
        let _e143 = waterTex;
        param_9 = _e143.xyz;
        let _e145 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        local = _e145;
    }
    let _e146 = local;
    waterColor = _e146;
    let _e147 = V_1;
    nV = normalize(_e147.xyz);
    let _e150 = N_1;
    let _e152 = nV;
    fresnel = (1f - abs(dot(normalize(_e150), _e152)));
    let _e156 = fresnel;
    let _e157 = fresnel;
    fresnel = (_e156 * _e157);
    let _e159 = refraction;
    let _e160 = waterColor;
    tinted = mix(_e159, _e160, vec3(tint_strength));
    let _e163 = tinted;
    let _e164 = waterColor;
    let _e166 = fresnel;
    finalColor = mix(_e163, (_e164 * 1.3f), vec3((_e166 * 0.6f)));
    let _e170 = L_1;
    nL = normalize(_e170.xyz);
    let _e173 = nL;
    let _e174 = nV;
    halfVec = normalize((_e173 + _e174));
    let _e177 = N_1;
    let _e179 = halfVec;
    spec = pow(max(dot(normalize(_e177), _e179), 0f), specular_power);
    let _e183 = spec;
    let _e186 = finalColor;
    finalColor = (_e186 + vec3((_e183 * 0.25f)));
    let _e188 = finalColor;
    let _e190 = waterTex[3u];
    out_color = vec4<f32>(_e188.x, _e188.y, _e188.z, _e190);
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_tex_coord: vec2<f32>, @location(3) V: vec4<f32>, @location(1) N: vec3<f32>, @location(2) L: vec4<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord_1 = frag_tex_coord;
    V_1 = V;
    N_1 = N;
    L_1 = L;
    main_1();
    let _e11 = out_color;
    return _e11;
}
