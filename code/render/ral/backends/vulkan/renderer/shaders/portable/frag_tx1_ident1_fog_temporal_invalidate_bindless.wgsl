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
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
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

    let _e64 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e64 + 0.5f));
    let _e69 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e71 = fogType;
    let _e74 = fogType;
    return (((_e69 > 0.5f) && (_e71 >= 1i)) && (_e74 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e64 = wired_advanced_fog_enabled_u0028_();
    if !(_e64) {
        return 0f;
    }
    let _e67 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e67, 0.000001f));
    let _e72 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e72 + 0.5f));
    let _e75 = fogType_1;
    if (_e75 == 1i) {
        let _e79 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e79 <= 0f) {
            return 0f;
        }
        let _e81 = viewDepth;
        let _e84 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e81 / _e84), 0f, 1f);
    }
    let _e89 = unnamed.advancedFogColorDensity[3u];
    let _e91 = viewDepth;
    opticalDepth = (max(_e89, 0f) * _e91);
    let _e93 = fogType_1;
    if (_e93 == 2i) {
        let _e95 = opticalDepth;
        return clamp((1f - exp(-(_e95))), 0f, 1f);
    }
    let _e100 = opticalDepth;
    let _e101 = opticalDepth;
    return clamp((1f - exp(-((_e100 * _e101)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e65 = (*c);
    (*c) = max(_e65, vec3<f32>(0f, 0f, 0f));
    let _e67 = (*c);
    cutoff = (_e67 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e69 = (*c);
    lo = (_e69 / vec3(12.92f));
    let _e72 = (*c);
    hi = pow(((_e72 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e77 = hi;
    let _e78 = lo;
    let _e79 = cutoff;
    return mix(_e77, _e78, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e79));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e66 = (*role);
    let _e68 = (*role);
    let _e73 = unnamed.packed_indices[(_e66 / 4u)][(_e68 % 4u)];
    let _e76 = (*role);
    let _e78 = (*role);
    let _e83 = unnamed.packed_indices[(_e76 / 4u)][(_e78 % 4u)];
    let _e88 = (*uv);
    let _e89 = textureSample(wired_bindless_images[(_e73 & 4095u)], wired_bindless_samplers[((_e83 >> bitcast<u32>(12i)) & 255u)], _e88);
    c_1 = _e89;
    let _e90 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e90))) == 0i) {
        let _e95 = c_1;
        param = _e95.xyz;
        let _e97 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e97.x;
        c_1[1u] = _e97.y;
        c_1[2u] = _e97.z;
    }
    let _e104 = (*slot);
    if (lightmap_slot == (_e104 + 1i)) {
        let _e109 = unnamed.worldLightParams[0u];
        let _e110 = c_1;
        let _e112 = (_e110.xyz * _e109);
        c_1[0u] = _e112.x;
        c_1[1u] = _e112.y;
        c_1[2u] = _e112.z;
    }
    let _e119 = c_1;
    return _e119;
}

fn main_1() {
    var fog: vec4<f32>;
    var color0_: vec4<f32>;
    var param_1: u32;
    var param_2: vec2<f32>;
    var param_3: i32;
    var color1_: vec4<f32>;
    var param_4: u32;
    var param_5: vec2<f32>;
    var param_6: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_7: u32;
    var param_8: vec2<f32>;
    var param_9: i32;
    var color1_2: vec4<f32>;
    var param_10: u32;
    var param_11: vec2<f32>;
    var param_12: i32;
    var fogAmount: f32;
    var param_13: f32;

    let _e84 = unnamed.packed_indices[0i][3u];
    let _e90 = unnamed.packed_indices[0i][3u];
    let _e95 = fog_tex_coord_1;
    let _e96 = textureSample(wired_bindless_images[(_e84 & 4095u)], wired_bindless_samplers[((_e90 >> bitcast<u32>(12i)) & 255u)], _e95);
    fog = _e96;
    param_1 = 0u;
    let _e97 = frag_tex_coord0_1;
    param_2 = _e97;
    param_3 = 0i;
    let _e98 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    color0_ = _e98;
    if override_type_4_ {
        param_4 = 1u;
        let _e99 = frag_tex_coord1_1;
        param_5 = _e99;
        param_6 = 1i;
        let _e100 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e100;
        let _e101 = color0_;
        let _e103 = color1_;
        let _e105 = (_e101.xyz + _e103.xyz);
        let _e107 = color0_[3u];
        let _e109 = color1_[3u];
        base = vec4<f32>(_e105.x, _e105.y, _e105.z, (_e107 * _e109));
    } else {
        if override_type_4_1 {
            param_7 = 1u;
            let _e115 = frag_tex_coord1_1;
            param_8 = _e115;
            param_9 = 1i;
            let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            color1_1 = _e116;
            let _e117 = color0_;
            let _e119 = color1_1;
            let _e121 = (_e117.xyz + _e119.xyz);
            let _e123 = color0_[3u];
            let _e125 = color1_1[3u];
            base = vec4<f32>(_e121.x, _e121.y, _e121.z, (_e123 * _e125));
        } else {
            param_10 = 1u;
            let _e131 = frag_tex_coord1_1;
            param_11 = _e131;
            param_12 = 1i;
            let _e132 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            color1_2 = _e132;
            let _e133 = color0_;
            let _e135 = color1_2;
            let _e137 = (_e133.xyz * _e135.xyz);
            base[0u] = _e137.x;
            base[1u] = _e137.y;
            base[2u] = _e137.z;
            let _e145 = color0_[3u];
            let _e147 = color1_2[3u];
            base[3u] = (_e145 * _e147);
        }
    }
    let _e150 = wired_advanced_fog_enabled_u0028_();
    if _e150 {
        let _e151 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e151;
        if override_type_4_2 {
            let _e152 = fogAmount;
            let _e154 = base;
            let _e156 = (_e154.xyz * (1f - _e152));
            base[0u] = _e156.x;
            base[1u] = _e156.y;
            base[2u] = _e156.z;
        } else {
            if override_type_4_3 {
                let _e163 = fogAmount;
                let _e165 = base;
                base = (_e165 * (1f - _e163));
            } else {
                if override_type_4_4 {
                    let _e167 = fogAmount;
                    let _e170 = base[3u];
                    base[3u] = (_e170 * (1f - _e167));
                } else {
                    let _e173 = base;
                    let _e176 = unnamed.advancedFogColorDensity;
                    let _e178 = fogAmount;
                    let _e180 = mix(_e173.xyz, _e176.xyz, vec3(_e178));
                    base[0u] = _e180.x;
                    base[1u] = _e180.y;
                    base[2u] = _e180.z;
                }
            }
        }
    } else {
        if override_type_4_5 {
            let _e187 = base;
            let _e190 = fog[3u];
            let _e192 = (_e187.xyz * (1f - _e190));
            base[0u] = _e192.x;
            base[1u] = _e192.y;
            base[2u] = _e192.z;
        } else {
            if override_type_4_6 {
                let _e199 = base;
                let _e201 = fog[3u];
                base = (_e199 * (1f - _e201));
            } else {
                if override_type_4_7 {
                    let _e205 = base[3u];
                    let _e207 = fog[3u];
                    base[3u] = (_e205 * (1f - _e207));
                } else {
                    let _e211 = base;
                    let _e212 = fog;
                    let _e214 = unnamed.fogColor;
                    let _e217 = fog[3u];
                    base = mix(_e211, (_e212 * _e214), vec4(_e217));
                }
            }
        }
    }
    if override_type_4_8 {
        let _e221 = base[3u];
        if (_e221 == 0f) {
            discard;
        }
    } else {
        if override_type_4_9 {
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
    wiredTemporalWriteAux_u0028_f1_u003b((&param_13));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(4) fog_tex_coord: vec2<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    fog_tex_coord_1 = fog_tex_coord;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e17 = out_temporal_velocity;
    let _e18 = out_temporal_validity;
    let _e19 = out_color;
    return FragmentOutput(_e17, _e18, _e19);
}
