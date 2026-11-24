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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> frag_tex_coord2_1: vec2<f32>;
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
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var sampledColor1_: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var sampledColor2_: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
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
    var param_17: f32;
    var param_18: vec4<f32>;

    let _e99 = frag_color0In_1;
    param_7 = _e99.xyz;
    let _e101 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e103 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e101.x, _e101.y, _e101.z, _e103);
    param_8 = 0u;
    let _e108 = frag_tex_coord0_1;
    param_9 = _e108;
    param_10 = 0i;
    let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e109;
    param_11 = 1u;
    let _e110 = frag_tex_coord1_1;
    param_12 = _e110;
    param_13 = 1i;
    let _e111 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor1_ = _e111;
    param_14 = 2u;
    let _e112 = frag_tex_coord2_1;
    param_15 = _e112;
    param_16 = 2i;
    let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
    sampledColor2_ = _e113;
    let _e114 = sampledColor0_;
    let _e115 = frag_color0_;
    color0_ = (_e114 * _e115);
    if override_type_3_ {
        let _e117 = sampledColor1_;
        color1_ = _e117;
        let _e118 = sampledColor2_;
        color2_ = _e118;
        let _e119 = color0_;
        let _e121 = color1_;
        let _e124 = color2_;
        let _e126 = ((_e119.xyz + _e121.xyz) + _e124.xyz);
        let _e128 = color0_[3u];
        let _e130 = color1_[3u];
        let _e133 = color2_[3u];
        base = vec4<f32>(_e126.x, _e126.y, _e126.z, ((_e128 * _e130) * _e133));
    } else {
        if override_type_3_1 {
            let _e139 = sampledColor1_;
            let _e140 = frag_color0_;
            color1_1 = (_e139 * _e140);
            let _e142 = sampledColor2_;
            let _e143 = frag_color0_;
            color2_1 = (_e142 * _e143);
            let _e145 = color0_;
            let _e147 = color1_1;
            let _e150 = color2_1;
            let _e152 = ((_e145.xyz + _e147.xyz) + _e150.xyz);
            let _e154 = color0_[3u];
            let _e156 = color1_1[3u];
            let _e159 = color2_1[3u];
            base = vec4<f32>(_e152.x, _e152.y, _e152.z, ((_e154 * _e156) * _e159));
        } else {
            let _e165 = sampledColor1_;
            color1_2 = _e165;
            let _e166 = sampledColor2_;
            color2_2 = _e166;
            let _e167 = color0_;
            let _e169 = color1_2;
            let _e172 = color2_2;
            let _e174 = ((_e167.xyz * _e169.xyz) * _e172.xyz);
            base[0u] = _e174.x;
            base[1u] = _e174.y;
            base[2u] = _e174.z;
            let _e182 = color0_[3u];
            let _e184 = color1_2[3u];
            let _e187 = color2_2[3u];
            base[3u] = ((_e182 * _e184) * _e187);
        }
    }
    if override_type_3_2 {
        let _e192 = unnamed.worldLightParams[1u];
        wetness = clamp(_e192, 0f, 1f);
        let _e196 = unnamed.worldLightParams[2u];
        frost = clamp(_e196, 0f, 1f);
        let _e198 = base;
        luminance = dot(_e198.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e201 = wetness;
        let _e203 = base;
        let _e205 = (_e203.xyz * mix(1f, 0.82f, _e201));
        base[0u] = _e205.x;
        base[1u] = _e205.y;
        base[2u] = _e205.z;
        let _e212 = base;
        let _e214 = luminance;
        let _e216 = luminance;
        let _e218 = luminance;
        let _e220 = frost;
        let _e223 = mix(_e212.xyz, vec3<f32>((_e214 * 0.88f), (_e216 * 0.94f), _e218), vec3((_e220 * 0.55f)));
        base[0u] = _e223.x;
        base[1u] = _e223.y;
        base[2u] = _e223.z;
    }
    let _e232 = unnamed.materialFactors[0u];
    if (_e232 >= 0f) {
        let _e235 = unnamed.materialBaseColor;
        let _e236 = base;
        base = (_e236 * _e235);
    }
    let _e238 = color0_;
    let _e241 = unnamed.emissionRadiance;
    let _e244 = base;
    let _e246 = (_e244.xyz + (_e238.xyz * _e241.xyz));
    base[0u] = _e246.x;
    base[1u] = _e246.y;
    base[2u] = _e246.z;
    let _e253 = wired_advanced_fog_enabled_u0028_();
    if _e253 {
        let _e254 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e254;
        let _e255 = base;
        let _e258 = unnamed.advancedFogColorDensity;
        let _e260 = fogAmount;
        let _e262 = mix(_e255.xyz, _e258.xyz, vec3(_e260));
        base[0u] = _e262.x;
        base[1u] = _e262.y;
        base[2u] = _e262.z;
    }
    if override_type_3_3 {
        let _e270 = base[3u];
        if (_e270 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e272 = base;
            let _e274 = base;
            if (dot(_e272.xyz, _e274.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e278 = base;
    out_color = _e278;
    param_17 = 1f;
    let _e279 = base;
    param_18 = _e279;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_17), (&param_18));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
