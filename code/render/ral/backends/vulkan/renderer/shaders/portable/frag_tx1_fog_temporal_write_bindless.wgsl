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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;

fn wiredTemporalFinite2_u0028_vf2_u003b(value: ptr<function, vec2<f32>>) -> bool {
    var phi_78_: bool;

    let _e79 = (*value);
    let _e80 = (*value);
    let _e82 = all((_e79 == _e80));
    phi_78_ = _e82;
    if _e82 {
        let _e83 = (*value);
        phi_78_ = all((abs(_e83) <= vec2<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e88 = phi_78_;
    return _e88;
}

fn wiredTemporalFinite4_u0028_vf4_u003b(value_1: ptr<function, vec4<f32>>) -> bool {
    var phi_63_: bool;

    let _e79 = (*value_1);
    let _e80 = (*value_1);
    let _e82 = all((_e79 == _e80));
    phi_63_ = _e82;
    if _e82 {
        let _e83 = (*value_1);
        phi_63_ = all((abs(_e83) <= vec4<f32>(340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f, 340282350000000000000000000000000000000f)));
    }
    let _e88 = phi_63_;
    return _e88;
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
    let _e89 = temporalOutcome_1;
    let _e90 = (_e89 != 1u);
    phi_101_ = _e90;
    if !(_e90) {
        let _e92 = temporalCurrentClip_1;
        param = _e92;
        let _e93 = wiredTemporalFinite4_u0028_vf4_u003b((&param));
        phi_101_ = !(_e93);
    }
    let _e96 = phi_101_;
    phi_110_ = _e96;
    if !(_e96) {
        let _e98 = temporalPreviousClip_1;
        param_1 = _e98;
        let _e99 = wiredTemporalFinite4_u0028_vf4_u003b((&param_1));
        phi_110_ = !(_e99);
    }
    let _e102 = phi_110_;
    phi_120_ = _e102;
    if !(_e102) {
        let _e105 = temporalCurrentClip_1[3u];
        phi_120_ = (_e105 <= 0.000001f);
    }
    let _e108 = phi_120_;
    phi_127_ = _e108;
    if !(_e108) {
        let _e111 = temporalPreviousClip_1[3u];
        phi_127_ = (_e111 <= 0.000001f);
    }
    let _e114 = phi_127_;
    if _e114 {
        return;
    }
    let _e115 = temporalCurrentClip_1;
    let _e118 = temporalCurrentClip_1[3u];
    currentNdc = (_e115.xy / vec2(_e118));
    let _e121 = temporalPreviousClip_1;
    let _e124 = temporalPreviousClip_1[3u];
    previousNdc = (_e121.xy / vec2(_e124));
    let _e127 = currentNdc;
    param_2 = _e127;
    let _e128 = wiredTemporalFinite2_u0028_vf2_u003b((&param_2));
    let _e129 = !(_e128);
    phi_156_ = _e129;
    if !(_e129) {
        let _e131 = previousNdc;
        param_3 = _e131;
        let _e132 = wiredTemporalFinite2_u0028_vf2_u003b((&param_3));
        phi_156_ = !(_e132);
    }
    let _e135 = phi_156_;
    if _e135 {
        return;
    }
    let _e136 = currentNdc;
    currentUv = ((_e136 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e139 = previousNdc;
    previousUv = ((_e139 * 0.5f) + vec2<f32>(0.5f, 0.5f));
    let _e142 = currentUv;
    let _e143 = previousUv;
    velocity = (_e142 - _e143);
    let _e145 = velocity;
    param_4 = _e145;
    let _e146 = wiredTemporalFinite2_u0028_vf2_u003b((&param_4));
    if !(_e146) {
        return;
    }
    let _e148 = velocity;
    out_temporal_velocity = _e148;
    let _e149 = (*coverageConfidence);
    out_temporal_validity = clamp(_e149, 0f, 1f);
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e81 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e81 + 0.5f));
    let _e86 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e88 = fogType;
    let _e91 = fogType;
    return (((_e86 > 0.5f) && (_e88 >= 1i)) && (_e91 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e81 = wired_advanced_fog_enabled_u0028_();
    if !(_e81) {
        return 0f;
    }
    let _e84 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e84, 0.000001f));
    let _e89 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e89 + 0.5f));
    let _e92 = fogType_1;
    if (_e92 == 1i) {
        let _e96 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e96 <= 0f) {
            return 0f;
        }
        let _e98 = viewDepth;
        let _e101 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e98 / _e101), 0f, 1f);
    }
    let _e106 = unnamed.advancedFogColorDensity[3u];
    let _e108 = viewDepth;
    opticalDepth = (max(_e106, 0f) * _e108);
    let _e110 = fogType_1;
    if (_e110 == 2i) {
        let _e112 = opticalDepth;
        return clamp((1f - exp(-(_e112))), 0f, 1f);
    }
    let _e117 = opticalDepth;
    let _e118 = opticalDepth;
    return clamp((1f - exp(-((_e117 * _e118)))), 0f, 1f);
}

