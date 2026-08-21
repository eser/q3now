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
}

struct FragmentOutput {
    @location(1) member: vec2<f32>,
    @location(2) member_1: f32,
    @location(0) member_2: vec4<f32>,
}

@id(4) override tex_domain: i32 = 0i;
@id(26) override lightmap_slot: i32 = 0i;
@id(6) override tex_mode: i32 = 0i;
override override_type_12_: bool = (tex_mode == 1i);
override override_type_12_1: bool = (tex_mode == 2i);
override override_type_12_2: bool = (override_type_12_ || override_type_12_1);
override override_type_12_3: bool = (tex_mode == 3i);
override override_type_12_4: bool = (tex_mode == 4i);
override override_type_12_5: bool = (tex_mode == 5i);
override override_type_12_6: bool = (tex_mode == 6i);
override override_type_12_7: bool = (tex_mode == 7i);
@id(10) override acff: i32 = 0i;
override override_type_12_8: bool = (acff == 1i);
override override_type_12_9: bool = (acff == 2i);
override override_type_12_10: bool = (acff == 3i);
@id(7) override discard_mode: i32 = 0i;
override override_type_12_11: bool = (discard_mode == 1i);
override override_type_12_12: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
@group(1) @binding(1) 
var wired_bindless_samplers: binding_array<sampler>;
var<private> fog_tex_coord_1: vec2<f32>;
var<private> frag_color0In_1: vec4<f32>;
var<private> frag_color1In_1: vec4<f32>;
var<private> frag_tex_coord0_1: vec2<f32>;
var<private> frag_tex_coord1_1: vec2<f32>;
var<private> out_color: vec4<f32>;
var<private> temporalCurrentClip_1: vec4<f32>;
var<private> temporalPreviousClip_1: vec4<f32>;
var<private> temporalOutcome_1: u32;

fn wiredTemporalWriteAux_u0028_() {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e69 = (*c);
    (*c) = max(_e69, vec3<f32>(0f, 0f, 0f));
    let _e71 = (*c);
    cutoff = (_e71 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e73 = (*c);
    lo = (_e73 / vec3(12.92f));
    let _e76 = (*c);
    hi = pow(((_e76 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e81 = hi;
    let _e82 = lo;
    let _e83 = cutoff;
    return mix(_e81, _e82, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e83));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e70 = (*role);
    let _e72 = (*role);
    let _e77 = unnamed.packed_indices[(_e70 / 4u)][(_e72 % 4u)];
    let _e80 = (*role);
    let _e82 = (*role);
    let _e87 = unnamed.packed_indices[(_e80 / 4u)][(_e82 % 4u)];
    let _e92 = (*uv);
    let _e93 = textureSample(wired_bindless_images[(_e77 & 4095u)], wired_bindless_samplers[((_e87 >> bitcast<u32>(12i)) & 255u)], _e92);
    c_1 = _e93;
    let _e94 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e94))) == 0i) {
        let _e99 = c_1;
        param = _e99.xyz;
        let _e101 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e101.x;
        c_1[1u] = _e101.y;
        c_1[2u] = _e101.z;
    }
    let _e108 = (*slot);
    if (lightmap_slot == (_e108 + 1i)) {
        let _e113 = unnamed.worldLightParams[0u];
        let _e114 = c_1;
        let _e116 = (_e114.xyz * _e113);
        c_1[0u] = _e116.x;
        c_1[1u] = _e116.y;
        c_1[2u] = _e116.z;
    }
    let _e123 = c_1;
    return _e123;
}

