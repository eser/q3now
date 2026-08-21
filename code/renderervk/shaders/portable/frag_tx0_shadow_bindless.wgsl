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
@id(7) override discard_mode: i32 = 0i;
override override_type_11_3: bool = (discard_mode == 1i);
override override_type_11_4: bool = (discard_mode == 2i);
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
var<private> frag_color0In_1: vec4<f32>;
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

    let _e65 = (*c);
    let _e68 = unnamed.cascadeMVP[_e65];
    let _e69 = (*worldPos);
    sc4_ = (_e68 * vec4<f32>(_e69.x, _e69.y, _e69.z, 1f));
    let _e75 = sc4_;
    let _e78 = sc4_[3u];
    sc = (_e75.xyz / vec3(_e78));
    let _e81 = sc;
    let _e85 = ((_e81.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e85.x;
    sc[1u] = _e85.y;
    let _e91 = sc[0u];
    let _e92 = (_e91 < 0f);
    phi_218_ = _e92;
    if !(_e92) {
        let _e95 = sc[0u];
        phi_218_ = (_e95 > 1f);
    }
    let _e98 = phi_218_;
    phi_225_ = _e98;
    if !(_e98) {
        let _e101 = sc[1u];
        phi_225_ = (_e101 < 0f);
    }
    let _e104 = phi_225_;
    phi_232_ = _e104;
    if !(_e104) {
        let _e107 = sc[1u];
        phi_232_ = (_e107 > 1f);
    }
    let _e110 = phi_232_;
    phi_239_ = _e110;
    if !(_e110) {
        let _e113 = sc[2u];
        phi_239_ = (_e113 > 1f);
    }
    let _e116 = phi_239_;
    if _e116 {
        return 1f;
    }
    let _e117 = (*c);
    layer = f32(_e117);
    let _e119 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e119).xy));
    let _e126 = sc[2u];
    currentDepth = (_e126 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e128 = currentDepth;
        let _e129 = sc;
        let _e130 = _e129.xy;
        let _e131 = layer;
        let _e134 = vec3<f32>(_e130.x, _e130.y, _e131);
        let _e140 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e134.x, _e134.y), i32(_e134.z));
        shadow = step(_e128, _e140.x);
    } else {
        if override_type_11_1 {
            let _e143 = currentDepth;
            let _e144 = sc;
            let _e145 = _e144.xy;
            let _e146 = layer;
            let _e149 = vec3<f32>(_e145.x, _e145.y, _e146);
            let _e155 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e149.x, _e149.y), i32(_e149.z));
            let _e158 = shadow;
            shadow = (_e158 + step(_e143, _e155.x));
            let _e160 = currentDepth;
            let _e161 = sc;
            let _e164 = texelSize[0u];
            let _e166 = (_e161.xy + vec2<f32>(_e164, 0f));
            let _e167 = layer;
            let _e170 = vec3<f32>(_e166.x, _e166.y, _e167);
            let _e176 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e170.x, _e170.y), i32(_e170.z));
            let _e179 = shadow;
            shadow = (_e179 + step(_e160, _e176.x));
            let _e181 = currentDepth;
            let _e182 = sc;
            let _e185 = texelSize[0u];
            let _e187 = (_e182.xy - vec2<f32>(_e185, 0f));
            let _e188 = layer;
            let _e191 = vec3<f32>(_e187.x, _e187.y, _e188);
            let _e197 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e191.x, _e191.y), i32(_e191.z));
            let _e200 = shadow;
            shadow = (_e200 + step(_e181, _e197.x));
            let _e202 = currentDepth;
            let _e203 = sc;
            let _e206 = texelSize[1u];
            let _e208 = (_e203.xy + vec2<f32>(0f, _e206));
            let _e209 = layer;
            let _e212 = vec3<f32>(_e208.x, _e208.y, _e209);
            let _e218 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e212.x, _e212.y), i32(_e212.z));
            let _e221 = shadow;
            shadow = (_e221 + step(_e202, _e218.x));
            let _e223 = currentDepth;
            let _e224 = sc;
            let _e227 = texelSize[1u];
            let _e229 = (_e224.xy - vec2<f32>(0f, _e227));
            let _e230 = layer;
            let _e233 = vec3<f32>(_e229.x, _e229.y, _e230);
            let _e239 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e233.x, _e233.y), i32(_e233.z));
            let _e242 = shadow;
            shadow = (_e242 + step(_e223, _e239.x));
            let _e244 = shadow;
            shadow = (_e244 / 5f);
        } else {
            x = -1i;
            loop {
                let _e246 = x;
                if (_e246 <= 1i) {
                    y = -1i;
                    loop {
                        let _e248 = y;
                        if (_e248 <= 1i) {
                            let _e250 = currentDepth;
                            let _e251 = sc;
                            let _e253 = x;
                            let _e255 = y;
                            let _e258 = texelSize;
                            let _e260 = (_e251.xy + (vec2<f32>(f32(_e253), f32(_e255)) * _e258));
                            let _e261 = layer;
                            let _e264 = vec3<f32>(_e260.x, _e260.y, _e261);
                            let _e270 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e264.x, _e264.y), i32(_e264.z));
                            let _e273 = shadow;
                            shadow = (_e273 + step(_e250, _e270.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e275 = y;
                            y = (_e275 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e277 = x;
                    x = (_e277 + 1i);
                }
            }
            let _e279 = shadow;
            shadow = (_e279 / 9f);
        }
    }
    let _e281 = shadow;
    return _e281;
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

    let _e72 = unnamed.cascadeSplits;
    let _e73 = (*viewDepth);
    cmp = step(_e72, vec4(_e73));
    let _e77 = cmp[0u];
    let _e79 = cmp[1u];
    let _e82 = cmp[2u];
    let _e85 = cmp[3u];
    cascade = min(i32((((_e77 + _e79) + _e82) + _e85)), 3i);
    let _e89 = cascade;
    (*outCascade) = _e89;
    let _e90 = cascade;
    if (_e90 == 0i) {
        local = 0f;
    } else {
        let _e92 = cascade;
        let _e97 = unnamed.cascadeSplits[max((_e92 - 1i), 0i)];
        local = _e97;
    }
    let _e98 = local;
    prevSplit = _e98;
    let _e99 = cascade;
    let _e102 = unnamed.cascadeSplits[_e99];
    farSplit = _e102;
    let _e103 = farSplit;
    let _e104 = prevSplit;
    blendRange = max((0.1f * (_e103 - _e104)), 1f);
    let _e108 = farSplit;
    let _e109 = (*viewDepth);
    let _e111 = blendRange;
    blendT = clamp(((_e108 - _e109) / _e111), 0f, 1f);
    let _e114 = cascade;
    param = _e114;
    let _e115 = (*worldPos_1);
    param_1 = _e115;
    let _e116 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e116;
    let _e117 = cascade;
    param_2 = min((_e117 + 1i), 3i);
    let _e120 = (*worldPos_1);
    param_3 = _e120;
    let _e121 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e121;
    let _e122 = s1_;
    let _e123 = s0_;
    let _e124 = blendT;
    return mix(_e122, _e123, _e124);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e66 = unnamed.packed_indices[1i][3u];
    let _e72 = unnamed.packed_indices[1i][3u];
    let _e77 = (*lm_uv);
    let _e78 = textureSample(wired_bindless_images[(_e66 & 4095u)], wired_bindless_samplers[((_e72 >> bitcast<u32>(12i)) & 255u)], _e77);
    sun_mask = _e78.x;
    let _e80 = sun_mask;
    if (_e80 > 0.001f) {
        let _e82 = shadowData_1;
        param_4 = _e82.xyz;
        let _e85 = shadowData_1[3u];
        param_5 = _e85;
        let _e86 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e87 = param_6;
        ignoredCascade = _e87;
        shadow_1 = _e86;
        let _e88 = shadow_1;
        let _e89 = sun_mask;
        let _e91 = (*rgb);
        (*rgb) = (_e91 * mix(1f, _e88, _e89));
    }
    let _e93 = (*rgb);
    return _e93;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_11_2 {
        let _e58 = (*rgb_1);
        param_7 = _e58;
        let _e59 = frag_tex_coord0_1;
        param_8 = _e59;
        let _e60 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e60;
    }
    let _e61 = (*rgb_1);
    return _e61;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e59 = (*c_1);
    (*c_1) = max(_e59, vec3<f32>(0f, 0f, 0f));
    let _e61 = (*c_1);
    cutoff = (_e61 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e63 = (*c_1);
    lo = (_e63 / vec3(12.92f));
    let _e66 = (*c_1);
    hi = pow(((_e66 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e71 = hi;
    let _e72 = lo;
    let _e73 = cutoff;
    return mix(_e71, _e72, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e73));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e60 = (*role);
    let _e62 = (*role);
    let _e67 = unnamed.packed_indices[(_e60 / 4u)][(_e62 % 4u)];
    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e82 = (*uv);
    let _e83 = textureSample(wired_bindless_images[(_e67 & 4095u)], wired_bindless_samplers[((_e77 >> bitcast<u32>(12i)) & 255u)], _e82);
    c_2 = _e83;
    let _e84 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e84))) == 0i) {
        let _e89 = c_2;
        param_9 = _e89.xyz;
        let _e91 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e91.x;
        c_2[1u] = _e91.y;
        c_2[2u] = _e91.z;
    }
    let _e98 = (*slot);
    if (lightmap_slot == (_e98 + 1i)) {
        let _e103 = unnamed.worldLightParams[0u];
        let _e104 = c_2;
        let _e106 = (_e104.xyz * _e103);
        c_2[0u] = _e106.x;
        c_2[1u] = _e106.y;
        c_2[2u] = _e106.z;
    }
    let _e113 = c_2;
    return _e113;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_10: vec3<f32>;
    var color0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var param_14: vec3<f32>;

    let _e63 = frag_color0In_1;
    param_10 = _e63.xyz;
    let _e65 = sRGBToLinear_u0028_vf3_u003b((&param_10));
    let _e67 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e65.x, _e65.y, _e65.z, _e67);
    param_11 = 0u;
    let _e72 = frag_tex_coord0_1;
    param_12 = _e72;
    param_13 = 0i;
    let _e73 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    let _e74 = frag_color0_;
    color0_ = (_e73 * _e74);
    let _e76 = color0_;
    base = _e76;
    let _e77 = color0_;
    base = _e77;
    let _e78 = base;
    param_14 = _e78.xyz;
    let _e80 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e80.x;
    base[1u] = _e80.y;
    base[2u] = _e80.z;
    if override_type_11_3 {
        let _e88 = base[3u];
        if (_e88 == 0f) {
            discard;
        }
    } else {
        if override_type_11_4 {
            let _e90 = base;
            let _e92 = base;
            if (dot(_e90.xyz, _e92.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e96 = base;
    out_color = _e96;
    return;
}

@fragment 
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e7 = out_color;
    return _e7;
}
