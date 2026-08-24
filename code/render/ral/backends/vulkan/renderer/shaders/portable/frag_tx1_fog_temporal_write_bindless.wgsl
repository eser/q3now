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
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
@id(10) override acff: i32 = 0i;
override override_type_3_2: bool = (acff == 1i);
override override_type_3_3: bool = (acff == 2i);
override override_type_3_4: bool = (acff == 3i);
override override_type_3_5: bool = (acff == 1i);
override override_type_3_6: bool = (acff == 2i);
override override_type_3_7: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_8: bool = (discard_mode == 1i);
override override_type_3_9: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
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
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var color0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color1_2: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var fogAmount: f32;
    var param_19: f32;

    let _e91 = unnamed.packed_indices[0i][3u];
    let _e97 = unnamed.packed_indices[0i][3u];
    let _e102 = fog_tex_coord_1;
    let _e103 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    fog = _e103;
    let _e104 = frag_color0In_1;
    param_6 = _e104.xyz;
    let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e108 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e106.x, _e106.y, _e106.z, _e108);
    param_7 = 0u;
    let _e113 = frag_tex_coord0_1;
    param_8 = _e113;
    param_9 = 0i;
    let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    let _e115 = frag_color0_;
    color0_ = (_e114 * _e115);
    if override_type_3_ {
        param_10 = 1u;
        let _e117 = frag_tex_coord1_1;
        param_11 = _e117;
        param_12 = 1i;
        let _e118 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        color1_ = _e118;
        let _e119 = color0_;
        let _e121 = color1_;
        let _e123 = (_e119.xyz + _e121.xyz);
        let _e125 = color0_[3u];
        let _e127 = color1_[3u];
        base = vec4<f32>(_e123.x, _e123.y, _e123.z, (_e125 * _e127));
    } else {
        if override_type_3_1 {
            param_13 = 1u;
            let _e133 = frag_tex_coord1_1;
            param_14 = _e133;
            param_15 = 1i;
            let _e134 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e135 = frag_color0_;
            color1_1 = (_e134 * _e135);
            let _e137 = color0_;
            let _e139 = color1_1;
            let _e141 = (_e137.xyz + _e139.xyz);
            let _e143 = color0_[3u];
            let _e145 = color1_1[3u];
            base = vec4<f32>(_e141.x, _e141.y, _e141.z, (_e143 * _e145));
        } else {
            param_16 = 1u;
            let _e151 = frag_tex_coord1_1;
            param_17 = _e151;
            param_18 = 1i;
            let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            color1_2 = _e152;
            let _e153 = color0_;
            let _e155 = color1_2;
            let _e157 = (_e153.xyz * _e155.xyz);
            base[0u] = _e157.x;
            base[1u] = _e157.y;
            base[2u] = _e157.z;
            let _e165 = color0_[3u];
            let _e167 = color1_2[3u];
            base[3u] = (_e165 * _e167);
        }
    }
    let _e170 = wired_advanced_fog_enabled_u0028_();
    if _e170 {
        let _e171 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e171;
        if override_type_3_2 {
            let _e172 = fogAmount;
            let _e174 = base;
            let _e176 = (_e174.xyz * (1f - _e172));
            base[0u] = _e176.x;
            base[1u] = _e176.y;
            base[2u] = _e176.z;
        } else {
            if override_type_3_3 {
                let _e183 = fogAmount;
                let _e185 = base;
                base = (_e185 * (1f - _e183));
            } else {
                if override_type_3_4 {
                    let _e187 = fogAmount;
                    let _e190 = base[3u];
                    base[3u] = (_e190 * (1f - _e187));
                } else {
                    let _e193 = base;
                    let _e196 = unnamed.advancedFogColorDensity;
                    let _e198 = fogAmount;
                    let _e200 = mix(_e193.xyz, _e196.xyz, vec3(_e198));
                    base[0u] = _e200.x;
                    base[1u] = _e200.y;
                    base[2u] = _e200.z;
                }
            }
        }
    } else {
        if override_type_3_5 {
            let _e207 = base;
            let _e210 = fog[3u];
            let _e212 = (_e207.xyz * (1f - _e210));
            base[0u] = _e212.x;
            base[1u] = _e212.y;
            base[2u] = _e212.z;
        } else {
            if override_type_3_6 {
                let _e219 = base;
                let _e221 = fog[3u];
                base = (_e219 * (1f - _e221));
            } else {
                if override_type_3_7 {
                    let _e225 = base[3u];
                    let _e227 = fog[3u];
                    base[3u] = (_e225 * (1f - _e227));
                } else {
                    let _e231 = base;
                    let _e232 = fog;
                    let _e234 = unnamed.fogColor;
                    let _e237 = fog[3u];
                    base = mix(_e231, (_e232 * _e234), vec4(_e237));
                }
            }
        }
    }
    if override_type_3_8 {
        let _e241 = base[3u];
        if (_e241 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e243 = base;
            let _e245 = base;
            if (dot(_e243.xyz, _e245.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e249 = base;
    out_color = _e249;
    param_19 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_19));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
