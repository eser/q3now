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
@id(7) override discard_mode: i32 = 0i;
override override_type_6_3: bool = (discard_mode == 1i);
override override_type_6_4: bool = (discard_mode == 2i);
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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e68 = (*sceneColor)[0u];
    let _e70 = (*sceneColor)[1u];
    let _e72 = (*sceneColor)[2u];
    let _e77 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e68, max(_e70, _e72)), 0f, 1f), 0f, 0f, clamp(_e77, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e68 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e68 + 0.5f));
    let _e73 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e75 = fogType;
    let _e78 = fogType;
    return (((_e73 > 0.5f) && (_e75 >= 1i)) && (_e78 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e68 = wired_advanced_fog_enabled_u0028_();
    if !(_e68) {
        return 0f;
    }
    let _e71 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e71, 0.000001f));
    let _e76 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e76 + 0.5f));
    let _e79 = fogType_1;
    if (_e79 == 1i) {
        let _e83 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e83 <= 0f) {
            return 0f;
        }
        let _e85 = viewDepth;
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e85 / _e88), 0f, 1f);
    }
    let _e93 = unnamed.advancedFogColorDensity[3u];
    let _e95 = viewDepth;
    opticalDepth = (max(_e93, 0f) * _e95);
    let _e97 = fogType_1;
    if (_e97 == 2i) {
        let _e99 = opticalDepth;
        return clamp((1f - exp(-(_e99))), 0f, 1f);
    }
    let _e104 = opticalDepth;
    let _e105 = opticalDepth;
    return clamp((1f - exp(-((_e104 * _e105)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e68 = (*rgb);
    let _e71 = unnamed.worldLightParams[0u];
    boosted = (_e68 * _e71);
    let _e74 = boosted[0u];
    let _e76 = boosted[1u];
    let _e78 = boosted[2u];
    peak = max(_e74, max(_e76, _e78));
    let _e81 = peak;
    if (_e81 > 1f) {
        let _e83 = peak;
        let _e84 = boosted;
        boosted = (_e84 / vec3(_e83));
    }
    let _e87 = boosted;
    return _e87;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c);
    (*c) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e71 = (*role);
    let _e73 = (*role);
    let _e78 = unnamed.packed_indices[(_e71 / 4u)][(_e73 % 4u)];
    let _e81 = (*role);
    let _e83 = (*role);
    let _e88 = unnamed.packed_indices[(_e81 / 4u)][(_e83 % 4u)];
    let _e93 = (*uv);
    let _e94 = textureSample(wired_bindless_images[(_e78 & 4095u)], wired_bindless_samplers[((_e88 >> bitcast<u32>(12i)) & 255u)], _e93);
    c_1 = _e94;
    let _e95 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e95))) == 0i) {
        let _e100 = c_1;
        param = _e100.xyz;
        let _e102 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e102.x;
        c_1[1u] = _e102.y;
        c_1[2u] = _e102.z;
    }
    let _e109 = (*slot);
    if (lightmap_slot == (_e109 + 1i)) {
        let _e112 = c_1;
        param_1 = _e112.xyz;
        let _e114 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = c_1;
    return _e121;
}

fn main_1() {
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

    param_2 = 0u;
    let _e84 = frag_tex_coord0_1;
    param_3 = _e84;
    param_4 = 0i;
    let _e85 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    sampledColor0_ = _e85;
    param_5 = 1u;
    let _e86 = frag_tex_coord1_1;
    param_6 = _e86;
    param_7 = 1i;
    let _e87 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
    sampledColor1_ = _e87;
    let _e88 = sampledColor0_;
    color0_ = _e88;
    if override_type_6_ {
        let _e89 = sampledColor1_;
        color1_ = _e89;
        let _e90 = color0_;
        let _e92 = color1_;
        let _e94 = (_e90.xyz + _e92.xyz);
        let _e96 = color0_[3u];
        let _e98 = color1_[3u];
        base = vec4<f32>(_e94.x, _e94.y, _e94.z, (_e96 * _e98));
    } else {
        if override_type_6_1 {
            let _e104 = sampledColor1_;
            color1_1 = _e104;
            let _e105 = color0_;
            let _e107 = color1_1;
            let _e109 = (_e105.xyz + _e107.xyz);
            let _e111 = color0_[3u];
            let _e113 = color1_1[3u];
            base = vec4<f32>(_e109.x, _e109.y, _e109.z, (_e111 * _e113));
        } else {
            let _e119 = sampledColor1_;
            color1_2 = _e119;
            let _e120 = color0_;
            let _e122 = color1_2;
            let _e124 = (_e120.xyz * _e122.xyz);
            base[0u] = _e124.x;
            base[1u] = _e124.y;
            base[2u] = _e124.z;
            let _e132 = color0_[3u];
            let _e134 = color1_2[3u];
            base[3u] = (_e132 * _e134);
        }
    }
    if override_type_6_2 {
        let _e139 = unnamed.worldLightParams[1u];
        wetness = clamp(_e139, 0f, 1f);
        let _e143 = unnamed.worldLightParams[2u];
        frost = clamp(_e143, 0f, 1f);
        let _e145 = base;
        luminance = dot(_e145.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e148 = wetness;
        let _e150 = base;
        let _e152 = (_e150.xyz * mix(1f, 0.82f, _e148));
        base[0u] = _e152.x;
        base[1u] = _e152.y;
        base[2u] = _e152.z;
        let _e159 = base;
        let _e161 = luminance;
        let _e163 = luminance;
        let _e165 = luminance;
        let _e167 = frost;
        let _e170 = mix(_e159.xyz, vec3<f32>((_e161 * 0.88f), (_e163 * 0.94f), _e165), vec3((_e167 * 0.55f)));
        base[0u] = _e170.x;
        base[1u] = _e170.y;
        base[2u] = _e170.z;
    }
    let _e179 = unnamed.materialFactors[0u];
    if (_e179 >= 0f) {
        let _e182 = unnamed.materialBaseColor;
        let _e183 = base;
        base = (_e183 * _e182);
    }
    let _e185 = color0_;
    let _e188 = unnamed.emissionRadiance;
    let _e191 = base;
    let _e193 = (_e191.xyz + (_e185.xyz * _e188.xyz));
    base[0u] = _e193.x;
    base[1u] = _e193.y;
    base[2u] = _e193.z;
    let _e200 = wired_advanced_fog_enabled_u0028_();
    if _e200 {
        let _e201 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e201;
        let _e202 = base;
        let _e205 = unnamed.advancedFogColorDensity;
        let _e207 = fogAmount;
        let _e209 = mix(_e202.xyz, _e205.xyz, vec3(_e207));
        base[0u] = _e209.x;
        base[1u] = _e209.y;
        base[2u] = _e209.z;
    }
    if override_type_6_3 {
        let _e217 = base[3u];
        if (_e217 == 0f) {
            discard;
        }
    } else {
        if override_type_6_4 {
            let _e219 = base;
            let _e221 = base;
            if (dot(_e219.xyz, _e221.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e225 = base;
    out_color = _e225;
    param_8 = 1f;
    let _e226 = base;
    param_9 = _e226;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_8), (&param_9));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
