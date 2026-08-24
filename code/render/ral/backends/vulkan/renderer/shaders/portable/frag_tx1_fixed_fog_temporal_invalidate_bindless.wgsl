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
@id(8) override identity_color: f32 = 1f;
@id(9) override identity_alpha: f32 = 1f;
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
    var frag_color: vec4<f32>;
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

    let _e87 = unnamed.packed_indices[0i][3u];
    let _e93 = unnamed.packed_indices[0i][3u];
    let _e98 = fog_tex_coord_1;
    let _e99 = textureSample(wired_bindless_images[(_e87 & 4095u)], wired_bindless_samplers[((_e93 >> bitcast<u32>(12i)) & 255u)], _e98);
    fog = _e99;
    frag_color[0u] = identity_color;
    frag_color[1u] = identity_color;
    frag_color[2u] = identity_color;
    frag_color[3u] = identity_alpha;
    param_1 = 0u;
    let _e104 = frag_tex_coord0_1;
    param_2 = _e104;
    param_3 = 0i;
    let _e105 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_1), (&param_2), (&param_3));
    let _e106 = frag_color;
    color0_ = (_e105 * _e106);
    if override_type_4_ {
        param_4 = 1u;
        let _e108 = frag_tex_coord1_1;
        param_5 = _e108;
        param_6 = 1i;
        let _e109 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_4), (&param_5), (&param_6));
        color1_ = _e109;
        let _e110 = color0_;
        let _e112 = color1_;
        let _e114 = (_e110.xyz + _e112.xyz);
        let _e116 = color0_[3u];
        let _e118 = color1_[3u];
        base = vec4<f32>(_e114.x, _e114.y, _e114.z, (_e116 * _e118));
    } else {
        if override_type_4_1 {
            param_7 = 1u;
            let _e124 = frag_tex_coord1_1;
            param_8 = _e124;
            param_9 = 1i;
            let _e125 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_7), (&param_8), (&param_9));
            let _e126 = frag_color;
            color1_1 = (_e125 * _e126);
            let _e128 = color0_;
            let _e130 = color1_1;
            let _e132 = (_e128.xyz + _e130.xyz);
            let _e134 = color0_[3u];
            let _e136 = color1_1[3u];
            base = vec4<f32>(_e132.x, _e132.y, _e132.z, (_e134 * _e136));
        } else {
            param_10 = 1u;
            let _e142 = frag_tex_coord1_1;
            param_11 = _e142;
            param_12 = 1i;
            let _e143 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_10), (&param_11), (&param_12));
            let _e144 = frag_color;
            color1_2 = (_e143 * _e144);
            let _e146 = color0_;
            let _e148 = color1_2;
            let _e150 = (_e146.xyz * _e148.xyz);
            base[0u] = _e150.x;
            base[1u] = _e150.y;
            base[2u] = _e150.z;
            let _e158 = color0_[3u];
            let _e160 = color1_2[3u];
            base[3u] = (_e158 * _e160);
        }
    }
    let _e163 = wired_advanced_fog_enabled_u0028_();
    if _e163 {
        let _e164 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e164;
        if override_type_4_2 {
            let _e165 = fogAmount;
            let _e167 = base;
            let _e169 = (_e167.xyz * (1f - _e165));
            base[0u] = _e169.x;
            base[1u] = _e169.y;
            base[2u] = _e169.z;
        } else {
            if override_type_4_3 {
                let _e176 = fogAmount;
                let _e178 = base;
                base = (_e178 * (1f - _e176));
            } else {
                if override_type_4_4 {
                    let _e180 = fogAmount;
                    let _e183 = base[3u];
                    base[3u] = (_e183 * (1f - _e180));
                } else {
                    let _e186 = base;
                    let _e189 = unnamed.advancedFogColorDensity;
                    let _e191 = fogAmount;
                    let _e193 = mix(_e186.xyz, _e189.xyz, vec3(_e191));
                    base[0u] = _e193.x;
                    base[1u] = _e193.y;
                    base[2u] = _e193.z;
                }
            }
        }
    } else {
        if override_type_4_5 {
            let _e200 = base;
            let _e203 = fog[3u];
            let _e205 = (_e200.xyz * (1f - _e203));
            base[0u] = _e205.x;
            base[1u] = _e205.y;
            base[2u] = _e205.z;
        } else {
            if override_type_4_6 {
                let _e212 = base;
                let _e214 = fog[3u];
                base = (_e212 * (1f - _e214));
            } else {
                if override_type_4_7 {
                    let _e218 = base[3u];
                    let _e220 = fog[3u];
                    base[3u] = (_e218 * (1f - _e220));
                } else {
                    let _e224 = base;
                    let _e225 = fog;
                    let _e227 = unnamed.fogColor;
                    let _e230 = fog[3u];
                    base = mix(_e224, (_e225 * _e227), vec4(_e230));
                }
            }
        }
    }
    if override_type_4_8 {
        let _e234 = base[3u];
        if (_e234 == 0f) {
            discard;
        }
    } else {
        if override_type_4_9 {
            let _e236 = base;
            let _e238 = base;
            if (dot(_e236.xyz, _e238.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e242 = base;
    out_color = _e242;
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
