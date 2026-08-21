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
@id(7) override discard_mode: i32 = 0i;
override override_type_12_8: bool = (discard_mode == 1i);
override override_type_12_9: bool = (discard_mode == 2i);
@id(3) override alpha_to_coverage: i32 = 0i;

var<private> out_temporal_velocity: vec2<f32>;
var<private> out_temporal_validity: f32;
@group(1) @binding(0) 
var wired_bindless_images: binding_array<texture_2d<f32>>;
@group(0) @binding(0) 
var<uniform> unnamed: UBO;
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

fn wiredTemporalWriteAux_u0028_() {
    out_temporal_velocity = vec2<f32>(0f, 0f);
    out_temporal_validity = 0f;
    return;
}

fn sRGBToLinear_u0028_vf3_u003b(c: ptr<function, vec3<f32>>) -> vec3<f32> {
    var cutoff: vec3<bool>;
    var lo: vec3<f32>;
    var hi: vec3<f32>;

    let _e64 = (*c);
    (*c) = max(_e64, vec3<f32>(0f, 0f, 0f));
    let _e66 = (*c);
    cutoff = (_e66 <= vec3<f32>(0.04045f, 0.04045f, 0.04045f));
    let _e68 = (*c);
    lo = (_e68 / vec3(12.92f));
    let _e71 = (*c);
    hi = pow(((_e71 + vec3<f32>(0.055f, 0.055f, 0.055f)) / vec3(1.055f)), vec3<f32>(2.4f, 2.4f, 2.4f));
    let _e76 = hi;
    let _e77 = lo;
    let _e78 = cutoff;
    return mix(_e76, _e77, select(vec3<f32>(0f, 0f, 0f), vec3<f32>(1f, 1f, 1f), _e78));
}

