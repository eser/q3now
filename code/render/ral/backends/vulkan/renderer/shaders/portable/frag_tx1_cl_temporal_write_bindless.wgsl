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
override override_type_3_2: bool = (override_type_3_ || override_type_3_1);
override override_type_3_3: bool = (tex_mode == 3i);
override override_type_3_4: bool = (tex_mode == 4i);
override override_type_3_5: bool = (tex_mode == 5i);
override override_type_3_6: bool = (tex_mode == 6i);
override override_type_3_7: bool = (tex_mode == 7i);
override override_type_3_8: bool = (lightmap_slot != 0i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e81 = (*value);
    let _e82 = (*value);
    let _e84 = all((_e81 == _e82));
    phi_78_ = _e84;
    if _e84 {
        let _e85 = (*value);
        phi_78_ = all((abs(_e85) <= vec2<f32>(0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e90 = phi_78_;
    return _e90;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e81 = (*value_1);
    let _e82 = (*value_1);
    let _e84 = all((_e81 == _e82));
    phi_63_ = _e84;
    if _e84 {
        let _e85 = (*value_1);
        phi_63_ = all((abs(_e85) <= vec4<f32>(0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f, 0x1.fffffep+127f)));
    }
    let _e90 = phi_63_;
    return _e90;
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
    let _e91 = temporalOutcome_1;
    let _e92 = (_e91 != 1u);
    phi_101_ = _e92;
    if !(_e92) {
        let _e94 = temporalCurrentClip_1;
        param = _e94;
        let _e95 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e95);
    }
    let _e98 = phi_101_;
    phi_110_ = _e98;
    if !(_e98) {
        let _e100 = temporalPreviousClip_1;
        param_1 = _e100;
        let _e101 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e101);
    }
    let _e104 = phi_110_;
    phi_120_ = _e104;
    if !(_e104) {
        let _e107 = temporalCurrentClip_1[3u];
        phi_120_ = (_e107 <= 0.000001f);
    }
    let _e110 = phi_120_;
    phi_127_ = _e110;
    if !(_e110) {
        let _e113 = temporalPreviousClip_1[3u];
        phi_127_ = (_e113 <= 0.000001f);
    }
    let _e116 = phi_127_;
    if _e116 {
        return;
    }
    let _e117 = temporalCurrentClip_1;
    let _e120 = temporalCurrentClip_1[3u];
    currentNdc = (_e117.xy / vec2(_e120));
    let _e123 = temporalPreviousClip_1;
    let _e126 = temporalPreviousClip_1[3u];
    previousNdc = (_e123.xy / vec2(_e126));
    let _e129 = currentNdc;
    param_2 = _e129;
    let _e130 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e131 = !(_e130);
    phi_156_ = _e131;
    if !(_e131) {
        let _e133 = previousNdc;
        param_3 = _e133;
        let _e134 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e134);
    }
    let _e137 = phi_156_;
    if _e137 {
        return;
    }
    let _e138 = currentNdc;
    currentUv = ((_e138 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e141 = previousNdc;
    previousUv = ((_e141 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e144 = currentUv;
    let _e145 = previousUv;
    velocity = (_e144 - _e145);
    let _e147 = velocity;
    param_4 = _e147;
    let _e148 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e148) {
        return;
    }
    let _e150 = velocity;
    out_temporal_velocity = _e150;
    let _e151 = (*coverageConfidence);
    out_temporal_validity = clamp(_e151, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e83 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e83 + 0.5f));
    let _e88 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e90 = fogType;
    let _e93 = fogType;
    return (((_e88 > 0.5f) && (_e90 >= 1i)) && (_e93 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e83 = wired_advanced_fog_enabled_u0028_();
    if !(_e83) {
        return 0f;
    }
    let _e86 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e86, 0.000001f));
    let _e91 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e91 + 0.5f));
    let _e94 = fogType_1;
    if (_e94 == 1i) {
        let _e98 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e98 <= 0f) {
            return 0f;
        }
        let _e100 = viewDepth;
        let _e103 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e100 / _e103), 0f, 1f);
    }
    let _e108 = unnamed.advancedFogColorDensity[3u];
    let _e110 = viewDepth;
    opticalDepth = (max(_e108, 0f) * _e110);
    let _e112 = fogType_1;
    if (_e112 == 2i) {
        let _e114 = opticalDepth;
        return clamp((1f - exp(-(_e114))), 0f, 1f);
    }
    let _e119 = opticalDepth;
    let _e120 = opticalDepth;
    return clamp((1f - exp(-((_e119 * _e120)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e83 = (*rgb);
    let _e86 = unnamed.worldLightParams[0u];
    boosted = (_e83 * _e86);
    let _e89 = boosted[0u];
    let _e91 = boosted[1u];
    let _e93 = boosted[2u];
    peak = max(_e89, max(_e91, _e93));
    let _e96 = peak;
    if (_e96 > 1f) {
        let _e98 = peak;
        let _e99 = boosted;
        boosted = (_e99 / vec3(_e98));
    }
    let _e102 = boosted;
    return _e102;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e84 = (*c);
    (*c) = max(_e84, vec3<f32>(0f, 0f, 0f));
    let _e86 = (*c);
    cutoff = (_e86 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e88 = (*c);
    lo = (_e88 / vec3(12.92f));
    let _e91 = (*c);
    hi = pow(((_e91 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e96 = hi;
    let _e97 = lo;
    let _e98 = cutoff;
    return mix(_e96, _e97, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e98));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e86 = (*role);
    let _e88 = (*role);
    let _e93 = unnamed.packed_indices[(_e86 / 4u)][(_e88 % 4u)];
    let _e96 = (*role);
    let _e98 = (*role);
    let _e103 = unnamed.packed_indices[(_e96 / 4u)][(_e98 % 4u)];
    let _e108 = (*uv);
    let _e109 = textureSample(wired_bindless_images[(_e93 & 4095u)], wired_bindless_samplers[((_e103 >> bitcast<u32>(12i)) & 255u)], _e108);
    c_1 = _e109;
    let _e110 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e110))) == 0i) {
        let _e115 = c_1;
        param_5 = _e115.xyz;
        let _e117 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e117.x;
        c_1[1u] = _e117.y;
        c_1[2u] = _e117.z;
    }
    let _e124 = (*slot);
    if (lightmap_slot == (_e124 + 1i)) {
        let _e127 = c_1;
        param_6 = _e127.xyz;
        let _e129 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e129.x;
        c_1[1u] = _e129.y;
        c_1[2u] = _e129.z;
    }
    let _e136 = c_1;
    return _e136;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_7: vec3<f32>;
    var frag_color1_: vec4<f32>;
    var param_8: vec3<f32>;
    var sampledColor0_: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var sampledColor1_: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color0_: vec4<f32>;
    var color1_: vec4<f32>;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var color1_2: vec4<f32>;
    var color1_3: vec4<f32>;
    var color1_4: vec4<f32>;
    var color1_5: vec4<f32>;
    var color1_6: vec4<f32>;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_15: f32;

    let _e106 = frag_color0In_1;
    param_7 = _e106.xyz;
    let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e110 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e108.x, _e108.y, _e108.z, _e110);
    let _e115 = frag_color1In_1;
    param_8 = _e115.xyz;
    let _e117 = sRGBToLinear_u0028_vf3_u003b((&param_8));
    let _e119 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e117.x, _e117.y, _e117.z, _e119);
    param_9 = 0u;
    let _e124 = frag_tex_coord0_1;
    param_10 = _e124;
    param_11 = 0i;
    let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
    sampledColor0_ = _e125;
    param_12 = 1u;
    let _e126 = frag_tex_coord1_1;
    param_13 = _e126;
    param_14 = 1i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
    sampledColor1_ = _e127;
    let _e128 = sampledColor0_;
    let _e129 = frag_color0_;
    color0_ = (_e128 * _e129);
    if override_type_3_2 {
        let _e131 = sampledColor1_;
        let _e132 = frag_color1_;
        color1_ = (_e131 * _e132);
        let _e134 = color0_;
        let _e136 = color1_;
        let _e138 = (_e134.xyz + _e136.xyz);
        let _e140 = color0_[3u];
        let _e142 = color1_[3u];
        base = vec4<f32>(_e138.x, _e138.y, _e138.z, (_e140 * _e142));
    } else {
        if override_type_3_3 {
            let _e148 = sampledColor1_;
            let _e149 = frag_color1_;
            color1_1 = (_e148 * _e149);
            let _e152 = color0_[3u];
            let _e153 = color0_;
            color0_ = (_e153 * _e152);
            let _e156 = color1_1[3u];
            let _e157 = color1_1;
            color1_1 = (_e157 * _e156);
            let _e159 = color0_;
            let _e161 = color1_1;
            let _e163 = (_e159.xyz + _e161.xyz);
            let _e165 = color0_[3u];
            let _e167 = color1_1[3u];
            base = vec4<f32>(_e163.x, _e163.y, _e163.z, (_e165 * _e167));
        } else {
            if override_type_3_4 {
                let _e173 = sampledColor1_;
                let _e174 = frag_color1_;
                color1_2 = (_e173 * _e174);
                let _e177 = color0_[3u];
                let _e179 = color0_;
                color0_ = (_e179 * (1f - _e177));
                let _e182 = color1_2[3u];
                let _e184 = color1_2;
                color1_2 = (_e184 * (1f - _e182));
                let _e186 = color0_;
                let _e188 = color1_2;
                let _e190 = (_e186.xyz + _e188.xyz);
                let _e192 = color0_[3u];
                let _e194 = color1_2[3u];
                base = vec4<f32>(_e190.x, _e190.y, _e190.z, (_e192 * _e194));
            } else {
                if override_type_3_5 {
                    let _e200 = sampledColor1_;
                    let _e201 = frag_color1_;
                    color1_3 = (_e200 * _e201);
                    let _e203 = color0_;
                    let _e204 = color1_3;
                    let _e206 = color1_3[3u];
                    base = mix(_e203, _e204, vec4(_e206));
                } else {
                    if override_type_3_6 {
                        let _e209 = sampledColor1_;
                        let _e210 = frag_color1_;
                        color1_4 = (_e209 * _e210);
                        let _e212 = color1_4;
                        let _e213 = color0_;
                        let _e215 = color1_4[3u];
                        base = mix(_e212, _e213, vec4(_e215));
                    } else {
                        if override_type_3_7 {
                            let _e218 = sampledColor1_;
                            let _e219 = frag_color1_;
                            color1_5 = (_e218 * _e219);
                            let _e221 = color1_5;
                            let _e223 = color1_5[3u];
                            let _e226 = color0_;
                            base = ((_e221 + vec4(_e223)) * _e226);
                        } else {
                            let _e228 = sampledColor1_;
                            let _e229 = frag_color1_;
                            color1_6 = (_e228 * _e229);
                            let _e231 = color0_;
                            let _e233 = color1_6;
                            let _e235 = (_e231.xyz * _e233.xyz);
                            base[0u] = _e235.x;
                            base[1u] = _e235.y;
                            base[2u] = _e235.z;
                            let _e243 = color0_[3u];
                            let _e245 = color1_6[3u];
                            base[3u] = (_e243 * _e245);
                        }
                    }
                }
            }
        }
    }
    if override_type_3_8 {
        let _e250 = unnamed.worldLightParams[1u];
        wetness = clamp(_e250, 0f, 1f);
        let _e254 = unnamed.worldLightParams[2u];
        frost = clamp(_e254, 0f, 1f);
        let _e256 = base;
        luminance = dot(_e256.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e259 = wetness;
        let _e261 = base;
        let _e263 = (_e261.xyz * mix(1f, 0.82f, _e259));
        base[0u] = _e263.x;
        base[1u] = _e263.y;
        base[2u] = _e263.z;
        let _e270 = base;
        let _e272 = luminance;
        let _e274 = luminance;
        let _e276 = luminance;
        let _e278 = frost;
        let _e281 = mix(_e270.xyz, vec3<f32>((_e272 * 0.88f), (_e274 * 0.94f), _e276), vec3((_e278 * 0.55f)));
        base[0u] = _e281.x;
        base[1u] = _e281.y;
        base[2u] = _e281.z;
    }
    let _e290 = unnamed.materialFactors[0u];
    if (_e290 >= 0f) {
        let _e293 = unnamed.materialBaseColor;
        let _e294 = base;
        base = (_e294 * _e293);
    }
    let _e296 = color0_;
    let _e299 = unnamed.emissionRadiance;
    let _e302 = base;
    let _e304 = (_e302.xyz + (_e296.xyz * _e299.xyz));
    base[0u] = _e304.x;
    base[1u] = _e304.y;
    base[2u] = _e304.z;
    let _e311 = wired_advanced_fog_enabled_u0028_();
    if _e311 {
        let _e312 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e312;
        let _e313 = base;
        let _e316 = unnamed.advancedFogColorDensity;
        let _e318 = fogAmount;
        let _e320 = mix(_e313.xyz, _e316.xyz, vec3(_e318));
        base[0u] = _e320.x;
        base[1u] = _e320.y;
        base[2u] = _e320.z;
    }
    if override_type_3_9 {
        let _e328 = base[3u];
        if (_e328 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e330 = base;
            let _e332 = base;
            if (dot(_e330.xyz, _e332.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e336 = base;
    out_color = _e336;
    param_15 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_15));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
