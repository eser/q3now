enable wgpu_binding_array;

struct UBO {
    eyePos: vec4<f32>,
    lightPos: vec4<f32>,
    lightColor: vec4<f32>,
    lightVector: vec4<f32>,
    fogDistanceVector: vec4<f32>,
    fogDepthVector: vec4<f32>,
    fogEyeT: vec4<f32>,
    fogColor: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 26>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
    emissionRadiance: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e61 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e61 + 0.5f));
    let _e66 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e68 = fogType;
    let _e71 = fogType;
    return (((_e66 > 0.5f) && (_e68 >= 1i)) && (_e71 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e61 = wired_advanced_fog_enabled_u0028_();
    if !(_e61) {
        return 0f;
    }
    let _e64 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e64, 0.000001f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e69 + 0.5f));
    let _e72 = fogType_1;
    if (_e72 == 1i) {
        let _e76 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e76 <= 0f) {
            return 0f;
        }
        let _e78 = viewDepth;
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e78 / _e81), 0f, 1f);
    }
    let _e86 = unnamed.advancedFogColorDensity[3u];
    let _e88 = viewDepth;
    opticalDepth = (max(_e86, 0f) * _e88);
    let _e90 = fogType_1;
    if (_e90 == 2i) {
        let _e92 = opticalDepth;
        return clamp((1f - exp(-(_e92))), 0f, 1f);
    }
    let _e97 = opticalDepth;
    let _e98 = opticalDepth;
    return clamp((1f - exp(-((_e97 * _e98)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e62 = (*c);
    (*c) = max(_e62, vec3<f32>(0f, 0f, 0f));
    let _e64 = (*c);
    cutoff = (_e64 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e66 = (*c);
    lo = (_e66 / vec3(12.92f));
    let _e69 = (*c);
    hi = pow(((_e69 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e74 = hi;
    let _e75 = lo;
    let _e76 = cutoff;
    return mix(_e74, _e75, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e76));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e63 = (*role);
    let _e65 = (*role);
    let _e70 = unnamed.packed_indices[(_e63 / 4u)][(_e65 % 4u)];
    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e85 = (*uv);
    let _e86 = textureSample(wired_bindless_images[(_e70 & 4095u)], wired_bindless_samplers[((_e80 >> bitcast<u32>(12i)) & 255u)], _e85);
    c_1 = _e86;
    let _e87 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e87))) == 0i) {
        let _e92 = c_1;
        param = _e92.xyz;
        let _e94 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e94.x;
        c_1[1u] = _e94.y;
        c_1[2u] = _e94.z;
    }
    let _e101 = (*slot);
    if (lightmap_slot == (_e101 + 1i)) {
        let _e106 = unnamed.worldLightParams[0u];
        let _e107 = c_1;
        let _e109 = (_e107.xyz * _e106);
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = c_1;
    return _e116;
}

fn main_1() {
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var color1_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_2: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e84 = frag_tex_coord0_1;
    param_2 = _e84;
    param_3 = 0i;
    let _e85 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e86 = frag_color;
    color0_ = (_e85 * _e86);
    if override_type_3_ {
        param_4 = 1u;
        let _e88 = frag_tex_coord1_1;
        param_5 = _e88;
        param_6 = 1i;
        let _e89 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e89;
        let _e90 = color0_;
        let _e92 = color1_;
        let _e94 = (_e90.xyz + _e92.xyz);
        let _e96 = color0_[3u];
        let _e98 = color1_[3u];
        base = vec4<f32>(_e94.x, _e94.y, _e94.z, (_e96 * _e98));
    } else {
        if override_type_3_1 {
            param_7 = 1u;
            let _e104 = frag_tex_coord1_1;
            param_8 = _e104;
            param_9 = 1i;
            let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e106 = frag_color;
            color1_1 = (_e105 * _e106);
            let _e108 = color0_;
            let _e110 = color1_1;
            let _e112 = (_e108.xyz + _e110.xyz);
            let _e114 = color0_[3u];
            let _e116 = color1_1[3u];
            base = vec4<f32>(_e112.x, _e112.y, _e112.z, (_e114 * _e116));
        } else {
            param_10 = 1u;
            let _e122 = frag_tex_coord1_1;
            param_11 = _e122;
            param_12 = 1i;
            let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e124 = frag_color;
            color1_2 = (_e123 * _e124);
            let _e126 = color0_;
            let _e128 = color1_2;
            let _e130 = (_e126.xyz * _e128.xyz);
            base[0u] = _e130.x;
            base[1u] = _e130.y;
            base[2u] = _e130.z;
            let _e138 = color0_[3u];
            let _e140 = color1_2[3u];
            base[3u] = (_e138 * _e140);
        }
    }
    if override_type_3_2 {
        let _e145 = unnamed.worldLightParams[1u];
        wetness = clamp(_e145, 0f, 1f);
        let _e149 = unnamed.worldLightParams[2u];
        frost = clamp(_e149, 0f, 1f);
        let _e151 = base;
        luminance = dot(_e151.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e154 = wetness;
        let _e156 = base;
        let _e158 = (_e156.xyz * mix(1f, 0.82f, _e154));
        base[0u] = _e158.x;
        base[1u] = _e158.y;
        base[2u] = _e158.z;
        let _e165 = base;
        let _e167 = luminance;
        let _e169 = luminance;
        let _e171 = luminance;
        let _e173 = frost;
        let _e176 = mix(_e165.xyz, vec3<f32>((_e167 * 0.88f), (_e169 * 0.94f), _e171), vec3((_e173 * 0.55f)));
        base[0u] = _e176.x;
        base[1u] = _e176.y;
        base[2u] = _e176.z;
    }
    let _e183 = color0_;
    let _e186 = unnamed.emissionRadiance;
    let _e189 = base;
    let _e191 = (_e189.xyz + (_e183.xyz * _e186.xyz));
    base[0u] = _e191.x;
    base[1u] = _e191.y;
    base[2u] = _e191.z;
    let _e198 = wired_advanced_fog_enabled_u0028_();
    if _e198 {
        let _e199 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e199;
        let _e200 = base;
        let _e203 = unnamed.advancedFogColorDensity;
        let _e205 = fogAmount;
        let _e207 = mix(_e200.xyz, _e203.xyz, vec3(_e205));
        base[0u] = _e207.x;
        base[1u] = _e207.y;
        base[2u] = _e207.z;
    }
    if override_type_3_3 {
        let _e215 = base[3u];
        if (_e215 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e217 = base;
            let _e219 = base;
            if (dot(_e217.xyz, _e219.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e223 = base;
    out_color = _e223;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e7 = out_color;
    return _e7;
}
