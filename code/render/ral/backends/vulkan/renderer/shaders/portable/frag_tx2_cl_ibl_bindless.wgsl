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
@id(14) override ibl_enabled: i32 = 0i;
override override_type_3_9: bool = (ibl_enabled != 0i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_color2In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> ibl_N_1: vec3<f32>;
var<private> ibl_V_1: vec3<f32>;
@group(2) @binding(3)
var radianceCube: texture_cube<f32>;
@group(2) @binding(35)
var radianceCube_sampler: sampler;
@group(2) @binding(1)
var brdfLut: texture_2d<f32>;
@group(2) @binding(33)
var brdfLut_sampler: sampler;
@group(2) @binding(4)
var gtaoMap: texture_2d<f32>;
@group(2) @binding(36)
var gtaoMap_sampler: sampler;
var<private> out_color: vec4<f32>;
@group(2) @binding(2)
var irradianceCube: texture_cube<f32>;
@group(2) @binding(34)
var irradianceCube_sampler: sampler;

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e95 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e95 + 0.5f));
    let _e100 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e102 = fogType;
    let _e105 = fogType;
    return (((_e100 > 0.5f) && (_e102 >= 1i)) && (_e105 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e95 = wired_advanced_fog_enabled_u0028_();
    if !(_e95) {
        return 0f;
    }
    let _e98 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e98, 0.000001f));
    let _e103 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e103 + 0.5f));
    let _e106 = fogType_1;
    if (_e106 == 1i) {
        let _e110 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e110 <= 0f) {
            return 0f;
        }
        let _e112 = viewDepth;
        let _e115 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e112 / _e115), 0f, 1f);
    }
    let _e120 = unnamed.advancedFogColorDensity[3u];
    let _e122 = viewDepth;
    opticalDepth = (max(_e120, 0f) * _e122);
    let _e124 = fogType_1;
    if (_e124 == 2i) {
        let _e126 = opticalDepth;
        return clamp((1f - exp(-(_e126))), 0f, 1f);
    }
    let _e131 = opticalDepth;
    let _e132 = opticalDepth;
    return clamp((1f - exp(-((_e131 * _e132)))), 0f, 1f);
}

