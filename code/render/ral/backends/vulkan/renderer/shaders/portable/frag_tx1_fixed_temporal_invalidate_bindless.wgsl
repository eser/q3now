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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
    var fogAmount: f32;
    var param_13: f32;

    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e78 = frag_tex_coord0_1;
    param_2 = _e78;
    param_3 = 0i;
    let _e79 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e80 = frag_color;
    color0_ = (_e79 * _e80);
    if override_type_4_ {
        param_4 = 1u;
        let _e82 = frag_tex_coord1_1;
        param_5 = _e82;
        param_6 = 1i;
        let _e83 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e83;
        let _e84 = color0_;
        let _e86 = color1_;
        let _e88 = (_e84.xyz + _e86.xyz);
        let _e90 = color0_[3u];
        let _e92 = color1_[3u];
        base = vec4<f32>(_e88.x, _e88.y, _e88.z, (_e90 * _e92));
    } else {
        if override_type_4_1 {
            param_7 = 1u;
            let _e98 = frag_tex_coord1_1;
            param_8 = _e98;
            param_9 = 1i;
            let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e100 = frag_color;
            color1_1 = (_e99 * _e100);
            let _e102 = color0_;
            let _e104 = color1_1;
            let _e106 = (_e102.xyz + _e104.xyz);
            let _e108 = color0_[3u];
            let _e110 = color1_1[3u];
            base = vec4<f32>(_e106.x, _e106.y, _e106.z, (_e108 * _e110));
        } else {
            param_10 = 1u;
            let _e116 = frag_tex_coord1_1;
            param_11 = _e116;
            param_12 = 1i;
            let _e117 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e118 = frag_color;
            color1_2 = (_e117 * _e118);
            let _e120 = color0_;
            let _e122 = color1_2;
            let _e124 = (_e120.xyz * _e122.xyz);
            base[0u] = _e124.x;
            base[1u] = _e124.y;
            base[2u] = _e124.z;
            let _e132 = color0_[3u];
            let _e134 = color1_2[3u];
            base[3u] = (_e132 * _e134);
        }
    }
    let _e137 = wired_advanced_fog_enabled_u0028_();
    if _e137 {
        let _e138 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e138;
        let _e139 = base;
        let _e142 = unnamed.advancedFogColorDensity;
        let _e144 = fogAmount;
        let _e146 = mix(_e139.xyz, _e142.xyz, vec3(_e144));
        base[0u] = _e146.x;
        base[1u] = _e146.y;
        base[2u] = _e146.z;
    }
    if override_type_4_2 {
        let _e154 = base[3u];
        if (_e154 == 0f) {
            discard;
        }
    } else {
        if override_type_4_3 {
            let _e156 = base;
            let _e158 = base;
            if (dot(_e156.xyz, _e158.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e162 = base;
    out_color = _e162;
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
