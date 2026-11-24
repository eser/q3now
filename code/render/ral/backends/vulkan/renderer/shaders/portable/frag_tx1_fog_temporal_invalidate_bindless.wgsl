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
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e78 = (*sceneColor)[0u];
    let _e80 = (*sceneColor)[1u];
    let _e82 = (*sceneColor)[2u];
    let _e87 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e78, max(_e80, _e82)), 0f, 1f), 0f, 0f, clamp(_e87, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e78 + 0.5f));
    let _e83 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e85 = fogType;
    let _e88 = fogType;
    return (((_e83 > 0.5f) && (_e85 >= 1i)) && (_e88 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e78 = wired_advanced_fog_enabled_u0028_();
    if !(_e78) {
        return 0f;
    }
    let _e81 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e81, 0.000001f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e86 + 0.5f));
    let _e89 = fogType_1;
    if (_e89 == 1i) {
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e93 <= 0f) {
            return 0f;
        }
        let _e95 = viewDepth;
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e95 / _e98), 0f, 1f);
    }
    let _e103 = unnamed.advancedFogColorDensity[3u];
    let _e105 = viewDepth;
    opticalDepth = (max(_e103, 0f) * _e105);
    let _e107 = fogType_1;
    if (_e107 == 2i) {
        let _e109 = opticalDepth;
        return clamp((1f - exp(-(_e109))), 0f, 1f);
    }
    let _e114 = opticalDepth;
    let _e115 = opticalDepth;
    return clamp((1f - exp(-((_e114 * _e115)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e78 = (*rgb);
    let _e81 = unnamed.worldLightParams[0u];
    boosted = (_e78 * _e81);
    let _e84 = boosted[0u];
    let _e86 = boosted[1u];
    let _e88 = boosted[2u];
    peak = max(_e84, max(_e86, _e88));
    let _e91 = peak;
    if (_e91 > 1f) {
        let _e93 = peak;
        let _e94 = boosted;
        boosted = (_e94 / vec3(_e93));
    }
    let _e97 = boosted;
    return _e97;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e79 = (*c);
    (*c) = max(_e79, vec3<f32>(0f, 0f, 0f));
    let _e81 = (*c);
    cutoff = (_e81 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e83 = (*c);
    lo = (_e83 / vec3(12.92f));
    let _e86 = (*c);
    hi = pow(((_e86 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e91 = hi;
    let _e92 = lo;
    let _e93 = cutoff;
    return mix(_e91, _e92, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e93));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e91 = (*role);
    let _e93 = (*role);
    let _e98 = unnamed.packed_indices[(_e91 / 4u)][(_e93 % 4u)];
    let _e103 = (*uv);
    let _e104 = textureSample(wired_bindless_images[(_e88 & 4095u)], wired_bindless_samplers[((_e98 >> bitcast<u32>(12i)) & 255u)], _e103);
    c_1 = _e104;
    let _e105 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e105))) == 0i) {
        let _e110 = c_1;
        param = _e110.xyz;
        let _e112 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = (*slot);
    if (lightmap_slot == (_e119 + 1i)) {
        let _e122 = c_1;
        param_1 = _e122.xyz;
        let _e124 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e124.x;
        c_1[1u] = _e124.y;
        c_1[2u] = _e124.z;
    }
    let _e131 = c_1;
    return _e131;
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
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_9: f32;
    var param_10: vec4<f32>;

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
    sampledColor0_ = _e123;
    param_6 = 1u;
    let _e124 = frag_tex_coord1_1;
    param_7 = _e124;
    param_8 = 1i;
    let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e125;
    let _e126 = sampledColor0_;
    let _e127 = frag_color0_;
    color0_ = (_e126 * _e127);
    if override_type_6_ {
        let _e129 = sampledColor1_;
        color1_ = _e129;
        let _e130 = color0_;
        let _e132 = color1_;
        let _e134 = (_e130.xyz + _e132.xyz);
        let _e136 = color0_[3u];
        let _e138 = color1_[3u];
        base = vec4<f32>(_e134.x, _e134.y, _e134.z, (_e136 * _e138));
    } else {
        if override_type_6_1 {
            let _e144 = sampledColor1_;
            let _e145 = frag_color0_;
            color1_1 = (_e144 * _e145);
            let _e147 = color0_;
            let _e149 = color1_1;
            let _e151 = (_e147.xyz + _e149.xyz);
            let _e153 = color0_[3u];
            let _e155 = color1_1[3u];
            base = vec4<f32>(_e151.x, _e151.y, _e151.z, (_e153 * _e155));
        } else {
            let _e161 = sampledColor1_;
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
    if override_type_6_2 {
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
    let _e221 = unnamed.materialFactors[0u];
    if (_e221 >= 0f) {
        let _e224 = unnamed.materialBaseColor;
        let _e225 = base;
        base = (_e225 * _e224);
    }
    let _e227 = color0_;
    let _e230 = unnamed.emissionRadiance;
    let _e233 = base;
    let _e235 = (_e233.xyz + (_e227.xyz * _e230.xyz));
    base[0u] = _e235.x;
    base[1u] = _e235.y;
    base[2u] = _e235.z;
    let _e242 = wired_advanced_fog_enabled_u0028_();
    if _e242 {
        let _e243 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e243;
        if override_type_6_3 {
            let _e244 = fogAmount;
            let _e246 = base;
            let _e248 = (_e246.xyz * (1f - _e244));
            base[0u] = _e248.x;
            base[1u] = _e248.y;
            base[2u] = _e248.z;
        } else {
            if override_type_6_4 {
                let _e255 = fogAmount;
                let _e257 = base;
                base = (_e257 * (1f - _e255));
            } else {
                if override_type_6_5 {
                    let _e259 = fogAmount;
                    let _e262 = base[3u];
                    base[3u] = (_e262 * (1f - _e259));
                } else {
                    let _e265 = base;
                    let _e268 = unnamed.advancedFogColorDensity;
                    let _e270 = fogAmount;
                    let _e272 = mix(_e265.xyz, _e268.xyz, vec3(_e270));
                    base[0u] = _e272.x;
                    base[1u] = _e272.y;
                    base[2u] = _e272.z;
                }
            }
        }
    } else {
        if override_type_6_6 {
            let _e279 = base;
            let _e282 = fog[3u];
            let _e284 = (_e279.xyz * (1f - _e282));
            base[0u] = _e284.x;
            base[1u] = _e284.y;
            base[2u] = _e284.z;
        } else {
            if override_type_6_7 {
                let _e291 = base;
                let _e293 = fog[3u];
                base = (_e291 * (1f - _e293));
            } else {
                if override_type_6_8 {
                    let _e297 = base[3u];
                    let _e299 = fog[3u];
                    base[3u] = (_e297 * (1f - _e299));
                } else {
                    let _e303 = base;
                    let _e304 = fog;
                    let _e306 = unnamed.fogColor;
                    let _e309 = fog[3u];
                    base = mix(_e303, (_e304 * _e306), vec4(_e309));
                }
            }
        }
    }
    if override_type_6_9 {
        let _e313 = base[3u];
        if (_e313 == 0f) {
            discard;
        }
    } else {
        if override_type_6_10 {
            let _e315 = base;
            let _e317 = base;
            if (dot(_e315.xyz, _e317.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e321 = base;
    out_color = _e321;
    param_9 = 1f;
    let _e322 = base;
    param_10 = _e322;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_9), (&param_10));
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
