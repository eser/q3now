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
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_2: bool = (discard_mode == 1i);
override override_type_4_3: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e56 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e56 + 0.5f));
    let _e61 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e63 = fogType;
    let _e66 = fogType;
    return (((_e61 > 0.5f) && (_e63 >= 1i)) && (_e66 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e56 = wired_advanced_fog_enabled_u0028_();
    if !(_e56) {
        return 0f;
    }
    let _e59 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e59, 0.000001f));
    let _e64 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e64 + 0.5f));
    let _e67 = fogType_1;
    if (_e67 == 1i) {
        let _e71 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e71 <= 0f) {
            return 0f;
        }
        let _e73 = viewDepth;
        let _e76 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e73 / _e76), 0f, 1f);
    }
    let _e81 = unnamed.advancedFogColorDensity[3u];
    let _e83 = viewDepth;
    opticalDepth = (max(_e81, 0f) * _e83);
    let _e85 = fogType_1;
    if (_e85 == 2i) {
        let _e87 = opticalDepth;
        return clamp((1f - exp(-(_e87))), 0f, 1f);
    }
    let _e92 = opticalDepth;
    let _e93 = opticalDepth;
    return clamp((1f - exp(-((_e92 * _e93)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e57 = (*c);
    (*c) = max(_e57, vec3<f32>(0f, 0f, 0f));
    let _e59 = (*c);
    cutoff = (_e59 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e61 = (*c);
    lo = (_e61 / vec3(12.92f));
    let _e64 = (*c);
    hi = pow(((_e64 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e69 = hi;
    let _e70 = lo;
    let _e71 = cutoff;
    return mix(_e69, _e70, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e71));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e58 = (*role);
    let _e60 = (*role);
    let _e65 = unnamed.packed_indices[(_e58 / 4u)][(_e60 % 4u)];
    let _e68 = (*role);
    let _e70 = (*role);
    let _e75 = unnamed.packed_indices[(_e68 / 4u)][(_e70 % 4u)];
    let _e80 = (*uv);
    let _e81 = textureSample(wired_bindless_images[(_e65 & 4095u)], wired_bindless_samplers[((_e75 >> bitcast<u32>(12i)) & 255u)], _e80);
    c_1 = _e81;
    let _e82 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e82))) == 0i) {
        let _e87 = c_1;
        param = _e87.xyz;
        let _e89 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e89.x;
        c_1[1u] = _e89.y;
        c_1[2u] = _e89.z;
    }
    let _e96 = (*slot);
    if (lightmap_slot == (_e96 + 1i)) {
        let _e101 = unnamed.worldLightParams[0u];
        let _e102 = c_1;
        let _e104 = (_e102.xyz * _e101);
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = c_1;
    return _e111;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var fogAmount: f32;
    var param_14: f32;

    let _e74 = frag_color0In_1;
    param_1 = _e74.xyz;
    let _e76 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e78 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e76.x, _e76.y, _e76.z, _e78);
    param_2 = 0u;
    let _e83 = frag_tex_coord0_1;
    param_3 = _e83;
    param_4 = 0i;
    let _e84 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e85 = frag_color0_;
    color0_ = (_e84 * _e85);
    if override_type_4_ {
        param_5 = 1u;
        let _e87 = frag_tex_coord1_1;
        param_6 = _e87;
        param_7 = 1i;
        let _e88 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e88;
        let _e89 = color0_;
        let _e91 = color1_;
        let _e93 = (_e89.xyz + _e91.xyz);
        let _e95 = color0_[3u];
        let _e97 = color1_[3u];
        base = vec4<f32>(_e93.x, _e93.y, _e93.z, (_e95 * _e97));
    } else {
        if override_type_4_1 {
            param_8 = 1u;
            let _e103 = frag_tex_coord1_1;
            param_9 = _e103;
            param_10 = 1i;
            let _e104 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            let _e105 = frag_color0_;
            color1_1 = (_e104 * _e105);
            let _e107 = color0_;
            let _e109 = color1_1;
            let _e111 = (_e107.xyz + _e109.xyz);
            let _e113 = color0_[3u];
            let _e115 = color1_1[3u];
            base = vec4<f32>(_e111.x, _e111.y, _e111.z, (_e113 * _e115));
        } else {
            param_11 = 1u;
            let _e121 = frag_tex_coord1_1;
            param_12 = _e121;
            param_13 = 1i;
            let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e122;
            let _e123 = color0_;
            let _e125 = color1_2;
            let _e127 = (_e123.xyz * _e125.xyz);
            base[0u] = _e127.x;
            base[1u] = _e127.y;
            base[2u] = _e127.z;
            let _e135 = color0_[3u];
            let _e137 = color1_2[3u];
            base[3u] = (_e135 * _e137);
        }
    }
    let _e140 = wired_advanced_fog_enabled_u0028_();
    if _e140 {
        let _e141 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e141;
        let _e142 = base;
        let _e145 = unnamed.advancedFogColorDensity;
        let _e147 = fogAmount;
        let _e149 = mix(_e142.xyz, _e145.xyz, vec3(_e147));
        base[0u] = _e149.x;
        base[1u] = _e149.y;
        base[2u] = _e149.z;
    }
    if override_type_4_2 {
        let _e157 = base[3u];
        if (_e157 == 0f) {
            discard;
        }
    } else {
        if override_type_4_3 {
            let _e159 = base;
            let _e161 = base;
            if (dot(_e159.xyz, _e161.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e165 = base;
    out_color = _e165;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
