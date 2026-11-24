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
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_79_: bool;

    let _e71 = (*value);
    let _e72 = (*value);
    let _e74 = all((_e71 == _e72));
    phi_79_ = _e74;
    if _e74 {
        let _e75 = (*value);
        phi_79_ = all((abs(_e75) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e80 = phi_79_;
    return _e80;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_64_: bool;

    let _e71 = (*value_1);
    let _e72 = (*value_1);
    let _e74 = all((_e71 == _e72));
    phi_64_ = _e74;
    if _e74 {
        let _e75 = (*value_1);
        phi_64_ = all((abs(_e75) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e80 = phi_64_;
    return _e80;
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
    let _e82 = temporalOutcome_1;
    let _e83 = (_e82 != 1u);
    phi_103_ = _e83;
    if !(_e83) {
        let _e85 = temporalCurrentClip_1;
        param = _e85;
        let _e86 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_103_ = !(_e86);
    }
    let _e89 = phi_103_;
    phi_112_ = _e89;
    if !(_e89) {
        let _e91 = temporalPreviousClip_1;
        param_1 = _e91;
        let _e92 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_112_ = !(_e92);
    }
    let _e95 = phi_112_;
    phi_122_ = _e95;
    if !(_e95) {
        let _e98 = temporalCurrentClip_1[3u];
        phi_122_ = (_e98 <= 0.000001f);
    }
    let _e101 = phi_122_;
    phi_129_ = _e101;
    if !(_e101) {
        let _e104 = temporalPreviousClip_1[3u];
        phi_129_ = (_e104 <= 0.000001f);
    }
    let _e107 = phi_129_;
    if _e107 {
        return;
    }
    let _e108 = temporalCurrentClip_1;
    let _e111 = temporalCurrentClip_1[3u];
    currentNdc = (_e108.xy / vec2(_e111));
    let _e114 = temporalPreviousClip_1;
    let _e117 = temporalPreviousClip_1[3u];
    previousNdc = (_e114.xy / vec2(_e117));
    let _e120 = currentNdc;
    param_2 = _e120;
    let _e121 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e122 = !(_e121);
    phi_158_ = _e122;
    if !(_e122) {
        let _e124 = previousNdc;
        param_3 = _e124;
        let _e125 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_158_ = !(_e125);
    }
    let _e128 = phi_158_;
    if _e128 {
        return;
    }
    let _e129 = currentNdc;
    currentUv = ((_e129 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e132 = previousNdc;
    previousUv = ((_e132 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e135 = currentUv;
    let _e136 = previousUv;
    velocity = (_e135 - _e136);
    let _e138 = velocity;
    param_4 = _e138;
    let _e139 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e139) {
        return;
    }
    let _e141 = velocity;
    out_temporal_velocity = _e141;
    let _e142 = (*coverageConfidence);
    out_temporal_validity = vec4<f32>(clamp(_e142, 0f, 1f), 0f, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e73 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e73 + 0.5f));
    let _e78 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e80 = fogType;
    let _e83 = fogType;
    return (((_e78 > 0.5f) && (_e80 >= 1i)) && (_e83 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e73 = wired_advanced_fog_enabled_u0028_();
    if !(_e73) {
        return 0f;
    }
    let _e76 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e76, 0.000001f));
    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e81 + 0.5f));
    let _e84 = fogType_1;
    if (_e84 == 1i) {
        let _e88 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e88 <= 0f) {
            return 0f;
        }
        let _e90 = viewDepth;
        let _e93 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e90 / _e93), 0f, 1f);
    }
    let _e98 = unnamed.advancedFogColorDensity[3u];
    let _e100 = viewDepth;
    opticalDepth = (max(_e98, 0f) * _e100);
    let _e102 = fogType_1;
    if (_e102 == 2i) {
        let _e104 = opticalDepth;
        return clamp((1f - exp(-(_e104))), 0f, 1f);
    }
    let _e109 = opticalDepth;
    let _e110 = opticalDepth;
    return clamp((1f - exp(-((_e109 * _e110)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e73 = (*rgb);
    let _e76 = unnamed.worldLightParams[0u];
    boosted = (_e73 * _e76);
    let _e79 = boosted[0u];
    let _e81 = boosted[1u];
    let _e83 = boosted[2u];
    peak = max(_e79, max(_e81, _e83));
    let _e86 = peak;
    if (_e86 > 1f) {
        let _e88 = peak;
        let _e89 = boosted;
        boosted = (_e89 / vec3(_e88));
    }
    let _e92 = boosted;
    return _e92;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e74 = (*c);
    (*c) = max(_e74, vec3<f32>(0f, 0f, 0f));
    let _e76 = (*c);
    cutoff = (_e76 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e78 = (*c);
    lo = (_e78 / vec3(12.92f));
    let _e81 = (*c);
    hi = pow(((_e81 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e86 = hi;
    let _e87 = lo;
    let _e88 = cutoff;
    return mix(_e86, _e87, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e88));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e98 = (*uv);
    let _e99 = textureSample(wired_bindless_images[(_e83 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    c_1 = _e99;
    let _e100 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e100))) == 0i) {
        let _e105 = c_1;
        param_5 = _e105.xyz;
        let _e107 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e107.x;
        c_1[1u] = _e107.y;
        c_1[2u] = _e107.z;
    }
    let _e114 = (*slot);
    if (lightmap_slot == (_e114 + 1i)) {
        let _e117 = c_1;
        param_6 = _e117.xyz;
        let _e119 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e119.x;
        c_1[1u] = _e119.y;
        c_1[2u] = _e119.z;
    }
    let _e126 = c_1;
    return _e126;
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
    var param_15: vec4<f32>;

    let _e91 = frag_color0In_1;
    param_7 = _e91.xyz;
    let _e93 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e95 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e93.x, _e93.y, _e93.z, _e95);
    param_8 = 0u;
    let _e100 = frag_tex_coord0_1;
    param_9 = _e100;
    param_10 = 0i;
    let _e101 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e101;
    param_11 = 1u;
    let _e102 = frag_tex_coord1_1;
    param_12 = _e102;
    param_13 = 1i;
    let _e103 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor1_ = _e103;
    let _e104 = sampledColor0_;
    let _e105 = frag_color0_;
    color0_ = (_e104 * _e105);
    if override_type_3_ {
        let _e107 = sampledColor1_;
        color1_ = _e107;
        let _e108 = color0_;
        let _e110 = color1_;
        let _e112 = (_e108.xyz + _e110.xyz);
        let _e114 = color0_[3u];
        let _e116 = color1_[3u];
        base = vec4<f32>(_e112.x, _e112.y, _e112.z, (_e114 * _e116));
    } else {
        if override_type_3_1 {
            let _e122 = sampledColor1_;
            let _e123 = frag_color0_;
            color1_1 = (_e122 * _e123);
            let _e125 = color0_;
            let _e127 = color1_1;
            let _e129 = (_e125.xyz + _e127.xyz);
            let _e131 = color0_[3u];
            let _e133 = color1_1[3u];
            base = vec4<f32>(_e129.x, _e129.y, _e129.z, (_e131 * _e133));
        } else {
            let _e139 = sampledColor1_;
            color1_2 = _e139;
            let _e140 = color0_;
            let _e142 = color1_2;
            let _e144 = (_e140.xyz * _e142.xyz);
            base[0u] = _e144.x;
            base[1u] = _e144.y;
            base[2u] = _e144.z;
            let _e152 = color0_[3u];
            let _e154 = color1_2[3u];
            base[3u] = (_e152 * _e154);
        }
    }
    if override_type_3_2 {
        let _e159 = unnamed.worldLightParams[1u];
        wetness = clamp(_e159, 0f, 1f);
        let _e163 = unnamed.worldLightParams[2u];
        frost = clamp(_e163, 0f, 1f);
        let _e165 = base;
        luminance = dot(_e165.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e168 = wetness;
        let _e170 = base;
        let _e172 = (_e170.xyz * mix(1f, 0.82f, _e168));
        base[0u] = _e172.x;
        base[1u] = _e172.y;
        base[2u] = _e172.z;
        let _e179 = base;
        let _e181 = luminance;
        let _e183 = luminance;
        let _e185 = luminance;
        let _e187 = frost;
        let _e190 = mix(_e179.xyz, vec3<f32>((_e181 * 0.88f), (_e183 * 0.94f), _e185), vec3((_e187 * 0.55f)));
        base[0u] = _e190.x;
        base[1u] = _e190.y;
        base[2u] = _e190.z;
    }
    let _e199 = unnamed.materialFactors[0u];
    if (_e199 >= 0f) {
        let _e202 = unnamed.materialBaseColor;
        let _e203 = base;
        base = (_e203 * _e202);
    }
    let _e205 = color0_;
    let _e208 = unnamed.emissionRadiance;
    let _e211 = base;
    let _e213 = (_e211.xyz + (_e205.xyz * _e208.xyz));
    base[0u] = _e213.x;
    base[1u] = _e213.y;
    base[2u] = _e213.z;
    let _e220 = wired_advanced_fog_enabled_u0028_();
    if _e220 {
        let _e221 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e221;
        let _e222 = base;
        let _e225 = unnamed.advancedFogColorDensity;
        let _e227 = fogAmount;
        let _e229 = mix(_e222.xyz, _e225.xyz, vec3(_e227));
        base[0u] = _e229.x;
        base[1u] = _e229.y;
        base[2u] = _e229.z;
    }
    if override_type_3_3 {
        let _e237 = base[3u];
        if (_e237 == 0f) {
            discard;
        }
    } else {
        if override_type_3_4 {
            let _e239 = base;
            let _e241 = base;
            if (dot(_e239.xyz, _e241.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e245 = base;
    out_color = _e245;
    param_14 = 1f;
    let _e246 = base;
    param_15 = _e246;
    wiredTemporalWriteAux_u0028_f1_u003b_vf4_u003b((&param_14), (&param_15));
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
