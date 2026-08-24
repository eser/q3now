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
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
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
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
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
    var frag_color1_: vec4<f32>;
    var param_2: vec3<f32>;
    var color0_: vec4<f32>;
    var param_3: u32;
    var param_4: vec2<f32>;
    var param_5: i32;
    var color1_: vec4<f32>;
    var param_6: u32;
    var param_7: vec2<f32>;
    var param_8: i32;
    var base: vec4<f32>;
    var color1_1: vec4<f32>;
    var param_9: u32;
    var param_10: vec2<f32>;
    var param_11: i32;
    var color1_2: vec4<f32>;
    var param_12: u32;
    var param_13: vec2<f32>;
    var param_14: i32;
    var color1_3: vec4<f32>;
    var param_15: u32;
    var param_16: vec2<f32>;
    var param_17: i32;
    var color1_4: vec4<f32>;
    var param_18: u32;
    var param_19: vec2<f32>;
    var param_20: i32;
    var color1_5: vec4<f32>;
    var param_21: u32;
    var param_22: vec2<f32>;
    var param_23: i32;
    var color1_6: vec4<f32>;
    var param_24: u32;
    var param_25: vec2<f32>;
    var param_26: i32;
    var fogAmount: f32;
    var param_27: f32;

    let _e103 = frag_color0In_1;
    param_1 = _e103.xyz;
    let _e105 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e107 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e105.x, _e105.y, _e105.z, _e107);
    let _e112 = frag_color1In_1;
    param_2 = _e112.xyz;
    let _e114 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e116 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e114.x, _e114.y, _e114.z, _e116);
    param_3 = 0u;
    let _e121 = frag_tex_coord0_1;
    param_4 = _e121;
    param_5 = 0i;
    let _e122 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e123 = frag_color0_;
    color0_ = (_e122 * _e123);
    if override_type_4_2 {
        param_6 = 1u;
        let _e125 = frag_tex_coord1_1;
        param_7 = _e125;
        param_8 = 1i;
        let _e126 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e127 = frag_color1_;
        color1_ = (_e126 * _e127);
        let _e129 = color0_;
        let _e131 = color1_;
        let _e133 = (_e129.xyz + _e131.xyz);
        let _e135 = color0_[3u];
        let _e137 = color1_[3u];
        base = vec4<f32>(_e133.x, _e133.y, _e133.z, (_e135 * _e137));
    } else {
        if override_type_4_3 {
            param_9 = 1u;
            let _e143 = frag_tex_coord1_1;
            param_10 = _e143;
            param_11 = 1i;
            let _e144 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e145 = frag_color1_;
            color1_1 = (_e144 * _e145);
            let _e148 = color0_[3u];
            let _e149 = color0_;
            color0_ = (_e149 * _e148);
            let _e152 = color1_1[3u];
            let _e153 = color1_1;
            color1_1 = (_e153 * _e152);
            let _e155 = color0_;
            let _e157 = color1_1;
            let _e159 = (_e155.xyz + _e157.xyz);
            let _e161 = color0_[3u];
            let _e163 = color1_1[3u];
            base = vec4<f32>(_e159.x, _e159.y, _e159.z, (_e161 * _e163));
        } else {
            if override_type_4_4 {
                param_12 = 1u;
                let _e169 = frag_tex_coord1_1;
                param_13 = _e169;
                param_14 = 1i;
                let _e170 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e171 = frag_color1_;
                color1_2 = (_e170 * _e171);
                let _e174 = color0_[3u];
                let _e176 = color0_;
                color0_ = (_e176 * (1f - _e174));
                let _e179 = color1_2[3u];
                let _e181 = color1_2;
                color1_2 = (_e181 * (1f - _e179));
                let _e183 = color0_;
                let _e185 = color1_2;
                let _e187 = (_e183.xyz + _e185.xyz);
                let _e189 = color0_[3u];
                let _e191 = color1_2[3u];
                base = vec4<f32>(_e187.x, _e187.y, _e187.z, (_e189 * _e191));
            } else {
                if override_type_4_5 {
                    param_15 = 1u;
                    let _e197 = frag_tex_coord1_1;
                    param_16 = _e197;
                    param_17 = 1i;
                    let _e198 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e199 = frag_color1_;
                    color1_3 = (_e198 * _e199);
                    let _e201 = color0_;
                    let _e202 = color1_3;
                    let _e204 = color1_3[3u];
                    base = mix(_e201, _e202, vec4(_e204));
                } else {
                    if override_type_4_6 {
                        param_18 = 1u;
                        let _e207 = frag_tex_coord1_1;
                        param_19 = _e207;
                        param_20 = 1i;
                        let _e208 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e209 = frag_color1_;
                        color1_4 = (_e208 * _e209);
                        let _e211 = color1_4;
                        let _e212 = color0_;
                        let _e214 = color1_4[3u];
                        base = mix(_e211, _e212, vec4(_e214));
                    } else {
                        if override_type_4_7 {
                            param_21 = 1u;
                            let _e217 = frag_tex_coord1_1;
                            param_22 = _e217;
                            param_23 = 1i;
                            let _e218 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e219 = frag_color1_;
                            color1_5 = (_e218 * _e219);
                            let _e221 = color1_5;
                            let _e223 = color1_5[3u];
                            let _e226 = color0_;
                            base = ((_e221 + vec4(_e223)) * _e226);
                        } else {
                            param_24 = 1u;
                            let _e228 = frag_tex_coord1_1;
                            param_25 = _e228;
                            param_26 = 1i;
                            let _e229 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e230 = frag_color1_;
                            color1_6 = (_e229 * _e230);
                            let _e232 = color0_;
                            let _e234 = color1_6;
                            let _e236 = (_e232.xyz * _e234.xyz);
                            base[0u] = _e236.x;
                            base[1u] = _e236.y;
                            base[2u] = _e236.z;
                            let _e244 = color0_[3u];
                            let _e246 = color1_6[3u];
                            base[3u] = (_e244 * _e246);
                        }
                    }
                }
            }
        }
    }
    let _e249 = wired_advanced_fog_enabled_u0028_();
    if _e249 {
        let _e250 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e250;
        let _e251 = base;
        let _e254 = unnamed.advancedFogColorDensity;
        let _e256 = fogAmount;
        let _e258 = mix(_e251.xyz, _e254.xyz, vec3(_e256));
        base[0u] = _e258.x;
        base[1u] = _e258.y;
        base[2u] = _e258.z;
    }
    if override_type_4_8 {
        let _e266 = base[3u];
        if (_e266 == 0f) {
            discard;
        }
    } else {
        if override_type_4_9 {
            let _e268 = base;
            let _e270 = base;
            if (dot(_e268.xyz, _e270.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e274 = base;
    out_color = _e274;
    param_27 = 1f;
    wiredTemporalWriteAux_u0028_f1_u003b((&param_27));
    return;
}

@fragment
fn main(@builtin(position) gl_FragCoord: vec4<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    gl_FragCoord_1 = gl_FragCoord;
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
    frag_tex_coord0_1 = frag_tex_coord0_;
    frag_tex_coord1_1 = frag_tex_coord1_;
    temporalCurrentClip_1 = temporalCurrentClip;
    temporalPreviousClip_1 = temporalPreviousClip;
    temporalOutcome_1 = temporalOutcome;
    main_1();
    let _e19 = out_temporal_velocity;
    let _e20 = out_temporal_validity;
    let _e21 = out_color;
    return FragmentOutput(_e19, _e20, _e21);
}
