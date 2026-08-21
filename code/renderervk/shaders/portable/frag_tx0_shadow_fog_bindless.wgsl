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
@id(10) override acff: i32 = 0i;
override override_type_11_3: bool = (acff == 1i);
override override_type_11_4: bool = (acff == 2i);
override override_type_11_5: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_11_6: bool = (discard_mode == 1i);
override override_type_11_7: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e71 = (*c);
    let _e74 = unnamed.cascadeMVP[_e71];
    let _e75 = (*worldPos);
    sc4_ = (_e74 * vec4<f32>(_e75.x, _e75.y, _e75.z, 1f));
    let _e81 = sc4_;
    let _e84 = sc4_[3u];
    sc = (_e81.xyz / vec3(_e84));
    let _e87 = sc;
    let _e91 = ((_e87.xy * 0.5f) + vec2(0.5f));
    sc[0u] = _e91.x;
    sc[1u] = _e91.y;
    let _e97 = sc[0u];
    let _e98 = (_e97 < 0f);
    phi_218_ = _e98;
    if !(_e98) {
        let _e101 = sc[0u];
        phi_218_ = (_e101 > 1f);
    }
    let _e104 = phi_218_;
    phi_225_ = _e104;
    if !(_e104) {
        let _e107 = sc[1u];
        phi_225_ = (_e107 < 0f);
    }
    let _e110 = phi_225_;
    phi_232_ = _e110;
    if !(_e110) {
        let _e113 = sc[1u];
        phi_232_ = (_e113 > 1f);
    }
    let _e116 = phi_232_;
    phi_239_ = _e116;
    if !(_e116) {
        let _e119 = sc[2u];
        phi_239_ = (_e119 > 1f);
    }
    let _e122 = phi_239_;
    if _e122 {
        return 1f;
    }
    let _e123 = (*c);
    layer = f32(_e123);
    let _e125 = textureDimensions(shadowMap, 0i);
    texelSize = (vec2(1f) / vec2<f32>(vec2<i32>(_e125).xy));
    let _e132 = sc[2u];
    currentDepth = (_e132 - shadow_bias);
    shadow = 0f;
    if override_type_11_ {
        let _e134 = currentDepth;
        let _e135 = sc;
        let _e136 = _e135.xy;
        let _e137 = layer;
        let _e140 = vec3<f32>(_e136.x, _e136.y, _e137);
        let _e146 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e140.x, _e140.y), i32(_e140.z));
        shadow = step(_e134, _e146.x);
    } else {
        if override_type_11_1 {
            let _e149 = currentDepth;
            let _e150 = sc;
            let _e151 = _e150.xy;
            let _e152 = layer;
            let _e155 = vec3<f32>(_e151.x, _e151.y, _e152);
            let _e161 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e155.x, _e155.y), i32(_e155.z));
            let _e164 = shadow;
            shadow = (_e164 + step(_e149, _e161.x));
            let _e166 = currentDepth;
            let _e167 = sc;
            let _e170 = texelSize[0u];
            let _e172 = (_e167.xy + vec2<f32>(_e170, 0f));
            let _e173 = layer;
            let _e176 = vec3<f32>(_e172.x, _e172.y, _e173);
            let _e182 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e176.x, _e176.y), i32(_e176.z));
            let _e185 = shadow;
            shadow = (_e185 + step(_e166, _e182.x));
            let _e187 = currentDepth;
            let _e188 = sc;
            let _e191 = texelSize[0u];
            let _e193 = (_e188.xy - vec2<f32>(_e191, 0f));
            let _e194 = layer;
            let _e197 = vec3<f32>(_e193.x, _e193.y, _e194);
            let _e203 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e197.x, _e197.y), i32(_e197.z));
            let _e206 = shadow;
            shadow = (_e206 + step(_e187, _e203.x));
            let _e208 = currentDepth;
            let _e209 = sc;
            let _e212 = texelSize[1u];
            let _e214 = (_e209.xy + vec2<f32>(0f, _e212));
            let _e215 = layer;
            let _e218 = vec3<f32>(_e214.x, _e214.y, _e215);
            let _e224 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e218.x, _e218.y), i32(_e218.z));
            let _e227 = shadow;
            shadow = (_e227 + step(_e208, _e224.x));
            let _e229 = currentDepth;
            let _e230 = sc;
            let _e233 = texelSize[1u];
            let _e235 = (_e230.xy - vec2<f32>(0f, _e233));
            let _e236 = layer;
            let _e239 = vec3<f32>(_e235.x, _e235.y, _e236);
            let _e245 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e239.x, _e239.y), i32(_e239.z));
            let _e248 = shadow;
            shadow = (_e248 + step(_e229, _e245.x));
            let _e250 = shadow;
            shadow = (_e250 / 5f);
        } else {
            x = -1i;
            loop {
                let _e252 = x;
                if (_e252 <= 1i) {
                    y = -1i;
                    loop {
                        let _e254 = y;
                        if (_e254 <= 1i) {
                            let _e256 = currentDepth;
                            let _e257 = sc;
                            let _e259 = x;
                            let _e261 = y;
                            let _e264 = texelSize;
                            let _e266 = (_e257.xy + (vec2<f32>(f32(_e259), f32(_e261)) * _e264));
                            let _e267 = layer;
                            let _e270 = vec3<f32>(_e266.x, _e266.y, _e267);
                            let _e276 = textureSample(shadowMap, shadowMap_sampler, vec2<f32>(_e270.x, _e270.y), i32(_e270.z));
                            let _e279 = shadow;
                            shadow = (_e279 + step(_e256, _e276.x));
                            continue;
                        } else {
                            break;
                        }
                        continuing {
                            let _e281 = y;
                            y = (_e281 + 1i);
                        }
                    }
                    continue;
                } else {
                    break;
                }
                continuing {
                    let _e283 = x;
                    x = (_e283 + 1i);
                }
            }
            let _e285 = shadow;
            shadow = (_e285 / 9f);
        }
    }
    let _e287 = shadow;
    return _e287;
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

    let _e78 = unnamed.cascadeSplits;
    let _e79 = (*viewDepth);
    cmp = step(_e78, vec4(_e79));
    let _e83 = cmp[0u];
    let _e85 = cmp[1u];
    let _e88 = cmp[2u];
    let _e91 = cmp[3u];
    cascade = min(i32((((_e83 + _e85) + _e88) + _e91)), 3i);
    let _e95 = cascade;
    (*outCascade) = _e95;
    let _e96 = cascade;
    if (_e96 == 0i) {
        local = 0f;
    } else {
        let _e98 = cascade;
        let _e103 = unnamed.cascadeSplits[max((_e98 - 1i), 0i)];
        local = _e103;
    }
    let _e104 = local;
    prevSplit = _e104;
    let _e105 = cascade;
    let _e108 = unnamed.cascadeSplits[_e105];
    farSplit = _e108;
    let _e109 = farSplit;
    let _e110 = prevSplit;
    blendRange = max((0.1f * (_e109 - _e110)), 1f);
    let _e114 = farSplit;
    let _e115 = (*viewDepth);
    let _e117 = blendRange;
    blendT = clamp(((_e114 - _e115) / _e117), 0f, 1f);
    let _e120 = cascade;
    param = _e120;
    let _e121 = (*worldPos_1);
    param_1 = _e121;
    let _e122 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param), (&param_1));
    s0_ = _e122;
    let _e123 = cascade;
    param_2 = min((_e123 + 1i), 3i);
    let _e126 = (*worldPos_1);
    param_3 = _e126;
    let _e127 = sampleCascade_u0028_i1_u003b_vf3_u003b((&param_2), (&param_3));
    s1_ = _e127;
    let _e128 = s1_;
    let _e129 = s0_;
    let _e130 = blendT;
    return mix(_e128, _e129, _e130);
}