fn fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b(cosTheta: ptr<function, f32>, F0_: ptr<function, vec3<f32>>, roughness: ptr<function, f32>) -> vec3<f32> {
    var t: f32;
    var t2_: f32;
    var Fmax: vec3<f32>;

    let _e98 = (*cosTheta);
    t = (1f - _e98);
    let _e100 = t;
    let _e101 = t;
    t2_ = (_e100 * _e101);
    let _e103 = (*roughness);
    let _e106 = (*F0_);
    Fmax = max(vec3((1f - _e103)), _e106);
    let _e108 = (*F0_);
    let _e109 = Fmax;
    let _e110 = (*F0_);
    let _e112 = t2_;
    let _e113 = t2_;
    let _e115 = t;
    return (_e108 + ((_e109 - _e110) * ((_e112 * _e113) * _e115)));
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e96 = (*c);
    (*c) = max(_e96, vec3<f32>(0f, 0f, 0f));
    let _e98 = (*c);
    cutoff = (_e98 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e100 = (*c);
    lo = (_e100 / vec3(12.92f));
    let _e103 = (*c);
    hi = pow(((_e103 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e108 = hi;
    let _e109 = lo;
    let _e110 = cutoff;
    return mix(_e108, _e109, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e110));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e97 = (*role);
    let _e99 = (*role);
    let _e104 = unnamed.packed_indices[(_e97 / 4u)][(_e99 % 4u)];
    let _e107 = (*role);
    let _e109 = (*role);
    let _e114 = unnamed.packed_indices[(_e107 / 4u)][(_e109 % 4u)];
    let _e119 = (*uv);
    let _e120 = textureSample(wired_bindless_images[(_e104 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    c_1 = _e120;
    let _e121 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e121))) == 0i) {
        let _e126 = c_1;
        param = _e126.xyz;
        let _e128 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e128.x;
        c_1[1u] = _e128.y;
        c_1[2u] = _e128.z;
    }
    let _e135 = (*slot);
    if (lightmap_slot == (_e135 + 1i)) {
        let _e140 = unnamed.worldLightParams[0u];
        let _e141 = c_1;
        let _e143 = (_e141.xyz * _e140);
        c_1[0u] = _e143.x;
        c_1[1u] = _e143.y;
        c_1[2u] = _e143.z;
    }
    let _e150 = c_1;
    return _e150;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color2_: vec4<f32>;
    var param_3: vec3<f32>;
    var color0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var color1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color2_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_13: u32;
    var param_14: vec2<f32>;
    var param_15: i32;
    var color2_1: vec4<f32>;
    var param_16: u32;
    var param_17: vec2<f32>;
    var param_18: i32;
    var color1_2: vec4<f32>;
    var param_19: u32;
    var param_20: vec2<f32>;
    var param_21: i32;
    var color2_2: vec4<f32>;
    var param_22: u32;
    var param_23: vec2<f32>;
    var param_24: i32;
    var color1_3: vec4<f32>;
    var param_25: u32;
    var param_26: vec2<f32>;
    var param_27: i32;
    var color2_3: vec4<f32>;
    var param_28: u32;
    var param_29: vec2<f32>;
    var param_30: i32;
    var color1_4: vec4<f32>;
    var param_31: u32;
    var param_32: vec2<f32>;
    var param_33: i32;
    var color2_4: vec4<f32>;
    var param_34: u32;
    var param_35: vec2<f32>;
    var param_36: i32;
    var color1_5: vec4<f32>;
    var param_37: u32;
    var param_38: vec2<f32>;
    var param_39: i32;
    var color2_5: vec4<f32>;
    var param_40: u32;
    var param_41: vec2<f32>;
    var param_42: i32;
    var color1_6: vec4<f32>;
    var param_43: u32;
    var param_44: vec2<f32>;
    var param_45: i32;
    var color2_6: vec4<f32>;
    var param_46: u32;
    var param_47: vec2<f32>;
    var param_48: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var upward: f32;
    var snowCoverage: f32;
    var orm: vec3<f32>;
    var ao: f32;
    var roughness_1: f32;
    var metalness: f32;
    var ibl_n: vec3<f32>;
    var ibl_v: vec3<f32>;
    var F0_1: vec3<f32>;
    var NdotV: f32;
    var F_amb: vec3<f32>;
    var param_49: f32;
    var param_50: vec3<f32>;
    var param_51: f32;
    var R: vec3<f32>;
    var prefiltered: vec3<f32>;
    var envBRDF: vec2<f32>;
    var specularIBL: vec3<f32>;
    var gtaoUV: vec2<f32>;
    var gtao_vis: f32;
    var fogAmount: f32;

    let _e183 = frag_color0In_1;
    param_1 = _e183.xyz;
    let _e185 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e187 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e185.x, _e185.y, _e185.z, _e187);
    let _e192 = frag_color1In_1;
    param_2 = _e192.xyz;
    let _e194 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e196 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e194.x, _e194.y, _e194.z, _e196);
    let _e201 = frag_color2In_1;
    param_3 = _e201.xyz;
    let _e203 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e205 = frag_color2In_1[3u];
    frag_color2_ = vec4<f32>(_e203.x, _e203.y, _e203.z, _e205);
    param_4 = 0u;
    let _e210 = frag_tex_coord0_1;
    param_5 = _e210;
    param_6 = 0i;
    let _e211 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    let _e212 = frag_color0_;
    color0_ = (_e211 * _e212);
    if override_type_3_2 {
        param_7 = 1u;
        let _e214 = frag_tex_coord1_1;
        param_8 = _e214;
        param_9 = 1i;
        let _e215 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
        let _e216 = frag_color1_;
        color1_ = (_e215 * _e216);
        param_10 = 2u;
        let _e218 = frag_tex_coord2_1;
        param_11 = _e218;
        param_12 = 2i;
        let _e219 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
        let _e220 = frag_color2_;
        color2_ = (_e219 * _e220);
        let _e222 = color0_;
        let _e224 = color1_;
        let _e227 = color2_;
        let _e229 = ((_e222.xyz + _e224.xyz) + _e227.xyz);
        let _e231 = color0_[3u];
        let _e233 = color1_[3u];
        let _e236 = color2_[3u];
        base = vec4<f32>(_e229.x, _e229.y, _e229.z, ((_e231 * _e233) * _e236));
    } else {
        if override_type_3_3 {
            param_13 = 1u;
            let _e242 = frag_tex_coord1_1;
            param_14 = _e242;
            param_15 = 1i;
            let _e243 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_13), (&param_14), (&param_15));
            let _e244 = frag_color1_;
            color1_1 = (_e243 * _e244);
            param_16 = 2u;
            let _e246 = frag_tex_coord2_1;
            param_17 = _e246;
            param_18 = 2i;
            let _e247 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_16), (&param_17), (&param_18));
            let _e248 = frag_color2_;
            color2_1 = (_e247 * _e248);
            let _e251 = color0_[3u];
            let _e252 = color0_;
            color0_ = (_e252 * _e251);
            let _e255 = color1_1[3u];
            let _e256 = color1_1;
            color1_1 = (_e256 * _e255);
            let _e259 = color2_1[3u];
            let _e260 = color2_1;
            color2_1 = (_e260 * _e259);
            let _e262 = color0_;
            let _e264 = color1_1;
            let _e267 = color2_1;
            let _e269 = ((_e262.xyz + _e264.xyz) + _e267.xyz);
            let _e271 = color0_[3u];
            let _e273 = color1_1[3u];
            let _e276 = color2_1[3u];
            base = vec4<f32>(_e269.x, _e269.y, _e269.z, ((_e271 * _e273) * _e276));
        } else {
            if override_type_3_4 {
                param_19 = 1u;
                let _e282 = frag_tex_coord1_1;
                param_20 = _e282;
                param_21 = 1i;
                let _e283 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_19), (&param_20), (&param_21));
                let _e284 = frag_color1_;
                color1_2 = (_e283 * _e284);
                param_22 = 2u;
                let _e286 = frag_tex_coord2_1;
                param_23 = _e286;
                param_24 = 2i;
                let _e287 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_22), (&param_23), (&param_24));
                let _e288 = frag_color2_;
                color2_2 = (_e287 * _e288);
                let _e291 = color0_[3u];
                let _e293 = color0_;
                color0_ = (_e293 * (1f - _e291));
                let _e296 = color1_2[3u];
                let _e298 = color1_2;
                color1_2 = (_e298 * (1f - _e296));
                let _e301 = color2_2[3u];
                let _e303 = color2_2;
                color2_2 = (_e303 * (1f - _e301));
                let _e305 = color0_;
                let _e307 = color1_2;
                let _e310 = color2_2;
                let _e312 = ((_e305.xyz + _e307.xyz) + _e310.xyz);
                let _e314 = color0_[3u];
                let _e316 = color1_2[3u];
                let _e319 = color2_2[3u];
                base = vec4<f32>(_e312.x, _e312.y, _e312.z, ((_e314 * _e316) * _e319));
            } else {
                if override_type_3_5 {
                    param_25 = 1u;
                    let _e325 = frag_tex_coord1_1;
                    param_26 = _e325;
                    param_27 = 1i;
                    let _e326 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_25), (&param_26), (&param_27));
                    let _e327 = frag_color1_;
                    color1_3 = (_e326 * _e327);
                    param_28 = 2u;
                    let _e329 = frag_tex_coord2_1;
                    param_29 = _e329;
                    param_30 = 2i;
                    let _e330 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_28), (&param_29), (&param_30));
                    let _e331 = frag_color2_;
                    color2_3 = (_e330 * _e331);
                    let _e333 = color0_;
                    let _e334 = color1_3;
                    let _e336 = color1_3[3u];
                    let _e339 = color2_3;
                    let _e341 = color2_3[3u];
                    base = mix(mix(_e333, _e334, vec4(_e336)), _e339, vec4(_e341));
                } else {
                    if override_type_3_6 {
                        param_31 = 1u;
                        let _e344 = frag_tex_coord1_1;
                        param_32 = _e344;
                        param_33 = 1i;
                        let _e345 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_31), (&param_32), (&param_33));
                        let _e346 = frag_color1_;
                        color1_4 = (_e345 * _e346);
                        param_34 = 2u;
                        let _e348 = frag_tex_coord2_1;
                        param_35 = _e348;
                        param_36 = 2i;
                        let _e349 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_34), (&param_35), (&param_36));
                        let _e350 = frag_color2_;
                        color2_4 = (_e349 * _e350);
                        let _e352 = color2_4;
                        let _e353 = color1_4;
                        let _e354 = color0_;
                        let _e356 = color1_4[3u];
                        let _e360 = color2_4[3u];
                        base = mix(_e352, mix(_e353, _e354, vec4(_e356)), vec4(_e360));
                    } else {
                        if override_type_3_7 {
                            param_37 = 1u;
                            let _e363 = frag_tex_coord1_1;
                            param_38 = _e363;
                            param_39 = 1i;
                            let _e364 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_37), (&param_38), (&param_39));
                            let _e365 = frag_color1_;
                            color1_5 = (_e364 * _e365);
                            param_40 = 2u;
                            let _e367 = frag_tex_coord2_1;
                            param_41 = _e367;
                            param_42 = 2i;
                            let _e368 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_40), (&param_41), (&param_42));
                            let _e369 = frag_color2_;
                            color2_5 = (_e368 * _e369);
                            let _e371 = color2_5;
                            let _e373 = color2_5[3u];
                            let _e376 = color1_5;
                            let _e378 = color1_5[3u];
                            let _e382 = color0_;
                            base = (((_e371 + vec4(_e373)) * (_e376 + vec4(_e378))) * _e382);
                        } else {
                            param_43 = 1u;
                            let _e384 = frag_tex_coord1_1;
                            param_44 = _e384;
                            param_45 = 1i;
                            let _e385 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_43), (&param_44), (&param_45));
                            let _e386 = frag_color1_;
                            color1_6 = (_e385 * _e386);
                            param_46 = 2u;
                            let _e388 = frag_tex_coord2_1;
                            param_47 = _e388;
                            param_48 = 2i;
                            let _e389 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_46), (&param_47), (&param_48));
                            let _e390 = frag_color2_;
                            color2_6 = (_e389 * _e390);
                            let _e392 = color0_;
                            let _e394 = color1_6;
                            let _e397 = color2_6;
                            let _e399 = ((_e392.xyz * _e394.xyz) * _e397.xyz);
                            base[0u] = _e399.x;
                            base[1u] = _e399.y;
                            base[2u] = _e399.z;
                            let _e407 = color0_[3u];
                            let _e409 = color1_6[3u];
                            let _e412 = color2_6[3u];
                            base[3u] = ((_e407 * _e409) * _e412);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e417 = unnamed.worldLightParams[1u];
        wetness = clamp(_e417, 0f, 1f);
        let _e421 = unnamed.worldLightParams[2u];
        frost = clamp(_e421, 0f, 1f);
        let _e423 = base;
        luminance = dot(_e423.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e426 = wetness;
        let _e428 = base;
        let _e430 = (_e428.xyz * mix(1f, 0.82f, _e426));
        base[0u] = _e430.x;
        base[1u] = _e430.y;
        base[2u] = _e430.z;
        let _e437 = base;
        let _e439 = luminance;
        let _e441 = luminance;
        let _e443 = luminance;
        let _e445 = frost;
        let _e448 = mix(_e437.xyz, vec3<f32>((_e439 * 0.88f), (_e441 * 0.94f), _e443), vec3((_e445 * 0.55f)));
        base[0u] = _e448.x;
        base[1u] = _e448.y;
        base[2u] = _e448.z;
        let _e455 = ibl_N_1;
        upward = smoothstep(0.35f, 0.85f, normalize(_e455).z);
        let _e461 = unnamed.worldLightParams[3u];
        let _e463 = upward;
        snowCoverage = (clamp(_e461, 0f, 1f) * _e463);
        let _e465 = base;
        let _e467 = snowCoverage;
        let _e470 = mix(_e465.xyz, vec3<f32>(0.82f, 0.86f, 0.9f), vec3((_e467 * 0.82f)));
        base[0u] = _e470.x;
        base[1u] = _e470.y;
        base[2u] = _e470.z;
    }
    let _e477 = color0_;
    let _e480 = unnamed.emissionRadiance;
    let _e483 = base;
    let _e485 = (_e483.xyz + (_e477.xyz * _e480.xyz));
    base[0u] = _e485.x;
    base[1u] = _e485.y;
    base[2u] = _e485.z;
    if override_type_3_9 {
        let _e495 = unnamed.packed_indices[2i][0u];
        let _e501 = unnamed.packed_indices[2i][0u];
        let _e506 = frag_tex_coord0_1;
        let _e507 = textureSample(wired_bindless_images[(_e495 & 4095u)], wired_bindless_samplers[((_e501 >> bitcast<u32>(12i)) & 255u)], _e506);
        orm = _e507.xyz;
        let _e510 = orm[0u];
        ao = _e510;
        let _e512 = orm[1u];
        let _e515 = unnamed.worldLightParams[1u];
        let _e520 = unnamed.worldLightParams[2u];
        let _e525 = unnamed.worldLightParams[3u];
        roughness_1 = clamp((((_e512 - (0.35f * _e515)) + (0.3f * _e520)) + (0.4f * _e525)), 0.04f, 1f);
        let _e530 = orm[2u];
        metalness = _e530;
        let _e531 = ibl_N_1;
        ibl_n = normalize(_e531);
        let _e533 = ibl_V_1;
        ibl_v = normalize(_e533);
        let _e535 = base;
        let _e537 = metalness;
        F0_1 = mix(vec3<f32>(0.04f, 0.04f, 0.04f), _e535.xyz, vec3(_e537));
        let _e540 = ibl_n;
        let _e541 = ibl_v;
        NdotV = max(dot(_e540, _e541), 0f);
        let _e544 = NdotV;
        param_49 = _e544;
        let _e545 = F0_1;
        param_50 = _e545;
        let _e546 = roughness_1;
        param_51 = _e546;
        let _e547 = fresnelSchlickRoughness_u0028_f1_u003b_vf3_u003b_f1_u003b((&param_49), (&param_50), (&param_51));
        F_amb = _e547;
        let _e548 = ibl_v;
        let _e550 = ibl_n;
        R = reflect(-(_e548), _e550);
        let _e552 = R;
        let _e553 = roughness_1;
        let _e555 = textureSampleLevel(radianceCube, radianceCube_sampler, _e552, (_e553 * 5f));
        prefiltered = _e555.xyz;
        let _e557 = NdotV;
        let _e558 = roughness_1;
        let _e560 = textureSampleLevel(brdfLut, brdfLut_sampler, vec2<f32>(_e557, _e558), 0f);
        envBRDF = _e560.xy;
        let _e562 = prefiltered;
        let _e563 = F_amb;
        let _e565 = envBRDF[0u];
        let _e568 = envBRDF[1u];
        let _e572 = ao;
        specularIBL = ((_e562 * ((_e563 * _e565) + vec3(_e568))) * _e572);
        let _e574 = gl_FragCoord_1;
        let _e576 = textureDimensions(gtaoMap, 0i);
        gtaoUV = (_e574.xy / vec2<f32>(vec2<i32>(_e576)));
        let _e580 = gtaoUV;
        let _e581 = textureSample(gtaoMap, gtaoMap_sampler, _e580);
        gtao_vis = _e581.x;
        let _e583 = gtao_vis;
        let _e584 = specularIBL;
        specularIBL = (_e584 * _e583);
        let _e586 = specularIBL;
        let _e587 = base;
        let _e589 = (_e587.xyz + _e586);
        base[0u] = _e589.x;
        base[1u] = _e589.y;
        base[2u] = _e589.z;
    }
    let _e596 = wired_advanced_fog_enabled_u0028_();
    if _e596 {
        let _e597 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e597;
        let _e598 = base;
        let _e601 = unnamed.advancedFogColorDensity;
        let _e603 = fogAmount;
        let _e605 = mix(_e598.xyz, _e601.xyz, vec3(_e603));
        base[0u] = _e605.x;
        base[1u] = _e605.y;
        base[2u] = _e605.z;
    }
    if override_type_3_10 {
        let _e613 = base[3u];
        if (_e613 == 0f) {
            discard;
        }
    } else {
        if override_type_3_11 {
            let _e615 = base;
            let _e617 = base;
            if (dot(_e615.xyz, _e617.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e621 = base;
    out_color = _e621;
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(6) frag_color2In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(8) ibl_N: vec3<f32>, @location(9) ibl_V: vec3<f32>) -> @location(0) vec4<f32> {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_color2In_1 = frag_color2In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    ibl_N_1 = ibl_N;
    ibl_V_1 = ibl_V;
    main_1();
    let _e19 = out_color;
    return _e19;
}
