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
    @location(2) member_1: vec4<f32>,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_6_: bool = (tex_mode == 1i);
override override_type_6_1: bool = (tex_mode == 2i);
override override_type_6_2: bool = (override_type_6_ || override_type_6_1);
override override_type_6_3: bool = (tex_mode == 3i);
override override_type_6_4: bool = (tex_mode == 4i);
override override_type_6_5: bool = (tex_mode == 5i);
override override_type_6_6: bool = (tex_mode == 6i);
override override_type_6_7: bool = (tex_mode == 7i);
override override_type_6_8: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_6_9: bool = (discard_mode == 1i);
override override_type_6_10: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
@group(0) @binding(0)
var<uniform> unnamed: UBO;
var<private> gl_FragCoord_1: vec4<f32>;
@group(1) @binding(0)
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(1) @binding(1)
var wired_bindless_samplers: binding_array<sampler>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e80 = (*sceneColor)[0u];
    let _e82 = (*sceneColor)[1u];
    let _e84 = (*sceneColor)[2u];
    let _e89 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e80, max(_e82, _e84)), 0f, 1f), 0f, 0f, clamp(_e89, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e80 + 0.5f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e87 = fogType;
    let _e90 = fogType;
    return (((_e85 > 0.5f) && (_e87 >= 1i)) && (_e90 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e80 = wired_advanced_fog_enabled_u0028_();
    if !(_e80) {
        return 0f;
    }
    let _e83 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e83, 0.000001f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e88 + 0.5f));
    let _e91 = fogType_1;
    if (_e91 == 1i) {
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e95 <= 0f) {
            return 0f;
        }
        let _e97 = viewDepth;
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e97 / _e100), 0f, 1f);
    }
    let _e105 = unnamed.advancedFogColorDensity[3u];
    let _e107 = viewDepth;
    opticalDepth = (max(_e105, 0f) * _e107);
    let _e109 = fogType_1;
    if (_e109 == 2i) {
        let _e111 = opticalDepth;
        return clamp((1f - exp(-(_e111))), 0f, 1f);
    }
    let _e116 = opticalDepth;
    let _e117 = opticalDepth;
    return clamp((1f - exp(-((_e116 * _e117)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e80 = (*rgb);
    let _e83 = unnamed.worldLightParams[0u];
    boosted = (_e80 * _e83);
    let _e86 = boosted[0u];
    let _e88 = boosted[1u];
    let _e90 = boosted[2u];
    peak = max(_e86, max(_e88, _e90));
    let _e93 = peak;
    if (_e93 > 1f) {
        let _e95 = peak;
        let _e96 = boosted;
        boosted = (_e96 / vec3(_e95));
    }
    let _e99 = boosted;
    return _e99;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e81 = (*c);
    (*c) = max(_e81, vec3<f32>(0f, 0f, 0f));
    let _e83 = (*c);
    cutoff = (_e83 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e85 = (*c);
    lo = (_e85 / vec3(12.92f));
    let _e88 = (*c);
    hi = pow(((_e88 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e93 = hi;
    let _e94 = lo;
    let _e95 = cutoff;
    return mix(_e93, _e94, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e95));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e105 = (*uv);
    let _e106 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    c_1 = _e106;
    let _e107 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e107))) == 0i) {
        let _e112 = c_1;
        param = _e112.xyz;
        let _e114 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = (*slot);
    if (lightmap_slot == (_e121 + 1i)) {
        let _e124 = c_1;
        param_1 = _e124.xyz;
        let _e126 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e126.x;
        c_1[1u] = _e126.y;
        c_1[2u] = _e126.z;
    }
    let _e133 = c_1;
    return _e133;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_3: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var sampledColor1_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_10: f32;
    var param_11: vec4<f32>;

    let _e104 = frag_color0In_1;
    param_2 = _e104.xyz;
    let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e108 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e106.x, _e106.y, _e106.z, _e108);
    let _e113 = frag_color1In_1;
    param_3 = _e113.xyz;
    let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e117 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e115.x, _e115.y, _e115.z, _e117);
    param_4 = 0u;
    let _e122 = frag_tex_coord0_1;
    param_5 = _e122;
    param_6 = 0i;
    let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    sampledColor0_ = _e123;
    param_7 = 1u;
    let _e124 = frag_tex_coord1_1;
    param_8 = _e124;
    param_9 = 1i;
    let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor1_ = _e125;
    let _e126 = sampledColor0_;
    let _e127 = frag_color0_;
    color0_ = (_e126 * _e127);
    if override_type_6_2 {
        let _e129 = sampledColor1_;
        let _e130 = frag_color1_;
        color1_ = (_e129 * _e130);
        let _e132 = color0_;
        let _e134 = color1_;
        let _e136 = (_e132.xyz + _e134.xyz);
        let _e138 = color0_[3u];
        let _e140 = color1_[3u];
        base = vec4<f32>(_e136.x, _e136.y, _e136.z, (_e138 * _e140));
    } else {
        if override_type_6_3 {
            let _e146 = sampledColor1_;
            let _e147 = frag_color1_;
            color1_1 = (_e146 * _e147);
            let _e150 = color0_[3u];
            let _e151 = color0_;
            color0_ = (_e151 * _e150);
            let _e154 = color1_1[3u];
            let _e155 = color1_1;
            color1_1 = (_e155 * _e154);
            let _e157 = color0_;
            let _e159 = color1_1;
            let _e161 = (_e157.xyz + _e159.xyz);
            let _e163 = color0_[3u];
            let _e165 = color1_1[3u];
            base = vec4<f32>(_e161.x, _e161.y, _e161.z, (_e163 * _e165));
        } else {
            if override_type_6_4 {
                let _e171 = sampledColor1_;
                let _e172 = frag_color1_;
                color1_2 = (_e171 * _e172);
                let _e175 = color0_[3u];
                let _e177 = color0_;
                color0_ = (_e177 * (1f - _e175));
                let _e180 = color1_2[3u];
                let _e182 = color1_2;
                color1_2 = (_e182 * (1f - _e180));
                let _e184 = color0_;
                let _e186 = color1_2;
                let _e188 = (_e184.xyz + _e186.xyz);
                let _e190 = color0_[3u];
                let _e192 = color1_2[3u];
                base = vec4<f32>(_e188.x, _e188.y, _e188.z, (_e190 * _e192));
            } else {
                if override_type_6_5 {
                    let _e198 = sampledColor1_;
                    let _e199 = frag_color1_;
                    color1_3 = (_e198 * _e199);
                    let _e201 = color0_;
                    let _e202 = color1_3;
                    let _e204 = color1_3[3u];
                    base = mix(_e201, _e202, vec4(_e204));
                } else {
                    if override_type_6_6 {
                        let _e207 = sampledColor1_;
                        let _e208 = frag_color1_;
                        color1_4 = (_e207 * _e208);
                        let _e210 = color1_4;
                        let _e211 = color0_;
                        let _e213 = color1_4[3u];
                        base = mix(_e210, _e211, vec4(_e213));
                    } else {
                        if override_type_6_7 {
                            let _e216 = sampledColor1_;
                            let _e217 = frag_color1_;
                            color1_5 = (_e216 * _e217);
                            let _e219 = color1_5;
                            let _e221 = color1_5[3u];
                            let _e224 = color0_;
                            base = ((_e219 + vec4(_e221)) * _e224);
                        } else {
                            let _e226 = sampledColor1_;
                            let _e227 = frag_color1_;
                            color1_6 = (_e226 * _e227);
                            let _e229 = color0_;
                            let _e231 = color1_6;
                            let _e233 = (_e229.xyz * _e231.xyz);
                            base[0u] = _e233.x;
                            base[1u] = _e233.y;
                            base[2u] = _e233.z;
                            let _e241 = color0_[3u];
                            let _e243 = color1_6[3u];
                            base[3u] = (_e241 * _e243);
                        }
                    }
                }
            }
        }
    }
    if override_type_6_8 {
        let _e248 = unnamed.worldLightParams[1u];
        wetness = clamp(_e248, 0f, 1f);
        let _e252 = unnamed.worldLightParams[2u];
        frost = clamp(_e252, 0f, 1f);
        let _e254 = base;
        luminance = dot(_e254.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e257 = wetness;
        let _e259 = base;
        let _e261 = (_e259.xyz * mix(1f, 0.82f, _e257));
        base[0u] = _e261.x;
        base[1u] = _e261.y;
        base[2u] = _e261.z;
        let _e268 = base;
        let _e270 = luminance;
        let _e272 = luminance;
        let _e274 = luminance;
        let _e276 = frost;
        let _e279 = mix(_e268.xyz, vec3<f32>((_e270 * 0.88f), (_e272 * 0.94f), _e274), vec3((_e276 * 0.55f)));
        base[0u] = _e279.x;
        base[1u] = _e279.y;
        base[2u] = _e279.z;
    }
    let _e288 = unnamed.materialFactors[0u];
    if (_e288 >= 0f) {
        let _e291 = unnamed.materialBaseColor;
        let _e292 = base;
        base = (_e292 * _e291);
    }
    let _e294 = color0_;
    let _e297 = unnamed.emissionRadiance;
    let _e300 = base;
    let _e302 = (_e300.xyz + (_e294.xyz * _e297.xyz));
    base[0u] = _e302.x;
    base[1u] = _e302.y;
    base[2u] = _e302.z;
    let _e309 = wired_advanced_fog_enabled_u0028_();
    if _e309 {
        let _e310 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e310;
        let _e311 = base;
        let _e314 = unnamed.advancedFogColorDensity;
        let _e316 = fogAmount;
        let _e318 = mix(_e311.xyz, _e314.xyz, vec3(_e316));
        base[0u] = _e318.x;
        base[1u] = _e318.y;
        base[2u] = _e318.z;
    }
    if override_type_6_9 {
        let _e326 = base[3u];
        if (_e326 == 0f) {
            discard;
        }
    } else {
        if override_type_6_10 {
            let _e328 = base;
            let _e330 = base;
            if (dot(_e328.xyz, _e330.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e334 = base;
    out_color = _e334;
    param_10 = 1f;
    let _e335 = base;
    param_11 = _e335;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_10), (&param_11));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
