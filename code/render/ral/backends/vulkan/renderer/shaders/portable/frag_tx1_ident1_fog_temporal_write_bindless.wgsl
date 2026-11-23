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
@id(10) override acff: i32 = 0i;
override override_type_3_3: bool = (acff == 1i);
override override_type_3_4: bool = (acff == 2i);
override override_type_3_5: bool = (acff == 3i);
override override_type_3_6: bool = (acff == 1i);
override override_type_3_7: bool = (acff == 2i);
override override_type_3_8: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_3_9: bool = (discard_mode == 1i);
override override_type_3_10: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e78 = (*value);
    let _e79 = (*value);
    let _e81 = all((_e78 == _e79));
    phi_78_ = _e81;
    if _e81 {
        let _e82 = (*value);
        phi_78_ = all((abs(_e82) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e87 = phi_78_;
    return _e87;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e78 = (*value_1);
    let _e79 = (*value_1);
    let _e81 = all((_e78 == _e79));
    phi_63_ = _e81;
    if _e81 {
        let _e82 = (*value_1);
        phi_63_ = all((abs(_e82) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e87 = phi_63_;
    return _e87;
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
    let _e88 = temporalOutcome_1;
    let _e89 = (_e88 != 1u);
    phi_101_ = _e89;
    if !(_e89) {
        let _e91 = temporalCurrentClip_1;
        param = _e91;
        let _e92 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e92);
    }
    let _e95 = phi_101_;
    phi_110_ = _e95;
    if !(_e95) {
        let _e97 = temporalPreviousClip_1;
        param_1 = _e97;
        let _e98 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e98);
    }
    let _e101 = phi_110_;
    phi_120_ = _e101;
    if !(_e101) {
        let _e104 = temporalCurrentClip_1[3u];
        phi_120_ = (_e104 <= 0.000001f);
    }
    let _e107 = phi_120_;
    phi_127_ = _e107;
    if !(_e107) {
        let _e110 = temporalPreviousClip_1[3u];
        phi_127_ = (_e110 <= 0.000001f);
    }
    let _e113 = phi_127_;
    if _e113 {
        return;
    }
    let _e114 = temporalCurrentClip_1;
    let _e117 = temporalCurrentClip_1[3u];
    currentNdc = (_e114.xy / vec2(_e117));
    let _e120 = temporalPreviousClip_1;
    let _e123 = temporalPreviousClip_1[3u];
    previousNdc = (_e120.xy / vec2(_e123));
    let _e126 = currentNdc;
    param_2 = _e126;
    let _e127 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e128 = !(_e127);
    phi_156_ = _e128;
    if !(_e128) {
        let _e130 = previousNdc;
        param_3 = _e130;
        let _e131 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e131);
    }
    let _e134 = phi_156_;
    if _e134 {
        return;
    }
    let _e135 = currentNdc;
    currentUv = ((_e135 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e138 = previousNdc;
    previousUv = ((_e138 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e141 = currentUv;
    let _e142 = previousUv;
    velocity = (_e141 - _e142);
    let _e144 = velocity;
    param_4 = _e144;
    let _e145 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e145) {
        return;
    }
    let _e147 = velocity;
    out_temporal_velocity = _e147;
    let _e148 = (*coverageConfidence);
    out_temporal_validity = clamp(_e148, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e80 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e80 + 0.5f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e87 = fogType;
    let _e90 = fogType;
    return (((_e85 > 0.5f) && (_e87 >= 1i)) && (_e90 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e80 = wired_advanced_fog_enabled_u0028_();
    if !(_e80) {
        return 0f;
    }
    let _e83 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e83, 0.000001f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e88 + 0.5f));
    let _e91 = fogType_1;
    if (_e91 == 1i) {
        let _e95 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e95 <= 0f) {
            return 0f;
        }
        let _e97 = viewDepth;
        let _e100 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e97 / _e100), 0f, 1f);
    }
    let _e105 = unnamed.advancedFogColorDensity[3u];
    let _e107 = viewDepth;
    opticalDepth = (max(_e105, 0f) * _e107);
    let _e109 = fogType_1;
    if (_e109 == 2i) {
        let _e111 = opticalDepth;
        return clamp((1f - exp(-(_e111))), 0f, 1f);
    }
    let _e116 = opticalDepth;
    let _e117 = opticalDepth;
    return clamp((1f - exp(-((_e116 * _e117)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e80 = (*rgb);
    let _e83 = unnamed.worldLightParams[0u];
    boosted = (_e80 * _e83);
    let _e86 = boosted[0u];
    let _e88 = boosted[1u];
    let _e90 = boosted[2u];
    peak = max(_e86, max(_e88, _e90));
    let _e93 = peak;
    if (_e93 > 1f) {
        let _e95 = peak;
        let _e96 = boosted;
        boosted = (_e96 / vec3(_e95));
    }
    let _e99 = boosted;
    return _e99;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e81 = (*c);
    (*c) = max(_e81, vec3<f32>(0f, 0f, 0f));
    let _e83 = (*c);
    cutoff = (_e83 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e85 = (*c);
    lo = (_e85 / vec3(12.92f));
    let _e88 = (*c);
    hi = pow(((_e88 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e93 = hi;
    let _e94 = lo;
    let _e95 = cutoff;
    return mix(_e93, _e94, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e95));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e83 = (*role);
    let _e85 = (*role);
    let _e90 = unnamed.packed_indices[(_e83 / 4u)][(_e85 % 4u)];
    let _e93 = (*role);
    let _e95 = (*role);
    let _e100 = unnamed.packed_indices[(_e93 / 4u)][(_e95 % 4u)];
    let _e105 = (*uv);
    let _e106 = textureSample(wired_bindless_images[(_e90 & 4095u)], wired_bindless_samplers[((_e100 >> bitcast<u32>(12i)) & 255u)], _e105);
    c_1 = _e106;
    let _e107 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e107))) == 0i) {
        let _e112 = c_1;
        param_5 = _e112.xyz;
        let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = (*slot);
    if (lightmap_slot == (_e121 + 1i)) {
        let _e124 = c_1;
        param_6 = _e124.xyz;
        let _e126 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e126.x;
        c_1[1u] = _e126.y;
        c_1[2u] = _e126.z;
    }
    let _e133 = c_1;
    return _e133;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e99 = unnamed.packed_indices[0i][3u];
    let _e105 = unnamed.packed_indices[0i][3u];
    let _e110 = fog_tex_coord_1;
    let _e111 = textureSample(wired_bindless_images[(_e99 & 4095u)], wired_bindless_samplers[((_e105 >> bitcast<u32>(12i)) & 255u)], _e110);
    fog = _e111;
    param_7 = 0u;
    let _e112 = frag_tex_coord0_1;
    param_8 = _e112;
    param_9 = 0i;
    let _e113 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
    sampledColor0_ = _e113;
    param_10 = 1u;
    let _e114 = frag_tex_coord1_1;
    param_11 = _e114;
    param_12 = 1i;
    let _e115 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
    sampledColor1_ = _e115;
    let _e116 = sampledColor0_;
    color0_ = _e116;
    if override_type_3_ {
        let _e117 = sampledColor1_;
        color1_ = _e117;
        let _e118 = color0_;
        let _e120 = color1_;
        let _e122 = (_e118.xyz + _e120.xyz);
        let _e124 = color0_[3u];
        let _e126 = color1_[3u];
        base = vec4<f32>(_e122.x, _e122.y, _e122.z, (_e124 * _e126));
    } else {
        if override_type_3_1 {
            let _e132 = sampledColor1_;
            color1_1 = _e132;
            let _e133 = color0_;
            let _e135 = color1_1;
            let _e137 = (_e133.xyz + _e135.xyz);
            let _e139 = color0_[3u];
            let _e141 = color1_1[3u];
            base = vec4<f32>(_e137.x, _e137.y, _e137.z, (_e139 * _e141));
        } else {
            let _e147 = sampledColor1_;
            color1_2 = _e147;
            let _e148 = color0_;
            let _e150 = color1_2;
            let _e152 = (_e148.xyz * _e150.xyz);
            base[0u] = _e152.x;
            base[1u] = _e152.y;
            base[2u] = _e152.z;
            let _e160 = color0_[3u];
            let _e162 = color1_2[3u];
            base[3u] = (_e160 * _e162);
        }
    }
    if override_type_3_2 {
        let _e167 = unnamed.worldLightParams[1u];
        wetness = clamp(_e167, 0f, 1f);
        let _e171 = unnamed.worldLightParams[2u];
        frost = clamp(_e171, 0f, 1f);
        let _e173 = base;
        luminance = dot(_e173.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e176 = wetness;
        let _e178 = base;
        let _e180 = (_e178.xyz * mix(1f, 0.82f, _e176));
        base[0u] = _e180.x;
        base[1u] = _e180.y;
        base[2u] = _e180.z;
        let _e187 = base;
        let _e189 = luminance;
        let _e191 = luminance;
        let _e193 = luminance;
        let _e195 = frost;
        let _e198 = mix(_e187.xyz, vec3<f32>((_e189 * 0.88f), (_e191 * 0.94f), _e193), vec3((_e195 * 0.55f)));
        base[0u] = _e198.x;
        base[1u] = _e198.y;
        base[2u] = _e198.z;
    }
    let _e207 = unnamed.materialFactors[0u];
    if (_e207 >= 0f) {
        let _e210 = unnamed.materialBaseColor;
        let _e211 = base;
        base = (_e211 * _e210);
    }
    let _e213 = color0_;
    let _e216 = unnamed.emissionRadiance;
    let _e219 = base;
    let _e221 = (_e219.xyz + (_e213.xyz * _e216.xyz));
    base[0u] = _e221.x;
    base[1u] = _e221.y;
    base[2u] = _e221.z;
    let _e228 = wired_advanced_fog_enabled_u0028_();
    if _e228 {
        let _e229 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e229;
        if override_type_3_3 {
            let _e230 = fogAmount;
            let _e232 = base;
            let _e234 = (_e232.xyz * (1f - _e230));
            base[0u] = _e234.x;
            base[1u] = _e234.y;
            base[2u] = _e234.z;
        } else {
            if override_type_3_4 {
                let _e241 = fogAmount;
                let _e243 = base;
                base = (_e243 * (1f - _e241));
            } else {
                if override_type_3_5 {
                    let _e245 = fogAmount;
                    let _e248 = base[3u];
                    base[3u] = (_e248 * (1f - _e245));
                } else {
                    let _e251 = base;
                    let _e254 = unnamed.advancedFogColorDensity;
                    let _e256 = fogAmount;
                    let _e258 = mix(_e251.xyz, _e254.xyz, vec3(_e256));
                    base[0u] = _e258.x;
                    base[1u] = _e258.y;
                    base[2u] = _e258.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e265 = base;
            let _e268 = fog[3u];
            let _e270 = (_e265.xyz * (1f - _e268));
            base[0u] = _e270.x;
            base[1u] = _e270.y;
            base[2u] = _e270.z;
        } else {
            if override_type_3_7 {
                let _e277 = base;
                let _e279 = fog[3u];
                base = (_e277 * (1f - _e279));
            } else {
                if override_type_3_8 {
                    let _e283 = base[3u];
                    let _e285 = fog[3u];
                    base[3u] = (_e283 * (1f - _e285));
                } else {
                    let _e289 = base;
                    let _e290 = fog;
                    let _e292 = unnamed.fogColor;
                    let _e295 = fog[3u];
                    base = mix(_e289, (_e290 * _e292), vec4(_e295));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e299 = base[3u];
        if (_e299 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e301 = base;
            let _e303 = base;
            if (dot(_e301.xyz, _e303.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e307 = base;
    out_color = _e307;
    param_13 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_13));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
