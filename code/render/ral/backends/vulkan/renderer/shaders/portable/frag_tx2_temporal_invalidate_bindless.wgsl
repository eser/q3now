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
var<private> frag_tex_coord2_1: vec2<f32>;
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

    let _e57 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e57 + 0.5f));
    let _e62 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e64 = fogType;
    let _e67 = fogType;
    return (((_e62 > 0.5f) && (_e64 >= 1i)) && (_e67 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e57 = wired_advanced_fog_enabled_u0028_();
    if !(_e57) {
        return 0f;
    }
    let _e60 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e60, 0.000001f));
    let _e65 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e65 + 0.5f));
    let _e68 = fogType_1;
    if (_e68 == 1i) {
        let _e72 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e72 <= 0f) {
            return 0f;
        }
        let _e74 = viewDepth;
        let _e77 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e74 / _e77), 0f, 1f);
    }
    let _e82 = unnamed.advancedFogColorDensity[3u];
    let _e84 = viewDepth;
    opticalDepth = (max(_e82, 0f) * _e84);
    let _e86 = fogType_1;
    if (_e86 == 2i) {
        let _e88 = opticalDepth;
        return clamp((1f - exp(-(_e88))), 0f, 1f);
    }
    let _e93 = opticalDepth;
    let _e94 = opticalDepth;
    return clamp((1f - exp(-((_e93 * _e94)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e58 = (*c);
    (*c) = max(_e58, vec3<f32>(0f, 0f, 0f));
    let _e60 = (*c);
    cutoff = (_e60 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e62 = (*c);
    lo = (_e62 / vec3(12.92f));
    let _e65 = (*c);
    hi = pow(((_e65 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e70 = hi;
    let _e71 = lo;
    let _e72 = cutoff;
    return mix(_e70, _e71, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e72));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e59 = (*role);
    let _e61 = (*role);
    let _e66 = unnamed.packed_indices[(_e59 / 4u)][(_e61 % 4u)];
    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e81 = (*uv);
    let _e82 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e76 >> bitcast<u32>(12i)) & 255u)], _e81);
    c_1 = _e82;
    let _e83 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e83))) == 0i) {
        let _e88 = c_1;
        param = _e88.xyz;
        let _e90 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e90.x;
        c_1[1u] = _e90.y;
        c_1[2u] = _e90.z;
    }
    let _e97 = (*slot);
    if (lightmap_slot == (_e97 + 1i)) {
        let _e102 = unnamed.worldLightParams[0u];
        let _e103 = c_1;
        let _e105 = (_e103.xyz * _e102);
        c_1[0u] = _e105.x;
        c_1[1u] = _e105.y;
        c_1[2u] = _e105.z;
    }
    let _e112 = c_1;
    return _e112;
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
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var fogAmount: f32;
    var param_23: f32;

    let _e87 = frag_color0In_1;
    param_1 = _e87.xyz;
    let _e89 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e91 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e89.x, _e89.y, _e89.z, _e91);
    param_2 = 0u;
    let _e96 = frag_tex_coord0_1;
    param_3 = _e96;
    param_4 = 0i;
    let _e97 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e98 = frag_color0_;
    color0_ = (_e97 * _e98);
    if override_type_4_ {
        param_5 = 1u;
        let _e100 = frag_tex_coord1_1;
        param_6 = _e100;
        param_7 = 1i;
        let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e101;
        param_8 = 2u;
        let _e102 = frag_tex_coord2_1;
        param_9 = _e102;
        param_10 = 2i;
        let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e103;
        let _e104 = color0_;
        let _e106 = color1_;
        let _e109 = color2_;
        let _e111 = ((_e104.xyz + _e106.xyz) + _e109.xyz);
        let _e113 = color0_[3u];
        let _e115 = color1_[3u];
        let _e118 = color2_[3u];
        base = vec4<f32>(_e111.x, _e111.y, _e111.z, ((_e113 * _e115) * _e118));
    } else {
        if override_type_4_1 {
            param_11 = 1u;
            let _e124 = frag_tex_coord1_1;
            param_12 = _e124;
            param_13 = 1i;
            let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e126 = frag_color0_;
            color1_1 = (_e125 * _e126);
            param_14 = 2u;
            let _e128 = frag_tex_coord2_1;
            param_15 = _e128;
            param_16 = 2i;
            let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e130 = frag_color0_;
            color2_1 = (_e129 * _e130);
            let _e132 = color0_;
            let _e134 = color1_1;
            let _e137 = color2_1;
            let _e139 = ((_e132.xyz + _e134.xyz) + _e137.xyz);
            let _e141 = color0_[3u];
            let _e143 = color1_1[3u];
            let _e146 = color2_1[3u];
            base = vec4<f32>(_e139.x, _e139.y, _e139.z, ((_e141 * _e143) * _e146));
        } else {
            param_17 = 1u;
            let _e152 = frag_tex_coord1_1;
            param_18 = _e152;
            param_19 = 1i;
            let _e153 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e153;
            param_20 = 2u;
            let _e154 = frag_tex_coord2_1;
            param_21 = _e154;
            param_22 = 2i;
            let _e155 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e155;
            let _e156 = color0_;
            let _e158 = color1_2;
            let _e161 = color2_2;
            let _e163 = ((_e156.xyz * _e158.xyz) * _e161.xyz);
            base[0u] = _e163.x;
            base[1u] = _e163.y;
            base[2u] = _e163.z;
            let _e171 = color0_[3u];
            let _e173 = color1_2[3u];
            let _e176 = color2_2[3u];
            base[3u] = ((_e171 * _e173) * _e176);
        }
    }
    let _e179 = wired_advanced_fog_enabled_u0028_();
    if _e179 {
        let _e180 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e180;
        let _e181 = base;
        let _e184 = unnamed.advancedFogColorDensity;
        let _e186 = fogAmount;
        let _e188 = mix(_e181.xyz, _e184.xyz, vec3(_e186));
        base[0u] = _e188.x;
        base[1u] = _e188.y;
        base[2u] = _e188.z;
    }
    if override_type_4_2 {
        let _e196 = base[3u];
        if (_e196 == 0f) {
            discard;
        }
    } else {
        if override_type_4_3 {
            let _e198 = base;
            let _e200 = base;
            if (dot(_e198.xyz, _e200.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e204 = base;
    out_color = _e204;
    param_23 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_23));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
