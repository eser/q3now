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
@id(10) override acff: i32 = 0i;
override override_type_3_8: bool = (acff == 1i);
override override_type_3_9: bool = (acff == 2i);
override override_type_3_10: bool = (acff == 3i);
override override_type_3_11: bool = (acff == 1i);
override override_type_3_12: bool = (acff == 2i);
override override_type_3_13: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_14: bool = (discard_mode == 1i);
override override_type_3_15: bool = (discard_mode == 2i);
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

    let _e79 = (*value);
    let _e80 = (*value);
    let _e82 = all((_e79 == _e80));
    phi_75_ = _e82;
    if _e82 {
        let _e83 = (*value);
        phi_75_ = all((abs(_e83) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e88 = phi_75_;
    return _e88;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_60_: bool;

    let _e79 = (*value_1);
    let _e80 = (*value_1);
    let _e82 = all((_e79 == _e80));
    phi_60_ = _e82;
    if _e82 {
        let _e83 = (*value_1);
        phi_60_ = all((abs(_e83) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e88 = phi_60_;
    return _e88;
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
    let _e89 = temporalOutcome_1;
    let _e90 = (_e89 != 1u);
    phi_98_ = _e90;
    if !(_e90) {
        let _e92 = temporalCurrentClip_1;
        param = _e92;
        let _e93 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_98_ = !(_e93);
    }
    let _e96 = phi_98_;
    phi_107_ = _e96;
    if !(_e96) {
        let _e98 = temporalPreviousClip_1;
        param_1 = _e98;
        let _e99 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_107_ = !(_e99);
    }
    let _e102 = phi_107_;
    phi_117_ = _e102;
    if !(_e102) {
        let _e105 = temporalCurrentClip_1[3u];
        phi_117_ = (_e105 <= 0.000001f);
    }
    let _e108 = phi_117_;
    phi_124_ = _e108;
    if !(_e108) {
        let _e111 = temporalPreviousClip_1[3u];
        phi_124_ = (_e111 <= 0.000001f);
    }
    let _e114 = phi_124_;
    if _e114 {
        return;
    }
    let _e115 = temporalCurrentClip_1;
    let _e118 = temporalCurrentClip_1[3u];
    currentNdc = (_e115.xy / vec2(_e118));
    let _e121 = temporalPreviousClip_1;
    let _e124 = temporalPreviousClip_1[3u];
    previousNdc = (_e121.xy / vec2(_e124));
    let _e127 = currentNdc;
    param_2 = _e127;
    let _e128 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e129 = !(_e128);
    phi_153_ = _e129;
    if !(_e129) {
        let _e131 = previousNdc;
        param_3 = _e131;
        let _e132 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_153_ = !(_e132);
    }
    let _e135 = phi_153_;
    if _e135 {
        return;
    }
    let _e136 = currentNdc;
    currentUv = ((_e136 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e139 = previousNdc;
    previousUv = ((_e139 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e142 = currentUv;
    let _e143 = previousUv;
    velocity = (_e142 - _e143);
    let _e145 = velocity;
    param_4 = _e145;
    let _e146 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e146) {
        return;
    }
    let _e148 = velocity;
    out_temporal_velocity = _e148;
    let _e149 = (*coverageConfidence);
    out_temporal_validity = clamp(_e149, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e81 + 0.5f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e88 = fogType;
    let _e91 = fogType;
    return (((_e86 > 0.5f) && (_e88 >= 1i)) && (_e91 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e81 = wired_advanced_fog_enabled_u0028_();
    if !(_e81) {
        return 0f;
    }
    let _e84 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e84, 0.000001f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e89 + 0.5f));
    let _e92 = fogType_1;
    if (_e92 == 1i) {
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e96 <= 0f) {
            return 0f;
        }
        let _e98 = viewDepth;
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e98 / _e101), 0f, 1f);
    }
    let _e106 = unnamed.advancedFogColorDensity[3u];
    let _e108 = viewDepth;
    opticalDepth = (max(_e106, 0f) * _e108);
    let _e110 = fogType_1;
    if (_e110 == 2i) {
        let _e112 = opticalDepth;
        return clamp((1f - exp(-(_e112))), 0f, 1f);
    }
    let _e117 = opticalDepth;
    let _e118 = opticalDepth;
    return clamp((1f - exp(-((_e117 * _e118)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e82 = (*c);
    (*c) = max(_e82, vec3<f32>(0f, 0f, 0f));
    let _e84 = (*c);
    cutoff = (_e84 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e86 = (*c);
    lo = (_e86 / vec3(12.92f));
    let _e89 = (*c);
    hi = pow(((_e89 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e94 = hi;
    let _e95 = lo;
    let _e96 = cutoff;
    return mix(_e94, _e95, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e96));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;

    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e105 = (*uv);
    let _e106 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    c_1 = _e106;
    let _e107 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e107))) == 0i) {
        let _e112 = c_1;
        param_5 = _e112.xyz;
        let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = (*slot);
    if (lightmap_slot == (_e121 + 1i)) {
        let _e126 = unnamed.worldLightParams[0u];
        let _e127 = c_1;
        let _e129 = (_e127.xyz * _e126);
        c_1[0u] = _e129.x;
        c_1[1u] = _e129.y;
        c_1[2u] = _e129.z;
    }
    let _e136 = c_1;
    return _e136;
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
    var fogAmount: f32;
    var param_54: f32;

    let _e151 = unnamed.packed_indices[0i][3u];
    let _e157 = unnamed.packed_indices[0i][3u];
    let _e162 = fog_tex_coord_1;
    let _e163 = textureSample(wired_bindless_images[(_e151 & 4095u)], wired_bindless_samplers[((_e157 >> bitcast<u32>(12i)) & 255u)], _e162);
    fog = _e163;
    let _e164 = frag_color0In_1;
    param_6 = _e164.xyz;
    let _e166 = sRGBToLinear_u0028_vf3_u003b((&param_6));
    let _e168 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e166.x, _e166.y, _e166.z, _e168);
    let _e173 = frag_color1In_1;
    param_7 = _e173.xyz;
    let _e175 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e177 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e175.x, _e175.y, _e175.z, _e177);
    let _e182 = frag_color2In_1;
    param_8 = _e182.xyz;
    let _e184 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e186 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e184.x, _e184.y, _e184.z, _e186);
    param_9 = 0u;
    let _e191 = frag_tex_coord0_1;
    param_10 = _e191;
    param_11 = 0i;
    let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    let _e193 = frag_color0_;
    color0_ = (_e192 * _e193);
    if override_type_3_2 {
        param_12 = 1u;
        let _e195 = frag_tex_coord1_1;
        param_13 = _e195;
        param_14 = 1i;
        let _e196 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
        let _e197 = frag_color1_;
        color1_ = (_e196 * _e197);
        param_15 = 2u;
        let _e199 = frag_tex_coord2_1;
        param_16 = _e199;
        param_17 = 2i;
        let _e200 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        let _e201 = frag_color2_;
        color2_ = (_e200 * _e201);
        let _e203 = color0_;
        let _e205 = color1_;
        let _e208 = color2_;
        let _e210 = ((_e203.xyz + _e205.xyz) + _e208.xyz);
        let _e212 = color0_[3u];
        let _e214 = color1_[3u];
        let _e217 = color2_[3u];
        base = vec4<f32>(_e210.x, _e210.y, _e210.z, ((_e212 * _e214) * _e217));
    } else {
        if override_type_3_3 {
            param_18 = 1u;
            let _e223 = frag_tex_coord1_1;
            param_19 = _e223;
            param_20 = 1i;
            let _e224 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            let _e225 = frag_color1_;
            color1_1 = (_e224 * _e225);
            param_21 = 2u;
            let _e227 = frag_tex_coord2_1;
            param_22 = _e227;
            param_23 = 2i;
            let _e228 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            let _e229 = frag_color2_;
            color2_1 = (_e228 * _e229);
            let _e232 = color0_[3u];
            let _e233 = color0_;
            color0_ = (_e233 * _e232);
            let _e236 = color1_1[3u];
            let _e237 = color1_1;
            color1_1 = (_e237 * _e236);
            let _e240 = color2_1[3u];
            let _e241 = color2_1;
            color2_1 = (_e241 * _e240);
            let _e243 = color0_;
            let _e245 = color1_1;
            let _e248 = color2_1;
            let _e250 = ((_e243.xyz + _e245.xyz) + _e248.xyz);
            let _e252 = color0_[3u];
            let _e254 = color1_1[3u];
            let _e257 = color2_1[3u];
            base = vec4<f32>(_e250.x, _e250.y, _e250.z, ((_e252 * _e254) * _e257));
        } else {
            if override_type_3_4 {
                param_24 = 1u;
                let _e263 = frag_tex_coord1_1;
                param_25 = _e263;
                param_26 = 1i;
                let _e264 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                let _e265 = frag_color1_;
                color1_2 = (_e264 * _e265);
                param_27 = 2u;
                let _e267 = frag_tex_coord2_1;
                param_28 = _e267;
                param_29 = 2i;
                let _e268 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_27), (&param_28), (&param_29));
                let _e269 = frag_color2_;
                color2_2 = (_e268 * _e269);
                let _e272 = color0_[3u];
                let _e274 = color0_;
                color0_ = (_e274 * (1f - _e272));
                let _e277 = color1_2[3u];
                let _e279 = color1_2;
                color1_2 = (_e279 * (1f - _e277));
                let _e282 = color2_2[3u];
                let _e284 = color2_2;
                color2_2 = (_e284 * (1f - _e282));
                let _e286 = color0_;
                let _e288 = color1_2;
                let _e291 = color2_2;
                let _e293 = ((_e286.xyz + _e288.xyz) + _e291.xyz);
                let _e295 = color0_[3u];
                let _e297 = color1_2[3u];
                let _e300 = color2_2[3u];
                base = vec4<f32>(_e293.x, _e293.y, _e293.z, ((_e295 * _e297) * _e300));
            } else {
                if override_type_3_5 {
                    param_30 = 1u;
                    let _e306 = frag_tex_coord1_1;
                    param_31 = _e306;
                    param_32 = 1i;
                    let _e307 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_30), (&param_31), (&param_32));
                    let _e308 = frag_color1_;
                    color1_3 = (_e307 * _e308);
                    param_33 = 2u;
                    let _e310 = frag_tex_coord2_1;
                    param_34 = _e310;
                    param_35 = 2i;
                    let _e311 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_33), (&param_34), (&param_35));
                    let _e312 = frag_color2_;
                    color2_3 = (_e311 * _e312);
                    let _e314 = color0_;
                    let _e315 = color1_3;
                    let _e317 = color1_3[3u];
                    let _e320 = color2_3;
                    let _e322 = color2_3[3u];
                    base = mix(mix(_e314, _e315, vec4(_e317)), _e320, vec4(_e322));
                } else {
                    if override_type_3_6 {
                        param_36 = 1u;
                        let _e325 = frag_tex_coord1_1;
                        param_37 = _e325;
                        param_38 = 1i;
                        let _e326 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_36), (&param_37), (&param_38));
                        let _e327 = frag_color1_;
                        color1_4 = (_e326 * _e327);
                        param_39 = 2u;
                        let _e329 = frag_tex_coord2_1;
                        param_40 = _e329;
                        param_41 = 2i;
                        let _e330 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_39), (&param_40), (&param_41));
                        let _e331 = frag_color2_;
                        color2_4 = (_e330 * _e331);
                        let _e333 = color2_4;
                        let _e334 = color1_4;
                        let _e335 = color0_;
                        let _e337 = color1_4[3u];
                        let _e341 = color2_4[3u];
                        base = mix(_e333, mix(_e334, _e335, vec4(_e337)), vec4(_e341));
                    } else {
                        if override_type_3_7 {
                            param_42 = 1u;
                            let _e344 = frag_tex_coord1_1;
                            param_43 = _e344;
                            param_44 = 1i;
                            let _e345 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_42), (&param_43), (&param_44));
                            let _e346 = frag_color1_;
                            color1_5 = (_e345 * _e346);
                            param_45 = 2u;
                            let _e348 = frag_tex_coord2_1;
                            param_46 = _e348;
                            param_47 = 2i;
                            let _e349 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_45), (&param_46), (&param_47));
                            let _e350 = frag_color2_;
                            color2_5 = (_e349 * _e350);
                            let _e352 = color2_5;
                            let _e354 = color2_5[3u];
                            let _e357 = color1_5;
                            let _e359 = color1_5[3u];
                            let _e363 = color0_;
                            base = (((_e352 + vec4(_e354)) * (_e357 + vec4(_e359))) * _e363);
                        } else {
                            param_48 = 1u;
                            let _e365 = frag_tex_coord1_1;
                            param_49 = _e365;
                            param_50 = 1i;
                            let _e366 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_48), (&param_49), (&param_50));
                            let _e367 = frag_color1_;
                            color1_6 = (_e366 * _e367);
                            param_51 = 2u;
                            let _e369 = frag_tex_coord2_1;
                            param_52 = _e369;
                            param_53 = 2i;
                            let _e370 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_51), (&param_52), (&param_53));
                            let _e371 = frag_color2_;
                            color2_6 = (_e370 * _e371);
                            let _e373 = color0_;
                            let _e375 = color1_6;
                            let _e378 = color2_6;
                            let _e380 = ((_e373.xyz * _e375.xyz) * _e378.xyz);
                            base[0u] = _e380.x;
                            base[1u] = _e380.y;
                            base[2u] = _e380.z;
                            let _e388 = color0_[3u];
                            let _e390 = color1_6[3u];
                            let _e393 = color2_6[3u];
                            base[3u] = ((_e388 * _e390) * _e393);
                        }
                    }
                }
            }
        }
    }
    let _e396 = wired_advanced_fog_enabled_u0028_();
    if _e396 {
        let _e397 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e397;
        if override_type_3_8 {
            let _e398 = fogAmount;
            let _e400 = base;
            let _e402 = (_e400.xyz * (1f - _e398));
            base[0u] = _e402.x;
            base[1u] = _e402.y;
            base[2u] = _e402.z;
        } else {
            if override_type_3_9 {
                let _e409 = fogAmount;
                let _e411 = base;
                base = (_e411 * (1f - _e409));
            } else {
                if override_type_3_10 {
                    let _e413 = fogAmount;
                    let _e416 = base[3u];
                    base[3u] = (_e416 * (1f - _e413));
                } else {
                    let _e419 = base;
                    let _e422 = unnamed.advancedFogColorDensity;
                    let _e424 = fogAmount;
                    let _e426 = mix(_e419.xyz, _e422.xyz, vec3(_e424));
                    base[0u] = _e426.x;
                    base[1u] = _e426.y;
                    base[2u] = _e426.z;
                }
            }
        }
    } else {
        if override_type_3_11 {
            let _e433 = base;
            let _e436 = fog[3u];
            let _e438 = (_e433.xyz * (1f - _e436));
            base[0u] = _e438.x;
            base[1u] = _e438.y;
            base[2u] = _e438.z;
        } else {
            if override_type_3_12 {
                let _e445 = base;
                let _e447 = fog[3u];
                base = (_e445 * (1f - _e447));
            } else {
                if override_type_3_13 {
                    let _e451 = base[3u];
                    let _e453 = fog[3u];
                    base[3u] = (_e451 * (1f - _e453));
                } else {
                    let _e457 = base;
                    let _e458 = fog;
                    let _e460 = unnamed.fogColor;
                    let _e463 = fog[3u];
                    base = mix(_e457, (_e458 * _e460), vec4(_e463));
                }
            }
        }
    }
    if override_type_3_14 {
        let _e467 = base[3u];
        if (_e467 == 0f) {
            discard;
        }
    } else {
        if override_type_3_15 {
            let _e469 = base;
            let _e471 = base;
            if (dot(_e469.xyz, _e471.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e475 = base;
    out_color = _e475;
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
