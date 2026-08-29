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
@id(10) override acff: i32 = 0i;
override override_type_3_9: bool = (acff == 1i);
override override_type_3_10: bool = (acff == 2i);
override override_type_3_11: bool = (acff == 3i);
override override_type_3_12: bool = (acff == 1i);
override override_type_3_13: bool = (acff == 2i);
override override_type_3_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_15: bool = (discard_mode == 1i);
override override_type_3_16: bool = (discard_mode == 2i);
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
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_75_: bool;

    let _e89 = (*value);
    let _e90 = (*value);
    let _e92 = all((_e89 == _e90));
    phi_75_ = _e92;
    if _e92 {
        let _e93 = (*value);
        phi_75_ = all((abs(_e93) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e98 = phi_75_;
    return _e98;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e89 = (*value_1);
    let _e90 = (*value_1);
    let _e92 = all((_e89 == _e90));
    phi_60_ = _e92;
    if _e92 {
        let _e93 = (*value_1);
        phi_60_ = all((abs(_e93) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e98 = phi_60_;
    return _e98;
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
    let _e99 = temporalOutcome_1;
    let _e100 = (_e99 != 1u);
    phi_98_ = _e100;
    if !(_e100) {
        let _e102 = temporalCurrentClip_1;
        param = _e102;
        let _e103 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e103);
    }
    let _e106 = phi_98_;
    phi_107_ = _e106;
    if !(_e106) {
        let _e108 = temporalPreviousClip_1;
        param_1 = _e108;
        let _e109 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e109);
    }
    let _e112 = phi_107_;
    phi_117_ = _e112;
    if !(_e112) {
        let _e115 = temporalCurrentClip_1[3u];
        phi_117_ = (_e115 <= 0.000001f);
    }
    let _e118 = phi_117_;
    phi_124_ = _e118;
    if !(_e118) {
        let _e121 = temporalPreviousClip_1[3u];
        phi_124_ = (_e121 <= 0.000001f);
    }
    let _e124 = phi_124_;
    if _e124 {
        return;
    }
    let _e125 = temporalCurrentClip_1;
    let _e128 = temporalCurrentClip_1[3u];
    currentNdc = (_e125.xy / vec2(_e128));
    let _e131 = temporalPreviousClip_1;
    let _e134 = temporalPreviousClip_1[3u];
    previousNdc = (_e131.xy / vec2(_e134));
    let _e137 = currentNdc;
    param_2 = _e137;
    let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e139 = !(_e138);
    phi_153_ = _e139;
    if !(_e139) {
        let _e141 = previousNdc;
        param_3 = _e141;
        let _e142 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e142);
    }
    let _e145 = phi_153_;
    if _e145 {
        return;
    }
    let _e146 = currentNdc;
    currentUv = ((_e146 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e149 = previousNdc;
    previousUv = ((_e149 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e152 = currentUv;
    let _e153 = previousUv;
    velocity = (_e152 - _e153);
    let _e155 = velocity;
    param_4 = _e155;
    let _e156 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e156) {
        return;
    }
    let _e158 = velocity;
    out_temporal_velocity = _e158;
    let _e159 = (*coverageConfidence);
    out_temporal_validity = clamp(_e159, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e91 + 0.5f));
    let _e96 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e98 = fogType;
    let _e101 = fogType;
    return (((_e96 > 0.5f) && (_e98 >= 1i)) && (_e101 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e91 = wired_advanced_fog_enabled_u0028_();
    if !(_e91) {
        return 0f;
    }
    let _e94 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e94, 0.000001f));
    let _e99 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e99 + 0.5f));
    let _e102 = fogType_1;
    if (_e102 == 1i) {
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e106 <= 0f) {
            return 0f;
        }
        let _e108 = viewDepth;
        let _e111 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e108 / _e111), 0f, 1f);
    }
    let _e116 = unnamed.advancedFogColorDensity[3u];
    let _e118 = viewDepth;
    opticalDepth = (max(_e116, 0f) * _e118);
    let _e120 = fogType_1;
    if (_e120 == 2i) {
        let _e122 = opticalDepth;
        return clamp((1f - exp(-(_e122))), 0f, 1f);
    }
    let _e127 = opticalDepth;
    let _e128 = opticalDepth;
    return clamp((1f - exp(-((_e127 * _e128)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e92 = (*c);
    (*c) = max(_e92, vec3<f32>(0f, 0f, 0f));
    let _e94 = (*c);
    cutoff = (_e94 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e96 = (*c);
    lo = (_e96 / vec3(12.92f));
    let _e99 = (*c);
    hi = pow(((_e99 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e104 = hi;
    let _e105 = lo;
    let _e106 = cutoff;
    return mix(_e104, _e105, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e106));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e103 = (*role);
    let _e105 = (*role);
    let _e110 = unnamed.packed_indices[(_e103 / 4u)][(_e105 % 4u)];
    let _e115 = (*uv);
    let _e116 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e110 >> bitcast<u32>(12i)) & 255u)], _e115);
    c_1 = _e116;
    let _e117 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e117))) == 0i) {
        let _e122 = c_1;
        param_5 = _e122.xyz;
        let _e124 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = (*slot);
    if (lightmap_slot == (_e131 + 1i)) {
        let _e136 = unnamed.worldLightParams[0u];
        let _e137 = c_1;
        let _e139 = (_e137.xyz * _e136);
        c_1[0u] = _e139.x;
        c_1[1u] = _e139.y;
        c_1[2u] = _e139.z;
    }
    let _e146 = c_1;
    return _e146;
}

