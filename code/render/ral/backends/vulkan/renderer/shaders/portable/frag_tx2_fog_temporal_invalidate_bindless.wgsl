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
override override_type_6_2: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_6_3: bool = (acff == 1i);
override override_type_6_4: bool = (acff == 2i);
override override_type_6_5: bool = (acff == 3i);
override override_type_6_6: bool = (acff == 1i);
override override_type_6_7: bool = (acff == 2i);
override override_type_6_8: bool = (acff == 3i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e79 = (*sceneColor)[0u];
    let _e81 = (*sceneColor)[1u];
    let _e83 = (*sceneColor)[2u];
    let _e88 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e79, max(_e81, _e83)), 0f, 1f), 0f, 0f, clamp(_e88, 0f, 1f));
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
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var sampledColor1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var sampledColor2_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var color2_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color2_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color2_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_12: f32;
    var param_13: vec4<f32>;

    let _e108 = unnamed.packed_indices[0i][3u];
    let _e114 = unnamed.packed_indices[0i][3u];
    let _e119 = fog_tex_coord_1;
    let _e120 = textureSample(wired_bindless_images[(_e108 & 4095u)], wired_bindless_samplers[((_e114 >> bitcast<u32>(12i)) & 255u)], _e119);
    fog = _e120;
    let _e121 = frag_color0In_1;
    param_2 = _e121.xyz;
    let _e123 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e125 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e123.x, _e123.y, _e123.z, _e125);
    param_3 = 0u;
    let _e130 = frag_tex_coord0_1;
    param_4 = _e130;
    param_5 = 0i;
    let _e131 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e131;
    param_6 = 1u;
    let _e132 = frag_tex_coord1_1;
    param_7 = _e132;
    param_8 = 1i;
    let _e133 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e133;
    param_9 = 2u;
    let _e134 = frag_tex_coord2_1;
    param_10 = _e134;
    param_11 = 2i;
    let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor2_ = _e135;
    let _e136 = sampledColor0_;
    let _e137 = frag_color0_;
    color0_ = (_e136 * _e137);
    if override_type_6_ {
        let _e139 = sampledColor1_;
        color1_ = _e139;
        let _e140 = sampledColor2_;
        color2_ = _e140;
        let _e141 = color0_;
        let _e143 = color1_;
        let _e146 = color2_;
        let _e148 = ((_e141.xyz + _e143.xyz) + _e146.xyz);
        let _e150 = color0_[3u];
        let _e152 = color1_[3u];
        let _e155 = color2_[3u];
        base = vec4<f32>(_e148.x, _e148.y, _e148.z, ((_e150 * _e152) * _e155));
    } else {
        if override_type_6_1 {
            let _e161 = sampledColor1_;
            let _e162 = frag_color0_;
            color1_1 = (_e161 * _e162);
            let _e164 = sampledColor2_;
            let _e165 = frag_color0_;
            color2_1 = (_e164 * _e165);
            let _e167 = color0_;
            let _e169 = color1_1;
            let _e172 = color2_1;
            let _e174 = ((_e167.xyz + _e169.xyz) + _e172.xyz);
            let _e176 = color0_[3u];
            let _e178 = color1_1[3u];
            let _e181 = color2_1[3u];
            base = vec4<f32>(_e174.x, _e174.y, _e174.z, ((_e176 * _e178) * _e181));
        } else {
            let _e187 = sampledColor1_;
            color1_2 = _e187;
            let _e188 = sampledColor2_;
            color2_2 = _e188;
            let _e189 = color0_;
            let _e191 = color1_2;
            let _e194 = color2_2;
            let _e196 = ((_e189.xyz * _e191.xyz) * _e194.xyz);
            base[0u] = _e196.x;
            base[1u] = _e196.y;
            base[2u] = _e196.z;
            let _e204 = color0_[3u];
            let _e206 = color1_2[3u];
            let _e209 = color2_2[3u];
            base[3u] = ((_e204 * _e206) * _e209);
        }
    }
    if override_type_6_2 {
        let _e214 = unnamed.worldLightParams[1u];
        wetness = clamp(_e214, 0f, 1f);
        let _e218 = unnamed.worldLightParams[2u];
        frost = clamp(_e218, 0f, 1f);
        let _e220 = base;
        luminance = dot(_e220.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e223 = wetness;
        let _e225 = base;
        let _e227 = (_e225.xyz * mix(1f, 0.82f, _e223));
        base[0u] = _e227.x;
        base[1u] = _e227.y;
        base[2u] = _e227.z;
        let _e234 = base;
        let _e236 = luminance;
        let _e238 = luminance;
        let _e240 = luminance;
        let _e242 = frost;
        let _e245 = mix(_e234.xyz, vec3<f32>((_e236 * 0.88f), (_e238 * 0.94f), _e240), vec3((_e242 * 0.55f)));
        base[0u] = _e245.x;
        base[1u] = _e245.y;
        base[2u] = _e245.z;
    }
    let _e254 = unnamed.materialFactors[0u];
    if (_e254 >= 0f) {
        let _e257 = unnamed.materialBaseColor;
        let _e258 = base;
        base = (_e258 * _e257);
    }
    let _e260 = color0_;
    let _e263 = unnamed.emissionRadiance;
    let _e266 = base;
    let _e268 = (_e266.xyz + (_e260.xyz * _e263.xyz));
    base[0u] = _e268.x;
    base[1u] = _e268.y;
    base[2u] = _e268.z;
    let _e275 = wired_advanced_fog_enabled_u0028_();
    if _e275 {
        let _e276 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e276;
        if override_type_6_3 {
            let _e277 = fogAmount;
            let _e279 = base;
            let _e281 = (_e279.xyz * (1f - _e277));
            base[0u] = _e281.x;
            base[1u] = _e281.y;
            base[2u] = _e281.z;
        } else {
            if override_type_6_4 {
                let _e288 = fogAmount;
                let _e290 = base;
                base = (_e290 * (1f - _e288));
            } else {
                if override_type_6_5 {
                    let _e292 = fogAmount;
                    let _e295 = base[3u];
                    base[3u] = (_e295 * (1f - _e292));
                } else {
                    let _e298 = base;
                    let _e301 = unnamed.advancedFogColorDensity;
                    let _e303 = fogAmount;
                    let _e305 = mix(_e298.xyz, _e301.xyz, vec3(_e303));
                    base[0u] = _e305.x;
                    base[1u] = _e305.y;
                    base[2u] = _e305.z;
                }
            }
        }
    } else {
        if override_type_6_6 {
            let _e312 = base;
            let _e315 = fog[3u];
            let _e317 = (_e312.xyz * (1f - _e315));
            base[0u] = _e317.x;
            base[1u] = _e317.y;
            base[2u] = _e317.z;
        } else {
            if override_type_6_7 {
                let _e324 = base;
                let _e326 = fog[3u];
                base = (_e324 * (1f - _e326));
            } else {
                if override_type_6_8 {
                    let _e330 = base[3u];
                    let _e332 = fog[3u];
                    base[3u] = (_e330 * (1f - _e332));
                } else {
                    let _e336 = base;
                    let _e337 = fog;
                    let _e339 = unnamed.fogColor;
                    let _e342 = fog[3u];
                    base = mix(_e336, (_e337 * _e339), vec4(_e342));
                }
            }
        }
    }
    if override_type_6_9 {
        let _e346 = base[3u];
        if (_e346 == 0f) {
            discard;
        }
    } else {
        if override_type_6_10 {
            let _e348 = base;
            let _e350 = base;
            if (dot(_e348.xyz, _e350.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e354 = base;
    out_color = _e354;
    param_12 = 1f;
    let _e355 = base;
    param_13 = _e355;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_12), (&param_13));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
