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
override override_type_4_2: bool = (lightmap_slot != 0i);
@id(10) override acff: i32 = 0i;
override override_type_4_3: bool = (acff == 1i);
override override_type_4_4: bool = (acff == 2i);
override override_type_4_5: bool = (acff == 3i);
override override_type_4_6: bool = (acff == 1i);
override override_type_4_7: bool = (acff == 2i);
override override_type_4_8: bool = (acff == 3i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
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

    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e76 + 0.5f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e83 = fogType;
    let _e86 = fogType;
    return (((_e81 > 0.5f) && (_e83 >= 1i)) && (_e86 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e76 = wired_advanced_fog_enabled_u0028_();
    if !(_e76) {
        return 0f;
    }
    let _e79 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e79, 0.000001f));
    let _e84 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e84 + 0.5f));
    let _e87 = fogType_1;
    if (_e87 == 1i) {
        let _e91 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e91 <= 0f) {
            return 0f;
        }
        let _e93 = viewDepth;
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e93 / _e96), 0f, 1f);
    }
    let _e101 = unnamed.advancedFogColorDensity[3u];
    let _e103 = viewDepth;
    opticalDepth = (max(_e101, 0f) * _e103);
    let _e105 = fogType_1;
    if (_e105 == 2i) {
        let _e107 = opticalDepth;
        return clamp((1f - exp(-(_e107))), 0f, 1f);
    }
    let _e112 = opticalDepth;
    let _e113 = opticalDepth;
    return clamp((1f - exp(-((_e112 * _e113)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e76 = (*rgb);
    let _e79 = unnamed.worldLightParams[0u];
    boosted = (_e76 * _e79);
    let _e82 = boosted[0u];
    let _e84 = boosted[1u];
    let _e86 = boosted[2u];
    peak = max(_e82, max(_e84, _e86));
    let _e89 = peak;
    if (_e89 > 1f) {
        let _e91 = peak;
        let _e92 = boosted;
        boosted = (_e92 / vec3(_e91));
    }
    let _e95 = boosted;
    return _e95;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e77 = (*c);
    (*c) = max(_e77, vec3<f32>(0f, 0f, 0f));
    let _e79 = (*c);
    cutoff = (_e79 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e81 = (*c);
    lo = (_e81 / vec3(12.92f));
    let _e84 = (*c);
    hi = pow(((_e84 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e89 = hi;
    let _e90 = lo;
    let _e91 = cutoff;
    return mix(_e89, _e90, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e91));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e101 = (*uv);
    let _e102 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    c_1 = _e102;
    let _e103 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e103))) == 0i) {
        let _e108 = c_1;
        param = _e108.xyz;
        let _e110 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = (*slot);
    if (lightmap_slot == (_e117 + 1i)) {
        let _e120 = c_1;
        param_1 = _e120.xyz;
        let _e122 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e122.x;
        c_1[1u] = _e122.y;
        c_1[2u] = _e122.z;
    }
    let _e129 = c_1;
    return _e129;
}

fn main_1() {
    var fog: vec4<f32>;
    var frag_color0_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var color2_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color2_1: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_2: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color2_2: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_24: f32;

    let _e113 = unnamed.packed_indices[0i][3u];
    let _e119 = unnamed.packed_indices[0i][3u];
    let _e124 = fog_tex_coord_1;
    let _e125 = textureSample(wired_bindless_images[(_e113 & 4095u)], wired_bindless_samplers[((_e119 >> bitcast<u32>(12i)) & 255u)], _e124);
    fog = _e125;
    let _e126 = frag_color0In_1;
    param_2 = _e126.xyz;
    let _e128 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e130 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e128.x, _e128.y, _e128.z, _e130);
    param_3 = 0u;
    let _e135 = frag_tex_coord0_1;
    param_4 = _e135;
    param_5 = 0i;
    let _e136 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e137 = frag_color0_;
    color0_ = (_e136 * _e137);
    if override_type_4_ {
        param_6 = 1u;
        let _e139 = frag_tex_coord1_1;
        param_7 = _e139;
        param_8 = 1i;
        let _e140 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        color1_ = _e140;
        param_9 = 2u;
        let _e141 = frag_tex_coord2_1;
        param_10 = _e141;
        param_11 = 2i;
        let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
        color2_ = _e142;
        let _e143 = color0_;
        let _e145 = color1_;
        let _e148 = color2_;
        let _e150 = ((_e143.xyz + _e145.xyz) + _e148.xyz);
        let _e152 = color0_[3u];
        let _e154 = color1_[3u];
        let _e157 = color2_[3u];
        base = vec4<f32>(_e150.x, _e150.y, _e150.z, ((_e152 * _e154) * _e157));
    } else {
        if override_type_4_1 {
            param_12 = 1u;
            let _e163 = frag_tex_coord1_1;
            param_13 = _e163;
            param_14 = 1i;
            let _e164 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            let _e165 = frag_color0_;
            color1_1 = (_e164 * _e165);
            param_15 = 2u;
            let _e167 = frag_tex_coord2_1;
            param_16 = _e167;
            param_17 = 2i;
            let _e168 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
            let _e169 = frag_color0_;
            color2_1 = (_e168 * _e169);
            let _e171 = color0_;
            let _e173 = color1_1;
            let _e176 = color2_1;
            let _e178 = ((_e171.xyz + _e173.xyz) + _e176.xyz);
            let _e180 = color0_[3u];
            let _e182 = color1_1[3u];
            let _e185 = color2_1[3u];
            base = vec4<f32>(_e178.x, _e178.y, _e178.z, ((_e180 * _e182) * _e185));
        } else {
            param_18 = 1u;
            let _e191 = frag_tex_coord1_1;
            param_19 = _e191;
            param_20 = 1i;
            let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
            color1_2 = _e192;
            param_21 = 2u;
            let _e193 = frag_tex_coord2_1;
            param_22 = _e193;
            param_23 = 2i;
            let _e194 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
            color2_2 = _e194;
            let _e195 = color0_;
            let _e197 = color1_2;
            let _e200 = color2_2;
            let _e202 = ((_e195.xyz * _e197.xyz) * _e200.xyz);
            base[0u] = _e202.x;
            base[1u] = _e202.y;
            base[2u] = _e202.z;
            let _e210 = color0_[3u];
            let _e212 = color1_2[3u];
            let _e215 = color2_2[3u];
            base[3u] = ((_e210 * _e212) * _e215);
        }
    }
    if override_type_4_2 {
        let _e220 = unnamed.worldLightParams[1u];
        wetness = clamp(_e220, 0f, 1f);
        let _e224 = unnamed.worldLightParams[2u];
        frost = clamp(_e224, 0f, 1f);
        let _e226 = base;
        luminance = dot(_e226.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e229 = wetness;
        let _e231 = base;
        let _e233 = (_e231.xyz * mix(1f, 0.82f, _e229));
        base[0u] = _e233.x;
        base[1u] = _e233.y;
        base[2u] = _e233.z;
        let _e240 = base;
        let _e242 = luminance;
        let _e244 = luminance;
        let _e246 = luminance;
        let _e248 = frost;
        let _e251 = mix(_e240.xyz, vec3<f32>((_e242 * 0.88f), (_e244 * 0.94f), _e246), vec3((_e248 * 0.55f)));
        base[0u] = _e251.x;
        base[1u] = _e251.y;
        base[2u] = _e251.z;
    }
    let _e258 = color0_;
    let _e261 = unnamed.emissionRadiance;
    let _e264 = base;
    let _e266 = (_e264.xyz + (_e258.xyz * _e261.xyz));
    base[0u] = _e266.x;
    base[1u] = _e266.y;
    base[2u] = _e266.z;
    let _e273 = wired_advanced_fog_enabled_u0028_();
    if _e273 {
        let _e274 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e274;
        if override_type_4_3 {
            let _e275 = fogAmount;
            let _e277 = base;
            let _e279 = (_e277.xyz * (1f - _e275));
            base[0u] = _e279.x;
            base[1u] = _e279.y;
            base[2u] = _e279.z;
        } else {
            if override_type_4_4 {
                let _e286 = fogAmount;
                let _e288 = base;
                base = (_e288 * (1f - _e286));
            } else {
                if override_type_4_5 {
                    let _e290 = fogAmount;
                    let _e293 = base[3u];
                    base[3u] = (_e293 * (1f - _e290));
                } else {
                    let _e296 = base;
                    let _e299 = unnamed.advancedFogColorDensity;
                    let _e301 = fogAmount;
                    let _e303 = mix(_e296.xyz, _e299.xyz, vec3(_e301));
                    base[0u] = _e303.x;
                    base[1u] = _e303.y;
                    base[2u] = _e303.z;
                }
            }
        }
    } else {
        if override_type_4_6 {
            let _e310 = base;
            let _e313 = fog[3u];
            let _e315 = (_e310.xyz * (1f - _e313));
            base[0u] = _e315.x;
            base[1u] = _e315.y;
            base[2u] = _e315.z;
        } else {
            if override_type_4_7 {
                let _e322 = base;
                let _e324 = fog[3u];
                base = (_e322 * (1f - _e324));
            } else {
                if override_type_4_8 {
                    let _e328 = base[3u];
                    let _e330 = fog[3u];
                    base[3u] = (_e328 * (1f - _e330));
                } else {
                    let _e334 = base;
                    let _e335 = fog;
                    let _e337 = unnamed.fogColor;
                    let _e340 = fog[3u];
                    base = mix(_e334, (_e335 * _e337), vec4(_e340));
                }
            }
        }
    }
    if override_type_4_9 {
        let _e344 = base[3u];
        if (_e344 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e346 = base;
            let _e348 = base;
            if (dot(_e346.xyz, _e348.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e352 = base;
    out_color = _e352;
    param_24 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_24));
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
