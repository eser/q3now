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
override override_type_4_2: bool = (override_type_4_ || override_type_4_1);
override override_type_4_3: bool = (tex_mode == 3i);
override override_type_4_4: bool = (tex_mode == 4i);
override override_type_4_5: bool = (tex_mode == 5i);
override override_type_4_6: bool = (tex_mode == 6i);
override override_type_4_7: bool = (tex_mode == 7i);
override override_type_4_8: bool = (lightmap_slot != 0i);
@id(7) override discard_mode: i32 = 0i;
override override_type_4_9: bool = (discard_mode == 1i);
override override_type_4_10: bool = (discard_mode == 2i);
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

    let _e77 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType = i32((_e77 + 0.5f));
    let _e82 = unnamed.advancedFogTypeFarEnabled[2u];
    let _e84 = fogType;
    let _e87 = fogType;
    return (((_e82 > 0.5f) && (_e84 >= 1i)) && (_e87 <= 3i));
}

fn wired_advanced_fog_amount_u0028_() -> f32 {
    var viewDepth: f32;
    var fogType_1: i32;
    var opticalDepth: f32;

    let _e77 = wired_advanced_fog_enabled_u0028_();
    if !(_e77) {
        return 0f;
    }
    let _e80 = gl_FragCoord_1[3u];
    viewDepth = (1f / max(_e80, 0.000001f));
    let _e85 = unnamed.advancedFogTypeFarEnabled[0u];
    fogType_1 = i32((_e85 + 0.5f));
    let _e88 = fogType_1;
    if (_e88 == 1i) {
        let _e92 = unnamed.advancedFogTypeFarEnabled[1u];
        if (_e92 <= 0f) {
            return 0f;
        }
        let _e94 = viewDepth;
        let _e97 = unnamed.advancedFogTypeFarEnabled[1u];
        return clamp((_e94 / _e97), 0f, 1f);
    }
    let _e102 = unnamed.advancedFogColorDensity[3u];
    let _e104 = viewDepth;
    opticalDepth = (max(_e102, 0f) * _e104);
    let _e106 = fogType_1;
    if (_e106 == 2i) {
        let _e108 = opticalDepth;
        return clamp((1f - exp(-(_e108))), 0f, 1f);
    }
    let _e113 = opticalDepth;
    let _e114 = opticalDepth;
    return clamp((1f - exp(-((_e113 * _e114)))), 0f, 1f);
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e78 = (*c);
    (*c) = max(_e78, vec3<f32>(0f, 0f, 0f));
    let _e80 = (*c);
    cutoff = (_e80 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e82 = (*c);
    lo = (_e82 / vec3(12.92f));
    let _e85 = (*c);
    hi = pow(((_e85 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e90 = hi;
    let _e91 = lo;
    let _e92 = cutoff;
    return mix(_e90, _e91, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e92));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e79 = (*role);
    let _e81 = (*role);
    let _e86 = unnamed.packed_indices[(_e79 / 4u)][(_e81 % 4u)];
    let _e89 = (*role);
    let _e91 = (*role);
    let _e96 = unnamed.packed_indices[(_e89 / 4u)][(_e91 % 4u)];
    let _e101 = (*uv);
    let _e102 = textureSample(wired_bindless_images[(_e86 & 4095u)], wired_bindless_samplers[((_e96 >> bitcast<u32>(12i)) & 255u)], _e101);
    c_1 = _e102;
    let _e103 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e103))) == 0i) {
        let _e108 = c_1;
        param = _e108.xyz;
        let _e110 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e110.x;
        c_1[1u] = _e110.y;
        c_1[2u] = _e110.z;
    }
    let _e117 = (*slot);
    if (lightmap_slot == (_e117 + 1i)) {
        let _e122 = unnamed.worldLightParams[0u];
        let _e123 = c_1;
        let _e125 = (_e123.xyz * _e122);
        c_1[0u] = _e125.x;
        c_1[1u] = _e125.y;
        c_1[2u] = _e125.z;
    }
    let _e132 = c_1;
    return _e132;
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
    var wetness: f32;
    var frost: f32;
    var luminance: f32;
    var fogAmount: f32;
    var param_27: f32;

    let _e116 = frag_color0In_1;
    param_1 = _e116.xyz;
    let _e118 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e120 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e118.x, _e118.y, _e118.z, _e120);
    let _e125 = frag_color1In_1;
    param_2 = _e125.xyz;
    let _e127 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e129 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e127.x, _e127.y, _e127.z, _e129);
    param_3 = 0u;
    let _e134 = frag_tex_coord0_1;
    param_4 = _e134;
    param_5 = 0i;
    let _e135 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e136 = frag_color0_;
    color0_ = (_e135 * _e136);
    if override_type_4_2 {
        param_6 = 1u;
        let _e138 = frag_tex_coord1_1;
        param_7 = _e138;
        param_8 = 1i;
        let _e139 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e140 = frag_color1_;
        color1_ = (_e139 * _e140);
        let _e142 = color0_;
        let _e144 = color1_;
        let _e146 = (_e142.xyz + _e144.xyz);
        let _e148 = color0_[3u];
        let _e150 = color1_[3u];
        base = vec4<f32>(_e146.x, _e146.y, _e146.z, (_e148 * _e150));
    } else {
        if override_type_4_3 {
            param_9 = 1u;
            let _e156 = frag_tex_coord1_1;
            param_10 = _e156;
            param_11 = 1i;
            let _e157 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e158 = frag_color1_;
            color1_1 = (_e157 * _e158);
            let _e161 = color0_[3u];
            let _e162 = color0_;
            color0_ = (_e162 * _e161);
            let _e165 = color1_1[3u];
            let _e166 = color1_1;
            color1_1 = (_e166 * _e165);
            let _e168 = color0_;
            let _e170 = color1_1;
            let _e172 = (_e168.xyz + _e170.xyz);
            let _e174 = color0_[3u];
            let _e176 = color1_1[3u];
            base = vec4<f32>(_e172.x, _e172.y, _e172.z, (_e174 * _e176));
        } else {
            if override_type_4_4 {
                param_12 = 1u;
                let _e182 = frag_tex_coord1_1;
                param_13 = _e182;
                param_14 = 1i;
                let _e183 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e184 = frag_color1_;
                color1_2 = (_e183 * _e184);
                let _e187 = color0_[3u];
                let _e189 = color0_;
                color0_ = (_e189 * (1f - _e187));
                let _e192 = color1_2[3u];
                let _e194 = color1_2;
                color1_2 = (_e194 * (1f - _e192));
                let _e196 = color0_;
                let _e198 = color1_2;
                let _e200 = (_e196.xyz + _e198.xyz);
                let _e202 = color0_[3u];
                let _e204 = color1_2[3u];
                base = vec4<f32>(_e200.x, _e200.y, _e200.z, (_e202 * _e204));
            } else {
                if override_type_4_5 {
                    param_15 = 1u;
                    let _e210 = frag_tex_coord1_1;
                    param_16 = _e210;
                    param_17 = 1i;
                    let _e211 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e212 = frag_color1_;
                    color1_3 = (_e211 * _e212);
                    let _e214 = color0_;
                    let _e215 = color1_3;
                    let _e217 = color1_3[3u];
                    base = mix(_e214, _e215, vec4(_e217));
                } else {
                    if override_type_4_6 {
                        param_18 = 1u;
                        let _e220 = frag_tex_coord1_1;
                        param_19 = _e220;
                        param_20 = 1i;
                        let _e221 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e222 = frag_color1_;
                        color1_4 = (_e221 * _e222);
                        let _e224 = color1_4;
                        let _e225 = color0_;
                        let _e227 = color1_4[3u];
                        base = mix(_e224, _e225, vec4(_e227));
                    } else {
                        if override_type_4_7 {
                            param_21 = 1u;
                            let _e230 = frag_tex_coord1_1;
                            param_22 = _e230;
                            param_23 = 1i;
                            let _e231 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e232 = frag_color1_;
                            color1_5 = (_e231 * _e232);
                            let _e234 = color1_5;
                            let _e236 = color1_5[3u];
                            let _e239 = color0_;
                            base = ((_e234 + vec4(_e236)) * _e239);
                        } else {
                            param_24 = 1u;
                            let _e241 = frag_tex_coord1_1;
                            param_25 = _e241;
                            param_26 = 1i;
                            let _e242 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e243 = frag_color1_;
                            color1_6 = (_e242 * _e243);
                            let _e245 = color0_;
                            let _e247 = color1_6;
                            let _e249 = (_e245.xyz * _e247.xyz);
                            base[0u] = _e249.x;
                            base[1u] = _e249.y;
                            base[2u] = _e249.z;
                            let _e257 = color0_[3u];
                            let _e259 = color1_6[3u];
                            base[3u] = (_e257 * _e259);
                        }
                    }
                }
            }
        }
    }
    if override_type_4_8 {
        let _e264 = unnamed.worldLightParams[1u];
        wetness = clamp(_e264, 0f, 1f);
        let _e268 = unnamed.worldLightParams[2u];
        frost = clamp(_e268, 0f, 1f);
        let _e270 = base;
        luminance = dot(_e270.xyz, vec3<f32>(0.2126f, 0.7152f, 0.0722f));
        let _e273 = wetness;
        let _e275 = base;
        let _e277 = (_e275.xyz * mix(1f, 0.82f, _e273));
        base[0u] = _e277.x;
        base[1u] = _e277.y;
        base[2u] = _e277.z;
        let _e284 = base;
        let _e286 = luminance;
        let _e288 = luminance;
        let _e290 = luminance;
        let _e292 = frost;
        let _e295 = mix(_e284.xyz, vec3<f32>((_e286 * 0.88f), (_e288 * 0.94f), _e290), vec3((_e292 * 0.55f)));
        base[0u] = _e295.x;
        base[1u] = _e295.y;
        base[2u] = _e295.z;
    }
    let _e302 = color0_;
    let _e305 = unnamed.emissionRadiance;
    let _e308 = base;
    let _e310 = (_e308.xyz + (_e302.xyz * _e305.xyz));
    base[0u] = _e310.x;
    base[1u] = _e310.y;
    base[2u] = _e310.z;
    let _e317 = wired_advanced_fog_enabled_u0028_();
    if _e317 {
        let _e318 = wired_advanced_fog_amount_u0028_();
        fogAmount = _e318;
        let _e319 = base;
        let _e322 = unnamed.advancedFogColorDensity;
        let _e324 = fogAmount;
        let _e326 = mix(_e319.xyz, _e322.xyz, vec3(_e324));
        base[0u] = _e326.x;
        base[1u] = _e326.y;
        base[2u] = _e326.z;
    }
    if override_type_4_9 {
        let _e334 = base[3u];
        if (_e334 == 0f) {
            discard;
        }
    } else {
        if override_type_4_10 {
            let _e336 = base;
            let _e338 = base;
            if (dot(_e336.xyz, _e338.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e342 = base;
    out_color = _e342;
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
