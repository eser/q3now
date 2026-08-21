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
@id(10) override acff: i32 = 0i;
override override_type_11_6: bool = (acff == 1i);
override override_type_11_7: bool = (acff == 2i);
override override_type_11_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_9: bool = (discard_mode == 1i);
override override_type_11_10: bool = (discard_mode == 2i);
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

    let _e75 = (*c);
    let _e78 = unnamed.cascadeMVP[_e75];
    let _e79 = (*worldPos);
    sc4_ = (_e78 * vec4<f32>(_e79.x, _e79.y, _e79.z, 1f));
    let _e85 = sc4_;
    let _e88 = sc4_[3u];
    sc = (_e85.xyz / vec3(_e88));
    let _e91 = sc;
    let _e95 = ((_e91.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e95.x;
    sc[1u] = _e95.y;
    let _e101 = sc[0u];
    let _e102 = (_e101 < 0f);
    phi_218_ = _e102;
    if !(_e102) {
        let _e105 = sc[0u];
        phi_218_ = (_e105 > 1f);
    }
    let _e108 = phi_218_;
    phi_225_ = _e108;
    if !(_e108) {
        let _e111 = sc[1u];
        phi_225_ = (_e111 < 0f);
    }
    let _e114 = phi_225_;
    phi_232_ = _e114;
    if !(_e114) {
        let _e117 = sc[1u];
        phi_232_ = (_e117 > 1f);
    }
    let _e120 = phi_232_;
    phi_239_ = _e120;
    if !(_e120) {
        let _e123 = sc[2u];
        phi_239_ = (_e123 > 1f);
    }
    let _e126 = phi_239_;
    if _e126 {
        return 1f;
    }
    let _e127 = (*c);
    layer = f32(_e127);
    let _e129 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e129).xy));
    let _e136 = sc[2u];
    currentDepth = (_e136 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e138 = currentDepth;
        let _e139 = sc;
        let _e140 = _e139.xy;
        let _e141 = layer;
        let _e144 = vec3<f32>(_e140.x, _e140.y, _e141);
        let _e150 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e144.x, _e144.y), i32(_e144.z));
        shadow = step(_e138, _e150.x);
    } else {
        if override_type_11_1 {
            let _e153 = currentDepth;
            let _e154 = sc;
            let _e155 = _e154.xy;
            let _e156 = layer;
            let _e159 = vec3<f32>(_e155.x, _e155.y, _e156);
            let _e165 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e159.x, _e159.y), i32(_e159.z));
            let _e168 = shadow;
            shadow = (_e168 + step(_e153, _e165.x));
            let _e170 = currentDepth;
            let _e171 = sc;
            let _e174 = texelSize[0u];
            let _e176 = (_e171.xy + vec2<f32>(_e174, 0f));
            let _e177 = layer;
            let _e180 = vec3<f32>(_e176.x, _e176.y, _e177);
            let _e186 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e180.x, _e180.y), i32(_e180.z));
            let _e189 = shadow;
            shadow = (_e189 + step(_e170, _e186.x));
            let _e191 = currentDepth;
            let _e192 = sc;
            let _e195 = texelSize[0u];
            let _e197 = (_e192.xy - vec2<f32>(_e195, 0f));
            let _e198 = layer;
            let _e201 = vec3<f32>(_e197.x, _e197.y, _e198);
            let _e207 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e201.x, _e201.y), i32(_e201.z));
            let _e210 = shadow;
            shadow = (_e210 + step(_e191, _e207.x));
            let _e212 = currentDepth;
            let _e213 = sc;
            let _e216 = texelSize[1u];
            let _e218 = (_e213.xy + vec2<f32>(0f, _e216));
            let _e219 = layer;
            let _e222 = vec3<f32>(_e218.x, _e218.y, _e219);
            let _e228 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e222.x, _e222.y), i32(_e222.z));
            let _e231 = shadow;
            shadow = (_e231 + step(_e212, _e228.x));
            let _e233 = currentDepth;
            let _e234 = sc;
            let _e237 = texelSize[1u];
            let _e239 = (_e234.xy - vec2<f32>(0f, _e237));
            let _e240 = layer;
            let _e243 = vec3<f32>(_e239.x, _e239.y, _e240);
            let _e249 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e243.x, _e243.y), i32(_e243.z));
            let _e252 = shadow;
            shadow = (_e252 + step(_e233, _e249.x));
            let _e254 = shadow;
            shadow = (_e254 / 5f);
        } else {
            x = -1i;
            loop {
                let _e256 = x;
                if (_e256 <= 1i) {
                    y = -1i;
                    loop {
                        let _e258 = y;
                        if (_e258 <= 1i) {
                            let _e260 = currentDepth;
                            let _e261 = sc;
                            let _e263 = x;
                            let _e265 = y;
                            let _e268 = texelSize;
                            let _e270 = (_e261.xy + (vec2<f32>(f32(_e263), f32(_e265)) * _e268));
                            let _e271 = layer;
                            let _e274 = vec3<f32>(_e270.x, _e270.y, _e271);
                            let _e280 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e274.x, _e274.y), i32(_e274.z));
                            let _e283 = shadow;
                            shadow = (_e283 + step(_e260, _e280.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e285 = y;
                            y = (_e285 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e287 = x;
                    x = (_e287 + 1i);
                }
            }
            let _e289 = shadow;
            shadow = (_e289 / 9f);
        }
    }
    let _e291 = shadow;
    return _e291;
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

    let _e82 = unnamed.cascadeSplits;
    let _e83 = (*viewDepth);
    cmp = step(_e82, vec4(_e83));
    let _e87 = cmp[0u];
    let _e89 = cmp[1u];
    let _e92 = cmp[2u];
    let _e95 = cmp[3u];
    cascade = min(i32((((_e87 + _e89) + _e92) + _e95)), 3i);
    let _e99 = cascade;
    (*outCascade) = _e99;
    let _e100 = cascade;
    if (_e100 == 0i) {
        local = 0f;
    } else {
        let _e102 = cascade;
        let _e107 = unnamed.cascadeSplits[max((_e102 - 1i), 0i)];
        local = _e107;
    }
    let _e108 = local;
    prevSplit = _e108;
    let _e109 = cascade;
    let _e112 = unnamed.cascadeSplits[_e109];
    farSplit = _e112;
    let _e113 = farSplit;
    let _e114 = prevSplit;
    blendRange = max((0.1f * (_e113 - _e114)), 1f);
    let _e118 = farSplit;
    let _e119 = (*viewDepth);
    let _e121 = blendRange;
    blendT = clamp(((_e118 - _e119) / _e121), 0f, 1f);
    let _e124 = cascade;
    param = _e124;
    let _e125 = (*worldPos_1);
    param_1 = _e125;
    let _e126 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e126;
    let _e127 = cascade;
    param_2 = min((_e127 + 1i), 3i);
    let _e130 = (*worldPos_1);
    param_3 = _e130;
    let _e131 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e131;
    let _e132 = s1_;
    let _e133 = s0_;
    let _e134 = blendT;
    return mix(_e132, _e133, _e134);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e76 = unnamed.packed_indices[1i][3u];
    let _e82 = unnamed.packed_indices[1i][3u];
    let _e87 = (*lm_uv);
    let _e88 = textureSample(wired_bindless_images[(_e76 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    sun_mask = _e88.x;
    let _e90 = sun_mask;
    if (_e90 > 0.001f) {
        let _e92 = shadowData_1;
        param_4 = _e92.xyz;
        let _e95 = shadowData_1[3u];
        param_5 = _e95;
        let _e96 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e97 = param_6;
        ignoredCascade = _e97;
        shadow_1 = _e96;
        let _e98 = shadow_1;
        let _e99 = sun_mask;
        let _e101 = (*rgb);
        (*rgb) = (_e101 * mix(1f, _e98, _e99));
    }
    let _e103 = (*rgb);
    return _e103;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;
    var param_9: vec3<f32>;
    var param_10: vec2<f32>;

    if override_type_11_2 {
        let _e70 = (*rgb_1);
        param_7 = _e70;
        let _e71 = frag_tex_coord0_1;
        param_8 = _e71;
        let _e72 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e72;
    }
    if override_type_11_3 {
        let _e73 = (*rgb_1);
        param_9 = _e73;
        let _e74 = frag_tex_coord1_1;
        param_10 = _e74;
        let _e75 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_9), (&param_10));
        return _e75;
    }
    let _e76 = (*rgb_1);
    return _e76;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c_1);
    (*c_1) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c_1);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c_1);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c_1);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_11: vec3<f32>;

    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e92 = (*uv);
    let _e93 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    c_2 = _e93;
    let _e94 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e94))) == 0i) {
        let _e99 = c_2;
        param_11 = _e99.xyz;
        let _e101 = sRGBToLinear_u0028_vf3_u003b((&param_11));
        c_2[0u] = _e101.x;
        c_2[1u] = _e101.y;
        c_2[2u] = _e101.z;
    }
    let _e108 = (*slot);
    if (lightmap_slot == (_e108 + 1i)) {
        let _e113 = unnamed.worldLightParams[0u];
        let _e114 = c_2;
        let _e116 = (_e114.xyz * _e113);
        c_2[0u] = _e116.x;
        c_2[1u] = _e116.y;
        c_2[2u] = _e116.z;
    }
    let _e123 = c_2;
    return _e123;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_2: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var param_24: vec3<f32>;

    let _e87 = unnamed.packed_indices[0i][3u];
    let _e93 = unnamed.packed_indices[0i][3u];
    let _e98 = fog_tex_coord_1;
    let _e99 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    fog = _e99;
    param_12 = 0u;
    let _e100 = frag_tex_coord0_1;
    param_13 = _e100;
    param_14 = 0i;
    let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    color0_ = _e101;
    if override_type_11_4 {
        param_15 = 1u;
        let _e102 = frag_tex_coord1_1;
        param_16 = _e102;
        param_17 = 1i;
        let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
        color1_ = _e103;
        let _e104 = color0_;
        let _e106 = color1_;
        let _e108 = (_e104.xyz + _e106.xyz);
        let _e110 = color0_[3u];
        let _e112 = color1_[3u];
        base = vec4<f32>(_e108.x, _e108.y, _e108.z, (_e110 * _e112));
    } else {
        if override_type_11_5 {
            param_18 = 1u;
            let _e118 = frag_tex_coord1_1;
            param_19 = _e118;
            param_20 = 1i;
            let _e119 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            color1_1 = _e119;
            let _e120 = color0_;
            let _e122 = color1_1;
            let _e124 = (_e120.xyz + _e122.xyz);
            let _e126 = color0_[3u];
            let _e128 = color1_1[3u];
            base = vec4<f32>(_e124.x, _e124.y, _e124.z, (_e126 * _e128));
        } else {
            param_21 = 1u;
            let _e134 = frag_tex_coord1_1;
            param_22 = _e134;
            param_23 = 1i;
            let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            color1_2 = _e135;
            let _e136 = color0_;
            let _e138 = color1_2;
            let _e140 = (_e136.xyz * _e138.xyz);
            base[0u] = _e140.x;
            base[1u] = _e140.y;
            base[2u] = _e140.z;
            let _e148 = color0_[3u];
            let _e150 = color1_2[3u];
            base[3u] = (_e148 * _e150);
        }
    }
    let _e153 = base;
    param_24 = _e153.xyz;
    let _e155 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_24));
    base[0u] = _e155.x;
    base[1u] = _e155.y;
    base[2u] = _e155.z;
    if override_type_11_6 {
        let _e162 = base;
        let _e165 = fog[3u];
        let _e167 = (_e162.xyz * (1f - _e165));
        base[0u] = _e167.x;
        base[1u] = _e167.y;
        base[2u] = _e167.z;
    } else {
        if override_type_11_7 {
            let _e174 = base;
            let _e176 = fog[3u];
            base = (_e174 * (1f - _e176));
        } else {
            if override_type_11_8 {
                let _e180 = base[3u];
                let _e182 = fog[3u];
                base[3u] = (_e180 * (1f - _e182));
            } else {
                let _e186 = base;
                let _e187 = fog;
                let _e189 = unnamed.fogColor;
                let _e192 = fog[3u];
                base = mix(_e186, (_e187 * _e189), vec4(_e192));
            }
        }
    }
    if override_type_11_9 {
        let _e196 = base[3u];
        if (_e196 == 0f) {
            discard;
        }
    } else {
        if override_type_11_10 {
            let _e198 = base;
            let _e200 = base;
            if (dot(_e198.xyz, _e200.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e204 = base;
    out_color = _e204;
    return;
}

@fragment 
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    fog_tex_coord_1 = fog_tex_coord;
    main_1();
    let _e9 = out_color;
    return _e9;
}