fn main_1() {
    var fog: vec4<f32>;
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
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_54: f32;

    let _e164 = unnamed.packed_indices[0i][3u];
    let _e170 = unnamed.packed_indices[0i][3u];
    let _e175 = fog_tex_coord_1;
    let _e176 = textureSample(wired_bindless_images[(_e164 & 4095u)], wired_bindless_samplers[((_e170 >> bitcast<u32>(12i)) & 255u)], _e175);
    fog = _e176;
    let _e177 = frag_color0In_1;
    param_6 = _e177.xyz;
    let _e179 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e181 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e179.x, _e179.y, _e179.z, _e181);
    let _e186 = frag_color1In_1;
    param_7 = _e186.xyz;
    let _e188 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e190 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e188.x, _e188.y, _e188.z, _e190);
    let _e195 = frag_color2In_1;
    param_8 = _e195.xyz;
    let _e197 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e199 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e197.x, _e197.y, _e197.z, _e199);
    param_9 = 0u;
    let _e204 = frag_tex_coord0_1;
    param_10 = _e204;
    param_11 = 0i;
    let _e205 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    let _e206 = frag_color0_;
    color0_ = (_e205 * _e206);
    if override_type_3_2 {
        param_12 = 1u;
        let _e208 = frag_tex_coord1_1;
        param_13 = _e208;
        param_14 = 1i;
        let _e209 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
        let _e210 = frag_color1_;
        color1_ = (_e209 * _e210);
        param_15 = 2u;
        let _e212 = frag_tex_coord2_1;
        param_16 = _e212;
        param_17 = 2i;
        let _e213 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        let _e214 = frag_color2_;
        color2_ = (_e213 * _e214);
        let _e216 = color0_;
        let _e218 = color1_;
        let _e221 = color2_;
        let _e223 = ((_e216.xyz + _e218.xyz) + _e221.xyz);
        let _e225 = color0_[3u];
        let _e227 = color1_[3u];
        let _e230 = color2_[3u];
        base = vec4<f32>(_e223.x, _e223.y, _e223.z, ((_e225 * _e227) * _e230));
    } else {
        if override_type_3_3 {
            param_18 = 1u;
            let _e236 = frag_tex_coord1_1;
            param_19 = _e236;
            param_20 = 1i;
            let _e237 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            let _e238 = frag_color1_;
            color1_1 = (_e237 * _e238);
            param_21 = 2u;
            let _e240 = frag_tex_coord2_1;
            param_22 = _e240;
            param_23 = 2i;
            let _e241 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            let _e242 = frag_color2_;
            color2_1 = (_e241 * _e242);
            let _e245 = color0_[3u];
            let _e246 = color0_;
            color0_ = (_e246 * _e245);
            let _e249 = color1_1[3u];
            let _e250 = color1_1;
            color1_1 = (_e250 * _e249);
            let _e253 = color2_1[3u];
            let _e254 = color2_1;
            color2_1 = (_e254 * _e253);
            let _e256 = color0_;
            let _e258 = color1_1;
            let _e261 = color2_1;
            let _e263 = ((_e256.xyz + _e258.xyz) + _e261.xyz);
            let _e265 = color0_[3u];
            let _e267 = color1_1[3u];
            let _e270 = color2_1[3u];
            base = vec4<f32>(_e263.x, _e263.y, _e263.z, ((_e265 * _e267) * _e270));
        } else {
            if override_type_3_4 {
                param_24 = 1u;
                let _e276 = frag_tex_coord1_1;
                param_25 = _e276;
                param_26 = 1i;
                let _e277 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                let _e278 = frag_color1_;
                color1_2 = (_e277 * _e278);
                param_27 = 2u;
                let _e280 = frag_tex_coord2_1;
                param_28 = _e280;
                param_29 = 2i;
                let _e281 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
                let _e282 = frag_color2_;
                color2_2 = (_e281 * _e282);
                let _e285 = color0_[3u];
                let _e287 = color0_;
                color0_ = (_e287 * (1f - _e285));
                let _e290 = color1_2[3u];
                let _e292 = color1_2;
                color1_2 = (_e292 * (1f - _e290));
                let _e295 = color2_2[3u];
                let _e297 = color2_2;
                color2_2 = (_e297 * (1f - _e295));
                let _e299 = color0_;
                let _e301 = color1_2;
                let _e304 = color2_2;
                let _e306 = ((_e299.xyz + _e301.xyz) + _e304.xyz);
                let _e308 = color0_[3u];
                let _e310 = color1_2[3u];
                let _e313 = color2_2[3u];
                base = vec4<f32>(_e306.x, _e306.y, _e306.z, ((_e308 * _e310) * _e313));
            } else {
                if override_type_3_5 {
                    param_30 = 1u;
                    let _e319 = frag_tex_coord1_1;
                    param_31 = _e319;
                    param_32 = 1i;
                    let _e320 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
                    let _e321 = frag_color1_;
                    color1_3 = (_e320 * _e321);
                    param_33 = 2u;
                    let _e323 = frag_tex_coord2_1;
                    param_34 = _e323;
                    param_35 = 2i;
                    let _e324 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_33), (&param_34), (&param_35));
                    let _e325 = frag_color2_;
                    color2_3 = (_e324 * _e325);
                    let _e327 = color0_;
                    let _e328 = color1_3;
                    let _e330 = color1_3[3u];
                    let _e333 = color2_3;
                    let _e335 = color2_3[3u];
                    base = mix(mix(_e327, _e328, vec4(_e330)), _e333, vec4(_e335));
                } else {
                    if override_type_3_6 {
                        param_36 = 1u;
                        let _e338 = frag_tex_coord1_1;
                        param_37 = _e338;
                        param_38 = 1i;
                        let _e339 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_36), (&param_37), (&param_38));
                        let _e340 = frag_color1_;
                        color1_4 = (_e339 * _e340);
                        param_39 = 2u;
                        let _e342 = frag_tex_coord2_1;
                        param_40 = _e342;
                        param_41 = 2i;
                        let _e343 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_39), (&param_40), (&param_41));
                        let _e344 = frag_color2_;
                        color2_4 = (_e343 * _e344);
                        let _e346 = color2_4;
                        let _e347 = color1_4;
                        let _e348 = color0_;
                        let _e350 = color1_4[3u];
                        let _e354 = color2_4[3u];
                        base = mix(_e346, mix(_e347, _e348, vec4(_e350)), vec4(_e354));
                    } else {
                        if override_type_3_7 {
                            param_42 = 1u;
                            let _e357 = frag_tex_coord1_1;
                            param_43 = _e357;
                            param_44 = 1i;
                            let _e358 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_42), (&param_43), (&param_44));
                            let _e359 = frag_color1_;
                            color1_5 = (_e358 * _e359);
                            param_45 = 2u;
                            let _e361 = frag_tex_coord2_1;
                            param_46 = _e361;
                            param_47 = 2i;
                            let _e362 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_45), (&param_46), (&param_47));
                            let _e363 = frag_color2_;
                            color2_5 = (_e362 * _e363);
                            let _e365 = color2_5;
                            let _e367 = color2_5[3u];
                            let _e370 = color1_5;
                            let _e372 = color1_5[3u];
                            let _e376 = color0_;
                            base = (((_e365 + vec4(_e367)) * (_e370 + vec4(_e372))) * _e376);
                        } else {
                            param_48 = 1u;
                            let _e378 = frag_tex_coord1_1;
                            param_49 = _e378;
                            param_50 = 1i;
                            let _e379 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_48), (&param_49), (&param_50));
                            let _e380 = frag_color1_;
                            color1_6 = (_e379 * _e380);
                            param_51 = 2u;
                            let _e382 = frag_tex_coord2_1;
                            param_52 = _e382;
                            param_53 = 2i;
                            let _e383 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_51), (&param_52), (&param_53));
                            let _e384 = frag_color2_;
                            color2_6 = (_e383 * _e384);
                            let _e386 = color0_;
                            let _e388 = color1_6;
                            let _e391 = color2_6;
                            let _e393 = ((_e386.xyz * _e388.xyz) * _e391.xyz);
                            base[0u] = _e393.x;
                            base[1u] = _e393.y;
                            base[2u] = _e393.z;
                            let _e401 = color0_[3u];
                            let _e403 = color1_6[3u];
                            let _e406 = color2_6[3u];
                            base[3u] = ((_e401 * _e403) * _e406);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e411 = unnamed.worldLightParams[1u];
        wetness = clamp(_e411, 0f, 1f);
        let _e415 = unnamed.worldLightParams[2u];
        frost = clamp(_e415, 0f, 1f);
        let _e417 = base;
        luminance = dot(_e417.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e420 = wetness;
        let _e422 = base;
        let _e424 = (_e422.xyz * mix(1f, 0.82f, _e420));
        base[0u] = _e424.x;
        base[1u] = _e424.y;
        base[2u] = _e424.z;
        let _e431 = base;
        let _e433 = luminance;
        let _e435 = luminance;
        let _e437 = luminance;
        let _e439 = frost;
        let _e442 = mix(_e431.xyz, vec3<f32>((_e433 * 0.88f), (_e435 * 0.94f), _e437), vec3((_e439 * 0.55f)));
        base[0u] = _e442.x;
        base[1u] = _e442.y;
        base[2u] = _e442.z;
    }
    let _e449 = color0_;
    let _e452 = unnamed.emissionRadiance;
    let _e455 = base;
    let _e457 = (_e455.xyz + (_e449.xyz * _e452.xyz));
    base[0u] = _e457.x;
    base[1u] = _e457.y;
    base[2u] = _e457.z;
    let _e464 = wired_advanced_fog_enabled_u0028_();
    if _e464 {
        let _e465 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e465;
        if override_type_3_9 {
            let _e466 = fogAmount;
            let _e468 = base;
            let _e470 = (_e468.xyz * (1f - _e466));
            base[0u] = _e470.x;
            base[1u] = _e470.y;
            base[2u] = _e470.z;
        } else {
            if override_type_3_10 {
                let _e477 = fogAmount;
                let _e479 = base;
                base = (_e479 * (1f - _e477));
            } else {
                if override_type_3_11 {
                    let _e481 = fogAmount;
                    let _e484 = base[3u];
                    base[3u] = (_e484 * (1f - _e481));
                } else {
                    let _e487 = base;
                    let _e490 = unnamed.advancedFogColorDensity;
                    let _e492 = fogAmount;
                    let _e494 = mix(_e487.xyz, _e490.xyz, vec3(_e492));
                    base[0u] = _e494.x;
                    base[1u] = _e494.y;
                    base[2u] = _e494.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e501 = base;
            let _e504 = fog[3u];
            let _e506 = (_e501.xyz * (1f - _e504));
            base[0u] = _e506.x;
            base[1u] = _e506.y;
            base[2u] = _e506.z;
        } else {
            if override_type_3_13 {
                let _e513 = base;
                let _e515 = fog[3u];
                base = (_e513 * (1f - _e515));
            } else {
                if override_type_3_14 {
                    let _e519 = base[3u];
                    let _e521 = fog[3u];
                    base[3u] = (_e519 * (1f - _e521));
                } else {
                    let _e525 = base;
                    let _e526 = fog;
                    let _e528 = unnamed.fogColor;
                    let _e531 = fog[3u];
                    base = mix(_e525, (_e526 * _e528), vec4(_e531));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e535 = base[3u];
        if (_e535 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e537 = base;
            let _e539 = base;
            if (dot(_e537.xyz, _e539.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e543 = base;
    out_color = _e543;
    param_54 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_54));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e25 = out_temporal_velocity;
    let _e26 = out_temporal_validity;
    let _e27 = out_color;
    return FragmentOutput(_e25, _e26, _e27);
}
