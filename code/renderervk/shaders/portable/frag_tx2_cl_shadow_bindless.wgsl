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
override override_type_11_4: bool = (lightmap_slot == 3i);
@id(6) override tex_mode: i32 = 0i;
override override_type_11_5: bool = (tex_mode == 1i);
override override_type_11_6: bool = (tex_mode == 2i);
override override_type_11_7: bool = (override_type_11_5 || override_type_11_6);
override override_type_11_8: bool = (tex_mode == 3i);
override override_type_11_9: bool = (tex_mode == 4i);
override override_type_11_10: bool = (tex_mode == 5i);
override override_type_11_11: bool = (tex_mode == 6i);
override override_type_11_12: bool = (tex_mode == 7i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_13: bool = (discard_mode == 1i);
override override_type_11_14: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
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

    let _e83 = (*c);
    let _e86 = unnamed.cascadeMVP[_e83];
    let _e87 = (*worldPos);
    sc4_ = (_e86 * vec4<f32>(_e87.x, _e87.y, _e87.z, 1f));
    let _e93 = sc4_;
    let _e96 = sc4_[3u];
    sc = (_e93.xyz / vec3(_e96));
    let _e99 = sc;
    let _e103 = ((_e99.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e103.x;
    sc[1u] = _e103.y;
    let _e109 = sc[0u];
    let _e110 = (_e109 < 0f);
    phi_218_ = _e110;
    if !(_e110) {
        let _e113 = sc[0u];
        phi_218_ = (_e113 > 1f);
    }
    let _e116 = phi_218_;
    phi_225_ = _e116;
    if !(_e116) {
        let _e119 = sc[1u];
        phi_225_ = (_e119 < 0f);
    }
    let _e122 = phi_225_;
    phi_232_ = _e122;
    if !(_e122) {
        let _e125 = sc[1u];
        phi_232_ = (_e125 > 1f);
    }
    let _e128 = phi_232_;
    phi_239_ = _e128;
    if !(_e128) {
        let _e131 = sc[2u];
        phi_239_ = (_e131 > 1f);
    }
    let _e134 = phi_239_;
    if _e134 {
        return 1f;
    }
    let _e135 = (*c);
    layer = f32(_e135);
    let _e137 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e137).xy));
    let _e144 = sc[2u];
    currentDepth = (_e144 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e146 = currentDepth;
        let _e147 = sc;
        let _e148 = _e147.xy;
        let _e149 = layer;
        let _e152 = vec3<f32>(_e148.x, _e148.y, _e149);
        let _e158 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e152.x, _e152.y), i32(_e152.z));
        shadow = step(_e146, _e158.x);
    } else {
        if override_type_11_1 {
            let _e161 = currentDepth;
            let _e162 = sc;
            let _e163 = _e162.xy;
            let _e164 = layer;
            let _e167 = vec3<f32>(_e163.x, _e163.y, _e164);
            let _e173 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e167.x, _e167.y), i32(_e167.z));
            let _e176 = shadow;
            shadow = (_e176 + step(_e161, _e173.x));
            let _e178 = currentDepth;
            let _e179 = sc;
            let _e182 = texelSize[0u];
            let _e184 = (_e179.xy + vec2<f32>(_e182, 0f));
            let _e185 = layer;
            let _e188 = vec3<f32>(_e184.x, _e184.y, _e185);
            let _e194 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e188.x, _e188.y), i32(_e188.z));
            let _e197 = shadow;
            shadow = (_e197 + step(_e178, _e194.x));
            let _e199 = currentDepth;
            let _e200 = sc;
            let _e203 = texelSize[0u];
            let _e205 = (_e200.xy - vec2<f32>(_e203, 0f));
            let _e206 = layer;
            let _e209 = vec3<f32>(_e205.x, _e205.y, _e206);
            let _e215 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e209.x, _e209.y), i32(_e209.z));
            let _e218 = shadow;
            shadow = (_e218 + step(_e199, _e215.x));
            let _e220 = currentDepth;
            let _e221 = sc;
            let _e224 = texelSize[1u];
            let _e226 = (_e221.xy + vec2<f32>(0f, _e224));
            let _e227 = layer;
            let _e230 = vec3<f32>(_e226.x, _e226.y, _e227);
            let _e236 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e230.x, _e230.y), i32(_e230.z));
            let _e239 = shadow;
            shadow = (_e239 + step(_e220, _e236.x));
            let _e241 = currentDepth;
            let _e242 = sc;
            let _e245 = texelSize[1u];
            let _e247 = (_e242.xy - vec2<f32>(0f, _e245));
            let _e248 = layer;
            let _e251 = vec3<f32>(_e247.x, _e247.y, _e248);
            let _e257 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e251.x, _e251.y), i32(_e251.z));
            let _e260 = shadow;
            shadow = (_e260 + step(_e241, _e257.x));
            let _e262 = shadow;
            shadow = (_e262 / 5f);
        } else {
            x = -1i;
            loop {
                let _e264 = x;
                if (_e264 <= 1i) {
                    y = -1i;
                    loop {
                        let _e266 = y;
                        if (_e266 <= 1i) {
                            let _e268 = currentDepth;
                            let _e269 = sc;
                            let _e271 = x;
                            let _e273 = y;
                            let _e276 = texelSize;
                            let _e278 = (_e269.xy + (vec2<f32>(f32(_e271), f32(_e273)) * _e276));
                            let _e279 = layer;
                            let _e282 = vec3<f32>(_e278.x, _e278.y, _e279);
                            let _e288 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e282.x, _e282.y), i32(_e282.z));
                            let _e291 = shadow;
                            shadow = (_e291 + step(_e268, _e288.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e293 = y;
                            y = (_e293 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e295 = x;
                    x = (_e295 + 1i);
                }
            }
            let _e297 = shadow;
            shadow = (_e297 / 9f);
        }
    }
    let _e299 = shadow;
    return _e299;
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

    let _e90 = unnamed.cascadeSplits;
    let _e91 = (*viewDepth);
    cmp = step(_e90, vec4(_e91));
    let _e95 = cmp[0u];
    let _e97 = cmp[1u];
    let _e100 = cmp[2u];
    let _e103 = cmp[3u];
    cascade = min(i32((((_e95 + _e97) + _e100) + _e103)), 3i);
    let _e107 = cascade;
    (*outCascade) = _e107;
    let _e108 = cascade;
    if (_e108 == 0i) {
        local = 0f;
    } else {
        let _e110 = cascade;
        let _e115 = unnamed.cascadeSplits[max((_e110 - 1i), 0i)];
        local = _e115;
    }
    let _e116 = local;
    prevSplit = _e116;
    let _e117 = cascade;
    let _e120 = unnamed.cascadeSplits[_e117];
    farSplit = _e120;
    let _e121 = farSplit;
    let _e122 = prevSplit;
    blendRange = max((0.1f * (_e121 - _e122)), 1f);
    let _e126 = farSplit;
    let _e127 = (*viewDepth);
    let _e129 = blendRange;
    blendT = clamp(((_e126 - _e127) / _e129), 0f, 1f);
    let _e132 = cascade;
    param = _e132;
    let _e133 = (*worldPos_1);
    param_1 = _e133;
    let _e134 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e134;
    let _e135 = cascade;
    param_2 = min((_e135 + 1i), 3i);
    let _e138 = (*worldPos_1);
    param_3 = _e138;
    let _e139 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e139;
    let _e140 = s1_;
    let _e141 = s0_;
    let _e142 = blendT;
    return mix(_e140, _e141, _e142);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e84 = unnamed.packed_indices[1i][3u];
    let _e90 = unnamed.packed_indices[1i][3u];
    let _e95 = (*lm_uv);
    let _e96 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    sun_mask = _e96.x;
    let _e98 = sun_mask;
    if (_e98 > 0.001f) {
        let _e100 = shadowData_1;
        param_4 = _e100.xyz;
        let _e103 = shadowData_1[3u];
        param_5 = _e103;
        let _e104 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e105 = param_6;
        ignoredCascade = _e105;
        shadow_1 = _e104;
        let _e106 = shadow_1;
        let _e107 = sun_mask;
        let _e109 = (*rgb);
        (*rgb) = (_e109 * mix(1f, _e106, _e107));
    }
    let _e111 = (*rgb);
    return _e111;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

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
    if override_type_11_4 {
        let _e86 = (*rgb_1);
        param_11 = _e86;
        let _e87 = frag_tex_coord2_1;
        param_12 = _e87;
        let _e88 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e88;
    }
    let _e89 = (*rgb_1);
    return _e89;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e77 = (*c_1);
    (*c_1) = max(_e77, vec3<f32>(0f, 0f, 0f));
    let _e79 = (*c_1);
    cutoff = (_e79 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e81 = (*c_1);
    lo = (_e81 / vec3(12.92f));
    let _e84 = (*c_1);
    hi = pow(((_e84 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e89 = hi;
    let _e90 = lo;
    let _e91 = cutoff;
    return mix(_e89, _e90, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e91));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;

    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e100 = (*uv);
    let _e101 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    c_2 = _e101;
    let _e102 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e102))) == 0i) {
        let _e107 = c_2;
        param_13 = _e107.xyz;
        let _e109 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e109.x;
        c_2[1u] = _e109.y;
        c_2[2u] = _e109.z;
    }
    let _e116 = (*slot);
    if (lightmap_slot == (_e116 + 1i)) {
        let _e121 = unnamed.worldLightParams[0u];
        let _e122 = c_2;
        let _e124 = (_e122.xyz * _e121);
        c_2[0u] = _e124.x;
        c_2[1u] = _e124.y;
        c_2[2u] = _e124.z;
    }
    let _e131 = c_2;
    return _e131;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_14: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_15: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_16: vec3<f32>;
    var color0_: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color1_: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var color2_: vec4<f32>;
    var param_23: u32;
    var param_24: vec2<f32>;
    var param_25: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_26: u32;
    var param_27: vec2<f32>;
    var param_28: i32;
    var color2_1: vec4<f32>;
    var param_29: u32;
    var param_30: vec2<f32>;
    var param_31: i32;
    var color1_2: vec4<f32>;
    var param_32: u32;
    var param_33: vec2<f32>;
    var param_34: i32;
    var color2_2: vec4<f32>;
    var param_35: u32;
    var param_36: vec2<f32>;
    var param_37: i32;
    var color1_3: vec4<f32>;
    var param_38: u32;
    var param_39: vec2<f32>;
    var param_40: i32;
    var color2_3: vec4<f32>;
    var param_41: u32;
    var param_42: vec2<f32>;
    var param_43: i32;
    var color1_4: vec4<f32>;
    var param_44: u32;
    var param_45: vec2<f32>;
    var param_46: i32;
    var color2_4: vec4<f32>;
    var param_47: u32;
    var param_48: vec2<f32>;
    var param_49: i32;
    var color1_5: vec4<f32>;
    var param_50: u32;
    var param_51: vec2<f32>;
    var param_52: i32;
    var color2_5: vec4<f32>;
    var param_53: u32;
    var param_54: vec2<f32>;
    var param_55: i32;
    var color1_6: vec4<f32>;
    var param_56: u32;
    var param_57: vec2<f32>;
    var param_58: i32;
    var color2_6: vec4<f32>;
    var param_59: u32;
    var param_60: vec2<f32>;
    var param_61: i32;
    var param_62: vec3<f32>;

    let _e141 = frag_color0In_1;
    param_14 = _e141.xyz;
    let _e143 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e145 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e143.x, _e143.y, _e143.z, _e145);
    let _e150 = frag_color1In_1;
    param_15 = _e150.xyz;
    let _e152 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e154 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e152.x, _e152.y, _e152.z, _e154);
    let _e159 = frag_color2In_1;
    param_16 = _e159.xyz;
    let _e161 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e163 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e161.x, _e161.y, _e161.z, _e163);
    param_17 = 0u;
    let _e168 = frag_tex_coord0_1;
    param_18 = _e168;
    param_19 = 0i;
    let _e169 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    let _e170 = frag_color0_;
    color0_ = (_e169 * _e170);
    if override_type_11_7 {
        param_20 = 1u;
        let _e172 = frag_tex_coord1_1;
        param_21 = _e172;
        param_22 = 1i;
        let _e173 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
        let _e174 = frag_color1_;
        color1_ = (_e173 * _e174);
        param_23 = 2u;
        let _e176 = frag_tex_coord2_1;
        param_24 = _e176;
        param_25 = 2i;
        let _e177 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
        let _e178 = frag_color2_;
        color2_ = (_e177 * _e178);
        let _e180 = color0_;
        let _e182 = color1_;
        let _e185 = color2_;
        let _e187 = ((_e180.xyz + _e182.xyz) + _e185.xyz);
        let _e189 = color0_[3u];
        let _e191 = color1_[3u];
        let _e194 = color2_[3u];
        base = vec4<f32>(_e187.x, _e187.y, _e187.z, ((_e189 * _e191) * _e194));
    } else {
        if override_type_11_8 {
            param_26 = 1u;
            let _e200 = frag_tex_coord1_1;
            param_27 = _e200;
            param_28 = 1i;
            let _e201 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
            let _e202 = frag_color1_;
            color1_1 = (_e201 * _e202);
            param_29 = 2u;
            let _e204 = frag_tex_coord2_1;
            param_30 = _e204;
            param_31 = 2i;
            let _e205 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
            let _e206 = frag_color2_;
            color2_1 = (_e205 * _e206);
            let _e209 = color0_[3u];
            let _e210 = color0_;
            color0_ = (_e210 * _e209);
            let _e213 = color1_1[3u];
            let _e214 = color1_1;
            color1_1 = (_e214 * _e213);
            let _e217 = color2_1[3u];
            let _e218 = color2_1;
            color2_1 = (_e218 * _e217);
            let _e220 = color0_;
            let _e222 = color1_1;
            let _e225 = color2_1;
            let _e227 = ((_e220.xyz + _e222.xyz) + _e225.xyz);
            let _e229 = color0_[3u];
            let _e231 = color1_1[3u];
            let _e234 = color2_1[3u];
            base = vec4<f32>(_e227.x, _e227.y, _e227.z, ((_e229 * _e231) * _e234));
        } else {
            if override_type_11_9 {
                param_32 = 1u;
                let _e240 = frag_tex_coord1_1;
                param_33 = _e240;
                param_34 = 1i;
                let _e241 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                let _e242 = frag_color1_;
                color1_2 = (_e241 * _e242);
                param_35 = 2u;
                let _e244 = frag_tex_coord2_1;
                param_36 = _e244;
                param_37 = 2i;
                let _e245 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                let _e246 = frag_color2_;
                color2_2 = (_e245 * _e246);
                let _e249 = color0_[3u];
                let _e251 = color0_;
                color0_ = (_e251 * (1f - _e249));
                let _e254 = color1_2[3u];
                let _e256 = color1_2;
                color1_2 = (_e256 * (1f - _e254));
                let _e259 = color2_2[3u];
                let _e261 = color2_2;
                color2_2 = (_e261 * (1f - _e259));
                let _e263 = color0_;
                let _e265 = color1_2;
                let _e268 = color2_2;
                let _e270 = ((_e263.xyz + _e265.xyz) + _e268.xyz);
                let _e272 = color0_[3u];
                let _e274 = color1_2[3u];
                let _e277 = color2_2[3u];
                base = vec4<f32>(_e270.x, _e270.y, _e270.z, ((_e272 * _e274) * _e277));
            } else {
                if override_type_11_10 {
                    param_38 = 1u;
                    let _e283 = frag_tex_coord1_1;
                    param_39 = _e283;
                    param_40 = 1i;
                    let _e284 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_38), (&param_39), (&param_40));
                    let _e285 = frag_color1_;
                    color1_3 = (_e284 * _e285);
                    param_41 = 2u;
                    let _e287 = frag_tex_coord2_1;
                    param_42 = _e287;
                    param_43 = 2i;
                    let _e288 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_41), (&param_42), (&param_43));
                    let _e289 = frag_color2_;
                    color2_3 = (_e288 * _e289);
                    let _e291 = color0_;
                    let _e292 = color1_3;
                    let _e294 = color1_3[3u];
                    let _e297 = color2_3;
                    let _e299 = color2_3[3u];
                    base = mix(mix(_e291, _e292, vec4(_e294)), _e297, vec4(_e299));
                } else {
                    if override_type_11_11 {
                        param_44 = 1u;
                        let _e302 = frag_tex_coord1_1;
                        param_45 = _e302;
                        param_46 = 1i;
                        let _e303 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_44), (&param_45), (&param_46));
                        let _e304 = frag_color1_;
                        color1_4 = (_e303 * _e304);
                        param_47 = 2u;
                        let _e306 = frag_tex_coord2_1;
                        param_48 = _e306;
                        param_49 = 2i;
                        let _e307 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_47), (&param_48), (&param_49));
                        let _e308 = frag_color2_;
                        color2_4 = (_e307 * _e308);
                        let _e310 = color2_4;
                        let _e311 = color1_4;
                        let _e312 = color0_;
                        let _e314 = color1_4[3u];
                        let _e318 = color2_4[3u];
                        base = mix(_e310, mix(_e311, _e312, vec4(_e314)), vec4(_e318));
                    } else {
                        if override_type_11_12 {
                            param_50 = 1u;
                            let _e321 = frag_tex_coord1_1;
                            param_51 = _e321;
                            param_52 = 1i;
                            let _e322 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_50), (&param_51), (&param_52));
                            let _e323 = frag_color1_;
                            color1_5 = (_e322 * _e323);
                            param_53 = 2u;
                            let _e325 = frag_tex_coord2_1;
                            param_54 = _e325;
                            param_55 = 2i;
                            let _e326 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_53), (&param_54), (&param_55));
                            let _e327 = frag_color2_;
                            color2_5 = (_e326 * _e327);
                            let _e329 = color2_5;
                            let _e331 = color2_5[3u];
                            let _e334 = color1_5;
                            let _e336 = color1_5[3u];
                            let _e340 = color0_;
                            base = (((_e329 + vec4(_e331)) * (_e334 + vec4(_e336))) * _e340);
                        } else {
                            param_56 = 1u;
                            let _e342 = frag_tex_coord1_1;
                            param_57 = _e342;
                            param_58 = 1i;
                            let _e343 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_56), (&param_57), (&param_58));
                            let _e344 = frag_color1_;
                            color1_6 = (_e343 * _e344);
                            param_59 = 2u;
                            let _e346 = frag_tex_coord2_1;
                            param_60 = _e346;
                            param_61 = 2i;
                            let _e347 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_59), (&param_60), (&param_61));
                            let _e348 = frag_color2_;
                            color2_6 = (_e347 * _e348);
                            let _e350 = color0_;
                            let _e352 = color1_6;
                            let _e355 = color2_6;
                            let _e357 = ((_e350.xyz * _e352.xyz) * _e355.xyz);
                            base[0u] = _e357.x;
                            base[1u] = _e357.y;
                            base[2u] = _e357.z;
                            let _e365 = color0_[3u];
                            let _e367 = color1_6[3u];
                            let _e370 = color2_6[3u];
                            base[3u] = ((_e365 * _e367) * _e370);
                        }
                    }
                }
            }
        }
    }
    let _e373 = base;
    param_62 = _e373.xyz;
    let _e375 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_62));
    base[0u] = _e375.x;
    base[1u] = _e375.y;
    base[2u] = _e375.z;
    if override_type_11_13 {
        let _e383 = base[3u];
        if (_e383 == 0f) {
            discard;
        }
    } else {
        if override_type_11_14 {
            let _e385 = base;
            let _e387 = base;
            if (dot(_e385.xyz, _e387.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e391 = base;
    out_color = _e391;
    return;
}

@fragment 
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    main_1();
    let _e15 = out_color;
    return _e15;
}