fn wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b(rgb: ptr<function, vec3<f32>>, lm_uv: ptr<function, vec2<f32>>) -> vec3<f32> {
    var sun_mask: f32;
    var shadow_1: f32;
    var ignoredCascade: i32;
    var param_4: vec3<f32>;
    var param_5: f32;
    var param_6: i32;

    let _e72 = unnamed.packed_indices[1i][3u];
    let _e78 = unnamed.packed_indices[1i][3u];
    let _e83 = (*lm_uv);
    let _e84 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e78 >> bitcast<u32>(12i)) & 255u)], _e83);
    sun_mask = _e84.x;
    let _e86 = sun_mask;
    if (_e86 > 0.001f) {
        let _e88 = shadowData_1;
        param_4 = _e88.xyz;
        let _e91 = shadowData_1[3u];
        param_5 = _e91;
        let _e92 = sampleShadow_u0028_vf3_u003b_f1_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        let _e93 = param_6;
        ignoredCascade = _e93;
        shadow_1 = _e92;
        let _e94 = shadow_1;
        let _e95 = sun_mask;
        let _e97 = (*rgb);
        (*rgb) = (_e97 * mix(1f, _e94, _e95));
    }
    let _e99 = (*rgb);
    return _e99;
}

fn wired_apply_sun_shadow_operand_u0028_vf3_u003b(rgb_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var param_7: vec3<f32>;
    var param_8: vec2<f32>;

    if override_type_11_2 {
        let _e64 = (*rgb_1);
        param_7 = _e64;
        let _e65 = frag_tex_coord0_1;
        param_8 = _e65;
        let _e66 = wired_apply_sun_shadow_u0028_vf3_u003b_vf2_u003b((&param_7), (&param_8));
        return _e66;
    }
    let _e67 = (*rgb_1);
    return _e67;
}

