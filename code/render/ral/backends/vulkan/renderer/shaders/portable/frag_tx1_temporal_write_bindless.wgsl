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
    @location(2) member_1: f32,
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
var<private> out_temporal_validity: f32;
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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e70 = (*value);
    let _e71 = (*value);
    let _e73 = all((_e70 == _e71));
    phi_78_ = _e73;
    if _e73 {
        let _e74 = (*value);
        phi_78_ = all((abs(_e74) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e79 = phi_78_;
    return _e79;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e70 = (*value_1);
    let _e71 = (*value_1);
    let _e73 = all((_e70 == _e71));
    phi_63_ = _e73;
    if _e73 {
        let _e74 = (*value_1);
        phi_63_ = all((abs(_e74) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e79 = phi_63_;
    return _e79;
}

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
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
    var phi_101_: bool;
    var phi_110_: bool;
    var phi_120_: bool;
    var phi_127_: bool;
    var phi_156_: bool;

    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    let _e80 = temporalOutcome_1;
    let _e81 = (_e80 != 1u);
    phi_101_ = _e81;
    if !(_e81) {
        let _e83 = temporalCurrentClip_1;
        param = _e83;
        let _e84 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e84);
    }
    let _e87 = phi_101_;
    phi_110_ = _e87;
    if !(_e87) {
        let _e89 = temporalPreviousClip_1;
        param_1 = _e89;
        let _e90 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e90);
    }
    let _e93 = phi_110_;
    phi_120_ = _e93;
    if !(_e93) {
        let _e96 = temporalCurrentClip_1[3u];
        phi_120_ = (_e96 <= 0.000001f);
    }
    let _e99 = phi_120_;
    phi_127_ = _e99;
    if !(_e99) {
        let _e102 = temporalPreviousClip_1[3u];
        phi_127_ = (_e102 <= 0.000001f);
    }
    let _e105 = phi_127_;
    if _e105 {
        return;
    }
    let _e106 = temporalCurrentClip_1;
    let _e109 = temporalCurrentClip_1[3u];
    currentNdc = (_e106.xy / vec2(_e109));
    let _e112 = temporalPreviousClip_1;
    let _e115 = temporalPreviousClip_1[3u];
    previousNdc = (_e112.xy / vec2(_e115));
    let _e118 = currentNdc;
    param_2 = _e118;
    let _e119 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e120 = !(_e119);
    phi_156_ = _e120;
    if !(_e120) {
        let _e122 = previousNdc;
        param_3 = _e122;
        let _e123 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e123);
    }
    let _e126 = phi_156_;
    if _e126 {
        return;
    }
    let _e127 = currentNdc;
    currentUv = ((_e127 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e130 = previousNdc;
    previousUv = ((_e130 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e133 = currentUv;
    let _e134 = previousUv;
    velocity = (_e133 - _e134);
    let _e136 = velocity;
    param_4 = _e136;
    let _e137 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e137) {
        return;
    }
    let _e139 = velocity;
    out_temporal_velocity = _e139;
    let _e140 = (*coverageConfidence);
    out_temporal_validity = clamp(_e140, 0f, 1f);
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
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_14: f32;

    let _e89 = frag_color0In_1;
    param_7 = _e89.xyz;
    let _e91 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e93 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e91.x, _e91.y, _e91.z, _e93);
    param_8 = 0u;
    let _e98 = frag_tex_coord0_1;
    param_9 = _e98;
    param_10 = 0i;
    let _e99 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e99;
    param_11 = 1u;
    let _e100 = frag_tex_coord1_1;
    param_12 = _e100;
    param_13 = 1i;
    let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor1_ = _e101;
    let _e102 = sampledColor0_;
    let _e103 = frag_color0_;
    color0_ = (_e102 * _e103);
    if override_type_3_ {
        let _e105 = sampledColor1_;
        color1_ = _e105;
        let _e106 = color0_;
        let _e108 = color1_;
        let _e110 = (_e106.xyz + _e108.xyz);
        let _e112 = color0_[3u];
        let _e114 = color1_[3u];
        base = vec4<f32>(_e110.x, _e110.y, _e110.z, (_e112 * _e114));
    } else {
        if override_type_3_1 {
            let _e120 = sampledColor1_;
            let _e121 = frag_color0_;
            color1_1 = (_e120 * _e121);
            let _e123 = color0_;
            let _e125 = color1_1;
            let _e127 = (_e123.xyz + _e125.xyz);
            let _e129 = color0_[3u];
            let _e131 = color1_1[3u];
            base = vec4<f32>(_e127.x, _e127.y, _e127.z, (_e129 * _e131));
        } else {
            let _e137 = sampledColor1_;
            color1_2 = _e137;
            let _e138 = color0_;
            let _e140 = color1_2;
            let _e142 = (_e138.xyz * _e140.xyz);
            base[0u] = _e142.x;
            base[1u] = _e142.y;
            base[2u] = _e142.z;
            let _e150 = color0_[3u];
            let _e152 = color1_2[3u];
            base[3u] = (_e150 * _e152);
        }
    }
    if override_type_3_2 {
        let _e157 = unnamed.worldLightParams[1u];
        wetness = clamp(_e157, 0f, 1f);
        let _e161 = unnamed.worldLightParams[2u];
        frost = clamp(_e161, 0f, 1f);
        let _e163 = base;
        luminance = dot(_e163.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e166 = wetness;
        let _e168 = base;
        let _e170 = (_e168.xyz * mix(1f, 0.82f, _e166));
        base[0u] = _e170.x;
        base[1u] = _e170.y;
        base[2u] = _e170.z;
        let _e177 = base;
        let _e179 = luminance;
        let _e181 = luminance;
        let _e183 = luminance;
        let _e185 = frost;
        let _e188 = mix(_e177.xyz, vec3<f32>((_e179 * 0.88f), (_e181 * 0.94f), _e183), vec3((_e185 * 0.55f)));
        base[0u] = _e188.x;
        base[1u] = _e188.y;
        base[2u] = _e188.z;
    }
    let _e197 = unnamed.materialFactors[0u];
    if (_e197 >= 0f) {
        let _e200 = unnamed.materialBaseColor;
        let _e201 = base;
        base = (_e201 * _e200);
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
        let _e220 = base;
        let _e223 = unnamed.advancedFogColorDensity;
        let _e225 = fogAmount;
        let _e227 = mix(_e220.xyz, _e223.xyz, vec3(_e225));
        base[0u] = _e227.x;
        base[1u] = _e227.y;
        base[2u] = _e227.z;
    }
    if override_type_3_3 {
        let _e235 = base[3u];
        if (_e235 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e237 = base;
            let _e239 = base;
            if (dot(_e237.xyz, _e239.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e243 = base;
    out_color = _e243;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
