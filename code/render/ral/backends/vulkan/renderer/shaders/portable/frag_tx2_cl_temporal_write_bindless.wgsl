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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
override override_type_3_8: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
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
    var phi_78_: bool;

    let _e81 = (*value);
    let _e82 = (*value);
    let _e84 = all((_e81 == _e82));
    phi_78_ = _e84;
    if _e84 {
        let _e85 = (*value);
        phi_78_ = all((abs(_e85) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e90 = phi_78_;
    return _e90;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e81 = (*value_1);
    let _e82 = (*value_1);
    let _e84 = all((_e81 == _e82));
    phi_63_ = _e84;
    if _e84 {
        let _e85 = (*value_1);
        phi_63_ = all((abs(_e85) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e90 = phi_63_;
    return _e90;
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
    var phi_101_: bool;
    var phi_110_: bool;
    var phi_120_: bool;
    var phi_127_: bool;
    var phi_156_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e91 = temporalOutcome_1;
    let _e92 = (_e91 != 1u);
    phi_101_ = _e92;
    if !(_e92) {
        let _e94 = temporalCurrentClip_1;
        param = _e94;
        let _e95 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e95);
    }
    let _e98 = phi_101_;
    phi_110_ = _e98;
    if !(_e98) {
        let _e100 = temporalPreviousClip_1;
        param_1 = _e100;
        let _e101 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e101);
    }
    let _e104 = phi_110_;
    phi_120_ = _e104;
    if !(_e104) {
        let _e107 = temporalCurrentClip_1[3u];
        phi_120_ = (_e107 <= 0.000001f);
    }
    let _e110 = phi_120_;
    phi_127_ = _e110;
    if !(_e110) {
        let _e113 = temporalPreviousClip_1[3u];
        phi_127_ = (_e113 <= 0.000001f);
    }
    let _e116 = phi_127_;
    if _e116 {
        return;
    }
    let _e117 = temporalCurrentClip_1;
    let _e120 = temporalCurrentClip_1[3u];
    currentNdc = (_e117.xy / vec2(_e120));
    let _e123 = temporalPreviousClip_1;
    let _e126 = temporalPreviousClip_1[3u];
    previousNdc = (_e123.xy / vec2(_e126));
    let _e129 = currentNdc;
    param_2 = _e129;
    let _e130 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e131 = !(_e130);
    phi_156_ = _e131;
    if !(_e131) {
        let _e133 = previousNdc;
        param_3 = _e133;
        let _e134 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e134);
    }
    let _e137 = phi_156_;
    if _e137 {
        return;
    }
    let _e138 = currentNdc;
    currentUv = ((_e138 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e141 = previousNdc;
    previousUv = ((_e141 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e144 = currentUv;
    let _e145 = previousUv;
    velocity = (_e144 - _e145);
    let _e147 = velocity;
    param_4 = _e147;
    let _e148 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e148) {
        return;
    }
    let _e150 = velocity;
    out_temporal_velocity = _e150;
    let _e151 = (*coverageConfidence);
    out_temporal_validity = clamp(_e151, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e83 + 0.5f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e90 = fogType;
    let _e93 = fogType;
    return (((_e88 > 0.5f) && (_e90 >= 1i)) && (_e93 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e83 = wired_advanced_fog_enabled_u0028_();
    if !(_e83) {
        return 0f;
    }
    let _e86 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e86, 0.000001f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e91 + 0.5f));
    let _e94 = fogType_1;
    if (_e94 == 1i) {
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e98 <= 0f) {
            return 0f;
        }
        let _e100 = viewDepth;
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e100 / _e103), 0f, 1f);
    }
    let _e108 = unnamed.advancedFogColorDensity[3u];
    let _e110 = viewDepth;
    opticalDepth = (max(_e108, 0f) * _e110);
    let _e112 = fogType_1;
    if (_e112 == 2i) {
        let _e114 = opticalDepth;
        return clamp((1f - exp(-(_e114))), 0f, 1f);
    }
    let _e119 = opticalDepth;
    let _e120 = opticalDepth;
    return clamp((1f - exp(-((_e119 * _e120)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e83 = (*rgb);
    let _e86 = unnamed.worldLightParams[0u];
    boosted = (_e83 * _e86);
    let _e89 = boosted[0u];
    let _e91 = boosted[1u];
    let _e93 = boosted[2u];
    peak = max(_e89, max(_e91, _e93));
    let _e96 = peak;
    if (_e96 > 1f) {
        let _e98 = peak;
        let _e99 = boosted;
        boosted = (_e99 / vec3(_e98));
    }
    let _e102 = boosted;
    return _e102;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e84 = (*c);
    (*c) = max(_e84, vec3<f32>(0f, 0f, 0f));
    let _e86 = (*c);
    cutoff = (_e86 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e88 = (*c);
    lo = (_e88 / vec3(12.92f));
    let _e91 = (*c);
    hi = pow(((_e91 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e96 = hi;
    let _e97 = lo;
    let _e98 = cutoff;
    return mix(_e96, _e97, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e98));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e108 = (*uv);
    let _e109 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    c_1 = _e109;
    let _e110 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e110))) == 0i) {
        let _e115 = c_1;
        param_5 = _e115.xyz;
        let _e117 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = (*slot);
    if (lightmap_slot == (_e124 + 1i)) {
        let _e127 = c_1;
        param_6 = _e127.xyz;
        let _e129 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e129.x;
        c_1[1u] = _e129.y;
        c_1[2u] = _e129.z;
    }
    let _e136 = c_1;
    return _e136;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_9: vec3<f32>;
    var color0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var color1_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color2_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color2_1: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color1_2: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var color2_2: vec4<f32>;
    var param_28: u32;
    var param_29: vec2<f32>;
    var param_30: i32;
    var color1_3: vec4<f32>;
    var param_31: u32;
    var param_32: vec2<f32>;
    var param_33: i32;
    var color2_3: vec4<f32>;
    var param_34: u32;
    var param_35: vec2<f32>;
    var param_36: i32;
    var color1_4: vec4<f32>;
    var param_37: u32;
    var param_38: vec2<f32>;
    var param_39: i32;
    var color2_4: vec4<f32>;
    var param_40: u32;
    var param_41: vec2<f32>;
    var param_42: i32;
    var color1_5: vec4<f32>;
    var param_43: u32;
    var param_44: vec2<f32>;
    var param_45: i32;
    var color2_5: vec4<f32>;
    var param_46: u32;
    var param_47: vec2<f32>;
    var param_48: i32;
    var color1_6: vec4<f32>;
    var param_49: u32;
    var param_50: vec2<f32>;
    var param_51: i32;
    var color2_6: vec4<f32>;
    var param_52: u32;
    var param_53: vec2<f32>;
    var param_54: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_55: f32;

    let _e152 = frag_color0In_1;
    param_7 = _e152.xyz;
    let _e154 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e156 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e154.x, _e154.y, _e154.z, _e156);
    let _e161 = frag_color1In_1;
    param_8 = _e161.xyz;
    let _e163 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e165 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e163.x, _e163.y, _e163.z, _e165);
    let _e170 = frag_color2In_1;
    param_9 = _e170.xyz;
    let _e172 = sRGBToLinear_u0028_vf3_u003b((&param_9));
    let _e174 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e172.x, _e172.y, _e172.z, _e174);
    param_10 = 0u;
    let _e179 = frag_tex_coord0_1;
    param_11 = _e179;
    param_12 = 0i;
    let _e180 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    let _e181 = frag_color0_;
    color0_ = (_e180 * _e181);
    if override_type_3_2 {
        param_13 = 1u;
        let _e183 = frag_tex_coord1_1;
        param_14 = _e183;
        param_15 = 1i;
        let _e184 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
        let _e185 = frag_color1_;
        color1_ = (_e184 * _e185);
        param_16 = 2u;
        let _e187 = frag_tex_coord2_1;
        param_17 = _e187;
        param_18 = 2i;
        let _e188 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
        let _e189 = frag_color2_;
        color2_ = (_e188 * _e189);
        let _e191 = color0_;
        let _e193 = color1_;
        let _e196 = color2_;
        let _e198 = ((_e191.xyz + _e193.xyz) + _e196.xyz);
        let _e200 = color0_[3u];
        let _e202 = color1_[3u];
        let _e205 = color2_[3u];
        base = vec4<f32>(_e198.x, _e198.y, _e198.z, ((_e200 * _e202) * _e205));
    } else {
        if override_type_3_3 {
            param_19 = 1u;
            let _e211 = frag_tex_coord1_1;
            param_20 = _e211;
            param_21 = 1i;
            let _e212 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
            let _e213 = frag_color1_;
            color1_1 = (_e212 * _e213);
            param_22 = 2u;
            let _e215 = frag_tex_coord2_1;
            param_23 = _e215;
            param_24 = 2i;
            let _e216 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
            let _e217 = frag_color2_;
            color2_1 = (_e216 * _e217);
            let _e220 = color0_[3u];
            let _e221 = color0_;
            color0_ = (_e221 * _e220);
            let _e224 = color1_1[3u];
            let _e225 = color1_1;
            color1_1 = (_e225 * _e224);
            let _e228 = color2_1[3u];
            let _e229 = color2_1;
            color2_1 = (_e229 * _e228);
            let _e231 = color0_;
            let _e233 = color1_1;
            let _e236 = color2_1;
            let _e238 = ((_e231.xyz + _e233.xyz) + _e236.xyz);
            let _e240 = color0_[3u];
            let _e242 = color1_1[3u];
            let _e245 = color2_1[3u];
            base = vec4<f32>(_e238.x, _e238.y, _e238.z, ((_e240 * _e242) * _e245));
        } else {
            if override_type_3_4 {
                param_25 = 1u;
                let _e251 = frag_tex_coord1_1;
                param_26 = _e251;
                param_27 = 1i;
                let _e252 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                let _e253 = frag_color1_;
                color1_2 = (_e252 * _e253);
                param_28 = 2u;
                let _e255 = frag_tex_coord2_1;
                param_29 = _e255;
                param_30 = 2i;
                let _e256 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                let _e257 = frag_color2_;
                color2_2 = (_e256 * _e257);
                let _e260 = color0_[3u];
                let _e262 = color0_;
                color0_ = (_e262 * (1f - _e260));
                let _e265 = color1_2[3u];
                let _e267 = color1_2;
                color1_2 = (_e267 * (1f - _e265));
                let _e270 = color2_2[3u];
                let _e272 = color2_2;
                color2_2 = (_e272 * (1f - _e270));
                let _e274 = color0_;
                let _e276 = color1_2;
                let _e279 = color2_2;
                let _e281 = ((_e274.xyz + _e276.xyz) + _e279.xyz);
                let _e283 = color0_[3u];
                let _e285 = color1_2[3u];
                let _e288 = color2_2[3u];
                base = vec4<f32>(_e281.x, _e281.y, _e281.z, ((_e283 * _e285) * _e288));
            } else {
                if override_type_3_5 {
                    param_31 = 1u;
                    let _e294 = frag_tex_coord1_1;
                    param_32 = _e294;
                    param_33 = 1i;
                    let _e295 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                    let _e296 = frag_color1_;
                    color1_3 = (_e295 * _e296);
                    param_34 = 2u;
                    let _e298 = frag_tex_coord2_1;
                    param_35 = _e298;
                    param_36 = 2i;
                    let _e299 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                    let _e300 = frag_color2_;
                    color2_3 = (_e299 * _e300);
                    let _e302 = color0_;
                    let _e303 = color1_3;
                    let _e305 = color1_3[3u];
                    let _e308 = color2_3;
                    let _e310 = color2_3[3u];
                    base = mix(mix(_e302, _e303, vec4(_e305)), _e308, vec4(_e310));
                } else {
                    if override_type_3_6 {
                        param_37 = 1u;
                        let _e313 = frag_tex_coord1_1;
                        param_38 = _e313;
                        param_39 = 1i;
                        let _e314 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                        let _e315 = frag_color1_;
                        color1_4 = (_e314 * _e315);
                        param_40 = 2u;
                        let _e317 = frag_tex_coord2_1;
                        param_41 = _e317;
                        param_42 = 2i;
                        let _e318 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                        let _e319 = frag_color2_;
                        color2_4 = (_e318 * _e319);
                        let _e321 = color2_4;
                        let _e322 = color1_4;
                        let _e323 = color0_;
                        let _e325 = color1_4[3u];
                        let _e329 = color2_4[3u];
                        base = mix(_e321, mix(_e322, _e323, vec4(_e325)), vec4(_e329));
                    } else {
                        if override_type_3_7 {
                            param_43 = 1u;
                            let _e332 = frag_tex_coord1_1;
                            param_44 = _e332;
                            param_45 = 1i;
                            let _e333 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e334 = frag_color1_;
                            color1_5 = (_e333 * _e334);
                            param_46 = 2u;
                            let _e336 = frag_tex_coord2_1;
                            param_47 = _e336;
                            param_48 = 2i;
                            let _e337 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e338 = frag_color2_;
                            color2_5 = (_e337 * _e338);
                            let _e340 = color2_5;
                            let _e342 = color2_5[3u];
                            let _e345 = color1_5;
                            let _e347 = color1_5[3u];
                            let _e351 = color0_;
                            base = (((_e340 + vec4(_e342)) * (_e345 + vec4(_e347))) * _e351);
                        } else {
                            param_49 = 1u;
                            let _e353 = frag_tex_coord1_1;
                            param_50 = _e353;
                            param_51 = 1i;
                            let _e354 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_49), (&param_50), (&param_51));
                            let _e355 = frag_color1_;
                            color1_6 = (_e354 * _e355);
                            param_52 = 2u;
                            let _e357 = frag_tex_coord2_1;
                            param_53 = _e357;
                            param_54 = 2i;
                            let _e358 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_52), (&param_53), (&param_54));
                            let _e359 = frag_color2_;
                            color2_6 = (_e358 * _e359);
                            let _e361 = color0_;
                            let _e363 = color1_6;
                            let _e366 = color2_6;
                            let _e368 = ((_e361.xyz * _e363.xyz) * _e366.xyz);
                            base[0u] = _e368.x;
                            base[1u] = _e368.y;
                            base[2u] = _e368.z;
                            let _e376 = color0_[3u];
                            let _e378 = color1_6[3u];
                            let _e381 = color2_6[3u];
                            base[3u] = ((_e376 * _e378) * _e381);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e386 = unnamed.worldLightParams[1u];
        wetness = clamp(_e386, 0f, 1f);
        let _e390 = unnamed.worldLightParams[2u];
        frost = clamp(_e390, 0f, 1f);
        let _e392 = base;
        luminance = dot(_e392.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e395 = wetness;
        let _e397 = base;
        let _e399 = (_e397.xyz * mix(1f, 0.82f, _e395));
        base[0u] = _e399.x;
        base[1u] = _e399.y;
        base[2u] = _e399.z;
        let _e406 = base;
        let _e408 = luminance;
        let _e410 = luminance;
        let _e412 = luminance;
        let _e414 = frost;
        let _e417 = mix(_e406.xyz, vec3<f32>((_e408 * 0.88f), (_e410 * 0.94f), _e412), vec3((_e414 * 0.55f)));
        base[0u] = _e417.x;
        base[1u] = _e417.y;
        base[2u] = _e417.z;
    }
    let _e424 = color0_;
    let _e427 = unnamed.emissionRadiance;
    let _e430 = base;
    let _e432 = (_e430.xyz + (_e424.xyz * _e427.xyz));
    base[0u] = _e432.x;
    base[1u] = _e432.y;
    base[2u] = _e432.z;
    let _e439 = wired_advanced_fog_enabled_u0028_();
    if _e439 {
        let _e440 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e440;
        let _e441 = base;
        let _e444 = unnamed.advancedFogColorDensity;
        let _e446 = fogAmount;
        let _e448 = mix(_e441.xyz, _e444.xyz, vec3(_e446));
        base[0u] = _e448.x;
        base[1u] = _e448.y;
        base[2u] = _e448.z;
    }
    if override_type_3_9 {
        let _e456 = base[3u];
        if (_e456 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e458 = base;
            let _e460 = base;
            if (dot(_e458.xyz, _e460.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e464 = base;
    out_color = _e464;
    param_55 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_55));
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