fn sRGBToLinear_u0028_vf3_u003b(c_1: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e65 = (*c_1);
    (*c_1) = max(_e65, vec3<f32>(0f, 0f, 0f));
    let _e67 = (*c_1);
    cutoff = (_e67 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e69 = (*c_1);
    lo = (_e69 / vec3(12.92f));
    let _e72 = (*c_1);
    hi = pow(((_e72 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e77 = hi;
    let _e78 = lo;
    let _e79 = cutoff;
    return mix(_e77, _e78, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e79));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_2: vec4<f32>;
    var param_9: vec3<f32>;

    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e88 = (*uv);
    let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    c_2 = _e89;
    let _e90 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e90))) == 0i) {
        let _e95 = c_2;
        param_9 = _e95.xyz;
        let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_9));
        c_2[0u] = _e97.x;
        c_2[1u] = _e97.y;
        c_2[2u] = _e97.z;
    }
    let _e104 = (*slot);
    if (lightmap_slot == (_e104 + 1i)) {
        let _e109 = unnamed.worldLightParams[0u];
        let _e110 = c_2;
        let _e112 = (_e110.xyz * _e109);
        c_2[0u] = _e112.x;
        c_2[1u] = _e112.y;
        c_2[2u] = _e112.z;
    }
    let _e119 = c_2;
    return _e119;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_10: vec3<f32>;
    var color0_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var base: vec4<f32>;
    var param_14: vec3<f32>;

    let _e73 = unnamed.packed_indices[0i][3u];
    let _e79 = unnamed.packed_indices[0i][3u];
    let _e84 = fog_tex_coord_1;
    let _e85 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e79 >> bitcast<u32>(12i)) & 255u)], _e84);
    fog = _e85;
    let _e86 = frag_color0In_1;
    param_10 = _e86.xyz;
    let _e88 = sRGBToLinear_u0028_vf3_u003b((&param_10));
    let _e90 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e88.x, _e88.y, _e88.z, _e90);
    param_11 = 0u;
    let _e95 = frag_tex_coord0_1;
    param_12 = _e95;
    param_13 = 0i;
    let _e96 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    let _e97 = frag_color0_;
    color0_ = (_e96 * _e97);
    let _e99 = color0_;
    base = _e99;
    let _e100 = color0_;
    base = _e100;
    let _e101 = base;
    param_14 = _e101.xyz;
    let _e103 = wired_apply_sun_shadow_operand_u0028_vf3_u003b((&param_14));
    base[0u] = _e103.x;
    base[1u] = _e103.y;
    base[2u] = _e103.z;
    if override_type_11_3 {
        let _e110 = base;
        let _e113 = fog[3u];
        let _e115 = (_e110.xyz * (1f - _e113));
        base[0u] = _e115.x;
        base[1u] = _e115.y;
        base[2u] = _e115.z;
    } else {
        if override_type_11_4 {
            let _e122 = base;
            let _e124 = fog[3u];
            base = (_e122 * (1f - _e124));
        } else {
            if override_type_11_5 {
                let _e128 = base[3u];
                let _e130 = fog[3u];
                base[3u] = (_e128 * (1f - _e130));
            } else {
                let _e134 = base;
                let _e135 = fog;
                let _e137 = unnamed.fogColor;
                let _e140 = fog[3u];
                base = mix(_e134, (_e135 * _e137), vec4(_e140));
            }
        }
    }
    if override_type_11_6 {
        let _e144 = base[3u];
        if (_e144 == 0f) {
            discard;
        }
    } else {
        if override_type_11_7 {
            let _e146 = base;
            let _e148 = base;
            if (dot(_e146.xyz, _e148.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e152 = base;
    out_color = _e152;
    return;
}

@fragment 
fn main(@location(7) shadowData: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>) -> @location(0) vec4<f32> {
    shadowData_1 = shadowData;
    frag_tex_coord0_1 = frag_tex_coord0_;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    main_1();
    let _e9 = out_color;
    return _e9;
}
