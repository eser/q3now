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
@id(10) override acff: i32 = 0i;
override override_type_11_12: bool = (acff == 1i);
override override_type_11_13: bool = (acff == 2i);
override override_type_11_14: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_15: bool = (discard_mode == 1i);
override override_type_11_16: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e85 = (*c);
    let _e88 = unnamed.cascadeMVP[_e85];
    let _e89 = (*worldPos);
    sc4_ = (_e88 * vec4<f32>(_e89.x, _e89.y, _e89.z, 1f));
    let _e95 = sc4_;
    let _e98 = sc4_[3u];
    sc = (_e95.xyz / vec3(_e98));
    let _e101 = sc;
    let _e105 = ((_e101.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e105.x;
    sc[1u] = _e105.y;
    let _e111 = sc[0u];
    let _e112 = (_e111 < 0f);
    phi_218_ = _e112;
    if !(_e112) {
        let _e115 = sc[0u];
        phi_218_ = (_e115 > 1f);
    }
    let _e118 = phi_218_;
    phi_225_ = _e118;
    if !(_e118) {
        let _e121 = sc[1u];
        phi_225_ = (_e121 < 0f);
    }
    let _e124 = phi_225_;
    phi_232_ = _e124;
    if !(_e124) {
        let _e127 = sc[1u];
        phi_232_ = (_e127 > 1f);
    }
    let _e130 = phi_232_;
    phi_239_ = _e130;
    if !(_e130) {
        let _e133 = sc[2u];
        phi_239_ = (_e133 > 1f);
    }
    let _e136 = phi_239_;
    if _e136 {
        return 1f;
    }
    let _e137 = (*c);
    layer = f32(_e137);
    let _e139 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e139).xy));
    let _e146 = sc[2u];
    currentDepth = (_e146 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e148 = currentDepth;
        let _e149 = sc;
        let _e150 = _e149.xy;
        let _e151 = layer;
        let _e154 = vec3<f32>(_e150.x, _e150.y, _e151);
        let _e160 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e154.x, _e154.y), i32(_e154.z));
        shadow = step(_e148, _e160.x);
    } else {
        if override_type_11_1 {
            let _e163 = currentDepth;
            let _e164 = sc;
            let _e165 = _e164.xy;
            let _e166 = layer;
            let _e169 = vec3<f32>(_e165.x, _e165.y, _e166);
            let _e175 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e169.x, _e169.y), i32(_e169.z));
            let _e178 = shadow;
            shadow = (_e178 + step(_e163, _e175.x));
            let _e180 = currentDepth;
            let _e181 = sc;
            let _e184 = texelSize[0u];
            let _e186 = (_e181.xy + vec2<f32>(_e184, 0f));
            let _e187 = layer;
            let _e190 = vec3<f32>(_e186.x, _e186.y, _e187);
            let _e196 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e190.x, _e190.y), i32(_e190.z));
            let _e199 = shadow;
            shadow = (_e199 + step(_e180, _e196.x));
            let _e201 = currentDepth;
            let _e202 = sc;
            let _e205 = texelSize[0u];
            let _e207 = (_e202.xy - vec2<f32>(_e205, 0f));
            let _e208 = layer;
            let _e211 = vec3<f32>(_e207.x, _e207.y, _e208);
            let _e217 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e211.x, _e211.y), i32(_e211.z));
            let _e220 = shadow;
            shadow = (_e220 + step(_e201, _e217.x));
            let _e222 = currentDepth;
            let _e223 = sc;
            let _e226 = texelSize[1u];
            let _e228 = (_e223.xy + vec2<f32>(0f, _e226));
            let _e229 = layer;
            let _e232 = vec3<f32>(_e228.x, _e228.y, _e229);
            let _e238 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e232.x, _e232.y), i32(_e232.z));
            let _e241 = shadow;
            shadow = (_e241 + step(_e222, _e238.x));
            let _e243 = currentDepth;
            let _e244 = sc;
            let _e247 = texelSize[1u];
            let _e249 = (_e244.xy - vec2<f32>(0f, _e247));
            let _e250 = layer;
            let _e253 = vec3<f32>(_e249.x, _e249.y, _e250);
            let _e259 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e253.x, _e253.y), i32(_e253.z));
            let _e262 = shadow;
            shadow = (_e262 + step(_e243, _e259.x));
            let _e264 = shadow;
            shadow = (_e264 / 5f);
        } else {
            x = -1i;
            loop {
                let _e266 = x;
                if (_e266 <= 1i) {
                    y = -1i;
                    loop {
                        let _e268 = y;
                        if (_e268 <= 1i) {
                            let _e270 = currentDepth;
                            let _e271 = sc;
                            let _e273 = x;
                            let _e275 = y;
                            let _e278 = texelSize;
                            let _e280 = (_e271.xy + (vec2<f32>(f32(_e273), f32(_e275)) * _e278));
                            let _e281 = layer;
                            let _e284 = vec3<f32>(_e280.x, _e280.y, _e281);
                            let _e290 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e284.x, _e284.y), i32(_e284.z));
                            let _e293 = shadow;
                            shadow = (_e293 + step(_e270, _e290.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e295 = y;
                            y = (_e295 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e297 = x;
                    x = (_e297 + 1i);
                }
            }
            let _e299 = shadow;
            shadow = (_e299 / 9f);
        }
    }
    let _e301 = shadow;
    return _e301;
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

    let _e92 = unnamed.cascadeSplits;
    let _e93 = (*viewDepth);
    cmp = step(_e92, vec4(_e93));
    let _e97 = cmp[0u];
    let _e99 = cmp[1u];
    let _e102 = cmp[2u];
    let _e105 = cmp[3u];
    cascade = min(i32((((_e97 + _e99) + _e102) + _e105)), 3i);
    let _e109 = cascade;
    (*outCascade) = _e109;
    let _e110 = cascade;
    if (_e110 == 0i) {
        local = 0f;
    } else {
        let _e112 = cascade;
        let _e117 = unnamed.cascadeSplits[max((_e112 - 1i), 0i)];
        local = _e117;
    }
    let _e118 = local;
    prevSplit = _e118;
    let _e119 = cascade;
    let _e122 = unnamed.cascadeSplits[_e119];
    farSplit = _e122;
    let _e123 = farSplit;
    let _e124 = prevSplit;
    blendRange = max((0.1f * (_e123 - _e124)), 1f);
    let _e128 = farSplit;
    let _e129 = (*viewDepth);
    let _e131 = blendRange;
    blendT = clamp(((_e128 - _e129) / _e131), 0f, 1f);
    let _e134 = cascade;
    param = _e134;
    let _e135 = (*worldPos_1);
    param_1 = _e135;
    let _e136 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e136;
    let _e137 = cascade;
    param_2 = min((_e137 + 1i), 3i);
    let _e140 = (*worldPos_1);
    param_3 = _e140;
    let _e141 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e141;
    let _e142 = s1_;
    let _e143 = s0_;
    let _e144 = blendT;
    return mix(_e142, _e143, _e144);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e86 = unnamed.packed_indices[1i][3u];
    let _e92 = unnamed.packed_indices[1i][3u];
    let _e97 = (*lm_uv);
    let _e98 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    sun_mask = _e98.x;
    let _e100 = sun_mask;
    if (_e100 > 0.001f) {
        let _e102 = shadowData_1;
        param_4 = _e102.xyz;
        let _e105 = shadowData_1[3u];
        param_5 = _e105;
        let _e106 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e107 = param_6;
        ignoredCascade = _e107;
        shadow_1 = _e106;
        let _e108 = shadow_1;
        let _e109 = sun_mask;
        let _e111 = (*rgb);
        (*rgb) = (_e111 * mix(1f, _e108, _e109));
    }
    let _e113 = (*rgb);
    return _e113;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_11_2 {
        let _e80 = (*rgb_1);
        param_7 = _e80;
        let _e81 = frag_tex_coord0_1;
        param_8 = _e81;
        let _e82 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e82;
    }
    if override_type_11_3 {
        let _e83 = (*rgb_1);
        param_9 = _e83;
        let _e84 = frag_tex_coord1_1;
        param_10 = _e84;
        let _e85 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e85;
    }
    let _e86 = (*rgb_1);
    return _e86;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e79 = (*c_1);
    (*c_1) = max(_e79, vec3<f32>(0f, 0f, 0f));
    let _e81 = (*c_1);
    cutoff = (_e81 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e83 = (*c_1);
    lo = (_e83 / vec3(12.92f));
    let _e86 = (*c_1);
    hi = pow(((_e86 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e91 = hi;
    let _e92 = lo;
    let _e93 = cutoff;
    return mix(_e91, _e92, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e93));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e102 = (*uv);
    let _e103 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    c_2 = _e103;
    let _e104 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e104))) == 0i) {
        let _e109 = c_2;
        param_11 = _e109.xyz;
        let _e111 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e111.x;
        c_2[1u] = _e111.y;
        c_2[2u] = _e111.z;
    }
    let _e118 = (*slot);
    if (lightmap_slot == (_e118 + 1i)) {
        let _e123 = unnamed.worldLightParams[0u];
        let _e124 = c_2;
        let _e126 = (_e124.xyz * _e123);
        c_2[0u] = _e126.x;
        c_2[1u] = _e126.y;
        c_2[2u] = _e126.z;
    }
    let _e133 = c_2;
    return _e133;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e117 = unnamed.packed_indices[0i][3u];
    let _e123 = unnamed.packed_indices[0i][3u];
    let _e128 = fog_tex_coord_1;
    let _e129 = textureSample(wired_bindless_images[(_e117 & 4095u)], wired_bindless_samplers[((_e123 >> bitcast<u32>(12i)) & 255u)], _e128);
    fog = _e129;
    let _e130 = frag_color0In_1;
    param_12 = _e130.xyz;
    let _e132 = sRGBToLinear_u0028_vf3_u003b((&param_12));
    let _e134 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e132.x, _e132.y, _e132.z, _e134);
    let _e139 = frag_color1In_1;
    param_13 = _e139.xyz;
    let _e141 = sRGBToLinear_u0028_vf3_u003b((&param_13));
    let _e143 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e141.x, _e141.y, _e141.z, _e143);
    param_14 = 0u;
    let _e148 = frag_tex_coord0_1;
    param_15 = _e148;
    param_16 = 0i;
    let _e149 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    let _e150 = frag_color0_;
    color0_ = (_e149 * _e150);
    if override_type_11_6 {
        param_17 = 1u;
        let _e152 = frag_tex_coord1_1;
        param_18 = _e152;
        param_19 = 1i;
        let _e153 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
        let _e154 = frag_color1_;
        color1_ = (_e153 * _e154);
        let _e156 = color0_;
        let _e158 = color1_;
        let _e160 = (_e156.xyz + _e158.xyz);
        let _e162 = color0_[3u];
        let _e164 = color1_[3u];
        base = vec4<f32>(_e160.x, _e160.y, _e160.z, (_e162 * _e164));
    } else {
        if override_type_11_7 {
            param_20 = 1u;
            let _e170 = frag_tex_coord1_1;
            param_21 = _e170;
            param_22 = 1i;
            let _e171 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            let _e172 = frag_color1_;
            color1_1 = (_e171 * _e172);
            let _e175 = color0_[3u];
            let _e176 = color0_;
            color0_ = (_e176 * _e175);
            let _e179 = color1_1[3u];
            let _e180 = color1_1;
            color1_1 = (_e180 * _e179);
            let _e182 = color0_;
            let _e184 = color1_1;
            let _e186 = (_e182.xyz + _e184.xyz);
            let _e188 = color0_[3u];
            let _e190 = color1_1[3u];
            base = vec4<f32>(_e186.x, _e186.y, _e186.z, (_e188 * _e190));
        } else {
            if override_type_11_8 {
                param_23 = 1u;
                let _e196 = frag_tex_coord1_1;
                param_24 = _e196;
                param_25 = 1i;
                let _e197 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
                let _e198 = frag_color1_;
                color1_2 = (_e197 * _e198);
                let _e201 = color0_[3u];
                let _e203 = color0_;
                color0_ = (_e203 * (1f - _e201));
                let _e206 = color1_2[3u];
                let _e208 = color1_2;
                color1_2 = (_e208 * (1f - _e206));
                let _e210 = color0_;
                let _e212 = color1_2;
                let _e214 = (_e210.xyz + _e212.xyz);
                let _e216 = color0_[3u];
                let _e218 = color1_2[3u];
                base = vec4<f32>(_e214.x, _e214.y, _e214.z, (_e216 * _e218));
            } else {
                if override_type_11_9 {
                    param_26 = 1u;
                    let _e224 = frag_tex_coord1_1;
                    param_27 = _e224;
                    param_28 = 1i;
                    let _e225 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
                    let _e226 = frag_color1_;
                    color1_3 = (_e225 * _e226);
                    let _e228 = color0_;
                    let _e229 = color1_3;
                    let _e231 = color1_3[3u];
                    base = mix(_e228, _e229, vec4(_e231));
                } else {
                    if override_type_11_10 {
                        param_29 = 1u;
                        let _e234 = frag_tex_coord1_1;
                        param_30 = _e234;
                        param_31 = 1i;
                        let _e235 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
                        let _e236 = frag_color1_;
                        color1_4 = (_e235 * _e236);
                        let _e238 = color1_4;
                        let _e239 = color0_;
                        let _e241 = color1_4[3u];
                        base = mix(_e238, _e239, vec4(_e241));
                    } else {
                        if override_type_11_11 {
                            param_32 = 1u;
                            let _e244 = frag_tex_coord1_1;
                            param_33 = _e244;
                            param_34 = 1i;
                            let _e245 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                            let _e246 = frag_color1_;
                            color1_5 = (_e245 * _e246);
                            let _e248 = color1_5;
                            let _e250 = color1_5[3u];
                            let _e253 = color0_;
                            base = ((_e248 + vec4(_e250)) * _e253);
                        } else {
                            param_35 = 1u;
                            let _e255 = frag_tex_coord1_1;
                            param_36 = _e255;
                            param_37 = 1i;
                            let _e256 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                            let _e257 = frag_color1_;
                            color1_6 = (_e256 * _e257);
                            let _e259 = color0_;
                            let _e261 = color1_6;
                            let _e263 = (_e259.xyz * _e261.xyz);
                            base[0u] = _e263.x;
                            base[1u] = _e263.y;
                            base[2u] = _e263.z;
                            let _e271 = color0_[3u];
                            let _e273 = color1_6[3u];
                            base[3u] = (_e271 * _e273);
                        }
                    }
                }
            }
        }
    }
    let _e276 = base;
    param_38 = _e276.xyz;
    let _e278 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_38));
    base[0u] = _e278.x;
    base[1u] = _e278.y;
    base[2u] = _e278.z;
    if override_type_11_12 {
        let _e285 = base;
        let _e288 = fog[3u];
        let _e290 = (_e285.xyz * (1f - _e288));
        base[0u] = _e290.x;
        base[1u] = _e290.y;
        base[2u] = _e290.z;
    } else {
        if override_type_11_13 {
            let _e297 = base;
            let _e299 = fog[3u];
            base = (_e297 * (1f - _e299));
        } else {
            if override_type_11_14 {
                let _e303 = base[3u];
                let _e305 = fog[3u];
                base[3u] = (_e303 * (1f - _e305));
            } else {
                let _e309 = base;
                let _e310 = fog;
                let _e312 = unnamed.fogColor;
                let _e315 = fog[3u];
                base = mix(_e309, (_e310 * _e312), vec4(_e315));
            }
        }
    }
    if override_type_11_15 {
        let _e319 = base[3u];
        if (_e319 == 0f) {
            discard;
        }
    } else {
        if override_type_11_16 {
            let _e321 = base;
            let _e323 = base;
            if (dot(_e321.xyz, _e323.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e327 = base;
    out_color = _e327;
    return;
}

@fragment 
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    main_1();
    let _e13 = out_color;
    return _e13;
}
