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

    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e74 + 0.5f));
    let _e79 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e81 = fogType;
    let _e84 = fogType;
    return (((_e79 > 0.5f) && (_e81 >= 1i)) && (_e84 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e74 = wired_advanced_fog_enabled_u0028_();
    if !(_e74) {
        return 0f;
    }
    let _e77 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e77, 0.000001f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e82 + 0.5f));
    let _e85 = fogType_1;
    if (_e85 == 1i) {
        let _e89 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e89 <= 0f) {
            return 0f;
        }
        let _e91 = viewDepth;
        let _e94 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e91 / _e94), 0f, 1f);
    }
    let _e99 = unnamed.advancedFogColorDensity[3u];
    let _e101 = viewDepth;
    opticalDepth = (max(_e99, 0f) * _e101);
    let _e103 = fogType_1;
    if (_e103 == 2i) {
        let _e105 = opticalDepth;
        return clamp((1f - exp(-(_e105))), 0f, 1f);
    }
    let _e110 = opticalDepth;
    let _e111 = opticalDepth;
    return clamp((1f - exp(-((_e110 * _e111)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e74 = (*rgb);
    let _e77 = unnamed.worldLightParams[0u];
    boosted = (_e74 * _e77);
    let _e80 = boosted[0u];
    let _e82 = boosted[1u];
    let _e84 = boosted[2u];
    peak = max(_e80, max(_e82, _e84));
    let _e87 = peak;
    if (_e87 > 1f) {
        let _e89 = peak;
        let _e90 = boosted;
        boosted = (_e90 / vec3(_e89));
    }
    let _e93 = boosted;
    return _e93;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e75 = (*c);
    (*c) = max(_e75, vec3<f32>(0f, 0f, 0f));
    let _e77 = (*c);
    cutoff = (_e77 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e79 = (*c);
    lo = (_e79 / vec3(12.92f));
    let _e82 = (*c);
    hi = pow(((_e82 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e87 = hi;
    let _e88 = lo;
    let _e89 = cutoff;
    return mix(_e87, _e88, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e89));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e77 = (*role);
    let _e79 = (*role);
    let _e84 = unnamed.packed_indices[(_e77 / 4u)][(_e79 % 4u)];
    let _e87 = (*role);
    let _e89 = (*role);
    let _e94 = unnamed.packed_indices[(_e87 / 4u)][(_e89 % 4u)];
    let _e99 = (*uv);
    let _e100 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e94 >> bitcast<u32>(12i)) & 255u)], _e99);
    c_1 = _e100;
    let _e101 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e101))) == 0i) {
        let _e106 = c_1;
        param = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e118 = c_1;
        param_1 = _e118.xyz;
        let _e120 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = c_1;
    return _e127;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var color1_2: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_14: f32;

    let _e97 = unnamed.packed_indices[0i][3u];
    let _e103 = unnamed.packed_indices[0i][3u];
    let _e108 = fog_tex_coord_1;
    let _e109 = textureSample(wired_bindless_images[(_e97 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    fog = _e109;
    param_2 = 0u;
    let _e110 = frag_tex_coord0_1;
    param_3 = _e110;
    param_4 = 0i;
    let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    color0_ = _e111;
    if override_type_4_ {
        param_5 = 1u;
        let _e112 = frag_tex_coord1_1;
        param_6 = _e112;
        param_7 = 1i;
        let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e113;
        let _e114 = color0_;
        let _e116 = color1_;
        let _e118 = (_e114.xyz + _e116.xyz);
        let _e120 = color0_[3u];
        let _e122 = color1_[3u];
        base = vec4<f32>(_e118.x, _e118.y, _e118.z, (_e120 * _e122));
    } else {
        if override_type_4_1 {
            param_8 = 1u;
            let _e128 = frag_tex_coord1_1;
            param_9 = _e128;
            param_10 = 1i;
            let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
            color1_1 = _e129;
            let _e130 = color0_;
            let _e132 = color1_1;
            let _e134 = (_e130.xyz + _e132.xyz);
            let _e136 = color0_[3u];
            let _e138 = color1_1[3u];
            base = vec4<f32>(_e134.x, _e134.y, _e134.z, (_e136 * _e138));
        } else {
            param_11 = 1u;
            let _e144 = frag_tex_coord1_1;
            param_12 = _e144;
            param_13 = 1i;
            let _e145 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            color1_2 = _e145;
            let _e146 = color0_;
            let _e148 = color1_2;
            let _e150 = (_e146.xyz * _e148.xyz);
            base[0u] = _e150.x;
            base[1u] = _e150.y;
            base[2u] = _e150.z;
            let _e158 = color0_[3u];
            let _e160 = color1_2[3u];
            base[3u] = (_e158 * _e160);
        }
    }
    if override_type_4_2 {
        let _e165 = unnamed.worldLightParams[1u];
        wetness = clamp(_e165, 0f, 1f);
        let _e169 = unnamed.worldLightParams[2u];
        frost = clamp(_e169, 0f, 1f);
        let _e171 = base;
        luminance = dot(_e171.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e174 = wetness;
        let _e176 = base;
        let _e178 = (_e176.xyz * mix(1f, 0.82f, _e174));
        base[0u] = _e178.x;
        base[1u] = _e178.y;
        base[2u] = _e178.z;
        let _e185 = base;
        let _e187 = luminance;
        let _e189 = luminance;
        let _e191 = luminance;
        let _e193 = frost;
        let _e196 = mix(_e185.xyz, vec3<f32>((_e187 * 0.88f), (_e189 * 0.94f), _e191), vec3((_e193 * 0.55f)));
        base[0u] = _e196.x;
        base[1u] = _e196.y;
        base[2u] = _e196.z;
    }
    let _e203 = color0_;
    let _e206 = unnamed.emissionRadiance;
    let _e209 = base;
    let _e211 = (_e209.xyz + (_e203.xyz * _e206.xyz));
    base[0u] = _e211.x;
    base[1u] = _e211.y;
    base[2u] = _e211.z;
    let _e218 = wired_advanced_fog_enabled_u0028_();
    if _e218 {
        let _e219 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e219;
        if override_type_4_3 {
            let _e220 = fogAmount;
            let _e222 = base;
            let _e224 = (_e222.xyz * (1f - _e220));
            base[0u] = _e224.x;
            base[1u] = _e224.y;
            base[2u] = _e224.z;
        } else {
            if override_type_4_4 {
                let _e231 = fogAmount;
                let _e233 = base;
                base = (_e233 * (1f - _e231));
            } else {
                if override_type_4_5 {
                    let _e235 = fogAmount;
                    let _e238 = base[3u];
                    base[3u] = (_e238 * (1f - _e235));
                } else {
                    let _e241 = base;
                    let _e244 = unnamed.advancedFogColorDensity;
                    let _e246 = fogAmount;
                    let _e248 = mix(_e241.xyz, _e244.xyz, vec3(_e246));
                    base[0u] = _e248.x;
                    base[1u] = _e248.y;
                    base[2u] = _e248.z;
                }
            }
        }
    } else {
        if override_type_4_6 {
            let _e255 = base;
            let _e258 = fog[3u];
            let _e260 = (_e255.xyz * (1f - _e258));
            base[0u] = _e260.x;
            base[1u] = _e260.y;
            base[2u] = _e260.z;
        } else {
            if override_type_4_7 {
                let _e267 = base;
                let _e269 = fog[3u];
                base = (_e267 * (1f - _e269));
            } else {
                if override_type_4_8 {
                    let _e273 = base[3u];
                    let _e275 = fog[3u];
                    base[3u] = (_e273 * (1f - _e275));
                } else {
                    let _e279 = base;
                    let _e280 = fog;
                    let _e282 = unnamed.fogColor;
                    let _e285 = fog[3u];
                    base = mix(_e279, (_e280 * _e282), vec4(_e285));
                }
            }
        }
    }
    if override_type_4_9 {
        let _e289 = base[3u];
        if (_e289 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e291 = base;
            let _e293 = base;
            if (dot(_e291.xyz, _e293.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e297 = base;
    out_color = _e297;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
