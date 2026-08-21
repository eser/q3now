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
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(29) override shadow_bias: f32 = 0.005f;
@id(28) override shadow_pcf: i32 = 5i;
override override_type_11_: bool = (shadow_pcf <= 1i);
override override_type_11_1: bool = (shadow_pcf <= 5i);
override override_type_11_2: bool = (lightmap_slot == 1i);
override override_type_11_3: bool = (lightmap_slot == 2i);
@id(6) override tex_mode: i32 = 0i;
override override_type_11_4: bool = (tex_mode == 1i);
override override_type_11_5: bool = (tex_mode == 2i);
override override_type_11_6: bool = (override_type_11_4 || override_type_11_5);
override override_type_11_7: bool = (tex_mode == 3i);
override override_type_11_8: bool = (tex_mode == 4i);
override override_type_11_9: bool = (tex_mode == 5i);
override override_type_11_10: bool = (tex_mode == 6i);
override override_type_11_11: bool = (tex_mode == 7i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_12: bool = (discard_mode == 1i);
override override_type_11_13: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
@group(2) @binding(0) 
var shadowMap: texture_2d_array<f32>;
@group(2) @binding(32) 
var shadowMap_sampler: sampler;
var<private> shadowData_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> out_color: vec4<f32>;

fn sampleCascade_u0028_i1_u003b_vf3_u003b(c: ptr<function, i32>, worldPos: ptr<function, vec3<f32>>) -> f32 {
    var sc4_: vec4<f32>;
    var sc: vec3<f32>;
    var layer: f32;
    var texelSize: vec2<f32>;
    var currentDepth: f32;
    var shadow: f32;
    var x: i32;
    var y: i32;
    var phi_218_: bool;
    var phi_225_: bool;
    var phi_232_: bool;
    var phi_239_: bool;

    let _e80 = (*c);
    let _e83 = unnamed.cascadeMVP[_e80];
    let _e84 = (*worldPos);
    sc4_ = (_e83 * vec4<f32>(_e84.x, _e84.y, _e84.z, 1f));
    let _e90 = sc4_;
    let _e93 = sc4_[3u];
    sc = (_e90.xyz / vec3(_e93));
    let _e96 = sc;
    let _e100 = ((_e96.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e100.x;
    sc[1u] = _e100.y;
    let _e106 = sc[0u];
    let _e107 = (_e106 < 0f);
    phi_218_ = _e107;
    if !(_e107) {
        let _e110 = sc[0u];
        phi_218_ = (_e110 > 1f);
    }
    let _e113 = phi_218_;
    phi_225_ = _e113;
    if !(_e113) {
        let _e116 = sc[1u];
        phi_225_ = (_e116 < 0f);
    }
    let _e119 = phi_225_;
    phi_232_ = _e119;
    if !(_e119) {
        let _e122 = sc[1u];
        phi_232_ = (_e122 > 1f);
    }
    let _e125 = phi_232_;
    phi_239_ = _e125;
    if !(_e125) {
        let _e128 = sc[2u];
        phi_239_ = (_e128 > 1f);
    }
    let _e131 = phi_239_;
    if _e131 {
        return 1f;
    }
    let _e132 = (*c);
    layer = f32(_e132);
    let _e134 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e134).xy));
    let _e141 = sc[2u];
    currentDepth = (_e141 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e143 = currentDepth;
        let _e144 = sc;
        let _e145 = _e144.xy;
        let _e146 = layer;
        let _e149 = vec3<f32>(_e145.x, _e145.y, _e146);
        let _e155 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e149.x, _e149.y), i32(_e149.z));
        shadow = step(_e143, _e155.x);
    } else {
        if override_type_11_1 {
            let _e158 = currentDepth;
            let _e159 = sc;
            let _e160 = _e159.xy;
            let _e161 = layer;
            let _e164 = vec3<f32>(_e160.x, _e160.y, _e161);
            let _e170 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e164.x, _e164.y), i32(_e164.z));
            let _e173 = shadow;
            shadow = (_e173 + step(_e158, _e170.x));
            let _e175 = currentDepth;
            let _e176 = sc;
            let _e179 = texelSize[0u];
            let _e181 = (_e176.xy + vec2<f32>(_e179, 0f));
            let _e182 = layer;
            let _e185 = vec3<f32>(_e181.x, _e181.y, _e182);
            let _e191 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e185.x, _e185.y), i32(_e185.z));
            let _e194 = shadow;
            shadow = (_e194 + step(_e175, _e191.x));
            let _e196 = currentDepth;
            let _e197 = sc;
            let _e200 = texelSize[0u];
            let _e202 = (_e197.xy - vec2<f32>(_e200, 0f));
            let _e203 = layer;
            let _e206 = vec3<f32>(_e202.x, _e202.y, _e203);
            let _e212 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e206.x, _e206.y), i32(_e206.z));
            let _e215 = shadow;
            shadow = (_e215 + step(_e196, _e212.x));
            let _e217 = currentDepth;
            let _e218 = sc;
            let _e221 = texelSize[1u];
            let _e223 = (_e218.xy + vec2<f32>(0f, _e221));
            let _e224 = layer;
            let _e227 = vec3<f32>(_e223.x, _e223.y, _e224);
            let _e233 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e227.x, _e227.y), i32(_e227.z));
            let _e236 = shadow;
            shadow = (_e236 + step(_e217, _e233.x));
            let _e238 = currentDepth;
            let _e239 = sc;
            let _e242 = texelSize[1u];
            let _e244 = (_e239.xy - vec2<f32>(0f, _e242));
            let _e245 = layer;
            let _e248 = vec3<f32>(_e244.x, _e244.y, _e245);
            let _e254 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e248.x, _e248.y), i32(_e248.z));
            let _e257 = shadow;
            shadow = (_e257 + step(_e238, _e254.x));
            let _e259 = shadow;
            shadow = (_e259 / 5f);
        } else {
            x = -1i;
            loop {
                let _e261 = x;
                if (_e261 <= 1i) {
                    y = -1i;
                    loop {
                        let _e263 = y;
                        if (_e263 <= 1i) {
                            let _e265 = currentDepth;
                            let _e266 = sc;
                            let _e268 = x;
                            let _e270 = y;
                            let _e273 = texelSize;
                            let _e275 = (_e266.xy + (vec2<f32>(f32(_e268), f32(_e270)) * _e273));
                            let _e276 = layer;
                            let _e279 = vec3<f32>(_e275.x, _e275.y, _e276);
                            let _e285 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e279.x, _e279.y), i32(_e279.z));
                            let _e288 = shadow;
                            shadow = (_e288 + step(_e265, _e285.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e290 = y;
                            y = (_e290 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e292 = x;
                    x = (_e292 + 1i);
                }
            }
            let _e294 = shadow;
            shadow = (_e294 / 9f);
        }
    }
    let _e296 = shadow;
    return _e296;
}

fn sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b(worldPos_1: ptr<function, vec3<f32>>, viewDepth: ptr<function, f32>, outCascade: ptr<function, i32>) -> f32 {
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

    let _e87 = unnamed.cascadeSplits;
    let _e88 = (*viewDepth);
    cmp = step(_e87, vec4(_e88));
    let _e92 = cmp[0u];
    let _e94 = cmp[1u];
    let _e97 = cmp[2u];
    let _e100 = cmp[3u];
    cascade = min(i32((((_e92 + _e94) + _e97) + _e100)), 3i);
    let _e104 = cascade;
    (*outCascade) = _e104;
    let _e105 = cascade;
    if (_e105 == 0i) {
        local = 0f;
    } else {
        let _e107 = cascade;
        let _e112 = unnamed.cascadeSplits[max((_e107 - 1i), 0i)];
        local = _e112;
    }
    let _e113 = local;
    prevSplit = _e113;
    let _e114 = cascade;
    let _e117 = unnamed.cascadeSplits[_e114];
    farSplit = _e117;
    let _e118 = farSplit;
    let _e119 = prevSplit;
    blendRange = max((0.1f * (_e118 - _e119)), 1f);
    let _e123 = farSplit;
    let _e124 = (*viewDepth);
    let _e126 = blendRange;
    blendT = clamp(((_e123 - _e124) / _e126), 0f, 1f);
    let _e129 = cascade;
    param = _e129;
    let _e130 = (*worldPos_1);
    param_1 = _e130;
    let _e131 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e131;
    let _e132 = cascade;
    param_2 = min((_e132 + 1i), 3i);
    let _e135 = (*worldPos_1);
    param_3 = _e135;
    let _e136 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e136;
    let _e137 = s1_;
    let _e138 = s0_;
    let _e139 = blendT;
    return mix(_e137, _e138, _e139);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e81 = unnamed.packed_indices[1i][3u];
    let _e87 = unnamed.packed_indices[1i][3u];
    let _e92 = (*lm_uv);
    let _e93 = textureSample(wired_bindless_images[(_e81 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    sun_mask = _e93.x;
    let _e95 = sun_mask;
    if (_e95 > 0.001f) {
        let _e97 = shadowData_1;
        param_4 = _e97.xyz;
        let _e100 = shadowData_1[3u];
        param_5 = _e100;
        let _e101 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e102 = param_6;
        ignoredCascade = _e102;
        shadow_1 = _e101;
        let _e103 = shadow_1;
        let _e104 = sun_mask;
        let _e106 = (*rgb);
        (*rgb) = (_e106 * mix(1f, _e103, _e104));
    }
    let _e108 = (*rgb);
    return _e108;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_11_2 {
        let _e75 = (*rgb_1);
        param_7 = _e75;
        let _e76 = frag_tex_coord0_1;
        param_8 = _e76;
        let _e77 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e77;
    }
    if override_type_11_3 {
        let _e78 = (*rgb_1);
        param_9 = _e78;
        let _e79 = frag_tex_coord1_1;
        param_10 = _e79;
        let _e80 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e80;
    }
    let _e81 = (*rgb_1);
    return _e81;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c_1);
    (*c_1) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c_1);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c_1);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c_1);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_2 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_2;
        param_11 = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e106.x;
        c_2[1u] = _e106.y;
        c_2[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e118 = unnamed.worldLightParams[0u];
        let _e119 = c_2;
        let _e121 = (_e119.xyz * _e118);
        c_2[0u] = _e121.x;
        c_2[1u] = _e121.y;
        c_2[2u] = _e121.z;
    }
    let _e128 = c_2;
    return _e128;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_12: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_13: vec3<f32>;
    var color0_: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color1_2: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var color1_3: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color1_4: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
    var color1_5: vec4<f32>;
    var param_32: u32;
    var param_33: vec2<f32>;
    var param_34: i32;
    var color1_6: vec4<f32>;
    var param_35: u32;
    var param_36: vec2<f32>;
    var param_37: i32;
    var param_38: vec3<f32>;

    let _e108 = frag_color0In_1;
    param_12 = _e108.xyz;
    let _e110 = sRGBToLinear_u0028_vf3_u003b((&param_12));
    let _e112 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e110.x, _e110.y, _e110.z, _e112);
    let _e117 = frag_color1In_1;
    param_13 = _e117.xyz;
    let _e119 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e121 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e119.x, _e119.y, _e119.z, _e121);
    param_14 = 0u;
    let _e126 = frag_tex_coord0_1;
    param_15 = _e126;
    param_16 = 0i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    let _e128 = frag_color0_;
    color0_ = (_e127 * _e128);
    if override_type_11_6 {
        param_17 = 1u;
        let _e130 = frag_tex_coord1_1;
        param_18 = _e130;
        param_19 = 1i;
        let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
        let _e132 = frag_color1_;
        color1_ = (_e131 * _e132);
        let _e134 = color0_;
        let _e136 = color1_;
        let _e138 = (_e134.xyz + _e136.xyz);
        let _e140 = color0_[3u];
        let _e142 = color1_[3u];
        base = vec4<f32>(_e138.x, _e138.y, _e138.z, (_e140 * _e142));
    } else {
        if override_type_11_7 {
            param_20 = 1u;
            let _e148 = frag_tex_coord1_1;
            param_21 = _e148;
            param_22 = 1i;
            let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            let _e150 = frag_color1_;
            color1_1 = (_e149 * _e150);
            let _e153 = color0_[3u];
            let _e154 = color0_;
            color0_ = (_e154 * _e153);
            let _e157 = color1_1[3u];
            let _e158 = color1_1;
            color1_1 = (_e158 * _e157);
            let _e160 = color0_;
            let _e162 = color1_1;
            let _e164 = (_e160.xyz + _e162.xyz);
            let _e166 = color0_[3u];
            let _e168 = color1_1[3u];
            base = vec4<f32>(_e164.x, _e164.y, _e164.z, (_e166 * _e168));
        } else {
            if override_type_11_8 {
                param_23 = 1u;
                let _e174 = frag_tex_coord1_1;
                param_24 = _e174;
                param_25 = 1i;
                let _e175 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                let _e176 = frag_color1_;
                color1_2 = (_e175 * _e176);
                let _e179 = color0_[3u];
                let _e181 = color0_;
                color0_ = (_e181 * (1f - _e179));
                let _e184 = color1_2[3u];
                let _e186 = color1_2;
                color1_2 = (_e186 * (1f - _e184));
                let _e188 = color0_;
                let _e190 = color1_2;
                let _e192 = (_e188.xyz + _e190.xyz);
                let _e194 = color0_[3u];
                let _e196 = color1_2[3u];
                base = vec4<f32>(_e192.x, _e192.y, _e192.z, (_e194 * _e196));
            } else {
                if override_type_11_9 {
                    param_26 = 1u;
                    let _e202 = frag_tex_coord1_1;
                    param_27 = _e202;
                    param_28 = 1i;
                    let _e203 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                    let _e204 = frag_color1_;
                    color1_3 = (_e203 * _e204);
                    let _e206 = color0_;
                    let _e207 = color1_3;
                    let _e209 = color1_3[3u];
                    base = mix(_e206, _e207, vec4(_e209));
                } else {
                    if override_type_11_10 {
                        param_29 = 1u;
                        let _e212 = frag_tex_coord1_1;
                        param_30 = _e212;
                        param_31 = 1i;
                        let _e213 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                        let _e214 = frag_color1_;
                        color1_4 = (_e213 * _e214);
                        let _e216 = color1_4;
                        let _e217 = color0_;
                        let _e219 = color1_4[3u];
                        base = mix(_e216, _e217, vec4(_e219));
                    } else {
                        if override_type_11_11 {
                            param_32 = 1u;
                            let _e222 = frag_tex_coord1_1;
                            param_33 = _e222;
                            param_34 = 1i;
                            let _e223 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                            let _e224 = frag_color1_;
                            color1_5 = (_e223 * _e224);
                            let _e226 = color1_5;
                            let _e228 = color1_5[3u];
                            let _e231 = color0_;
                            base = ((_e226 + vec4(_e228)) * _e231);
                        } else {
                            param_35 = 1u;
                            let _e233 = frag_tex_coord1_1;
                            param_36 = _e233;
                            param_37 = 1i;
                            let _e234 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                            let _e235 = frag_color1_;
                            color1_6 = (_e234 * _e235);
                            let _e237 = color0_;
                            let _e239 = color1_6;
                            let _e241 = (_e237.xyz * _e239.xyz);
                            base[0u] = _e241.x;
                            base[1u] = _e241.y;
                            base[2u] = _e241.z;
                            let _e249 = color0_[3u];
                            let _e251 = color1_6[3u];
                            base[3u] = (_e249 * _e251);
                        }
                    }
                }
            }
        }
    }
    let _e254 = base;
    param_38 = _e254.xyz;
    let _e256 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_38));
    base[0u] = _e256.x;
    base[1u] = _e256.y;
    base[2u] = _e256.z;
    if override_type_11_12 {
        let _e264 = base[3u];
        if (_e264 == 0f) {
            discard;
        }
    } else {
        if override_type_11_13 {
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
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    main_1();
    let _e11 = out_color;
    return _e11;
}
