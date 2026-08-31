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

    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e75 + 0.5f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e82 = fogType;
    let _e85 = fogType;
    return (((_e80 > 0.5f) && (_e82 >= 1i)) && (_e85 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e75 = wired_advanced_fog_enabled_u0028_();
    if !(_e75) {
        return 0f;
    }
    let _e78 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e78, 0.000001f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e83 + 0.5f));
    let _e86 = fogType_1;
    if (_e86 == 1i) {
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e90 <= 0f) {
            return 0f;
        }
        let _e92 = viewDepth;
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e92 / _e95), 0f, 1f);
    }
    let _e100 = unnamed.advancedFogColorDensity[3u];
    let _e102 = viewDepth;
    opticalDepth = (max(_e100, 0f) * _e102);
    let _e104 = fogType_1;
    if (_e104 == 2i) {
        let _e106 = opticalDepth;
        return clamp((1f - exp(-(_e106))), 0f, 1f);
    }
    let _e111 = opticalDepth;
    let _e112 = opticalDepth;
    return clamp((1f - exp(-((_e111 * _e112)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e75 = (*rgb);
    let _e78 = unnamed.worldLightParams[0u];
    boosted = (_e75 * _e78);
    let _e81 = boosted[0u];
    let _e83 = boosted[1u];
    let _e85 = boosted[2u];
    peak = max(_e81, max(_e83, _e85));
    let _e88 = peak;
    if (_e88 > 1f) {
        let _e90 = peak;
        let _e91 = boosted;
        boosted = (_e91 / vec3(_e90));
    }
    let _e94 = boosted;
    return _e94;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e76 = (*c);
    (*c) = max(_e76, vec3<f32>(0f, 0f, 0f));
    let _e78 = (*c);
    cutoff = (_e78 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e80 = (*c);
    lo = (_e80 / vec3(12.92f));
    let _e83 = (*c);
    hi = pow(((_e83 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e88 = hi;
    let _e89 = lo;
    let _e90 = cutoff;
    return mix(_e88, _e89, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e90));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e88 = (*role);
    let _e90 = (*role);
    let _e95 = unnamed.packed_indices[(_e88 / 4u)][(_e90 % 4u)];
    let _e100 = (*uv);
    let _e101 = textureSample(wired_bindless_images[(_e85 & 4095u)], wired_bindless_samplers[((_e95 >> bitcast<u32>(12i)) & 255u)], _e100);
    c_1 = _e101;
    let _e102 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e102))) == 0i) {
        let _e107 = c_1;
        param = _e107.xyz;
        let _e109 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e109.x;
        c_1[1u] = _e109.y;
        c_1[2u] = _e109.z;
    }
    let _e116 = (*slot);
    if (lightmap_slot == (_e116 + 1i)) {
        let _e119 = c_1;
        param_1 = _e119.xyz;
        let _e121 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e121.x;
        c_1[1u] = _e121.y;
        c_1[2u] = _e121.z;
    }
    let _e128 = c_1;
    return _e128;
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
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_2: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_15: f32;

    let _e100 = unnamed.packed_indices[0i][3u];
    let _e106 = unnamed.packed_indices[0i][3u];
    let _e111 = fog_tex_coord_1;
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    fog = _e112;
    let _e113 = frag_color0In_1;
    param_2 = _e113.xyz;
    let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e117 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e115.x, _e115.y, _e115.z, _e117);
    param_3 = 0u;
    let _e122 = frag_tex_coord0_1;
    param_4 = _e122;
    param_5 = 0i;
    let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e124 = frag_color0_;
    color0_ = (_e123 * _e124);
    if override_type_4_ {
        param_6 = 1u;
        let _e126 = frag_tex_coord1_1;
        param_7 = _e126;
        param_8 = 1i;
        let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        color1_ = _e127;
        let _e128 = color0_;
        let _e130 = color1_;
        let _e132 = (_e128.xyz + _e130.xyz);
        let _e134 = color0_[3u];
        let _e136 = color1_[3u];
        base = vec4<f32>(_e132.x, _e132.y, _e132.z, (_e134 * _e136));
    } else {
        if override_type_4_1 {
            param_9 = 1u;
            let _e142 = frag_tex_coord1_1;
            param_10 = _e142;
            param_11 = 1i;
            let _e143 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e144 = frag_color0_;
            color1_1 = (_e143 * _e144);
            let _e146 = color0_;
            let _e148 = color1_1;
            let _e150 = (_e146.xyz + _e148.xyz);
            let _e152 = color0_[3u];
            let _e154 = color1_1[3u];
            base = vec4<f32>(_e150.x, _e150.y, _e150.z, (_e152 * _e154));
        } else {
            param_12 = 1u;
            let _e160 = frag_tex_coord1_1;
            param_13 = _e160;
            param_14 = 1i;
            let _e161 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
            color1_2 = _e161;
            let _e162 = color0_;
            let _e164 = color1_2;
            let _e166 = (_e162.xyz * _e164.xyz);
            base[0u] = _e166.x;
            base[1u] = _e166.y;
            base[2u] = _e166.z;
            let _e174 = color0_[3u];
            let _e176 = color1_2[3u];
            base[3u] = (_e174 * _e176);
        }
    }
    if override_type_4_2 {
        let _e181 = unnamed.worldLightParams[1u];
        wetness = clamp(_e181, 0f, 1f);
        let _e185 = unnamed.worldLightParams[2u];
        frost = clamp(_e185, 0f, 1f);
        let _e187 = base;
        luminance = dot(_e187.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e190 = wetness;
        let _e192 = base;
        let _e194 = (_e192.xyz * mix(1f, 0.82f, _e190));
        base[0u] = _e194.x;
        base[1u] = _e194.y;
        base[2u] = _e194.z;
        let _e201 = base;
        let _e203 = luminance;
        let _e205 = luminance;
        let _e207 = luminance;
        let _e209 = frost;
        let _e212 = mix(_e201.xyz, vec3<f32>((_e203 * 0.88f), (_e205 * 0.94f), _e207), vec3((_e209 * 0.55f)));
        base[0u] = _e212.x;
        base[1u] = _e212.y;
        base[2u] = _e212.z;
    }
    let _e219 = color0_;
    let _e222 = unnamed.emissionRadiance;
    let _e225 = base;
    let _e227 = (_e225.xyz + (_e219.xyz * _e222.xyz));
    base[0u] = _e227.x;
    base[1u] = _e227.y;
    base[2u] = _e227.z;
    let _e234 = wired_advanced_fog_enabled_u0028_();
    if _e234 {
        let _e235 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e235;
        if override_type_4_3 {
            let _e236 = fogAmount;
            let _e238 = base;
            let _e240 = (_e238.xyz * (1f - _e236));
            base[0u] = _e240.x;
            base[1u] = _e240.y;
            base[2u] = _e240.z;
        } else {
            if override_type_4_4 {
                let _e247 = fogAmount;
                let _e249 = base;
                base = (_e249 * (1f - _e247));
            } else {
                if override_type_4_5 {
                    let _e251 = fogAmount;
                    let _e254 = base[3u];
                    base[3u] = (_e254 * (1f - _e251));
                } else {
                    let _e257 = base;
                    let _e260 = unnamed.advancedFogColorDensity;
                    let _e262 = fogAmount;
                    let _e264 = mix(_e257.xyz, _e260.xyz, vec3(_e262));
                    base[0u] = _e264.x;
                    base[1u] = _e264.y;
                    base[2u] = _e264.z;
                }
            }
        }
    } else {
        if override_type_4_6 {
            let _e271 = base;
            let _e274 = fog[3u];
            let _e276 = (_e271.xyz * (1f - _e274));
            base[0u] = _e276.x;
            base[1u] = _e276.y;
            base[2u] = _e276.z;
        } else {
            if override_type_4_7 {
                let _e283 = base;
                let _e285 = fog[3u];
                base = (_e283 * (1f - _e285));
            } else {
                if override_type_4_8 {
                    let _e289 = base[3u];
                    let _e291 = fog[3u];
                    base[3u] = (_e289 * (1f - _e291));
                } else {
                    let _e295 = base;
                    let _e296 = fog;
                    let _e298 = unnamed.fogColor;
                    let _e301 = fog[3u];
                    base = mix(_e295, (_e296 * _e298), vec4(_e301));
                }
            }
        }
    }
    if override_type_4_9 {
        let _e305 = base[3u];
        if (_e305 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e307 = base;
            let _e309 = base;
            if (dot(_e307.xyz, _e309.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e313 = base;
    out_color = _e313;
    param_15 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_15));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
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
