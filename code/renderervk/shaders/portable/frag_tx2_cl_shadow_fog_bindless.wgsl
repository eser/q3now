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
@id(10) override acff: i32 = 0i;
override override_type_11_13: bool = (acff == 1i);
override override_type_11_14: bool = (acff == 2i);
override override_type_11_15: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_16: bool = (discard_mode == 1i);
override override_type_11_17: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e88 = (*c);
    let _e91 = unnamed.cascadeMVP[_e88];
    let _e92 = (*worldPos);
    sc4_ = (_e91 * vec4<f32>(_e92.x, _e92.y, _e92.z, 1f));
    let _e98 = sc4_;
    let _e101 = sc4_[3u];
    sc = (_e98.xyz / vec3(_e101));
    let _e104 = sc;
    let _e108 = ((_e104.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e108.x;
    sc[1u] = _e108.y;
    let _e114 = sc[0u];
    let _e115 = (_e114 < 0f);
    phi_218_ = _e115;
    if !(_e115) {
        let _e118 = sc[0u];
        phi_218_ = (_e118 > 1f);
    }
    let _e121 = phi_218_;
    phi_225_ = _e121;
    if !(_e121) {
        let _e124 = sc[1u];
        phi_225_ = (_e124 < 0f);
    }
    let _e127 = phi_225_;
    phi_232_ = _e127;
    if !(_e127) {
        let _e130 = sc[1u];
        phi_232_ = (_e130 > 1f);
    }
    let _e133 = phi_232_;
    phi_239_ = _e133;
    if !(_e133) {
        let _e136 = sc[2u];
        phi_239_ = (_e136 > 1f);
    }
    let _e139 = phi_239_;
    if _e139 {
        return 1f;
    }
    let _e140 = (*c);
    layer = f32(_e140);
    let _e142 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e142).xy));
    let _e149 = sc[2u];
    currentDepth = (_e149 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e151 = currentDepth;
        let _e152 = sc;
        let _e153 = _e152.xy;
        let _e154 = layer;
        let _e157 = vec3<f32>(_e153.x, _e153.y, _e154);
        let _e163 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e157.x, _e157.y), i32(_e157.z));
        shadow = step(_e151, _e163.x);
    } else {
        if override_type_11_1 {
            let _e166 = currentDepth;
            let _e167 = sc;
            let _e168 = _e167.xy;
            let _e169 = layer;
            let _e172 = vec3<f32>(_e168.x, _e168.y, _e169);
            let _e178 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e172.x, _e172.y), i32(_e172.z));
            let _e181 = shadow;
            shadow = (_e181 + step(_e166, _e178.x));
            let _e183 = currentDepth;
            let _e184 = sc;
            let _e187 = texelSize[0u];
            let _e189 = (_e184.xy + vec2<f32>(_e187, 0f));
            let _e190 = layer;
            let _e193 = vec3<f32>(_e189.x, _e189.y, _e190);
            let _e199 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e193.x, _e193.y), i32(_e193.z));
            let _e202 = shadow;
            shadow = (_e202 + step(_e183, _e199.x));
            let _e204 = currentDepth;
            let _e205 = sc;
            let _e208 = texelSize[0u];
            let _e210 = (_e205.xy - vec2<f32>(_e208, 0f));
            let _e211 = layer;
            let _e214 = vec3<f32>(_e210.x, _e210.y, _e211);
            let _e220 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e214.x, _e214.y), i32(_e214.z));
            let _e223 = shadow;
            shadow = (_e223 + step(_e204, _e220.x));
            let _e225 = currentDepth;
            let _e226 = sc;
            let _e229 = texelSize[1u];
            let _e231 = (_e226.xy + vec2<f32>(0f, _e229));
            let _e232 = layer;
            let _e235 = vec3<f32>(_e231.x, _e231.y, _e232);
            let _e241 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e235.x, _e235.y), i32(_e235.z));
            let _e244 = shadow;
            shadow = (_e244 + step(_e225, _e241.x));
            let _e246 = currentDepth;
            let _e247 = sc;
            let _e250 = texelSize[1u];
            let _e252 = (_e247.xy - vec2<f32>(0f, _e250));
            let _e253 = layer;
            let _e256 = vec3<f32>(_e252.x, _e252.y, _e253);
            let _e262 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e256.x, _e256.y), i32(_e256.z));
            let _e265 = shadow;
            shadow = (_e265 + step(_e246, _e262.x));
            let _e267 = shadow;
            shadow = (_e267 / 5f);
        } else {
            x = -1i;
            loop {
                let _e269 = x;
                if (_e269 <= 1i) {
                    y = -1i;
                    loop {
                        let _e271 = y;
                        if (_e271 <= 1i) {
                            let _e273 = currentDepth;
                            let _e274 = sc;
                            let _e276 = x;
                            let _e278 = y;
                            let _e281 = texelSize;
                            let _e283 = (_e274.xy + (vec2<f32>(f32(_e276), f32(_e278)) * _e281));
                            let _e284 = layer;
                            let _e287 = vec3<f32>(_e283.x, _e283.y, _e284);
                            let _e293 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e287.x, _e287.y), i32(_e287.z));
                            let _e296 = shadow;
                            shadow = (_e296 + step(_e273, _e293.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e298 = y;
                            y = (_e298 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e300 = x;
                    x = (_e300 + 1i);
                }
            }
            let _e302 = shadow;
            shadow = (_e302 / 9f);
        }
    }
    let _e304 = shadow;
    return _e304;
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

    let _e95 = unnamed.cascadeSplits;
    let _e96 = (*viewDepth);
    cmp = step(_e95, vec4(_e96));
    let _e100 = cmp[0u];
    let _e102 = cmp[1u];
    let _e105 = cmp[2u];
    let _e108 = cmp[3u];
    cascade = min(i32((((_e100 + _e102) + _e105) + _e108)), 3i);
    let _e112 = cascade;
    (*outCascade) = _e112;
    let _e113 = cascade;
    if (_e113 == 0i) {
        local = 0f;
    } else {
        let _e115 = cascade;
        let _e120 = unnamed.cascadeSplits[max((_e115 - 1i), 0i)];
        local = _e120;
    }
    let _e121 = local;
    prevSplit = _e121;
    let _e122 = cascade;
    let _e125 = unnamed.cascadeSplits[_e122];
    farSplit = _e125;
    let _e126 = farSplit;
    let _e127 = prevSplit;
    blendRange = max((0.1f * (_e126 - _e127)), 1f);
    let _e131 = farSplit;
    let _e132 = (*viewDepth);
    let _e134 = blendRange;
    blendT = clamp(((_e131 - _e132) / _e134), 0f, 1f);
    let _e137 = cascade;
    param = _e137;
    let _e138 = (*worldPos_1);
    param_1 = _e138;
    let _e139 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e139;
    let _e140 = cascade;
    param_2 = min((_e140 + 1i), 3i);
    let _e143 = (*worldPos_1);
    param_3 = _e143;
    let _e144 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e144;
    let _e145 = s1_;
    let _e146 = s0_;
    let _e147 = blendT;
    return mix(_e145, _e146, _e147);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e89 = unnamed.packed_indices[1i][3u];
    let _e95 = unnamed.packed_indices[1i][3u];
    let _e100 = (*lm_uv);
    let _e101 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    sun_mask = _e101.x;
    let _e103 = sun_mask;
    if (_e103 > 0.001f) {
        let _e105 = shadowData_1;
        param_4 = _e105.xyz;
        let _e108 = shadowData_1[3u];
        param_5 = _e108;
        let _e109 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e110 = param_6;
        ignoredCascade = _e110;
        shadow_1 = _e109;
        let _e111 = shadow_1;
        let _e112 = sun_mask;
        let _e114 = (*rgb);
        (*rgb) = (_e114 * mix(1f, _e111, _e112));
    }
    let _e116 = (*rgb);
    return _e116;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;
    var param_11: vec3<f32>;
    var param_12: vec2<f32>;

    if override_type_11_2 {
        let _e85 = (*rgb_1);
        param_7 = _e85;
        let _e86 = frag_tex_coord0_1;
        param_8 = _e86;
        let _e87 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e87;
    }
    if override_type_11_3 {
        let _e88 = (*rgb_1);
        param_9 = _e88;
        let _e89 = frag_tex_coord1_1;
        param_10 = _e89;
        let _e90 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e90;
    }
    if override_type_11_4 {
        let _e91 = (*rgb_1);
        param_11 = _e91;
        let _e92 = frag_tex_coord2_1;
        param_12 = _e92;
        let _e93 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_11), (&param_12));
        return _e93;
    }
    let _e94 = (*rgb_1);
    return _e94;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e82 = (*c_1);
    (*c_1) = max(_e82, vec3<f32>(0f, 0f, 0f));
    let _e84 = (*c_1);
    cutoff = (_e84 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e86 = (*c_1);
    lo = (_e86 / vec3(12.92f));
    let _e89 = (*c_1);
    hi = pow(((_e89 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e94 = hi;
    let _e95 = lo;
    let _e96 = cutoff;
    return mix(_e94, _e95, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e96));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_13: vec3<f32>;

    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e105 = (*uv);
    let _e106 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    c_2 = _e106;
    let _e107 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e107))) == 0i) {
        let _e112 = c_2;
        param_13 = _e112.xyz;
        let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_13));
        c_2[0u] = _e114.x;
        c_2[1u] = _e114.y;
        c_2[2u] = _e114.z;
    }
    let _e121 = (*slot);
    if (lightmap_slot == (_e121 + 1i)) {
        let _e126 = unnamed.worldLightParams[0u];
        let _e127 = c_2;
        let _e129 = (_e127.xyz * _e126);
        c_2[0u] = _e129.x;
        c_2[1u] = _e129.y;
        c_2[2u] = _e129.z;
    }
    let _e136 = c_2;
    return _e136;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e150 = unnamed.packed_indices[0i][3u];
    let _e156 = unnamed.packed_indices[0i][3u];
    let _e161 = fog_tex_coord_1;
    let _e162 = textureSample(wired_bindless_images[(_e150 & 4095u)], wired_bindless_samplers[((_e156 >> bitcast<u32>(12i)) & 255u)], _e161);
    fog = _e162;
    let _e163 = frag_color0In_1;
    param_14 = _e163.xyz;
    let _e165 = sRGBToLinear_u0028_vf3_u003b((&param_14));
    let _e167 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e165.x, _e165.y, _e165.z, _e167);
    let _e172 = frag_color1In_1;
    param_15 = _e172.xyz;
    let _e174 = sRGBToLinear_u0028_vf3_u003b((&param_15));
    let _e176 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e174.x, _e174.y, _e174.z, _e176);
    let _e181 = frag_color2In_1;
    param_16 = _e181.xyz;
    let _e183 = sRGBToLinear_u0028_vf3_u003b((&param_16));
    let _e185 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e183.x, _e183.y, _e183.z, _e185);
    param_17 = 0u;
    let _e190 = frag_tex_coord0_1;
    param_18 = _e190;
    param_19 = 0i;
    let _e191 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
    let _e192 = frag_color0_;
    color0_ = (_e191 * _e192);
    if override_type_11_7 {
        param_20 = 1u;
        let _e194 = frag_tex_coord1_1;
        param_21 = _e194;
        param_22 = 1i;
        let _e195 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
        let _e196 = frag_color1_;
        color1_ = (_e195 * _e196);
        param_23 = 2u;
        let _e198 = frag_tex_coord2_1;
        param_24 = _e198;
        param_25 = 2i;
        let _e199 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_23), (&param_24), (&param_25));
        let _e200 = frag_color2_;
        color2_ = (_e199 * _e200);
        let _e202 = color0_;
        let _e204 = color1_;
        let _e207 = color2_;
        let _e209 = ((_e202.xyz + _e204.xyz) + _e207.xyz);
        let _e211 = color0_[3u];
        let _e213 = color1_[3u];
        let _e216 = color2_[3u];
        base = vec4<f32>(_e209.x, _e209.y, _e209.z, ((_e211 * _e213) * _e216));
    } else {
        if override_type_11_8 {
            param_26 = 1u;
            let _e222 = frag_tex_coord1_1;
            param_27 = _e222;
            param_28 = 1i;
            let _e223 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_26), (&param_27), (&param_28));
            let _e224 = frag_color1_;
            color1_1 = (_e223 * _e224);
            param_29 = 2u;
            let _e226 = frag_tex_coord2_1;
            param_30 = _e226;
            param_31 = 2i;
            let _e227 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_29), (&param_30), (&param_31));
            let _e228 = frag_color2_;
            color2_1 = (_e227 * _e228);
            let _e231 = color0_[3u];
            let _e232 = color0_;
            color0_ = (_e232 * _e231);
            let _e235 = color1_1[3u];
            let _e236 = color1_1;
            color1_1 = (_e236 * _e235);
            let _e239 = color2_1[3u];
            let _e240 = color2_1;
            color2_1 = (_e240 * _e239);
            let _e242 = color0_;
            let _e244 = color1_1;
            let _e247 = color2_1;
            let _e249 = ((_e242.xyz + _e244.xyz) + _e247.xyz);
            let _e251 = color0_[3u];
            let _e253 = color1_1[3u];
            let _e256 = color2_1[3u];
            base = vec4<f32>(_e249.x, _e249.y, _e249.z, ((_e251 * _e253) * _e256));
        } else {
            if override_type_11_9 {
                param_32 = 1u;
                let _e262 = frag_tex_coord1_1;
                param_33 = _e262;
                param_34 = 1i;
                let _e263 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_32), (&param_33), (&param_34));
                let _e264 = frag_color1_;
                color1_2 = (_e263 * _e264);
                param_35 = 2u;
                let _e266 = frag_tex_coord2_1;
                param_36 = _e266;
                param_37 = 2i;
                let _e267 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_35), (&param_36), (&param_37));
                let _e268 = frag_color2_;
                color2_2 = (_e267 * _e268);
                let _e271 = color0_[3u];
                let _e273 = color0_;
                color0_ = (_e273 * (1f - _e271));
                let _e276 = color1_2[3u];
                let _e278 = color1_2;
                color1_2 = (_e278 * (1f - _e276));
                let _e281 = color2_2[3u];
                let _e283 = color2_2;
                color2_2 = (_e283 * (1f - _e281));
                let _e285 = color0_;
                let _e287 = color1_2;
                let _e290 = color2_2;
                let _e292 = ((_e285.xyz + _e287.xyz) + _e290.xyz);
                let _e294 = color0_[3u];
                let _e296 = color1_2[3u];
                let _e299 = color2_2[3u];
                base = vec4<f32>(_e292.x, _e292.y, _e292.z, ((_e294 * _e296) * _e299));
            } else {
                if override_type_11_10 {
                    param_38 = 1u;
                    let _e305 = frag_tex_coord1_1;
                    param_39 = _e305;
                    param_40 = 1i;
                    let _e306 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_38), (&param_39), (&param_40));
                    let _e307 = frag_color1_;
                    color1_3 = (_e306 * _e307);
                    param_41 = 2u;
                    let _e309 = frag_tex_coord2_1;
                    param_42 = _e309;
                    param_43 = 2i;
                    let _e310 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_41), (&param_42), (&param_43));
                    let _e311 = frag_color2_;
                    color2_3 = (_e310 * _e311);
                    let _e313 = color0_;
                    let _e314 = color1_3;
                    let _e316 = color1_3[3u];
                    let _e319 = color2_3;
                    let _e321 = color2_3[3u];
                    base = mix(mix(_e313, _e314, vec4(_e316)), _e319, vec4(_e321));
                } else {
                    if override_type_11_11 {
                        param_44 = 1u;
                        let _e324 = frag_tex_coord1_1;
                        param_45 = _e324;
                        param_46 = 1i;
                        let _e325 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_44), (&param_45), (&param_46));
                        let _e326 = frag_color1_;
                        color1_4 = (_e325 * _e326);
                        param_47 = 2u;
                        let _e328 = frag_tex_coord2_1;
                        param_48 = _e328;
                        param_49 = 2i;
                        let _e329 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_47), (&param_48), (&param_49));
                        let _e330 = frag_color2_;
                        color2_4 = (_e329 * _e330);
                        let _e332 = color2_4;
                        let _e333 = color1_4;
                        let _e334 = color0_;
                        let _e336 = color1_4[3u];
                        let _e340 = color2_4[3u];
                        base = mix(_e332, mix(_e333, _e334, vec4(_e336)), vec4(_e340));
                    } else {
                        if override_type_11_12 {
                            param_50 = 1u;
                            let _e343 = frag_tex_coord1_1;
                            param_51 = _e343;
                            param_52 = 1i;
                            let _e344 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_50), (&param_51), (&param_52));
                            let _e345 = frag_color1_;
                            color1_5 = (_e344 * _e345);
                            param_53 = 2u;
                            let _e347 = frag_tex_coord2_1;
                            param_54 = _e347;
                            param_55 = 2i;
                            let _e348 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_53), (&param_54), (&param_55));
                            let _e349 = frag_color2_;
                            color2_5 = (_e348 * _e349);
                            let _e351 = color2_5;
                            let _e353 = color2_5[3u];
                            let _e356 = color1_5;
                            let _e358 = color1_5[3u];
                            let _e362 = color0_;
                            base = (((_e351 + vec4(_e353)) * (_e356 + vec4(_e358))) * _e362);
                        } else {
                            param_56 = 1u;
                            let _e364 = frag_tex_coord1_1;
                            param_57 = _e364;
                            param_58 = 1i;
                            let _e365 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_56), (&param_57), (&param_58));
                            let _e366 = frag_color1_;
                            color1_6 = (_e365 * _e366);
                            param_59 = 2u;
                            let _e368 = frag_tex_coord2_1;
                            param_60 = _e368;
                            param_61 = 2i;
                            let _e369 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_59), (&param_60), (&param_61));
                            let _e370 = frag_color2_;
                            color2_6 = (_e369 * _e370);
                            let _e372 = color0_;
                            let _e374 = color1_6;
                            let _e377 = color2_6;
                            let _e379 = ((_e372.xyz * _e374.xyz) * _e377.xyz);
                            base[0u] = _e379.x;
                            base[1u] = _e379.y;
                            base[2u] = _e379.z;
                            let _e387 = color0_[3u];
                            let _e389 = color1_6[3u];
                            let _e392 = color2_6[3u];
                            base[3u] = ((_e387 * _e389) * _e392);
                        }
                    }
                }
            }
        }
    }
    let _e395 = base;
    param_62 = _e395.xyz;
    let _e397 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_62));
    base[0u] = _e397.x;
    base[1u] = _e397.y;
    base[2u] = _e397.z;
    if override_type_11_13 {
        let _e404 = base;
        let _e407 = fog[3u];
        let _e409 = (_e404.xyz * (1f - _e407));
        base[0u] = _e409.x;
        base[1u] = _e409.y;
        base[2u] = _e409.z;
    } else {
        if override_type_11_14 {
            let _e416 = base;
            let _e418 = fog[3u];
            base = (_e416 * (1f - _e418));
        } else {
            if override_type_11_15 {
                let _e422 = base[3u];
                let _e424 = fog[3u];
                base[3u] = (_e422 * (1f - _e424));
            } else {
                let _e428 = base;
                let _e429 = fog;
                let _e431 = unnamed.fogColor;
                let _e434 = fog[3u];
                base = mix(_e428, (_e429 * _e431), vec4(_e434));
            }
        }
    }
    if override_type_11_16 {
        let _e438 = base[3u];
        if (_e438 == 0f) {
            discard;
        }
    } else {
        if override_type_11_17 {
            let _e440 = base;
            let _e442 = base;
            if (dot(_e440.xyz, _e442.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e446 = base;
    out_color = _e446;
    return;
}

@fragment 
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    main_1();
    let _e17 = out_color;
    return _e17;
}
