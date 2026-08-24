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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e66 = (*value);
    let _e67 = (*value);
    let _e69 = all((_e66 == _e67));
    phi_75_ = _e69;
    if _e69 {
        let _e70 = (*value);
        phi_75_ = all((abs(_e70) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e75 = phi_75_;
    return _e75;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e66 = (*value_1);
    let _e67 = (*value_1);
    let _e69 = all((_e66 == _e67));
    phi_60_ = _e69;
    if _e69 {
        let _e70 = (*value_1);
        phi_60_ = all((abs(_e70) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e75 = phi_60_;
    return _e75;
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
    let _e76 = temporalOutcome_1;
    let _e77 = (_e76 != 1u);
    phi_98_ = _e77;
    if !(_e77) {
        let _e79 = temporalCurrentClip_1;
        param = _e79;
        let _e80 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e80);
    }
    let _e83 = phi_98_;
    phi_107_ = _e83;
    if !(_e83) {
        let _e85 = temporalPreviousClip_1;
        param_1 = _e85;
        let _e86 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e86);
    }
    let _e89 = phi_107_;
    phi_117_ = _e89;
    if !(_e89) {
        let _e92 = temporalCurrentClip_1[3u];
        phi_117_ = (_e92 <= 0.000001f);
    }
    let _e95 = phi_117_;
    phi_124_ = _e95;
    if !(_e95) {
        let _e98 = temporalPreviousClip_1[3u];
        phi_124_ = (_e98 <= 0.000001f);
    }
    let _e101 = phi_124_;
    if _e101 {
        return;
    }
    let _e102 = temporalCurrentClip_1;
    let _e105 = temporalCurrentClip_1[3u];
    currentNdc = (_e102.xy / vec2(_e105));
    let _e108 = temporalPreviousClip_1;
    let _e111 = temporalPreviousClip_1[3u];
    previousNdc = (_e108.xy / vec2(_e111));
    let _e114 = currentNdc;
    param_2 = _e114;
    let _e115 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e116 = !(_e115);
    phi_153_ = _e116;
    if !(_e116) {
        let _e118 = previousNdc;
        param_3 = _e118;
        let _e119 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e119);
    }
    let _e122 = phi_153_;
    if _e122 {
        return;
    }
    let _e123 = currentNdc;
    currentUv = ((_e123 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e126 = previousNdc;
    previousUv = ((_e126 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e129 = currentUv;
    let _e130 = previousUv;
    velocity = (_e129 - _e130);
    let _e132 = velocity;
    param_4 = _e132;
    let _e133 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e133) {
        return;
    }
    let _e135 = velocity;
    out_temporal_velocity = _e135;
    let _e136 = (*coverageConfidence);
    out_temporal_validity = clamp(_e136, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e68 + 0.5f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e75 = fogType;
    let _e78 = fogType;
    return (((_e73 > 0.5f) && (_e75 >= 1i)) && (_e78 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e68 = wired_advanced_fog_enabled_u0028_();
    if !(_e68) {
        return 0f;
    }
    let _e71 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e71, 0.000001f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e76 + 0.5f));
    let _e79 = fogType_1;
    if (_e79 == 1i) {
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e83 <= 0f) {
            return 0f;
        }
        let _e85 = viewDepth;
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e85 / _e88), 0f, 1f);
    }
    let _e93 = unnamed.advancedFogColorDensity[3u];
    let _e95 = viewDepth;
    opticalDepth = (max(_e93, 0f) * _e95);
    let _e97 = fogType_1;
    if (_e97 == 2i) {
        let _e99 = opticalDepth;
        return clamp((1f - exp(-(_e99))), 0f, 1f);
    }
    let _e104 = opticalDepth;
    let _e105 = opticalDepth;
    return clamp((1f - exp(-((_e104 * _e105)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c);
    (*c) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e92 = (*uv);
    let _e93 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    c_1 = _e93;
    let _e94 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e94))) == 0i) {
        let _e99 = c_1;
        param_5 = _e99.xyz;
        let _e101 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e101.x;
        c_1[1u] = _e101.y;
        c_1[2u] = _e101.z;
    }
    let _e108 = (*slot);
    if (lightmap_slot == (_e108 + 1i)) {
        let _e113 = unnamed.worldLightParams[0u];
        let _e114 = c_1;
        let _e116 = (_e114.xyz * _e113);
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = c_1;
    return _e123;
}

fn main_1() {
    var fog: vec4<f32>;
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
    var fogAmount: f32;
    var param_18: f32;

    let _e88 = unnamed.packed_indices[0i][3u];
    let _e94 = unnamed.packed_indices[0i][3u];
    let _e99 = fog_tex_coord_1;
    let _e100 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    fog = _e100;
    param_6 = 0u;
    let _e101 = frag_tex_coord0_1;
    param_7 = _e101;
    param_8 = 0i;
    let _e102 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    color0_ = _e102;
    if override_type_3_ {
        param_9 = 1u;
        let _e103 = frag_tex_coord1_1;
        param_10 = _e103;
        param_11 = 1i;
        let _e104 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color1_ = _e104;
        let _e105 = color0_;
        let _e107 = color1_;
        let _e109 = (_e105.xyz + _e107.xyz);
        let _e111 = color0_[3u];
        let _e113 = color1_[3u];
        base = vec4<f32>(_e109.x, _e109.y, _e109.z, (_e111 * _e113));
    } else {
        if override_type_3_1 {
            param_12 = 1u;
            let _e119 = frag_tex_coord1_1;
            param_13 = _e119;
            param_14 = 1i;
            let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            color1_1 = _e120;
            let _e121 = color0_;
            let _e123 = color1_1;
            let _e125 = (_e121.xyz + _e123.xyz);
            let _e127 = color0_[3u];
            let _e129 = color1_1[3u];
            base = vec4<f32>(_e125.x, _e125.y, _e125.z, (_e127 * _e129));
        } else {
            param_15 = 1u;
            let _e135 = frag_tex_coord1_1;
            param_16 = _e135;
            param_17 = 1i;
            let _e136 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            color1_2 = _e136;
            let _e137 = color0_;
            let _e139 = color1_2;
            let _e141 = (_e137.xyz * _e139.xyz);
            base[0u] = _e141.x;
            base[1u] = _e141.y;
            base[2u] = _e141.z;
            let _e149 = color0_[3u];
            let _e151 = color1_2[3u];
            base[3u] = (_e149 * _e151);
        }
    }
    let _e154 = wired_advanced_fog_enabled_u0028_();
    if _e154 {
        let _e155 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e155;
        if override_type_3_2 {
            let _e156 = fogAmount;
            let _e158 = base;
            let _e160 = (_e158.xyz * (1f - _e156));
            base[0u] = _e160.x;
            base[1u] = _e160.y;
            base[2u] = _e160.z;
        } else {
            if override_type_3_3 {
                let _e167 = fogAmount;
                let _e169 = base;
                base = (_e169 * (1f - _e167));
            } else {
                if override_type_3_4 {
                    let _e171 = fogAmount;
                    let _e174 = base[3u];
                    base[3u] = (_e174 * (1f - _e171));
                } else {
                    let _e177 = base;
                    let _e180 = unnamed.advancedFogColorDensity;
                    let _e182 = fogAmount;
                    let _e184 = mix(_e177.xyz, _e180.xyz, vec3(_e182));
                    base[0u] = _e184.x;
                    base[1u] = _e184.y;
                    base[2u] = _e184.z;
                }
            }
        }
    } else {
        if override_type_3_5 {
            let _e191 = base;
            let _e194 = fog[3u];
            let _e196 = (_e191.xyz * (1f - _e194));
            base[0u] = _e196.x;
            base[1u] = _e196.y;
            base[2u] = _e196.z;
        } else {
            if override_type_3_6 {
                let _e203 = base;
                let _e205 = fog[3u];
                base = (_e203 * (1f - _e205));
            } else {
                if override_type_3_7 {
                    let _e209 = base[3u];
                    let _e211 = fog[3u];
                    base[3u] = (_e209 * (1f - _e211));
                } else {
                    let _e215 = base;
                    let _e216 = fog;
                    let _e218 = unnamed.fogColor;
                    let _e221 = fog[3u];
                    base = mix(_e215, (_e216 * _e218), vec4(_e221));
                }
            }
        }
    }
    if override_type_3_8 {
        let _e225 = base[3u];
        if (_e225 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e227 = base;
            let _e229 = base;
            if (dot(_e227.xyz, _e229.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e233 = base;
    out_color = _e233;
    param_18 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_18));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
