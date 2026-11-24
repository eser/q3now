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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e70 = (*value);
    let _e71 = (*value);
    let _e73 = all((_e70 == _e71));
    phi_79_ = _e73;
    if _e73 {
        let _e74 = (*value);
        phi_79_ = all((abs(_e74) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e79 = phi_79_;
    return _e79;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e70 = (*value_1);
    let _e71 = (*value_1);
    let _e73 = all((_e70 == _e71));
    phi_64_ = _e73;
    if _e73 {
        let _e74 = (*value_1);
        phi_64_ = all((abs(_e74) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e79 = phi_64_;
    return _e79;
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
    let _e81 = temporalOutcome_1;
    let _e82 = (_e81 != 1u);
    phi_103_ = _e82;
    if !(_e82) {
        let _e84 = temporalCurrentClip_1;
        param = _e84;
        let _e85 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e85);
    }
    let _e88 = phi_103_;
    phi_112_ = _e88;
    if !(_e88) {
        let _e90 = temporalPreviousClip_1;
        param_1 = _e90;
        let _e91 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e91);
    }
    let _e94 = phi_112_;
    phi_122_ = _e94;
    if !(_e94) {
        let _e97 = temporalCurrentClip_1[3u];
        phi_122_ = (_e97 <= 0.000001f);
    }
    let _e100 = phi_122_;
    phi_129_ = _e100;
    if !(_e100) {
        let _e103 = temporalPreviousClip_1[3u];
        phi_129_ = (_e103 <= 0.000001f);
    }
    let _e106 = phi_129_;
    if _e106 {
        return;
    }
    let _e107 = temporalCurrentClip_1;
    let _e110 = temporalCurrentClip_1[3u];
    currentNdc = (_e107.xy / vec2(_e110));
    let _e113 = temporalPreviousClip_1;
    let _e116 = temporalPreviousClip_1[3u];
    previousNdc = (_e113.xy / vec2(_e116));
    let _e119 = currentNdc;
    param_2 = _e119;
    let _e120 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e121 = !(_e120);
    phi_158_ = _e121;
    if !(_e121) {
        let _e123 = previousNdc;
        param_3 = _e123;
        let _e124 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e124);
    }
    let _e127 = phi_158_;
    if _e127 {
        return;
    }
    let _e128 = currentNdc;
    currentUv = ((_e128 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e131 = previousNdc;
    previousUv = ((_e131 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e134 = currentUv;
    let _e135 = previousUv;
    velocity = (_e134 - _e135);
    let _e137 = velocity;
    param_4 = _e137;
    let _e138 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e138) {
        return;
    }
    let _e140 = velocity;
    out_temporal_velocity = _e140;
    let _e141 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e141, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e72 + 0.5f));
    let _e77 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e79 = fogType;
    let _e82 = fogType;
    return (((_e77 > 0.5f) && (_e79 >= 1i)) && (_e82 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e72 = wired_advanced_fog_enabled_u0028_();
    if !(_e72) {
        return 0f;
    }
    let _e75 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e75, 0.000001f));
    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e80 + 0.5f));
    let _e83 = fogType_1;
    if (_e83 == 1i) {
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e87 <= 0f) {
            return 0f;
        }
        let _e89 = viewDepth;
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e89 / _e92), 0f, 1f);
    }
    let _e97 = unnamed.advancedFogColorDensity[3u];
    let _e99 = viewDepth;
    opticalDepth = (max(_e97, 0f) * _e99);
    let _e101 = fogType_1;
    if (_e101 == 2i) {
        let _e103 = opticalDepth;
        return clamp((1f - exp(-(_e103))), 0f, 1f);
    }
    let _e108 = opticalDepth;
    let _e109 = opticalDepth;
    return clamp((1f - exp(-((_e108 * _e109)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e72 = (*rgb);
    let _e75 = unnamed.worldLightParams[0u];
    boosted = (_e72 * _e75);
    let _e78 = boosted[0u];
    let _e80 = boosted[1u];
    let _e82 = boosted[2u];
    peak = max(_e78, max(_e80, _e82));
    let _e85 = peak;
    if (_e85 > 1f) {
        let _e87 = peak;
        let _e88 = boosted;
        boosted = (_e88 / vec3(_e87));
    }
    let _e91 = boosted;
    return _e91;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e73 = (*c);
    (*c) = max(_e73, vec3<f32>(0f, 0f, 0f));
    let _e75 = (*c);
    cutoff = (_e75 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e77 = (*c);
    lo = (_e77 / vec3(12.92f));
    let _e80 = (*c);
    hi = pow(((_e80 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e85 = hi;
    let _e86 = lo;
    let _e87 = cutoff;
    return mix(_e85, _e86, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e87));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e85 = (*role);
    let _e87 = (*role);
    let _e92 = unnamed.packed_indices[(_e85 / 4u)][(_e87 % 4u)];
    let _e97 = (*uv);
    let _e98 = textureSample(wired_bindless_images[(_e82 & 4095u)], wired_bindless_samplers[((_e92 >> bitcast<u32>(12i)) & 255u)], _e97);
    c_1 = _e98;
    let _e99 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e99))) == 0i) {
        let _e104 = c_1;
        param_5 = _e104.xyz;
        let _e106 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e106.x;
        c_1[1u] = _e106.y;
        c_1[2u] = _e106.z;
    }
    let _e113 = (*slot);
    if (lightmap_slot == (_e113 + 1i)) {
        let _e116 = c_1;
        param_6 = _e116.xyz;
        let _e118 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e118.x;
        c_1[1u] = _e118.y;
        c_1[2u] = _e118.z;
    }
    let _e125 = c_1;
    return _e125;
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
    let _e88 = frag_tex_coord0_1;
    param_8 = _e88;
    param_9 = 0i;
    let _e89 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e89;
    param_10 = 1u;
    let _e90 = frag_tex_coord1_1;
    param_11 = _e90;
    param_12 = 1i;
    let _e91 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor1_ = _e91;
    let _e92 = sampledColor0_;
    color0_ = _e92;
    if override_type_3_ {
        let _e93 = sampledColor1_;
        color1_ = _e93;
        let _e94 = color0_;
        let _e96 = color1_;
        let _e98 = (_e94.xyz + _e96.xyz);
        let _e100 = color0_[3u];
        let _e102 = color1_[3u];
        base = vec4<f32>(_e98.x, _e98.y, _e98.z, (_e100 * _e102));
    } else {
        if override_type_3_1 {
            let _e108 = sampledColor1_;
            color1_1 = _e108;
            let _e109 = color0_;
            let _e111 = color1_1;
            let _e113 = (_e109.xyz + _e111.xyz);
            let _e115 = color0_[3u];
            let _e117 = color1_1[3u];
            base = vec4<f32>(_e113.x, _e113.y, _e113.z, (_e115 * _e117));
        } else {
            let _e123 = sampledColor1_;
            color1_2 = _e123;
            let _e124 = color0_;
            let _e126 = color1_2;
            let _e128 = (_e124.xyz * _e126.xyz);
            base[0u] = _e128.x;
            base[1u] = _e128.y;
            base[2u] = _e128.z;
            let _e136 = color0_[3u];
            let _e138 = color1_2[3u];
            base[3u] = (_e136 * _e138);
        }
    }
    if override_type_3_2 {
        let _e143 = unnamed.worldLightParams[1u];
        wetness = clamp(_e143, 0f, 1f);
        let _e147 = unnamed.worldLightParams[2u];
        frost = clamp(_e147, 0f, 1f);
        let _e149 = base;
        luminance = dot(_e149.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e152 = wetness;
        let _e154 = base;
        let _e156 = (_e154.xyz * mix(1f, 0.82f, _e152));
        base[0u] = _e156.x;
        base[1u] = _e156.y;
        base[2u] = _e156.z;
        let _e163 = base;
        let _e165 = luminance;
        let _e167 = luminance;
        let _e169 = luminance;
        let _e171 = frost;
        let _e174 = mix(_e163.xyz, vec3<f32>((_e165 * 0.88f), (_e167 * 0.94f), _e169), vec3((_e171 * 0.55f)));
        base[0u] = _e174.x;
        base[1u] = _e174.y;
        base[2u] = _e174.z;
    }
    let _e183 = unnamed.materialFactors[0u];
    if (_e183 >= 0f) {
        let _e186 = unnamed.materialBaseColor;
        let _e187 = base;
        base = (_e187 * _e186);
    }
    let _e189 = color0_;
    let _e192 = unnamed.emissionRadiance;
    let _e195 = base;
    let _e197 = (_e195.xyz + (_e189.xyz * _e192.xyz));
    base[0u] = _e197.x;
    base[1u] = _e197.y;
    base[2u] = _e197.z;
    let _e204 = wired_advanced_fog_enabled_u0028_();
    if _e204 {
        let _e205 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e205;
        let _e206 = base;
        let _e209 = unnamed.advancedFogColorDensity;
        let _e211 = fogAmount;
        let _e213 = mix(_e206.xyz, _e209.xyz, vec3(_e211));
        base[0u] = _e213.x;
        base[1u] = _e213.y;
        base[2u] = _e213.z;
    }
    if override_type_3_3 {
        let _e221 = base[3u];
        if (_e221 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e223 = base;
            let _e225 = base;
            if (dot(_e223.xyz, _e225.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e229 = base;
    out_color = _e229;
    param_13 = 1f;
    let _e230 = base;
    param_14 = _e230;
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