fn main_1() {
    var fog: vec4<f32>;
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

    let _e106 = unnamed.packed_indices[0i][3u];
    let _e112 = unnamed.packed_indices[0i][3u];
    let _e117 = fog_tex_coord_1;
    let _e118 = textureSample(wired_bindless_images[(_e106 & 4095u)], wired_bindless_samplers[((_e112 >> bitcast<u32>(12i)) & 255u)], _e117);
    fog = _e118;
    let _e119 = frag_color0In_1;
    param_1 = _e119.xyz;
    let _e121 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e123 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e121.x, _e121.y, _e121.z, _e123);
    let _e128 = frag_color1In_1;
    param_2 = _e128.xyz;
    let _e130 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e132 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e130.x, _e130.y, _e130.z, _e132);
    param_3 = 0u;
    let _e137 = frag_tex_coord0_1;
    param_4 = _e137;
    param_5 = 0i;
    let _e138 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e139 = frag_color0_;
    color0_ = (_e138 * _e139);
    if override_type_12_2 {
        param_6 = 1u;
        let _e141 = frag_tex_coord1_1;
        param_7 = _e141;
        param_8 = 1i;
        let _e142 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e143 = frag_color1_;
        color1_ = (_e142 * _e143);
        let _e145 = color0_;
        let _e147 = color1_;
        let _e149 = (_e145.xyz + _e147.xyz);
        let _e151 = color0_[3u];
        let _e153 = color1_[3u];
        base = vec4<f32>(_e149.x, _e149.y, _e149.z, (_e151 * _e153));
    } else {
        if override_type_12_3 {
            param_9 = 1u;
            let _e159 = frag_tex_coord1_1;
            param_10 = _e159;
            param_11 = 1i;
            let _e160 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e161 = frag_color1_;
            color1_1 = (_e160 * _e161);
            let _e164 = color0_[3u];
            let _e165 = color0_;
            color0_ = (_e165 * _e164);
            let _e168 = color1_1[3u];
            let _e169 = color1_1;
            color1_1 = (_e169 * _e168);
            let _e171 = color0_;
            let _e173 = color1_1;
            let _e175 = (_e171.xyz + _e173.xyz);
            let _e177 = color0_[3u];
            let _e179 = color1_1[3u];
            base = vec4<f32>(_e175.x, _e175.y, _e175.z, (_e177 * _e179));
        } else {
            if override_type_12_4 {
                param_12 = 1u;
                let _e185 = frag_tex_coord1_1;
                param_13 = _e185;
                param_14 = 1i;
                let _e186 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e187 = frag_color1_;
                color1_2 = (_e186 * _e187);
                let _e190 = color0_[3u];
                let _e192 = color0_;
                color0_ = (_e192 * (1f - _e190));
                let _e195 = color1_2[3u];
                let _e197 = color1_2;
                color1_2 = (_e197 * (1f - _e195));
                let _e199 = color0_;
                let _e201 = color1_2;
                let _e203 = (_e199.xyz + _e201.xyz);
                let _e205 = color0_[3u];
                let _e207 = color1_2[3u];
                base = vec4<f32>(_e203.x, _e203.y, _e203.z, (_e205 * _e207));
            } else {
                if override_type_12_5 {
                    param_15 = 1u;
                    let _e213 = frag_tex_coord1_1;
                    param_16 = _e213;
                    param_17 = 1i;
                    let _e214 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e215 = frag_color1_;
                    color1_3 = (_e214 * _e215);
                    let _e217 = color0_;
                    let _e218 = color1_3;
                    let _e220 = color1_3[3u];
                    base = mix(_e217, _e218, vec4(_e220));
                } else {
                    if override_type_12_6 {
                        param_18 = 1u;
                        let _e223 = frag_tex_coord1_1;
                        param_19 = _e223;
                        param_20 = 1i;
                        let _e224 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e225 = frag_color1_;
                        color1_4 = (_e224 * _e225);
                        let _e227 = color1_4;
                        let _e228 = color0_;
                        let _e230 = color1_4[3u];
                        base = mix(_e227, _e228, vec4(_e230));
                    } else {
                        if override_type_12_7 {
                            param_21 = 1u;
                            let _e233 = frag_tex_coord1_1;
                            param_22 = _e233;
                            param_23 = 1i;
                            let _e234 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e235 = frag_color1_;
                            color1_5 = (_e234 * _e235);
                            let _e237 = color1_5;
                            let _e239 = color1_5[3u];
                            let _e242 = color0_;
                            base = ((_e237 + vec4(_e239)) * _e242);
                        } else {
                            param_24 = 1u;
                            let _e244 = frag_tex_coord1_1;
                            param_25 = _e244;
                            param_26 = 1i;
                            let _e245 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e246 = frag_color1_;
                            color1_6 = (_e245 * _e246);
                            let _e248 = color0_;
                            let _e250 = color1_6;
                            let _e252 = (_e248.xyz * _e250.xyz);
                            base[0u] = _e252.x;
                            base[1u] = _e252.y;
                            base[2u] = _e252.z;
                            let _e260 = color0_[3u];
                            let _e262 = color1_6[3u];
                            base[3u] = (_e260 * _e262);
                        }
                    }
                }
            }
        }
    }
    if override_type_12_8 {
        let _e265 = base;
        let _e268 = fog[3u];
        let _e270 = (_e265.xyz * (1f - _e268));
        base[0u] = _e270.x;
        base[1u] = _e270.y;
        base[2u] = _e270.z;
    } else {
        if override_type_12_9 {
            let _e277 = base;
            let _e279 = fog[3u];
            base = (_e277 * (1f - _e279));
        } else {
            if override_type_12_10 {
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
    if override_type_12_11 {
        let _e299 = base[3u];
        if (_e299 == 0f) {
            discard;
        }
    } else {
        if override_type_12_12 {
            let _e301 = base;
            let _e303 = base;
            if (dot(_e301.xyz, _e303.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e307 = base;
    out_color = _e307;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(4) fog_tex_coord: vec2<f32>, @location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    fog_tex_coord_1 = fog_tex_coord;
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
