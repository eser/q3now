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
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e84 = (*value);
    let _e85 = (*value);
    let _e87 = all((_e84 == _e85));
    phi_79_ = _e87;
    if _e87 {
        let _e88 = (*value);
        phi_79_ = all((abs(_e88) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e93 = phi_79_;
    return _e93;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e84 = (*value_1);
    let _e85 = (*value_1);
    let _e87 = all((_e84 == _e85));
    phi_64_ = _e87;
    if _e87 {
        let _e88 = (*value_1);
        phi_64_ = all((abs(_e88) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e93 = phi_64_;
    return _e93;
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
    let _e95 = temporalOutcome_1;
    let _e96 = (_e95 != 1u);
    phi_103_ = _e96;
    if !(_e96) {
        let _e98 = temporalCurrentClip_1;
        param = _e98;
        let _e99 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e99);
    }
    let _e102 = phi_103_;
    phi_112_ = _e102;
    if !(_e102) {
        let _e104 = temporalPreviousClip_1;
        param_1 = _e104;
        let _e105 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e105);
    }
    let _e108 = phi_112_;
    phi_122_ = _e108;
    if !(_e108) {
        let _e111 = temporalCurrentClip_1[3u];
        phi_122_ = (_e111 <= 0.000001f);
    }
    let _e114 = phi_122_;
    phi_129_ = _e114;
    if !(_e114) {
        let _e117 = temporalPreviousClip_1[3u];
        phi_129_ = (_e117 <= 0.000001f);
    }
    let _e120 = phi_129_;
    if _e120 {
        return;
    }
    let _e121 = temporalCurrentClip_1;
    let _e124 = temporalCurrentClip_1[3u];
    currentNdc = (_e121.xy / vec2(_e124));
    let _e127 = temporalPreviousClip_1;
    let _e130 = temporalPreviousClip_1[3u];
    previousNdc = (_e127.xy / vec2(_e130));
    let _e133 = currentNdc;
    param_2 = _e133;
    let _e134 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e135 = !(_e134);
    phi_158_ = _e135;
    if !(_e135) {
        let _e137 = previousNdc;
        param_3 = _e137;
        let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e138);
    }
    let _e141 = phi_158_;
    if _e141 {
        return;
    }
    let _e142 = currentNdc;
    currentUv = ((_e142 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e145 = previousNdc;
    previousUv = ((_e145 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e148 = currentUv;
    let _e149 = previousUv;
    velocity = (_e148 - _e149);
    let _e151 = velocity;
    param_4 = _e151;
    let _e152 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e152) {
        return;
    }
    let _e154 = velocity;
    out_temporal_velocity = _e154;
    let _e155 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e155, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e86 + 0.5f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e93 = fogType;
    let _e96 = fogType;
    return (((_e91 > 0.5f) && (_e93 >= 1i)) && (_e96 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e86 = wired_advanced_fog_enabled_u0028_();
    if !(_e86) {
        return 0f;
    }
    let _e89 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e89, 0.000001f));
    let _e94 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e94 + 0.5f));
    let _e97 = fogType_1;
    if (_e97 == 1i) {
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e101 <= 0f) {
            return 0f;
        }
        let _e103 = viewDepth;
        let _e106 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e103 / _e106), 0f, 1f);
    }
    let _e111 = unnamed.advancedFogColorDensity[3u];
    let _e113 = viewDepth;
    opticalDepth = (max(_e111, 0f) * _e113);
    let _e115 = fogType_1;
    if (_e115 == 2i) {
        let _e117 = opticalDepth;
        return clamp((1f - exp(-(_e117))), 0f, 1f);
    }
    let _e122 = opticalDepth;
    let _e123 = opticalDepth;
    return clamp((1f - exp(-((_e122 * _e123)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e86 = (*rgb);
    let _e89 = unnamed.worldLightParams[0u];
    boosted = (_e86 * _e89);
    let _e92 = boosted[0u];
    let _e94 = boosted[1u];
    let _e96 = boosted[2u];
    peak = max(_e92, max(_e94, _e96));
    let _e99 = peak;
    if (_e99 > 1f) {
        let _e101 = peak;
        let _e102 = boosted;
        boosted = (_e102 / vec3(_e101));
    }
    let _e105 = boosted;
    return _e105;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e87 = (*c);
    (*c) = max(_e87, vec3<f32>(0f, 0f, 0f));
    let _e89 = (*c);
    cutoff = (_e89 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e91 = (*c);
    lo = (_e91 / vec3(12.92f));
    let _e94 = (*c);
    hi = pow(((_e94 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e99 = hi;
    let _e100 = lo;
    let _e101 = cutoff;
    return mix(_e99, _e100, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e101));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e99 = (*role);
    let _e101 = (*role);
    let _e106 = unnamed.packed_indices[(_e99 / 4u)][(_e101 % 4u)];
    let _e111 = (*uv);
    let _e112 = textureSample(wired_bindless_images[(_e96 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    c_1 = _e112;
    let _e113 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e113))) == 0i) {
        let _e118 = c_1;
        param_5 = _e118.xyz;
        let _e120 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = (*slot);
    if (lightmap_slot == (_e127 + 1i)) {
        let _e130 = c_1;
        param_6 = _e130.xyz;
        let _e132 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e132.x;
        c_1[1u] = _e132.y;
        c_1[2u] = _e132.z;
    }
    let _e139 = c_1;
    return _e139;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_9: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var sampledColor1_: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var sampledColor2_: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color2_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color2_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color2_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var color2_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_19: f32;
    var param_20: vec4<f32>;

    let _e123 = frag_color0In_1;
    param_7 = _e123.xyz;
    let _e125 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e127 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e125.x, _e125.y, _e125.z, _e127);
    let _e132 = frag_color1In_1;
    param_8 = _e132.xyz;
    let _e134 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e136 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e134.x, _e134.y, _e134.z, _e136);
    let _e141 = frag_color2In_1;
    param_9 = _e141.xyz;
    let _e143 = sRGBToLinear_u0028_vf3_u003b((&param_9));
    let _e145 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e143.x, _e143.y, _e143.z, _e145);
    param_10 = 0u;
    let _e150 = frag_tex_coord0_1;
    param_11 = _e150;
    param_12 = 0i;
    let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor0_ = _e151;
    param_13 = 1u;
    let _e152 = frag_tex_coord1_1;
    param_14 = _e152;
    param_15 = 1i;
    let _e153 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    sampledColor1_ = _e153;
    param_16 = 2u;
    let _e154 = frag_tex_coord2_1;
    param_17 = _e154;
    param_18 = 2i;
    let _e155 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor2_ = _e155;
    let _e156 = sampledColor0_;
    let _e157 = frag_color0_;
    color0_ = (_e156 * _e157);
    if override_type_3_2 {
        let _e159 = sampledColor1_;
        let _e160 = frag_color1_;
        color1_ = (_e159 * _e160);
        let _e162 = sampledColor2_;
        let _e163 = frag_color2_;
        color2_ = (_e162 * _e163);
        let _e165 = color0_;
        let _e167 = color1_;
        let _e170 = color2_;
        let _e172 = ((_e165.xyz + _e167.xyz) + _e170.xyz);
        let _e174 = color0_[3u];
        let _e176 = color1_[3u];
        let _e179 = color2_[3u];
        base = vec4<f32>(_e172.x, _e172.y, _e172.z, ((_e174 * _e176) * _e179));
    } else {
        if override_type_3_3 {
            let _e185 = sampledColor1_;
            let _e186 = frag_color1_;
            color1_1 = (_e185 * _e186);
            let _e188 = sampledColor2_;
            let _e189 = frag_color2_;
            color2_1 = (_e188 * _e189);
            let _e192 = color0_[3u];
            let _e193 = color0_;
            color0_ = (_e193 * _e192);
            let _e196 = color1_1[3u];
            let _e197 = color1_1;
            color1_1 = (_e197 * _e196);
            let _e200 = color2_1[3u];
            let _e201 = color2_1;
            color2_1 = (_e201 * _e200);
            let _e203 = color0_;
            let _e205 = color1_1;
            let _e208 = color2_1;
            let _e210 = ((_e203.xyz + _e205.xyz) + _e208.xyz);
            let _e212 = color0_[3u];
            let _e214 = color1_1[3u];
            let _e217 = color2_1[3u];
            base = vec4<f32>(_e210.x, _e210.y, _e210.z, ((_e212 * _e214) * _e217));
        } else {
            if override_type_3_4 {
                let _e223 = sampledColor1_;
                let _e224 = frag_color1_;
                color1_2 = (_e223 * _e224);
                let _e226 = sampledColor2_;
                let _e227 = frag_color2_;
                color2_2 = (_e226 * _e227);
                let _e230 = color0_[3u];
                let _e232 = color0_;
                color0_ = (_e232 * (1f - _e230));
                let _e235 = color1_2[3u];
                let _e237 = color1_2;
                color1_2 = (_e237 * (1f - _e235));
                let _e240 = color2_2[3u];
                let _e242 = color2_2;
                color2_2 = (_e242 * (1f - _e240));
                let _e244 = color0_;
                let _e246 = color1_2;
                let _e249 = color2_2;
                let _e251 = ((_e244.xyz + _e246.xyz) + _e249.xyz);
                let _e253 = color0_[3u];
                let _e255 = color1_2[3u];
                let _e258 = color2_2[3u];
                base = vec4<f32>(_e251.x, _e251.y, _e251.z, ((_e253 * _e255) * _e258));
            } else {
                if override_type_3_5 {
                    let _e264 = sampledColor1_;
                    let _e265 = frag_color1_;
                    color1_3 = (_e264 * _e265);
                    let _e267 = sampledColor2_;
                    let _e268 = frag_color2_;
                    color2_3 = (_e267 * _e268);
                    let _e270 = color0_;
                    let _e271 = color1_3;
                    let _e273 = color1_3[3u];
                    let _e276 = color2_3;
                    let _e278 = color2_3[3u];
                    base = mix(mix(_e270, _e271, vec4(_e273)), _e276, vec4(_e278));
                } else {
                    if override_type_3_6 {
                        let _e281 = sampledColor1_;
                        let _e282 = frag_color1_;
                        color1_4 = (_e281 * _e282);
                        let _e284 = sampledColor2_;
                        let _e285 = frag_color2_;
                        color2_4 = (_e284 * _e285);
                        let _e287 = color2_4;
                        let _e288 = color1_4;
                        let _e289 = color0_;
                        let _e291 = color1_4[3u];
                        let _e295 = color2_4[3u];
                        base = mix(_e287, mix(_e288, _e289, vec4(_e291)), vec4(_e295));
                    } else {
                        if override_type_3_7 {
                            let _e298 = sampledColor1_;
                            let _e299 = frag_color1_;
                            color1_5 = (_e298 * _e299);
                            let _e301 = sampledColor2_;
                            let _e302 = frag_color2_;
                            color2_5 = (_e301 * _e302);
                            let _e304 = color2_5;
                            let _e306 = color2_5[3u];
                            let _e309 = color1_5;
                            let _e311 = color1_5[3u];
                            let _e315 = color0_;
                            base = (((_e304 + vec4(_e306)) * (_e309 + vec4(_e311))) * _e315);
                        } else {
                            let _e317 = sampledColor1_;
                            let _e318 = frag_color1_;
                            color1_6 = (_e317 * _e318);
                            let _e320 = sampledColor2_;
                            let _e321 = frag_color2_;
                            color2_6 = (_e320 * _e321);
                            let _e323 = color0_;
                            let _e325 = color1_6;
                            let _e328 = color2_6;
                            let _e330 = ((_e323.xyz * _e325.xyz) * _e328.xyz);
                            base[0u] = _e330.x;
                            base[1u] = _e330.y;
                            base[2u] = _e330.z;
                            let _e338 = color0_[3u];
                            let _e340 = color1_6[3u];
                            let _e343 = color2_6[3u];
                            base[3u] = ((_e338 * _e340) * _e343);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e348 = unnamed.worldLightParams[1u];
        wetness = clamp(_e348, 0f, 1f);
        let _e352 = unnamed.worldLightParams[2u];
        frost = clamp(_e352, 0f, 1f);
        let _e354 = base;
        luminance = dot(_e354.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e357 = wetness;
        let _e359 = base;
        let _e361 = (_e359.xyz * mix(1f, 0.82f, _e357));
        base[0u] = _e361.x;
        base[1u] = _e361.y;
        base[2u] = _e361.z;
        let _e368 = base;
        let _e370 = luminance;
        let _e372 = luminance;
        let _e374 = luminance;
        let _e376 = frost;
        let _e379 = mix(_e368.xyz, vec3<f32>((_e370 * 0.88f), (_e372 * 0.94f), _e374), vec3((_e376 * 0.55f)));
        base[0u] = _e379.x;
        base[1u] = _e379.y;
        base[2u] = _e379.z;
    }
    let _e388 = unnamed.materialFactors[0u];
    if (_e388 >= 0f) {
        let _e391 = unnamed.materialBaseColor;
        let _e392 = base;
        base = (_e392 * _e391);
    }
    let _e394 = color0_;
    let _e397 = unnamed.emissionRadiance;
    let _e400 = base;
    let _e402 = (_e400.xyz + (_e394.xyz * _e397.xyz));
    base[0u] = _e402.x;
    base[1u] = _e402.y;
    base[2u] = _e402.z;
    let _e409 = wired_advanced_fog_enabled_u0028_();
    if _e409 {
        let _e410 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e410;
        let _e411 = base;
        let _e414 = unnamed.advancedFogColorDensity;
        let _e416 = fogAmount;
        let _e418 = mix(_e411.xyz, _e414.xyz, vec3(_e416));
        base[0u] = _e418.x;
        base[1u] = _e418.y;
        base[2u] = _e418.z;
    }
    if override_type_3_9 {
        let _e426 = base[3u];
        if (_e426 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e428 = base;
            let _e430 = base;
            if (dot(_e428.xyz, _e430.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e434 = base;
    out_color = _e434;
    param_19 = 1f;
    let _e435 = base;
    param_20 = _e435;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_19), (&param_20));
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
