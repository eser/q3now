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

    let _e55 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e55 + 0.5f));
    let _e60 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e62 = fogType;
    let _e65 = fogType;
    return (((_e60 > 0.5f) && (_e62 >= 1i)) && (_e65 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e55 = wired_advanced_fog_enabled_u0028_();
    if !(_e55) {
        return 0f;
    }
    let _e58 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e58, 0.000001f));
    let _e63 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e63 + 0.5f));
    let _e66 = fogType_1;
    if (_e66 == 1i) {
        let _e70 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e70 <= 0f) {
            return 0f;
        }
        let _e72 = viewDepth;
        let _e75 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e72 / _e75), 0f, 1f);
    }
    let _e80 = unnamed.advancedFogColorDensity[3u];
    let _e82 = viewDepth;
    opticalDepth = (max(_e80, 0f) * _e82);
    let _e84 = fogType_1;
    if (_e84 == 2i) {
        let _e86 = opticalDepth;
        return clamp((1f - exp(-(_e86))), 0f, 1f);
    }
    let _e91 = opticalDepth;
    let _e92 = opticalDepth;
    return clamp((1f - exp(-((_e91 * _e92)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e56 = (*c);
    (*c) = max(_e56, vec3<f32>(0f, 0f, 0f));
    let _e58 = (*c);
    cutoff = (_e58 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e60 = (*c);
    lo = (_e60 / vec3(12.92f));
    let _e63 = (*c);
    hi = pow(((_e63 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e68 = hi;
    let _e69 = lo;
    let _e70 = cutoff;
    return mix(_e68, _e69, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e70));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e57 = (*role);
    let _e59 = (*role);
    let _e64 = unnamed.packed_indices[(_e57 / 4u)][(_e59 % 4u)];
    let _e67 = (*role);
    let _e69 = (*role);
    let _e74 = unnamed.packed_indices[(_e67 / 4u)][(_e69 % 4u)];
    let _e79 = (*uv);
    let _e80 = textureSample(wired_bindless_images[(_e64 & 4095u)], wired_bindless_samplers[((_e74 >> bitcast<u32>(12i)) & 255u)], _e79);
    c_1 = _e80;
    let _e81 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e81))) == 0i) {
        let _e86 = c_1;
        param = _e86.xyz;
        let _e88 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e88.x;
        c_1[1u] = _e88.y;
        c_1[2u] = _e88.z;
    }
    let _e95 = (*slot);
    if (lightmap_slot == (_e95 + 1i)) {
        let _e100 = unnamed.worldLightParams[0u];
        let _e101 = c_1;
        let _e103 = (_e101.xyz * _e100);
        c_1[0u] = _e103.x;
        c_1[1u] = _e103.y;
        c_1[2u] = _e103.z;
    }
    let _e110 = c_1;
    return _e110;
}

fn main_1() {
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
    var fogAmount: f32;
    var param_13: f32;

    param_1 = 0u;
    let _e71 = frag_tex_coord0_1;
    param_2 = _e71;
    param_3 = 0i;
    let _e72 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e72;
    if override_type_4_ {
        param_4 = 1u;
        let _e73 = frag_tex_coord1_1;
        param_5 = _e73;
        param_6 = 1i;
        let _e74 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e74;
        let _e75 = color0_;
        let _e77 = color1_;
        let _e79 = (_e75.xyz + _e77.xyz);
        let _e81 = color0_[3u];
        let _e83 = color1_[3u];
        base = vec4<f32>(_e79.x, _e79.y, _e79.z, (_e81 * _e83));
    } else {
        if override_type_4_1 {
            param_7 = 1u;
            let _e89 = frag_tex_coord1_1;
            param_8 = _e89;
            param_9 = 1i;
            let _e90 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            color1_1 = _e90;
            let _e91 = color0_;
            let _e93 = color1_1;
            let _e95 = (_e91.xyz + _e93.xyz);
            let _e97 = color0_[3u];
            let _e99 = color1_1[3u];
            base = vec4<f32>(_e95.x, _e95.y, _e95.z, (_e97 * _e99));
        } else {
            param_10 = 1u;
            let _e105 = frag_tex_coord1_1;
            param_11 = _e105;
            param_12 = 1i;
            let _e106 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            color1_2 = _e106;
            let _e107 = color0_;
            let _e109 = color1_2;
            let _e111 = (_e107.xyz * _e109.xyz);
            base[0u] = _e111.x;
            base[1u] = _e111.y;
            base[2u] = _e111.z;
            let _e119 = color0_[3u];
            let _e121 = color1_2[3u];
            base[3u] = (_e119 * _e121);
        }
    }
    let _e124 = wired_advanced_fog_enabled_u0028_();
    if _e124 {
        let _e125 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e125;
        let _e126 = base;
        let _e129 = unnamed.advancedFogColorDensity;
        let _e131 = fogAmount;
        let _e133 = mix(_e126.xyz, _e129.xyz, vec3(_e131));
        base[0u] = _e133.x;
        base[1u] = _e133.y;
        base[2u] = _e133.z;
    }
    if override_type_4_2 {
        let _e141 = base[3u];
        if (_e141 == 0f) {
            discard;
        }
    } else {
        if override_type_4_3 {
            let _e143 = base;
            let _e145 = base;
            if (dot(_e143.xyz, _e145.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e149 = base;
    out_color = _e149;
    param_13 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_13));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
