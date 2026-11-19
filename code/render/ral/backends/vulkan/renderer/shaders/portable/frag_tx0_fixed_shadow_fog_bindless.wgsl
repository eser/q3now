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
    _pad_to_cascadeMVP: array<vec4<f32>, 1>,
    cascadeMVP: array<mat4x4<f32>, 4>,
    cascadeSplits: vec4<f32>,
    _pad_to_packed_indices: array<vec4<f32>, 8>,
    packed_indices: array<vec4<u32>, 3>,
    worldLightParams: vec4<f32>,
    advancedFogColorDensity: vec4<f32>,
    advancedFogTypeFarEnabled: vec4<f32>,
    emissionRadiance: vec4<f32>,
    materialBaseColor: vec4<f32>,
    materialFactors: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_3_: bool = (shadow_pcf <= 1i);
override override_type_3_1: bool = (shadow_pcf <= 5i);
override override_type_3_2: bool = (lightmap_slot == 1i);
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
override override_type_3_3: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_3_4: bool = (acff == 1i);
override override_type_3_5: bool = (acff == 2i);
override override_type_3_6: bool = (acff == 3i);
override override_type_3_7: bool = (acff == 1i);
override override_type_3_8: bool = (acff == 2i);
override override_type_3_9: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_10: bool = (discard_mode == 1i);
override override_type_3_11: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
@group(2) @binding(0)
var shadowMap: texture_2d_array<f32>;
@group(2) @binding(32)
var shadowMap_sampler: sampler;
var<private> shadowData_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e84 + 0.5f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e91 = fogType;
    let _e94 = fogType;
    return (((_e89 > 0.5f) && (_e91 >= 1i)) && (_e94 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e84 = wired_advanced_fog_enabled_u0028_();
    if !(_e84) {
        return 0f;
    }
    let _e87 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e87, 0.000001f));
    let _e92 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e92 + 0.5f));
    let _e95 = fogType_1;
    if (_e95 == 1i) {
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e99 <= 0f) {
            return 0f;
        }
        let _e101 = viewDepth;
        let _e104 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e101 / _e104), 0f, 1f);
    }
    let _e109 = unnamed.advancedFogColorDensity[3u];
    let _e111 = viewDepth;
    opticalDepth = (max(_e109, 0f) * _e111);
    let _e113 = fogType_1;
    if (_e113 == 2i) {
        let _e115 = opticalDepth;
        return clamp((1f - exp(-(_e115))), 0f, 1f);
    }
    let _e120 = opticalDepth;
    let _e121 = opticalDepth;
    return clamp((1f - exp(-((_e120 * _e121)))), 0f, 1f);
}

