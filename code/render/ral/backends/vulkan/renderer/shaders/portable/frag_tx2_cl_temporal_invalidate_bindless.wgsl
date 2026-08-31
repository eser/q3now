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
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
override override_type_4_8: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_9: bool = (discard_mode == 1i);
override override_type_4_10: bool = (discard_mode == 2i);
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
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
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

    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e79 + 0.5f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e86 = fogType;
    let _e89 = fogType;
    return (((_e84 > 0.5f) && (_e86 >= 1i)) && (_e89 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e79 = wired_advanced_fog_enabled_u0028_();
    if !(_e79) {
        return 0f;
    }
    let _e82 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e82, 0.000001f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e87 + 0.5f));
    let _e90 = fogType_1;
    if (_e90 == 1i) {
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e94 <= 0f) {
            return 0f;
        }
        let _e96 = viewDepth;
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e96 / _e99), 0f, 1f);
    }
    let _e104 = unnamed.advancedFogColorDensity[3u];
    let _e106 = viewDepth;
    opticalDepth = (max(_e104, 0f) * _e106);
    let _e108 = fogType_1;
    if (_e108 == 2i) {
        let _e110 = opticalDepth;
        return clamp((1f - exp(-(_e110))), 0f, 1f);
    }
    let _e115 = opticalDepth;
    let _e116 = opticalDepth;
    return clamp((1f - exp(-((_e115 * _e116)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e79 = (*rgb);
    let _e82 = unnamed.worldLightParams[0u];
    boosted = (_e79 * _e82);
    let _e85 = boosted[0u];
    let _e87 = boosted[1u];
    let _e89 = boosted[2u];
    peak = max(_e85, max(_e87, _e89));
    let _e92 = peak;
    if (_e92 > 1f) {
        let _e94 = peak;
        let _e95 = boosted;
        boosted = (_e95 / vec3(_e94));
    }
    let _e98 = boosted;
    return _e98;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e80 = (*c);
    (*c) = max(_e80, vec3<f32>(0f, 0f, 0f));
    let _e82 = (*c);
    cutoff = (_e82 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e84 = (*c);
    lo = (_e84 / vec3(12.92f));
    let _e87 = (*c);
    hi = pow(((_e87 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e92 = hi;
    let _e93 = lo;
    let _e94 = cutoff;
    return mix(_e92, _e93, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e94));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e104 = (*uv);
    let _e105 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    c_1 = _e105;
    let _e106 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e106))) == 0i) {
        let _e111 = c_1;
        param = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e123 = c_1;
        param_1 = _e123.xyz;
        let _e125 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = c_1;
    return _e132;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_4: vec3<f32>;
    var color0_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color1_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color2_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color2_1: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color1_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color2_2: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var color1_3: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color2_3: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
    var color1_4: vec4<f32>;
    var param_32: u32;
    var param_33: vec2<f32>;
    var param_34: i32;
    var color2_4: vec4<f32>;
    var param_35: u32;
    var param_36: vec2<f32>;
    var param_37: i32;
    var color1_5: vec4<f32>;
    var param_38: u32;
    var param_39: vec2<f32>;
    var param_40: i32;
    var color2_5: vec4<f32>;
    var param_41: u32;
    var param_42: vec2<f32>;
    var param_43: i32;
    var color1_6: vec4<f32>;
    var param_44: u32;
    var param_45: vec2<f32>;
    var param_46: i32;
    var color2_6: vec4<f32>;
    var param_47: u32;
    var param_48: vec2<f32>;
    var param_49: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_50: f32;

    let _e148 = frag_color0In_1;
    param_2 = _e148.xyz;
    let _e150 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e152 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e150.x, _e150.y, _e150.z, _e152);
    let _e157 = frag_color1In_1;
    param_3 = _e157.xyz;
    let _e159 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e161 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e159.x, _e159.y, _e159.z, _e161);
    let _e166 = frag_color2In_1;
    param_4 = _e166.xyz;
    let _e168 = sRGBToLinear_u0028_vf3_u003b((&param_4));
    let _e170 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e168.x, _e168.y, _e168.z, _e170);
    param_5 = 0u;
    let _e175 = frag_tex_coord0_1;
    param_6 = _e175;
    param_7 = 0i;
    let _e176 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    let _e177 = frag_color0_;
    color0_ = (_e176 * _e177);
    if override_type_4_2 {
        param_8 = 1u;
        let _e179 = frag_tex_coord1_1;
        param_9 = _e179;
        param_10 = 1i;
        let _e180 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        let _e181 = frag_color1_;
        color1_ = (_e180 * _e181);
        param_11 = 2u;
        let _e183 = frag_tex_coord2_1;
        param_12 = _e183;
        param_13 = 2i;
        let _e184 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
        let _e185 = frag_color2_;
        color2_ = (_e184 * _e185);
        let _e187 = color0_;
        let _e189 = color1_;
        let _e192 = color2_;
        let _e194 = ((_e187.xyz + _e189.xyz) + _e192.xyz);
        let _e196 = color0_[3u];
        let _e198 = color1_[3u];
        let _e201 = color2_[3u];
        base = vec4<f32>(_e194.x, _e194.y, _e194.z, ((_e196 * _e198) * _e201));
    } else {
        if override_type_4_3 {
            param_14 = 1u;
            let _e207 = frag_tex_coord1_1;
            param_15 = _e207;
            param_16 = 1i;
            let _e208 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e209 = frag_color1_;
            color1_1 = (_e208 * _e209);
            param_17 = 2u;
            let _e211 = frag_tex_coord2_1;
            param_18 = _e211;
            param_19 = 2i;
            let _e212 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            let _e213 = frag_color2_;
            color2_1 = (_e212 * _e213);
            let _e216 = color0_[3u];
            let _e217 = color0_;
            color0_ = (_e217 * _e216);
            let _e220 = color1_1[3u];
            let _e221 = color1_1;
            color1_1 = (_e221 * _e220);
            let _e224 = color2_1[3u];
            let _e225 = color2_1;
            color2_1 = (_e225 * _e224);
            let _e227 = color0_;
            let _e229 = color1_1;
            let _e232 = color2_1;
            let _e234 = ((_e227.xyz + _e229.xyz) + _e232.xyz);
            let _e236 = color0_[3u];
            let _e238 = color1_1[3u];
            let _e241 = color2_1[3u];
            base = vec4<f32>(_e234.x, _e234.y, _e234.z, ((_e236 * _e238) * _e241));
        } else {
            if override_type_4_4 {
                param_20 = 1u;
                let _e247 = frag_tex_coord1_1;
                param_21 = _e247;
                param_22 = 1i;
                let _e248 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
                let _e249 = frag_color1_;
                color1_2 = (_e248 * _e249);
                param_23 = 2u;
                let _e251 = frag_tex_coord2_1;
                param_24 = _e251;
                param_25 = 2i;
                let _e252 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                let _e253 = frag_color2_;
                color2_2 = (_e252 * _e253);
                let _e256 = color0_[3u];
                let _e258 = color0_;
                color0_ = (_e258 * (1f - _e256));
                let _e261 = color1_2[3u];
                let _e263 = color1_2;
                color1_2 = (_e263 * (1f - _e261));
                let _e266 = color2_2[3u];
                let _e268 = color2_2;
                color2_2 = (_e268 * (1f - _e266));
                let _e270 = color0_;
                let _e272 = color1_2;
                let _e275 = color2_2;
                let _e277 = ((_e270.xyz + _e272.xyz) + _e275.xyz);
                let _e279 = color0_[3u];
                let _e281 = color1_2[3u];
                let _e284 = color2_2[3u];
                base = vec4<f32>(_e277.x, _e277.y, _e277.z, ((_e279 * _e281) * _e284));
            } else {
                if override_type_4_5 {
                    param_26 = 1u;
                    let _e290 = frag_tex_coord1_1;
                    param_27 = _e290;
                    param_28 = 1i;
                    let _e291 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                    let _e292 = frag_color1_;
                    color1_3 = (_e291 * _e292);
                    param_29 = 2u;
                    let _e294 = frag_tex_coord2_1;
                    param_30 = _e294;
                    param_31 = 2i;
                    let _e295 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                    let _e296 = frag_color2_;
                    color2_3 = (_e295 * _e296);
                    let _e298 = color0_;
                    let _e299 = color1_3;
                    let _e301 = color1_3[3u];
                    let _e304 = color2_3;
                    let _e306 = color2_3[3u];
                    base = mix(mix(_e298, _e299, vec4(_e301)), _e304, vec4(_e306));
                } else {
                    if override_type_4_6 {
                        param_32 = 1u;
                        let _e309 = frag_tex_coord1_1;
                        param_33 = _e309;
                        param_34 = 1i;
                        let _e310 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                        let _e311 = frag_color1_;
                        color1_4 = (_e310 * _e311);
                        param_35 = 2u;
                        let _e313 = frag_tex_coord2_1;
                        param_36 = _e313;
                        param_37 = 2i;
                        let _e314 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                        let _e315 = frag_color2_;
                        color2_4 = (_e314 * _e315);
                        let _e317 = color2_4;
                        let _e318 = color1_4;
                        let _e319 = color0_;
                        let _e321 = color1_4[3u];
                        let _e325 = color2_4[3u];
                        base = mix(_e317, mix(_e318, _e319, vec4(_e321)), vec4(_e325));
                    } else {
                        if override_type_4_7 {
                            param_38 = 1u;
                            let _e328 = frag_tex_coord1_1;
                            param_39 = _e328;
                            param_40 = 1i;
                            let _e329 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_38), (&param_39), (&param_40));
                            let _e330 = frag_color1_;
                            color1_5 = (_e329 * _e330);
                            param_41 = 2u;
                            let _e332 = frag_tex_coord2_1;
                            param_42 = _e332;
                            param_43 = 2i;
                            let _e333 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_41), (&param_42), (&param_43));
                            let _e334 = frag_color2_;
                            color2_5 = (_e333 * _e334);
                            let _e336 = color2_5;
                            let _e338 = color2_5[3u];
                            let _e341 = color1_5;
                            let _e343 = color1_5[3u];
                            let _e347 = color0_;
                            base = (((_e336 + vec4(_e338)) * (_e341 + vec4(_e343))) * _e347);
                        } else {
                            param_44 = 1u;
                            let _e349 = frag_tex_coord1_1;
                            param_45 = _e349;
                            param_46 = 1i;
                            let _e350 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_44), (&param_45), (&param_46));
                            let _e351 = frag_color1_;
                            color1_6 = (_e350 * _e351);
                            param_47 = 2u;
                            let _e353 = frag_tex_coord2_1;
                            param_48 = _e353;
                            param_49 = 2i;
                            let _e354 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_47), (&param_48), (&param_49));
                            let _e355 = frag_color2_;
                            color2_6 = (_e354 * _e355);
                            let _e357 = color0_;
                            let _e359 = color1_6;
                            let _e362 = color2_6;
                            let _e364 = ((_e357.xyz * _e359.xyz) * _e362.xyz);
                            base[0u] = _e364.x;
                            base[1u] = _e364.y;
                            base[2u] = _e364.z;
                            let _e372 = color0_[3u];
                            let _e374 = color1_6[3u];
                            let _e377 = color2_6[3u];
                            base[3u] = ((_e372 * _e374) * _e377);
                        }
                    }
                }
            }
        }
    }
    if override_type_4_8 {
        let _e382 = unnamed.worldLightParams[1u];
        wetness = clamp(_e382, 0f, 1f);
        let _e386 = unnamed.worldLightParams[2u];
        frost = clamp(_e386, 0f, 1f);
        let _e388 = base;
        luminance = dot(_e388.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e391 = wetness;
        let _e393 = base;
        let _e395 = (_e393.xyz * mix(1f, 0.82f, _e391));
        base[0u] = _e395.x;
        base[1u] = _e395.y;
        base[2u] = _e395.z;
        let _e402 = base;
        let _e404 = luminance;
        let _e406 = luminance;
        let _e408 = luminance;
        let _e410 = frost;
        let _e413 = mix(_e402.xyz, vec3<f32>((_e404 * 0.88f), (_e406 * 0.94f), _e408), vec3((_e410 * 0.55f)));
        base[0u] = _e413.x;
        base[1u] = _e413.y;
        base[2u] = _e413.z;
    }
    let _e420 = color0_;
    let _e423 = unnamed.emissionRadiance;
    let _e426 = base;
    let _e428 = (_e426.xyz + (_e420.xyz * _e423.xyz));
    base[0u] = _e428.x;
    base[1u] = _e428.y;
    base[2u] = _e428.z;
    let _e435 = wired_advanced_fog_enabled_u0028_();
    if _e435 {
        let _e436 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e436;
        let _e437 = base;
        let _e440 = unnamed.advancedFogColorDensity;
        let _e442 = fogAmount;
        let _e444 = mix(_e437.xyz, _e440.xyz, vec3(_e442));
        base[0u] = _e444.x;
        base[1u] = _e444.y;
        base[2u] = _e444.z;
    }
    if override_type_4_9 {
        let _e452 = base[3u];
        if (_e452 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e454 = base;
            let _e456 = base;
            if (dot(_e454.xyz, _e456.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e460 = base;
    out_color = _e460;
    param_50 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_50));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e23 = out_temporal_velocity;
    let _e24 = out_temporal_validity;
    let _e25 = out_color;
    return FragmentOutput(_e23, _e24, _e25);
}
