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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e71 = (*value);
    let _e72 = (*value);
    let _e74 = all((_e71 == _e72));
    phi_75_ = _e74;
    if _e74 {
        let _e75 = (*value);
        phi_75_ = all((abs(_e75) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e80 = phi_75_;
    return _e80;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e71 = (*value_1);
    let _e72 = (*value_1);
    let _e74 = all((_e71 == _e72));
    phi_60_ = _e74;
    if _e74 {
        let _e75 = (*value_1);
        phi_60_ = all((abs(_e75) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e80 = phi_60_;
    return _e80;
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
    let _e81 = temporalOutcome_1;
    let _e82 = (_e81 != 1u);
    phi_98_ = _e82;
    if !(_e82) {
        let _e84 = temporalCurrentClip_1;
        param = _e84;
        let _e85 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e85);
    }
    let _e88 = phi_98_;
    phi_107_ = _e88;
    if !(_e88) {
        let _e90 = temporalPreviousClip_1;
        param_1 = _e90;
        let _e91 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e91);
    }
    let _e94 = phi_107_;
    phi_117_ = _e94;
    if !(_e94) {
        let _e97 = temporalCurrentClip_1[3u];
        phi_117_ = (_e97 <= 0.000001f);
    }
    let _e100 = phi_117_;
    phi_124_ = _e100;
    if !(_e100) {
        let _e103 = temporalPreviousClip_1[3u];
        phi_124_ = (_e103 <= 0.000001f);
    }
    let _e106 = phi_124_;
    if _e106 {
        return;
    }
    let _e107 = temporalCurrentClip_1;
    let _e110 = temporalCurrentClip_1[3u];
    currentNdc = (_e107.xy / vec2(_e110));
    let _e113 = temporalPreviousClip_1;
    let _e116 = temporalPreviousClip_1[3u];
    previousNdc = (_e113.xy / vec2(_e116));
    let _e119 = currentNdc;
    param_2 = _e119;
    let _e120 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e121 = !(_e120);
    phi_153_ = _e121;
    if !(_e121) {
        let _e123 = previousNdc;
        param_3 = _e123;
        let _e124 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e124);
    }
    let _e127 = phi_153_;
    if _e127 {
        return;
    }
    let _e128 = currentNdc;
    currentUv = ((_e128 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e131 = previousNdc;
    previousUv = ((_e131 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e134 = currentUv;
    let _e135 = previousUv;
    velocity = (_e134 - _e135);
    let _e137 = velocity;
    param_4 = _e137;
    let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e138) {
        return;
    }
    let _e140 = velocity;
    out_temporal_velocity = _e140;
    let _e141 = (*coverageConfidence);
    out_temporal_validity = clamp(_e141, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param_5 = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e118 = unnamed.worldLightParams[0u];
        let _e119 = c_1;
        let _e121 = (_e119.xyz * _e118);
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = c_1;
    return _e128;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_6: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_8: vec3<f32>;
    var color0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color2_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color2_1: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_2: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var color2_2: vec4<f32>;
    var param_27: u32;
    var param_28: vec2<f32>;
    var param_29: i32;
    var color1_3: vec4<f32>;
    var param_30: u32;
    var param_31: vec2<f32>;
    var param_32: i32;
    var color2_3: vec4<f32>;
    var param_33: u32;
    var param_34: vec2<f32>;
    var param_35: i32;
    var color1_4: vec4<f32>;
    var param_36: u32;
    var param_37: vec2<f32>;
    var param_38: i32;
    var color2_4: vec4<f32>;
    var param_39: u32;
    var param_40: vec2<f32>;
    var param_41: i32;
    var color1_5: vec4<f32>;
    var param_42: u32;
    var param_43: vec2<f32>;
    var param_44: i32;
    var color2_5: vec4<f32>;
    var param_45: u32;
    var param_46: vec2<f32>;
    var param_47: i32;
    var color1_6: vec4<f32>;
    var param_48: u32;
    var param_49: vec2<f32>;
    var param_50: i32;
    var color2_6: vec4<f32>;
    var param_51: u32;
    var param_52: vec2<f32>;
    var param_53: i32;
    var fogAmount: f32;
    var param_54: f32;

    let _e139 = frag_color0In_1;
    param_6 = _e139.xyz;
    let _e141 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e143 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e141.x, _e141.y, _e141.z, _e143);
    let _e148 = frag_color1In_1;
    param_7 = _e148.xyz;
    let _e150 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e152 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e150.x, _e150.y, _e150.z, _e152);
    let _e157 = frag_color2In_1;
    param_8 = _e157.xyz;
    let _e159 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e161 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e159.x, _e159.y, _e159.z, _e161);
    param_9 = 0u;
    let _e166 = frag_tex_coord0_1;
    param_10 = _e166;
    param_11 = 0i;
    let _e167 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    let _e168 = frag_color0_;
    color0_ = (_e167 * _e168);
    if override_type_3_2 {
        param_12 = 1u;
        let _e170 = frag_tex_coord1_1;
        param_13 = _e170;
        param_14 = 1i;
        let _e171 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
        let _e172 = frag_color1_;
        color1_ = (_e171 * _e172);
        param_15 = 2u;
        let _e174 = frag_tex_coord2_1;
        param_16 = _e174;
        param_17 = 2i;
        let _e175 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        let _e176 = frag_color2_;
        color2_ = (_e175 * _e176);
        let _e178 = color0_;
        let _e180 = color1_;
        let _e183 = color2_;
        let _e185 = ((_e178.xyz + _e180.xyz) + _e183.xyz);
        let _e187 = color0_[3u];
        let _e189 = color1_[3u];
        let _e192 = color2_[3u];
        base = vec4<f32>(_e185.x, _e185.y, _e185.z, ((_e187 * _e189) * _e192));
    } else {
        if override_type_3_3 {
            param_18 = 1u;
            let _e198 = frag_tex_coord1_1;
            param_19 = _e198;
            param_20 = 1i;
            let _e199 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            let _e200 = frag_color1_;
            color1_1 = (_e199 * _e200);
            param_21 = 2u;
            let _e202 = frag_tex_coord2_1;
            param_22 = _e202;
            param_23 = 2i;
            let _e203 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            let _e204 = frag_color2_;
            color2_1 = (_e203 * _e204);
            let _e207 = color0_[3u];
            let _e208 = color0_;
            color0_ = (_e208 * _e207);
            let _e211 = color1_1[3u];
            let _e212 = color1_1;
            color1_1 = (_e212 * _e211);
            let _e215 = color2_1[3u];
            let _e216 = color2_1;
            color2_1 = (_e216 * _e215);
            let _e218 = color0_;
            let _e220 = color1_1;
            let _e223 = color2_1;
            let _e225 = ((_e218.xyz + _e220.xyz) + _e223.xyz);
            let _e227 = color0_[3u];
            let _e229 = color1_1[3u];
            let _e232 = color2_1[3u];
            base = vec4<f32>(_e225.x, _e225.y, _e225.z, ((_e227 * _e229) * _e232));
        } else {
            if override_type_3_4 {
                param_24 = 1u;
                let _e238 = frag_tex_coord1_1;
                param_25 = _e238;
                param_26 = 1i;
                let _e239 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                let _e240 = frag_color1_;
                color1_2 = (_e239 * _e240);
                param_27 = 2u;
                let _e242 = frag_tex_coord2_1;
                param_28 = _e242;
                param_29 = 2i;
                let _e243 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
                let _e244 = frag_color2_;
                color2_2 = (_e243 * _e244);
                let _e247 = color0_[3u];
                let _e249 = color0_;
                color0_ = (_e249 * (1f - _e247));
                let _e252 = color1_2[3u];
                let _e254 = color1_2;
                color1_2 = (_e254 * (1f - _e252));
                let _e257 = color2_2[3u];
                let _e259 = color2_2;
                color2_2 = (_e259 * (1f - _e257));
                let _e261 = color0_;
                let _e263 = color1_2;
                let _e266 = color2_2;
                let _e268 = ((_e261.xyz + _e263.xyz) + _e266.xyz);
                let _e270 = color0_[3u];
                let _e272 = color1_2[3u];
                let _e275 = color2_2[3u];
                base = vec4<f32>(_e268.x, _e268.y, _e268.z, ((_e270 * _e272) * _e275));
            } else {
                if override_type_3_5 {
                    param_30 = 1u;
                    let _e281 = frag_tex_coord1_1;
                    param_31 = _e281;
                    param_32 = 1i;
                    let _e282 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
                    let _e283 = frag_color1_;
                    color1_3 = (_e282 * _e283);
                    param_33 = 2u;
                    let _e285 = frag_tex_coord2_1;
                    param_34 = _e285;
                    param_35 = 2i;
                    let _e286 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_33), (&param_34), (&param_35));
                    let _e287 = frag_color2_;
                    color2_3 = (_e286 * _e287);
                    let _e289 = color0_;
                    let _e290 = color1_3;
                    let _e292 = color1_3[3u];
                    let _e295 = color2_3;
                    let _e297 = color2_3[3u];
                    base = mix(mix(_e289, _e290, vec4(_e292)), _e295, vec4(_e297));
                } else {
                    if override_type_3_6 {
                        param_36 = 1u;
                        let _e300 = frag_tex_coord1_1;
                        param_37 = _e300;
                        param_38 = 1i;
                        let _e301 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_36), (&param_37), (&param_38));
                        let _e302 = frag_color1_;
                        color1_4 = (_e301 * _e302);
                        param_39 = 2u;
                        let _e304 = frag_tex_coord2_1;
                        param_40 = _e304;
                        param_41 = 2i;
                        let _e305 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_39), (&param_40), (&param_41));
                        let _e306 = frag_color2_;
                        color2_4 = (_e305 * _e306);
                        let _e308 = color2_4;
                        let _e309 = color1_4;
                        let _e310 = color0_;
                        let _e312 = color1_4[3u];
                        let _e316 = color2_4[3u];
                        base = mix(_e308, mix(_e309, _e310, vec4(_e312)), vec4(_e316));
                    } else {
                        if override_type_3_7 {
                            param_42 = 1u;
                            let _e319 = frag_tex_coord1_1;
                            param_43 = _e319;
                            param_44 = 1i;
                            let _e320 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_42), (&param_43), (&param_44));
                            let _e321 = frag_color1_;
                            color1_5 = (_e320 * _e321);
                            param_45 = 2u;
                            let _e323 = frag_tex_coord2_1;
                            param_46 = _e323;
                            param_47 = 2i;
                            let _e324 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_45), (&param_46), (&param_47));
                            let _e325 = frag_color2_;
                            color2_5 = (_e324 * _e325);
                            let _e327 = color2_5;
                            let _e329 = color2_5[3u];
                            let _e332 = color1_5;
                            let _e334 = color1_5[3u];
                            let _e338 = color0_;
                            base = (((_e327 + vec4(_e329)) * (_e332 + vec4(_e334))) * _e338);
                        } else {
                            param_48 = 1u;
                            let _e340 = frag_tex_coord1_1;
                            param_49 = _e340;
                            param_50 = 1i;
                            let _e341 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_48), (&param_49), (&param_50));
                            let _e342 = frag_color1_;
                            color1_6 = (_e341 * _e342);
                            param_51 = 2u;
                            let _e344 = frag_tex_coord2_1;
                            param_52 = _e344;
                            param_53 = 2i;
                            let _e345 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_51), (&param_52), (&param_53));
                            let _e346 = frag_color2_;
                            color2_6 = (_e345 * _e346);
                            let _e348 = color0_;
                            let _e350 = color1_6;
                            let _e353 = color2_6;
                            let _e355 = ((_e348.xyz * _e350.xyz) * _e353.xyz);
                            base[0u] = _e355.x;
                            base[1u] = _e355.y;
                            base[2u] = _e355.z;
                            let _e363 = color0_[3u];
                            let _e365 = color1_6[3u];
                            let _e368 = color2_6[3u];
                            base[3u] = ((_e363 * _e365) * _e368);
                        }
                    }
                }
            }
        }
    }
    let _e371 = wired_advanced_fog_enabled_u0028_();
    if _e371 {
        let _e372 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e372;
        let _e373 = base;
        let _e376 = unnamed.advancedFogColorDensity;
        let _e378 = fogAmount;
        let _e380 = mix(_e373.xyz, _e376.xyz, vec3(_e378));
        base[0u] = _e380.x;
        base[1u] = _e380.y;
        base[2u] = _e380.z;
    }
    if override_type_3_8 {
        let _e388 = base[3u];
        if (_e388 == 0f) {
            discard;
        }
    } else {
        if override_type_3_9 {
            let _e390 = base;
            let _e392 = base;
            if (dot(_e390.xyz, _e392.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e396 = base;
    out_color = _e396;
    param_54 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_54));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
