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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e77 = (*sceneColor)[0u];
    let _e79 = (*sceneColor)[1u];
    let _e81 = (*sceneColor)[2u];
    let _e86 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e77, max(_e79, _e81)), 0f, 1f), 0f, 0f, clamp(_e86, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e77 + 0.5f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e84 = fogType;
    let _e87 = fogType;
    return (((_e82 > 0.5f) && (_e84 >= 1i)) && (_e87 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e77 = wired_advanced_fog_enabled_u0028_();
    if !(_e77) {
        return 0f;
    }
    let _e80 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e80, 0.000001f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e85 + 0.5f));
    let _e88 = fogType_1;
    if (_e88 == 1i) {
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e92 <= 0f) {
            return 0f;
        }
        let _e94 = viewDepth;
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e94 / _e97), 0f, 1f);
    }
    let _e102 = unnamed.advancedFogColorDensity[3u];
    let _e104 = viewDepth;
    opticalDepth = (max(_e102, 0f) * _e104);
    let _e106 = fogType_1;
    if (_e106 == 2i) {
        let _e108 = opticalDepth;
        return clamp((1f - exp(-(_e108))), 0f, 1f);
    }
    let _e113 = opticalDepth;
    let _e114 = opticalDepth;
    return clamp((1f - exp(-((_e113 * _e114)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e77 = (*rgb);
    let _e80 = unnamed.worldLightParams[0u];
    boosted = (_e77 * _e80);
    let _e83 = boosted[0u];
    let _e85 = boosted[1u];
    let _e87 = boosted[2u];
    peak = max(_e83, max(_e85, _e87));
    let _e90 = peak;
    if (_e90 > 1f) {
        let _e92 = peak;
        let _e93 = boosted;
        boosted = (_e93 / vec3(_e92));
    }
    let _e96 = boosted;
    return _e96;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e78 = (*c);
    (*c) = max(_e78, vec3<f32>(0f, 0f, 0f));
    let _e80 = (*c);
    cutoff = (_e80 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e82 = (*c);
    lo = (_e82 / vec3(12.92f));
    let _e85 = (*c);
    hi = pow(((_e85 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e90 = hi;
    let _e91 = lo;
    let _e92 = cutoff;
    return mix(_e90, _e91, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e92));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e90 = (*role);
    let _e92 = (*role);
    let _e97 = unnamed.packed_indices[(_e90 / 4u)][(_e92 % 4u)];
    let _e102 = (*uv);
    let _e103 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e97 >> bitcast<u32>(12i)) & 255u)], _e102);
    c_1 = _e103;
    let _e104 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e104))) == 0i) {
        let _e109 = c_1;
        param = _e109.xyz;
        let _e111 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = (*slot);
    if (lightmap_slot == (_e118 + 1i)) {
        let _e121 = c_1;
        param_1 = _e121.xyz;
        let _e123 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e123.x;
        c_1[1u] = _e123.y;
        c_1[2u] = _e123.z;
    }
    let _e130 = c_1;
    return _e130;
}

fn main_1() {
    var fog: vec4<f32>;
    var sampledColor0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var sampledColor1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_8: f32;
    var param_9: vec4<f32>;

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
    sampledColor0_ = _e111;
    param_5 = 1u;
    let _e112 = frag_tex_coord1_1;
    param_6 = _e112;
    param_7 = 1i;
    let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e113;
    let _e114 = sampledColor0_;
    color0_ = _e114;
    if override_type_6_ {
        let _e115 = sampledColor1_;
        color1_ = _e115;
        let _e116 = color0_;
        let _e118 = color1_;
        let _e120 = (_e116.xyz + _e118.xyz);
        let _e122 = color0_[3u];
        let _e124 = color1_[3u];
        base = vec4<f32>(_e120.x, _e120.y, _e120.z, (_e122 * _e124));
    } else {
        if override_type_6_1 {
            let _e130 = sampledColor1_;
            color1_1 = _e130;
            let _e131 = color0_;
            let _e133 = color1_1;
            let _e135 = (_e131.xyz + _e133.xyz);
            let _e137 = color0_[3u];
            let _e139 = color1_1[3u];
            base = vec4<f32>(_e135.x, _e135.y, _e135.z, (_e137 * _e139));
        } else {
            let _e145 = sampledColor1_;
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
    if override_type_6_2 {
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
    let _e205 = unnamed.materialFactors[0u];
    if (_e205 >= 0f) {
        let _e208 = unnamed.materialBaseColor;
        let _e209 = base;
        base = (_e209 * _e208);
    }
    let _e211 = color0_;
    let _e214 = unnamed.emissionRadiance;
    let _e217 = base;
    let _e219 = (_e217.xyz + (_e211.xyz * _e214.xyz));
    base[0u] = _e219.x;
    base[1u] = _e219.y;
    base[2u] = _e219.z;
    let _e226 = wired_advanced_fog_enabled_u0028_();
    if _e226 {
        let _e227 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e227;
        if override_type_6_3 {
            let _e228 = fogAmount;
            let _e230 = base;
            let _e232 = (_e230.xyz * (1f - _e228));
            base[0u] = _e232.x;
            base[1u] = _e232.y;
            base[2u] = _e232.z;
        } else {
            if override_type_6_4 {
                let _e239 = fogAmount;
                let _e241 = base;
                base = (_e241 * (1f - _e239));
            } else {
                if override_type_6_5 {
                    let _e243 = fogAmount;
                    let _e246 = base[3u];
                    base[3u] = (_e246 * (1f - _e243));
                } else {
                    let _e249 = base;
                    let _e252 = unnamed.advancedFogColorDensity;
                    let _e254 = fogAmount;
                    let _e256 = mix(_e249.xyz, _e252.xyz, vec3(_e254));
                    base[0u] = _e256.x;
                    base[1u] = _e256.y;
                    base[2u] = _e256.z;
                }
            }
        }
    } else {
        if override_type_6_6 {
            let _e263 = base;
            let _e266 = fog[3u];
            let _e268 = (_e263.xyz * (1f - _e266));
            base[0u] = _e268.x;
            base[1u] = _e268.y;
            base[2u] = _e268.z;
        } else {
            if override_type_6_7 {
                let _e275 = base;
                let _e277 = fog[3u];
                base = (_e275 * (1f - _e277));
            } else {
                if override_type_6_8 {
                    let _e281 = base[3u];
                    let _e283 = fog[3u];
                    base[3u] = (_e281 * (1f - _e283));
                } else {
                    let _e287 = base;
                    let _e288 = fog;
                    let _e290 = unnamed.fogColor;
                    let _e293 = fog[3u];
                    base = mix(_e287, (_e288 * _e290), vec4(_e293));
                }
            }
        }
    }
    if override_type_6_9 {
        let _e297 = base[3u];
        if (_e297 == 0f) {
            discard;
        }
    } else {
        if override_type_6_10 {
            let _e299 = base;
            let _e301 = base;
            if (dot(_e299.xyz, _e301.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e305 = base;
    out_color = _e305;
    param_8 = 1f;
    let _e306 = base;
    param_9 = _e306;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_8), (&param_9));
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
