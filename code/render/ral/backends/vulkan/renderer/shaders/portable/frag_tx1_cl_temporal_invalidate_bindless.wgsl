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
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
override override_type_4_8: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_9: bool = (discard_mode == 1i);
override override_type_4_10: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e79 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e79 + 0.5f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e86 = fogType;
    let _e89 = fogType;
    return (((_e84 > 0.5f) && (_e86 >= 1i)) && (_e89 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e79 = wired_advanced_fog_enabled_u0028_();
    if !(_e79) {
        return 0f;
    }
    let _e82 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e82, 0.000001f));
    let _e87 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e87 + 0.5f));
    let _e90 = fogType_1;
    if (_e90 == 1i) {
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e94 <= 0f) {
            return 0f;
        }
        let _e96 = viewDepth;
        let _e99 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e96 / _e99), 0f, 1f);
    }
    let _e104 = unnamed.advancedFogColorDensity[3u];
    let _e106 = viewDepth;
    opticalDepth = (max(_e104, 0f) * _e106);
    let _e108 = fogType_1;
    if (_e108 == 2i) {
        let _e110 = opticalDepth;
        return clamp((1f - exp(-(_e110))), 0f, 1f);
    }
    let _e115 = opticalDepth;
    let _e116 = opticalDepth;
    return clamp((1f - exp(-((_e115 * _e116)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e79 = (*rgb);
    let _e82 = unnamed.worldLightParams[0u];
    boosted = (_e79 * _e82);
    let _e85 = boosted[0u];
    let _e87 = boosted[1u];
    let _e89 = boosted[2u];
    peak = max(_e85, max(_e87, _e89));
    let _e92 = peak;
    if (_e92 > 1f) {
        let _e94 = peak;
        let _e95 = boosted;
        boosted = (_e95 / vec3(_e94));
    }
    let _e98 = boosted;
    return _e98;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e80 = (*c);
    (*c) = max(_e80, vec3<f32>(0f, 0f, 0f));
    let _e82 = (*c);
    cutoff = (_e82 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e84 = (*c);
    lo = (_e84 / vec3(12.92f));
    let _e87 = (*c);
    hi = pow(((_e87 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e92 = hi;
    let _e93 = lo;
    let _e94 = cutoff;
    return mix(_e92, _e93, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e94));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e82 = (*role);
    let _e84 = (*role);
    let _e89 = unnamed.packed_indices[(_e82 / 4u)][(_e84 % 4u)];
    let _e92 = (*role);
    let _e94 = (*role);
    let _e99 = unnamed.packed_indices[(_e92 / 4u)][(_e94 % 4u)];
    let _e104 = (*uv);
    let _e105 = textureSample(wired_bindless_images[(_e89 & 4095u)], wired_bindless_samplers[((_e99 >> bitcast<u32>(12i)) & 255u)], _e104);
    c_1 = _e105;
    let _e106 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e106))) == 0i) {
        let _e111 = c_1;
        param = _e111.xyz;
        let _e113 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e113.x;
        c_1[1u] = _e113.y;
        c_1[2u] = _e113.z;
    }
    let _e120 = (*slot);
    if (lightmap_slot == (_e120 + 1i)) {
        let _e123 = c_1;
        param_1 = _e123.xyz;
        let _e125 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = c_1;
    return _e132;
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

    let _e102 = frag_color0In_1;
    param_2 = _e102.xyz;
    let _e104 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e106 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e104.x, _e104.y, _e104.z, _e106);
    let _e111 = frag_color1In_1;
    param_3 = _e111.xyz;
    let _e113 = sRGBToLinear_u0028_vf3_u003b((&param_3));
    let _e115 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e113.x, _e113.y, _e113.z, _e115);
    param_4 = 0u;
    let _e120 = frag_tex_coord0_1;
    param_5 = _e120;
    param_6 = 0i;
    let _e121 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
    sampledColor0_ = _e121;
    param_7 = 1u;
    let _e122 = frag_tex_coord1_1;
    param_8 = _e122;
    param_9 = 1i;
    let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor1_ = _e123;
    let _e124 = sampledColor0_;
    let _e125 = frag_color0_;
    color0_ = (_e124 * _e125);
    if override_type_4_2 {
        let _e127 = sampledColor1_;
        let _e128 = frag_color1_;
        color1_ = (_e127 * _e128);
        let _e130 = color0_;
        let _e132 = color1_;
        let _e134 = (_e130.xyz + _e132.xyz);
        let _e136 = color0_[3u];
        let _e138 = color1_[3u];
        base = vec4<f32>(_e134.x, _e134.y, _e134.z, (_e136 * _e138));
    } else {
        if override_type_4_3 {
            let _e144 = sampledColor1_;
            let _e145 = frag_color1_;
            color1_1 = (_e144 * _e145);
            let _e148 = color0_[3u];
            let _e149 = color0_;
            color0_ = (_e149 * _e148);
            let _e152 = color1_1[3u];
            let _e153 = color1_1;
            color1_1 = (_e153 * _e152);
            let _e155 = color0_;
            let _e157 = color1_1;
            let _e159 = (_e155.xyz + _e157.xyz);
            let _e161 = color0_[3u];
            let _e163 = color1_1[3u];
            base = vec4<f32>(_e159.x, _e159.y, _e159.z, (_e161 * _e163));
        } else {
            if override_type_4_4 {
                let _e169 = sampledColor1_;
                let _e170 = frag_color1_;
                color1_2 = (_e169 * _e170);
                let _e173 = color0_[3u];
                let _e175 = color0_;
                color0_ = (_e175 * (1f - _e173));
                let _e178 = color1_2[3u];
                let _e180 = color1_2;
                color1_2 = (_e180 * (1f - _e178));
                let _e182 = color0_;
                let _e184 = color1_2;
                let _e186 = (_e182.xyz + _e184.xyz);
                let _e188 = color0_[3u];
                let _e190 = color1_2[3u];
                base = vec4<f32>(_e186.x, _e186.y, _e186.z, (_e188 * _e190));
            } else {
                if override_type_4_5 {
                    let _e196 = sampledColor1_;
                    let _e197 = frag_color1_;
                    color1_3 = (_e196 * _e197);
                    let _e199 = color0_;
                    let _e200 = color1_3;
                    let _e202 = color1_3[3u];
                    base = mix(_e199, _e200, vec4(_e202));
                } else {
                    if override_type_4_6 {
                        let _e205 = sampledColor1_;
                        let _e206 = frag_color1_;
                        color1_4 = (_e205 * _e206);
                        let _e208 = color1_4;
                        let _e209 = color0_;
                        let _e211 = color1_4[3u];
                        base = mix(_e208, _e209, vec4(_e211));
                    } else {
                        if override_type_4_7 {
                            let _e214 = sampledColor1_;
                            let _e215 = frag_color1_;
                            color1_5 = (_e214 * _e215);
                            let _e217 = color1_5;
                            let _e219 = color1_5[3u];
                            let _e222 = color0_;
                            base = ((_e217 + vec4(_e219)) * _e222);
                        } else {
                            let _e224 = sampledColor1_;
                            let _e225 = frag_color1_;
                            color1_6 = (_e224 * _e225);
                            let _e227 = color0_;
                            let _e229 = color1_6;
                            let _e231 = (_e227.xyz * _e229.xyz);
                            base[0u] = _e231.x;
                            base[1u] = _e231.y;
                            base[2u] = _e231.z;
                            let _e239 = color0_[3u];
                            let _e241 = color1_6[3u];
                            base[3u] = (_e239 * _e241);
                        }
                    }
                }
            }
        }
    }
    if override_type_4_8 {
        let _e246 = unnamed.worldLightParams[1u];
        wetness = clamp(_e246, 0f, 1f);
        let _e250 = unnamed.worldLightParams[2u];
        frost = clamp(_e250, 0f, 1f);
        let _e252 = base;
        luminance = dot(_e252.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e255 = wetness;
        let _e257 = base;
        let _e259 = (_e257.xyz * mix(1f, 0.82f, _e255));
        base[0u] = _e259.x;
        base[1u] = _e259.y;
        base[2u] = _e259.z;
        let _e266 = base;
        let _e268 = luminance;
        let _e270 = luminance;
        let _e272 = luminance;
        let _e274 = frost;
        let _e277 = mix(_e266.xyz, vec3<f32>((_e268 * 0.88f), (_e270 * 0.94f), _e272), vec3((_e274 * 0.55f)));
        base[0u] = _e277.x;
        base[1u] = _e277.y;
        base[2u] = _e277.z;
    }
    let _e286 = unnamed.materialFactors[0u];
    if (_e286 >= 0f) {
        let _e289 = unnamed.materialBaseColor;
        let _e290 = base;
        base = (_e290 * _e289);
    }
    let _e292 = color0_;
    let _e295 = unnamed.emissionRadiance;
    let _e298 = base;
    let _e300 = (_e298.xyz + (_e292.xyz * _e295.xyz));
    base[0u] = _e300.x;
    base[1u] = _e300.y;
    base[2u] = _e300.z;
    let _e307 = wired_advanced_fog_enabled_u0028_();
    if _e307 {
        let _e308 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e308;
        let _e309 = base;
        let _e312 = unnamed.advancedFogColorDensity;
        let _e314 = fogAmount;
        let _e316 = mix(_e309.xyz, _e312.xyz, vec3(_e314));
        base[0u] = _e316.x;
        base[1u] = _e316.y;
        base[2u] = _e316.z;
    }
    if override_type_4_9 {
        let _e324 = base[3u];
        if (_e324 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e326 = base;
            let _e328 = base;
            if (dot(_e326.xyz, _e328.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e332 = base;
    out_color = _e332;
    param_10 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_10));
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