fn sampleCascade_u0028_i1_u003b_vf3_u003b(c: ptr<function, i32>, worldPos: ptr<function, vec3<f32>>) -> f32 {
    var sc4_: vec4<f32>;
    var sc: vec3<f32>;
    var layer: f32;
    var texelSize: vec2<f32>;
    var currentDepth: f32;
    var shadow: f32;
    var x: i32;
    var y: i32;
    var phi_331_: bool;
    var phi_338_: bool;
    var phi_345_: bool;
    var phi_352_: bool;

    let _e91 = (*c);
    let _e94 = unnamed.cascadeMVP[_e91];
    let _e95 = (*worldPos);
    sc4_ = (_e94 * vec4<f32>(_e95.x, _e95.y, _e95.z, 1f));
    let _e101 = sc4_;
    let _e104 = sc4_[3u];
    sc = (_e101.xyz / vec3(_e104));
    let _e107 = sc;
    let _e111 = ((_e107.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e111.x;
    sc[1u] = _e111.y;
    let _e117 = sc[0u];
    let _e118 = (_e117 < 0f);
    phi_331_ = _e118;
    if !(_e118) {
        let _e121 = sc[0u];
        phi_331_ = (_e121 > 1f);
    }
    let _e124 = phi_331_;
    phi_338_ = _e124;
    if !(_e124) {
        let _e127 = sc[1u];
        phi_338_ = (_e127 < 0f);
    }
    let _e130 = phi_338_;
    phi_345_ = _e130;
    if !(_e130) {
        let _e133 = sc[1u];
        phi_345_ = (_e133 > 1f);
    }
    let _e136 = phi_345_;
    phi_352_ = _e136;
    if !(_e136) {
        let _e139 = sc[2u];
        phi_352_ = (_e139 > 1f);
    }
    let _e142 = phi_352_;
    if _e142 {
        return 1f;
    }
    let _e143 = (*c);
    layer = f32(_e143);
    let _e145 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e145).xy));
    let _e152 = sc[2u];
    currentDepth = (_e152 - shadow_bias);
    shadow = 0f;
    if override_type_3_ {
        let _e154 = currentDepth;
        let _e155 = sc;
        let _e156 = _e155.xy;
        let _e157 = layer;
        let _e160 = vec3<f32>(_e156.x, _e156.y, _e157);
        let _e166 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e160.x, _e160.y), i32(_e160.z));
        shadow = step(_e154, _e166.x);
    } else {
        if override_type_3_1 {
            let _e169 = currentDepth;
            let _e170 = sc;
            let _e171 = _e170.xy;
            let _e172 = layer;
            let _e175 = vec3<f32>(_e171.x, _e171.y, _e172);
            let _e181 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e175.x, _e175.y), i32(_e175.z));
            let _e184 = shadow;
            shadow = (_e184 + step(_e169, _e181.x));
            let _e186 = currentDepth;
            let _e187 = sc;
            let _e190 = texelSize[0u];
            let _e192 = (_e187.xy + vec2<f32>(_e190, 0f));
            let _e193 = layer;
            let _e196 = vec3<f32>(_e192.x, _e192.y, _e193);
            let _e202 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e196.x, _e196.y), i32(_e196.z));
            let _e205 = shadow;
            shadow = (_e205 + step(_e186, _e202.x));
            let _e207 = currentDepth;
            let _e208 = sc;
            let _e211 = texelSize[0u];
            let _e213 = (_e208.xy - vec2<f32>(_e211, 0f));
            let _e214 = layer;
            let _e217 = vec3<f32>(_e213.x, _e213.y, _e214);
            let _e223 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e217.x, _e217.y), i32(_e217.z));
            let _e226 = shadow;
            shadow = (_e226 + step(_e207, _e223.x));
            let _e228 = currentDepth;
            let _e229 = sc;
            let _e232 = texelSize[1u];
            let _e234 = (_e229.xy + vec2<f32>(0f, _e232));
            let _e235 = layer;
            let _e238 = vec3<f32>(_e234.x, _e234.y, _e235);
            let _e244 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e238.x, _e238.y), i32(_e238.z));
            let _e247 = shadow;
            shadow = (_e247 + step(_e228, _e244.x));
            let _e249 = currentDepth;
            let _e250 = sc;
            let _e253 = texelSize[1u];
            let _e255 = (_e250.xy - vec2<f32>(0f, _e253));
            let _e256 = layer;
            let _e259 = vec3<f32>(_e255.x, _e255.y, _e256);
            let _e265 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e259.x, _e259.y), i32(_e259.z));
            let _e268 = shadow;
            shadow = (_e268 + step(_e249, _e265.x));
            let _e270 = shadow;
            shadow = (_e270 / 5f);
        } else {
            x = -1i;
            loop {
                let _e272 = x;
                if (_e272 <= 1i) {
                    y = -1i;
                    loop {
                        let _e274 = y;
                        if (_e274 <= 1i) {
                            let _e276 = currentDepth;
                            let _e277 = sc;
                            let _e279 = x;
                            let _e281 = y;
                            let _e284 = texelSize;
                            let _e286 = (_e277.xy + (vec2<f32>(f32(_e279), f32(_e281)) * _e284));
                            let _e287 = layer;
                            let _e290 = vec3<f32>(_e286.x, _e286.y, _e287);
                            let _e296 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e290.x, _e290.y), i32(_e290.z));
                            let _e299 = shadow;
                            shadow = (_e299 + step(_e276, _e296.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e301 = y;
                            y = (_e301 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e303 = x;
                    x = (_e303 + 1i);
                }
            }
            let _e305 = shadow;
            shadow = (_e305 / 9f);
        }
    }
    let _e307 = shadow;
    return _e307;
}

fn sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b(worldPos_1: ptr<function, vec3<f32>>, viewDepth_1: ptr<function, f32>, outCascade: ptr<function, i32>) -> f32 {
    var cmp: vec4<f32>;
    var cascade: i32;
    var prevSplit: f32;
    var local: f32;
    var farSplit: f32;
    var blendRange: f32;
    var blendT: f32;
    var s0_: f32;
    var param: i32;
    var param_1: vec3<f32>;
    var s1_: f32;
    var param_2: i32;
    var param_3: vec3<f32>;

    let _e98 = unnamed.cascadeSplits;
    let _e99 = (*viewDepth_1);
    cmp = step(_e98, vec4(_e99));
    let _e103 = cmp[0u];
    let _e105 = cmp[1u];
    let _e108 = cmp[2u];
    let _e111 = cmp[3u];
    cascade = min(i32((((_e103 + _e105) + _e108) + _e111)), 3i);
    let _e115 = cascade;
    (*outCascade) = _e115;
    let _e116 = cascade;
    if (_e116 == 0i) {
        local = 0f;
    } else {
        let _e118 = cascade;
        let _e123 = unnamed.cascadeSplits[max((_e118 - 1i), 0i)];
        local = _e123;
    }
    let _e124 = local;
    prevSplit = _e124;
    let _e125 = cascade;
    let _e128 = unnamed.cascadeSplits[_e125];
    farSplit = _e128;
    let _e129 = farSplit;
    let _e130 = prevSplit;
    blendRange = max((0.1f * (_e129 - _e130)), 1f);
    let _e134 = farSplit;
    let _e135 = (*viewDepth_1);
    let _e137 = blendRange;
    blendT = clamp(((_e134 - _e135) / _e137), 0f, 1f);
    let _e140 = cascade;
    param = _e140;
    let _e141 = (*worldPos_1);
    param_1 = _e141;
    let _e142 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e142;
    let _e143 = cascade;
    param_2 = min((_e143 + 1i), 3i);
    let _e146 = (*worldPos_1);
    param_3 = _e146;
    let _e147 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e147;
    let _e148 = s1_;
    let _e149 = s0_;
    let _e150 = blendT;
    return mix(_e148, _e149, _e150);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e92 = unnamed.packed_indices[1i][3u];
    let _e98 = unnamed.packed_indices[1i][3u];
    let _e103 = (*lm_uv);
    let _e104 = textureSample(wired_bindless_images[(_e92 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    sun_mask = _e104.x;
    let _e106 = sun_mask;
    if (_e106 > 0.001f) {
        let _e108 = shadowData_1;
        param_4 = _e108.xyz;
        let _e111 = shadowData_1[3u];
        param_5 = _e111;
        let _e112 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e113 = param_6;
        ignoredCascade = _e113;
        shadow_1 = _e112;
        let _e114 = shadow_1;
        let _e115 = sun_mask;
        let _e117 = (*rgb);
        (*rgb) = (_e117 * mix(1f, _e114, _e115));
    }
    let _e119 = (*rgb);
    return _e119;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_3_2 {
        let _e84 = (*rgb_1);
        param_7 = _e84;
        let _e85 = frag_tex_coord0_1;
        param_8 = _e85;
        let _e86 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e86;
    }
    let _e87 = (*rgb_1);
    return _e87;
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb_2: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e84 = (*rgb_2);
    let _e87 = unnamed.worldLightParams[0u];
    boosted = (_e84 * _e87);
    let _e90 = boosted[0u];
    let _e92 = boosted[1u];
    let _e94 = boosted[2u];
    peak = max(_e90, max(_e92, _e94));
    let _e97 = peak;
    if (_e97 > 1f) {
        let _e99 = peak;
        let _e100 = boosted;
        boosted = (_e100 / vec3(_e99));
    }
    let _e103 = boosted;
    return _e103;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e85 = (*c_1);
    (*c_1) = max(_e85, vec3<f32>(0f, 0f, 0f));
    let _e87 = (*c_1);
    cutoff = (_e87 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e89 = (*c_1);
    lo = (_e89 / vec3(12.92f));
    let _e92 = (*c_1);
    hi = pow(((_e92 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e97 = hi;
    let _e98 = lo;
    let _e99 = cutoff;
    return mix(_e97, _e98, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e99));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;
    var param_10: vec3<f32>;

    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e109 = (*uv);
    let _e110 = textureSample(wired_bindless_images[(_e94 & 4095u)], wired_bindless_samplers[((_e104 >> bitcast<u32>(12i)) & 255u)], _e109);
    c_2 = _e110;
    let _e111 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e111))) == 0i) {
        let _e116 = c_2;
        param_9 = _e116.xyz;
        let _e118 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e118.x;
        c_2[1u] = _e118.y;
        c_2[2u] = _e118.z;
    }
    let _e125 = (*slot);
    if (lightmap_slot == (_e125 + 1i)) {
        let _e128 = c_2;
        param_10 = _e128.xyz;
        let _e130 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_10));
        c_2[0u] = _e130.x;
        c_2[1u] = _e130.y;
        c_2[2u] = _e130.z;
    }
    let _e137 = c_2;
    return _e137;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var base: vec4<f32>;
    var param_14: vec3<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;

    let _e97 = unnamed.packed_indices[0i][3u];
    let _e103 = unnamed.packed_indices[0i][3u];
    let _e108 = fog_tex_coord_1;
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    fog = _e109;
    param_11 = 0u;
    let _e110 = frag_tex_coord0_1;
    param_12 = _e110;
    param_13 = 0i;
    let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor0_ = _e111;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e116 = sampledColor0_;
    let _e117 = frag_color;
    color0_ = (_e116 * _e117);
    let _e119 = color0_;
    base = _e119;
    let _e120 = color0_;
    base = _e120;
    let _e121 = base;
    param_14 = _e121.xyz;
    let _e123 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e123.x;
    base[1u] = _e123.y;
    base[2u] = _e123.z;
    if override_type_3_3 {
        let _e132 = unnamed.worldLightParams[1u];
        wetness = clamp(_e132, 0f, 1f);
        let _e136 = unnamed.worldLightParams[2u];
        frost = clamp(_e136, 0f, 1f);
        let _e138 = base;
        luminance = dot(_e138.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e141 = wetness;
        let _e143 = base;
        let _e145 = (_e143.xyz * mix(1f, 0.82f, _e141));
        base[0u] = _e145.x;
        base[1u] = _e145.y;
        base[2u] = _e145.z;
        let _e152 = base;
        let _e154 = luminance;
        let _e156 = luminance;
        let _e158 = luminance;
        let _e160 = frost;
        let _e163 = mix(_e152.xyz, vec3<f32>((_e154 * 0.88f), (_e156 * 0.94f), _e158), vec3((_e160 * 0.55f)));
        base[0u] = _e163.x;
        base[1u] = _e163.y;
        base[2u] = _e163.z;
    }
    let _e172 = unnamed.materialFactors[0u];
    if (_e172 >= 0f) {
        let _e175 = unnamed.materialBaseColor;
        let _e176 = base;
        base = (_e176 * _e175);
    }
    let _e178 = color0_;
    let _e181 = unnamed.emissionRadiance;
    let _e184 = base;
    let _e186 = (_e184.xyz + (_e178.xyz * _e181.xyz));
    base[0u] = _e186.x;
    base[1u] = _e186.y;
    base[2u] = _e186.z;
    let _e193 = wired_advanced_fog_enabled_u0028_();
    if _e193 {
        let _e194 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e194;
        if override_type_3_4 {
            let _e195 = fogAmount;
            let _e197 = base;
            let _e199 = (_e197.xyz * (1f - _e195));
            base[0u] = _e199.x;
            base[1u] = _e199.y;
            base[2u] = _e199.z;
        } else {
            if override_type_3_5 {
                let _e206 = fogAmount;
                let _e208 = base;
                base = (_e208 * (1f - _e206));
            } else {
                if override_type_3_6 {
                    let _e210 = fogAmount;
                    let _e213 = base[3u];
                    base[3u] = (_e213 * (1f - _e210));
                } else {
                    let _e216 = base;
                    let _e219 = unnamed.advancedFogColorDensity;
                    let _e221 = fogAmount;
                    let _e223 = mix(_e216.xyz, _e219.xyz, vec3(_e221));
                    base[0u] = _e223.x;
                    base[1u] = _e223.y;
                    base[2u] = _e223.z;
                }
            }
        }
    } else {
        if override_type_3_7 {
            let _e230 = base;
            let _e233 = fog[3u];
            let _e235 = (_e230.xyz * (1f - _e233));
            base[0u] = _e235.x;
            base[1u] = _e235.y;
            base[2u] = _e235.z;
        } else {
            if override_type_3_8 {
                let _e242 = base;
                let _e244 = fog[3u];
                base = (_e242 * (1f - _e244));
            } else {
                if override_type_3_9 {
                    let _e248 = base[3u];
                    let _e250 = fog[3u];
                    base[3u] = (_e248 * (1f - _e250));
                } else {
                    let _e254 = base;
                    let _e255 = fog;
                    let _e257 = unnamed.fogColor;
                    let _e260 = fog[3u];
                    base = mix(_e254, (_e255 * _e257), vec4(_e260));
                }
            }
        }
    }
    if override_type_3_10 {
        let _e264 = base[3u];
        if (_e264 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e266 = base;
            let _e268 = base;
            if (dot(_e266.xyz, _e268.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e272 = base;
    out_color = _e272;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e9 = out_color;
    return _e9;
}