fn wired_boost_legacy_lightmap_u0028_vf3_u003b(rgb: ptr<function, vec3<f32>>) -> vec3<f32> {
    var boosted: vec3<f32>;
    var peak: f32;

    let _e81 = (*rgb);
    let _e84 = unnamed.worldLightParams[0u];
    boosted = (_e81 * _e84);
    let _e87 = boosted[0u];
    let _e89 = boosted[1u];
    let _e91 = boosted[2u];
    peak = max(_e87, max(_e89, _e91));
    let _e94 = peak;
    if (_e94 > 1f) {
        let _e96 = peak;
        let _e97 = boosted;
        boosted = (_e97 / vec3(_e96));
    }
    let _e100 = boosted;
    return _e100;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e82 = (*c);
    (*c) = max(_e82, vec3<f32>(0f, 0f, 0f));
    let _e84 = (*c);
    cutoff = (_e84 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e86 = (*c);
    lo = (_e86 / vec3(12.92f));
    let _e89 = (*c);
    hi = pow(((_e89 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e94 = hi;
    let _e95 = lo;
    let _e96 = cutoff;
    return mix(_e94, _e95, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e96));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param_5: vec3<f32>;
    var param_6: vec3<f32>;

    let _e84 = (*role);
    let _e86 = (*role);
    let _e91 = unnamed.packed_indices[(_e84 / 4u)][(_e86 % 4u)];
    let _e94 = (*role);
    let _e96 = (*role);
    let _e101 = unnamed.packed_indices[(_e94 / 4u)][(_e96 % 4u)];
    let _e106 = (*uv);
    let _e107 = textureSample(wired_bindless_images[(_e91 & 4095u)], wired_bindless_samplers[((_e101 >> bitcast<u32>(12i)) & 255u)], _e106);
    c_1 = _e107;
    let _e108 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e108))) == 0i) {
        let _e113 = c_1;
        param_5 = _e113.xyz;
        let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_5));
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = (*slot);
    if (lightmap_slot == (_e122 + 1i)) {
        let _e125 = c_1;
        param_6 = _e125.xyz;
        let _e127 = wired_boost_legacy_lightmap_u0028_vf3_u003b((&param_6));
        c_1[0u] = _e127.x;
        c_1[1u] = _e127.y;
        c_1[2u] = _e127.z;
    }
    let _e134 = c_1;
    return _e134;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e102 = unnamed.packed_indices[0i][3u];
    let _e108 = unnamed.packed_indices[0i][3u];
    let _e113 = fog_tex_coord_1;
    let _e114 = textureSample(wired_bindless_images[(_e102 & 4095u)], wired_bindless_samplers[((_e108 >> bitcast<u32>(12i)) & 255u)], _e113);
    fog = _e114;
    let _e115 = frag_color0In_1;
    param_7 = _e115.xyz;
    let _e117 = sRGBToLinear_u0028_vf3_u003b((&param_7));
    let _e119 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e117.x, _e117.y, _e117.z, _e119);
    param_8 = 0u;
    let _e124 = frag_tex_coord0_1;
    param_9 = _e124;
    param_10 = 0i;
    let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
    sampledColor0_ = _e125;
    param_11 = 1u;
    let _e126 = frag_tex_coord1_1;
    param_12 = _e126;
    param_13 = 1i;
    let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
    sampledColor1_ = _e127;
    let _e128 = sampledColor0_;
    let _e129 = frag_color0_;
    color0_ = (_e128 * _e129);
    if override_type_3_ {
        let _e131 = sampledColor1_;
        color1_ = _e131;
        let _e132 = color0_;
        let _e134 = color1_;
        let _e136 = (_e132.xyz + _e134.xyz);
        let _e138 = color0_[3u];
        let _e140 = color1_[3u];
        base = vec4<f32>(_e136.x, _e136.y, _e136.z, (_e138 * _e140));
    } else {
        if override_type_3_1 {
            let _e146 = sampledColor1_;
            let _e147 = frag_color0_;
            color1_1 = (_e146 * _e147);
            let _e149 = color0_;
            let _e151 = color1_1;
            let _e153 = (_e149.xyz + _e151.xyz);
            let _e155 = color0_[3u];
            let _e157 = color1_1[3u];
            base = vec4<f32>(_e153.x, _e153.y, _e153.z, (_e155 * _e157));
        } else {
            let _e163 = sampledColor1_;
            color1_2 = _e163;
            let _e164 = color0_;
            let _e166 = color1_2;
            let _e168 = (_e164.xyz * _e166.xyz);
            base[0u] = _e168.x;
            base[1u] = _e168.y;
            base[2u] = _e168.z;
            let _e176 = color0_[3u];
            let _e178 = color1_2[3u];
            base[3u] = (_e176 * _e178);
        }
    }
    if override_type_3_2 {
        let _e183 = unnamed.worldLightParams[1u];
        wetness = clamp(_e183, 0f, 1f);
        let _e187 = unnamed.worldLightParams[2u];
        frost = clamp(_e187, 0f, 1f);
        let _e189 = base;
        luminance = dot(_e189.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e192 = wetness;
        let _e194 = base;
        let _e196 = (_e194.xyz * mix(1f, 0.82f, _e192));
        base[0u] = _e196.x;
        base[1u] = _e196.y;
        base[2u] = _e196.z;
        let _e203 = base;
        let _e205 = luminance;
        let _e207 = luminance;
        let _e209 = luminance;
        let _e211 = frost;
        let _e214 = mix(_e203.xyz, vec3<f32>((_e205 * 0.88f), (_e207 * 0.94f), _e209), vec3((_e211 * 0.55f)));
        base[0u] = _e214.x;
        base[1u] = _e214.y;
        base[2u] = _e214.z;
    }
    let _e223 = unnamed.materialFactors[0u];
    if (_e223 >= 0f) {
        let _e226 = unnamed.materialBaseColor;
        let _e227 = base;
        base = (_e227 * _e226);
    }
    let _e229 = color0_;
    let _e232 = unnamed.emissionRadiance;
    let _e235 = base;
    let _e237 = (_e235.xyz + (_e229.xyz * _e232.xyz));
    base[0u] = _e237.x;
    base[1u] = _e237.y;
    base[2u] = _e237.z;
    let _e244 = wired_advanced_fog_enabled_u0028_();
    if _e244 {
        let _e245 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e245;
        if override_type_3_3 {
            let _e246 = fogAmount;
            let _e248 = base;
            let _e250 = (_e248.xyz * (1f - _e246));
            base[0u] = _e250.x;
            base[1u] = _e250.y;
            base[2u] = _e250.z;
        } else {
            if override_type_3_4 {
                let _e257 = fogAmount;
                let _e259 = base;
                base = (_e259 * (1f - _e257));
            } else {
                if override_type_3_5 {
                    let _e261 = fogAmount;
                    let _e264 = base[3u];
                    base[3u] = (_e264 * (1f - _e261));
                } else {
                    let _e267 = base;
                    let _e270 = unnamed.advancedFogColorDensity;
                    let _e272 = fogAmount;
                    let _e274 = mix(_e267.xyz, _e270.xyz, vec3(_e272));
                    base[0u] = _e274.x;
                    base[1u] = _e274.y;
                    base[2u] = _e274.z;
                }
            }
        }
    } else {
        if override_type_3_6 {
            let _e281 = base;
            let _e284 = fog[3u];
            let _e286 = (_e281.xyz * (1f - _e284));
            base[0u] = _e286.x;
            base[1u] = _e286.y;
            base[2u] = _e286.z;
        } else {
            if override_type_3_7 {
                let _e293 = base;
                let _e295 = fog[3u];
                base = (_e293 * (1f - _e295));
            } else {
                if override_type_3_8 {
                    let _e299 = base[3u];
                    let _e301 = fog[3u];
                    base[3u] = (_e299 * (1f - _e301));
                } else {
                    let _e305 = base;
                    let _e306 = fog;
                    let _e308 = unnamed.fogColor;
                    let _e311 = fog[3u];
                    base = mix(_e305, (_e306 * _e308), vec4(_e311));
                }
            }
        }
    }
    if override_type_3_9 {
        let _e315 = base[3u];
        if (_e315 == 0f) {
            discard;
        }
    } else {
        if override_type_3_10 {
            let _e317 = base;
            let _e319 = base;
            if (dot(_e317.xyz, _e319.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e323 = base;
    out_color = _e323;
    param_14 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_14));
    return;
}

@fragment
fn main(@location(12) @interpolate(flat) temporalOutcome: u32, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>) -> FragmentOutput {
    temporalOutcome_1 = temporalOutcome;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
