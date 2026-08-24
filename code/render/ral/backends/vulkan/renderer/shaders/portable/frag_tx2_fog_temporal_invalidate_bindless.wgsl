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
@id(10) override acff: i32 = 0i;
override override_type_4_2: bool = (acff == 1i);
override override_type_4_3: bool = (acff == 2i);
override override_type_4_4: bool = (acff == 3i);
override override_type_4_5: bool = (acff == 1i);
override override_type_4_6: bool = (acff == 2i);
override override_type_4_7: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_8: bool = (discard_mode == 1i);
override override_type_4_9: bool = (discard_mode == 2i);
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
var<private> fog_tex_coord_1: vec2<f32>;
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

    let _e66 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e66 + 0.5f));
    let _e71 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e73 = fogType;
    let _e76 = fogType;
    return (((_e71 > 0.5f) && (_e73 >= 1i)) && (_e76 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e66 = wired_advanced_fog_enabled_u0028_();
    if !(_e66) {
        return 0f;
    }
    let _e69 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e69, 0.000001f));
    let _e74 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e74 + 0.5f));
    let _e77 = fogType_1;
    if (_e77 == 1i) {
        let _e81 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e81 <= 0f) {
            return 0f;
        }
        let _e83 = viewDepth;
        let _e86 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e83 / _e86), 0f, 1f);
    }
    let _e91 = unnamed.advancedFogColorDensity[3u];
    let _e93 = viewDepth;
    opticalDepth = (max(_e91, 0f) * _e93);
    let _e95 = fogType_1;
    if (_e95 == 2i) {
        let _e97 = opticalDepth;
        return clamp((1f - exp(-(_e97))), 0f, 1f);
    }
    let _e102 = opticalDepth;
    let _e103 = opticalDepth;
    return clamp((1f - exp(-((_e102 * _e103)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e67 = (*c);
    (*c) = max(_e67, vec3<f32>(0f, 0f, 0f));
    let _e69 = (*c);
    cutoff = (_e69 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e71 = (*c);
    lo = (_e71 / vec3(12.92f));
    let _e74 = (*c);
    hi = pow(((_e74 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e79 = hi;
    let _e80 = lo;
    let _e81 = cutoff;
    return mix(_e79, _e80, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e81));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e68 = (*role);
    let _e70 = (*role);
    let _e75 = unnamed.packed_indices[(_e68 / 4u)][(_e70 % 4u)];
    let _e78 = (*role);
    let _e80 = (*role);
    let _e85 = unnamed.packed_indices[(_e78 / 4u)][(_e80 % 4u)];
    let _e90 = (*uv);
    let _e91 = textureSample(wired_bindless_images[(_e75 & 4095u)], wired_bindless_samplers[((_e85 >> bitcast<u32>(12i)) & 255u)], _e90);
    c_1 = _e91;
    let _e92 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e92))) == 0i) {
        let _e97 = c_1;
        param = _e97.xyz;
        let _e99 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e99.x;
        c_1[1u] = _e99.y;
        c_1[2u] = _e99.z;
    }
    let _e106 = (*slot);
    if (lightmap_slot == (_e106 + 1i)) {
        let _e111 = unnamed.worldLightParams[0u];
        let _e112 = c_1;
        let _e114 = (_e112.xyz * _e111);
        c_1[0u] = _e114.x;
        c_1[1u] = _e114.y;
        c_1[2u] = _e114.z;
    }
    let _e121 = c_1;
    return _e121;
}

fn main_1() {
    var fog: vec4<f32>;
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
    var fogAmount: f32;
    var param_23: f32;

    let _e100 = unnamed.packed_indices[0i][3u];
    let _e106 = unnamed.packed_indices[0i][3u];
    let _e111 = fog_tex_coord_1;
    let _e112 = textureSample(wired_bindless_images[(_e100 & 4095u)], wired_bindless_samplers[((_e106 >> bitcast<u32>(12i)) & 255u)], _e111);
    fog = _e112;
    let _e113 = frag_color0In_1;
    param_1 = _e113.xyz;
    let _e115 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e117 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e115.x, _e115.y, _e115.z, _e117);
    param_2 = 0u;
    let _e122 = frag_tex_coord0_1;
    param_3 = _e122;
    param_4 = 0i;
    let _e123 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_2), (&param_3), (&param_4));
    let _e124 = frag_color0_;
    color0_ = (_e123 * _e124);
    if override_type_4_ {
        param_5 = 1u;
        let _e126 = frag_tex_coord1_1;
        param_6 = _e126;
        param_7 = 1i;
        let _e127 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_5), (&param_6), (&param_7));
        color1_ = _e127;
        param_8 = 2u;
        let _e128 = frag_tex_coord2_1;
        param_9 = _e128;
        param_10 = 2i;
        let _e129 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_8), (&param_9), (&param_10));
        color2_ = _e129;
        let _e130 = color0_;
        let _e132 = color1_;
        let _e135 = color2_;
        let _e137 = ((_e130.xyz + _e132.xyz) + _e135.xyz);
        let _e139 = color0_[3u];
        let _e141 = color1_[3u];
        let _e144 = color2_[3u];
        base = vec4<f32>(_e137.x, _e137.y, _e137.z, ((_e139 * _e141) * _e144));
    } else {
        if override_type_4_1 {
            param_11 = 1u;
            let _e150 = frag_tex_coord1_1;
            param_12 = _e150;
            param_13 = 1i;
            let _e151 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_11), (&param_12), (&param_13));
            let _e152 = frag_color0_;
            color1_1 = (_e151 * _e152);
            param_14 = 2u;
            let _e154 = frag_tex_coord2_1;
            param_15 = _e154;
            param_16 = 2i;
            let _e155 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_14), (&param_15), (&param_16));
            let _e156 = frag_color0_;
            color2_1 = (_e155 * _e156);
            let _e158 = color0_;
            let _e160 = color1_1;
            let _e163 = color2_1;
            let _e165 = ((_e158.xyz + _e160.xyz) + _e163.xyz);
            let _e167 = color0_[3u];
            let _e169 = color1_1[3u];
            let _e172 = color2_1[3u];
            base = vec4<f32>(_e165.x, _e165.y, _e165.z, ((_e167 * _e169) * _e172));
        } else {
            param_17 = 1u;
            let _e178 = frag_tex_coord1_1;
            param_18 = _e178;
            param_19 = 1i;
            let _e179 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_17), (&param_18), (&param_19));
            color1_2 = _e179;
            param_20 = 2u;
            let _e180 = frag_tex_coord2_1;
            param_21 = _e180;
            param_22 = 2i;
            let _e181 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_20), (&param_21), (&param_22));
            color2_2 = _e181;
            let _e182 = color0_;
            let _e184 = color1_2;
            let _e187 = color2_2;
            let _e189 = ((_e182.xyz * _e184.xyz) * _e187.xyz);
            base[0u] = _e189.x;
            base[1u] = _e189.y;
            base[2u] = _e189.z;
            let _e197 = color0_[3u];
            let _e199 = color1_2[3u];
            let _e202 = color2_2[3u];
            base[3u] = ((_e197 * _e199) * _e202);
        }
    }
    let _e205 = wired_advanced_fog_enabled_u0028_();
    if _e205 {
        let _e206 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e206;
        if override_type_4_2 {
            let _e207 = fogAmount;
            let _e209 = base;
            let _e211 = (_e209.xyz * (1f - _e207));
            base[0u] = _e211.x;
            base[1u] = _e211.y;
            base[2u] = _e211.z;
        } else {
            if override_type_4_3 {
                let _e218 = fogAmount;
                let _e220 = base;
                base = (_e220 * (1f - _e218));
            } else {
                if override_type_4_4 {
                    let _e222 = fogAmount;
                    let _e225 = base[3u];
                    base[3u] = (_e225 * (1f - _e222));
                } else {
                    let _e228 = base;
                    let _e231 = unnamed.advancedFogColorDensity;
                    let _e233 = fogAmount;
                    let _e235 = mix(_e228.xyz, _e231.xyz, vec3(_e233));
                    base[0u] = _e235.x;
                    base[1u] = _e235.y;
                    base[2u] = _e235.z;
                }
            }
        }
    } else {
        if override_type_4_5 {
            let _e242 = base;
            let _e245 = fog[3u];
            let _e247 = (_e242.xyz * (1f - _e245));
            base[0u] = _e247.x;
            base[1u] = _e247.y;
            base[2u] = _e247.z;
        } else {
            if override_type_4_6 {
                let _e254 = base;
                let _e256 = fog[3u];
                base = (_e254 * (1f - _e256));
            } else {
                if override_type_4_7 {
                    let _e260 = base[3u];
                    let _e262 = fog[3u];
                    base[3u] = (_e260 * (1f - _e262));
                } else {
                    let _e266 = base;
                    let _e267 = fog;
                    let _e269 = unnamed.fogColor;
                    let _e272 = fog[3u];
                    base = mix(_e266, (_e267 * _e269), vec4(_e272));
                }
            }
        }
    }
    if override_type_4_8 {
        let _e276 = base[3u];
        if (_e276 == 0f) {
            discard;
        }
    } else {
        if override_type_4_9 {
            let _e278 = base;
            let _e280 = base;
            if (dot(_e278.xyz, _e280.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e284 = base;
    out_color = _e284;
    param_23 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_23));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(3) frag_tex_coord2_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_color0In_1 = frag_color0In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    frag_tex_coord2_1 = frag_tex_coord2_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e21 = out_temporal_velocity;
    let _e22 = out_temporal_validity;
    let _e23 = out_color;
    return FragmentOutput(_e21, _e22, _e23);
}
