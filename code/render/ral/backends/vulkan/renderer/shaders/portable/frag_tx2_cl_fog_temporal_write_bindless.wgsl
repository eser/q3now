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
    var phi_78_: bool;

    let _e91 = (*value);
    let _e92 = (*value);
    let _e94 = all((_e91 == _e92));
    phi_78_ = _e94;
    if _e94 {
        let _e95 = (*value);
        phi_78_ = all((abs(_e95) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e100 = phi_78_;
    return _e100;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e91 = (*value_1);
    let _e92 = (*value_1);
    let _e94 = all((_e91 == _e92));
    phi_63_ = _e94;
    if _e94 {
        let _e95 = (*value_1);
        phi_63_ = all((abs(_e95) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e100 = phi_63_;
    return _e100;
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
    let _e101 = temporalOutcome_1;
    let _e102 = (_e101 != 1u);
    phi_101_ = _e102;
    if !(_e102) {
        let _e104 = temporalCurrentClip_1;
        param = _e104;
        let _e105 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e105);
    }
    let _e108 = phi_101_;
    phi_110_ = _e108;
    if !(_e108) {
        let _e110 = temporalPreviousClip_1;
        param_1 = _e110;
        let _e111 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e111);
    }
    let _e114 = phi_110_;
    phi_120_ = _e114;
    if !(_e114) {
        let _e117 = temporalCurrentClip_1[3u];
        phi_120_ = (_e117 <= 0.000001f);
    }
    let _e120 = phi_120_;
    phi_127_ = _e120;
    if !(_e120) {
        let _e123 = temporalPreviousClip_1[3u];
        phi_127_ = (_e123 <= 0.000001f);
    }
    let _e126 = phi_127_;
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
    phi_156_ = _e141;
    if !(_e141) {
        let _e143 = previousNdc;
        param_3 = _e143;
        let _e144 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e144);
    }
    let _e147 = phi_156_;
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
    out_temporal_validity = clamp(_e161, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e93 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e93 + 0.5f));
    let _e98 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e100 = fogType;
    let _e103 = fogType;
    return (((_e98 > 0.5f) && (_e100 >= 1i)) && (_e103 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e93 = wired_advanced_fog_enabled_u0028_();
    if !(_e93) {
        return 0f;
    }
    let _e96 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e96, 0.000001f));
    let _e101 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e101 + 0.5f));
    let _e104 = fogType_1;
    if (_e104 == 1i) {
        let _e108 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e108 <= 0f) {
            return 0f;
        }
        let _e110 = viewDepth;
        let _e113 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e110 / _e113), 0f, 1f);
    }
    let _e118 = unnamed.advancedFogColorDensity[3u];
    let _e120 = viewDepth;
    opticalDepth = (max(_e118, 0f) * _e120);
    let _e122 = fogType_1;
    if (_e122 == 2i) {
        let _e124 = opticalDepth;
        return clamp((1f - exp(-(_e124))), 0f, 1f);
    }
    let _e129 = opticalDepth;
    let _e130 = opticalDepth;
    return clamp((1f - exp(-((_e129 * _e130)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e93 = (*rgb);
    let _e96 = unnamed.worldLightParams[0u];
    boosted = (_e93 * _e96);
    let _e99 = boosted[0u];
    let _e101 = boosted[1u];
    let _e103 = boosted[2u];
    peak = max(_e99, max(_e101, _e103));
    let _e106 = peak;
    if (_e106 > 1f) {
        let _e108 = peak;
        let _e109 = boosted;
        boosted = (_e109 / vec3(_e108));
    }
    let _e112 = boosted;
    return _e112;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e94 = (*c);
    (*c) = max(_e94, vec3<f32>(0f, 0f, 0f));
    let _e96 = (*c);
    cutoff = (_e96 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e98 = (*c);
    lo = (_e98 / vec3(12.92f));
    let _e101 = (*c);
    hi = pow(((_e101 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e106 = hi;
    let _e107 = lo;
    let _e108 = cutoff;
    return mix(_e106, _e107, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e108));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e106 = (*role);
    let _e108 = (*role);
    let _e113 = unnamed.packed_indices[(_e106 / 4u)][(_e108 % 4u)];
    let _e118 = (*uv);
    let _e119 = textureSample(wired_bindless_images[(_e103 & 4095u)], wired_bindless_samplers[((_e113 >> bitcast<u32>(12i)) & 255u)], _e118);
    c_1 = _e119;
    let _e120 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e120))) == 0i) {
        let _e125 = c_1;
        param_5 = _e125.xyz;
        let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e127.x;
        c_1[1u] = _e127.y;
        c_1[2u] = _e127.z;
    }
    let _e134 = (*slot);
    if (lightmap_slot == (_e134 + 1i)) {
        let _e137 = c_1;
        param_6 = _e137.xyz;
        let _e139 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
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

    let _e133 = unnamed.packed_indices[0i][3u];
    let _e139 = unnamed.packed_indices[0i][3u];
    let _e144 = fog_tex_coord_1;
    let _e145 = textureSample(wired_bindless_images[(_e133 & 4095u)], wired_bindless_samplers[((_e139 >> bitcast<u32>(12i)) & 255u)], _e144);
    fog = _e145;
    let _e146 = frag_color0In_1;
    param_7 = _e146.xyz;
    let _e148 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e150 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e148.x, _e148.y, _e148.z, _e150);
    let _e155 = frag_color1In_1;
    param_8 = _e155.xyz;
    let _e157 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e159 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e157.x, _e157.y, _e157.z, _e159);
    let _e164 = frag_color2In_1;
    param_9 = _e164.xyz;
    let _e166 = sRGBToLinear_u0028_vf3_u003b((&param_9));
    let _e168 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e166.x, _e166.y, _e166.z, _e168);
    param_10 = 0u;
    let _e173 = frag_tex_coord0_1;
    param_11 = _e173;
    param_12 = 0i;
    let _e174 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor0_ = _e174;
    param_13 = 1u;
    let _e175 = frag_tex_coord1_1;
    param_14 = _e175;
    param_15 = 1i;
    let _e176 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
    sampledColor1_ = _e176;
    param_16 = 2u;
    let _e177 = frag_tex_coord2_1;
    param_17 = _e177;
    param_18 = 2i;
    let _e178 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
    sampledColor2_ = _e178;
    let _e179 = sampledColor0_;
    let _e180 = frag_color0_;
    color0_ = (_e179 * _e180);
    if override_type_3_2 {
        let _e182 = sampledColor1_;
        let _e183 = frag_color1_;
        color1_ = (_e182 * _e183);
        let _e185 = sampledColor2_;
        let _e186 = frag_color2_;
        color2_ = (_e185 * _e186);
        let _e188 = color0_;
        let _e190 = color1_;
        let _e193 = color2_;
        let _e195 = ((_e188.xyz + _e190.xyz) + _e193.xyz);
        let _e197 = color0_[3u];
        let _e199 = color1_[3u];
        let _e202 = color2_[3u];
        base = vec4<f32>(_e195.x, _e195.y, _e195.z, ((_e197 * _e199) * _e202));
    } else {
        if override_type_3_3 {
            let _e208 = sampledColor1_;
            let _e209 = frag_color1_;
            color1_1 = (_e208 * _e209);
            let _e211 = sampledColor2_;
            let _e212 = frag_color2_;
            color2_1 = (_e211 * _e212);
            let _e215 = color0_[3u];
            let _e216 = color0_;
            color0_ = (_e216 * _e215);
            let _e219 = color1_1[3u];
            let _e220 = color1_1;
            color1_1 = (_e220 * _e219);
            let _e223 = color2_1[3u];
            let _e224 = color2_1;
            color2_1 = (_e224 * _e223);
            let _e226 = color0_;
            let _e228 = color1_1;
            let _e231 = color2_1;
            let _e233 = ((_e226.xyz + _e228.xyz) + _e231.xyz);
            let _e235 = color0_[3u];
            let _e237 = color1_1[3u];
            let _e240 = color2_1[3u];
            base = vec4<f32>(_e233.x, _e233.y, _e233.z, ((_e235 * _e237) * _e240));
        } else {
            if override_type_3_4 {
                let _e246 = sampledColor1_;
                let _e247 = frag_color1_;
                color1_2 = (_e246 * _e247);
                let _e249 = sampledColor2_;
                let _e250 = frag_color2_;
                color2_2 = (_e249 * _e250);
                let _e253 = color0_[3u];
                let _e255 = color0_;
                color0_ = (_e255 * (1f - _e253));
                let _e258 = color1_2[3u];
                let _e260 = color1_2;
                color1_2 = (_e260 * (1f - _e258));
                let _e263 = color2_2[3u];
                let _e265 = color2_2;
                color2_2 = (_e265 * (1f - _e263));
                let _e267 = color0_;
                let _e269 = color1_2;
                let _e272 = color2_2;
                let _e274 = ((_e267.xyz + _e269.xyz) + _e272.xyz);
                let _e276 = color0_[3u];
                let _e278 = color1_2[3u];
                let _e281 = color2_2[3u];
                base = vec4<f32>(_e274.x, _e274.y, _e274.z, ((_e276 * _e278) * _e281));
            } else {
                if override_type_3_5 {
                    let _e287 = sampledColor1_;
                    let _e288 = frag_color1_;
                    color1_3 = (_e287 * _e288);
                    let _e290 = sampledColor2_;
                    let _e291 = frag_color2_;
                    color2_3 = (_e290 * _e291);
                    let _e293 = color0_;
                    let _e294 = color1_3;
                    let _e296 = color1_3[3u];
                    let _e299 = color2_3;
                    let _e301 = color2_3[3u];
                    base = mix(mix(_e293, _e294, vec4(_e296)), _e299, vec4(_e301));
                } else {
                    if override_type_3_6 {
                        let _e304 = sampledColor1_;
                        let _e305 = frag_color1_;
                        color1_4 = (_e304 * _e305);
                        let _e307 = sampledColor2_;
                        let _e308 = frag_color2_;
                        color2_4 = (_e307 * _e308);
                        let _e310 = color2_4;
                        let _e311 = color1_4;
                        let _e312 = color0_;
                        let _e314 = color1_4[3u];
                        let _e318 = color2_4[3u];
                        base = mix(_e310, mix(_e311, _e312, vec4(_e314)), vec4(_e318));
                    } else {
                        if override_type_3_7 {
                            let _e321 = sampledColor1_;
                            let _e322 = frag_color1_;
                            color1_5 = (_e321 * _e322);
                            let _e324 = sampledColor2_;
                            let _e325 = frag_color2_;
                            color2_5 = (_e324 * _e325);
                            let _e327 = color2_5;
                            let _e329 = color2_5[3u];
                            let _e332 = color1_5;
                            let _e334 = color1_5[3u];
                            let _e338 = color0_;
                            base = (((_e327 + vec4(_e329)) * (_e332 + vec4(_e334))) * _e338);
                        } else {
                            let _e340 = sampledColor1_;
                            let _e341 = frag_color1_;
                            color1_6 = (_e340 * _e341);
                            let _e343 = sampledColor2_;
                            let _e344 = frag_color2_;
                            color2_6 = (_e343 * _e344);
                            let _e346 = color0_;
                            let _e348 = color1_6;
                            let _e351 = color2_6;
                            let _e353 = ((_e346.xyz * _e348.xyz) * _e351.xyz);
                            base[0u] = _e353.x;
                            base[1u] = _e353.y;
                            base[2u] = _e353.z;
                            let _e361 = color0_[3u];
                            let _e363 = color1_6[3u];
                            let _e366 = color2_6[3u];
                            base[3u] = ((_e361 * _e363) * _e366);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e371 = unnamed.worldLightParams[1u];
        wetness = clamp(_e371, 0f, 1f);
        let _e375 = unnamed.worldLightParams[2u];
        frost = clamp(_e375, 0f, 1f);
        let _e377 = base;
        luminance = dot(_e377.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e380 = wetness;
        let _e382 = base;
        let _e384 = (_e382.xyz * mix(1f, 0.82f, _e380));
        base[0u] = _e384.x;
        base[1u] = _e384.y;
        base[2u] = _e384.z;
        let _e391 = base;
        let _e393 = luminance;
        let _e395 = luminance;
        let _e397 = luminance;
        let _e399 = frost;
        let _e402 = mix(_e391.xyz, vec3<f32>((_e393 * 0.88f), (_e395 * 0.94f), _e397), vec3((_e399 * 0.55f)));
        base[0u] = _e402.x;
        base[1u] = _e402.y;
        base[2u] = _e402.z;
    }
    let _e411 = unnamed.materialFactors[0u];
    if (_e411 >= 0f) {
        let _e414 = unnamed.materialBaseColor;
        let _e415 = base;
        base = (_e415 * _e414);
    }
    let _e417 = color0_;
    let _e420 = unnamed.emissionRadiance;
    let _e423 = base;
    let _e425 = (_e423.xyz + (_e417.xyz * _e420.xyz));
    base[0u] = _e425.x;
    base[1u] = _e425.y;
    base[2u] = _e425.z;
    let _e432 = wired_advanced_fog_enabled_u0028_();
    if _e432 {
        let _e433 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e433;
        if override_type_3_9 {
            let _e434 = fogAmount;
            let _e436 = base;
            let _e438 = (_e436.xyz * (1f - _e434));
            base[0u] = _e438.x;
            base[1u] = _e438.y;
            base[2u] = _e438.z;
        } else {
            if override_type_3_10 {
                let _e445 = fogAmount;
                let _e447 = base;
                base = (_e447 * (1f - _e445));
            } else {
                if override_type_3_11 {
                    let _e449 = fogAmount;
                    let _e452 = base[3u];
                    base[3u] = (_e452 * (1f - _e449));
                } else {
                    let _e455 = base;
                    let _e458 = unnamed.advancedFogColorDensity;
                    let _e460 = fogAmount;
                    let _e462 = mix(_e455.xyz, _e458.xyz, vec3(_e460));
                    base[0u] = _e462.x;
                    base[1u] = _e462.y;
                    base[2u] = _e462.z;
                }
            }
        }
    } else {
        if override_type_3_12 {
            let _e469 = base;
            let _e472 = fog[3u];
            let _e474 = (_e469.xyz * (1f - _e472));
            base[0u] = _e474.x;
            base[1u] = _e474.y;
            base[2u] = _e474.z;
        } else {
            if override_type_3_13 {
                let _e481 = base;
                let _e483 = fog[3u];
                base = (_e481 * (1f - _e483));
            } else {
                if override_type_3_14 {
                    let _e487 = base[3u];
                    let _e489 = fog[3u];
                    base[3u] = (_e487 * (1f - _e489));
                } else {
                    let _e493 = base;
                    let _e494 = fog;
                    let _e496 = unnamed.fogColor;
                    let _e499 = fog[3u];
                    base = mix(_e493, (_e494 * _e496), vec4(_e499));
                }
            }
        }
    }
    if override_type_3_15 {
        let _e503 = base[3u];
        if (_e503 == 0f) {
            discard;
        }
    } else {
        if override_type_3_16 {
            let _e505 = base;
            let _e507 = base;
            if (dot(_e505.xyz, _e507.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e511 = base;
    out_color = _e511;
    param_19 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_19));
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
