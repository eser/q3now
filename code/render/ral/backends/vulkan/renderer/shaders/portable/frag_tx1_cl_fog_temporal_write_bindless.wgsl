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
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: vec4<f32>,
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
var<private> out_temporal_validity: vec4<f32>;
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e90 = (*value);
    let _e91 = (*value);
    let _e93 = all((_e90 == _e91));
    phi_79_ = _e93;
    if _e93 {
        let _e94 = (*value);
        phi_79_ = all((abs(_e94) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e99 = phi_79_;
    return _e99;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e90 = (*value_1);
    let _e91 = (*value_1);
    let _e93 = all((_e90 == _e91));
    phi_64_ = _e93;
    if _e93 {
        let _e94 = (*value_1);
        phi_64_ = all((abs(_e94) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e99 = phi_64_;
    return _e99;
}

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
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
    var phi_103_: bool;
    var phi_112_: bool;
    var phi_122_: bool;
    var phi_129_: bool;
    var phi_158_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e101 = temporalOutcome_1;
    let _e102 = (_e101 != 1u);
    phi_103_ = _e102;
    if !(_e102) {
        let _e104 = temporalCurrentClip_1;
        param = _e104;
        let _e105 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e105);
    }
    let _e108 = phi_103_;
    phi_112_ = _e108;
    if !(_e108) {
        let _e110 = temporalPreviousClip_1;
        param_1 = _e110;
        let _e111 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e111);
    }
    let _e114 = phi_112_;
    phi_122_ = _e114;
    if !(_e114) {
        let _e117 = temporalCurrentClip_1[3u];
        phi_122_ = (_e117 <= 0.000001f);
    }
    let _e120 = phi_122_;
    phi_129_ = _e120;
    if !(_e120) {
        let _e123 = temporalPreviousClip_1[3u];
        phi_129_ = (_e123 <= 0.000001f);
    }
    let _e126 = phi_129_;
    if _e126 {
        return;
    }
    let _e127 = temporalCurrentClip_1;
    let _e130 = temporalCurrentClip_1[3u];
    currentNdc = (_e127.xy / vec2(_e130));
    let _e133 = temporalPreviousClip_1;
    let _e136 = temporalPreviousClip_1[3u];
    previousNdc = (_e133.xy / vec2(_e136));
    let _e139 = currentNdc;
    param_2 = _e139;
    let _e140 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e141 = !(_e140);
    phi_158_ = _e141;
    if !(_e141) {
        let _e143 = previousNdc;
        param_3 = _e143;
        let _e144 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e144);
    }
    let _e147 = phi_158_;
    if _e147 {
        return;
    }
    let _e148 = currentNdc;
    currentUv = ((_e148 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e151 = previousNdc;
    previousUv = ((_e151 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e154 = currentUv;
    let _e155 = previousUv;
    velocity = (_e154 - _e155);
    let _e157 = velocity;
    param_4 = _e157;
    let _e158 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e158) {
        return;
    }
    let _e160 = velocity;
    out_temporal_velocity = _e160;
    let _e161 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e161, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e92 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e92 + 0.5f));
    let _e97 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e99 = fogType;
    let _e102 = fogType;
    return (((_e97 > 0.5f) && (_e99 >= 1i)) && (_e102 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e92 = wired_advanced_fog_enabled_u0028_();
    if !(_e92) {
        return 0f;
    }
    let _e95 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e95, 0.000001f));
    let _e100 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e100 + 0.5f));
    let _e103 = fogType_1;
    if (_e103 == 1i) {
        let _e107 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e107 <= 0f) {
            return 0f;
        }
        let _e109 = viewDepth;
        let _e112 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e109 / _e112), 0f, 1f);
    }
    let _e117 = unnamed.advancedFogColorDensity[3u];
    let _e119 = viewDepth;
    opticalDepth = (max(_e117, 0f) * _e119);
    let _e121 = fogType_1;
    if (_e121 == 2i) {
        let _e123 = opticalDepth;
        return clamp((1f - exp(-(_e123))), 0f, 1f);
    }
    let _e128 = opticalDepth;
    let _e129 = opticalDepth;
    return clamp((1f - exp(-((_e128 * _e129)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e92 = (*rgb);
    let _e95 = unnamed.worldLightParams[0u];
    boosted = (_e92 * _e95);
    let _e98 = boosted[0u];
    let _e100 = boosted[1u];
    let _e102 = boosted[2u];
    peak = max(_e98, max(_e100, _e102));
    let _e105 = peak;
    if (_e105 > 1f) {
        let _e107 = peak;
        let _e108 = boosted;
        boosted = (_e108 / vec3(_e107));
    }
    let _e111 = boosted;
    return _e111;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e93 = (*c);
    (*c) = max(_e93, vec3<f32>(0f, 0f, 0f));
    let _e95 = (*c);
    cutoff = (_e95 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e97 = (*c);
    lo = (_e97 / vec3(12.92f));
    let _e100 = (*c);
    hi = pow(((_e100 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e105 = hi;
    let _e106 = lo;
    let _e107 = cutoff;
    return mix(_e105, _e106, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e107));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e95 = (*role);
    let _e97 = (*role);
    let _e102 = unnamed.packed_indices[(_e95 / 4u)][(_e97 % 4u)];
    let _e105 = (*role);
    let _e107 = (*role);
    let _e112 = unnamed.packed_indices[(_e105 / 4u)][(_e107 % 4u)];
    let _e117 = (*uv);
    let _e118 = textureSample(wired_bindless_images[(_e102 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    c_1 = _e118;
    let _e119 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e119))) == 0i) {
        let _e124 = c_1;
        param_5 = _e124.xyz;
        let _e126 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e126.x;
        c_1[1u] = _e126.y;
        c_1[2u] = _e126.z;
    }
    let _e133 = (*slot);
    if (lightmap_slot == (_e133 + 1i)) {
        let _e136 = c_1;
        param_6 = _e136.xyz;
        let _e138 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e138.x;
        c_1[1u] = _e138.y;
        c_1[2u] = _e138.z;
    }
    let _e145 = c_1;
    return _e145;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var sampledColor1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_15: f32;
    var param_16: vec4<f32>;

    let _e120 = unnamed.packed_indices[0i][3u];
    let _e126 = unnamed.packed_indices[0i][3u];
    let _e131 = fog_tex_coord_1;
    let _e132 = textureSample(wired_bindless_images[(_e120 & 4095u)], wired_bindless_samplers[((_e126 >> bitcast<u32>(12i)) & 255u)], _e131);
    fog = _e132;
    let _e133 = frag_color0In_1;
    param_7 = _e133.xyz;
    let _e135 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e137 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e135.x, _e135.y, _e135.z, _e137);
    let _e142 = frag_color1In_1;
    param_8 = _e142.xyz;
    let _e144 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e146 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e144.x, _e144.y, _e144.z, _e146);
    param_9 = 0u;
    let _e151 = frag_tex_coord0_1;
    param_10 = _e151;
    param_11 = 0i;
    let _e152 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor0_ = _e152;
    param_12 = 1u;
    let _e153 = frag_tex_coord1_1;
    param_13 = _e153;
    param_14 = 1i;
    let _e154 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    sampledColor1_ = _e154;
    let _e155 = sampledColor0_;
    let _e156 = frag_color0_;
    color0_ = (_e155 * _e156);
    if override_type_3_2 {
        let _e158 = sampledColor1_;
        let _e159 = frag_color1_;
        color1_ = (_e158 * _e159);
        let _e161 = color0_;
        let _e163 = color1_;
        let _e165 = (_e161.xyz + _e163.xyz);
        let _e167 = color0_[3u];
        let _e169 = color1_[3u];
        base = vec4<f32>(_e165.x, _e165.y, _e165.z, (_e167 * _e169));
    } else {
        if override_type_3_3 {
            let _e175 = sampledColor1_;
            let _e176 = frag_color1_;
            color1_1 = (_e175 * _e176);
            let _e179 = color0_[3u];
            let _e180 = color0_;
            color0_ = (_e180 * _e179);
            let _e183 = color1_1[3u];
            let _e184 = color1_1;
            color1_1 = (_e184 * _e183);
            let _e186 = color0_;
            let _e188 = color1_1;
            let _e190 = (_e186.xyz + _e188.xyz);
            let _e192 = color0_[3u];
            let _e194 = color1_1[3u];
            base = vec4<f32>(_e190.x, _e190.y, _e190.z, (_e192 * _e194));
        } else {
            if override_type_3_4 {
                let _e200 = sampledColor1_;
                let _e201 = frag_color1_;
                color1_2 = (_e200 * _e201);
                let _e204 = color0_[3u];
                let _e206 = color0_;
                color0_ = (_e206 * (1f - _e204));
                let _e209 = color1_2[3u];
                let _e211 = color1_2;
                color1_2 = (_e211 * (1f - _e209));
                let _e213 = color0_;
                let _e215 = color1_2;
                let _e217 = (_e213.xyz + _e215.xyz);
                let _e219 = color0_[3u];
                let _e221 = color1_2[3u];
                base = vec4<f32>(_e217.x, _e217.y, _e217.z, (_e219 * _e221));
            } else {
                if override_type_3_5 {
                    let _e227 = sampledColor1_;
                    let _e228 = frag_color1_;
                    color1_3 = (_e227 * _e228);
                    let _e230 = color0_;
                    let _e231 = color1_3;
                    let _e233 = color1_3[3u];
                    base = mix(_e230, _e231, vec4(_e233));
                } else {
                    if override_type_3_6 {
                        let _e236 = sampledColor1_;
                        let _e237 = frag_color1_;
                        color1_4 = (_e236 * _e237);
                        let _e239 = color1_4;
                        let _e240 = color0_;
                        let _e242 = color1_4[3u];
                        base = mix(_e239, _e240, vec4(_e242));
                    } else {
                        if override_type_3_7 {
                            let _e245 = sampledColor1_;
                            let _e246 = frag_color1_;
                            color1_5 = (_e245 * _e246);
                            let _e248 = color1_5;
                            let _e250 = color1_5[3u];
                            let _e253 = color0_;
                            base = ((_e248 + vec4(_e250)) * _e253);
                        } else {
                            let _e255 = sampledColor1_;
                            let _e256 = frag_color1_;
                            color1_6 = (_e255 * _e256);
                            let _e258 = color0_;
                            let _e260 = color1_6;
                            let _e262 = (_e258.xyz * _e260.xyz);
                            base[0u] = _e262.x;
                            base[1u] = _e262.y;
                            base[2u] = _e262.z;
                            let _e270 = color0_[3u];
                            let _e272 = color1_6[3u];
                            base[3u] = (_e270 * _e272);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e277 = unnamed.worldLightParams[1u];
        wetness = clamp(_e277, 0f, 1f);
        let _e281 = unnamed.worldLightParams[2u];
        frost = clamp(_e281, 0f, 1f);
        let _e283 = base;
        luminance = dot(_e283.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e286 = wetness;
        let _e288 = base;
        let _e290 = (_e288.xyz * mix(1f, 0.82f, _e286));
        base[0u] = _e290.x;
        base[1u] = _e290.y;
        base[2u] = _e290.z;
        let _e297 = base;
        let _e299 = luminance;
        let _e301 = luminance;
        let _e303 = luminance;
        let _e305 = frost;
        let _e308 = mix(_e297.xyz, vec3<f32>((_e299 * 0.88f), (_e301 * 0.94f), _e303), vec3((_e305 * 0.55f)));
        base[0u] = _e308.x;
        base[1u] = _e308.y;
        base[2u] = _e308.z;
    }
    let _e317 = unnamed.materialFactors[0u];
    if (_e317 >= 0f) {
        let _e320 = unnamed.materialBaseColor;
        let _e321 = base;
        base = (_e321 * _e320);
    }
    let _e323 = color0_;
    let _e326 = unnamed.emissionRadiance;
    let _e329 = base;
    let _e331 = (_e329.xyz + (_e323.xyz * _e326.xyz));
    base[0u] = _e331.x;
    base[1u] = _e331.y;
    base[2u] = _e331.z;
    let _e338 = wired_advanced_fog_enabled_u0028_();
    if _e338 {
        let _e339 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e339;
        if override_type_3_9 {
            let _e340 = fogAmount;
            let _e342 = base;
            let _e344 = (_e342.xyz * (1f - _e340));
            base[0u] = _e344.x;
            base[1u] = _e344.y;
            base[2u] = _e344.z;
        } else {
            if override_type_3_10 {
                let _e351 = fogAmount;
                let _e353 = base;
                base = (_e353 * (1f - _e351));
            } else {
                if override_type_3_11 {
                    let _e355 = fogAmount;
                    let _e358 = base[3u];
                    base[3u] = (_e358 * (1f - _e355));
                } else {
                    let _e361 = base;
                    let _e364 = unnamed.advancedFogColorDensity;
                    let _e366 = fogAmount;
                    let _e368 = mix(_e361.xyz, _e364.xyz, vec3(_e366));
                    base[0u] = _e368.x;
                    base[1u] = _e368.y;
                    base[2u] = _e368.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e375 = base;
            let _e378 = fog[3u];
            let _e380 = (_e375.xyz * (1f - _e378));
            base[0u] = _e380.x;
            base[1u] = _e380.y;
            base[2u] = _e380.z;
        } else {
            if override_type_3_13 {
                let _e387 = base;
                let _e389 = fog[3u];
                base = (_e387 * (1f - _e389));
            } else {
                if override_type_3_14 {
                    let _e393 = base[3u];
                    let _e395 = fog[3u];
                    base[3u] = (_e393 * (1f - _e395));
                } else {
                    let _e399 = base;
                    let _e400 = fog;
                    let _e402 = unnamed.fogColor;
                    let _e405 = fog[3u];
                    base = mix(_e399, (_e400 * _e402), vec4(_e405));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e409 = base[3u];
        if (_e409 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e411 = base;
            let _e413 = base;
            if (dot(_e411.xyz, _e413.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e417 = base;
    out_color = _e417;
    param_15 = 1f;
    let _e418 = base;
    param_16 = _e418;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_15), (&param_16));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
