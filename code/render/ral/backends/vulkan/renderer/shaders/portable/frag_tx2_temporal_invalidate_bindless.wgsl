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
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_4_: bool = (tex_mode == 1i);
override override_type_4_1: bool = (tex_mode == 2i);
override override_type_4_2: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_3: bool = (discard_mode == 1i);
override override_type_4_4: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
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

fn wiredTemporalWriteAux_u0028_f1_u003b(coverageConfidence: ptr<function, f32>) {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn wired_advanced_fog_enabled_u0028_() -> bool {
    var fogType: i32;

    let _e67 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e67 + 0.5f));
    let _e72 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e74 = fogType;
    let _e77 = fogType;
    return (((_e72 > 0.5f) && (_e74 >= 1i)) && (_e77 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e67 = wired_advanced_fog_enabled_u0028_();
    if !(_e67) {
        return 0f;
    }
    let _e70 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e70, 0.000001f));
    let _e75 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e75 + 0.5f));
    let _e78 = fogType_1;
    if (_e78 == 1i) {
        let _e82 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e82 <= 0f) {
            return 0f;
        }
        let _e84 = viewDepth;
        let _e87 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e84 / _e87), 0f, 1f);
    }
    let _e92 = unnamed.advancedFogColorDensity[3u];
    let _e94 = viewDepth;
    opticalDepth = (max(_e92, 0f) * _e94);
    let _e96 = fogType_1;
    if (_e96 == 2i) {
        let _e98 = opticalDepth;
        return clamp((1f - exp(-(_e98))), 0f, 1f);
    }
    let _e103 = opticalDepth;
    let _e104 = opticalDepth;
    return clamp((1f - exp(-((_e103 * _e104)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e68 = (*c);
    (*c) = max(_e68, vec3<f32>(0f, 0f, 0f));
    let _e70 = (*c);
    cutoff = (_e70 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e72 = (*c);
    lo = (_e72 / vec3(12.92f));
    let _e75 = (*c);
    hi = pow(((_e75 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e80 = hi;
    let _e81 = lo;
    let _e82 = cutoff;
    return mix(_e80, _e81, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e82));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e69 = (*role);
    let _e71 = (*role);
    let _e76 = unnamed.packed_indices[(_e69 / 4u)][(_e71 % 4u)];
    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e91 = (*uv);
    let _e92 = textureSample(wired_bindless_images[(_e76 & 4095u)], wired_bindless_samplers[((_e86 >> bitcast<u32>(12i)) & 255u)], _e91);
    c_1 = _e92;
    let _e93 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e93))) == 0i) {
        let _e98 = c_1;
        param = _e98.xyz;
        let _e100 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e100.x;
        c_1[1u] = _e100.y;
        c_1[2u] = _e100.z;
    }
    let _e107 = (*slot);
    if (lightmap_slot == (_e107 + 1i)) {
        let _e112 = unnamed.worldLightParams[0u];
        let _e113 = c_1;
        let _e115 = (_e113.xyz * _e112);
        c_1[0u] = _e115.x;
        c_1[1u] = _e115.y;
        c_1[2u] = _e115.z;
    }
    let _e122 = c_1;
    return _e122;
}

fn main_1() {
    var frag_color0_: vec4<f32>;
    var param_1: vec3<f32>;
    var color0_: vec4<f32>;
    var param_2: u32;
    var param_3: vec2<f32>;
    var param_4: i32;
    var color1_: vec4<f32>;
    var param_5: u32;
    var param_6: vec2<f32>;
    var param_7: i32;
    var color2_: vec4<f32>;
    var param_8: u32;
    var param_9: vec2<f32>;
    var param_10: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_11: u32;
    var param_12: vec2<f32>;
    var param_13: i32;
    var color2_1: vec4<f32>;
    var param_14: u32;
    var param_15: vec2<f32>;
    var param_16: i32;
    var color1_2: vec4<f32>;
    var param_17: u32;
    var param_18: vec2<f32>;
    var param_19: i32;
    var color2_2: vec4<f32>;
    var param_20: u32;
    var param_21: vec2<f32>;
    var param_22: i32;
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_23: f32;

    let _e100 = frag_color0In_1;
    param_1 = _e100.xyz;
    let _e102 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e104 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e102.x, _e102.y, _e102.z, _e104);
    param_2 = 0u;
    let _e109 = frag_tex_coord0_1;
    param_3 = _e109;
    param_4 = 0i;
    let _e110 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e111 = frag_color0_;
    color0_ = (_e110 * _e111);
    if override_type_4_ {
        param_5 = 1u;
        let _e113 = frag_tex_coord1_1;
        param_6 = _e113;
        param_7 = 1i;
        let _e114 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e114;
        param_8 = 2u;
        let _e115 = frag_tex_coord2_1;
        param_9 = _e115;
        param_10 = 2i;
        let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e116;
        let _e117 = color0_;
        let _e119 = color1_;
        let _e122 = color2_;
        let _e124 = ((_e117.xyz + _e119.xyz) + _e122.xyz);
        let _e126 = color0_[3u];
        let _e128 = color1_[3u];
        let _e131 = color2_[3u];
        base = vec4<f32>(_e124.x, _e124.y, _e124.z, ((_e126 * _e128) * _e131));
    } else {
        if override_type_4_1 {
            param_11 = 1u;
            let _e137 = frag_tex_coord1_1;
            param_12 = _e137;
            param_13 = 1i;
            let _e138 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e139 = frag_color0_;
            color1_1 = (_e138 * _e139);
            param_14 = 2u;
            let _e141 = frag_tex_coord2_1;
            param_15 = _e141;
            param_16 = 2i;
            let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
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
            param_17 = 1u;
            let _e165 = frag_tex_coord1_1;
            param_18 = _e165;
            param_19 = 1i;
            let _e166 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e166;
            param_20 = 2u;
            let _e167 = frag_tex_coord2_1;
            param_21 = _e167;
            param_22 = 2i;
            let _e168 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e168;
            let _e169 = color0_;
            let _e171 = color1_2;
            let _e174 = color2_2;
            let _e176 = ((_e169.xyz * _e171.xyz) * _e174.xyz);
            base[0u] = _e176.x;
            base[1u] = _e176.y;
            base[2u] = _e176.z;
            let _e184 = color0_[3u];
            let _e186 = color1_2[3u];
            let _e189 = color2_2[3u];
            base[3u] = ((_e184 * _e186) * _e189);
        }
    }
    if override_type_4_2 {
        let _e194 = unnamed.worldLightParams[1u];
        wetness = clamp(_e194, 0f, 1f);
        let _e198 = unnamed.worldLightParams[2u];
        frost = clamp(_e198, 0f, 1f);
        let _e200 = base;
        luminance = dot(_e200.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e203 = wetness;
        let _e205 = base;
        let _e207 = (_e205.xyz * mix(1f, 0.82f, _e203));
        base[0u] = _e207.x;
        base[1u] = _e207.y;
        base[2u] = _e207.z;
        let _e214 = base;
        let _e216 = luminance;
        let _e218 = luminance;
        let _e220 = luminance;
        let _e222 = frost;
        let _e225 = mix(_e214.xyz, vec3<f32>((_e216 * 0.88f), (_e218 * 0.94f), _e220), vec3((_e222 * 0.55f)));
        base[0u] = _e225.x;
        base[1u] = _e225.y;
        base[2u] = _e225.z;
    }
    let _e232 = color0_;
    let _e235 = unnamed.emissionRadiance;
    let _e238 = base;
    let _e240 = (_e238.xyz + (_e232.xyz * _e235.xyz));
    base[0u] = _e240.x;
    base[1u] = _e240.y;
    base[2u] = _e240.z;
    let _e247 = wired_advanced_fog_enabled_u0028_();
    if _e247 {
        let _e248 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e248;
        let _e249 = base;
        let _e252 = unnamed.advancedFogColorDensity;
        let _e254 = fogAmount;
        let _e256 = mix(_e249.xyz, _e252.xyz, vec3(_e254));
        base[0u] = _e256.x;
        base[1u] = _e256.y;
        base[2u] = _e256.z;
    }
    if override_type_4_3 {
        let _e264 = base[3u];
        if (_e264 == 0f) {
            discard;
        }
    } else {
        if override_type_4_4 {
            let _e266 = base;
            let _e268 = base;
            if (dot(_e266.xyz, _e268.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e272 = base;
    out_color = _e272;
    param_23 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_23));
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
