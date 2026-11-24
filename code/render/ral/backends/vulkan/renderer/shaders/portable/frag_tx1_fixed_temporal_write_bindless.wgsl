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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
@id(6) override tex_mode: i32 = 0i;
override override_type_3_: bool = (tex_mode == 1i);
override override_type_3_1: bool = (tex_mode == 2i);
override override_type_3_2: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_3: bool = (discard_mode == 1i);
override override_type_3_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: vec4<f32>;
var<private> temporalOutcome_1: u32;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
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

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e72 = (*value);
    let _e73 = (*value);
    let _e75 = all((_e72 == _e73));
    phi_79_ = _e75;
    if _e75 {
        let _e76 = (*value);
        phi_79_ = all((abs(_e76) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e81 = phi_79_;
    return _e81;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e72 = (*value_1);
    let _e73 = (*value_1);
    let _e75 = all((_e72 == _e73));
    phi_64_ = _e75;
    if _e75 {
        let _e76 = (*value_1);
        phi_64_ = all((abs(_e76) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e81 = phi_64_;
    return _e81;
}

fn wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b(coverageConfidence: ptr<function, f32>, sceneColor: ptr<function, vec4<f32>>) {
    var param: vec4<f32>;
    var param_1: vec4<f32>;
    var currentNdc: vec2<f32>;
    var previousNdc: vec2<f32>;
    var param_2: vec2<f32>;
    var param_3: vec2<f32>;
    var currentUv: vec2<f32>;
    var previousUv: vec2<f32>;
    var velocity: vec2<f32>;
    var param_4: vec2<f32>;
    var phi_103_: bool;
    var phi_112_: bool;
    var phi_122_: bool;
    var phi_129_: bool;
    var phi_158_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = vec4<f32>(0f, 0f, 0f, 0f);
    let _e83 = temporalOutcome_1;
    let _e84 = (_e83 != 1u);
    phi_103_ = _e84;
    if !(_e84) {
        let _e86 = temporalCurrentClip_1;
        param = _e86;
        let _e87 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e87);
    }
    let _e90 = phi_103_;
    phi_112_ = _e90;
    if !(_e90) {
        let _e92 = temporalPreviousClip_1;
        param_1 = _e92;
        let _e93 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e93);
    }
    let _e96 = phi_112_;
    phi_122_ = _e96;
    if !(_e96) {
        let _e99 = temporalCurrentClip_1[3u];
        phi_122_ = (_e99 <= 0.000001f);
    }
    let _e102 = phi_122_;
    phi_129_ = _e102;
    if !(_e102) {
        let _e105 = temporalPreviousClip_1[3u];
        phi_129_ = (_e105 <= 0.000001f);
    }
    let _e108 = phi_129_;
    if _e108 {
        return;
    }
    let _e109 = temporalCurrentClip_1;
    let _e112 = temporalCurrentClip_1[3u];
    currentNdc = (_e109.xy / vec2(_e112));
    let _e115 = temporalPreviousClip_1;
    let _e118 = temporalPreviousClip_1[3u];
    previousNdc = (_e115.xy / vec2(_e118));
    let _e121 = currentNdc;
    param_2 = _e121;
    let _e122 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e123 = !(_e122);
    phi_158_ = _e123;
    if !(_e123) {
        let _e125 = previousNdc;
        param_3 = _e125;
        let _e126 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e126);
    }
    let _e129 = phi_158_;
    if _e129 {
        return;
    }
    let _e130 = currentNdc;
    currentUv = ((_e130 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e133 = previousNdc;
    previousUv = ((_e133 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e136 = currentUv;
    let _e137 = previousUv;
    velocity = (_e136 - _e137);
    let _e139 = velocity;
    param_4 = _e139;
    let _e140 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e140) {
        return;
    }
    let _e142 = velocity;
    out_temporal_velocity = _e142;
    let _e143 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e143, 0f, 1f), 0f, 0f, 1f);
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
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

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
        param_5 = _e106.xyz;
        let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e108.x;
        c_1[1u] = _e108.y;
        c_1[2u] = _e108.z;
    }
    let _e115 = (*slot);
    if (lightmap_slot == (_e115 + 1i)) {
        let _e118 = c_1;
        param_6 = _e118.xyz;
        let _e120 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e120.x;
        c_1[1u] = _e120.y;
        c_1[2u] = _e120.z;
    }
    let _e127 = c_1;
    return _e127;
}

fn main_1() {
    var sampledColor0_: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var sampledColor1_: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var frag_color: vec4<f32>;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_13: f32;
    var param_14: vec4<f32>;

    param_7 = 0u;
    let _e91 = frag_tex_coord0_1;
    param_8 = _e91;
    param_9 = 0i;
    let _e92 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e92;
    param_10 = 1u;
    let _e93 = frag_tex_coord1_1;
    param_11 = _e93;
    param_12 = 1i;
    let _e94 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor1_ = _e94;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    let _e99 = sampledColor0_;
    let _e100 = frag_color;
    color0_ = (_e99 * _e100);
    if override_type_3_ {
        let _e102 = sampledColor1_;
        color1_ = _e102;
        let _e103 = color0_;
        let _e105 = color1_;
        let _e107 = (_e103.xyz + _e105.xyz);
        let _e109 = color0_[3u];
        let _e111 = color1_[3u];
        base = vec4<f32>(_e107.x, _e107.y, _e107.z, (_e109 * _e111));
    } else {
        if override_type_3_1 {
            let _e117 = sampledColor1_;
            let _e118 = frag_color;
            color1_1 = (_e117 * _e118);
            let _e120 = color0_;
            let _e122 = color1_1;
            let _e124 = (_e120.xyz + _e122.xyz);
            let _e126 = color0_[3u];
            let _e128 = color1_1[3u];
            base = vec4<f32>(_e124.x, _e124.y, _e124.z, (_e126 * _e128));
        } else {
            let _e134 = sampledColor1_;
            let _e135 = frag_color;
            color1_2 = (_e134 * _e135);
            let _e137 = color0_;
            let _e139 = color1_2;
            let _e141 = (_e137.xyz * _e139.xyz);
            base[0u] = _e141.x;
            base[1u] = _e141.y;
            base[2u] = _e141.z;
            let _e149 = color0_[3u];
            let _e151 = color1_2[3u];
            base[3u] = (_e149 * _e151);
        }
    }
    if override_type_3_2 {
        let _e156 = unnamed.worldLightParams[1u];
        wetness = clamp(_e156, 0f, 1f);
        let _e160 = unnamed.worldLightParams[2u];
        frost = clamp(_e160, 0f, 1f);
        let _e162 = base;
        luminance = dot(_e162.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e165 = wetness;
        let _e167 = base;
        let _e169 = (_e167.xyz * mix(1f, 0.82f, _e165));
        base[0u] = _e169.x;
        base[1u] = _e169.y;
        base[2u] = _e169.z;
        let _e176 = base;
        let _e178 = luminance;
        let _e180 = luminance;
        let _e182 = luminance;
        let _e184 = frost;
        let _e187 = mix(_e176.xyz, vec3<f32>((_e178 * 0.88f), (_e180 * 0.94f), _e182), vec3((_e184 * 0.55f)));
        base[0u] = _e187.x;
        base[1u] = _e187.y;
        base[2u] = _e187.z;
    }
    let _e196 = unnamed.materialFactors[0u];
    if (_e196 >= 0f) {
        let _e199 = unnamed.materialBaseColor;
        let _e200 = base;
        base = (_e200 * _e199);
    }
    let _e202 = color0_;
    let _e205 = unnamed.emissionRadiance;
    let _e208 = base;
    let _e210 = (_e208.xyz + (_e202.xyz * _e205.xyz));
    base[0u] = _e210.x;
    base[1u] = _e210.y;
    base[2u] = _e210.z;
    let _e217 = wired_advanced_fog_enabled_u0028_();
    if _e217 {
        let _e218 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e218;
        let _e219 = base;
        let _e222 = unnamed.advancedFogColorDensity;
        let _e224 = fogAmount;
        let _e226 = mix(_e219.xyz, _e222.xyz, vec3(_e224));
        base[0u] = _e226.x;
        base[1u] = _e226.y;
        base[2u] = _e226.z;
    }
    if override_type_3_3 {
        let _e234 = base[3u];
        if (_e234 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e236 = base;
            let _e238 = base;
            if (dot(_e236.xyz, _e238.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e242 = base;
    out_color = _e242;
    param_13 = 1f;
    let _e243 = base;
    param_14 = _e243;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_13), (&param_14));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e15 = out_temporal_velocity;
    let _e16 = out_temporal_validity;
    let _e17 = out_color;
    return FragmentOutput(_e15, _e16, _e17);
}
