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

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
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

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e67 = (*value);
    let _e68 = (*value);
    let _e70 = all((_e67 == _e68));
    phi_75_ = _e70;
    if _e70 {
        let _e71 = (*value);
        phi_75_ = all((abs(_e71) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e76 = phi_75_;
    return _e76;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e67 = (*value_1);
    let _e68 = (*value_1);
    let _e70 = all((_e67 == _e68));
    phi_60_ = _e70;
    if _e70 {
        let _e71 = (*value_1);
        phi_60_ = all((abs(_e71) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e76 = phi_60_;
    return _e76;
}

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_98_: bool;
    var phi_107_: bool;
    var phi_117_: bool;
    var phi_124_: bool;
    var phi_153_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e77 = temporalOutcome_1;
    let _e78 = (_e77 != 1u);
    phi_98_ = _e78;
    if !(_e78) {
        let _e80 = temporalCurrentClip_1;
        param = _e80;
        let _e81 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e81);
    }
    let _e84 = phi_98_;
    phi_107_ = _e84;
    if !(_e84) {
        let _e86 = temporalPreviousClip_1;
        param_1 = _e86;
        let _e87 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e87);
    }
    let _e90 = phi_107_;
    phi_117_ = _e90;
    if !(_e90) {
        let _e93 = temporalCurrentClip_1[3u];
        phi_117_ = (_e93 <= 0.000001f);
    }
    let _e96 = phi_117_;
    phi_124_ = _e96;
    if !(_e96) {
        let _e99 = temporalPreviousClip_1[3u];
        phi_124_ = (_e99 <= 0.000001f);
    }
    let _e102 = phi_124_;
    if _e102 {
        return;
    }
    let _e103 = temporalCurrentClip_1;
    let _e106 = temporalCurrentClip_1[3u];
    currentNdc = (_e103.xy / vec2(_e106));
    let _e109 = temporalPreviousClip_1;
    let _e112 = temporalPreviousClip_1[3u];
    previousNdc = (_e109.xy / vec2(_e112));
    let _e115 = currentNdc;
    param_2 = _e115;
    let _e116 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e117 = !(_e116);
    phi_153_ = _e117;
    if !(_e117) {
        let _e119 = previousNdc;
        param_3 = _e119;
        let _e120 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e120);
    }
    let _e123 = phi_153_;
    if _e123 {
        return;
    }
    let _e124 = currentNdc;
    currentUv = ((_e124 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e127 = previousNdc;
    previousUv = ((_e127 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e130 = currentUv;
    let _e131 = previousUv;
    velocity = (_e130 - _e131);
    let _e133 = velocity;
    param_4 = _e133;
    let _e134 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e134) {
        return;
    }
    let _e136 = velocity;
    out_temporal_velocity = _e136;
    let _e137 = (*coverageConfidence);
    out_temporal_validity = clamp(_e137, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e69 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e69 + 0.5f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e76 = fogType;
    let _e79 = fogType;
    return (((_e74 > 0.5f) && (_e76 >= 1i)) && (_e79 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e69 = wired_advanced_fog_enabled_u0028_();
    if !(_e69) {
        return 0f;
    }
    let _e72 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e72, 0.000001f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e77 + 0.5f));
    let _e80 = fogType_1;
    if (_e80 == 1i) {
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e84 <= 0f) {
            return 0f;
        }
        let _e86 = viewDepth;
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e86 / _e89), 0f, 1f);
    }
    let _e94 = unnamed.advancedFogColorDensity[3u];
    let _e96 = viewDepth;
    opticalDepth = (max(_e94, 0f) * _e96);
    let _e98 = fogType_1;
    if (_e98 == 2i) {
        let _e100 = opticalDepth;
        return clamp((1f - exp(-(_e100))), 0f, 1f);
    }
    let _e105 = opticalDepth;
    let _e106 = opticalDepth;
    return clamp((1f - exp(-((_e105 * _e106)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e70 = (*c);
    (*c) = max(_e70, vec3<f32>(0f, 0f, 0f));
    let _e72 = (*c);
    cutoff = (_e72 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e74 = (*c);
    lo = (_e74 / vec3(12.92f));
    let _e77 = (*c);
    hi = pow(((_e77 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e82 = hi;
    let _e83 = lo;
    let _e84 = cutoff;
    return mix(_e82, _e83, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e84));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_1 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_1;
        param_5 = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e114 = unnamed.worldLightParams[0u];
        let _e115 = c_1;
        let _e117 = (_e115.xyz * _e114);
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = c_1;
    return _e124;
}

fn main_1() {
    var color0_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var color1_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_2: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_18: f32;

    param_6 = 0u;
    let _e88 = frag_tex_coord0_1;
    param_7 = _e88;
    param_8 = 0i;
    let _e89 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    color0_ = _e89;
    if override_type_3_ {
        param_9 = 1u;
        let _e90 = frag_tex_coord1_1;
        param_10 = _e90;
        param_11 = 1i;
        let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e91;
        let _e92 = color0_;
        let _e94 = color1_;
        let _e96 = (_e92.xyz + _e94.xyz);
        let _e98 = color0_[3u];
        let _e100 = color1_[3u];
        base = vec4<f32>(_e96.x, _e96.y, _e96.z, (_e98 * _e100));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e106 = frag_tex_coord1_1;
            param_13 = _e106;
            param_14 = 1i;
            let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            color1_1 = _e107;
            let _e108 = color0_;
            let _e110 = color1_1;
            let _e112 = (_e108.xyz + _e110.xyz);
            let _e114 = color0_[3u];
            let _e116 = color1_1[3u];
            base = vec4<f32>(_e112.x, _e112.y, _e112.z, (_e114 * _e116));
        } else {
            param_15 = 1u;
            let _e122 = frag_tex_coord1_1;
            param_16 = _e122;
            param_17 = 1i;
            let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            color1_2 = _e123;
            let _e124 = color0_;
            let _e126 = color1_2;
            let _e128 = (_e124.xyz * _e126.xyz);
            base[0u] = _e128.x;
            base[1u] = _e128.y;
            base[2u] = _e128.z;
            let _e136 = color0_[3u];
            let _e138 = color1_2[3u];
            base[3u] = (_e136 * _e138);
        }
    }
    if override_type_3_2 {
        let _e143 = unnamed.worldLightParams[1u];
        wetness = clamp(_e143, 0f, 1f);
        let _e147 = unnamed.worldLightParams[2u];
        frost = clamp(_e147, 0f, 1f);
        let _e149 = base;
        luminance = dot(_e149.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e152 = wetness;
        let _e154 = base;
        let _e156 = (_e154.xyz * mix(1f, 0.82f, _e152));
        base[0u] = _e156.x;
        base[1u] = _e156.y;
        base[2u] = _e156.z;
        let _e163 = base;
        let _e165 = luminance;
        let _e167 = luminance;
        let _e169 = luminance;
        let _e171 = frost;
        let _e174 = mix(_e163.xyz, vec3<f32>((_e165 * 0.88f), (_e167 * 0.94f), _e169), vec3((_e171 * 0.55f)));
        base[0u] = _e174.x;
        base[1u] = _e174.y;
        base[2u] = _e174.z;
    }
    let _e181 = color0_;
    let _e184 = unnamed.emissionRadiance;
    let _e187 = base;
    let _e189 = (_e187.xyz + (_e181.xyz * _e184.xyz));
    base[0u] = _e189.x;
    base[1u] = _e189.y;
    base[2u] = _e189.z;
    let _e196 = wired_advanced_fog_enabled_u0028_();
    if _e196 {
        let _e197 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e197;
        let _e198 = base;
        let _e201 = unnamed.advancedFogColorDensity;
        let _e203 = fogAmount;
        let _e205 = mix(_e198.xyz, _e201.xyz, vec3(_e203));
        base[0u] = _e205.x;
        base[1u] = _e205.y;
        base[2u] = _e205.z;
    }
    if override_type_3_3 {
        let _e213 = base[3u];
        if (_e213 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e215 = base;
            let _e217 = base;
            if (dot(_e215.xyz, _e217.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e221 = base;
    out_color = _e221;
    param_18 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_18));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
