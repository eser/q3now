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
    let _e70 = (*sceneColor)[0u];
    let _e72 = (*sceneColor)[1u];
    let _e74 = (*sceneColor)[2u];
    let _e79 = (*sceneColor)[3u];
    out_temporal_validity = vec4<f32>(clamp(max(_e70, max(_e72, _e74)), 0f, 1f), 0f, 0f, clamp(_e79, 0f, 1f));
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e70 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e70 + 0.5f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e77 = fogType;
    let _e80 = fogType;
    return (((_e75 > 0.5f) && (_e77 >= 1i)) && (_e80 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e70 = wired_advanced_fog_enabled_u0028_();
    if !(_e70) {
        return 0f;
    }
    let _e73 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e73, 0.000001f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e78 + 0.5f));
    let _e81 = fogType_1;
    if (_e81 == 1i) {
        let _e85 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e85 <= 0f) {
            return 0f;
        }
        let _e87 = viewDepth;
        let _e90 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e87 / _e90), 0f, 1f);
    }
    let _e95 = unnamed.advancedFogColorDensity[3u];
    let _e97 = viewDepth;
    opticalDepth = (max(_e95, 0f) * _e97);
    let _e99 = fogType_1;
    if (_e99 == 2i) {
        let _e101 = opticalDepth;
        return clamp((1f - exp(-(_e101))), 0f, 1f);
    }
    let _e106 = opticalDepth;
    let _e107 = opticalDepth;
    return clamp((1f - exp(-((_e106 * _e107)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e70 = (*rgb);
    let _e73 = unnamed.worldLightParams[0u];
    boosted = (_e70 * _e73);
    let _e76 = boosted[0u];
    let _e78 = boosted[1u];
    let _e80 = boosted[2u];
    peak = max(_e76, max(_e78, _e80));
    let _e83 = peak;
    if (_e83 > 1f) {
        let _e85 = peak;
        let _e86 = boosted;
        boosted = (_e86 / vec3(_e85));
    }
    let _e89 = boosted;
    return _e89;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e71 = (*c);
    (*c) = max(_e71, vec3<f32>(0f, 0f, 0f));
    let _e73 = (*c);
    cutoff = (_e73 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e75 = (*c);
    lo = (_e75 / vec3(12.92f));
    let _e78 = (*c);
    hi = pow(((_e78 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e83 = hi;
    let _e84 = lo;
    let _e85 = cutoff;
    return mix(_e83, _e84, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e85));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;
    var param_1: vec3<f32>;

    let _e73 = (*role);
    let _e75 = (*role);
    let _e80 = unnamed.packed_indices[(_e73 / 4u)][(_e75 % 4u)];
    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e95 = (*uv);
    let _e96 = textureSample(wired_bindless_images[(_e80 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    c_1 = _e96;
    let _e97 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e97))) == 0i) {
        let _e102 = c_1;
        param = _e102.xyz;
        let _e104 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e104.x;
        c_1[1u] = _e104.y;
        c_1[2u] = _e104.z;
    }
    let _e111 = (*slot);
    if (lightmap_slot == (_e111 + 1i)) {
        let _e114 = c_1;
        param_1 = _e114.xyz;
        let _e116 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_1));
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = c_1;
    return _e123;
}

fn main_1() {
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

    let _e95 = frag_color0In_1;
    param_2 = _e95.xyz;
    let _e97 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e99 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e97.x, _e97.y, _e97.z, _e99);
    param_3 = 0u;
    let _e104 = frag_tex_coord0_1;
    param_4 = _e104;
    param_5 = 0i;
    let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    sampledColor0_ = _e105;
    param_6 = 1u;
    let _e106 = frag_tex_coord1_1;
    param_7 = _e106;
    param_8 = 1i;
    let _e107 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
    sampledColor1_ = _e107;
    param_9 = 2u;
    let _e108 = frag_tex_coord2_1;
    param_10 = _e108;
    param_11 = 2i;
    let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor2_ = _e109;
    let _e110 = sampledColor0_;
    let _e111 = frag_color0_;
    color0_ = (_e110 * _e111);
    if override_type_6_ {
        let _e113 = sampledColor1_;
        color1_ = _e113;
        let _e114 = sampledColor2_;
        color2_ = _e114;
        let _e115 = color0_;
        let _e117 = color1_;
        let _e120 = color2_;
        let _e122 = ((_e115.xyz + _e117.xyz) + _e120.xyz);
        let _e124 = color0_[3u];
        let _e126 = color1_[3u];
        let _e129 = color2_[3u];
        base = vec4<f32>(_e122.x, _e122.y, _e122.z, ((_e124 * _e126) * _e129));
    } else {
        if override_type_6_1 {
            let _e135 = sampledColor1_;
            let _e136 = frag_color0_;
            color1_1 = (_e135 * _e136);
            let _e138 = sampledColor2_;
            let _e139 = frag_color0_;
            color2_1 = (_e138 * _e139);
            let _e141 = color0_;
            let _e143 = color1_1;
            let _e146 = color2_1;
            let _e148 = ((_e141.xyz + _e143.xyz) + _e146.xyz);
            let _e150 = color0_[3u];
            let _e152 = color1_1[3u];
            let _e155 = color2_1[3u];
            base = vec4<f32>(_e148.x, _e148.y, _e148.z, ((_e150 * _e152) * _e155));
        } else {
            let _e161 = sampledColor1_;
            color1_2 = _e161;
            let _e162 = sampledColor2_;
            color2_2 = _e162;
            let _e163 = color0_;
            let _e165 = color1_2;
            let _e168 = color2_2;
            let _e170 = ((_e163.xyz * _e165.xyz) * _e168.xyz);
            base[0u] = _e170.x;
            base[1u] = _e170.y;
            base[2u] = _e170.z;
            let _e178 = color0_[3u];
            let _e180 = color1_2[3u];
            let _e183 = color2_2[3u];
            base[3u] = ((_e178 * _e180) * _e183);
        }
    }
    if override_type_6_2 {
        let _e188 = unnamed.worldLightParams[1u];
        wetness = clamp(_e188, 0f, 1f);
        let _e192 = unnamed.worldLightParams[2u];
        frost = clamp(_e192, 0f, 1f);
        let _e194 = base;
        luminance = dot(_e194.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e197 = wetness;
        let _e199 = base;
        let _e201 = (_e199.xyz * mix(1f, 0.82f, _e197));
        base[0u] = _e201.x;
        base[1u] = _e201.y;
        base[2u] = _e201.z;
        let _e208 = base;
        let _e210 = luminance;
        let _e212 = luminance;
        let _e214 = luminance;
        let _e216 = frost;
        let _e219 = mix(_e208.xyz, vec3<f32>((_e210 * 0.88f), (_e212 * 0.94f), _e214), vec3((_e216 * 0.55f)));
        base[0u] = _e219.x;
        base[1u] = _e219.y;
        base[2u] = _e219.z;
    }
    let _e228 = unnamed.materialFactors[0u];
    if (_e228 >= 0f) {
        let _e231 = unnamed.materialBaseColor;
        let _e232 = base;
        base = (_e232 * _e231);
    }
    let _e234 = color0_;
    let _e237 = unnamed.emissionRadiance;
    let _e240 = base;
    let _e242 = (_e240.xyz + (_e234.xyz * _e237.xyz));
    base[0u] = _e242.x;
    base[1u] = _e242.y;
    base[2u] = _e242.z;
    let _e249 = wired_advanced_fog_enabled_u0028_();
    if _e249 {
        let _e250 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e250;
        let _e251 = base;
        let _e254 = unnamed.advancedFogColorDensity;
        let _e256 = fogAmount;
        let _e258 = mix(_e251.xyz, _e254.xyz, vec3(_e256));
        base[0u] = _e258.x;
        base[1u] = _e258.y;
        base[2u] = _e258.z;
    }
    if override_type_6_3 {
        let _e266 = base[3u];
        if (_e266 == 0f) {
            discard;
        }
    } else {
        if override_type_6_4 {
            let _e268 = base;
            let _e270 = base;
            if (dot(_e268.xyz, _e270.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e274 = base;
    out_color = _e274;
    param_12 = 1f;
    let _e275 = base;
    param_13 = _e275;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_12), (&param_13));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