fn wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b(role: ptr<function, u32>, uv: ptr<function, vec2<f32>>, slot: ptr<function, i32>) -> vec4<f32> {
    var c_1: vec4<f32>;
    var param: vec3<f32>;

    let _e65 = (*role);
    let _e67 = (*role);
    let _e72 = unnamed.packed_indices[(_e65 / 4u)][(_e67 % 4u)];
    let _e75 = (*role);
    let _e77 = (*role);
    let _e82 = unnamed.packed_indices[(_e75 / 4u)][(_e77 % 4u)];
    let _e87 = (*uv);
    let _e88 = textureSample(wired_bindless_images[(_e72 & 4095u)], wired_bindless_samplers[((_e82 >> bitcast<u32>(12i)) & 255u)], _e87);
    c_1 = _e88;
    let _e89 = (*slot);
    if ((tex_domain & (1i << bitcast<u32>(_e89))) == 0i) {
        let _e94 = c_1;
        param = _e94.xyz;
        let _e96 = sRGBToLinear_u0028_vf3_u003b((&param));
        c_1[0u] = _e96.x;
        c_1[1u] = _e96.y;
        c_1[2u] = _e96.z;
    }
    let _e103 = (*slot);
    if (lightmap_slot == (_e103 + 1i)) {
        let _e108 = unnamed.worldLightParams[0u];
        let _e109 = c_1;
        let _e111 = (_e109.xyz * _e108);
        c_1[0u] = _e111.x;
        c_1[1u] = _e111.y;
        c_1[2u] = _e111.z;
    }
    let _e118 = c_1;
    return _e118;
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

    let _e97 = frag_color0In_1;
    param_1 = _e97.xyz;
    let _e99 = sRGBToLinear_u0028_vf3_u003b((&param_1));
    let _e101 = frag_color0In_1[3u];
    frag_color0_ = vec4<f32>(_e99.x, _e99.y, _e99.z, _e101);
    let _e106 = frag_color1In_1;
    param_2 = _e106.xyz;
    let _e108 = sRGBToLinear_u0028_vf3_u003b((&param_2));
    let _e110 = frag_color1In_1[3u];
    frag_color1_ = vec4<f32>(_e108.x, _e108.y, _e108.z, _e110);
    param_3 = 0u;
    let _e115 = frag_tex_coord0_1;
    param_4 = _e115;
    param_5 = 0i;
    let _e116 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_3), (&param_4), (&param_5));
    let _e117 = frag_color0_;
    color0_ = (_e116 * _e117);
    if override_type_12_2 {
        param_6 = 1u;
        let _e119 = frag_tex_coord1_1;
        param_7 = _e119;
        param_8 = 1i;
        let _e120 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_6), (&param_7), (&param_8));
        let _e121 = frag_color1_;
        color1_ = (_e120 * _e121);
        let _e123 = color0_;
        let _e125 = color1_;
        let _e127 = (_e123.xyz + _e125.xyz);
        let _e129 = color0_[3u];
        let _e131 = color1_[3u];
        base = vec4<f32>(_e127.x, _e127.y, _e127.z, (_e129 * _e131));
    } else {
        if override_type_12_3 {
            param_9 = 1u;
            let _e137 = frag_tex_coord1_1;
            param_10 = _e137;
            param_11 = 1i;
            let _e138 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_9), (&param_10), (&param_11));
            let _e139 = frag_color1_;
            color1_1 = (_e138 * _e139);
            let _e142 = color0_[3u];
            let _e143 = color0_;
            color0_ = (_e143 * _e142);
            let _e146 = color1_1[3u];
            let _e147 = color1_1;
            color1_1 = (_e147 * _e146);
            let _e149 = color0_;
            let _e151 = color1_1;
            let _e153 = (_e149.xyz + _e151.xyz);
            let _e155 = color0_[3u];
            let _e157 = color1_1[3u];
            base = vec4<f32>(_e153.x, _e153.y, _e153.z, (_e155 * _e157));
        } else {
            if override_type_12_4 {
                param_12 = 1u;
                let _e163 = frag_tex_coord1_1;
                param_13 = _e163;
                param_14 = 1i;
                let _e164 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_12), (&param_13), (&param_14));
                let _e165 = frag_color1_;
                color1_2 = (_e164 * _e165);
                let _e168 = color0_[3u];
                let _e170 = color0_;
                color0_ = (_e170 * (1f - _e168));
                let _e173 = color1_2[3u];
                let _e175 = color1_2;
                color1_2 = (_e175 * (1f - _e173));
                let _e177 = color0_;
                let _e179 = color1_2;
                let _e181 = (_e177.xyz + _e179.xyz);
                let _e183 = color0_[3u];
                let _e185 = color1_2[3u];
                base = vec4<f32>(_e181.x, _e181.y, _e181.z, (_e183 * _e185));
            } else {
                if override_type_12_5 {
                    param_15 = 1u;
                    let _e191 = frag_tex_coord1_1;
                    param_16 = _e191;
                    param_17 = 1i;
                    let _e192 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_15), (&param_16), (&param_17));
                    let _e193 = frag_color1_;
                    color1_3 = (_e192 * _e193);
                    let _e195 = color0_;
                    let _e196 = color1_3;
                    let _e198 = color1_3[3u];
                    base = mix(_e195, _e196, vec4(_e198));
                } else {
                    if override_type_12_6 {
                        param_18 = 1u;
                        let _e201 = frag_tex_coord1_1;
                        param_19 = _e201;
                        param_20 = 1i;
                        let _e202 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_18), (&param_19), (&param_20));
                        let _e203 = frag_color1_;
                        color1_4 = (_e202 * _e203);
                        let _e205 = color1_4;
                        let _e206 = color0_;
                        let _e208 = color1_4[3u];
                        base = mix(_e205, _e206, vec4(_e208));
                    } else {
                        if override_type_12_7 {
                            param_21 = 1u;
                            let _e211 = frag_tex_coord1_1;
                            param_22 = _e211;
                            param_23 = 1i;
                            let _e212 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_21), (&param_22), (&param_23));
                            let _e213 = frag_color1_;
                            color1_5 = (_e212 * _e213);
                            let _e215 = color1_5;
                            let _e217 = color1_5[3u];
                            let _e220 = color0_;
                            base = ((_e215 + vec4(_e217)) * _e220);
                        } else {
                            param_24 = 1u;
                            let _e222 = frag_tex_coord1_1;
                            param_25 = _e222;
                            param_26 = 1i;
                            let _e223 = wired_bl_sample_domain_u0028_u1_u003b_vf2_u003b_i1_u003b((&param_24), (&param_25), (&param_26));
                            let _e224 = frag_color1_;
                            color1_6 = (_e223 * _e224);
                            let _e226 = color0_;
                            let _e228 = color1_6;
                            let _e230 = (_e226.xyz * _e228.xyz);
                            base[0u] = _e230.x;
                            base[1u] = _e230.y;
                            base[2u] = _e230.z;
                            let _e238 = color0_[3u];
                            let _e240 = color1_6[3u];
                            base[3u] = (_e238 * _e240);
                        }
                    }
                }
            }
        }
    }
    if override_type_12_8 {
        let _e244 = base[3u];
        if (_e244 == 0f) {
            discard;
        }
    } else {
        if override_type_12_9 {
            let _e246 = base;
            let _e248 = base;
            if (dot(_e246.xyz, _e248.xyz) == 0f) {
                discard;
            }
        }
    }
    let _e252 = base;
    out_color = _e252;
    wiredTemporalWriteAux_u0028_();
    return;
}

@fragment 
fn main(@location(0) frag_color0In: vec4<f32>, @location(5) frag_color1In: vec4<f32>, @location(1) frag_tex_coord0_: vec2<f32>, @location(2) frag_tex_coord1_: vec2<f32>, @location(10) temporalCurrentClip: vec4<f32>, @location(11) temporalPreviousClip: vec4<f32>, @location(12) @interpolate(flat) temporalOutcome: u32) -> FragmentOutput {
    frag_color0In_1 = frag_color0In;
    frag_color1In_1 = frag_color1In;
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
